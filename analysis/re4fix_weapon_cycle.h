// Include after the weapon/interact helpers; the caller supplies touch_equipped_gun.
#include <stdint.h>
static void* touch_equipped_gun(void* hand);
struct wc_vec { float x,y,z; };
static void* (*wc_shotgun_class)(void), *(*wc_scene_class)(void), *(*wc_group_class)(void);
static void* (*wc_right)(const void*), *(*wc_forestock_hand)(const void*);
static const void* (*wc_settings)(void), *(*wc_group_grips)(const void*);
static int (*wc_in_valid_grips)(const void*), (*wc_recoiling)(const void*);
static int (*wc_ammo_held)(const void*,float), (*wc_blending)(const void*), (*wc_flourishing)(const void*);
static struct reload_tf (*wc_grip_target)(const void*);
static struct wc_vec (*wc_grab_location)(const void*), (*wc_start)(const void*), (*wc_end)(const void*);
static struct wc_vec (*wc_local_forestock)(const void*,float*,const void*);
static int (*wc_reload_pending)(const void*);   // Native shell insertion may leave the same shotgun stock open.
struct wc_binding { void* object[12]; unsigned long long name[12]; };
static struct { struct wc_binding binding,blocked; long started,last,until,retry_at; int phase,pressed,hover,failed; } wc_cycle;
// Single atomic publication: frame stamp, remaining lifetime and actual off-hand.
// The controller reader never dereferences game objects or calls native code.
static unsigned long long wc_input_plan;
static struct {unsigned seq,bits[3],side;long stamp;} wc_tracking_pose;
static struct {unsigned seq,bits[3],side,generation;long stamp,start;} wc_return_pose;
static struct {long start;unsigned generation;int active;} wc_return;
static unsigned wc_pose_generation;
static unsigned wc_logs;
static unsigned wc_native_pose_logs;
static int g_pump_ms[2] = {230, 200};   // pump stroke: back (eject), forward (chamber); visible, not instant
static int wc_input_bit(const void* hand) {return *(const unsigned char*)((const char*)hand+0x314) ? 27 : 29;}
static unsigned wc_input_mask(const void* hand) {return 1u<<wc_input_bit(hand);}
static void touch_weapon_cycle_input_reset(void) {
    __atomic_add_fetch(&wc_pose_generation,1,__ATOMIC_ACQ_REL);
    __atomic_store_n(&wc_input_plan,0,__ATOMIC_RELEASE);
    __atomic_store_n(&wc_tracking_pose.stamp,0,__ATOMIC_RELEASE);
    __atomic_store_n(&wc_return_pose.stamp,0,__ATOMIC_RELEASE);
}
static int wc_pose_context(int side,long now,int menu) {
    if(now<=0||menu||side<0||side>1||__atomic_load_n(&g_touch_context,__ATOMIC_ACQUIRE)!=1
        ||__atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE)
        ||__atomic_load_n(&g_touch_weapon_request,__ATOMIC_ACQUIRE)
        ||__atomic_load_n(&g_touch_reload_request,__ATOMIC_ACQUIRE)
        ||__atomic_load_n(&g_touch_interact_request,__ATOMIC_ACQUIRE)
        ||__atomic_load_n(&g_touch_inventory_request,__ATOMIC_ACQUIRE)
        ||__atomic_load_n(&g_touch_pause,__ATOMIC_ACQUIRE))return 0;
    int phase=__atomic_load_n(&g_reload_phase,__ATOMIC_ACQUIRE),bit=side?27:29;
    if((phase&&phase!=7)||__atomic_load_n(&g_cmd_until[bit],__ATOMIC_ACQUIRE)>now
        ||(__atomic_load_n(&g_grip_latched,__ATOMIC_ACQUIRE)&(1u<<bit)))return 0;
    return 1;
}
static unsigned touch_weapon_cycle_input_mask(long now,int menu) {
    unsigned long long plan=__atomic_load_n(&wc_input_plan,__ATOMIC_ACQUIRE);
    unsigned side=(unsigned)plan&3,duration=(unsigned)plan>>2;
    unsigned age=(unsigned)now-(unsigned)(plan>>32);
    if(!plan||side<1||side>2||!duration||duration>1800||age>250||age>=duration
        ||!wc_pose_context((int)side-1,now,menu))return 0;
    return 1u<<(side==2?27:29);
}
static void wc_publish_input(const void* hand,long now) {
    long remaining=wc_cycle.until-now;
    unsigned side=*(const unsigned char*)((const char*)hand+0x314);
    if(now<=0||remaining<=0||remaining>1800||side>1){touch_weapon_cycle_input_reset();return;}
    __atomic_store_n(&wc_input_plan,((unsigned long long)(unsigned)now<<32)|((unsigned)remaining<<2)|(side+1),__ATOMIC_RELEASE);
}
static int wc_read_tracking_pose(int side,long now,float pose[3]) {
    for(int attempt=0;attempt<2;attempt++) {
        unsigned seq=__atomic_load_n(&wc_tracking_pose.seq,__ATOMIC_ACQUIRE);if(seq&1)continue;
        long stamp=__atomic_load_n(&wc_tracking_pose.stamp,__ATOMIC_RELAXED);
        unsigned actual=__atomic_load_n(&wc_tracking_pose.side,__ATOMIC_RELAXED),bits[3];
        for(int i=0;i<3;i++)bits[i]=__atomic_load_n(&wc_tracking_pose.bits[i],__ATOMIC_RELAXED);
        if(seq!=__atomic_load_n(&wc_tracking_pose.seq,__ATOMIC_ACQUIRE))continue;
        float norm=0;memcpy(pose,bits,3*sizeof(float));
        for(int i=0;i<3;i++)norm+=pose[i]*pose[i];
        if(!stamp||now<stamp||now-stamp>250||actual!=(unsigned)side||!isfinite(norm)||norm>2.25f)return 0;
        return 1;
    }
    return 0;
}
static void touch_weapon_cycle_pose(int side,long now,float off[3]) {
    if(!off||!wc_pose_context(side,now,ptr_menu_now()))return;
    float pose[3];
    if((touch_weapon_cycle_input_mask(now,0)&(1u<<(side?27:29))) && wc_read_tracking_pose(side,now,pose)) {
        memcpy(off,pose,sizeof pose);return;
    }
    // Native release happens at the real endpoint. Only presentation blends back to the
    // caller's current ordinary hand position; no grip input or weapon state is extended.
    for(int attempt=0;attempt<2;attempt++) {
        unsigned seq=__atomic_load_n(&wc_return_pose.seq,__ATOMIC_ACQUIRE);if(seq&1)continue;
        long stamp=__atomic_load_n(&wc_return_pose.stamp,__ATOMIC_RELAXED),start=__atomic_load_n(&wc_return_pose.start,__ATOMIC_RELAXED);
        unsigned actual=__atomic_load_n(&wc_return_pose.side,__ATOMIC_RELAXED),generation=__atomic_load_n(&wc_return_pose.generation,__ATOMIC_RELAXED),bits[3];
        for(int i=0;i<3;i++)bits[i]=__atomic_load_n(&wc_return_pose.bits[i],__ATOMIC_RELAXED);
        if(seq!=__atomic_load_n(&wc_return_pose.seq,__ATOMIC_ACQUIRE))continue;
        memcpy(pose,bits,sizeof pose);float norm=0;
        for(int i=0;i<3;i++){if(!isfinite(off[i]))return;norm+=pose[i]*pose[i];}
        if(!stamp||now<stamp||now-stamp>250||now<start||now-start>=220||actual!=(unsigned)side
            ||generation!=__atomic_load_n(&wc_pose_generation,__ATOMIC_ACQUIRE)||!isfinite(norm)||norm>2.25f)return;
        float t=(float)(now-start)/220.0f;t=t*t*(3.0f-2.0f*t);
        for(int i=0;i<3;i++)off[i]=pose[i]+(off[i]-pose[i])*t;
        return;
    }
}

static int wc_typed(const void* object,const void* expected,int minimum) {
    return expected && reload_layout(object,minimum) && reload_is_a(object,expected);
}
static int wc_grip_owned(void* grip,void* gun) {
    return reload_grip_registered(grip) && wc_typed(grip,tw_grip_class(),0x460)
        && *(void* const*)((const char*)grip+0x20)==gun
        && *(void* const*)((const char*)grip+0x300)==gun && ti_parent(grip);
}
// A two-handed gun changes its actor rotation independently of the primary
// hand. Cache neutral FireBone in this rigid actor frame, outside mesh recoil.
static void* touch_weapon_cycle_pose_reference(void* primary,void* gun,struct reload_tf* reference) {
    if(!reference || !primary || !wc_typed(gun,wc_shotgun_class ? wc_shotgun_class():0,0x11a0)
        || !wc_scene_class || !rl_primary || !f_GetCurrentGrip || rl_primary(gun)!=primary)return 0;
    void* grip=*(void* const*)((const char*)gun+0x270);
    if(!wc_grip_owned(grip,gun) || f_GetCurrentGrip(primary)!=grip)return 0;
    void* root=*(void* const*)((const char*)gun+0x130);
    if(!wc_typed(root,wc_scene_class(),0x1f0) || *(void* const*)((const char*)root+0x20)!=gun)return 0;
    memcpy(reference,(const char*)root+0x1c0,sizeof *reference);
    return reload_tf_valid(reference) ? root:0;
}
static int wc_forced_hover(void* hand,void* expected) {
    void* const* entries=*(void* const* const*)((const char*)hand+0x460);
    int count=*(const int*)((const char*)hand+0x468),max=*(const int*)((const char*)hand+0x46c);
    return count==0 ? max>=0 && max<=64 : count==1 && max>=1 && max<=64 && entries && entries[0]==expected;
}
static void* wc_find_forestock(void* gun,void* held,int side,void** group_out) {
    void* group=*(void* const*)((const char*)gun+0xd38);
    if (!wc_typed(group,wc_group_class(),0x280) || *(void* const*)((const char*)group+0x20)!=gun) return 0;
    const char* list=wc_group_grips(group);
    if (!list) return 0;
    void* const* items=*(void* const* const*)list;
    int count=*(const int*)(list+8),max=*(const int*)(list+12);
    if (count<0 || count>32 || max<count || max>128 || (count && !items)) return 0;
    *group_out=group;
    if (!count) {
        void* grip=*(void* const*)((const char*)gun+0xd40);
        return wc_grip_owned(grip,gun) && (!held || grip==held) ? grip : 0;
    }
    for (int i=0;i<count;i++) {
        void* grip=items[i];
        if (wc_grip_owned(grip,gun) && (held ? held==grip : ti_free(grip,side) && wc_in_valid_grips(grip))) return grip;
    }
    return 0;
}
// The first hand is the actual off-hand, for either authored handedness.
static int wc_bind_mode(void* left,struct wc_binding* out,void* released_grip) {
    if (!wc_shotgun_class || !tw_hand_class || !tw_pawn_class || !tw_grip_class
        || !wc_scene_class || !wc_group_class || !wc_group_grips || !wc_right || !wc_forestock_hand
        || !wc_settings || !wc_in_valid_grips || !wc_grip_target || !wc_grab_location || !wc_start || !wc_end
        || !phone_player || !phone_active || !phone_type || !rl_left || !rl_primary || !f_GetCurrentGrip
        || !ti_parent || !ti_free || !wc_typed(left,tw_hand_class(),0x510)) return 0;
    unsigned side=*(const unsigned char*)((const char*)left+0x314);
    if (side>1) return 0;
    void* pawn=phone_player(left);
    if (!wc_typed(pawn,tw_pawn_class(),0x10d0) || pawn!=phone_active()
        || (side ? wc_right(pawn) : rl_left(pawn))!=left) return 0;
    void* right=side ? rl_left(pawn) : wc_right(pawn);
    if (!wc_typed(right,tw_hand_class(),0x510) || *(const unsigned char*)((const char*)right+0x314)!=1-side
        || phone_player(right)!=pawn) return 0;
    void* gun=touch_equipped_gun(right);
    if (!wc_typed(gun,wc_shotgun_class(),0x11a0) || *(void* const*)((const char*)gun+0x3c0)!=pawn) return 0;
    void* primary=*(void* const*)((const char*)gun+0x270),*group=0;
    if (!wc_grip_owned(primary,gun) || f_GetCurrentGrip(right)!=primary || rl_primary(gun)!=right) return 0;
    void* held=f_GetCurrentGrip(left);
    if(released_grip && held)return 0;
    void* grip=wc_find_forestock(gun,released_grip ? released_grip:held,side,&group);
    if (!grip || grip==primary) return 0;
    void* root=*(void* const*)((const char*)left+0x130),*gun_root=*(void* const*)((const char*)gun+0x130);
    void* right_root=*(void* const*)((const char*)right+0x130),*target=*(void* const*)((const char*)grip+0x230);
    void* head=*(void* const*)((const char*)pawn+0x2d8);
    if (!wc_typed(root,wc_scene_class(),0x1f0) || *(void* const*)((const char*)root+0x20)!=left
        || !wc_typed(gun_root,wc_scene_class(),0x1f0) || *(void* const*)((const char*)gun_root+0x20)!=gun
        || !wc_typed(right_root,wc_scene_class(),0x1f0) || *(void* const*)((const char*)right_root+0x20)!=right
        || !wc_typed(target,wc_scene_class(),0x1f0) || *(void* const*)((const char*)target+0x20)!=gun
        || !wc_typed(head,wc_scene_class(),0x1f0) || *(void* const*)((const char*)head+0x20)!=pawn) return 0;
    void* objects[12]={left,right,pawn,gun,primary,grip,root,gun_root,right_root,group,target,head};
    for (int i=0;i<12;i++) {
        out->object[i]=objects[i];memcpy(&out->name[i],(const char*)objects[i]+0x18,8);
    }
    return 1;
}
static int wc_bind(void* left,struct wc_binding* out) {return wc_bind_mode(left,out,0);}
static int wc_inputs_clear(void) {
    return __atomic_load_n(&g_touch_context,__ATOMIC_ACQUIRE)==1
        && !__atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE)
        && !__atomic_load_n(&g_touch_weapon_request,__ATOMIC_ACQUIRE)
        && !__atomic_load_n(&g_touch_reload_request,__ATOMIC_ACQUIRE)
        && (!g_reload_phase || (g_reload_phase==7 && wc_reload_pending && wc_reload_pending(0)))
        && !__atomic_load_n(&g_touch_interact_request,__ATOMIC_ACQUIRE)
        && !__atomic_load_n(&g_touch_inventory_request,__ATOMIC_ACQUIRE)
        && !__atomic_load_n(&g_touch_pause,__ATOMIC_ACQUIRE);
}
static int wc_allowed(void) {
    const char* settings=wc_settings ? wc_settings() : 0;
    return settings && settings[0x3d]==1 && rl_paused && rl_player_valid && rl_interactivity && rl_reloading
        && wc_recoiling && wc_ammo_held && wc_blending && wc_flourishing
        && phone_type && phone_type()==1 && !ptr_menu_now() && !rl_paused() && rl_player_valid() && wc_inputs_clear();
}
static int wc_current_mode(const struct wc_binding* binding,int returning) {
    struct wc_binding current;
    return wc_allowed() && wc_bind_mode(binding->object[0],&current,returning ? binding->object[5]:0)
        && !memcmp(binding,&current,sizeof current) && (!g_reload_phase || wc_reload_pending(binding->object[3]))
        && rl_interactivity(binding->object[3])==2 && !rl_reloading(binding->object[3])
        && !wc_ammo_held(binding->object[3],0.0f) && !wc_flourishing(binding->object[3]) && wc_inputs_clear()
        && (!returning || (!f_GetCurrentGrip(binding->object[0]) && !ti_down(binding->object[0])
            && !wc_forestock_hand(binding->object[3]) && wc_forced_hover(binding->object[0],0)));
}
static int wc_current(const struct wc_binding* binding) {return wc_current_mode(binding,0);}
static void wc_finish(void* left,int failed) {
    long now=ptr_now_ms();float from[3];
    int returning=!failed && wc_current(&wc_cycle.binding)
        && wc_read_tracking_pose(*(const unsigned char*)((const char*)left+0x314),now,from);
    touch_weapon_cycle_input_reset();
    unsigned generation=__atomic_load_n(&wc_pose_generation,__ATOMIC_ACQUIRE);wc_return.active=0;
    unsigned long long name=0;
    if (wc_typed(left,tw_hand_class(),0x510)) memcpy(&name,(const char*)left+0x18,8);
    if (left==wc_cycle.binding.object[0] && name==wc_cycle.binding.name[0]) {
        void* current=f_GetCurrentGrip(left);
        unsigned long long grip_name=0;
        if (current==wc_cycle.binding.object[5] && reload_grip_registered(current)
            && wc_typed(current,tw_grip_class(),0x460)) memcpy(&grip_name,(const char*)current+0x18,8);
        int owns_current=!current || (current==wc_cycle.binding.object[5] && grip_name==wc_cycle.binding.name[5]);
        if (wc_cycle.pressed && owns_current && g_cmd_until[wc_input_bit(left)]<=ptr_now_ms()
            && !(g_grip_latched&wc_input_mask(left)) && ti_down(left)) ti_release(left);
        void* grip=wc_cycle.binding.object[5];
        if (wc_cycle.hover && reload_grip_registered(grip) && wc_typed(grip,tw_grip_class(),0x460)) {
            memcpy(&grip_name,(const char*)grip+0x18,8);
            if (grip_name==wc_cycle.binding.name[5]) f_SetGripForcedHover(left,grip,0);
        }
    }
    if (failed) {wc_cycle.blocked=wc_cycle.binding;wc_cycle.failed=1;wc_cycle.retry_at=ptr_now_ms()+450;}
    wc_cycle.phase=wc_cycle.pressed=wc_cycle.hover=0;
    if(returning && wc_current_mode(&wc_cycle.binding,1)
        && generation==__atomic_load_n(&wc_pose_generation,__ATOMIC_ACQUIRE)) {
        unsigned bits[3];memcpy(bits,from,sizeof bits);
        wc_return.start=now;wc_return.generation=generation;wc_return.active=1;
        __atomic_add_fetch(&wc_return_pose.seq,1,__ATOMIC_ACQ_REL);
        for(int i=0;i<3;i++)__atomic_store_n(&wc_return_pose.bits[i],bits[i],__ATOMIC_RELAXED);
        __atomic_store_n(&wc_return_pose.side,*(const unsigned char*)((const char*)left+0x314),__ATOMIC_RELAXED);
        __atomic_store_n(&wc_return_pose.generation,generation,__ATOMIC_RELAXED);
        __atomic_store_n(&wc_return_pose.start,now,__ATOMIC_RELAXED);
        __atomic_store_n(&wc_return_pose.stamp,now,__ATOMIC_RELAXED);
        __atomic_add_fetch(&wc_return_pose.seq,1,__ATOMIC_RELEASE);
    }
    if (wc_logs<8) {++wc_logs;LOGF("CYCLE pump %s",failed ? "retired" : "finished");}
}
static int wc_pose_safe(const struct wc_binding* b,const struct reload_tf* pose) {
    struct reload_tf right;memcpy(&right,(const char*)b->object[8]+0x1c0,sizeof right);
    if (!reload_tf_valid(pose) || !reload_tf_valid(&right)) return 0;
    float distance=0;
    for (int i=0;i<3;i++) {float d=pose->p[i]-right.p[i];distance+=d*d;}
    return isfinite(distance) && distance<=150.0f*150.0f;
}
static int wc_publish_pose(const struct wc_binding* b,const struct reload_tf* pose) {
    struct reload_tf head;memcpy(&head,(const char*)b->object[11]+0x1c0,sizeof head);
    if(!reload_tf_valid(&head)||!reload_tf_valid(pose))return 0;
    float world[3],local[3];for(int i=0;i<3;i++)world[i]=pose->p[i]-head.p[i];
    q_rot((qf_t){-head.q.x,-head.q.y,-head.q.z,head.q.w},world,local);
    // UE: X forward,Y right,Z up in cm. OVR: X right,Y up,-Z forward in m.
    float off[3]={local[1]*.01f,local[2]*.01f,-local[0]*.01f},norm=0;
    for(int i=0;i<3;i++)norm+=off[i]*off[i];
    if(!isfinite(norm)||norm>2.25f||!wc_current(b))return 0;
    unsigned bits[3];memcpy(bits,off,sizeof bits);
    __atomic_add_fetch(&wc_tracking_pose.seq,1,__ATOMIC_ACQ_REL);
    for(int i=0;i<3;i++)__atomic_store_n(&wc_tracking_pose.bits[i],bits[i],__ATOMIC_RELAXED);
    __atomic_store_n(&wc_tracking_pose.side,*(const unsigned char*)((const char*)b->object[0]+0x314),__ATOMIC_RELAXED);
    __atomic_store_n(&wc_tracking_pose.stamp,ptr_now_ms(),__ATOMIC_RELAXED);
    __atomic_add_fetch(&wc_tracking_pose.seq,1,__ATOMIC_RELEASE);return 1;
}
static int wc_guide(const struct wc_binding* b) {
    struct reload_tf pose,target;memcpy(&pose,(const char*)b->object[6]+0x1c0,sizeof pose);
    target=wc_grip_target(b->object[5]);
    struct wc_vec grab=wc_grab_location(b->object[0]);float location[3]={grab.x,grab.y,grab.z};
    if (!reload_tf_valid(&target)) return 0;
    for (int i=0;i<3;i++) {if (!isfinite(location[i])) return 0;pose.p[i]+=target.p[i]-location[i];}
    if (!wc_pose_safe(b,&pose) || !wc_current(b)) return 0;
    rl_world_transform(b->object[6],&pose,0,0,0);return wc_publish_pose(b,&pose);
}
static int wc_stroke(const struct wc_binding* b,float t,int back) {
    struct reload_tf pose,gun;memcpy(&pose,(const char*)b->object[6]+0x1c0,sizeof pose);
    memcpy(&gun,(const char*)b->object[7]+0x1c0,sizeof gun);
    struct wc_vec from=wc_start(b->object[3]),to=wc_end(b->object[3]);
    float a[3]={from.x,from.y,from.z},z[3]={to.x,to.y,to.z},local[3],world[3],length=0;
    if (!reload_tf_valid(&gun)) return 0;
    t=fminf(1.0f,fmaxf(0.0f,t));t=t*t*(3.0f-2.0f*t);
    // Native gates compare exactly to 0/1. Apply the floating-point endpoint
    // margin only at the end, so the original gate cannot shorten the stroke.
    float fraction=t>=1.0f ? 1.02f:t;
    if(back)fraction=1.0f-fraction;
    for (int i=0;i<3;i++) {
        // Native ForestockGripGrabbed stores this tightly packed FVector at
        // 10f4+88=117c (XY),1184 (Z); GetLocalForestockPosition reads it there.
        float offset=*(const float*)((const char*)b->object[3]+0x117c+4*i),d=z[i]-a[i];
        if (!isfinite(a[i]) || !isfinite(z[i]) || !isfinite(offset)) return 0;
        length+=d*d;local[i]=a[i]+fraction*d+offset;
    }
    if (!isfinite(length) || length<0.01f || length>100.0f*100.0f) return 0;
    q_rot(gun.q,local,world);for (int i=0;i<3;i++) pose.p[i]=gun.p[i]+world[i];
    if (!wc_pose_safe(b,&pose) || !wc_current(b)) return 0;
    rl_world_transform(b->object[6],&pose,0,0,0);return wc_publish_pose(b,&pose);
}
// The native gun is attached to the primary hand, while the other tracking
// root is independent. Move both by the same rigid correction or the next
// UpdateTwoHandedGrip aims the gun at the previous frame's off-hand position.
static int touch_weapon_cycle_primary_transform(void* primary,void* gun,void* root,
                                               const struct reload_tf* before,const struct reload_tf* after) {
    const struct wc_binding* b=&wc_cycle.binding;
    if(!wc_cycle.phase || primary!=b->object[1] || gun!=b->object[3] || root!=b->object[8]
        || !before || !after || !reload_tf_valid(before) || !reload_tf_valid(after) || !wc_current(b))return 0;
    struct reload_tf off;memcpy(&off,(const char*)b->object[6]+0x1c0,sizeof off);
    if(!reload_tf_valid(&off)) {wc_finish(b->object[0],1);return 0;}
    qf_t delta=q_mul(after->q,(qf_t){-before->q.x,-before->q.y,-before->q.z,before->q.w});
    float relative[3],moved[3],distance=0;
    for(int i=0;i<3;i++)relative[i]=off.p[i]-before->p[i];
    q_rot(delta,relative,moved);off.q=q_mul(delta,off.q);
    for(int i=0;i<3;i++){off.p[i]=after->p[i]+moved[i];distance+=moved[i]*moved[i];}
    if(!reload_tf_valid(&off) || !isfinite(distance) || distance>150.0f*150.0f) {
        wc_finish(b->object[0],1);return 0;
    }
    rl_world_transform(root,after,0,0,0);
    // A native setter can dispatch callbacks: never reuse the binding after a
    // menu transition, release or object/name change.
    if(!wc_current(b)) {wc_finish(b->object[0],1);return 1;}
    rl_world_transform(b->object[6],&off,0,0,0);
    if(!wc_publish_pose(b,&off))wc_finish(b->object[0],1);
    return 1;
}
static int touch_weapon_cycle_busy(void) {return wc_cycle.phase!=0;}
static void touch_weapon_cycle_cancel(void* hand) {
    int gt=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE);
    if (gt>0 && gettid()==gt && wc_cycle.phase && hand==wc_cycle.binding.object[0]) wc_finish(hand,1);
    else if(gt>0 && gettid()==gt && wc_return.active && hand==wc_cycle.binding.object[0]) {
        wc_return.active=0;touch_weapon_cycle_input_reset();
    }
}
static void touch_weapon_cycle_tick(void* hand,int side,long now) {
    int gt=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE);
    if (gt<=0 || gettid()!=gt || !wc_shotgun_class || !hand || side<0 || side>1
        || !wc_typed(hand,tw_hand_class(),0x510)
        || *(const unsigned char*)((const char*)hand+0x314)!=(unsigned)side) return;
    if(wc_return.active && hand==wc_cycle.binding.object[0]) {
        if(now<wc_return.start || now-wc_return.start>=220
            || wc_return.generation!=__atomic_load_n(&wc_pose_generation,__ATOMIC_ACQUIRE)
            || !__atomic_load_n(&wc_return_pose.stamp,__ATOMIC_ACQUIRE)
            || !wc_pose_context(side,now,ptr_menu_now()) || !wc_current_mode(&wc_cycle.binding,1)) {
            wc_return.active=0;__atomic_store_n(&wc_return_pose.stamp,0,__ATOMIC_RELEASE);
        } else __atomic_store_n(&wc_return_pose.stamp,now,__ATOMIC_RELEASE);
    }
    if (wc_cycle.phase && hand!=wc_cycle.binding.object[0]) return;
    struct wc_binding binding;
    if (!wc_allowed() || !wc_bind(hand,&binding)) {
        if (wc_cycle.phase) wc_finish(hand,1);
        else if (hand==wc_cycle.blocked.object[0]) wc_cycle.failed=0;
        return;
    }
    void* gun=binding.object[3];unsigned stock=*(const unsigned char*)((const char*)gun+0x1160);
    unsigned chamber=*(const unsigned char*)((const char*)gun+0x1161);
    if (wc_cycle.failed && (memcmp(&binding,&wc_cycle.blocked,sizeof binding) || (stock==0 && chamber!=1)
        || now>=wc_cycle.retry_at)) wc_cycle.failed=0;
    if (wc_cycle.phase) {
        long grip_input=__atomic_load_n(&g_cmd_until[wc_input_bit(hand)],__ATOMIC_ACQUIRE);
        if (memcmp(&binding,&wc_cycle.binding,sizeof binding) || now<wc_cycle.last || now>=wc_cycle.until
            || !wc_current(&binding) || grip_input>now || (g_grip_latched&wc_input_mask(hand))
            || !wc_forced_hover(hand,binding.object[5]) || f_GetCurrentGrip(hand)!=binding.object[5]
            || wc_forestock_hand(gun)!=hand || !ti_down(hand) || stock>1 || chamber>2) {
            if(wc_logs<16) {++wc_logs;LOGF("CYCLE interrupted elapsed=%ld phase=%d stock=%u chamber=%u current=%p forestock=%p down=%d input=%ld hold=%x hover=%d live=%d",
                now-wc_cycle.started,wc_cycle.phase,stock,chamber,f_GetCurrentGrip(hand),wc_forestock_hand(gun),
                ti_down(hand),grip_input,touch_weapon_cycle_input_mask(now,0),wc_forced_hover(hand,binding.object[5]),wc_current(&binding));}
            wc_finish(hand,1);return;
        }
        wc_cycle.last=now;
        if (wc_cycle.phase==1 && stock==1) {wc_cycle.phase=2;wc_cycle.started=now;}
        if (wc_cycle.phase==2 && stock==0) {wc_finish(hand,0);return;}
        wc_publish_input(hand,now);
        if (!wc_stroke(&binding,(float)(now-wc_cycle.started)/(float)g_pump_ms[wc_cycle.phase==2],wc_cycle.phase==2)) wc_finish(hand,1);
        return;
    }
    if (now<=0 || wc_cycle.failed || stock>1 || chamber>2 || !(stock==1 || (stock==0 && chamber==1))
        || f_GetCurrentGrip(hand) || wc_forestock_hand(gun) || ti_down(hand)
        || g_cmd_until[wc_input_bit(hand)]>now || (g_grip_latched&wc_input_mask(hand)) || !wc_forced_hover(hand,0)
        || rl_interactivity(gun)!=2 || rl_reloading(gun) || wc_recoiling(gun)
        || wc_ammo_held(gun,0.0f) || wc_blending(gun) || wc_flourishing(gun)
        || !wc_in_valid_grips(binding.object[5]) || !ti_free(binding.object[5],side)) return;
    void** vtable=*(void***)hand;
    if (!vtable || !vtable[0x740/sizeof(void*)]
        || !((int (*)(const void*))vtable[0x740/sizeof(void*)])(hand) || !wc_guide(&binding)) return;
    if (!wc_current(&binding) || f_GetCurrentGrip(hand) || ti_down(hand)
        || !wc_forced_hover(hand,0) || !wc_in_valid_grips(binding.object[5]) || !ti_free(binding.object[5],side)) return;
    wc_return.active=0;__atomic_store_n(&wc_return_pose.stamp,0,__ATOMIC_RELEASE);
    wc_cycle.binding=binding;wc_cycle.until=now+1800;wc_cycle.started=wc_cycle.last=now;
    wc_cycle.phase=stock==1 ? 2 : 1;
    f_SetGripForcedHover(hand,binding.object[5],1);wc_cycle.hover=1;
    if (ti_best(hand)!=binding.object[5] || f_GetCurrentGrip(hand) || ti_down(hand)
        || !ti_free(binding.object[5],side) || !wc_in_valid_grips(binding.object[5])
        || ptr_now_ms()<now || ptr_now_ms()>=wc_cycle.until || !wc_current(&binding)) {wc_finish(hand,1);return;}
    // Native hand/input ticks precede this adapter. Keep the normal controller
    // grip down until the native endpoints acknowledge the complete stroke.
    wc_publish_input(hand,now);
    if(!wc_current(&binding) || !touch_weapon_cycle_input_mask(now,0)) {wc_finish(hand,1);return;}
    unsigned char auto_push=*((unsigned char*)gun+0x113c);
    if(auto_push>1){wc_finish(hand,1);return;}
    // The authored grab shortcut skips the visible forward stroke; only this owned gesture supplies it.
    *((unsigned char*)gun+0x113c)=0;
    ti_press(hand);wc_cycle.pressed=1;
    if(reload_grip_registered(binding.object[4])) {
        void* current_gun=*(void* const*)((const char*)binding.object[4]+0x20);
        unsigned long long name=0;
        int live=current_gun==gun && wc_typed(current_gun,wc_shotgun_class(),0x11a0);
        if(live)memcpy(&name,(const char*)current_gun+0x18,8);
        if(live && name==binding.name[3])*((unsigned char*)current_gun+0x113c)=auto_push;
    }
    if (f_GetCurrentGrip(hand)!=binding.object[5] || wc_forestock_hand(gun)!=hand) {
        if (!f_GetCurrentGrip(hand) && ti_down(hand)) ti_release(hand);
        wc_finish(hand,1);return;
    }
    if (wc_logs<8) {++wc_logs;LOGF("CYCLE pump normal grip gun=%p grip=%p",gun,binding.object[5]);}
}

// Bio4 hands smooth their tracking target and the primary grip subsequently
// solves a new two-handed gun rotation. Those native stages can change the
// hand/gun relative position after our post-hand tick. Place the same timed
// stroke in the current gun frame immediately before the original shotgun
// PreBio4Tick reads it; that original tick alone changes stock/chamber/ammo.
static void wc_native_pre_pose(void* gun,long now) {
    const struct wc_binding* b=&wc_cycle.binding;
    int gt=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE);
    if(gt<=0 || gettid()!=gt || !wc_cycle.phase || gun!=b->object[3])return;
    void* hand=b->object[0];
    if(now<wc_cycle.last || now>=wc_cycle.until || !wc_current(b)
        || !(touch_weapon_cycle_input_mask(now,ptr_menu_now())&wc_input_mask(hand))
        || !wc_forced_hover(hand,b->object[5]) || f_GetCurrentGrip(hand)!=b->object[5]
        || wc_forestock_hand(gun)!=hand || !ti_down(hand)) {wc_finish(hand,1);return;}
    unsigned stock=*(const unsigned char*)((const char*)gun+0x1160);
    unsigned chamber=*(const unsigned char*)((const char*)gun+0x1161);
    if(stock>1 || chamber>2) {wc_finish(hand,1);return;}
    // Phase changes remain in the hand tick, after observing native feedback.
    // Do not run a finished back stroke again during that one-frame handoff.
    if(stock!=(unsigned)(wc_cycle.phase==2))return;
    long elapsed=now-wc_cycle.started;
    int trace=wc_native_pose_logs<8 && wc_local_forestock && elapsed>=g_pump_ms[wc_cycle.phase==2];
    float before=NAN,after=NAN;
    if(trace)wc_local_forestock(gun,&before,hand);
    if(!wc_stroke(b,(float)elapsed/(float)g_pump_ms[wc_cycle.phase==2],wc_cycle.phase==2)) {
        wc_finish(hand,1);return;
    }
    if(trace && wc_current(b)) {
        wc_local_forestock(gun,&after,hand);++wc_native_pose_logs;
        LOGF("CYCLE native endpoint phase=%d elapsed=%ld before=%.6f after=%.6f",wc_cycle.phase,elapsed,before,after);
    }
}
unsigned long g_tgt_wc_shotgun_pre;
#if defined(__aarch64__)
__attribute__((naked)) static void gate_wc_shotgun_pre(void* gun,float dt) {
    asm volatile(".inst 0xd10203ff\n" ".inst 0x6d0123e9\n" ".inst 0xf90013fb\n" ".inst 0xa90367fa\n"
        "adrp x16, :got:g_tgt_wc_shotgun_pre\n" "ldr x16, [x16, :got_lo12:g_tgt_wc_shotgun_pre]\n"
        "ldr x16, [x16]\n" "br x16\n");
}
#endif
static void hook_wc_shotgun_pre(void* gun,float dt) {
    if(wc_shotgun_class)wc_native_pre_pose(gun,ptr_now_ms());
    gate_wc_shotgun_pre(gun,dt);
}

static void install_touch_weapon_cycle(unsigned long base) {
    static const struct {unsigned long addr;unsigned first[2];} check[]={
        {0x643a7c8,{0xd10183ff,0xf90023f3}},
        {0x643a850,{0x52823404,0x52800205}},
        {0x8f82500,{0xd10183ff,0xf90023f3}},
        {0x8f82588,{0x52803e04,0x52800205}},
        {0x641baa0,{0xd10183ff,0xf90023f3}},
        {0x641bb28,{0x52805004,0x52800205}},
        {0x62a8e50,{0xf81e0ff3,0xa9017bfd}},
        {0x6292c38,{0xf81e0ff3,0xa9017bfd}},
        {0x628572c,{0x9107e000,0xd65f03c0}},
        {0x6280060,{0xd10143ff,0xf90013f5}},
        {0x628e55c,{0x39710008,0x34000068}},
        {0x6281040,{0xf9411809,0xb40002c9}},
        {0x6299ee8,{0xa9be4ff4,0xa9017bfd}},
        {0x62ca634,{0x52822c28,0x38686808}},
        {0x62ca660,{0x52822c28,0x38686808}},
        {0x62c7e04,{0xa9be4ff4,0xa9017bfd}},
        {0x629a7cc,{0xf81a0ffb,0xa90167fa}},
        {0x633c54c,{0x52800028,0x390cf408}},
        {0x633c558,{0x390cf41f,0x1405d0a2}},
        {0x633d048,{0x394cf400,0xd65f03c0}},
        {0x629a930,{0xd10403ff,0x6d082beb}},
        {0x83a926c,{0xd10303ff,0xa9085ff8}},
        {0x628db4c,{0xfc1e0fe8,0xf90007f3}},
        {0x62bb880,{0xf81e0ff3,0xa9017bfd}},
        {0x62bb584,{0xf81e0ff3,0xa9017bfd}},
        {0x62c8ff0,{0x9400022f,0x14000058}},
        {0x62c8fdc,{0x94000521,0xbd4007e0}},
        {0x62c9370,{0x9400001b,0x3940aee9}},
        {0x62c9354,{0x94000443,0xbd4007e0}},
        {0x62c9cc8,{0x39000288,0xf9400268}},
        {0x62c2938,{0x39000289,0x3c804280}},
        {0x62ca3f4,{0xfd004680,0xbd118661}},
        {0x62ca44c,{0x1e203820,0xbd118660}},
        {0x62ca4fc,{0x90020449,0xf943f929}},
        {0x629ab94,{0x9400ca77,0xb4fff8c0}},
        {0x6341aec,{0xf9416c09,0xad4e8122}},
        {0x62ca338,{0x52821e88,0x8b080274}},
        {0x62ca5c8,{0xfc41b2f0,0x6e0c04e5}},
        {0x6296b94,{0x94000cd5,0x1e2a3800}},
        {0x6296d8c,{0x2a1f03e2,0x3d801be0}},
        {0x62c8e44,{0xd10203ff,0x6d0123e9}},
        {0x62c8e4c,{0xf90013fb,0xa90367fa}},
        {0x62ca460,{0xd10203ff,0xf90023f7}},
        {0x62ca3fc,{0x39412288,0x34000068}},
        {0x643a798,{0x52822788,0x52800029}},
    };
    static const struct {unsigned long addr;unsigned offset;} fields[]={
        {0x9db6e10,0x1160},
        {0x9db6db0,0x1161},
        {0x9da59a0,0xd40},
        {0x9da59d0,0xd38},
        {0x9db0e60,0x270},
        {0x9db0b18,0x3c0},
        {0x9d9e298,0x300},
        {0x9db70c8,0x10f4},
        {0x9db7038,0x1118},
        {0x9db7098,0x1100},
        {0x9db7008,0x1124},
        {0x9da7038,0x460},
    };
    wc_shotgun_class=0;memset(&wc_cycle,0,sizeof wc_cycle);touch_weapon_cycle_input_reset();
    for (unsigned i=0;i<sizeof check/sizeof *check;i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,8)) {
            LOGF("CYCLE disabled: opcode mismatch +%lx",check[i].addr);return;
        }
    for (unsigned i=0;i<sizeof fields/sizeof *fields;i++)
        if (*(const unsigned*)(base+fields[i].addr+36)!=fields[i].offset) {
            LOGF("CYCLE disabled: field mismatch +%lx",fields[i].addr);return;
        }
    static const struct {unsigned long addr;long long value;const char* name;} enums[]={
        {0x9da2300,0,"EChamberState::Ready"},{0x9da2310,1,"EChamberState::Spent"},
        {0x9da2320,2,"EChamberState::Empty"},{0x9da4ef0,0,"EForestockPosition::Front"},
        {0x9da4f00,1,"EForestockPosition::Back"},
    };
    for (unsigned i=0;i<sizeof enums/sizeof *enums;i++) {
        const char* entry=(const char*)(base+enums[i].addr),*name=*(const char* const*)entry;
        if (!name || *(const long long*)(entry+8)!=enums[i].value || strcmp(name,enums[i].name)) {
            LOGF("CYCLE disabled: enum mismatch +%lx",enums[i].addr);return;
        }
    }
    if (!ti_ready || (uintptr_t)tw_hand_class!=base+0x6424b5c
        || (uintptr_t)tw_pawn_class!=base+0x642c718 || (uintptr_t)tw_grip_class!=base+0x64192f4
        || (uintptr_t)ti_best!=base+0x629a930 || (uintptr_t)ti_press!=base+0x633c54c
        || (uintptr_t)ti_release!=base+0x633c558 || (uintptr_t)ti_down!=base+0x633d048
        || (uintptr_t)ti_free!=base+0x627feb8 || (uintptr_t)ti_parent!=base+0x6280170
        || (uintptr_t)f_SetGripForcedHover!=base+0x629a7cc
        || (uintptr_t)f_GetCurrentGrip!=base+0x629a774 || (uintptr_t)rl_primary!=base+0x62bbc30
        || (uintptr_t)rl_world_transform!=base+0x83a926c || (uintptr_t)rl_left!=base+0x62a8db4
        || (uintptr_t)rl_interactivity!=base+0x628cf08 || (uintptr_t)rl_reloading!=base+0x628c410
        || (uintptr_t)rl_paused!=base+0x61d0934 || (uintptr_t)rl_player_valid!=base+0x61d401c) {
        LOGF("CYCLE disabled: native dependency mismatch (Gun interactivity=%p expected=%p)",
            (void*)rl_interactivity,(void*)(base+0x628cf08));return;
    }
    wc_right=(void* (*)(const void*))(base+0x62a8e50);
    wc_forestock_hand=(void* (*)(const void*))(base+0x6292c38);
    wc_settings=(const void* (*)(void))(base+0x62c7e04);
    wc_group_grips=(const void* (*)(const void*))(base+0x628572c);
    wc_in_valid_grips=(int (*)(const void*))(base+0x6280060);
    wc_recoiling=(int (*)(const void*))(base+0x628e55c);
    wc_ammo_held=(int (*)(const void*,float))(base+0x628db4c);
    wc_blending=(int (*)(const void*))(base+0x62bb880);
    wc_flourishing=(int (*)(const void*))(base+0x62bb584);
    wc_grip_target=(struct reload_tf (*)(const void*))(base+0x6281040);
    wc_grab_location=(struct wc_vec (*)(const void*))(base+0x6299ee8);
    wc_start=(struct wc_vec (*)(const void*))(base+0x62ca634);
    wc_end=(struct wc_vec (*)(const void*))(base+0x62ca660);
    wc_local_forestock=(struct wc_vec (*)(const void*,float*,const void*))(base+0x62ca460);
    wc_scene_class=(void* (*)(void))(base+0x8f82500);
    wc_group_class=(void* (*)(void))(base+0x641baa0);
    wc_shotgun_class=(void* (*)(void))(base+0x643a7c8);
    g_tgt_wc_shotgun_pre=base+0x62c8e44+16;
    if(patch_code((void*)(base+0x62c8e44),(void*)hook_wc_shotgun_pre)) {
        wc_shotgun_class=0;touch_weapon_cycle_input_reset();LOGF("CYCLE disabled: shotgun pre-tick hook failed");return;
    }
    LOGF("CYCLE native QuickSelect rifle auto-bolt retained; owned shotgun pump helper ready");
}
