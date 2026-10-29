// Include after reload/body/aim diagnostic helpers; run after the native right hand Tick.
#ifndef RE4FIX_AIM_H
#define RE4FIX_AIM_H
#include "re4fix_weapon_pose.h"
static int mobile_prefs_fov(void);
static int mobile_prefs_weapon_fov(void);
static int touch_aim_ready;
static int touch_motion_pose_busy(void);
// Phone FPS framing (centimetres, head frame: forward, right, up). Tunable: "gunpose hip|ads|draw x y z [deg]".
static float g_gun_hip[3] = {46.0f, 15.0f, -16.0f};   // hip: farther and smaller than the VR-derived 35/14/-15, low right
static float g_gun_ads[3] = {52.0f, 0.0f, -2.0f};     // bore 2 cm under the view axis: front sight on the screen centre
static float g_gun_draw[4] = {26.0f, 16.0f, -58.0f, 55.0f};   // draw starts below the view, muzzle pitched down (deg)
static int g_gun_draw_ms = 320;
// Magazine reload offset (cm, view frame: forward/right/up) and roll/lift (deg), blended in while reloading.
static float g_gun_reload[5] = {-4.0f, -3.0f, 3.0f, 26.0f, 9.0f};
static int g_gun_reload_ms[2] = {220, 280};   // tilt in / out
static struct reload_tf (*aim_socket)(const void*,unsigned long long,int);
static const struct reload_tf* (*aim_default_mesh)(const void*);
static struct reload_tf (*aim_mesh_relative)(const void*);
static void (*aim_mesh_set_relative)(void*,const struct reload_tf*);
static void* (*aim_gun_class)(void);
static int (*aim_blending)(const void*), (*aim_flourishing)(const void*), (*aim_recoiling)(const void*);
static struct {
    void *pawn,*hand,*gun,*grip,*reference_root;
    unsigned long long pawn_name,hand_name,gun_name,grip_name,reference_name;
    struct reload_tf fire_local;
    long drawn_at,changed_at,last_tick,reload_changed_at;
    float from,blend,reload_from,reload_level;
    float draw_from[3];
    int target,reload_target;
    unsigned equip_generation;
    struct weapon_sway_state sway;
    int inventory_hold;
    void* inventory_root;
    unsigned long long inventory_root_name;
    struct reload_tf inventory_pose;
} aim_fps;

static float aim_ease(long now,long start,long duration) {
    if (now<=start) return 0;
    if (now-start>=duration) return 1;
    float t=(float)(now-start)/(float)duration;
    return t*t*(3.0f-2.0f*t);
}

static struct reload_tf aim_relative_marker(struct reload_tf parent,struct reload_tf marker) {
    qf_t inverse={-parent.q.x,-parent.q.y,-parent.q.z,parent.q.w};
    float offset[3]={marker.p[0]-parent.p[0],marker.p[1]-parent.p[1],marker.p[2]-parent.p[2]};
    q_rot(inverse,offset,marker.p);marker.q=q_mul(inverse,marker.q);
    return marker;
}

static struct reload_tf aim_world_marker(struct reload_tf parent,struct reload_tf marker) {
    float offset[3];q_rot(parent.q,marker.p,offset);
    for (int i=0;i<3;i++) marker.p[i]=parent.p[i]+offset[i];
    marker.q=q_mul(parent.q,marker.q);
    return marker;
}

// Quest's one-handed shotgun recoil can rotate the mesh out of a phone's
// entire view. Retain the original recoil path, compressed to 6 degrees/3 cm
// around its original default mesh pose. Bone animation and native recoil
// timers are independent of this component transform and stay untouched.
static int aim_mobile_recoil_pose(struct reload_tf neutral,struct reload_tf native,struct reload_tf* output) {
    if(!output || !reload_tf_valid(&neutral) || !reload_tf_valid(&native))return -1;
    qf_t delta=q_mul((qf_t){-neutral.q.x,-neutral.q.y,-neutral.q.z,neutral.q.w},native.q);
    if(delta.w<0){delta.x=-delta.x;delta.y=-delta.y;delta.z=-delta.z;delta.w=-delta.w;}
    float half=acosf(fminf(1.0f,fmaxf(0.0f,delta.w))),distance=0;
    for(int i=0;i<3;i++){float d=native.p[i]-neutral.p[i];distance+=d*d;}
    if(!isfinite(distance))return -1;
    distance=sqrtf(distance);
    float fraction=1;
    if(half>0.05235987756f)fraction=0.05235987756f/half;
    if(distance>3.0f)fraction=fminf(fraction,3.0f/distance);
    if(fraction>=1){*output=native;return 0;}
    struct reload_tf bounded=native;
    if(half>0.000001f){
        float scale=sinf(half*fraction)/sinf(half);
        delta.x*=scale;delta.y*=scale;delta.z*=scale;delta.w=cosf(half*fraction);
        bounded.q=q_mul(neutral.q,delta);
    }
    for(int i=0;i<3;i++)bounded.p[i]=neutral.p[i]+(native.p[i]-neutral.p[i])*fraction;
    if(!reload_tf_valid(&bounded))return -1;
    *output=bounded;return 1;
}

static int aim_shotgun_recoil(void* gun,void* mesh) {
    if(!aim_recoiling || !aim_recoiling(gun))return 0;
    // The original getter asserts if UpdateRecoil has not cached this pose.
    if(!aim_default_mesh || !aim_mesh_relative || !aim_mesh_set_relative
        || *(const unsigned char*)((const char*)gun+0x8f8)!=1
        || *(void* const*)((const char*)mesh+0x20)!=gun)return -1;
    unsigned long long gn,mn;memcpy(&gn,(const char*)gun+0x18,8);memcpy(&mn,(const char*)mesh+0x18,8);
    const struct reload_tf* native_default=aim_default_mesh(gun);
    if(native_default!=(const struct reload_tf*)((const char*)gun+0x900))return -1;
    struct reload_tf neutral=*native_default,native=aim_mesh_relative(mesh),bounded;
    int result=aim_mobile_recoil_pose(neutral,native,&bounded);
    if(result<=0)return result;
    unsigned long long current_gn,current_mn;
    memcpy(&current_gn,(const char*)gun+0x18,8);memcpy(&current_mn,(const char*)mesh+0x18,8);
    int interactivity=rl_interactivity(gun);
    if(g_touch_context!=1 || g_touch_weapon!=2 || g_touch_cancel || rl_paused() || !rl_player_valid()
        || current_gn!=gn || current_mn!=mn || *(void* const*)((const char*)gun+0x540)!=mesh
        || interactivity<1 || interactivity>2
        || *(void* const*)((const char*)mesh+0x20)!=gun)return -1;
    aim_mesh_set_relative(mesh,&bounded);
    static unsigned logs;
    if(logs<8){logs++;LOGF("AIMRECOIL bounded owned shotgun native mesh gun=%p mesh=%p",gun,mesh);}
    return 1;
}
#include "re4fix_scope.h"

static int aim_direction_delta(qf_t fire,qf_t head,qf_t* output) {
    float from[3],to[3];
    q_rot(fire,(float[3]){0,1,0},from);q_rot(head,(float[3]){1,0,0},to);
    float a=from[0]*from[0]+from[1]*from[1]+from[2]*from[2];
    float b=to[0]*to[0]+to[1]*to[1]+to[2]*to[2];
    if (!isfinite(a) || !isfinite(b) || a<0.000001f || b<0.000001f) return 0;
    a=1.0f/sqrtf(a);b=1.0f/sqrtf(b);
    for (int i=0;i<3;i++) { from[i]*=a;to[i]*=b; }
    float dot=from[0]*to[0]+from[1]*to[1]+from[2]*to[2];
    dot=fmaxf(-1.0f,fminf(1.0f,dot));
    // Opposite directions have no unique shortest axis; reject instead of choosing arbitrary roll.
    if (dot<=-0.999999f) return 0;
    qf_t delta={from[1]*to[2]-from[2]*to[1],from[2]*to[0]-from[0]*to[2],
                from[0]*to[1]-from[1]*to[0],1.0f+dot};
    float norm=delta.x*delta.x+delta.y*delta.y+delta.z*delta.z+delta.w*delta.w;
    if (!isfinite(norm) || norm<0.000001f) return 0;
    norm=1.0f/sqrtf(norm);
    delta.x*=norm;delta.y*=norm;delta.z*=norm;delta.w*=norm;
    *output=delta;return 1;
}

static int aim_mobile_pose(struct reload_tf hand, struct reload_tf sights, struct reload_tf fire,
                           struct reload_tf head, float aiming, struct reload_tf* output) {
    if (!reload_tf_valid(&head)) return -1;
    if (!reload_tf_valid(&hand)) return -2;
    if (!reload_tf_valid(&sights)) return -3;
    if (!reload_tf_valid(&fire)) return -6;
    if (!isfinite(aiming) || aiming<0 || aiming>1) return -8;
    qf_t delta;
    if (!aim_direction_delta(fire.q,head.q,&delta)) return -7;
    const float* marker=fire.p;
    float target[3],offset[3],forward[3]={g_gun_ads[0]+(g_gun_hip[0]-g_gun_ads[0])*(1.0f-aiming),
        g_gun_ads[1]+(g_gun_hip[1]-g_gun_ads[1])*(1.0f-aiming),g_gun_ads[2]+(g_gun_hip[2]-g_gun_ads[2])*(1.0f-aiming)};
    q_rot(head.q,forward,offset);
    for (int i=0;i<3;i++) target[i]=head.p[i]+offset[i];
    // shortcut: rigid alignment requires unit scale; use scaled composition if an authored rig needs it.
    struct reload_tf pose=hand;
    float from[3]={hand.p[0]-marker[0],hand.p[1]-marker[1],hand.p[2]-marker[2]};
    q_rot(delta,from,offset);
    for (int i=0;i<3;i++) pose.p[i]=target[i]+offset[i];
    pose.q=q_mul(delta,hand.q);
    if (!reload_tf_valid(&pose)) return -4;
    float dx=pose.p[0]-hand.p[0],dy=pose.p[1]-hand.p[1],dz=pose.p[2]-hand.p[2];
    if (dx*dx+dy*dy+dz*dz>150.0f*150.0f) return -5;
    *output=pose;
    return 1;
}

static void aim_mobile_reject(const char* reason) {
    static unsigned logs;
    if (logs<8) { logs++; LOGF("AIMALIGN rejected: %s",reason); }
}
static int aim_mobile_size(const void* object, int minimum) {
    const char* cls=object ? *(const char* const*)((const char*)object+0x10) : 0;
    int size=cls ? *(const int*)(cls+0x58) : 0;
    return size>=minimum && size<=0x10000;
}

static int aim_fire_position_raw(struct reload_tf raw,struct reload_tf native) {
    if (!reload_tf_valid(&raw)) return 0;
    float x=raw.p[0]-native.p[0],y=raw.p[1]-native.p[1],z=raw.p[2]-native.p[2];
    // Native obstruction/assist can move the firing origin; never chase that adjustment with the hand.
    return x*x+y*y+z*z<=0.05f*0.05f;
}

static void aim_mobile_post_diag(void* root,void* gun,const struct reload_tf* head,
                                const struct reload_tf* commanded,int aiming) {
    static unsigned logs;
    if (logs>=8 || !__atomic_load_n(&g_aim_diag_request,__ATOMIC_ACQUIRE)) return;
    logs++;
    struct reload_tf cached,sights=aim_sights(gun);
    struct aim_fire_tf fire=aim_fire(gun);
    memcpy(&cached,(const char*)root+0x1c0,sizeof cached);
    qf_t inv={-head->q.x,-head->q.y,-head->q.z,head->q.w};
    float delta[3]={sights.p[0]-head->p[0],sights.p[1]-head->p[1],sights.p[2]-head->p[2]},sight_p[3],fire_p[3],shot[3],view_shot[3];
    q_rot(inv,delta,sight_p);q_rot(fire.tf.q,(float[3]){0,1,0},shot);q_rot(inv,shot,view_shot);
    for (int i=0;i<3;i++)delta[i]=fire.tf.p[i]-head->p[i];
    q_rot(inv,delta,fire_p);
    float dot=fabsf(cached.q.x*commanded->q.x+cached.q.y*commanded->q.y+cached.q.z*commanded->q.z+cached.q.w*commanded->q.w);
    LOGF("AIMALIGNPOST aim=%d commanded_p=%.3f,%.3f,%.3f cached_p=%.3f,%.3f,%.3f cached_q_dot=%.6f sights_head_p=%.3f,%.3f,%.3f fire_head_forward=%.6f,%.6f,%.6f fire_head_p=%.3f,%.3f,%.3f valid=%d,%d,%d",
         aiming,commanded->p[0],commanded->p[1],commanded->p[2],cached.p[0],cached.p[1],cached.p[2],dot,
         sight_p[0],sight_p[1],sight_p[2],view_shot[0],view_shot[1],view_shot[2],fire_p[0],fire_p[1],fire_p[2],
         reload_tf_valid(&cached),reload_tf_valid(&sights),reload_tf_valid(&fire.tf));
}

static int aim_inventory_held(void* hand,long now) {
    if(aim_fps.inventory_hold<=0 || (g_touch_context!=2&&g_touch_context!=1))return 0;
    int resuming=g_touch_context==1&&!g_touch_cancel&&!rl_paused();
    aim_fps.last_tick=now;
    if(hand!=aim_fps.hand)return !resuming;
    if(resuming)aim_fps.inventory_hold=0;
    void* pawn=body_player(hand),*gun=touch_equipped_gun(hand);
    if(!pawn||pawn!=aim_fps.pawn||!gun||gun!=aim_fps.gun||!aim_mobile_size(pawn,0x2e0)
        ||!aim_mobile_size(gun,0xd40)||*(void* const*)((const char*)gun+0x3c0)!=pawn
        ||rl_primary(gun)!=hand)return !resuming;
    void* grip=*(void* const*)((const char*)gun+0x270),*root=*(void* const*)((const char*)hand+0x130);
    if(grip!=aim_fps.grip||!grip||!reload_grip_registered(grip)||f_GetCurrentGrip(hand)!=grip
        ||*(void* const*)((const char*)grip+0x20)!=gun||root!=aim_fps.inventory_root
        ||!aim_mobile_size(root,0x1f0)||!reload_tf_valid(&aim_fps.inventory_pose))return !resuming;
    void* objects[]={pawn,hand,gun,grip,root};
    unsigned long long names[]={aim_fps.pawn_name,aim_fps.hand_name,aim_fps.gun_name,aim_fps.grip_name,aim_fps.inventory_root_name};
    for(unsigned i=0;i<5;i++) {
        unsigned long long name;memcpy(&name,(const char*)objects[i]+0x18,8);if(name!=names[i])return !resuming;
    }
    // Pausing retires Quick Select every frame; the same live grip keeps its neutral calibration on return.
    if(resuming){aim_fps.equip_generation=touch_quickselect_pose_generation();return 0;}
    rl_world_transform(root,&aim_fps.inventory_pose,0,0,0);return 1;
}
static void touch_aim_tick(void* hand) {
    if (!__atomic_load_n(&touch_aim_ready,__ATOMIC_ACQUIRE) || !qs_gt() || !hand) return;
    if (!aim_mobile_size(hand,0x51c)) { aim_mobile_reject("hand layout"); return; }
    int side=*(const unsigned char*)((const char*)hand+0x314);
    if(side>1)return;
    long now=ptr_now_ms();
    if(aim_inventory_held(hand,now))return;
    void* pawn=body_player(hand);
    if(!pawn||pawn!=body_active())return;
    struct touch_quickselect_state selected=touch_quickselect_state(now);
    if(selected.fire_side>=0 && selected.fire_side!=side)return;
    if (g_touch_context!=1 || g_touch_weapon!=2 || g_touch_cancel || body_type()!=1
        || __atomic_load_n(&g_touch_weapon_request,__ATOMIC_ACQUIRE) || touch_quickselect_busy()
        || touch_motion_pose_busy()) {
        memset(&aim_fps,0,sizeof aim_fps);return;
    }
    if (rl_paused() || !rl_player_valid()) {
        memset(&aim_fps,0,sizeof aim_fps);return;
    }
    if (!aim_mobile_size(pawn,0x2e0)) { aim_mobile_reject("pawn layout"); return; }
    void* gun=touch_equipped_gun(hand);
    if (!gun) return;
    if (!aim_mobile_size(gun,0xd40)) { aim_mobile_reject("gun layout"); return; }
    if (*(void* const*)((const char*)gun+0x3c0)!=pawn || !aim_gun_class
        || !reload_is_a(gun,aim_gun_class())) return;
    void* grip=*(void* const*)((const char*)gun+0x270);
    int interactivity=rl_interactivity(gun);
    // Native control-disabled state still needs its FPS pose; otherwise the centred VR hand looks like ADS.
    if (!grip || !reload_grip_registered(grip) || f_GetCurrentGrip(hand)!=grip
        || *(void* const*)((const char*)grip+0x20)!=gun
        || rl_primary(gun)!=hand || interactivity<1 || interactivity>2) return;
    // Magazine reloads keep the gun in the first-person pose, tilted toward the incoming magazine; family
    // (shell/round) reloads and the shotgun pump keep their native hand poses.
    int reloading=g_reload_phase || __atomic_load_n(&g_touch_reload_request,__ATOMIC_ACQUIRE) || rl_reloading(gun);
    // The shotgun pump keeps the first-person pose (the off hand strokes the forestock in gun space); resetting
    // here dropped the gun to the raw VR hand and replayed the draw after every shot.
    int cycling=touch_weapon_cycle_busy();
    if (g_reload_phase==7 || ((reloading || cycling) && !aim_fps.gun)) {
        aim_fps.last_tick=now;aim_fps.from=aim_fps.blend=0;aim_fps.target=0;
        aim_fps.reload_level=aim_fps.reload_from=0;aim_fps.reload_target=0;return;
    }
    void* root=*(void* const*)((const char*)hand+0x130);
    void* head_component=*(void* const*)((const char*)pawn+0x2d8);
    void* mesh=*(void* const*)((const char*)gun+0x540);
    if (!aim_mobile_size(root,0x1f0) || !aim_mobile_size(head_component,0x1f0)
        || !aim_mobile_size(mesh,0x1f0)) { aim_mobile_reject("missing/short scene component"); return; }
    struct reload_tf current,head=aim_head(pawn),sights=aim_sights(gun),pose;
    qf_t cached_camera_q=head.q;
    if (reload_tf_valid(&head)) {   // bring the one-frame-old camera rotation to this frame's look
        head.q=q_mul(head.q,pose_camera_delta_ue());
        float n=sqrtf(head.q.x*head.q.x+head.q.y*head.q.y+head.q.z*head.q.z+head.q.w*head.q.w);
        head.q.x/=n;head.q.y/=n;head.q.z/=n;head.q.w/=n;
    }
    if (g_pose_trace>0 && reload_tf_valid(&head)) {   // dev: corrected camera vs. this frame's look latch
        float f[3]={1,0,0},w[3],cached[3];q_rot(head.q,f,w);q_rot(cached_camera_q,f,cached);
        unsigned long long native_frame=g_pose_frame_counter ? __atomic_load_n(g_pose_frame_counter,__ATOMIC_ACQUIRE):0;
        pthread_mutex_lock(&g_pose_latch_lock);
        unsigned long long latch_frame=g_pose_latch_frame;
        float latch_yaw=g_pose_latch_yaw,latch_pitch=g_pose_latch_pitch,previous_yaw=g_pose_prev_yaw,previous_pitch=g_pose_prev_pitch;
        pthread_mutex_unlock(&g_pose_latch_lock);
        LOGF("CAMTRACE nativeFrame=%llu latchFrame=%llu camYaw=%.3f camPitch=%.3f cachedYaw=%.3f cachedPitch=%.3f latchYaw=%.3f latchPitch=%.3f previousYaw=%.3f previousPitch=%.3f",
             native_frame,latch_frame,atan2f(w[1],w[0])*57.29578f,asinf(fmaxf(-1.0f,fminf(1.0f,w[2])))*57.29578f,
             atan2f(cached[1],cached[0])*57.29578f,asinf(fmaxf(-1.0f,fminf(1.0f,cached[2])))*57.29578f,
             latch_yaw*57.29578f,latch_pitch*57.29578f,previous_yaw*57.29578f,previous_pitch*57.29578f);
    }
    struct aim_fire_tf fire=aim_fire(gun);
    memcpy(&current,(const char*)root+0x1c0,sizeof current);
    int aiming=reloading ? 0 : !!g_touch_aim;
    if (!aim_socket || !aim_mobile_size(mesh,0x520)
            || *(struct reload_tf (**)(const void*,unsigned long long,int))((const char*)*(const void* const*)mesh+0x430)!=aim_socket) {
        aim_mobile_reject("unknown FireBone socket getter/layout");return;
    }
    struct reload_tf raw=aim_socket(mesh,*(const unsigned long long*)((const char*)gun+0x620),0);
    if (!reload_tf_valid(&raw) || !reload_tf_valid(&fire.tf)) {
        aim_mobile_reject("invalid/non-unit native FireBone transform");return;
    }
    struct reload_tf reference=current;
    void* reference_root=touch_weapon_cycle_pose_reference(hand,gun,&reference);
    if(!reference_root){reference=current;reference_root=root;}
    if(reference_root!=root){
        int bounded=aim_shotgun_recoil(gun,mesh);
        if(bounded<0){aim_mobile_reject("invalid native shotgun recoil pose");return;}
        if(bounded){
            // Read the actual same FireBone after its original mesh setter;
            // sights/fire getters above may contain the previous VR-size kick.
            sights=aim_sights(gun);fire=aim_fire(gun);
            raw=aim_socket(mesh,*(const unsigned long long*)((const char*)gun+0x620),0);
            if(!reload_tf_valid(&raw) || !reload_tf_valid(&fire.tf))return;
        }
    }
    unsigned long long pn,hn,gn,rn,refn;
    memcpy(&pn,(const char*)pawn+0x18,8);memcpy(&hn,(const char*)hand+0x18,8);
    memcpy(&gn,(const char*)gun+0x18,8);memcpy(&rn,(const char*)grip+0x18,8);
    memcpy(&refn,(const char*)reference_root+0x18,8);
    if (aim_fps.pawn!=pawn || aim_fps.hand!=hand || aim_fps.gun!=gun || aim_fps.grip!=grip
        || aim_fps.pawn_name!=pn || aim_fps.hand_name!=hn || aim_fps.gun_name!=gn || aim_fps.grip_name!=rn
        || aim_fps.reference_root!=reference_root || aim_fps.reference_name!=refn
        || aim_fps.equip_generation!=touch_quickselect_pose_generation()
        || now<aim_fps.last_tick || (now-aim_fps.last_tick>5000 && aim_fps.inventory_hold<=0)) {
        memset(&aim_fps,0,sizeof aim_fps);
        // Only acquisition reads the firing origin; a cached neutral pose cannot chase native assist.
        if (!aim_fire_position_raw(raw,fire.tf)) {
            aim_mobile_reject("native firing origin adjusted during neutral acquisition");return;
        }
        // A neutral grip reference preserves the native mesh recoil and draw flourish.
        if (!aim_blending || !aim_flourishing || !aim_recoiling || aim_blending(gun)
            || aim_flourishing(gun) || aim_recoiling(gun)
            || !reload_tf_valid(&current) || !reload_tf_valid(&head)) return;
        struct reload_tf local=aim_relative_marker(reference,raw);
        if (!reload_tf_valid(&local)) return;
        aim_fps.pawn=pawn;aim_fps.hand=hand;aim_fps.gun=gun;aim_fps.grip=grip;
        aim_fps.pawn_name=pn;aim_fps.hand_name=hn;aim_fps.gun_name=gn;aim_fps.grip_name=rn;
        aim_fps.reference_root=reference_root;aim_fps.reference_name=refn;
        aim_fps.fire_local=local;aim_fps.drawn_at=aim_fps.changed_at=now;
        aim_fps.equip_generation=touch_quickselect_pose_generation();
        float initial[3]={raw.p[0]-head.p[0],raw.p[1]-head.p[1],raw.p[2]-head.p[2]};
        q_rot((qf_t){-head.q.x,-head.q.y,-head.q.z,head.q.w},initial,aim_fps.draw_from);
        LOGF("FPSPOSE neutral gun=%p hand=%p draw=%dms hip=%.0f,%.0f,%.0fcm ADS=%.0f,%.0f,%.0fcm",gun,hand,g_gun_draw_ms,
             g_gun_hip[0],g_gun_hip[1],g_gun_hip[2],g_gun_ads[0],g_gun_ads[1],g_gun_ads[2]);
    }
    aim_fps.last_tick=now;
    aim_fps.reload_level=aim_fps.reload_from+((float)aim_fps.reload_target-aim_fps.reload_from)
        *aim_ease(now,aim_fps.reload_changed_at,aim_fps.reload_target ? g_gun_reload_ms[0] : g_gun_reload_ms[1]);
    if (aim_fps.reload_target!=reloading) {
        aim_fps.reload_from=aim_fps.reload_level;aim_fps.reload_target=reloading;aim_fps.reload_changed_at=now;
    }
    aim_fps.blend=aim_fps.from+((float)aim_fps.target-aim_fps.from)*aim_ease(now,aim_fps.changed_at,180);
    if (aim_fps.target!=aiming) {
        aim_fps.from=aim_fps.blend;aim_fps.target=aiming;aim_fps.changed_at=now;
    }
    struct reload_tf neutral=aim_world_marker(reference,aim_fps.fire_local),view=head;
    float sign=side==1 ? 1.0f : -1.0f;
    float hip=1.0f-aim_fps.blend;
    float rb=aim_fps.reload_level;
    float desired[3]={g_gun_ads[0]+(g_gun_hip[0]-g_gun_ads[0])*hip,
                      sign*(g_gun_ads[1]+(g_gun_hip[1]-g_gun_ads[1])*hip),
                      g_gun_ads[2]+(g_gun_hip[2]-g_gun_ads[2])*hip},lower[3];
    // Draw: rise from below the view with the muzzle pitched down, ease-out (classic FPS draw).
    float t=now<=aim_fps.drawn_at ? 0.0f : (float)(now-aim_fps.drawn_at)/(float)g_gun_draw_ms;
    float remaining=t>=1.0f ? 0.0f : (1.0f-t)*(1.0f-t)*(1.0f-t);
    float start[3]={g_gun_draw[0],sign*g_gun_draw[1],g_gun_draw[2]};
    for (int i=0;i<3;i++) desired[i]=(start[i]-desired[i])*remaining;
    q_rot(head.q,desired,lower);
    for (int i=0;i<3;i++) view.p[i]+=lower[i];
    if (remaining>0.0f) {
        float half=g_gun_draw[3]*remaining*0.00872665f;   // +Y (right axis) rotation pitches the bore down
        qf_t pitch={0,sinf(half),0,cosf(half)};
        view.q=q_mul(head.q,pitch);
    }
    if (rb>0.0f) {   // reload: roll the gun toward the centre and lift the muzzle (mirrored for a left primary)
        float roll=sign*g_gun_reload[3]*rb*0.00872665f,lift=-g_gun_reload[4]*rb*0.00872665f;
        qf_t tilt=q_mul((qf_t){sinf(roll),0,0,cosf(roll)},(qf_t){0,sinf(lift),0,cosf(lift)});
        // aim_mobile_pose places the gun at view.q * (hip/ADS offset): keep that offset unrotated by the tilt
        // and add the reload translation in the view frame.
        float hip_blend=1.0f-aim_fps.blend;
        float fwd[3]={g_gun_ads[0]+(g_gun_hip[0]-g_gun_ads[0])*hip_blend,g_gun_ads[1]+(g_gun_hip[1]-g_gun_ads[1])*hip_blend,
                      g_gun_ads[2]+(g_gun_hip[2]-g_gun_ads[2])*hip_blend},turned[3],shift[3];
        q_rot(tilt,fwd,turned);
        float comp[3]={fwd[0]-turned[0]+g_gun_reload[0]*rb,fwd[1]-turned[1]+sign*g_gun_reload[1]*rb,
                       fwd[2]-turned[2]+g_gun_reload[2]*rb};
        q_rot(view.q,comp,shift);
        for (int i=0;i<3;i++) view.p[i]+=shift[i];
        view.q=q_mul(view.q,tilt);
    }
    // The shared pose solver's right-hand hip offset is mirrored for the actual native left primary.
    if(!side){q_rot(head.q,(float[3]){0,-2.0f*g_gun_hip[1]*hip,0},lower);
        for(int i=0;i<3;i++)view.p[i]+=lower[i];}
    float motion=hip*(1.0f-rb)*(1.0f-remaining),shift[3],roll=0,view_forward[3];
    q_rot(head.q,(float[3]){1,0,0},view_forward);
    weapon_sway_step(&aim_fps.sway,now,atan2f(view_forward[1],view_forward[0]),
        asinf(fmaxf(-1.0f,fminf(1.0f,view_forward[2]))),reloading || cycling ? 0 : motion,shift,&roll);
    static unsigned sway_logs;
    if(sway_logs<6 && (fabsf(shift[1])>.02f || fabsf(shift[2])>.02f || fabsf(roll)>.02f)) {
        ++sway_logs;LOGF("WEAPONSWAY native view shift=%.4f,%.4f roll=%.4f hip=%.3f",shift[1],shift[2],roll,motion);
    }
    // Inventory retires ADS before its camera zoom resets; freeze the future hip framing.
    shift[0]+=(weapon_fov_depth(g_gun_hip[0],aim_fps.inventory_hold<0 ? (float)mobile_prefs_fov():g_fov_h_deg,(float)mobile_prefs_weapon_fov())-g_gun_hip[0])*motion;
    q_rot(head.q,shift,lower);for(int i=0;i<3;i++)view.p[i]+=lower[i];
    if(roll){float half=roll*0.00872664626f;view.q=q_mul(view.q,(qf_t){sinf(half),0,0,cosf(half)});}
    int result=aim_mobile_pose(current,sights,neutral,view,aim_fps.blend,&pose);
    if (result<0) {
        const char* reason[]={"","invalid/non-unit head transform","invalid/non-unit hand transform",
                              "invalid/non-unit sights transform","invalid composed hand transform","target exceeds arm reach",
                              "invalid/non-unit native FireBone transform","opposite/invalid shot directions","invalid ADS blend"};
        aim_mobile_reject(reason[-result]); return;
    }
    (void)aim_scope_adjust(gun,pawn,hand,current,head,aim_fps.drawn_at,aim_fps.blend,&pose);
    int reloading_now=g_reload_phase || __atomic_load_n(&g_touch_reload_request,__ATOMIC_ACQUIRE) || rl_reloading(gun);
    (void)cycling;
    if (g_touch_context!=1 || g_touch_weapon!=2 || g_touch_cancel || g_reload_phase==7 || reloading_now!=reloading
        || rl_interactivity(gun)!=interactivity || rl_primary(gun)!=hand || f_GetCurrentGrip(hand)!=grip
        || (!reloading && !!g_touch_aim!=aiming)
        || __atomic_load_n(&g_touch_weapon_request,__ATOMIC_ACQUIRE) || touch_quickselect_busy()
        || touch_motion_pose_busy()) {
        if (g_touch_context!=1 || g_touch_weapon!=2 || g_touch_cancel
            || __atomic_load_n(&g_touch_weapon_request,__ATOMIC_ACQUIRE)) memset(&aim_fps,0,sizeof aim_fps);
        return;
    }
    if(!touch_weapon_cycle_primary_transform(hand,gun,root,&current,&pose))rl_world_transform(root,&pose,0,0,0);
    if(aim_fps.inventory_hold<0) {
        aim_fps.inventory_root=root;memcpy(&aim_fps.inventory_root_name,(const char*)root+0x18,8);
        aim_fps.inventory_pose=pose;
    } else aim_fps.inventory_hold=0;
    aim_mobile_post_diag(root,gun,&head,&pose,aiming);
}

static void touch_aim_inventory_prepare(void* hand) {
    if(!__atomic_load_n(&touch_aim_ready,__ATOMIC_ACQUIRE)||!qs_gt()||!hand||g_touch_context!=1
        ||g_touch_aim||g_touch_cancel||rl_paused()||!rl_player_valid()||!rl_left||!rl_right)return;
    void* pawn=body_player(hand);if(!pawn||pawn!=body_active()||pawn!=aim_fps.pawn)return;
    struct touch_quickselect_state state=touch_quickselect_state(ptr_now_ms());
    void* primary=state.fire_side==0 ? rl_left(pawn):state.fire_side==1 ? rl_right(pawn):0;
    if(!primary||primary!=aim_fps.hand||touch_equipped_gun(primary)!=aim_fps.gun)return;
    aim_fps.from=aim_fps.blend=0;aim_fps.target=0;aim_fps.changed_at=ptr_now_ms();
    aim_fps.drawn_at=ptr_now_ms()-g_gun_draw_ms;
    aim_fps.inventory_hold=-1;aim_fps.inventory_root=0;
    touch_aim_tick(primary);
}
static void touch_aim_inventory_result(int accepted) {
    aim_fps.inventory_hold=accepted&&aim_fps.inventory_hold<0&&aim_fps.inventory_root ? 1:0;
}

unsigned long g_tgt_aim_try_pause;
#if defined(__aarch64__)
__attribute__((naked)) static int gate_aim_try_pause(unsigned short start) {
    asm volatile(".inst 0xf81d0ff5\n" ".inst 0xa9014ff4\n" ".inst 0xa9027bfd\n" ".inst 0x910083fd\n"
        "adrp x16, :got:g_tgt_aim_try_pause\n" "ldr x16, [x16, :got_lo12:g_tgt_aim_try_pause]\n"
        "ldr x16, [x16]\n" "br x16\n");
}
#endif
static int hook_aim_try_pause(unsigned short start) {
    int prepared=0;
    // Queued Pause reaches the same native entry as Inventory, after its own availability checks.
    if(__atomic_load_n(&touch_aim_ready,__ATOMIC_ACQUIRE)&&qs_gt()&&!aim_fps.inventory_hold
        &&g_touch_context==1&&body_active&&body_type&&body_type()==1&&rl_left) {
        void* pawn=body_active();
        if(pawn&&pawn==aim_fps.pawn) {
            touch_aim_inventory_prepare(rl_left(pawn));
            prepared=aim_fps.inventory_hold<0;
        }
    }
    int accepted=gate_aim_try_pause(start);
    if(prepared)touch_aim_inventory_result(accepted);
    return accepted;
}

unsigned long g_tgt_aim_bio4_hand;
#if defined(__aarch64__)
__attribute__((naked)) static void gate_aim_bio4_hand(void* hand,float dt) {
    asm volatile(".inst 0x6db63bef\n" ".inst 0x6d0133ed\n" ".inst 0x6d022beb\n" ".inst 0x6d0323e9\n"
        "adrp x16, :got:g_tgt_aim_bio4_hand\n" "ldr x16, [x16, :got_lo12:g_tgt_aim_bio4_hand]\n"
        "ldr x16, [x16]\n" "br x16\n");
}
#endif
static void hook_aim_bio4_hand(void* hand,float dt) {
    gate_aim_bio4_hand(hand,dt);
    touch_aim_tick(hand); // The derived native Tick continues writing hand transforms after the hooked base Tick.
}

unsigned long g_tgt_aim_shotgun_post;
#if defined(__aarch64__)
__attribute__((naked)) static void gate_aim_shotgun_post(void* gun,float dt) {
    asm volatile(".inst 0xd10283ff\n" ".inst 0xfd0033e8\n" ".inst 0xa90757f6\n" ".inst 0xa9084ff4\n"
        "adrp x16, :got:g_tgt_aim_shotgun_post\n" "ldr x16, [x16, :got_lo12:g_tgt_aim_shotgun_post]\n"
        "ldr x16, [x16]\n" "br x16\n");
}
#endif
static void hook_aim_shotgun_post(void* gun,float dt) {
    gate_aim_shotgun_post(gun,dt);
    // PreBio4Tick updates recoil and PostBio4Tick updates held/mesh transforms.
    // Apply the presentation after both, even if the hand tick ran earlier.
    if(!__atomic_load_n(&touch_aim_ready,__ATOMIC_ACQUIRE) || !qs_gt() || !gun
        || g_touch_context!=1 || g_touch_weapon!=2 || g_touch_cancel
        || !aim_mobile_size(gun,0x11a0) || !aim_gun_class || !reload_is_a(gun,aim_gun_class()))return;
    void* primary=rl_primary(gun);
    if(!aim_mobile_size(primary,0x51c) || touch_equipped_gun(primary)!=gun)return;
    struct reload_tf reference;
    if(touch_weapon_cycle_pose_reference(primary,gun,&reference))touch_aim_tick(primary);
}

static void install_touch_aim(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[2]; } check[]={
        {0x633d030,{0xf9417800,0xd65f03c0}}, {0x63380dc,{0xd0020168,0xf9431508}},
        {0x6338aec,{0xd0020168,0xf9431508}}, {0x62ac2c8,{0xf81e0ff3,0xa9017bfd}},
        {0x62bbc30,{0xf9413800,0x17ff1a5f}}, {0x628cf08,{0xf81e0ff3,0xa9017bfd}},
        {0x628c410,{0xf9447000,0xb4000240}}, {0x61d0934,{0xb0020ca8,0xf9446508}},
        {0x61d401c,{0xb0020c68,0xf946bd08}}, {0x642e548,{0xd10183ff,0xf90023f3}},
        {0x629a774,{0xf9421408,0xb4000268}}, {0x6341aec,{0xf9416c09,0xad4e8122}},
        {0x6290628,{0xf942a009,0xf9430c01}}, {0x83a926c,{0xd10303ff,0xa9085ff8}},
        {0x62bb648,{0xf941e260,0xb4ffff40}}, {0x633caa8,{0xf9409a88,0xf9406108}},
        {0x6290d8c,{0xd10683ff,0xfd009bea}}, {0x6380de8,{0xad408042,0x3dc00041}},
        {0x83c6310,{0xd10643ff,0xfd0083ee}},
        {0x62bb880,{0xf81e0ff3,0xa9017bfd}}, {0x62bb584,{0xf81e0ff3,0xa9017bfd}},
        {0x628e55c,{0x39710008,0x34000068}}, {0x6420bc0,{0xd10183ff,0xf90023f3}},
        {0x6420c48,{0x5281a804,0x52800205}},
        {0x628e9b4,{0xd10143ff,0xa9034ff4}},
        {0x628e9d4,{0x3963e008,0x35000328}},
        {0x628ea4c,{0x91240260,0xa9447bfd}},
        {0x83a96f4,{0x6dba33ed,0x6d012beb}},
        {0x83a969c,{0xd10103ff,0xa9024ff4}},
        {0x62c9a8c,{0xd10283ff,0xfd0033e8}},
        {0x62c9a94,{0xa90757f6,0xa9084ff4}},
        {0x6213b60,{0x6db63bef,0x6d0133ed}},
        {0x6213b68,{0x6d022beb,0x6d0323e9}},
        {0x633223c,{0xf81d0ff5,0xa9014ff4}},
        {0x6332244,{0xa9027bfd,0x910083fd}},
    };
    if (!base || __atomic_load_n(&touch_aim_ready,__ATOMIC_ACQUIRE)) return;
    for (unsigned i=0;i<sizeof check/sizeof check[0];i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,sizeof check[i].first)) {
            LOGF("AIMALIGN disabled: opcode mismatch +%lx",check[i].addr); return;
        }
    if (body_player!=(void* (*)(const void*))(base+0x633d030)
        || body_active!=(void* (*)(void))(base+0x63380dc) || body_type!=(unsigned (*)(void))(base+0x6338aec)
        || rl_gun1!=(void* (*)(const void*))(base+0x62ac2c8) || rl_primary!=(void* (*)(const void*))(base+0x62bbc30)
        || rl_interactivity!=(int (*)(const void*))(base+0x628cf08) || rl_reloading!=(int (*)(void*))(base+0x628c410)
        || rl_paused!=(int (*)(void))(base+0x61d0934) || rl_player_valid!=(int (*)(void))(base+0x61d401c)
        || rl_pistol_class!=(void* (*)(void))(base+0x642e548)
        || f_GetCurrentGrip!=(void* (*)(const void*))(base+0x629a774)
        || aim_head!=(struct reload_tf (*)(const void*))(base+0x6341aec)
        || aim_sights!=(struct reload_tf (*)(const void*))(base+0x6290628)
        || aim_fire!=(struct aim_fire_tf (*)(const void*))(base+0x6290d8c)
        || rl_world_transform!=(void (*)(void*,const struct reload_tf*,int,void*,int))(base+0x83a926c)) {
        LOGF("AIMALIGN disabled: native dependency mismatch"); return;
    }
    aim_socket=(struct reload_tf (*)(const void*,unsigned long long,int))(base+0x83c6310);
    aim_gun_class=(void* (*)(void))(base+0x6420bc0);
    aim_blending=(int (*)(const void*))(base+0x62bb880);
    aim_flourishing=(int (*)(const void*))(base+0x62bb584);
    aim_recoiling=(int (*)(const void*))(base+0x628e55c);
    aim_default_mesh=(const struct reload_tf* (*)(const void*))(base+0x628e9b4);
    aim_mesh_relative=(struct reload_tf (*)(const void*))(base+0x83a96f4);
    aim_mesh_set_relative=(void (*)(void*,const struct reload_tf*))(base+0x83a969c);
    g_tgt_aim_shotgun_post=base+0x62c9a8c+16;
    if(patch_code((void*)(base+0x62c9a8c),(void*)hook_aim_shotgun_post)){
        LOGF("AIMALIGN disabled: shotgun post-tick hook failed");return;
    }
    g_tgt_aim_bio4_hand=base+0x6213b60+16;
    if(patch_code((void*)(base+0x6213b60),(void*)hook_aim_bio4_hand)){
        LOGF("AIMALIGN disabled: derived Bio4 hand tick hook failed");return;
    }
    g_tgt_aim_try_pause=base+0x633223c+16;
    if(patch_code((void*)(base+0x633223c),(void*)hook_aim_try_pause)){
        LOGF("AIMALIGN disabled: native pause pose hook failed");return;
    }
    inventory_aim_prepare=touch_aim_inventory_prepare;inventory_aim_result=touch_aim_inventory_result;
    __atomic_store_n(&touch_aim_ready,1,__ATOMIC_RELEASE);
    LOGF("AIMALIGN installed: owned native Gun, neutral FireBone +Y, FPS hip/draw/ADS, native shotgun recoil bounded after post-tick");
}
#endif
