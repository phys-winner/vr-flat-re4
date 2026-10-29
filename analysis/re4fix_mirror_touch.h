// Include after quickselect and prompt_visual. Pair the mirror/dog trap's authored grips;
// ordinary look and movement supply the original native/Blueprint hand-position input.
#ifndef RE4FIX_MIRROR_TOUCH_H
#define RE4FIX_MIRROR_TOUCH_H
#include <limits.h>
static void* (*tm_mirror_class)(void);
static void* (*tm_trap_class)(void);
static int (*tm_dog_trap)(const void*);
static void (*tm_holster)(void*,int);
static struct ti_vec (*tm_grab_location)(const void*);
static float (*tm_release_distance)(const void*,const struct ti_vec*);
static struct reload_tf (*tm_joint_transform)(const void*,int);
static struct {
    void* object[6]; // pawn, grabbable, left/right hand, left/right grip
    struct cinema_ui_weak weak[6];
    long prior_input[2];
    unsigned owned_input,pressed;
    unsigned generation,seen;
    int phase,kind;
    long start;
    struct reload_tf head;
    float yaw,pitch,origin[2][3],correction[2][3];
    qf_t joint_q[2];
} tm_hold;
// GameThread publishes values only. The OVR callback also runs on RenderThread:
// it must never follow a cached UObject or call native/reflection functions.
struct tm_pose_packet { long stamp;unsigned generation;float position[2][3]; };
static pthread_mutex_t tm_pose_mutex=PTHREAD_MUTEX_INITIALIZER;
static struct tm_pose_packet tm_pose_plan,tm_pose_frame;
static unsigned long long tm_pose_frame_number;
static unsigned tm_pose_served,tm_pose_serial,tm_pose_ack[2];
static unsigned tm_failure_logs;

static int touch_mirror_pose(int side,long now,float position[3]) {
    if(side<0 || side>1 || !position || now<=0 || __atomic_load_n(&g_touch_context,__ATOMIC_ACQUIRE)!=1
        || __atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE) || __atomic_load_n(&g_touch_weapon_request,__ATOMIC_ACQUIRE))return 0;
    unsigned long long frame=g_pose_frame_counter ? __atomic_load_n(g_pose_frame_counter,__ATOMIC_ACQUIRE):0;
    pthread_mutex_lock(&tm_pose_mutex);
    if(!tm_pose_plan.generation) {pthread_mutex_unlock(&tm_pose_mutex);return 0;}
    if(tm_pose_frame.generation!=tm_pose_plan.generation
        || (g_pose_frame_counter ? frame!=tm_pose_frame_number : (tm_pose_served&(1u<<side)))) {
        tm_pose_frame=tm_pose_plan;tm_pose_frame_number=frame;tm_pose_served=0;
    }
    struct tm_pose_packet packet=tm_pose_frame;tm_pose_served|=1u<<side;
    pthread_mutex_unlock(&tm_pose_mutex);
    if(now<packet.stamp || now-packet.stamp>250)return 0;
    for(int i=0;i<3;i++)if(!isfinite(packet.position[side][i]) || fabsf(packet.position[side][i])>100)return 0;
    memcpy(position,packet.position[side],3*sizeof(float));
    __atomic_store_n(&tm_pose_ack[side],packet.generation,__ATOMIC_RELEASE);return 1;
}
static int tm_component_tf(const void* component,struct reload_tf* tf) {
    if(!pv_alive(component) || !interact_layout(component,0x1f0) || !interact_is_a(component,ti_scene_class()))return 0;
    memcpy(tf,(const char*)component+0x1c0,sizeof *tf);return reload_marker_valid(tf);
}
static int tm_head_tf(struct reload_tf* tf) {
    void* pawn=tm_hold.object[0];
    void* head=pawn ? *(void* const*)((const char*)pawn+0x2d8):0;
    if(!tm_component_tf(head,tf) || *(void* const*)((const char*)head+0x20)!=pawn)return 0;
    tf->q=q_mul(tf->q,pose_camera_delta_ue());return reload_tf_valid(tf);
}
static int tm_hand_tf(int side,struct reload_tf* tf) {
    void* hand=tm_hold.object[2+side];
    void* root=hand ? *(void* const*)((const char*)hand+0x130):0;
    return tm_component_tf(root,tf) && reload_tf_valid(tf) && *(void* const*)((const char*)root+0x20)==hand;
}
static void tm_raw_position(struct reload_tf world,struct reload_tf head,qf_t raw_q,const float raw_p[3],float out[3]) {
    struct reload_tf raw_head={{-raw_q.z,raw_q.x,raw_q.y,-raw_q.w},
        {-raw_p[2]*100,raw_p[0]*100,raw_p[1]*100},{1,1,1},{0,0}};
    // Reuse the same rigid-transform composition as the native magazine pose.
    struct reload_tf raw=reload_hand_pose(world,head,raw_head);
    out[0]=raw.p[1]*.01f;out[1]=raw.p[2]*.01f;out[2]=-raw.p[0]*.01f;
}
static float tm_look_delta(float now,float before) {return atan2f(sinf(now-before),cosf(now-before));}
static int tm_publish(long now) {
    struct reload_tf head,root;
    if(!tm_head_tf(&head))return 0;
    float yaw,pitch,raw_p[3];qf_t raw_q;pose_angles(2,&yaw,&pitch);head_pose_node(2,&raw_q,raw_p);
    float bias[3]={0},pull=0;
    if(tm_hold.phase==2 && tm_hold.kind==1) {
        void* component=*(void* const*)((const char*)tm_hold.object[1]+0x130);
        if(!tm_component_tf(component,&root))return 0;
        qf_t inverse={-root.q.x,-root.q.y,-root.q.z,root.q.w};
        float delta[3]={head.p[0]-tm_hold.head.p[0],head.p[1]-tm_hold.head.p[1],head.p[2]-tm_hold.head.p[2]};q_rot(inverse,delta,bias);
        for(int i=0;i<3;i++)bias[i]/=root.s[i];
        // Only look travel is bounded. Actual player translation stays intact,
        // so walking out of the authored release box still drops the handles.
        bias[1]+=fminf(18,fmaxf(-18,-40*tm_look_delta(yaw,tm_hold.yaw)-20*(pitch-tm_hold.pitch)));
        bias[2]+=fminf(12,fmaxf(-12,-20*(pitch-tm_hold.pitch)));
        float local[3];for(int i=0;i<3;i++)local[i]=bias[i]*root.s[i];q_rot(root.q,local,bias);
    } else if(tm_hold.phase==2) {
        // Look down or sideways pulls both actual controllers. Returning to the
        // initial view returns the hands; original Blueprint owns the jaw angles.
        float down=fminf(0,pitch-tm_hold.pitch),turn=tm_look_delta(yaw,tm_hold.yaw);
        pull=fminf(30,hypotf(down,turn)*(15.0f/(20.0f*.01745329252f)));
    }
    struct tm_pose_packet packet={.stamp=now,.generation=tm_hold.generation};
    for(int s=0;s<2;s++) {
        struct reload_tf desired=head;
        const float* center=(const float*)((const char*)tm_hold.object[4+s]+0x1d0);
        float move[3]={0};
        if(tm_hold.phase==2 && tm_hold.kind==2) {
            float local[3]={0,0,-pull};q_rot(tm_hold.joint_q[s],local,move);center=tm_hold.origin[s];
            for(int i=0;i<3;i++)move[i]+=head.p[i]-tm_hold.head.p[i];
        }
        for(int i=0;i<3;i++)desired.p[i]=center[i]+tm_hold.correction[s][i]+(tm_hold.kind==1 ? bias[i]:move[i]);
        if(!reload_tf_valid(&desired))return 0;
        tm_raw_position(desired,head,raw_q,raw_p,packet.position[s]);
    }
    pthread_mutex_lock(&tm_pose_mutex);tm_pose_plan=packet;pthread_mutex_unlock(&tm_pose_mutex);return 1;
}
static void tm_finish_prepare(long now);

static int tm_live(int i) {
    return tm_hold.object[i] && cu_weak_get && cu_weak_get(&tm_hold.weak[i])==tm_hold.object[i];
}
static void tm_release(void) {
    pthread_mutex_lock(&tm_pose_mutex);memset(&tm_pose_plan,0,sizeof tm_pose_plan);memset(&tm_pose_frame,0,sizeof tm_pose_frame);tm_pose_served=0;pthread_mutex_unlock(&tm_pose_mutex);
    for(int s=0;s<2;s++)if(tm_hold.owned_input&(1u<<s)) {
        long owned=LONG_MAX;
        __atomic_compare_exchange_n(&g_cmd_until[s?27:29],&owned,tm_hold.prior_input[s],0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
    }
    // A new weapon, another native interaction, or a reused UObject retains its own input.
    for(int s=0;s<2;s++)if((tm_hold.pressed&(1u<<s)) && tm_live(2+s) && ti_down(tm_hold.object[2+s])) {
        void* current=f_GetCurrentGrip(tm_hold.object[2+s]);
        if(!current || (tm_live(4+s) && current==tm_hold.object[4+s]))ti_release(tm_hold.object[2+s]);
    }
    memset(&tm_hold,0,sizeof tm_hold);
}
static void touch_mirror_tick(void* hand,int gameplay,int cancelled) {
    if(!tm_hold.object[0] || (hand!=tm_hold.object[2] && hand!=tm_hold.object[3]))return;
    int valid=gameplay && !cancelled && !__atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE) && phone_active()==tm_hold.object[0];
    for(int i=0;valid && i<6;i++)valid=tm_live(i);
    for(int s=0;valid && s<2;s++)valid=ti_get_hand(tm_hold.object[0],(unsigned char)s)==tm_hold.object[2+s]
        && reload_grip_registered(tm_hold.object[4+s])
        && (tm_hold.phase==1 ? !f_GetCurrentGrip(tm_hold.object[2+s]) && !ti_down(tm_hold.object[2+s])
            : f_GetCurrentGrip(tm_hold.object[2+s])==tm_hold.object[4+s] && ti_down(tm_hold.object[2+s])
                && (tm_hold.owned_input&(1u<<s)) && __atomic_load_n(&g_cmd_until[s?27:29],__ATOMIC_ACQUIRE)==LONG_MAX);
    long now=ptr_now_ms();
    if(!valid || g_touch_weapon_request || g_reload_phase || g_touch_reload_request || touch_quickselect_busy()
        || (tm_hold.phase==1 && (now<tm_hold.start || now-tm_hold.start>1500))) {
        if(tm_failure_logs<8 && tm_hold.phase) {
            ++tm_failure_logs;
            LOGF("INTERACT pair retired phase=%d valid=%d context=%d cancel=%d current=%p,%p down=%d,%d input=%ld,%ld seen=%u age=%ld",
                tm_hold.phase,valid,g_touch_context,cancelled,
                tm_live(2)?f_GetCurrentGrip(tm_hold.object[2]):0,tm_live(3)?f_GetCurrentGrip(tm_hold.object[3]):0,
                tm_live(2)?ti_down(tm_hold.object[2]):-1,tm_live(3)?ti_down(tm_hold.object[3]):-1,
                __atomic_load_n(&g_cmd_until[29],__ATOMIC_ACQUIRE),__atomic_load_n(&g_cmd_until[27],__ATOMIC_ACQUIRE),tm_hold.seen,now-tm_hold.start);
            for(int s=0;s<2;s++)if(tm_live(2+s) && tm_live(4+s) && reload_grip_registered(tm_hold.object[4+s])) {
                struct ti_vec p=tm_grab_location(tm_hold.object[2+s]);
                float squared=tm_release_distance(tm_hold.object[4+s],&p),radius=*(const float*)((const char*)tm_hold.object[2+s]+0x34c);
                LOGF("INTERACT pair release side=%d grab=(%.3f,%.3f,%.3f) distance=%.3f radius=%.3f excess=%.3f",
                    s,p.x,p.y,p.z,squared>=0?sqrtf(squared):-1,radius,squared>=0?fmaxf(0,sqrtf(squared)-radius):-1);
            }
        }
        tm_release();return;
    }
    if(tm_hold.phase==1) {
        int s=hand==tm_hold.object[3];struct reload_tf current;
        if(__atomic_load_n(&tm_pose_ack[s],__ATOMIC_ACQUIRE)==tm_hold.generation && tm_hand_tf(s,&current)) {
            const float* point=(const float*)((const char*)tm_hold.object[4+s]+0x1d0);float error[3],distance=0;
            for(int i=0;i<3;i++){error[i]=point[i]-current.p[i];distance+=error[i]*error[i];}
            if(distance<.25f*.25f)tm_hold.seen|=1u<<s;
            else {
                tm_hold.seen&=~(1u<<s);
                if(distance<30*30)for(int i=0;i<3;i++)tm_hold.correction[s][i]=fminf(25,fmaxf(-25,tm_hold.correction[s][i]+.5f*error[i]));
            }
        }
        if(tm_hold.seen==3 && hand==tm_hold.object[2])tm_finish_prepare(now);
    }
    if(tm_hold.object[0] && !tm_publish(now))tm_release();
}
// GameThread menu-hand Tick also retires pairs if the paused game hands stop ticking.
static void touch_mirror_idle_tick(void) {
    if(tm_hold.object[0] && (g_touch_context!=1 || g_touch_cancel || phone_type()!=1 || rl_paused()))tm_release();
}
static int tm_target(void* pawn,void* hand,void** actor,void** grips) {
    void* grip=ti_best(hand);
    void* parent=interact_world_grip(grip,pawn) ? *(void* const*)((const char*)grip+0x300):0;
    if(!parent || (!interact_is_a(parent,tm_mirror_class())
        && !(tm_trap_class && interact_is_a(parent,tm_trap_class()))))grip=interact_near_world(hand,pawn,0);
    if(!interact_world_grip(grip,pawn))return 0;
    void* mirror=*(void* const*)((const char*)grip+0x300);
    if(!pv_alive(mirror))return 0;
    if(interact_is_a(mirror,tm_mirror_class()) && interact_layout(mirror,0x2b8)) {
        grips[0]=*(void* const*)((const char*)mirror+0x248);grips[1]=*(void* const*)((const char*)mirror+0x260);
    } else if(tm_trap_class && tm_dog_trap && interact_is_a(mirror,tm_trap_class())
        && interact_layout(mirror,0x270) && tm_dog_trap(mirror)) {
        const unsigned char* jaws=*(const unsigned char* const*)((const char*)mirror+0x218);
        void* const* registered=*(void* const* const*)((const char*)mirror+0x200);
        int count=*(const int*)((const char*)mirror+0x220),max=*(const int*)((const char*)mirror+0x224);
        if(!jaws || count!=2 || max<count || max>16 || !registered
            || *(const int*)((const char*)mirror+0x208)!=2
            || *(const int*)((const char*)mirror+0x20c)<2 || *(const int*)((const char*)mirror+0x20c)>16)return 0;
        grips[0]=*(void* const*)(jaws+0x10);grips[1]=*(void* const*)(jaws+0x38);
        if(registered[0]!=grips[0] || registered[1]!=grips[1])return 0;
        if(grip==grips[1]){void* swap=grips[0];grips[0]=grips[1];grips[1]=swap;}
    } else return 0;
    if(grips[0]==grips[1] || grip!=grips[0])return 0;
    void* head=*(void* const*)((const char*)pawn+0x2d8);
    if(!pv_alive(head) || !interact_layout(head,0x1f0) || !interact_is_a(head,ti_scene_class())
        || *(void* const*)((const char*)head+0x20)!=pawn)return 0;
    float q[4],eye[3],forward[3],norm=0;
    memcpy(q,(const char*)head+0x1c0,sizeof q);memcpy(eye,(const char*)head+0x1d0,sizeof eye);
    for(int i=0;i<4;i++)norm+=q[i]*q[i];
    if(!isfinite(norm) || fabsf(norm-1)>.01f)return 0;
    forward[0]=1-2*(q[1]*q[1]+q[2]*q[2]);forward[1]=2*(q[0]*q[1]+q[3]*q[2]);forward[2]=2*(q[0]*q[2]-q[3]*q[1]);
    for(int s=0;s<2;s++) {
        if(!pv_alive(grips[s]) || !interact_world_grip(grips[s],pawn) || !ti_free(grips[s],s)
            || *(void* const*)((const char*)grips[s]+0x300)!=mirror || !ti_in_valid(grips[s]))return 0;
        struct ti_vec center=ti_grab_center(grips[s]);float point[3]={center.x,center.y,center.z},score;
        if(!interact_pickup_score(eye,forward,point,0,&score) || !interact_line_of_sight(grips[s],pawn,eye))return 0;
    }
    *actor=mirror;return 1;
}
static int touch_mirror_interact(void* pawn,long now,int release_only) {
    if(tm_hold.object[0]) {
        int owned=tm_hold.object[0]==pawn;tm_release();
        if(owned)return 1;
    }
    if(release_only || !tm_mirror_class || !tm_holster || !tm_grab_location || !tm_release_distance || !tm_joint_transform
        || !qs_ready || !cu_weak_assign || !cu_weak_get
        || ti_hand || ti_face_until || g_reload_phase || g_touch_reload_request
        || g_touch_cancel || g_touch_context!=1 || g_touch_weapon_request || touch_quickselect_busy()
        || !ti_get_hand || !f_SetGripForcedHover || !ti_grab_center || !ti_in_valid || !ti_line_of_sight
        || !ti_scene_class || phone_active()!=pawn || !pv_alive(pawn) || !interact_layout(pawn,0x2e0))return 0;
    void* hands[2]={ti_get_hand(pawn,0),ti_get_hand(pawn,1)},*grips[2],*mirror=0,*items[2]={0,0};
    for(int s=0;s<2;s++)if(!pv_alive(hands[s]) || !interact_bound_hand(hands[s],pawn,s)
        || *(const int*)((const char*)hands[s]+0x468) || g_cmd_until[s?27:29]>now)return 0;
    if(!tm_target(pawn,hands[0],&mirror,grips))return 0;
    // Only a proved target may put away a slotted pawn gun/knife. Foreign props,
    // world guns, grenades and healing items require their own original release.
    for(int s=0;s<2;s++) {
        void* current=f_GetCurrentGrip(hands[s]);
        if(!current) {
            if(ti_down(hands[s]) || (g_grip_latched&(1u<<(s?27:29))))return 0;
            continue;
        }
        const int slots[]={0,1,3};
        for(unsigned i=0;i<sizeof slots/sizeof *slots;i++) {
            void* binding[5];
            if(qs_binding(pawn,slots[i],binding) && binding[2]==hands[s] && binding[3]==current
                && qs_current(binding) && tw_holster && tw_holster(binding[1])){items[s]=binding[1];break;}
        }
        if(!items[s])return 0;
    }
    for(int s=0;s<2;s++)if(items[s]) {
        tm_holster(items[s],1);
        if(f_GetCurrentGrip(hands[s]))return 1;
        if(ti_down(hands[s]))ti_release(hands[s]);
        __atomic_fetch_and(&g_grip_latched,~(1u<<(s?27:29)),__ATOMIC_ACQ_REL);
    }
    // Recheck callbacks' side effects before the first press. Each original grip
    // still owns handedness, native validity, visibility and authored acceptance.
    void* checked=0,*checked_grips[2];
    if(phone_active()!=pawn || !tm_target(pawn,hands[0],&checked,checked_grips) || checked!=mirror
        || checked_grips[0]!=grips[0] || checked_grips[1]!=grips[1])return 1;
    void* objects[6]={pawn,mirror,hands[0],hands[1],grips[0],grips[1]};
    for(int i=0;i<6;i++) {
        tm_hold.object[i]=objects[i];cu_weak_assign(&tm_hold.weak[i],objects[i]);
        if(!tm_live(i)){tm_release();return 1;}
    }
    tm_hold.phase=1;tm_hold.kind=interact_is_a(mirror,tm_mirror_class()) ? 1:2;tm_hold.start=now;
    if(!++tm_pose_serial)++tm_pose_serial;tm_hold.generation=tm_pose_serial;
    for(int s=0;s<2;s++)__atomic_store_n(&tm_pose_ack[s],0,__ATOMIC_RELEASE);
    if(!tm_publish(now)){tm_release();return 1;}
    LOGF("INTERACT %s pair preparing: native controller poses before both grip presses",tm_hold.kind==1?"mirror":"dog trap");return 1;
}
static void tm_finish_prepare(long now) {
    void* pawn=tm_hold.object[0],*mirror=tm_hold.object[1];
    void* hands[2]={tm_hold.object[2],tm_hold.object[3]},*grips[2]={tm_hold.object[4],tm_hold.object[5]};
    void* checked=0,*checked_grips[2];
    if((ti_action_active() && ti_can_action()) || !tm_target(pawn,hands[0],&checked,checked_grips)
        || checked!=mirror || checked_grips[0]!=grips[0] || checked_grips[1]!=grips[1]
        || !tm_head_tf(&tm_hold.head)) {tm_release();return;}
    pose_angles(2,&tm_hold.yaw,&tm_hold.pitch);
    for(int s=0;s<2;s++) {
        struct reload_tf actual;
        if(!tm_hand_tf(s,&actual)) {tm_release();return;}
        const float* center=(const float*)((const char*)grips[s]+0x1d0);float error=0;
        for(int i=0;i<3;i++){float d=actual.p[i]-center[i];error+=d*d;}
        if(error>=.25f*.25f){tm_hold.seen&=~(1u<<s);return;}
        struct ti_vec point=tm_grab_location(hands[s]);float d2=tm_release_distance(grips[s],&point);
        float radius=*(const float*)((const char*)hands[s]+0x34c);
        if(!isfinite(d2) || d2<0 || !isfinite(radius) || radius<0 || radius>50 || sqrtf(d2)>radius+4.5f) {
            if(tm_failure_logs<8){++tm_failure_logs;LOGF("INTERACT pair declined: authored release distance side=%d squared=%.3f radius=%.3f",s,d2,radius);}
            tm_release();return;
        }
        memcpy(tm_hold.origin[s],(const char*)grips[s]+0x1d0,3*sizeof(float));
        if(tm_hold.kind==2) {
            const unsigned char* jaws=*(const unsigned char* const*)((const char*)mirror+0x218);
            int index=*(void* const*)(jaws+0x10)==grips[s] ? 0:1;
            struct reload_tf joint=tm_joint_transform(mirror,index);
            if(!reload_marker_valid(&joint)){tm_release();return;}
            tm_hold.joint_q[s]=joint.q;
        }
    }
    // Direct native presses own IsGripDown, while Oculus keeps its previous
    // HandTrigger independently. Retain both inputs until release: otherwise the
    // weapon's previous RG=1 becomes an RG-up event after holstering and drops
    // this newly accepted pair. The native current-grip guard ignores the first
    // controller press on a hand which already accepted its direct press.
    for(int s=0;s<2;s++) {
        long prior=__atomic_load_n(&g_cmd_until[s?27:29],__ATOMIC_ACQUIRE);
        if(prior>now || !__atomic_compare_exchange_n(&g_cmd_until[s?27:29],&prior,LONG_MAX,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
            tm_release();return;
        }
        tm_hold.prior_input[s]=prior;tm_hold.owned_input|=1u<<s;
    }
    for(int s=0;s<2;s++) {
        void** vtable=*(void***)hands[s];
        if(phone_active()!=pawn || phone_type()!=1 || rl_paused() || !rl_player_valid() || g_touch_cancel
            || !tm_live(2+s) || !tm_live(4+s) || ti_get_hand(pawn,(unsigned char)s)!=hands[s]
            || !vtable || !vtable[0x740/sizeof(void*)]
            || !((int (*)(const void*))vtable[0x740/sizeof(void*)])(hands[s])
            || f_GetCurrentGrip(hands[s]) || ti_down(hands[s]) || !ti_free(grips[s],s)) {tm_release();return;}
        f_SetGripForcedHover(hands[s],grips[s],1);
        if(ti_best(hands[s])==grips[s] && ti_free(grips[s],s) && ti_in_valid(grips[s])) {
            tm_hold.pressed|=1u<<s;ti_press(hands[s]);
        }
        if(reload_grip_registered(grips[s]))f_SetGripForcedHover(hands[s],grips[s],0);
        if(phone_active()!=pawn || g_touch_cancel || !tm_live(4+s)
            || f_GetCurrentGrip(hands[s])!=grips[s] || !ti_down(hands[s])){tm_release();return;}
    }
    tm_hold.phase=2;
    LOGF("INTERACT %s pair held: original look/movement and authored solver",
        tm_hold.kind==1 ? "mirror":"dog trap");
}
static void install_touch_mirror(unsigned long base) {
    static const struct {unsigned long addr;unsigned words[2];} check[]={
        {0x64bf44c,{0xd10183ff,0xf90023f3}},{0x62bc1e4,{0xd10103ff,0xa90157f6}},
        {0x6354eec,{0xd10483ff,0x6d093bef}},
        {0x63c4278,{0xd10183ff,0xf90023f3}},{0x6210e80,{0xf9413408,0xb4000068}},
        {0x6299ee8,{0xa9be4ff4,0xa9017bfd}},{0x628193c,{0xf81d0ff5,0xa9014ff4}},
        {0x6211160,{0xb9422009,0x6b01013f}},
    };
    tm_mirror_class=0;tm_trap_class=0;tm_dog_trap=0;tm_holster=0;tm_grab_location=0;tm_release_distance=0;tm_joint_transform=0;interact_mirror_hook=0;interact_mirror_tick=0;
    if(!base || !ti_ready || !qs_ready || !cu_weak_assign || !cu_weak_get)return;
    for(unsigned i=0;i<sizeof check/sizeof *check;i++)
        if(memcmp((const void*)(base+check[i].addr),check[i].words,8))return;
    if(*(const unsigned*)(base+0x9e035f0+0x24)!=0x248 || *(const unsigned*)(base+0x9e03560+0x24)!=0x260)return;
    if(*(const unsigned*)(base+0x9d6fa80+0x24)!=0x218 || *(const unsigned*)(base+0x9d6f3c0+0x24)!=0x10)return;
    tm_mirror_class=(void* (*)(void))(base+0x64bf44c);tm_holster=(void (*)(void*,int))(base+0x62bc1e4);
    tm_trap_class=(void* (*)(void))(base+0x63c4278);tm_dog_trap=(int (*)(const void*))(base+0x6210e80);
    tm_grab_location=(struct ti_vec (*)(const void*))(base+0x6299ee8);
    tm_release_distance=(float (*)(const void*,const struct ti_vec*))(base+0x628193c);
    tm_joint_transform=(struct reload_tf (*)(const void*,int))(base+0x6211160);
    interact_mirror_hook=touch_mirror_interact;interact_mirror_tick=touch_mirror_tick;
    LOGF("INTERACT pairs ready: native mirror/dog-trap grips, look/movement and authored solvers");
}
#endif
