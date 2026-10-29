// Include after reload/interact/weapon/QuickSelect; only explicit phase7 arms this adapter.
#include <stdint.h>
static void* touch_equipped_gun(void* hand);
static void* (*rf_classes[6])(void),*(*rf_scene_class)(void),*(*rf_mesh_class)(void),*(*rf_right)(const void*);
static int (*rf_ready[6])(const void*),(*rf_count)(const void*,int),(*rf_in_valid)(const void*);
static int (*rf_recoil)(const void*),(*rf_blend)(const void*),(*rf_flourish)(const void*),(*rf_ammo_held)(const void*,float);
static void* (*rf_slide_hand[3])(const void*),*(*rf_grip_hand)(const void*);
static struct reload_tf (*rf_socket)(const void*,unsigned long long,int),(*rf_grip_target)(const void*);
static const char* (*rf_settings)(void);
struct rf_vec {float x,y,z;};
static struct rf_vec (*rf_grab_location)(const void*);
struct rf_binding {void* object[13];unsigned long long name[13];};
static struct {struct rf_binding binding;int kind,side,stage,grip_pressed,hover,trigger_bit;long until,last,start,up_since,pulse;} rf_post;
static unsigned rf_logs;
static int (*rf_capacity)(const void*),(*rf_reserve)(const void*);
static int rf_single_shell_ready;
static unsigned rf_shell_logs;

static int rf_typed(const void* object,const void* cls,int size) {return cls&&reload_layout(object,size)&&reload_is_a(object,cls);}
static int rf_kind(const void* gun) {
    static const int size[6]={0x1110,0x1210,0x1150,0xe50,0x11a0,0x10d0};
    for(int i=0;i<6;i++)if(rf_classes[i]&&rf_ready[i]&&rf_typed(gun,rf_classes[i](),size[i])) {
        void** table=*(void***)gun;return table&&table[0x778/sizeof(void*)]==(void*)rf_ready[i]?i+1:0;
    }
    return 0;
}
static int rf_grip(void* grip,void* owner) {
    return rl_grip_class&&reload_grip_registered(grip)&&rf_typed(grip,rl_grip_class(),0x460)
        && *(void* const*)((const char*)grip+0x20)==owner
        && *(void* const*)((const char*)grip+0x300)==owner && ti_parent&&ti_parent(grip);
}
static int rf_owner(void* gun,void** hand_out,void** pawn_out) {
    if(!rl_hand_class||!rl_pawn_class||!rl_player||!rl_active||!rl_primary||!f_GetCurrentGrip||!rf_kind(gun))return 0;
    void* pawn=*(void* const*)((const char*)gun+0x3c0),*hand=rl_primary(gun);
    if(!rf_typed(pawn,rl_pawn_class(),0x10d0)||pawn!=rl_active()
        ||!rf_typed(hand,rl_hand_class(),0x510)||rl_player(hand)!=pawn
        ||*(const unsigned char*)((const char*)hand+0x314)>1)return 0;
    void* grip=*(void* const*)((const char*)gun+0x270);
    if(!rf_grip(grip,gun)||f_GetCurrentGrip(hand)!=grip||touch_equipped_gun(hand)!=gun)return 0;
    *hand_out=hand;*pawn_out=pawn;return 1;
}
static void rf_single_shell_reload(void* gun,int kind,void (*original)(void*)) {
    void* hand,*pawn;int gt=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE);
    if(!__atomic_load_n(&rf_single_shell_ready,__ATOMIC_ACQUIRE) || gt<=0 || gettid()!=gt
        || kind<5 || kind>6 || rf_kind(gun)!=kind || !rf_owner(gun,&hand,&pawn)){original(gun);return;}
    int enable_offset=kind==5?0x1130:0x10c5,amount_offset=kind==5?0x1134:0x10c8;
    unsigned char* enabled=(unsigned char*)gun+enable_offset;
    int* amount=(int*)((char*)gun+amount_offset),saved_amount=*amount;
    unsigned char saved_enabled=*enabled;unsigned long long name;
    memcpy(&name,(const char*)gun+0x18,8);void* cls=*(void* const*)((const char*)gun+0x10);
    int before=rf_count(gun,0);
    if(saved_enabled>1 || saved_amount<1 || saved_amount>9999 || before<0 || before>9999){original(gun);return;}
    // Native FinishReload owns timing/economy; each completed animation may transfer only one shell.
    *enabled=1;*amount=1;original(gun);
    unsigned long long current;memcpy(&current,(const char*)gun+0x18,8);
    if(current!=name || *(void* const*)((const char*)gun+0x10)!=cls)return;
    *enabled=saved_enabled;*amount=saved_amount;
    if(rf_shell_logs<16){++rf_shell_logs;LOGF("RELOADSHELL native kind=%d ammo=%d->%d restored=%u/%d",kind,before,rf_count(gun,0),saved_enabled,saved_amount);}
}
#if defined(__aarch64__)
unsigned long g_tgt_rf_shotgun_reload,g_tgt_rf_striker_reload;
__attribute__((naked)) static void gate_rf_shotgun_reload(void* gun) {
    asm volatile(".inst 0xa9be4ff4\n" ".inst 0xa9017bfd\n" ".inst 0x910043fd\n" ".inst 0x52822608\n"
        "adrp x16, :got:g_tgt_rf_shotgun_reload\n" "ldr x16, [x16, :got_lo12:g_tgt_rf_shotgun_reload]\n"
        "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static void gate_rf_striker_reload(void* gun) {
    asm volatile(".inst 0xf81e0ff3\n" ".inst 0xa9017bfd\n" ".inst 0x910043fd\n" ".inst 0x528218a8\n"
        "adrp x16, :got:g_tgt_rf_striker_reload\n" "ldr x16, [x16, :got_lo12:g_tgt_rf_striker_reload]\n"
        "ldr x16, [x16]\n" "br x16\n");
}
static void hook_rf_shotgun_reload(void* gun){rf_single_shell_reload(gun,5,gate_rf_shotgun_reload);}
static void hook_rf_striker_reload(void* gun){rf_single_shell_reload(gun,6,gate_rf_striker_reload);}
#endif
static int reload_family_info(void* gun,int* angle_offset) {
    if(angle_offset)*angle_offset=0;
    int gt=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE);if(gt<=0||gettid()!=gt)return 0;
    void* hand,*pawn;int kind=rf_kind(gun);
    if(!kind||!angle_offset||!rf_owner(gun,&hand,&pawn))return 0;
    int offset=kind==4?0xd38:0xd60;float angle=*(const float*)((const char*)gun+offset);
    if(!isfinite(angle)||angle<0.0f||angle>180.0f)return 0;
    if(kind==4) {
        float norm=0;for(int i=0;i<3;i++){float x=*(const float*)((const char*)gun+0xda0+4*i);if(!isfinite(x))return 0;norm+=x*x;}
        if(!isfinite(norm)||norm<0.0001f)return 0;
    }
    *angle_offset=offset;return kind>5?5:kind;
}
static int reload_family_shells_pending(void* gun,int* loaded) {
    int kind=rf_kind(gun);
    if(!loaded || !rf_count || !rf_capacity || !rf_reserve || kind<5 || kind>6)return -1;
    void* hand,*pawn;if(!rf_owner(gun,&hand,&pawn))return -1;
    void* const* table=*(void* const* const*)gun;
    if(!table || table[0x7b8/sizeof(void*)]!=(void*)rf_count
        || table[0x7c8/sizeof(void*)]!=(void*)rf_capacity
        || table[0x7d8/sizeof(void*)]!=(void*)rf_reserve)return -1;
    unsigned long long name;memcpy(&name,(const char*)gun+0x18,8);
    int count=rf_count(gun,0),capacity=((int (*)(const void*))table[0x7c8/sizeof(void*)])(gun);
    int reserve=((int (*)(const void*))table[0x7d8/sizeof(void*)])(gun);
    unsigned long long current;memcpy(&current,(const char*)gun+0x18,8);
    if(current!=name || !rf_owner(gun,&hand,&pawn) || count<0 || count>9999
        || capacity<1 || capacity>9999 || reserve<0 || reserve>9999)return -1;
    *loaded=count;return count<capacity && reserve>0;
}
static int rf_inputs_clear(long now) {
    return g_reload_phase==7&&__atomic_load_n(&g_touch_context,__ATOMIC_ACQUIRE)==1
        &&!__atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE)
        &&!__atomic_load_n(&g_touch_weapon_request,__ATOMIC_ACQUIRE)
        &&!__atomic_load_n(&g_touch_reload_request,__ATOMIC_ACQUIRE)
        &&!__atomic_load_n(&g_touch_inventory_request,__ATOMIC_ACQUIRE)
        &&!__atomic_load_n(&g_touch_pause,__ATOMIC_ACQUIRE)
        &&!__atomic_load_n(&g_touch_interact_request,__ATOMIC_ACQUIRE)&&!touch_reload_user_fire(now);
}
static int rf_allowed(long now) {
    return rl_type&&rl_type()==1&&rl_paused&&!rl_paused()&&rl_player_valid&&rl_player_valid()&&rf_inputs_clear(now);
}
static int rf_bind(void* primary,void* gun,struct rf_binding* out) {
    void* owner,*pawn;int angle;
    if(!rf_scene_class||!rf_mesh_class||!rf_right||!rl_left||!rl_ammo_class||!rl_get_pawn||!reload_family_info(gun,&angle)
        ||!rf_owner(gun,&owner,&pawn)||owner!=primary)return 0;
    unsigned side=*(const unsigned char*)((const char*)primary+0x314);
    if((side?rf_right(pawn):rl_left(pawn))!=primary)return 0;
    void* off=side?rl_left(pawn):rf_right(pawn);
    if(!rf_typed(off,rl_hand_class(),0x510)||rl_player(off)!=pawn||*(const unsigned char*)((const char*)off+0x314)!=1-side)return 0;
    void* ammo=*(void* const*)((const char*)gun+0x8e0),*ammo_grip=reload_ammo_primary(gun,pawn);
    if(!rf_typed(ammo,rl_ammo_class(),0x610)||rl_get_pawn(ammo)!=pawn||*(void* const*)((const char*)ammo+0x578)!=gun||!rf_grip(ammo_grip,ammo))return 0;
    void* primary_root=*(void* const*)((const char*)primary+0x130),*off_root=*(void* const*)((const char*)off+0x130);
    void* root=*(void* const*)((const char*)gun+0x130),*mesh=*(void* const*)((const char*)gun+0x540);
    if(!rf_typed(primary_root,rf_scene_class(),0x1f0)||*(void* const*)((const char*)primary_root+0x20)!=primary
        ||!rf_typed(off_root,rf_scene_class(),0x1f0)||*(void* const*)((const char*)off_root+0x20)!=off
        ||!rf_typed(root,rf_scene_class(),0x1f0)||*(void* const*)((const char*)root+0x20)!=gun
        ||!rf_typed(mesh,rf_mesh_class(),0x7a0)||*(void* const*)((const char*)mesh+0x20)!=gun)return 0;
    void* mechanical=0,*target=0;
    if(rf_kind(gun)==3) {
        mechanical=*(void* const*)((const char*)gun+0x10c8);
        if(!rf_grip(mechanical,gun))return 0;
        target=*(void* const*)((const char*)mechanical+0x230);
        if(!rf_typed(target,rf_scene_class(),0x1f0)||*(void* const*)((const char*)target+0x20)!=gun)return 0;
    }
    void* objects[13]={primary,off,pawn,gun,*(void* const*)((const char*)gun+0x270),ammo,ammo_grip,primary_root,off_root,root,mesh,mechanical,target};
    memset(out,0,sizeof *out);for(int i=0;i<13;i++){out->object[i]=objects[i];if(objects[i])memcpy(&out->name[i],(const char*)objects[i]+0x18,8);}
    return 1;
}
static int rf_current(long now) {
    struct rf_binding b;void* pawn=rl_active();
    if(!rf_allowed(now)||!rf_typed(pawn,rl_pawn_class(),0x10d0)||pawn!=rf_post.binding.object[2])return 0;
    void* primary=rf_post.side?rf_right(pawn):rl_left(pawn),*gun=touch_equipped_gun(primary);
    return primary==rf_post.binding.object[0]&&gun==rf_post.binding.object[3]&&rf_bind(primary,gun,&b)
        &&!memcmp(&b,&rf_post.binding,sizeof b)&&rl_interactivity(b.object[3])==2&&!rl_reloading(b.object[3])
        &&rf_count(b.object[3],0)>0
        &&rf_bind(primary,gun,&b)&&!memcmp(&b,&rf_post.binding,sizeof b)&&rf_inputs_clear(now);
}
static int rf_shotgun_reload_pending(const void* gun) {
    if(rf_post.kind!=5 || rf_post.stage!=1 || !rf_post.binding.object[3])return 0;
    if(!gun)return 1;
    if(gun!=rf_post.binding.object[3])return 0;
    unsigned long long name;memcpy(&name,(const char*)gun+0x18,8);return name==rf_post.binding.name[3];
}
static int rf_hover_clear(void* hand,void* expected) {
    void* const* entries=*(void* const* const*)((const char*)hand+0x460);
    int n=*(const int*)((const char*)hand+0x468),max=*(const int*)((const char*)hand+0x46c);
    return n==0?max>=0&&max<=64:n==1&&max>=1&&max<=64&&entries&&entries[0]==expected;
}
static int rf_live_off(void** off_out,void** grip_out) {
    void* grip=rf_post.binding.object[11];
    if(!reload_grip_registered(grip)||!rf_typed(grip,rl_grip_class(),0x460))return 0;
    unsigned long long name;memcpy(&name,(const char*)grip+0x18,8);if(name!=rf_post.binding.name[11])return 0;
    void* gun=*(void* const*)((const char*)grip+0x20),*hand,*pawn;
    if(!rf_kind(gun)||gun!=rf_post.binding.object[3])return 0;
    memcpy(&name,(const char*)gun+0x18,8);if(name!=rf_post.binding.name[3])return 0;
    pawn=rl_get_pawn(gun);if(!rf_typed(pawn,rl_pawn_class(),0x10d0)||pawn!=rf_post.binding.object[2])return 0;
    memcpy(&name,(const char*)pawn+0x18,8);if(name!=rf_post.binding.name[2])return 0;
    hand=rf_post.side?rl_left(pawn):rf_right(pawn);
    if(!rf_typed(hand,rl_hand_class(),0x510)||hand!=rf_post.binding.object[1])return 0;
    memcpy(&name,(const char*)hand+0x18,8);if(name!=rf_post.binding.name[1])return 0;
    *off_out=hand;*grip_out=grip;return 1;
}
static void reload_family_post_cancel(void) {
    int gt=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE);if(gt<=0||gettid()!=gt)return;
    if(rf_post.pulse) {long pulse=rf_post.pulse;(void)__atomic_compare_exchange_n(&g_cmd_until[rf_post.trigger_bit],&pulse,0,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);}
    if(rf_post.hover||rf_post.grip_pressed) {
        void* off,*grip;if(rf_live_off(&off,&grip)) {
            unsigned side=*(const unsigned char*)((const char*)off+0x314);int bit=side?27:29;
            void* current=f_GetCurrentGrip(off);
            if(rf_post.grip_pressed&&(!current||current==grip)&&g_cmd_until[bit]<=ptr_now_ms()&&!(g_grip_latched&(1u<<bit))&&ti_down(off))ti_release(off);
            if(rf_post.hover)f_SetGripForcedHover(off,grip,0);
        }
    }
    memset(&rf_post,0,sizeof rf_post);
}
static int reload_family_post_begin(void* primary,void* gun,long now) {
    int gt=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE);if(gt<=0||gettid()!=gt||rf_post.kind||now<=0)return 0;
    struct rf_binding b;
    if(!rf_allowed(now)||!rf_bind(primary,gun,&b)||rl_interactivity(gun)!=2||rl_reloading(gun)
        ||rf_count(gun,0)<=0||!rf_inputs_clear(now))return 0;
    void* current=f_GetCurrentGrip(b.object[1]),*ammo_hand=rl_primary(b.object[5]);
    unsigned offside=*(const unsigned char*)((const char*)b.object[1]+0x314);int bit=offside?27:29;
    if((current&&current!=b.object[6])||(ammo_hand&&ammo_hand!=b.object[1])
        ||g_cmd_until[bit]>now||(g_grip_latched&(1u<<bit)))return 0;
    rf_post.binding=b;rf_post.kind=rf_kind(gun);rf_post.side=*(const unsigned char*)((const char*)primary+0x314);
    rf_post.stage=0;rf_post.until=now+5000;rf_post.last=rf_post.start=now;rf_post.trigger_bit=rf_post.side?26:28;
    if(rf_logs<8){++rf_logs;LOGF("RELOADFAMILY begin kind=%d gun=%p",rf_post.kind,gun);}return 1;
}
static int rf_pose_safe(const struct reload_tf* pose,const struct reload_tf* primary) {
    if(!reload_tf_valid(pose)||!reload_tf_valid(primary))return 0;float distance=0;
    for(int i=0;i<3;i++){float d=pose->p[i]-primary->p[i];distance+=d*d;}return isfinite(distance)&&distance<=22500;
}
static int rf_machine_pose(long now,int guide) {
    struct rf_binding* b=&rf_post.binding;struct reload_tf pose,primary,mesh;
    memcpy(&pose,(const char*)b->object[8]+0x1c0,sizeof pose);memcpy(&primary,(const char*)b->object[7]+0x1c0,sizeof primary);
    if(guide) {
        struct reload_tf target=rf_grip_target(b->object[11]);if(!reload_tf_valid(&target))return 0;
        struct rf_vec grab=rf_grab_location(b->object[1]);float p[3]={grab.x,grab.y,grab.z};
        for(int i=0;i<3;i++){if(!isfinite(p[i]))return 0;pose.p[i]+=target.p[i]-p[i];}
    } else {
        unsigned long long name[3];memcpy(&name[0],(const char*)b->object[3]+0x10d8,8);memcpy(&name[1],(const char*)b->object[3]+0x10e0,8);memcpy(&name[2],(const char*)b->object[3]+0x10e8,8);
        if(!name[0]||!name[1]||!name[2])return 0;
        struct reload_tf start=rf_socket(b->object[10],name[0],3),middle=rf_socket(b->object[10],name[1],3),end=rf_socket(b->object[10],name[2],3);
        memcpy(&mesh,(const char*)b->object[10]+0x1c0,sizeof mesh);
        if(!reload_marker_valid(&start)||!reload_marker_valid(&middle)||!reload_marker_valid(&end)||!reload_tf_valid(&mesh))return 0;
        float t=fminf(1,fmaxf(0,(float)(now-rf_post.start)/180));t=t*t*(3-2*t);float local[3],world[3],length=0;
        for(int i=0;i<3;i++){float offset=*(const float*)((const char*)b->object[3]+0x112c+4*i),d=end.p[i]-start.p[i];if(!isfinite(offset))return 0;length+=d*d;local[i]=start.p[i]+1.02f*t*d-middle.p[i]+offset;}
        if(length<.01f||length>10000)return 0;q_rot(mesh.q,local,world);for(int i=0;i<3;i++)pose.p[i]=mesh.p[i]+world[i];
    }
    if(!rf_pose_safe(&pose,&primary)||!rf_current(now))return 0;
    rl_world_transform(b->object[8],&pose,0,0,0);return 1;
}
static int rf_revolver_pose(long now) {
    struct rf_binding* b=&rf_post.binding;struct reload_tf pose,gun;
    memcpy(&pose,(const char*)b->object[7]+0x1c0,sizeof pose);memcpy(&gun,(const char*)b->object[9]+0x1c0,sizeof gun);
    if(!reload_tf_valid(&pose)||!reload_tf_valid(&gun))return 0;
    float local[3],direction[3],norm=0;for(int i=0;i<3;i++){local[i]=*(const float*)((const char*)b->object[3]+0xda0+4*i);norm+=local[i]*local[i];}
    if(!isfinite(norm)||norm<.0001f)return 0;q_rot(gun.q,local,direction);norm=sqrtf(norm);for(int i=0;i<3;i++)direction[i]/=norm;
    qf_t delta={-direction[1],direction[0],0,1-direction[2]};norm=delta.x*delta.x+delta.y*delta.y+delta.w*delta.w;
    if(norm<.0001f)delta=(qf_t){1,0,0,0};else{norm=1/sqrtf(norm);delta.x*=norm;delta.y*=norm;delta.w*=norm;}
    float t=fminf(1,fmaxf(0,(float)(now-rf_post.start)/200));t=t*t*(3-2*t);
    delta.x*=t;delta.y*=t;delta.z*=t;delta.w=1+(delta.w-1)*t;norm=sqrtf(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z+delta.w*delta.w);
    if(!isfinite(norm)||norm<.0001f)return 0;delta.x/=norm;delta.y/=norm;delta.z/=norm;delta.w/=norm;pose.q=q_mul(delta,pose.q);
    if(!reload_tf_valid(&pose)||!rf_current(now))return 0;rl_world_transform(b->object[7],&pose,0,0,0);return 1;
}
static int rf_authored_edge(void* gun) {
    if(!*(const unsigned char*)((const char*)gun+0xd2c))return 0;
    if(rf_post.kind==1)return !*(const unsigned char*)((const char*)gun+0x10f0);
    const char* settings=rf_settings();return settings&&settings[0x3d]==1
        &&*(const unsigned char*)((const char*)gun+0x10e8)&&*(const unsigned char*)((const char*)gun+0x11dd)
        &&*(const unsigned char*)((const char*)gun+0x11c8)==2;
}
static int rf_edge_quiet(void* gun,void* primary,void* off,long now) {
    return g_cmd_until[rf_post.trigger_bit]<=now&&!rl_trigger(gun)&&!*(const unsigned char*)((const char*)primary+0x33c)
        &&!f_GetCurrentGrip(off)&&!ti_down(off)&&!rf_slide_hand[rf_post.kind-1](gun)
        &&!rf_recoil(gun)&&!rf_blend(gun)&&!rf_flourish(gun)&&rl_base_ready(gun)
        &&!*(const unsigned char*)((const char*)gun+0xdb8)&&!*(const unsigned char*)((const char*)gun+0xd22)
        &&!*(const unsigned char*)((const char*)gun+0xd23)&&!rf_ammo_held(gun,0)&&!rl_primary(rf_post.binding.object[5]);
}
static int rf_retire_pulse(void) {
    if(!rf_post.pulse)return 1;long pulse=rf_post.pulse;
    if(!__atomic_compare_exchange_n(&g_cmd_until[rf_post.trigger_bit],&pulse,0,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    rf_post.pulse=0;return 1;
}
static int rf_machine_up(void* gun,void* off,long now) {
    unsigned side=*(const unsigned char*)((const char*)off+0x314);int bit=side?27:29;
    void* grip=rf_post.binding.object[11],*current=f_GetCurrentGrip(off),*slide_hand=rf_slide_hand[2](gun);
    if(g_cmd_until[bit]>now||(g_grip_latched&(1u<<bit))||(current&&current!=grip)||(slide_hand&&slide_hand!=off))return 0;
    if(rf_post.grip_pressed&&ti_down(off))ti_release(off);
    if(!rf_current(now)||f_GetCurrentGrip(off)||rf_slide_hand[2](gun)||ti_down(off))return 0;
    rf_post.grip_pressed=0;if(rf_post.hover){f_SetGripForcedHover(off,grip,0);rf_post.hover=0;}return 1;
}
static int reload_family_post_tick(void* hand,int side,long now) {
    int gt=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE);if(gt<=0||gettid()!=gt||!rf_post.kind)return 0;
    if(hand!=rf_post.binding.object[0]&&hand!=rf_post.binding.object[1])return 0;
    if(!rf_typed(hand,rl_hand_class(),0x510)||side<0||side>1||*(const unsigned char*)((const char*)hand+0x314)!=(unsigned)side)return 0;
    if(now<rf_post.last||now>=rf_post.until||!rf_current(now)){reload_family_post_cancel();return 3;}rf_post.last=now;
    void* gun=rf_post.binding.object[3],*primary=rf_post.binding.object[0],*off=rf_post.binding.object[1];
    void* ammo_hand=rl_primary(rf_post.binding.object[5]);
    if(rf_post.stage==0) {
        void* current=f_GetCurrentGrip(off);unsigned offside=*(const unsigned char*)((const char*)off+0x314);int bit=offside?27:29;
        if((current&&current!=rf_post.binding.object[6])||(ammo_hand&&ammo_hand!=off)
            ||g_cmd_until[bit]>now||(g_grip_latched&(1u<<bit))){reload_family_post_cancel();return 3;}
        if(current||ammo_hand||rf_ammo_held(gun,0)||ti_down(off))return 1;
        rf_post.stage=1;rf_post.start=now;
    } else if(ammo_hand||rf_ammo_held(gun,0)){reload_family_post_cancel();return 3;}
    int ready=rf_ready[rf_post.kind-1](gun);
    if(!rf_current(now)){reload_family_post_cancel();return 3;}
    long active_trigger=__atomic_load_n(&g_cmd_until[rf_post.trigger_bit],__ATOMIC_ACQUIRE);
    if(active_trigger>now&&active_trigger!=rf_post.pulse){reload_family_post_cancel();return 3;}
    if(rf_post.stage==2&&rf_post.kind<=2) {
        int ack=ready;
        if(rf_post.kind==1)ack=ack||*(const unsigned char*)((const char*)gun+0x10f0);
        else {const char* settings=rf_settings();ack=ack||(settings&&settings[0x3d]==1&&*(const unsigned char*)((const char*)gun+0xd2c)
            &&*(const unsigned char*)((const char*)gun+0x10e8)&&!*(const unsigned char*)((const char*)gun+0x11dd)&&!rf_slide_hand[1](gun));}
        if(ack){if(!rf_retire_pulse()||!rf_current(now)){reload_family_post_cancel();return 3;}rf_post.stage=3;}
        else if(g_cmd_until[rf_post.trigger_bit]!=rf_post.pulse||now>=rf_post.pulse){reload_family_post_cancel();return 3;}
    }
    if(ready) {
        // AutoPushForestockOnGrab can acknowledge readiness inside the native
        // grip callback. Let the shared pump retire its own grip first.
        if(rf_post.kind==5&&touch_weapon_cycle_busy())return 1;
        if(!rf_retire_pulse()||(rf_post.kind==3&&!rf_machine_up(gun,off,now))){reload_family_post_cancel();return 3;}
        if(rf_post.kind==3) {rf_post.stage=4;if(!rf_ready[2](gun))return 1;}
        if(rl_trigger(gun)||*(const unsigned char*)((const char*)primary+0x33c))return 1;
        if(!rf_current(now)){reload_family_post_cancel();return 3;}
        reload_family_post_cancel();return 2;
    }
    if(rf_post.kind>=5)return 1;
    if(rf_post.kind==4) {
        if(hand!=primary)return 1;
        if(!*(const unsigned char*)((const char*)gun+0xe40))return 1;
        if(!*(const unsigned char*)((const char*)gun+0xd2c)||rl_held_ammo_ready(gun,0)) {reload_family_post_cancel();return 3;}
        if(!rf_revolver_pose(now)){reload_family_post_cancel();return 3;}return 1;
    }
    if(rf_post.kind==3) {
        if(rf_post.stage==4)return 1;
        if(hand!=off)return 1;unsigned offside=*(const unsigned char*)((const char*)off+0x314);int bit=offside?27:29;
        if(g_cmd_until[bit]>now||(g_grip_latched&(1u<<bit))||!rf_hover_clear(off,rf_post.hover?rf_post.binding.object[11]:0)){reload_family_post_cancel();return 3;}
        if(!rf_post.grip_pressed) {
            void* grip=rf_post.binding.object[11];void** table=*(void***)off;
            if(f_GetCurrentGrip(off)||ti_down(off)||rf_slide_hand[2](gun)||!rf_in_valid(grip)||!ti_free(grip,offside)
                ||!table||!table[0x740/sizeof(void*)]||!((int (*)(const void*))table[0x740/sizeof(void*)])(off))return 1;
            if(!rf_machine_pose(now,1)) {reload_family_post_cancel();return 3;}
            f_SetGripForcedHover(off,grip,1);rf_post.hover=1;
            if(ti_best(off)!=grip||!rf_in_valid(grip)||!ti_free(grip,offside)||f_GetCurrentGrip(off)||ti_down(off)||!rf_current(now)) {reload_family_post_cancel();return 3;}
            ti_press(off);rf_post.grip_pressed=1;rf_post.start=now;
            if(f_GetCurrentGrip(off)!=grip||rf_slide_hand[2](gun)!=off){reload_family_post_cancel();return 3;}
        }
        if(f_GetCurrentGrip(off)!=rf_post.binding.object[11]||rf_slide_hand[2](gun)!=off||!ti_down(off)||!rf_machine_pose(now,0)){reload_family_post_cancel();return 3;}return 1;
    }
    if(hand!=primary)return 1;
    long trigger=__atomic_load_n(&g_cmd_until[rf_post.trigger_bit],__ATOMIC_ACQUIRE);
    if(rf_post.stage==2||rf_post.stage==3)return 1;
    if(!rf_edge_quiet(gun,primary,off,now)){rf_post.up_since=0;return 1;}
    if(!rf_authored_edge(gun)){reload_family_post_cancel();return 3;}
    if(!rf_post.up_since){rf_post.up_since=now;return 1;}if(now-rf_post.up_since<150)return 1;
    long live=ptr_now_ms();if(live<now||live>=rf_post.until||!rf_current(live)||rf_ready[rf_post.kind-1](gun)
        ||!rf_edge_quiet(gun,primary,off,live)||!rf_authored_edge(gun)||!rf_current(live)||!rf_authored_edge(gun)
        ||*(const unsigned char*)((const char*)gun+0xdb8)||*(const unsigned char*)((const char*)gun+0xd22)
        ||*(const unsigned char*)((const char*)gun+0xd23)||*(const unsigned char*)((const char*)primary+0x33c)
        ||!rf_inputs_clear(live)){reload_family_post_cancel();return 3;}
    long until=live+150<rf_post.until?live+150:rf_post.until;
    if(!__atomic_compare_exchange_n(&g_cmd_until[rf_post.trigger_bit],&trigger,until,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){reload_family_post_cancel();return 3;}
    rf_post.pulse=until;rf_post.stage=2;
    if(!rf_inputs_clear(live)){reload_family_post_cancel();return 3;}
    if(rf_logs<8){++rf_logs;LOGF("RELOADFAMILY normal edge kind=%d bit=%d",rf_post.kind,rf_post.trigger_bit);}return 1;
}

static void install_touch_reload_family(unsigned long base) {
    static const struct {unsigned long addr;unsigned words[2];} check[]={
        {0x6438a30,{0xd10183ff,0xf90023f3}},
        {0x6435f44,{0xd10183ff,0xf90023f3}},
        {0x6427294,{0xd10183ff,0xf90023f3}},
        {0x6434e34,{0xd10183ff,0xf90023f3}},
        {0x643a7c8,{0xd10183ff,0xf90023f3}},
        {0x643bd4c,{0xd10183ff,0xf90023f3}},
        {0x6438ab8,{0x52822204,0x52800205}},
        {0x6435fcc,{0x52824204,0x52800205}},
        {0x642731c,{0x52822a04,0x52800205}},
        {0x6434ebc,{0x5281ca04,0x52800205}},
        {0x643a850,{0x52823404,0x52800205}},
        {0x643bdd4,{0x52821a04,0x52800205}},
        {0x8f4e5bc,{0xd10183ff,0xf90023f3}},
        {0x8f4e644,{0x5280f404,0x52800205}},
        {0x8f82500,{0xd10183ff,0xf90023f3}},
        {0x62a8e50,{0xf81e0ff3,0xa9017bfd}},
        {0x62c7790,{0x52821e08,0x38686808}},
        {0x62c40ec,{0x52823908,0x38686808}},
        {0x62a1dc4,{0x52822388,0x38686808}},
        {0x62c23a4,{0x39790008,0x34000068}},
        {0x62c9c78,{0x52822c28,0x38686808}},
        {0x62cbf98,{0x52821988,0x38686808}},
        {0x6291744,{0xa9be4ff4,0xa9017bfd}},
        {0x629182c,{0xf81e0ff3,0xa9017bfd}},
        {0x62918d4,{0xf81d0ff5,0xa9014ff4}},
        {0x6280060,{0xd10143ff,0xf90013f5}},
        {0x628e55c,{0x39710008,0x34000068}},
        {0x62bb880,{0xf81e0ff3,0xa9017bfd}},
        {0x62bb584,{0xf81e0ff3,0xa9017bfd}},
        {0x628db4c,{0xfc1e0fe8,0xf90007f3}},
        {0x62c763c,{0xf9486400,0x17fef900}},
        {0x62c3588,{0xf9486400,0x17fefc09}},
        {0x62a1ca4,{0xf9486400,0x17ff8242}},
        {0x62825b0,{0xf9417c08,0xb4000268}},
        {0x83c6310,{0xd10643ff,0xfd0083ee}},
        {0x6281040,{0xf9411809,0xb40002c9}},
        {0x6299ee8,{0xa9be4ff4,0xa9017bfd}},
        {0x62c7e04,{0xa9be4ff4,0xa9017bfd}},
        {0x62c7614,{0x52800028,0xaa1303e0}},
        {0x62c7620,{0x97ff1061,0x1e2703e0}},
        {0x62c3170,{0x97ff218d,0x52824088}},
        {0x62c3dbc,{0xaa1303e0,0x9405c6c7}},
        {0x62a1b18,{0x390022e8,0x97ffa722}},
        {0x62c1fc8,{0x1e202168,0x1e200981}},
        {0x62c2004,{0x94000004,0x52800035}},
        {0x62a1678,{0xfd000140,0xbd000941}},
        {0x62a1694,{0xd63f0120,0xf9400be8}},
        {0x83a926c,{0xd10303ff,0xa9085ff8}},
        {0x629a7cc,{0xf81a0ffb,0xa90167fa}},
        {0x633c54c,{0x52800028,0x390cf408}},
        {0x633c558,{0x390cf41f,0x1405d0a2}},
        {0x62ca018,{0xa9be4ff4,0xa9017bfd}},
        {0x62ca020,{0x910043fd,0x52822608}},
        {0x62cbfb0,{0xf81e0ff3,0xa9017bfd}},
        {0x62cbfb8,{0x910043fd,0x528218a8}},
        {0x643a7a8,{0x52822608,0x52800029}},
        {0x643a7b0,{0x38286809,0xd65f03c0}},
        {0x643bd3c,{0x528218a8,0x52800029}},
        {0x643bd44,{0x38286809,0xd65f03c0}},
    };
    static const struct {unsigned long addr;unsigned offset;} fields[]={
        {0x9da58e0,0xd60},
        {0x9db2d78,0xd38},
        {0x9da8918,0x10c8},
        {0x9db2b30,0xda0},
        {0x9db3ac0,0x10c8},
        {0x9db4e50,0x10c8},
        {0x9da88c8,0x10d8},
        {0x9da88a0,0x10e0},
        {0x9da8878,0x10e8},
        {0x9da7038,0x460},
        {0x9db6fa8,0x1134},
        {0x9db7968,0x10c8},
    };
    __atomic_store_n(&rf_single_shell_ready,0,__ATOMIC_RELEASE);
    rf_capacity=rf_reserve=0;
    memset(rf_classes,0,sizeof rf_classes);memset(rf_ready,0,sizeof rf_ready);memset(&rf_post,0,sizeof rf_post);
    for(unsigned i=0;i<sizeof check/sizeof *check;i++)if(memcmp((const void*)(base+check[i].addr),check[i].words,8)) {
        LOGF("RELOADFAMILY disabled: opcode mismatch +%lx",check[i].addr);return;
    }
    for(unsigned i=0;i<sizeof fields/sizeof *fields;i++)if(*(const unsigned*)(base+fields[i].addr+36)!=fields[i].offset) {
        LOGF("RELOADFAMILY disabled: field mismatch +%lx",fields[i].addr);return;
    }
    if(!ti_ready||(uintptr_t)rl_hand_class!=base+0x6424b5c||(uintptr_t)rl_pawn_class!=base+0x642c718
        ||(uintptr_t)rl_grip_class!=base+0x64192f4||(uintptr_t)rl_ammo_class!=base+0x640dea8
        ||(uintptr_t)rl_player!=base+0x633d030||(uintptr_t)rl_active!=base+0x63380dc
        ||(uintptr_t)rl_left!=base+0x62a8db4||(uintptr_t)rl_primary!=base+0x62bbc30
        ||(uintptr_t)rl_get_pawn!=base+0x62bbd24||(uintptr_t)rl_paused!=base+0x61d0934
        ||(uintptr_t)rl_player_valid!=base+0x61d401c||(uintptr_t)rl_interactivity!=base+0x628cf08
        ||(uintptr_t)rl_reloading!=base+0x628c410||(uintptr_t)rl_world_transform!=base+0x83a926c
        ||(uintptr_t)rl_base_ready!=base+0x628d268||(uintptr_t)rl_trigger!=base+0x628d978
        ||(uintptr_t)rl_held_ammo_ready!=base+0x628e090||(uintptr_t)f_GetCurrentGrip!=base+0x629a774
        ||(uintptr_t)f_SetGripForcedHover!=base+0x629a7cc||(uintptr_t)ti_parent!=base+0x6280170
        ||(uintptr_t)ti_free!=base+0x627feb8||(uintptr_t)ti_best!=base+0x629a930
        ||(uintptr_t)ti_press!=base+0x633c54c||(uintptr_t)ti_release!=base+0x633c558||(uintptr_t)ti_down!=base+0x633d048) {
        LOGF("RELOADFAMILY disabled: native dependency mismatch (Gun interactivity=%p expected=%p)",
            (void*)rl_interactivity,(void*)(base+0x628cf08));return;
    }
    rf_classes[0]=(void* (*)(void))(base+0x6438a30);
    rf_classes[1]=(void* (*)(void))(base+0x6435f44);
    rf_classes[2]=(void* (*)(void))(base+0x6427294);
    rf_classes[3]=(void* (*)(void))(base+0x6434e34);
    rf_classes[4]=(void* (*)(void))(base+0x643a7c8);
    rf_classes[5]=(void* (*)(void))(base+0x643bd4c);
    rf_ready[0]=(int (*)(const void*))(base+0x62c7790);
    rf_ready[1]=(int (*)(const void*))(base+0x62c40ec);
    rf_ready[2]=(int (*)(const void*))(base+0x62a1dc4);
    rf_ready[3]=(int (*)(const void*))(base+0x62c23a4);
    rf_ready[4]=(int (*)(const void*))(base+0x62c9c78);
    rf_ready[5]=(int (*)(const void*))(base+0x62cbf98);
    rf_scene_class=(void* (*)(void))(base+0x8f82500);rf_mesh_class=(void* (*)(void))(base+0x8f4e5bc);
    rf_right=(void* (*)(const void*))(base+0x62a8e50);rf_count=(int (*)(const void*,int))(base+0x6291744);
    rf_in_valid=(int (*)(const void*))(base+0x6280060);rf_recoil=(int (*)(const void*))(base+0x628e55c);
    rf_blend=(int (*)(const void*))(base+0x62bb880);rf_flourish=(int (*)(const void*))(base+0x62bb584);
    rf_ammo_held=(int (*)(const void*,float))(base+0x628db4c);
    rf_slide_hand[0]=(void* (*)(const void*))(base+0x62c763c);rf_slide_hand[1]=(void* (*)(const void*))(base+0x62c3588);
    rf_slide_hand[2]=(void* (*)(const void*))(base+0x62a1ca4);rf_grip_hand=(void* (*)(const void*))(base+0x62825b0);
    rf_socket=(struct reload_tf (*)(const void*,unsigned long long,int))(base+0x83c6310);
    rf_grip_target=(struct reload_tf (*)(const void*))(base+0x6281040);
    rf_grab_location=(struct rf_vec (*)(const void*))(base+0x6299ee8);rf_settings=(const char* (*)(void))(base+0x62c7e04);
    rf_capacity=(int (*)(const void*))(base+0x629182c);rf_reserve=(int (*)(const void*))(base+0x62918d4);
#if defined(__aarch64__)
    g_tgt_rf_shotgun_reload=base+0x62ca018+16;g_tgt_rf_striker_reload=base+0x62cbfb0+16;
    if(patch_code((void*)(base+0x62ca018),(void*)hook_rf_shotgun_reload)
        || patch_code((void*)(base+0x62cbfb0),(void*)hook_rf_striker_reload)){
        LOGF("RELOADSHELL disabled: native shell hook failed");return;
    }
#endif
    __atomic_store_n(&rf_single_shell_ready,1,__ATOMIC_RELEASE);
    wc_reload_pending=rf_shotgun_reload_pending;
    LOGF("RELOADFAMILY native metadata/readiness and explicit post-magazine completion ready");
}
