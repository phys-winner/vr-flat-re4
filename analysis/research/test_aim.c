#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <math.h>
#include <float.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
typedef struct { float x,y,z,w; } qf_t;
struct __attribute__((aligned(16))) reload_tf { qf_t q; float p[3],s[3],padding[2]; };
_Static_assert(sizeof(struct reload_tf)==48 && _Alignof(struct reload_tf)==16 && offsetof(struct reload_tf,p)==16 && offsetof(struct reload_tf,s)==28 && offsetof(struct reload_tf,padding)==40,"native scalar FTransform ABI");
/* SHARED_MATH */
/* NATIVE_IDENTITY */
/* FIRE_ABI */

static volatile int g_touch_context=1,g_touch_weapon=2,g_touch_aim,g_touch_cancel;
static volatile int g_reload_phase,g_touch_reload_request,g_touch_weapon_request,g_aim_diag_request;
static unsigned char pawn[0x2e0],other_pawn[0x2e0],hand[0x51c],left[0x51c],root_component[0x1f0],gun[0x11a0],shotgun_root[0x1f0];
static unsigned char hand_class[0x60],pawn_class[0x60],gun_class[0x60],gun_base_class[0x60],component_class[0x60],grip[0x28],mesh[0x520],mesh_class[0x60],mesh_table[0x438];
static const void* gun_chain[1];
static void *owner=pawn,*active=pawn,*weapon=gun,*held=&grip,*primary=hand;
static const void* foreign_hand;
static unsigned active_type=1;
static int paused,valid_player=1,pistol=1,registered=1,interactivity=2,reloading,writes,tf_calls,diagnostic_logs,reset_in_getter;
static int post_logs,simulate_setter,interactivity_in_getter=-1;
static int blending,flourishing,recoiling;
static long test_now=10000;
static long ptr_now_ms(void) { return test_now; }
struct touch_quickselect_state {unsigned available;int selected,fire_side,mode;};
static int fixture_fire_side=1,fixture_cycle_busy,fixture_gt=1;
static int fixture_shotgun_reference;
static int qs_gt(void){return fixture_gt;}
static int fixture_equip_busy;static unsigned fixture_equip_generation;
static int touch_quickselect_busy(void){return fixture_equip_busy;}
static unsigned touch_quickselect_pose_generation(void){return fixture_equip_generation;}
static int fixture_motion_busy;
static int touch_motion_pose_busy(void){return fixture_motion_busy;}
static struct touch_quickselect_state touch_quickselect_state(long now){(void)now;return (struct touch_quickselect_state){3,0,fixture_fire_side,1};}
static int touch_weapon_cycle_busy(void){return fixture_cycle_busy;}
static void* touch_weapon_cycle_pose_reference(void* h,void* g,struct reload_tf* reference) {
    assert(h==primary&&g==gun);
    if(!fixture_shotgun_reference)return 0;
    memcpy(reference,shotgun_root+0x1c0,sizeof *reference);return shotgun_root;
}
static int touch_weapon_cycle_primary_transform(void* h,void* g,void* component,
                                               const struct reload_tf* before,const struct reload_tf* after) {
    assert(h==primary&&g==gun&&component==root_component&&before&&after);return 0;
}
static void* touch_equipped_gun(void* h){return primary==h && held ? weapon : 0;}
static struct reload_tf head,sights,last_pose;
static struct reload_tf raw_fire;
static struct reload_tf relative_mesh,post_mesh;
static int mesh_writes,post_calls,post_overwrite,mesh_getter_cancel,mesh_getter_rename;
static struct aim_fire_tf fire;
static void* player(const void* h) { return h==foreign_hand ? other_pawn : owner; }
static void* current(void) { return active; }
static unsigned type(void) { return active_type; }
static void* gun1(const void* p) { (void)p;return weapon; }
static void* primary_hand(const void* g) { (void)g;return primary; }
static void* current_grip(const void* h) { (void)h;return held; }
static void* pistol_class(void) { return gun_class; }
static void* native_gun_class(void) { return gun_base_class; }
static int native_blending(const void* g) { assert(g==gun);return blending; }
static int native_flourishing(const void* g) { assert(g==gun);return flourishing; }
static int native_recoiling(const void* g) { assert(g==gun);return recoiling; }
static int is_paused(void) { return paused; }
static int player_valid(void) { return valid_player; }
static int get_interactivity(const void* g) { (void)g;return interactivity; }
static int is_reloading(void* g) { (void)g;return reloading; }
static int reload_is_pistol(void* g) { (void)g;return pistol; }
static int reload_grip_registered(void* g) { return g==&grip && registered; }
static struct reload_tf head_transform(const void* p) { assert(p==pawn);tf_calls++;if(reset_in_getter)g_touch_cancel=1;if(interactivity_in_getter>=0)interactivity=interactivity_in_getter;return head; }
static struct reload_tf sight_transform(const void* g) { assert(g==gun);tf_calls++;return sights; }
static struct aim_fire_tf fire_transform(const void* g) { assert(g==gun);tf_calls++;return fire; }
static struct reload_tf socket_transform(const void* m,unsigned long long name,int space) { assert(m==mesh && name==0x123400005678ULL && space==0);tf_calls++;return raw_fire; }
static struct reload_tf fixture_compose(struct reload_tf parent,struct reload_tf local) {
    float offset[3];q_rot(parent.q,local.p,offset);
    for(int i=0;i<3;i++)local.p[i]=parent.p[i]+offset[i];
    local.q=q_mul(parent.q,local.q);return local;
}
static const struct reload_tf* default_mesh_transform(const void* g) {
    assert(g==gun&&gun[0x8f8]==1);return (const struct reload_tf*)(gun+0x900);
}
static struct reload_tf mesh_relative_transform(const void* m) {
    assert(m==mesh);
    if(mesh_getter_cancel)g_touch_cancel=1;
    if(mesh_getter_rename)mesh[0x1c]^=1; // high half of the complete native FName
    return relative_mesh;
}
static void fixture_mesh_transform(struct reload_tf pose) {
    struct reload_tf actor;memcpy(&actor,shotgun_root+0x1c0,sizeof actor);
    struct reload_tf before=fixture_compose(actor,relative_mesh),after=fixture_compose(actor,pose);
    qf_t delta=q_mul(after.q,(qf_t){-before.q.x,-before.q.y,-before.q.z,before.q.w});
    struct reload_tf* attached[]={&sights,&fire.tf,&raw_fire};
    for(int i=0;i<3;i++) {
        float relative[3],offset[3];for(int j=0;j<3;j++)relative[j]=attached[i]->p[j]-before.p[j];
        q_rot(delta,relative,offset);
        for(int j=0;j<3;j++)attached[i]->p[j]=after.p[j]+offset[j];
        attached[i]->q=q_mul(delta,attached[i]->q);
    }
    relative_mesh=pose;
}
static void mesh_set_relative(void* m,const struct reload_tf* pose) {
    assert(m==mesh&&pose&&reload_tf_valid(pose));mesh_writes++;fixture_mesh_transform(*pose);
}
#if !defined(__aarch64__)
static void gate_aim_shotgun_post(void* g,float dt) {
    assert(g==gun&&fabsf(dt-.016f)<.00001f);post_calls++;
    if(post_overwrite)fixture_mesh_transform(post_mesh);
}
#endif
static int patch_code(void* target,void* hook) {(void)target;(void)hook;assert(0);return -1;}
#if !defined(__aarch64__)
static void gate_aim_bio4_hand(void* hand,float dt);
static int gate_aim_try_pause(unsigned short start);
#endif
static void* inventory_left(const void* p){assert(p==pawn);return fixture_fire_side==0?hand:left;}
static void* inventory_right(const void* p){assert(p==pawn);return fixture_fire_side==1?hand:left;}
static void*(*rl_left)(const void*)=inventory_left,*(*rl_right)(const void*)=inventory_right;
static void(*inventory_aim_prepare)(void*);
static void(*inventory_aim_result)(int);
static void world_transform(void* component,const struct reload_tf* pose,int sweep,void* hit,int teleport) {
    assert(component==root_component && !sweep && !hit && !teleport);writes++;last_pose=*pose;
    if(simulate_setter) {
        struct reload_tf old;memcpy(&old,root_component+0x1c0,sizeof old);
        qf_t delta=q_mul(pose->q,(qf_t){-old.q.x,-old.q.y,-old.q.z,old.q.w});
        struct reload_tf* attached[]={&sights,&fire.tf,&raw_fire,(struct reload_tf*)(shotgun_root+0x1c0)};
        for(int i=0;i<3+fixture_shotgun_reference;i++) {
            float local[3]={attached[i]->p[0]-old.p[0],attached[i]->p[1]-old.p[1],attached[i]->p[2]-old.p[2]},offset[3];
            q_rot(delta,local,offset);
            for(int j=0;j<3;j++)attached[i]->p[j]=pose->p[j]+offset[j];
            attached[i]->q=q_mul(delta,attached[i]->q);
        }
        memcpy(root_component+0x1c0,pose,sizeof *pose);
    }
}
static void* (*body_player)(const void*)=player;
static void* (*body_active)(void)=current;
static unsigned (*body_type)(void)=type;
static void* (*rl_gun1)(const void*)=gun1;
static void* (*rl_primary)(const void*)=primary_hand;
static void* (*rl_pistol_class)(void)=pistol_class;
static void* (*f_GetCurrentGrip)(const void*)=current_grip;
static int (*rl_paused)(void)=is_paused;
static int (*rl_player_valid)(void)=player_valid;
static int (*rl_interactivity)(const void*)=get_interactivity;
static int (*rl_reloading)(void*)=is_reloading;
static struct reload_tf (*aim_head)(const void*)=head_transform;
static struct reload_tf (*aim_sights)(const void*)=sight_transform;
static struct aim_fire_tf (*aim_fire)(const void*)=fire_transform;
static void (*rl_world_transform)(void*,const struct reload_tf*,int,void*,int)=world_transform;
static float g_gun_ads[3];
static void probe_log(const char* fmt,...) {
    if(strncmp(fmt,"AIMALIGNPOST ",13)){diagnostic_logs++;return;}
    post_logs++;va_list ap;va_start(ap,fmt);assert(va_arg(ap,int)==1);
    double command[3],cached[3];
    for(int i=0;i<3;i++)command[i]=va_arg(ap,double);
    for(int i=0;i<3;i++)cached[i]=va_arg(ap,double);
    for(int i=0;i<3;i++)assert(fabs(command[i]-cached[i])<0.001);
    assert(fabs(va_arg(ap,double)-1)<0.0001);
    double sight_distance=0;
    for(int i=0;i<3;i++){double offset=va_arg(ap,double)-g_gun_ads[i];sight_distance+=offset*offset;}
    assert(fabs(sight_distance-209)<.01);   // authored roll may rotate the rigid 12/1/8 cm sight offset
    assert(fabs(va_arg(ap,double)-1)<0.0001);
    for(int i=0;i<2;i++)assert(fabs(va_arg(ap,double))<0.0001);
    for(int i=0;i<3;i++)assert(fabs(va_arg(ap,double)-g_gun_ads[i])<0.001);
    for(int i=0;i<3;i++)assert(va_arg(ap,int)==1);
    va_end(ap);
}
#define LOGF(...) probe_log(__VA_ARGS__)
// p303 look latch (re4fix.c): the fixture camera is already this frame's view, so the one-frame delta is identity.
static qf_t pose_camera_delta_ue(void){qf_t identity={0,0,0,1};return identity;}
static volatile int g_pose_trace;
static float g_pose_latch_yaw,g_pose_latch_pitch,g_pose_prev_yaw,g_pose_prev_pitch;
static unsigned long long fixture_frame,g_pose_latch_frame;
static const unsigned long long* g_pose_frame_counter=&fixture_frame;
static int g_pose_latch_lock;
static int pthread_mutex_lock(int* lock){assert(!*lock);*lock=1;return 0;}
static int pthread_mutex_unlock(int* lock){assert(*lock);*lock=0;return 0;}
static float g_fov_h_deg=90;
static int fixture_world_fov=90;
static int fixture_weapon_fov=90;
static int mobile_prefs_fov(void){return fixture_world_fov;}
static int mobile_prefs_weapon_fov(void){return fixture_weapon_fov;}
#include "AIM_HEADER"
static int derived_overwrite,derived_calls;
static int pause_accepted=1,pause_calls;
static unsigned short pause_start;
#if !defined(__aarch64__)
static int gate_aim_try_pause(unsigned short start) {
    pause_start=start;pause_calls++;
    if(pause_accepted){paused=1;g_touch_context=2;g_touch_cancel=3;}
    return pause_accepted;
}
static void gate_aim_bio4_hand(void* h,float dt) {
    assert(h==hand&&fabsf(dt-.016f)<.0001f);derived_calls++;
    if(derived_overwrite) {
        touch_aim_tick(h);
        struct reload_tf raised;memcpy(&raised,root_component+0x1c0,sizeof raised);
        for(int k=0;k<3;k++)raised.p[k]+=head.p[k]+g_gun_ads[k]-fire.tf.p[k];
        world_transform(root_component,&raised,0,0,0);
    }
}
#endif

static void pointer_at(unsigned char* object,int offset,void* value) { memcpy(object+offset,&value,sizeof value); }
static void class_size(unsigned char* cls,int size) { memcpy(cls+0x58,&size,sizeof size); }
static void compare_quat(qf_t a,qf_t b) {
    float dot=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
    assert(fabsf(fabsf(dot)-1.0f)<0.0001f);
}
static void check_fire_and_sight(struct reload_tf pose,qf_t local_fire,const float local_sight[3],qf_t head_q,const float target[3]) {
    float actual[3],wanted[3],offset[3];q_rot(pose.q,local_sight,offset);
    for(int j=0;j<3;j++)assert(fabsf(pose.p[j]+offset[j]-target[j])<0.001f);
    q_rot(q_mul(pose.q,local_fire),(float[3]){0,1,0},actual);
    q_rot(head_q,(float[3]){1,0,0},wanted);
    for(int j=0;j<3;j++)assert(fabsf(actual[j]-wanted[j])<0.0001f);
}
static void check_minimal(qf_t old_hand,qf_t new_hand,qf_t fire_q,qf_t head_q) {
    qf_t delta=q_mul(new_hand,(qf_t){-old_hand.x,-old_hand.y,-old_hand.z,old_hand.w});
    float from[3],to[3];q_rot(fire_q,(float[3]){0,1,0},from);q_rot(head_q,(float[3]){1,0,0},to);
    float dot=from[0]*to[0]+from[1]*to[1]+from[2]*to[2];
    float expected=acosf(fmaxf(-1,fminf(1,dot))),actual=2*acosf(fmaxf(-1,fminf(1,fabsf(delta.w))));
    assert(fabsf(actual-expected)<0.0003f);
    assert(fabsf(delta.x*from[0]+delta.y*from[1]+delta.z*from[2])<0.0001f);
}
int main(void) {
    assert(fabsf(weapon_fov_depth(46,90,90)-46)<.0001f);
    assert(weapon_fov_depth(46,90,60)<46 && weapon_fov_depth(46,90,110)>46);
    assert(weapon_fov_depth(46,NAN,90)==46 && weapon_fov_depth(46,90,111)==46);
    struct weapon_sway_state sw={0};float shift[3],roll;
    weapon_sway_step(&sw,1000,3.13f,0,1,shift,&roll);assert(!shift[1]&&!roll);
    weapon_sway_step(&sw,1016,-3.13f,.02f,1,shift,&roll);assert(shift[1]<0&&shift[2]<0);
    for(int i=0;i<200;i++)weapon_sway_step(&sw,1032+16*i,-3.13f,.02f,1,shift,&roll);
    assert(fabsf(shift[1])<.0001f&&fabsf(shift[2])<.0001f&&fabsf(roll)<.0001f);
    for(int i=0;i<300;i++){
        weapon_sway_step(&sw,5000+16*i,.1f*i,.05f*i,1,shift,&roll);
        assert(fabsf(shift[1])<=1.2f&&fabsf(shift[2])<=.8f&&fabsf(roll)<=1.08f);
        float half=roll*.00872664626f,forward[3];q_rot((qf_t){sinf(half),0,0,cosf(half)},(float[3]){1,0,0},forward);
        assert(fabsf(forward[0]-1)<.0001f&&fabsf(forward[1])<.0001f&&fabsf(forward[2])<.0001f);
    }
    weapon_sway_step(&sw,10000,1,1,0,shift,&roll);assert(!sw.time&&!shift[1]&&!roll);
    struct weapon_bob_state bob={0};float bob_out[3];weapon_bob_step(&bob,1000,180,1,bob_out);
    float max_bob=0;for(int i=1;i<=120;i++){weapon_bob_step(&bob,1000+16*i,180,1,bob_out);max_bob=fmaxf(max_bob,fabsf(bob_out[2]));}
    assert(max_bob>.4f&&max_bob<=.45f);
    // Actual full-stick travel is about 420 cm/s: the 400 cap gives a vertical bob every 0.65 s.
    struct weapon_bob_state slow={0},fast={0},capped={0};float slow_out[3],fast_out[3],capped_out[3];
    weapon_bob_step(&slow,1000,420,1,slow_out);weapon_bob_step(&fast,1000,420,1,fast_out);
    weapon_bob_step(&capped,1000,400,1,capped_out);
    for(int i=1;i<=50;i++) {
        weapon_bob_step(&slow,1000+20*i,420,1,slow_out);
        weapon_bob_step(&capped,1000+20*i,400,1,capped_out);
    }
    for(int i=1;i<=100;i++)weapon_bob_step(&fast,1000+10*i,420,1,fast_out);
    assert(fabsf(slow.phase-remainderf(4.8f,6.28318530718f))<.0001f);
    assert(slow.phase==capped.phase&&!memcmp(slow_out,capped_out,sizeof slow_out));
    assert(fabsf(slow.phase-fast.phase)<.0001f&&fabsf(slow_out[2]-fast_out[2])<.0001f);
    const int bob_dt[]={10,20,33};
    for(unsigned i=0;i<sizeof bob_dt/sizeof *bob_dt;i++) {
        struct weapon_bob_state walking={0};float out[3],previous=0;int first=0,last=0,crossings=0;
        weapon_bob_step(&walking,1000,420,1,out);
        for(int elapsed=bob_dt[i];elapsed<=4000;elapsed+=bob_dt[i]) {
            weapon_bob_step(&walking,1000+elapsed,420,1,out);
            if(elapsed>100&&previous<=0&&out[2]>0) {if(!first)first=elapsed;last=elapsed;++crossings;}
            previous=out[2];
        }
        assert(crossings>=5);
        float period=(float)(last-first)/(crossings-1)/1000.0f;
        assert(period>.64f&&period<.67f);
    }
    puts("PASS: actual full-stick 420 cm/s clamps to 400, vertical bob period 0.64..0.67 s at 10/20/33 ms dt");
    for(int i=1;i<=180;i++)weapon_bob_step(&bob,2920+16*i,0,1,bob_out);
    assert(fabsf(bob_out[1])<.0001f&&fabsf(bob_out[2])<.0001f);
    weapon_bob_step(&bob,6000,180,0,bob_out);assert(!bob.time&&!bob_out[1]&&!bob_out[2]);
    struct reload_tf identity;memcpy(&identity,native_identity,sizeof identity);
    assert(identity.q.w==1 && identity.p[0]==0 && identity.p[1]==0 && identity.p[2]==0);
    assert(identity.s[0]==1 && identity.s[1]==1 && identity.s[2]==1 && reload_tf_valid(&identity));
    qf_t marker={0,sinf(0.3f),0,cosf(0.3f)},local_fire={sinf(0.2f),0,0,cosf(0.2f)};float local[3]={8,3,4},fire_local[3]={20,-2,8};
    for(int i=0;i<100;i++) {
        float a=(float)i*0.013f,b=(float)i*0.029f;
        struct reload_tf hand_pose={{sinf(a),0,0,cosf(a)},{25,2,140},{1,1,1},{7,8}};
        struct reload_tf camera={q_mul((qf_t){0,0,sinf(b),cosf(b)},(qf_t){0,sinf(a),0,cosf(a)}),{20,-30,150},{1,1,1},{0,0}};
        struct reload_tf sight=hand_pose,fire_pose=hand_pose,pose;float offset[3],target[3],forward[3]={g_gun_ads[0],g_gun_ads[1],g_gun_ads[2]};
        /* Expectations follow the tunable phone framing (g_gun_hip/g_gun_ads), not historical constants. */
        float half[3]={(g_gun_hip[0]+g_gun_ads[0])*.5f,(g_gun_hip[1]+g_gun_ads[1])*.5f,(g_gun_hip[2]+g_gun_ads[2])*.5f};
        sight.q=q_mul(hand_pose.q,marker);q_rot(hand_pose.q,local,offset);
        fire_pose.q=q_mul(hand_pose.q,local_fire);
        for(int j=0;j<3;j++)sight.p[j]=hand_pose.p[j]+offset[j];
        q_rot(hand_pose.q,fire_local,offset);for(int j=0;j<3;j++)fire_pose.p[j]=hand_pose.p[j]+offset[j];
        assert(aim_mobile_pose(hand_pose,sight,fire_pose,camera,0,&pose)==1);
        q_rot(camera.q,g_gun_hip,offset);for(int j=0;j<3;j++)target[j]=camera.p[j]+offset[j];
        check_fire_and_sight(pose,local_fire,fire_local,camera.q,target);
        check_minimal(hand_pose.q,pose.q,fire_pose.q,camera.q);
        assert(aim_mobile_pose(hand_pose,sight,fire_pose,camera,.5f,&pose)==1);
        q_rot(camera.q,half,offset);for(int j=0;j<3;j++)target[j]=camera.p[j]+offset[j];
        check_fire_and_sight(pose,local_fire,fire_local,camera.q,target);
        assert(!memcmp(pose.s,hand_pose.s,sizeof pose.s) && !memcmp(pose.padding,hand_pose.padding,sizeof pose.padding));
        q_rot(camera.q,forward,offset);for(int j=0;j<3;j++)target[j]=camera.p[j]+offset[j];
        assert(aim_mobile_pose(hand_pose,sight,fire_pose,camera,1,&pose)==1);
        check_fire_and_sight(pose,local_fire,fire_local,camera.q,target);
        float barrel[3],muzzle_local[3];q_rot(local_fire,(float[3]){0,20,0},barrel);
        for(int j=0;j<3;j++)muzzle_local[j]=fire_local[j]+barrel[j];
        q_rot(camera.q,(float[3]){g_gun_ads[0]+20,g_gun_ads[1],g_gun_ads[2]},offset);for(int j=0;j<3;j++)target[j]=camera.p[j]+offset[j];
        check_fire_and_sight(pose,local_fire,muzzle_local,camera.q,target);
        check_minimal(hand_pose.q,pose.q,fire_pose.q,camera.q);
    }
    struct reload_tf base={{0,0,0,1},{0,0,0},{1,1,1},{0,0}},bad=base,output=base;
    bad.q.w=NAN;assert(aim_mobile_pose(base,base,base,bad,1,&output)==-1);
    bad=base;bad.s[0]=2;assert(aim_mobile_pose(bad,base,base,base,1,&output)==-2);
    bad=base;bad.q.w=0;assert(aim_mobile_pose(base,bad,base,base,1,&output)==-3);
    assert(aim_mobile_pose(base,base,bad,base,1,&output)==-6);
    bad=base;bad.p[0]=FLT_MAX;struct reload_tf huge_sight=base;huge_sight.p[0]=-FLT_MAX;
    assert(aim_mobile_pose(bad,base,huge_sight,base,0,&output)==-4);
    bad=base;bad.q=(qf_t){0,0,0.70710678f,0.70710678f};assert(aim_mobile_pose(base,base,bad,base,1,&output)==-7);
    bad=base;bad.p[0]=10000;assert(aim_mobile_pose(base,base,base,bad,1,&output)==-5);
    assert(aim_mobile_pose(base,base,base,base,NAN,&output)==-8);
    assert(aim_mobile_pose(base,base,base,base,-.01f,&output)==-8);
    assert(aim_mobile_pose(base,base,base,base,1.01f,&output)==-8);
    assert(!memcmp(&output,&base,sizeof output));
    assert(aim_fire_position_raw(base,base));bad=base;bad.p[2]=.04f;assert(aim_fire_position_raw(base,bad));
    bad.p[2]=.051f;assert(!aim_fire_position_raw(base,bad));bad=base;bad.s[2]=0;assert(!aim_fire_position_raw(bad,base));
    struct reload_tf parallel_fire=base,rolled_hand=base;
    parallel_fire.q=(qf_t){0,0,-0.70710678f,0.70710678f};
    rolled_hand.q=(qf_t){sinf(.7f),0,0,cosf(.7f)};
    assert(aim_mobile_pose(rolled_hand,base,parallel_fire,base,0,&output)==1);
    compare_quat(output.q,rolled_hand.q);
    for(int i=0;i<100;i++) {
        float angle=(float)i*.071f;struct reload_tf rolled_fire=parallel_fire,rolled_head=base,pose;
        rolled_fire.q=q_mul(parallel_fire.q,(qf_t){0,sinf(angle),0,cosf(angle)});
        rolled_head.q=q_mul(base.q,(qf_t){sinf(angle),0,0,cosf(angle)});
        assert(aim_mobile_pose(rolled_hand,base,rolled_fire,rolled_head,0,&pose)==1);
        compare_quat(pose.q,rolled_hand.q);
        assert(reload_tf_valid(&pose));
    }

    assert(aim_ease(100,100,180)==0 && aim_ease(99,100,180)==0 && aim_ease(280,100,180)==1 && aim_ease(500,100,180)==1);
    assert(fabsf(aim_ease(190,100,180)-.5f)<.00001f);
    class_size(hand_class,0x51c);class_size(pawn_class,0x2e0);class_size(gun_class,0xd40);class_size(component_class,0x1f0);
    gun_chain[0]=gun_base_class+0x30;pointer_at(gun_class,0x30,gun_chain);
    pointer_at(hand,0x10,hand_class);pointer_at(left,0x10,hand_class);pointer_at(hand,0x130,root_component);hand[0x314]=1;
    pointer_at(pawn,0x10,pawn_class);pointer_at(pawn,0x2d8,root_component);pointer_at(root_component,0x10,component_class);
    pointer_at(gun,0x10,gun_class);pointer_at(gun,0x270,&grip);pointer_at(gun,0x3c0,pawn);pointer_at(gun,0x540,root_component);
    pointer_at(grip,0x20,gun);
    class_size(mesh_class,0x520);pointer_at(mesh,0x10,mesh_class);pointer_at(mesh,0,mesh_table);pointer_at(gun,0x540,mesh);pointer_at(mesh,0x20,gun);
    aim_socket=socket_transform;memcpy(mesh_table+0x430,&aim_socket,sizeof aim_socket);unsigned long long fname=0x123400005678ULL;memcpy(gun+0x620,&fname,sizeof fname);
    aim_gun_class=native_gun_class;aim_blending=native_blending;aim_flourishing=native_flourishing;aim_recoiling=native_recoiling;
    head=base;head.p[2]=150;sights=base;sights.p[0]=25;sights.p[2]=140;fire.tf=base;fire.tf.p[0]=37;fire.tf.p[1]=1;fire.tf.p[2]=148;raw_fire=fire.tf;fire.scale_type=1;
    struct reload_tf hand_pose=base;hand_pose.p[0]=17;hand_pose.p[2]=136;memcpy(root_component+0x1c0,&hand_pose,sizeof hand_pose);
    install_touch_aim(0);touch_aim_tick(hand);assert(!writes && !tf_calls);
    __atomic_store_n(&touch_aim_ready,1,__ATOMIC_RELEASE);
    fixture_gt=0;touch_aim_tick(hand);assert(!writes && !tf_calls);fixture_gt=1;
    touch_aim_tick(0);touch_aim_tick(left);assert(!writes);
    volatile int* blockers[]={&g_touch_cancel,&g_reload_phase,&g_touch_reload_request,&g_touch_weapon_request};
    for(unsigned i=0;i<sizeof blockers/sizeof blockers[0];i++){*blockers[i]=1;touch_aim_tick(hand);*blockers[i]=0;assert(!writes);}
    g_touch_context=5;touch_aim_tick(hand);g_touch_context=1;g_touch_weapon=0;touch_aim_tick(hand);g_touch_weapon=2;
    active_type=5;touch_aim_tick(hand);active_type=1;owner=0;touch_aim_tick(hand);owner=pawn;active=other_pawn;touch_aim_tick(hand);active=pawn;
    paused=1;touch_aim_tick(hand);paused=0;valid_player=0;touch_aim_tick(hand);valid_player=1;
    weapon=0;touch_aim_tick(hand);weapon=gun;pointer_at(gun,0x3c0,other_pawn);touch_aim_tick(hand);pointer_at(gun,0x3c0,pawn);
    gun_chain[0]=0;touch_aim_tick(hand);gun_chain[0]=gun_base_class+0x30;registered=0;touch_aim_tick(hand);registered=1;held=0;touch_aim_tick(hand);held=&grip;
    primary=left;touch_aim_tick(hand);primary=hand;interactivity=0;touch_aim_tick(hand);interactivity=2;reloading=1;touch_aim_tick(hand);reloading=0;
    pointer_at(grip,0x20,0);touch_aim_tick(hand);pointer_at(grip,0x20,gun);
    pointer_at(gun,0x270,0);touch_aim_tick(hand);pointer_at(gun,0x270,&grip);assert(!writes && !tf_calls);
    unsigned char* classes[]={hand_class,pawn_class,gun_class,component_class};int minimum[]={0x51c,0x2e0,0xd40,0x1f0};
    for(unsigned i=0;i<4;i++){class_size(classes[i],minimum[i]-1);touch_aim_tick(hand);class_size(classes[i],0x10001);touch_aim_tick(hand);class_size(classes[i],minimum[i]);assert(!writes);}
    pointer_at(hand,0x130,0);touch_aim_tick(hand);pointer_at(hand,0x130,root_component);
    pointer_at(pawn,0x2d8,0);touch_aim_tick(hand);pointer_at(pawn,0x2d8,root_component);
    pointer_at(gun,0x540,0);touch_aim_tick(hand);pointer_at(gun,0x540,mesh);assert(!writes);
    reset_in_getter=1;touch_aim_tick(hand);reset_in_getter=0;g_touch_cancel=0;assert(!writes);
    unsigned char before_p[sizeof pawn],before_h[sizeof hand],before_g[sizeof gun],before_c[sizeof root_component];
    memcpy(before_p,pawn,sizeof pawn);memcpy(before_h,hand,sizeof hand);memcpy(before_g,gun,sizeof gun);memcpy(before_c,root_component,sizeof root_component);
    int* animations[]={&blending,&flourishing,&recoiling};
    for(int i=0;i<3;i++) { *animations[i]=1;touch_aim_tick(hand);*animations[i]=0;assert(!writes && !aim_fps.gun); }
    fire.tf.p[2]+=.1f;touch_aim_tick(hand);fire.tf.p[2]-=.1f;assert(!writes && !aim_fps.gun);
    g_touch_aim=0;touch_aim_tick(hand);assert(writes==1 && aim_fps.gun==gun);
    assert(reload_tf_valid(&last_pose));   // first draw frame starts below the view, with the muzzle pitched down
    assert(!memcmp(before_p,pawn,sizeof pawn) && !memcmp(before_h,hand,sizeof hand)
        && !memcmp(before_g,gun,sizeof gun) && !memcmp(before_c,root_component,sizeof root_component));
    simulate_setter=1;test_now+=g_gun_draw_ms/2;touch_aim_tick(hand);assert(writes==2);
    test_now+=g_gun_draw_ms-g_gun_draw_ms/2;touch_aim_tick(hand);assert(writes==3);
    float relative[3]={fire.tf.p[0]-head.p[0],fire.tf.p[1]-head.p[1],fire.tf.p[2]-head.p[2]};
    for(int i=0;i<3;i++)assert(fabsf(relative[i]-g_gun_hip[i])<.001);
    g_touch_aim=1;touch_aim_tick(hand);assert(writes==4 && aim_fps.blend==0);
    test_now+=90;touch_aim_tick(hand);assert(writes==5 && fabsf(aim_fps.blend-.5f)<.00001f);
    g_touch_aim=0;touch_aim_tick(hand);assert(writes==6 && fabsf(aim_fps.blend-.5f)<.00001f);
    test_now+=90;touch_aim_tick(hand);assert(writes==7 && fabsf(aim_fps.blend-.25f)<.00001f);
    g_touch_aim=1;touch_aim_tick(hand);test_now+=180;touch_aim_tick(hand);assert(writes==9 && aim_fps.blend==1);
    g_aim_diag_request=1;int old_tf_calls=tf_calls;
    touch_aim_tick(hand);assert(writes==10 && post_logs==1);struct reload_tf stable=last_pose;
    for(int i=0;i<11;i++)touch_aim_tick(hand);
    assert(writes==21 && post_logs==8 && tf_calls-old_tf_calls==12*4+8*2);compare_quat(stable.q,last_pose.q);
    for(int j=0;j<3;j++)assert(fabsf(stable.p[j]-last_pose.p[j])<0.001f);
    old_tf_calls=tf_calls;g_aim_diag_request=0;touch_aim_tick(hand);assert(writes==22 && post_logs==8 && tf_calls-old_tf_calls==4);
    struct reload_tf idle=fire.tf;
    qf_t recoil={0,sinf(.08f),0,cosf(.08f)};
    fire.tf.q=q_mul(recoil,idle.q);fire.tf.p[2]+=2;raw_fire=fire.tf;recoiling=1;
    touch_aim_tick(hand);assert(writes==23);compare_quat(stable.q,last_pose.q);
    assert(fabsf(fire.tf.p[2]-idle.p[2]-2)<.001f);compare_quat(fire.tf.q,q_mul(recoil,idle.q));
    recoiling=0;fire.tf=idle;raw_fire=idle;
    // Getter packets stay immutable; attached-scene convergence is exercised separately above.
    fire.tf.p[2]+=.1f;struct aim_fire_tf assisted=fire;simulate_setter=0;touch_aim_tick(hand);assert(writes==24);
    compare_quat(stable.q,last_pose.q);assert(!memcmp(&fire,&assisted,sizeof fire));fire.tf.p[2]-=.1f;simulate_setter=1;
    raw_fire.s[2]=0;touch_aim_tick(hand);raw_fire.s[2]=1;assert(writes==24);
    fire.tf.q.w=0;touch_aim_tick(hand);fire.tf=idle;assert(writes==24);
    class_size(mesh_class,0x51f);touch_aim_tick(hand);class_size(mesh_class,0x520);assert(writes==24);
    memset(mesh_table+0x430,0,sizeof aim_socket);touch_aim_tick(hand);memcpy(mesh_table+0x430,&aim_socket,sizeof aim_socket);assert(writes==24);
    // Full native names, reload, menu and long discontinuities cannot reuse a stale grip reference.
    unsigned long long new_name=7;memcpy(gun+0x18,&new_name,8);blending=1;touch_aim_tick(hand);assert(writes==24 && !aim_fps.gun);
    blending=0;touch_aim_tick(hand);assert(writes==25 && aim_fps.gun_name==7 && aim_fps.blend==0);
    g_reload_phase=7;test_now+=100;touch_aim_tick(hand);assert(writes==25 && aim_fps.blend==0);g_reload_phase=0;
    g_touch_context=2;touch_aim_tick(hand);assert(writes==25 && !aim_fps.gun);g_touch_context=1;
    pistol=0;touch_aim_tick(hand);assert(writes==26 && aim_fps.gun==gun);pistol=1;
    test_now+=5001;recoiling=1;touch_aim_tick(hand);assert(writes==26 && !aim_fps.gun);recoiling=0;
    head.q.w=0;for(int i=0;i<20;i++)touch_aim_tick(hand);assert(writes==26 && diagnostic_logs>=8);
    // Actual native left primary uses the same shot axis, with a mirrored hip and centred ADS.
    memset(&aim_fps,0,sizeof aim_fps);head=base;head.p[2]=150;hand[0x314]=0;fixture_fire_side=0;
    hand_pose=base;hand_pose.p[0]=17;hand_pose.p[2]=136;memcpy(root_component+0x1c0,&hand_pose,sizeof hand_pose);
    fire.tf=base;fire.tf.p[0]=37;fire.tf.p[1]=1;fire.tf.p[2]=148;raw_fire=fire.tf;g_touch_aim=0;
    touch_aim_tick(hand);test_now+=g_gun_draw_ms;touch_aim_tick(hand);
    assert(fabsf(fire.tf.p[0]-head.p[0]-g_gun_hip[0])<.001 && fabsf(fire.tf.p[1]-head.p[1]+g_gun_hip[1])<.001 && fabsf(fire.tf.p[2]-head.p[2]-g_gun_hip[2])<.001);
    int count=writes;fixture_cycle_busy=1;touch_aim_tick(hand);assert(writes==count+1);fixture_cycle_busy=0;
    g_touch_aim=1;touch_aim_tick(hand);test_now+=180;touch_aim_tick(hand);
    for(int i=0;i<3;i++)assert(fabsf(fire.tf.p[i]-head.p[i]-g_gun_ads[i])<.001);
    fixture_fire_side=1;count=writes;touch_aim_tick(hand);assert(writes==count);
    fixture_fire_side=0;fixture_motion_busy=1;touch_aim_tick(hand);assert(writes==count && !aim_fps.gun);fixture_motion_busy=0;
    touch_aim_tick(hand);count=writes;fixture_equip_busy=1;touch_aim_tick(hand);assert(writes==count && !aim_fps.gun);fixture_equip_busy=0;
    touch_aim_tick(hand);count=writes;fixture_equip_generation++;recoiling=1;touch_aim_tick(hand);assert(writes==count && !aim_fps.gun);
    recoiling=0;touch_aim_tick(hand);assert(writes==count+1 && aim_fps.equip_generation==fixture_equip_generation);
    // Magazine insertion/cocking retain the cached FPS pose and ease toward the
    // off-hand, then return without replaying the draw or snapping the wrist.
    test_now+=g_gun_draw_ms;touch_aim_tick(hand);long draw_started=aim_fps.drawn_at;
    g_reload_phase=1;count=writes;touch_aim_tick(hand);assert(writes==count+1 && !aim_fps.reload_level);
    test_now+=g_gun_reload_ms[0]/2;touch_aim_tick(hand);assert(fabsf(aim_fps.reload_level-.5f)<.0001f);
    test_now+=g_gun_reload_ms[0]-g_gun_reload_ms[0]/2;touch_aim_tick(hand);assert(aim_fps.reload_level==1);
    g_reload_phase=5;touch_aim_tick(hand);assert(aim_fps.reload_level==1 && aim_fps.drawn_at==draw_started);
    g_reload_phase=0;touch_aim_tick(hand);test_now+=g_gun_reload_ms[1]/2;touch_aim_tick(hand);
    assert(fabsf(aim_fps.reload_level-.5f)<.0001f && aim_fps.drawn_at==draw_started && reload_tf_valid(&last_pose));
    test_now+=g_gun_reload_ms[1]-g_gun_reload_ms[1]/2;touch_aim_tick(hand);assert(!aim_fps.reload_level);
    g_touch_aim=0;touch_aim_tick(hand);test_now+=180;touch_aim_tick(hand);
    const int weapon_fovs[]={60,90,110};
    for(int i=0;i<3;i++){
        fixture_weapon_fov=weapon_fovs[i];test_now+=16;touch_aim_tick(hand);
        assert(fabsf(fire.tf.p[0]-head.p[0]-weapon_fov_depth(g_gun_hip[0],g_fov_h_deg,(float)fixture_weapon_fov))<.001f);
        float actual[3];q_rot(fire.tf.q,(float[3]){0,1,0},actual);
        assert(fabsf(actual[0]-1)<.0001f&&fabsf(actual[1])<.0001f&&fabsf(actual[2])<.0001f);
    }
    g_touch_aim=1;touch_aim_tick(hand);test_now+=180;touch_aim_tick(hand);
    for(int i=0;i<3;i++)assert(fabsf(fire.tf.p[i]-head.p[i]-g_gun_ads[i])<.001f);
    g_touch_aim=0;touch_aim_tick(hand);test_now+=180;touch_aim_tick(hand);fixture_weapon_fov=90;
    for(int i=1;i<=32;i++){
        float angle=.035f*i;head.q=(qf_t){0,0,sinf(angle/2),cosf(angle/2)};test_now+=16;touch_aim_tick(hand);
        float actual[3],expected[3];q_rot(fire.tf.q,(float[3]){0,1,0},actual);q_rot(head.q,(float[3]){1,0,0},expected);
        for(int k=0;k<3;k++)assert(fabsf(actual[k]-expected[k])<.0001f);
    }
    // Inventory leaves pooled hands ticking alongside the active hand. Their
    // owner is another pawn: they must never restart the active gun's draw.
    unsigned char pooled_hand[sizeof hand],cached_pose[sizeof aim_fps];
    memcpy(pooled_hand,hand,sizeof pooled_hand);foreign_hand=pooled_hand;
    long stable_drawn_at=aim_fps.drawn_at;
    for(int i=0;i<120;i++){
        memcpy(cached_pose,&aim_fps,sizeof aim_fps);count=writes;
        touch_aim_tick(pooled_hand);
        assert(writes==count&&!memcmp(cached_pose,&aim_fps,sizeof aim_fps));
        owner=0;touch_aim_tick(hand);owner=pawn;
        assert(writes==count&&!memcmp(cached_pose,&aim_fps,sizeof aim_fps));
        test_now+=16;touch_aim_tick(hand);
        assert(writes==count+1&&aim_fps.gun==gun&&aim_fps.drawn_at==stable_drawn_at);
    }
    foreign_hand=0;
    paused=1;touch_aim_tick(hand);assert(!aim_fps.gun);paused=0;
    // Original PrimaryGripPreGrabTick calls UpdateTwoHandedGrip, which changes
    // gun ActorRotation independently of its primary hand. Exercise that
    // changing native gun-to-hand relationship, not an immutable attached gun.
    struct reload_tf bone=base;bone.q=(qf_t){0,0,-.70710678f,.70710678f};
    bone.p[0]=30;bone.p[1]=2;bone.p[2]=3;
    fixture_shotgun_reference=1;fixture_weapon_fov=90;
    aim_default_mesh=default_mesh_transform;aim_mesh_relative=mesh_relative_transform;aim_mesh_set_relative=mesh_set_relative;
    gun[0x8f8]=1;memcpy(gun+0x900,&base,sizeof base);relative_mesh=base;
    class_size(gun_class,0x11a0);
    for(int side=0;side<2;side++)for(int ads=0;ads<2;ads++) {
        memset(&aim_fps,0,sizeof aim_fps);head=base;head.p[2]=150;
        hand[0x314]=(unsigned char)side;fixture_fire_side=side;g_touch_aim=0;
        hand_pose=base;hand_pose.p[0]=17;hand_pose.p[2]=136;
        memcpy(root_component+0x1c0,&hand_pose,sizeof hand_pose);
        struct reload_tf actor=hand_pose;actor.p[0]+=4;
        memcpy(shotgun_root+0x1c0,&actor,sizeof actor);
        raw_fire=aim_world_marker(actor,bone);fire.tf=raw_fire;sights=raw_fire;
        touch_aim_tick(hand);test_now+=g_gun_draw_ms;touch_aim_tick(hand);
        g_touch_aim=ads;touch_aim_tick(hand);test_now+=180;touch_aim_tick(hand);
        long drawn=aim_fps.drawn_at;fixture_cycle_busy=1;
        for(int frame=0;frame<60;frame++) {
            memcpy(&hand_pose,root_component+0x1c0,sizeof hand_pose);
            // A native world-space gun rotation, followed by the attached
            // FireBone update. The actual barrel remains native local +Y.
            actor=hand_pose;actor.p[0]+=4;
            float angle=.025f*(float)frame;
            actor.q=q_mul(hand_pose.q,(qf_t){0,sinf(angle*.5f),0,cosf(angle*.5f)});
            memcpy(shotgun_root+0x1c0,&actor,sizeof actor);
            raw_fire=aim_world_marker(actor,bone);fire.tf=raw_fire;sights=raw_fire;
            test_now+=16;touch_aim_tick(hand);
            float direction[3];q_rot(fire.tf.q,(float[3]){0,1,0},direction);
            assert(fabsf(direction[0]-1)<.0002f&&fabsf(direction[1])<.0002f&&fabsf(direction[2])<.0002f);
            float expected[3]={ads?g_gun_ads[0]:g_gun_hip[0],ads?0:(side?g_gun_hip[1]:-g_gun_hip[1]),ads?g_gun_ads[2]:g_gun_hip[2]};
            for(int j=0;j<3;j++)assert(fabsf(fire.tf.p[j]-head.p[j]-expected[j])<.002f);
            assert(aim_fps.drawn_at==drawn&&reload_tf_valid(&last_pose));
        }
        // After releasing the forestock, native one-hand reorientation still
        // moves the gun relative to the hand. No draw/reset or axis jump.
        fixture_cycle_busy=0;
        for(int frame=0;frame<20;frame++) {
            memcpy(&hand_pose,root_component+0x1c0,sizeof hand_pose);actor=hand_pose;
            float angle=.5f*(1-(float)frame/20);
            actor.q=q_mul(hand_pose.q,(qf_t){0,sinf(angle*.5f),0,cosf(angle*.5f)});
            memcpy(shotgun_root+0x1c0,&actor,sizeof actor);raw_fire=aim_world_marker(actor,bone);fire.tf=raw_fire;sights=raw_fire;
            test_now+=16;touch_aim_tick(hand);
            float direction[3];q_rot(fire.tf.q,(float[3]){0,1,0},direction);
            assert(fabsf(direction[0]-1)<.0002f&&fabsf(direction[1])<.0002f&&fabsf(direction[2])<.0002f);
            assert(aim_fps.drawn_at==drawn);
        }
        // A small native mesh recoil remains exactly the authored rotation.
        struct reload_tf before=fire.tf;
        qf_t kick={0,sinf(.02f),0,cosf(.02f)};raw_fire.q=q_mul(kick,raw_fire.q);fire.tf=raw_fire;recoiling=1;
        test_now+=16;touch_aim_tick(hand);recoiling=0;
        qf_t expected=q_mul(kick,before.q);compare_quat(fire.tf.q,expected);
        g_reload_phase=7;count=writes;touch_aim_tick(hand);assert(writes==count);g_reload_phase=0;
    }
    // Original one-hand recoil modifies the PoseableMesh independently of the
    // gun actor/hand. Drive that actual hierarchy, including a native overwrite
    // AFTER the ordinary hand tick, rather than rotating only the gun root.
    for(int side=0;side<2;side++)for(int ads=0;ads<2;ads++) {
        memset(&aim_fps,0,sizeof aim_fps);head=base;head.p[2]=150;
        hand[0x314]=(unsigned char)side;fixture_fire_side=side;g_touch_aim=0;recoiling=0;
        hand_pose=base;hand_pose.p[0]=17;hand_pose.p[2]=136;relative_mesh=base;
        memcpy(root_component+0x1c0,&hand_pose,sizeof hand_pose);
        struct reload_tf actor=hand_pose;memcpy(shotgun_root+0x1c0,&actor,sizeof actor);
        raw_fire=fixture_compose(actor,bone);fire.tf=raw_fire;sights=raw_fire;
        touch_aim_tick(hand);test_now+=g_gun_draw_ms;touch_aim_tick(hand);
        g_touch_aim=ads;touch_aim_tick(hand);test_now+=180;touch_aim_tick(hand);
        unsigned char immutable_gun[sizeof gun];memcpy(immutable_gun,gun,sizeof gun);
        struct reload_tf immutable_head=head;
        long original_draw=aim_fps.drawn_at;int original_post=post_calls,original_mesh=mesh_writes;
        post_overwrite=1;recoiling=1;
        for(int frame=0;frame<40;frame++) {
            float phase=sinf((float)frame*3.14159265359f/39),half=phase*.70f;
            post_mesh=base;post_mesh.q=(qf_t){sinf(half),0,0,cosf(half)};
            post_mesh.p[1]=18*phase;post_mesh.p[2]=24*phase;
            test_now+=16;touch_aim_tick(hand);hook_aim_shotgun_post(gun,.016f);
            assert(reload_tf_valid(&relative_mesh));
            float angle=2*acosf(fminf(1,fabsf(relative_mesh.q.w))),distance=0;
            for(int j=0;j<3;j++)distance+=relative_mesh.p[j]*relative_mesh.p[j];
            assert(angle<=.10473f&&distance<=9.001f);
            assert(!memcmp(immutable_gun,gun,sizeof gun)&&!memcmp(&head,&immutable_head,sizeof head));
            assert(aim_fps.drawn_at==original_draw);
            float actual[3];q_rot(fire.tf.q,(float[3]){0,1,0},actual);
            assert(actual[0]>=cosf(.10473f)-.001f);
            // A finite 3 cm mesh kick plus the 30 cm FireBone lever arm stays
            // bounded around the intended hip/ADS marker, in centimetres.
            float target[3]={ads?g_gun_ads[0]:g_gun_hip[0],ads?0:(side?g_gun_hip[1]:-g_gun_hip[1]),ads?g_gun_ads[2]:g_gun_hip[2]},error=0;
            for(int j=0;j<3;j++){float d=fire.tf.p[j]-head.p[j]-target[j];error+=d*d;}
            assert(error<7.0f*7.0f);
        }
        assert(post_calls==original_post+40&&mesh_writes>original_mesh);
        post_overwrite=0;recoiling=0;fixture_mesh_transform(base);test_now+=16;hook_aim_shotgun_post(gun,.016f);
        float actual[3];q_rot(fire.tf.q,(float[3]){0,1,0},actual);
        assert(fabsf(actual[0]-1)<.0002f&&fabsf(actual[1])<.0002f&&fabsf(actual[2])<.0002f);
        assert(!memcmp(immutable_gun,gun,sizeof gun));
        // Original post-tick always runs; cancelled/menu/paused/wrong-owner
        // callbacks cannot apply a mesh or hand presentation afterwards.
        volatile int* gates[]={&g_touch_cancel,&paused,&g_reload_phase};
        for(int i=0;i<3;i++){int before_writes=writes,before_mesh=mesh_writes;*gates[i]=i==2?7:1;hook_aim_shotgun_post(gun,.016f);*gates[i]=0;assert(writes==before_writes&&mesh_writes==before_mesh);}
        count=writes;g_touch_context=2;hook_aim_shotgun_post(gun,.016f);g_touch_context=1;assert(writes==count);
        owner=other_pawn;hook_aim_shotgun_post(gun,.016f);owner=pawn;assert(writes==count);
    }
    struct reload_tf kick=base,limited=base;
    kick.q=(qf_t){sinf(.02f),0,0,cosf(.02f)};kick.p[2]=2;
    assert(aim_mobile_recoil_pose(base,kick,&limited)==0&&!memcmp(&limited,&kick,sizeof kick));
    kick.q.x=NAN;assert(aim_mobile_recoil_pose(base,kick,&limited)==-1);
    kick=base;kick.s[1]=2;assert(aim_mobile_recoil_pose(base,kick,&limited)==-1);
    kick=base;kick.p[0]=FLT_MAX;assert(aim_mobile_recoil_pose(base,kick,&limited)==-1);
    kick=base;kick.q=(qf_t){sinf(.7f),0,0,cosf(.7f)};fixture_mesh_transform(kick);recoiling=1;
    count=mesh_writes;pointer_at(mesh,0x20,pawn);assert(aim_shotgun_recoil(gun,mesh)==-1&&mesh_writes==count);pointer_at(mesh,0x20,gun);
    gun[0x8f8]=0;assert(aim_shotgun_recoil(gun,mesh)==-1&&mesh_writes==count);gun[0x8f8]=1;
    mesh_getter_cancel=1;assert(aim_shotgun_recoil(gun,mesh)==-1&&mesh_writes==count);mesh_getter_cancel=0;g_touch_cancel=0;
    mesh_getter_rename=1;assert(aim_shotgun_recoil(gun,mesh)==-1&&mesh_writes==count);mesh_getter_rename=0;mesh[0x1c]^=1;
    recoiling=0;fixture_mesh_transform(base);
    fixture_shotgun_reference=0;
    // Damage/control lock keeps the calibrated hip pose; it never creates an ADS input or edits game state.
    memset(&aim_fps,0,sizeof aim_fps);head=base;head.p[2]=150;hand[0x314]=1;fixture_fire_side=1;
    fixture_cycle_busy=0;g_touch_aim=0;interactivity=1;simulate_setter=1;fixture_weapon_fov=90;g_fov_h_deg=90;
    hand_pose=base;hand_pose.p[0]=17;hand_pose.p[2]=136;memcpy(root_component+0x1c0,&hand_pose,sizeof hand_pose);
    fire.tf=base;fire.tf.p[0]=37;fire.tf.p[1]=1;fire.tf.p[2]=148;raw_fire=fire.tf;sights=fire.tf;
    unsigned char locked_gun[sizeof gun],locked_pawn[sizeof pawn];memcpy(locked_gun,gun,sizeof gun);memcpy(locked_pawn,pawn,sizeof pawn);
    count=writes;test_now+=16;touch_aim_tick(hand);test_now+=g_gun_draw_ms;touch_aim_tick(hand);
    assert(writes==count+2&&aim_fps.blend==0&&!g_touch_aim);
    for(int i=0;i<3;i++)assert(fabsf(fire.tf.p[i]-head.p[i]-g_gun_hip[i])<.001f);
    assert(!memcmp(locked_gun,gun,sizeof gun)&&!memcmp(locked_pawn,pawn,sizeof pawn));
    count=writes;interactivity_in_getter=0;test_now+=16;touch_aim_tick(hand);interactivity_in_getter=-1;
    assert(writes==count);interactivity=3;touch_aim_tick(hand);assert(writes==count);interactivity=2;
    puts("PASS: original control-disabled state1 retains calibrated hip with aim0 and immutable game objects; state0/3 and mid-getter interactivity changes cannot write a pose");
    puts("PASS: 300 hip/midpoint/ADS shortest-arc Fire alignments and 100 roll cases; actual native-Gun guards, configured FPS draw/180 ms reversible ADS, continuous pistol reload/cocking/return, recoil retained, full-name/context/expiry recovery and immutable objects");
    puts("PASS: actual held-gun FOV60/90/110 depth, invariant ADS/shot axis, 32 live camera-sway frames, 120 interleaved pooled/null-owner hand frames, active-pawn pause cache reset and pure bounded sway/bob/off/zero-speed/wrap cases");
    puts("PASS: 240 independently rotated two-hand shotgun frames and 80 release/reorientation frames, both primary sides in hip/ADS, native +Y FireBone axis, mesh recoil retained and shell reload gate");
    puts("PASS: 160 independently animated shotgun mesh frames, native post-tick overwrite, both primary sides hip/ADS, <=6 degree/3 cm recoil, unchanged head/gun state, small recoil retained, exact neutral shot axis restored and post-tick cancellation/ownership gates");
    // The derived native hand continues after the hooked base Tick. Inventory
    // can pause inside that base Tick, so exercise its subsequent overwrite too.
    const int inventory_world_fovs[]={70,85,90,110},inventory_weapon_fovs[]={70,90,110};
    for(int side=0;side<2;side++)for(int world=0;world<4;world++)for(int weapon_profile=0;weapon_profile<3;weapon_profile++){
        memset(&aim_fps,0,sizeof aim_fps);fixture_shotgun_reference=0;fixture_cycle_busy=0;
        fixture_motion_busy=fixture_equip_busy=0;g_touch_cancel=g_touch_aim=g_reload_phase=g_touch_reload_request=g_touch_weapon_request=0;
        g_touch_context=active_type=1;g_touch_weapon=2;paused=0;valid_player=1;interactivity=2;
        owner=active=pawn;primary=hand;weapon=gun;held=grip;fixture_fire_side=side;hand[0x314]=(unsigned char)side;
        head=base;head.p[2]=150;hand_pose=base;hand_pose.p[0]=17;hand_pose.p[2]=136;memcpy(root_component+0x1c0,&hand_pose,sizeof hand_pose);
        fire.tf=base;fire.tf.p[0]=37;fire.tf.p[1]=1;fire.tf.p[2]=148;raw_fire=fire.tf;sights=fire.tf;simulate_setter=1;
        fixture_world_fov=inventory_world_fovs[world];fixture_weapon_fov=inventory_weapon_fovs[weapon_profile];g_fov_h_deg=(float)fixture_world_fov;
        test_now+=10000;touch_aim_tick(hand);test_now+=g_gun_draw_ms;touch_aim_tick(hand);
        struct reload_tf future_hip=last_pose;
        float hip_target[3]={weapon_fov_depth(g_gun_hip[0],(float)fixture_world_fov,(float)fixture_weapon_fov),
            (side?1:-1)*g_gun_hip[1],g_gun_hip[2]};
        for(int k=0;k<3;k++)assert(fabsf(fire.tf.p[k]-head.p[k]-hip_target[k])<.001f);
        g_fov_h_deg=.84f*fixture_world_fov;float old_ads_fov=g_fov_h_deg;
        g_touch_aim=1;touch_aim_tick(hand);test_now+=180;touch_aim_tick(hand);assert(aim_fps.blend==1);
        g_touch_aim=0;touch_aim_inventory_prepare(side?hand:left);assert(aim_fps.inventory_hold<0&&aim_fps.inventory_root&&aim_fps.blend==0);
        assert(g_fov_h_deg==old_ads_fov); // Presentation must not change the real camera or its zoom.
        for(int k=0;k<3;k++)assert(fabsf(aim_fps.inventory_pose.p[k]-future_hip.p[k])<.001f);
        compare_quat(aim_fps.inventory_pose.q,future_hip.q);
        struct reload_tf calibration=aim_fps.fire_local;long drawn=aim_fps.drawn_at;
        touch_aim_inventory_result(1);assert(aim_fps.inventory_hold==1);paused=1;g_touch_context=2;g_touch_cancel=3;
        g_fov_h_deg=(float)fixture_world_fov; // The normal view has retired the old ADS zoom.
        for(int frame=0;frame<40;frame++){
            fixture_equip_generation++; // Real paused Quick Select retires its publication on every hand Tick.
            test_now+=160;derived_overwrite=1;hook_aim_bio4_hand(hand,.016f);
            assert(aim_fps.inventory_hold==1&&aim_fps.blend==0&&!g_touch_aim);
            for(int k=0;k<3;k++)assert(fabsf(fire.tf.p[k]-head.p[k]-hip_target[k])<.001f);
        }
        void* guarded[]={pawn,hand,gun,grip,root_component};
        for(int changed=0;changed<5;changed++){
            ((unsigned char*)guarded[changed])[0x1c]^=1;count=writes;touch_aim_tick(hand);
            assert(writes==count);((unsigned char*)guarded[changed])[0x1c]^=1;
        }
        fixture_equip_generation++;count=writes;touch_aim_tick(hand);assert(writes==count+1);fixture_equip_generation--;
        fixture_equip_generation++;derived_overwrite=1;g_touch_context=1;paused=0;g_touch_cancel=0;
        foreign_hand=left;left[0x314]=hand[0x314];touch_aim_tick(left);foreign_hand=0;
        test_now+=16;hook_aim_bio4_hand(hand,.016f);
        assert(!aim_fps.inventory_hold&&!aim_fps.blend&&aim_fps.drawn_at==drawn&&!memcmp(&calibration,&aim_fps.fire_local,sizeof calibration));
        assert(aim_fps.equip_generation==fixture_equip_generation);
        for(int k=0;k<3;k++)assert(fabsf(fire.tf.p[k]-head.p[k]-hip_target[k])<.001f);
        derived_overwrite=0;
        touch_aim_inventory_prepare(side?hand:left);touch_aim_inventory_result(0);assert(!aim_fps.inventory_hold);
    }
    puts("PASS: final derived-hand writer defeats later native ADS overwrite; both primary hands lower before native Inventory pause, remain hip for6.4s paused/Cancelled, survive real paused Quick Select epoch churn and pooled hand on first resume frame without recapturing raised pose; refused pause never holds cache");
    puts("PASS: 24 both-hand Inventory profiles worldFOV70/85/90/110 weaponFOV70/90/110 capture the future unzoomed hip despite old .84*world ADS FOV; live camera FOV unchanged and native identity/paused/resume guards retained");
    const unsigned short pause_modes[]={0xff00,0xff02};
    for(int side=0;side<2;side++)for(int mode=0;mode<2;mode++) {
        g_touch_context=active_type=1;g_touch_cancel=paused=g_touch_aim=0;pause_accepted=1;
        owner=active=pawn;primary=hand;weapon=gun;held=grip;fixture_fire_side=side;hand[0x314]=(unsigned char)side;
        fixture_world_fov=90;fixture_weapon_fov=90;g_fov_h_deg=90;
        memset(&aim_fps,0,sizeof aim_fps);head=base;head.p[2]=150;
        hand_pose=base;hand_pose.p[0]=17;hand_pose.p[2]=136;memcpy(root_component+0x1c0,&hand_pose,sizeof hand_pose);
        fire.tf=base;fire.tf.p[0]=37;fire.tf.p[1]=1;fire.tf.p[2]=148;raw_fire=fire.tf;sights=fire.tf;
        test_now+=10000;touch_aim_tick(hand);test_now+=g_gun_draw_ms;touch_aim_tick(hand);
        g_touch_aim=1;touch_aim_tick(hand);test_now+=180;touch_aim_tick(hand);assert(aim_fps.blend==1);
        g_fov_h_deg=75.6f;g_touch_aim=0;
        // Existing Inventory owns preparation outside the shared native entry.
        if(mode)touch_aim_inventory_prepare(side?hand:left);
        int calls=pause_calls;assert(hook_aim_try_pause(pause_modes[mode])==1&&pause_calls==calls+1&&pause_start==pause_modes[mode]);
        if(mode){assert(aim_fps.inventory_hold<0);touch_aim_inventory_result(1);}
        assert(aim_fps.inventory_hold==1&&!aim_fps.blend&&aim_fps.inventory_root==root_component);
        struct reload_tf neutral_cache=aim_fps.fire_local;long preserved_draw=aim_fps.drawn_at;
        unsigned char accepted_cache[sizeof aim_fps];memcpy(accepted_cache,&aim_fps,sizeof aim_fps);
        pause_accepted=0;assert(!hook_aim_try_pause(0xff03));
        assert(!memcmp(accepted_cache,&aim_fps,sizeof aim_fps)); // Refused/nested pause cannot retire an accepted hold.
        g_fov_h_deg=90;
        for(int frame=0;frame<8;frame++) {
            fixture_equip_generation++;test_now+=160;derived_overwrite=1;hook_aim_bio4_hand(hand,.016f);
            assert(aim_fps.inventory_hold==1&&!g_touch_aim&&!aim_fps.blend);
            for(int k=0;k<3;k++)assert(fabsf(fire.tf.p[k]-head.p[k]-(k==1?(side?1:-1)*g_gun_hip[k]:g_gun_hip[k]))<.001f);
        }
        g_touch_context=1;paused=g_touch_cancel=0;flourishing=1;test_now+=16;hook_aim_bio4_hand(hand,.016f);
        assert(!aim_fps.inventory_hold&&!aim_fps.blend&&aim_fps.drawn_at==preserved_draw&&aim_fps.gun==gun);
        assert(!memcmp(&neutral_cache,&aim_fps.fire_local,sizeof neutral_cache));
        for(int k=0;k<3;k++)assert(fabsf(fire.tf.p[k]-head.p[k]-(k==1?(side?1:-1)*g_gun_hip[k]:g_gun_hip[k]))<.001f);
        flourishing=derived_overwrite=0;
        memcpy(accepted_cache,&aim_fps,sizeof aim_fps);active=other_pawn;count=writes;
        assert(!hook_aim_try_pause(pause_modes[mode])&&writes==count&&!memcmp(accepted_cache,&aim_fps,sizeof aim_fps));active=pawn;
        fixture_gt=0;assert(!hook_aim_try_pause(pause_modes[mode])&&writes==count);fixture_gt=1;
        assert(!hook_aim_try_pause(pause_modes[mode])&&!aim_fps.inventory_hold&&g_touch_context==1&&!paused);
    }
    puts("PASS: actual shared TryPause wrapper preserves ordinary Pause and existing Inventory start-info/acceptance; both primary hands stay hip through native overwrite/QuickSelect churn and first resume frame while flourish still active; refused/foreign/off-GT/nested requests preserve owned state");
    fixture_frame=17;g_pose_latch_frame=16;g_pose_trace=1;count=writes;int trace_logs=diagnostic_logs;
    touch_aim_tick(hand);g_pose_trace=0;
    assert(writes==count+1&&diagnostic_logs==trace_logs+1&&!g_pose_latch_lock&&fixture_frame==17&&g_pose_latch_frame==16);
    return 0;
}
