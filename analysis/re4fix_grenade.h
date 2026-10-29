// Explicit touch Fire uses the original default-style grip/flick/release route.
#include <stdint.h>
#include <limits.h>
static void* (*gr_class)(void);

static int (*gr_auto_pin)(const void*),(*gr_body)(const void*),(*gr_ammo)(const void*),(*gr_interactivity)(const void*);
static int (*gr_tracked)(const void*,const void*,float,float);
static void* (*gr_pin_hand)(const void*);
static int gr_installed,gr_finger,gr_cancel_request;
static volatile int g_grenade_diag_request;
static unsigned long long gr_ui,gr_request;
static struct {void* object[6];unsigned long long name[6];int side,phase,pressed,ammo;unsigned generation;long start,last,until,grip_until;float hold,tracked,damping,rise,fall,on,off,speed;long move_ms,move_start;} gr_run;
// Every shared payload word is atomic; readers retry a bounded seqlock.
static struct {unsigned seq;long stamp,start,move,back,end;unsigned distance,side,generation,base[3];} gr_plan;
static unsigned gr_generation,gr_logs;
static long gr_busy_until;
static unsigned gr_owned_seq,gr_owned_mask;
static long gr_owned_deadline,gr_last_pose,gr_freeze_pose;
static int gr_freeze;
static int gr_cancel_frames;static long gr_cancel_start,gr_cancel_last;
static int gr_ownership_begin(unsigned* seq) {
    *seq=__atomic_load_n(&gr_owned_seq,__ATOMIC_ACQUIRE);
    return !(*seq&1)&&__atomic_compare_exchange_n(&gr_owned_seq,seq,*seq+1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
static void gr_ownership_end(unsigned seq) {__atomic_store_n(&gr_owned_seq,seq+2,__ATOMIC_RELEASE);}
static unsigned touch_grenade_owned_grip_mask(void) {return __atomic_load_n(&gr_owned_mask,__ATOMIC_ACQUIRE);}
static unsigned touch_grenade_input_reset(void) {
    __atomic_store_n(&gr_ui,0,__ATOMIC_RELEASE);__atomic_store_n(&gr_request,0,__ATOMIC_RELEASE);
    __atomic_store_n(&gr_freeze_pose,__atomic_load_n(&gr_last_pose,__ATOMIC_ACQUIRE),__ATOMIC_RELEASE);
    __atomic_store_n(&gr_freeze,1,__ATOMIC_RELEASE);__atomic_store_n(&gr_cancel_request,1,__ATOMIC_RELEASE);
    unsigned seq=__atomic_load_n(&gr_owned_seq,__ATOMIC_ACQUIRE);
    if((seq&1)||!__atomic_compare_exchange_n(&gr_owned_seq,&seq,seq+1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
        unsigned reserved=__atomic_load_n(&gr_owned_mask,__ATOMIC_ACQUIRE);long expected=__atomic_load_n(&gr_owned_deadline,__ATOMIC_ACQUIRE);
        int bit=reserved==0x08000000u?27:reserved==0x20000000u?29:-1;
        long current=bit>=0?__atomic_load_n(&g_cmd_until[bit],__ATOMIC_ACQUIRE):-1;
        return bit>=0&&expected&&(current==expected||current==0)?reserved:0;
    }
    unsigned mask=__atomic_load_n(&gr_owned_mask,__ATOMIC_ACQUIRE);long own=__atomic_load_n(&gr_owned_deadline,__ATOMIC_ACQUIRE);
    if(mask&&own) {
        int bit=mask==0x08000000u?27:mask==0x20000000u?29:-1;
        if(bit<0||!__atomic_compare_exchange_n(&g_cmd_until[bit],&own,LONG_MAX,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))mask=0;
        else __atomic_store_n(&gr_owned_deadline,LONG_MAX,__ATOMIC_RELEASE);
    } else mask=0;
    __atomic_store_n(&gr_owned_seq,seq+2,__ATOMIC_RELEASE);return mask;
}
static int gr_context(void) {
    return g_touch_context==1&&!g_touch_cancel&&!g_touch_weapon_request&&!g_touch_reload_request&&!g_reload_phase
        &&!g_touch_inventory_request&&!g_touch_pause&&!g_touch_interact_request&&!touch_quickselect_busy();
}
static void gr_held_pose(int side,float off[3]) {
    float aspect=g_comp_w && g_comp_h ? (float)g_comp_w/(float)g_comp_h:2400.0f/1080.0f;
    grenade_held_pose(side,g_fov_h_deg,aspect,off);
}
static void gr_publish_plan(long now,long start,long move,long back,long end,float distance,int side,unsigned generation) {
    unsigned bits,base_bits[3];float base[3]={0};gr_held_pose(side,base);memcpy(base_bits,base,sizeof base_bits);
    memcpy(&bits,&distance,4);__atomic_add_fetch(&gr_plan.seq,1,__ATOMIC_ACQ_REL);
    __atomic_store_n(&gr_plan.stamp,now,__ATOMIC_RELAXED);__atomic_store_n(&gr_plan.start,start,__ATOMIC_RELAXED);
    __atomic_store_n(&gr_plan.move,move,__ATOMIC_RELAXED);__atomic_store_n(&gr_plan.back,back,__ATOMIC_RELAXED);
    __atomic_store_n(&gr_plan.end,end,__ATOMIC_RELAXED);__atomic_store_n(&gr_plan.distance,bits,__ATOMIC_RELAXED);
    for(int i=0;i<3;i++)__atomic_store_n(&gr_plan.base[i],base_bits[i],__ATOMIC_RELAXED);
    __atomic_store_n(&gr_plan.side,(unsigned)side,__ATOMIC_RELAXED);__atomic_store_n(&gr_plan.generation,generation,__ATOMIC_RELAXED);
    __atomic_add_fetch(&gr_plan.seq,1,__ATOMIC_RELEASE);
}
static int touch_grenade_pose(int side,long now,float off[3]) {
    int freeze=__atomic_load_n(&gr_freeze,__ATOMIC_ACQUIRE);
    if(!off||side<0||side>1||now<=0||(!freeze&&!gr_context()))return 0;
    for(int attempt=0;attempt<2;attempt++) {
        unsigned seq=__atomic_load_n(&gr_plan.seq,__ATOMIC_ACQUIRE);if(seq&1)continue;
        long stamp=__atomic_load_n(&gr_plan.stamp,__ATOMIC_RELAXED),start=__atomic_load_n(&gr_plan.start,__ATOMIC_RELAXED);
        long move=__atomic_load_n(&gr_plan.move,__ATOMIC_RELAXED),back=__atomic_load_n(&gr_plan.back,__ATOMIC_RELAXED),end=__atomic_load_n(&gr_plan.end,__ATOMIC_RELAXED);
        unsigned bits=__atomic_load_n(&gr_plan.distance,__ATOMIC_RELAXED),actual_side=__atomic_load_n(&gr_plan.side,__ATOMIC_RELAXED),generation=__atomic_load_n(&gr_plan.generation,__ATOMIC_RELAXED),base_bits[3];
        for(int i=0;i<3;i++)base_bits[i]=__atomic_load_n(&gr_plan.base[i],__ATOMIC_RELAXED);
        if(seq!=__atomic_load_n(&gr_plan.seq,__ATOMIC_ACQUIRE))continue;
        float distance,base[3];memcpy(&distance,&bits,4);memcpy(base,base_bits,sizeof base);
        long pose_now=freeze?__atomic_load_n(&gr_freeze_pose,__ATOMIC_ACQUIRE):now;
        if(!generation||actual_side!=(unsigned)side||(!freeze&&(now<stamp||now-stamp>250))||pose_now<start||pose_now>=end
            ||move<80||move>350||back<start+move||end<=back||end-back>2200||!isfinite(distance)||distance<=0||distance>.75f)return 0;
        float travel=pose_now<start+move?(float)(pose_now-start)/move:pose_now<back?1:1-(float)(pose_now-back)/(end-back);
        for(int i=0;i<3;i++)if(!isfinite(base[i]) || fabsf(base[i])>1)return 0;
        memcpy(off,base,sizeof base);off[2]-=distance*fminf(1,fmaxf(0,travel));
        if(!freeze)__atomic_store_n(&gr_last_pose,now,__ATOMIC_RELEASE);return 1;
    }
    return 0;
}
static int touch_grenade_busy(long now) {
    if(__atomic_load_n(&gr_freeze,__ATOMIC_ACQUIRE)&&touch_grenade_owned_grip_mask())return now>0;
    long until=__atomic_load_n(&gr_busy_until,__ATOMIC_ACQUIRE);return now>0&&now<until&&until-now<=5000;
}
static int touch_grenade_fire_side(long now) {
    unsigned long long ui=__atomic_load_n(&gr_ui,__ATOMIC_ACQUIRE);
    return ui&&now>0&&gr_context()&&(unsigned)((unsigned)now-(unsigned)(ui>>32))<=250?((unsigned)ui>>16)&1:-1;
}
static int touch_grenade_fire_down(long now) {
    unsigned long long ui=__atomic_load_n(&gr_ui,__ATOMIC_ACQUIRE);
    if(!ui||now<=0||!gr_context()||touch_grenade_busy(now)||(unsigned)((unsigned)now-(unsigned)(ui>>32))>250)return 0;
    int expected=0;if(!__atomic_compare_exchange_n(&gr_finger,&expected,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    if(ui!=__atomic_load_n(&gr_ui,__ATOMIC_ACQUIRE)||!gr_context()){__atomic_store_n(&gr_finger,0,__ATOMIC_RELEASE);return 0;}
    unsigned long long request=((unsigned long long)(unsigned)now<<32)|(unsigned)ui;
    __atomic_store_n(&gr_request,request,__ATOMIC_RELEASE);return 1;
}
static void touch_grenade_fire_up(long now) {(void)now;__atomic_store_n(&gr_finger,0,__ATOMIC_RELEASE);}
static int gr_binding(void* pawn,void** objects) {
    if(!gr_installed||!qs_ready||!qs_gt()||!gr_class||!tw_typed(pawn,tw_pawn_class(),0x10d0)||pawn!=phone_active()
        ||!qs_binding(pawn,2,objects)||!tw_typed(objects[1],gr_class(),0x640)||!qs_current(objects))return 0;
    void* const* table=*(void* const* const*)objects[1];
    if(!table||table[0x668/sizeof(void*)]!=(void*)gr_interactivity||table[0x680/sizeof(void*)]!=(void*)gr_body)return 0;
    objects[5]=*(void* const*)((const char*)objects[2]+0x130);
    if(!reload_layout(objects[5],0x1f0)||*(void* const*)((const char*)objects[5]+0x20)!=objects[2])return 0;
    return 1;
}
static int gr_same(void** objects) {
    for(int i=0;i<6;i++)if(objects[i]!=gr_run.object[i]||qs_name(objects[i])!=gr_run.name[i])return 0;return 1;
}
static int gr_live(void** objects) {
    void* pawn=phone_active();return gr_binding(pawn,objects)&&gr_same(objects);
}
static int gr_owned(void** objects) {
    void* grip=gr_run.object[3];if(!reload_grip_registered(grip)||!tw_typed(grip,tw_grip_class(),0x460)||qs_name(grip)!=gr_run.name[3])return 0;
    void* grenade=*(void* const*)((const char*)grip+0x20);
    if(!tw_typed(grenade,gr_class(),0x640)||grenade!=gr_run.object[1]||qs_name(grenade)!=gr_run.name[1]
        ||*(void* const*)((const char*)grip+0x300)!=grenade)return 0;
    void* pawn=rl_get_pawn(grenade);if(!tw_typed(pawn,tw_pawn_class(),0x10d0)||pawn!=phone_active()||pawn!=gr_run.object[0]||qs_name(pawn)!=gr_run.name[0])return 0;
    void* hand=gr_run.side?rl_right(pawn):rl_left(pawn),*cls=*(void* const*)((const char*)grenade+0x10);
    if(!tw_typed(hand,tw_hand_class(),0x510)||hand!=gr_run.object[2]||qs_name(hand)!=gr_run.name[2]||phone_player(hand)!=pawn
        ||cls!=gr_run.object[4]||qs_name(cls)!=gr_run.name[4])return 0;
    void* root=*(void* const*)((const char*)hand+0x130);
    if(root!=gr_run.object[5]||!reload_layout(root,0x1f0)||qs_name(root)!=gr_run.name[5]||*(void* const*)((const char*)root+0x20)!=hand)return 0;
    objects[0]=pawn;objects[1]=grenade;objects[2]=hand;objects[3]=grip;objects[4]=cls;objects[5]=root;return 1;
}
static int gr_eligible(void* grenade) {
    const unsigned char* settings=qs_settings();unsigned type=*(const unsigned short*)((const char*)grenade+0x5ea);
    return settings&&settings[0x3d]==1&&settings[0x40]<=1&&type<=14&&(0x4006u&(1u<<type))
        &&gr_interactivity(grenade)==2&&!gr_body(grenade)&&!gr_pin_hand(grenade)
        &&*(const unsigned char*)((const char*)grenade+0x5ee)==2&&gr_ammo(grenade)>0;
}
static int gr_authored(void* grenade) {
    const unsigned char* settings=qs_settings();return settings&&settings[0x40]==0&&gr_eligible(grenade)&&gr_auto_pin(grenade);
}
struct gr_style_txn {unsigned char bytes[0xb8] __attribute__((aligned(16)));int changed,prior;};
static int gr_style_begin(struct gr_style_txn* txn) {
    memset(txn,0,sizeof *txn);const unsigned char* source=qs_settings();if(!source||source[0x40]>1)return 0;
    txn->prior=source[0x40];if(!txn->prior)return 1;
    if(!qs_copy||!qs_set||!qs_destroy)return 0;
    qs_copy(txn->bytes,source);txn->changed=1;txn->bytes[0x40]=0;qs_set(txn->bytes);return 1;
}
static void gr_style_finish(struct gr_style_txn* txn,int accepted) {
    if(txn->changed){if(!accepted){txn->bytes[0x40]=(unsigned char)txn->prior;qs_set(txn->bytes);}qs_destroy(txn->bytes);txn->changed=0;}
}
static int gr_parameters(void* grenade) {
    const int offsets[]={0x548,0x54c,0x550,0x580,0x584,0x588,0x58c,0x590};float values[8];
    for(int i=0;i<8;i++){values[i]=*(const float*)((const char*)grenade+offsets[i]);if(!isfinite(values[i]))return 0;}
    if(values[0]<0||values[0]>.8f||values[1]<0||values[1]>.5f||values[2]<0||values[2]>.15f
        ||values[3]<0||values[3]>.25f||values[4]<0||values[4]>.25f||values[5]<=0||values[5]>=.99f
        ||values[6]<0||values[6]>=values[5]||values[7]<20||values[7]>350)return 0;
    long move=(long)ceilf(1000*(values[2]+values[3]*logf(1-values[5])/logf(.01f)))+80;if(move<120)move=120;
    float distance=values[7]*1.6f*.01f*(float)move*.001f;
    if(move>350||distance>.75f)return 0;
    gr_run.hold=values[0];gr_run.tracked=values[1];gr_run.damping=values[2];gr_run.rise=values[3];gr_run.fall=values[4];
    gr_run.on=values[5];gr_run.off=values[6];gr_run.speed=values[7];gr_run.move_ms=move;return 1;
}
static int gr_parameters_same(void* grenade) {
    const int offsets[]={0x548,0x54c,0x550,0x580,0x584,0x588,0x58c,0x590};
    const float values[]={gr_run.hold,gr_run.tracked,gr_run.damping,gr_run.rise,gr_run.fall,gr_run.on,gr_run.off,gr_run.speed};
    for(int i=0;i<8;i++)if(memcmp((const char*)grenade+offsets[i],&values[i],4))return 0;return 1;
}
static void gr_retire(long now,int normal_up) {
    unsigned seq;if(!gr_ownership_begin(&seq)){__atomic_store_n(&gr_cancel_request,1,__ATOMIC_RELEASE);return;}
    int cleared=0;int bit=gr_run.side?27:29;
    if(gr_run.grip_until){long own=__atomic_load_n(&gr_owned_deadline,__ATOMIC_ACQUIRE);int bit=gr_run.side?27:29;
        cleared=__atomic_compare_exchange_n(&g_cmd_until[bit],&own,0,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
    }
    __atomic_store_n(&gr_owned_mask,0,__ATOMIC_RELEASE);__atomic_store_n(&gr_owned_deadline,0,__ATOMIC_RELEASE);
    gr_ownership_end(seq);
    void* b[6];if(normal_up&&cleared&&gr_owned(b)&&f_GetCurrentGrip(b[2])==b[3]
        &&!(g_grip_latched&(1u<<bit))&&g_cmd_until[bit]<=now&&ti_down(b[2]))ti_release(b[2]);
    __atomic_store_n(&gr_freeze,0,__ATOMIC_RELEASE);
    memset(&gr_run,0,sizeof gr_run);__atomic_store_n(&gr_busy_until,0,__ATOMIC_RELEASE);gr_publish_plan(now,0,0,0,0,0,0,0);
}
static void touch_grenade_cancel(void) {
    __atomic_store_n(&gr_ui,0,__ATOMIC_RELEASE);__atomic_store_n(&gr_request,0,__ATOMIC_RELEASE);
    __atomic_store_n(&gr_cancel_request,1,__ATOMIC_RELEASE);
    (void)touch_grenade_input_reset();
}
static void touch_grenade_cancel_tick(void* hand,int side,long now) {
    if(!qs_gt()||side<0||side>1||now<=0||(__atomic_load_n(&gr_owned_seq,__ATOMIC_ACQUIRE)&1))return;
    if(!__atomic_load_n(&gr_cancel_request,__ATOMIC_ACQUIRE)&&gr_run.phase!=6)return;
    if(!gr_run.phase){__atomic_store_n(&gr_cancel_request,0,__ATOMIC_RELEASE);__atomic_store_n(&gr_freeze,0,__ATOMIC_RELEASE);return;}
    void* b[6];if(!gr_owned(b)){gr_retire(now,0);__atomic_store_n(&gr_cancel_request,0,__ATOMIC_RELEASE);return;}
    if(hand!=b[2]||*(const unsigned char*)((const char*)hand+0x314)!=(unsigned)side)return;
    if(gr_run.phase!=6){gr_run.phase=6;gr_cancel_frames=0;gr_cancel_start=0;gr_cancel_last=0;}
    __atomic_store_n(&gr_cancel_request,0,__ATOMIC_RELEASE);__atomic_store_n(&gr_plan.stamp,now,__ATOMIC_RELEASE);
    if(rl_paused()||phone_type()!=1)return;
    if(!gr_cancel_start)gr_cancel_start=now;
    if(now<gr_cancel_start){gr_retire(now,0);return;}
    if(now!=gr_cancel_last){gr_cancel_last=now;gr_cancel_frames++;}
    if(gr_cancel_frames>=2&&(!*(const unsigned char*)((const char*)b[1]+0x5ef)||gr_body(b[1])||gr_interactivity(b[1])!=2)){gr_retire(now,1);return;}
    // A still-live original flick cannot be cancelled by UP: park the owned hold.
    // Busy remains set until a safe native signal/body transition or owner recovery.
}
static int gr_parked_owned(int bit) {
    return __atomic_load_n(&gr_owned_mask,__ATOMIC_ACQUIRE)==(1u<<bit)
        &&__atomic_load_n(&gr_owned_deadline,__ATOMIC_ACQUIRE)==LONG_MAX
        &&__atomic_load_n(&g_cmd_until[bit],__ATOMIC_ACQUIRE)==LONG_MAX
        &&(__atomic_load_n(&gr_cancel_request,__ATOMIC_ACQUIRE)||__atomic_load_n(&gr_freeze,__ATOMIC_ACQUIRE));
}
static void gr_diag_tf(const char* label,const void* component) {
    if(!reload_layout(component,0x1f0)){LOGF("GRENADEPOSE %s unavailable=%p",label,component);return;}
    struct reload_tf tf;memcpy(&tf,(const char*)component+0x1c0,sizeof tf);
    LOGF("GRENADEPOSE %s component=%p p=%.4f,%.4f,%.4f q=%.5f,%.5f,%.5f,%.5f s=%.6f,%.6f,%.6f visible=%u",
        label,component,tf.p[0],tf.p[1],tf.p[2],tf.q.x,tf.q.y,tf.q.z,tf.q.w,tf.s[0],tf.s[1],tf.s[2],
        !!(*(const unsigned short*)((const char*)component+0x14c)&0x20));
}
static void gr_diag_tick(void* hand) {
    if(!__atomic_load_n(&g_grenade_diag_request,__ATOMIC_ACQUIRE))return;
    void* pawn=qs_player(hand),*b[5];
    if(!pawn || !__atomic_exchange_n(&g_grenade_diag_request,0,__ATOMIC_ACQ_REL))return;
    if(!qs_binding(pawn,2,b) || !tw_typed(b[1],gr_class(),0x640)){LOGF("GRENADEPOSE no assigned slot2 pawn=%p",pawn);return;}
    void* grenade=b[1],*model=*(void* const*)((const char*)grenade+0x628);
    void* mesh=*(void* const*)((const char*)grenade+0x4e0);
    LOGF("GRENADEPOSE binding pawn=%p item=%p model=%p hand=%p actualPrimary=%p current=%d state=%u flourish=%u ammo=%d modelHidden=%d",
        pawn,grenade,model,b[2],rl_primary(grenade),qs_current(b),*(const unsigned char*)((const char*)grenade+0x5ee),
        *(const unsigned char*)((const char*)grenade+0x458),gr_ammo(grenade),reload_layout(model,0x220) ? !!(*(const unsigned*)((const char*)model+0x58)&0x20):-1);
    gr_diag_tf("head",*(void* const*)((const char*)pawn+0x2d8));
    gr_diag_tf("hand",*(void* const*)((const char*)b[2]+0x130));
    gr_diag_tf("placeholder",mesh);
    if(reload_layout(model,0x220))gr_diag_tf("model",*(void* const*)((const char*)model+0x130));
}
static void touch_grenade_tick(void* hand,int side,long now) {
    if(!gr_installed||!qs_gt()||side<0||side>1||now<=0||!tw_typed(hand,tw_hand_class(),0x510)
        ||*(const unsigned char*)((const char*)hand+0x314)!=(unsigned)side)return;
    gr_diag_tick(hand);
    if(__atomic_load_n(&gr_owned_seq,__ATOMIC_ACQUIRE)&1)return;
    if(gr_run.phase==6||__atomic_load_n(&gr_cancel_request,__ATOMIC_ACQUIRE)||!gr_context()||rl_paused()||!rl_player_valid()||phone_type()!=1) {
        if(gr_run.phase!=6&&!__atomic_load_n(&gr_cancel_request,__ATOMIC_ACQUIRE))touch_grenade_cancel();
        touch_grenade_cancel_tick(hand,side,now);return;
    }
    void* b[6];int bound=gr_binding(phone_active(),b);
    if(gr_run.phase) {
        if(gr_run.phase>=3) {
            if(!gr_owned(b)||now<gr_run.last||now>=gr_run.until){gr_retire(now,0);return;}
            if(hand!=b[2])return;gr_run.last=now;
            if(gr_run.phase==3) {
                if(ti_down(hand)||gr_ammo(b[1])!=gr_run.ammo-1)return;
                gr_run.phase=4;if(gr_logs<12){gr_logs++;LOGF("GRENADE native throw acknowledged");}
            }
            long back=gr_run.move_start+gr_run.move_ms+100,end=back+2000;
            if(now>=end){gr_retire(now,0);return;}
            float distance=gr_run.speed*1.6f*.01f*(float)gr_run.move_ms*.001f;
            gr_publish_plan(now,gr_run.move_start,gr_run.move_ms,back,end,distance,side,gr_run.generation);return;
        }
        if(!bound||!gr_same(b)){gr_retire(now,0);return;}
        if(now<gr_run.last||now>=gr_run.until){touch_grenade_cancel();touch_grenade_cancel_tick(hand,side,now);return;}
        if(hand!=b[2])return;gr_run.last=now;int bit=side?27:29;
        if(g_cmd_until[bit]!=gr_run.grip_until||(g_grip_latched&(1u<<bit))){
            if(gr_parked_owned(bit))touch_grenade_cancel_tick(hand,side,now);else gr_retire(now,0);return;
        }
        if(now>=gr_run.grip_until){touch_grenade_cancel();touch_grenade_cancel_tick(hand,side,now);return;}
        if(!gr_authored(b[1])||!gr_parameters_same(b[1])) {touch_grenade_cancel();touch_grenade_cancel_tick(hand,side,now);return;}
        if(gr_run.phase==1) {
            if(!ti_down(hand))return;
            if(*(const float*)((const char*)b[1]+0x600)<=gr_run.hold||!*(const unsigned char*)((const char*)b[1]+0x5f1)
                ||!gr_tracked(b[1],hand,0,gr_run.tracked))return;
            gr_run.phase=2;gr_run.move_start=now;
        }
        if(gr_run.phase==2) {
            if(!gr_context()||!gr_live(b)){touch_grenade_cancel();touch_grenade_cancel_tick(hand,side,now);return;}
            float distance=gr_run.speed*1.6f*.01f*(float)gr_run.move_ms*.001f;
            long back=gr_run.move_start+gr_run.move_ms+100,end=back+2000;
            gr_publish_plan(now,gr_run.move_start,gr_run.move_ms,back,end,distance,side,gr_run.generation);
            if(now<gr_run.move_start+gr_run.move_ms)return;
            if(!*(const unsigned char*)((const char*)b[1]+0x5ef)){touch_grenade_cancel();touch_grenade_cancel_tick(hand,side,now);return;}
            if(!gr_tracked(b[1],hand,0,gr_run.tracked)||!gr_live(b)||!gr_context()){touch_grenade_cancel();touch_grenade_cancel_tick(hand,side,now);return;}
            unsigned seq;if(!gr_ownership_begin(&seq))return;
            long own=gr_run.grip_until;if(!__atomic_compare_exchange_n(&g_cmd_until[bit],&own,0,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
                gr_ownership_end(seq);if(gr_parked_owned(bit))touch_grenade_cancel_tick(hand,side,now);else gr_retire(now,0);return;
            }
            if(__atomic_load_n(&gr_cancel_request,__ATOMIC_ACQUIRE)||!gr_context()) {
                long zero=0;if(__atomic_compare_exchange_n(&g_cmd_until[bit],&zero,LONG_MAX,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
                    gr_run.grip_until=LONG_MAX;__atomic_store_n(&gr_owned_deadline,LONG_MAX,__ATOMIC_RELEASE);
                }
                gr_ownership_end(seq);return;
            }
            gr_run.grip_until=0;__atomic_store_n(&gr_owned_mask,0,__ATOMIC_RELEASE);__atomic_store_n(&gr_owned_deadline,0,__ATOMIC_RELEASE);
            gr_ownership_end(seq);
            gr_run.phase=3;if(ti_down(hand))ti_release(hand);return;
        }
        return;
    }
    if(!bound||!gr_eligible(b[1])||!gr_parameters(b[1])||!gr_context()){__atomic_store_n(&gr_ui,0,__ATOMIC_RELEASE);return;}
    unsigned actual_side=*(const unsigned char*)((const char*)b[2]+0x314);
    if(hand!=b[2])return;
    static void* observed[6];static unsigned long long names[6];int same=1;
    for(int i=0;i<6;i++)if(observed[i]!=b[i]||names[i]!=qs_name(b[i]))same=0;
    if(!same){gr_generation=(gr_generation+1)&0xffff;if(!gr_generation)gr_generation=1;for(int i=0;i<6;i++){observed[i]=b[i];names[i]=qs_name(b[i]);}}
    unsigned long long request=__atomic_exchange_n(&gr_request,0,__ATOMIC_ACQ_REL);
    if(request) {
        unsigned generation=(unsigned)request&0xffff;int bit=actual_side?27:29;long prior=__atomic_load_n(&g_cmd_until[bit],__ATOMIC_ACQUIRE);
        if(!generation||generation!=gr_generation||(((unsigned)request>>16)&1)!=actual_side
            ||(unsigned)((unsigned)now-(unsigned)(request>>32))>250||ti_down(hand)
            ||prior>now||(g_grip_latched&(1u<<bit))||!gr_parameters(b[1]))return;
        for(int i=0;i<6;i++){gr_run.object[i]=b[i];gr_run.name[i]=qs_name(b[i]);}
        gr_run.side=actual_side;gr_run.ammo=gr_ammo(b[1]);gr_run.generation=generation;gr_run.phase=1;gr_run.start=gr_run.last=now;gr_run.until=now+5000;
        struct gr_style_txn style;
        long until=now+1800;if(!gr_style_begin(&style)||!gr_live(b)||!gr_authored(b[1])||!gr_context()){gr_style_finish(&style,0);memset(&gr_run,0,sizeof gr_run);return;}
        unsigned seq;if(!gr_ownership_begin(&seq)){gr_style_finish(&style,0);memset(&gr_run,0,sizeof gr_run);return;}
        if(__atomic_load_n(&gr_cancel_request,__ATOMIC_ACQUIRE)){gr_ownership_end(seq);gr_style_finish(&style,0);memset(&gr_run,0,sizeof gr_run);return;}
        __atomic_store_n(&gr_owned_deadline,until,__ATOMIC_RELEASE);__atomic_store_n(&gr_owned_mask,1u<<bit,__ATOMIC_RELEASE);
        if(!__atomic_compare_exchange_n(&g_cmd_until[bit],&prior,until,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
            __atomic_store_n(&gr_owned_mask,0,__ATOMIC_RELEASE);__atomic_store_n(&gr_owned_deadline,0,__ATOMIC_RELEASE);gr_ownership_end(seq);gr_style_finish(&style,0);memset(&gr_run,0,sizeof gr_run);return;
        }
        if(__atomic_load_n(&gr_cancel_request,__ATOMIC_ACQUIRE)||!gr_context()) {
            long own=until;if(__atomic_compare_exchange_n(&g_cmd_until[bit],&own,LONG_MAX,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
                until=LONG_MAX;__atomic_store_n(&gr_owned_deadline,until,__ATOMIC_RELEASE);
            }
        }
        gr_ownership_end(seq);
        gr_style_finish(&style,1);
        gr_run.grip_until=until;__atomic_store_n(&gr_ui,0,__ATOMIC_RELEASE);__atomic_store_n(&gr_busy_until,gr_run.until,__ATOMIC_RELEASE);
        if(gr_logs<12){gr_logs++;LOGF("GRENADE authored gesture side=%u",actual_side);}return;
    }
    unsigned low=gr_generation|(actual_side<<16);__atomic_store_n(&gr_ui,((unsigned long long)(unsigned)now<<32)|low,__ATOMIC_RELEASE);
}

static void install_touch_grenade(unsigned long base) {
    static const struct {unsigned long addr;unsigned words[2];} check[]={
        {0x6414d98,{0xd10183ff,0xf90023f3}},
        {0x6414e20,{0x5280c804,0x52800205}},
        {0x627c69c,{0xa9bf7bfd,0x910003fd}},
        {0x627ea7c,{0xa9be4ff4,0xa9017bfd}},
        {0x627d4ac,{0xa9bf7bfd,0x910003fd}},
        {0x627cae8,{0xf81e0ff3,0xa9017bfd}},
        {0x627d97c,{0x6dbe23e9,0xa9017bfd}},
        {0x627c6dc,{0xf9427400,0x140017b4}},
        {0x627b44c,{0xd10303ff,0xfd002bec}},
        {0x627b538,{0x940306c4,0x1e2703ea}},
        {0x627b544,{0xbd460260,0x1e28280a}},
        {0x627b54c,{0xbd454a6b,0xaa1403e0}},
        {0x627b55c,{0x1e2b2140,0x1a9fd7f9}},
        {0x627b564,{0x9402fe51,0x2d0707e0}},
        {0x627b5ac,{0x4ea81d00,0xaa1603e1}},
        {0x627b918,{0x3957c668,0x34000348}},
        {0x627b920,{0x3957be68,0x340001e8}},
        {0x627b928,{0x3957ba68,0x7100091f}},
        {0x627b954,{0x9401312c,0x39410008}},
        {0x627b96c,{0xbd454660,0xbd45fe61}},
        {0x627b980,{0x94000198,0x3917c679}},
        {0x627c3c8,{0x940000c7,0xf9431a60}},
        {0x61ce740,{0xf81e0ff3,0xa9017bfd}},
        {0x61ce778,{0x2d420001,0x1e212040}},
        {0x61ce7bc,{0xbd400a61,0x1e212000}},
        {0x633d048,{0x394cf400,0xd65f03c0}},
        {0x633c558,{0x390cf41f,0x1405d0a2}},
        {0x627c090,{0xbd454e69,0xaa1403e0}},
    };
    static const struct {unsigned long addr;unsigned offset;} fields[]={
        {0x9d9ae38,0x548},
        {0x9d9ae10,0x54c},
        {0x9d9ade8,0x550},
        {0x9d9acf8,0x580},
        {0x9d50ee0,0x0},
        {0x9d50eb8,0x4},
        {0x9d50e90,0x8},
        {0x9d50e68,0xc},
        {0x9d50e40,0x10},
        {0x9d9af80,0x520},
        {0x9d9af58,0x524},
        {0x9db5890,0x40},
    };
    static const struct {unsigned long addr;long long value;const char* name;} enums[]={
        {0x9d5e808,0,"EGrenadeStyle::Default"},
        {0x9d5e818,1,"EGrenadeStyle::Manual"},
        {0x9d5e828,2,"EGrenadeStyle::Count"},
        {0x9d998e0,0,"EGrenadeState::NoAmmo"},
        {0x9d998f0,1,"EGrenadeState::Assigned"},
        {0x9d99900,2,"EGrenadeState::Ready"},
        {0x9d99910,3,"EGrenadeState::Cooking"},
    };
    gr_installed=0;gr_class=0;__atomic_store_n(&gr_ui,0,__ATOMIC_RELEASE);__atomic_store_n(&gr_request,0,__ATOMIC_RELEASE);
    if(!base||!qs_ready||!ti_ready)return;
    for(unsigned i=0;i<sizeof check/sizeof *check;i++)if(memcmp((const void*)(base+check[i].addr),check[i].words,8))return;
    for(unsigned i=0;i<sizeof fields/sizeof *fields;i++)if(*(const unsigned*)(base+fields[i].addr+36)!=fields[i].offset)return;
    for(unsigned i=0;i<sizeof enums/sizeof *enums;i++){const char* name=*(const char* const*)(base+enums[i].addr);
        if(!name||strcmp(name,enums[i].name)||*(const long long*)(base+enums[i].addr+8)!=enums[i].value)return;}
    if((uintptr_t)tw_hand_class!=base+0x6424b5c
        ||(uintptr_t)tw_pawn_class!=base+0x642c718
        ||(uintptr_t)tw_grip_class!=base+0x64192f4
        ||(uintptr_t)phone_player!=base+0x633d030
        ||(uintptr_t)phone_active!=base+0x63380dc
        ||(uintptr_t)phone_type!=base+0x6338aec
        ||(uintptr_t)rl_primary!=base+0x62bbc30
        ||(uintptr_t)rl_get_pawn!=base+0x62bbd24
        ||(uintptr_t)rl_left!=base+0x62a8db4
        ||(uintptr_t)rl_right!=base+0x62a8e50
        ||(uintptr_t)rl_paused!=base+0x61d0934
        ||(uintptr_t)rl_player_valid!=base+0x61d401c
        ||(uintptr_t)f_GetCurrentGrip!=base+0x629a774
        ||(uintptr_t)ti_parent!=base+0x6280170
        ||(uintptr_t)ti_down!=base+0x633d048
        ||(uintptr_t)ti_release!=base+0x633c558
        ||(uintptr_t)qs_prop!=base+0x62a7ad4
        ||(uintptr_t)qs_hand!=base+0x62a7b34
        ||(uintptr_t)qs_settings!=base+0x62c7e04
        ||(uintptr_t)qs_context!=base+0x62bc710
        ||(uintptr_t)qs_copy!=base+0x5fe1978||(uintptr_t)qs_set!=base+0x62c80d4||(uintptr_t)qs_destroy!=base+0x62c7fd0)return;
    gr_class=(void* (*)(void))(base+0x6414d98);
    gr_auto_pin=(int (*)(const void*))(base+0x627c69c);gr_body=(int (*)(const void*))(base+0x627ea7c);
    gr_ammo=(int (*)(const void*))(base+0x627d4ac);gr_interactivity=(int (*)(const void*))(base+0x627cae8);
    gr_tracked=(int (*)(const void*,const void*,float,float))(base+0x627d97c);
    gr_pin_hand=(void* (*)(const void*))(base+0x627c6dc);gr_installed=1;
    LOGF("GRENADE original default grip/flick/release adaptation ready");
}
