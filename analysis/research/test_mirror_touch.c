// Reuse the ordinary interaction fixture to exercise the new callback priorities too.
#define main ordinary_interact_tests
#define request legacy_request
#include "test_interact.c"
#undef request
#undef main

#include <math.h>
typedef struct {float x,y,z,w;} qf_t;
struct __attribute__((aligned(16))) reload_tf {qf_t q;float p[3],s[3],padding[2];};
#include MIRROR_POSE_MATH
#define pthread_mutex_t int
#define PTHREAD_MUTEX_INITIALIZER 0
static unsigned long long pose_frame;
static volatile unsigned long long* g_pose_frame_counter=&pose_frame;
static float pose_yaw,pose_pitch;
static long ptr_now_ms(void){return clock_now;}
static void pose_angles(long node,float* yaw,float* pitch){(void)node;*yaw=pose_yaw;*pitch=pose_pitch;}
static void head_pose_node(long node,qf_t* q,float p[3]) {
    (void)node;*q=q_mul((qf_t){0,sinf(pose_yaw*.5f),0,cosf(pose_yaw*.5f)},(qf_t){sinf(pose_pitch*.5f),0,0,cosf(pose_pitch*.5f)});
    p[0]=p[2]=0;p[1]=1.6f;
}
static qf_t pose_camera_delta_ue(void){return (qf_t){0,0,0,1};}

struct cinema_ui_weak {int index,serial;};
static void* weak_object[12];static int weak_serial[12],weak_count;
static int alive(const void* p) {
    for(int i=0;i<weak_count;i++)if(weak_object[i]==p)return weak_serial[i]>0;
    return p!=0;
}
static void assign_weak(struct cinema_ui_weak* w,const void* p) {
    int i=0;for(;i<weak_count;i++)if(weak_object[i]==p)break;
    if(i==weak_count){assert(i<12);weak_object[i]=(void*)p;weak_serial[i]=1;weak_count++;}
    w->index=i;w->serial=weak_serial[i];
}
static void* resolve_weak(const struct cinema_ui_weak* w) {
    return w->index>=0 && w->index<weak_count && w->serial==weak_serial[w->index] && w->serial>0 ? weak_object[w->index]:0;
}
static void (*cu_weak_assign)(struct cinema_ui_weak*,const void*)=assign_weak;
static void* (*cu_weak_get)(const struct cinema_ui_weak*)=resolve_weak;
static int pv_alive(const void* p){return alive(p);}
static int qs_ready=1,g_touch_context=1,g_touch_weapon_request;
static int qs_binding(void*,int,void**),qs_current(void**),touch_quickselect_busy(void);
static void* (*tw_holster)(const void*);
#include "../re4fix_mirror_touch.h"
#ifdef MIRROR_NATIVE_FIXTURE
#include MIRROR_NATIVE_FIXTURE
#endif

static void* pair_forced[2];static int pair_accept[2],pair_valid[2],pair_wall[2],pair_grips,holsters,slot_held,qs_busy;
static int pair_pressed[2],pair_released[2],pair_hover_add[2],pair_hover_remove[2],pair_los;
static int foreign_after_press,change_owner_on_press;
static int trap_enabled,trap_reverse,trap_dog;
static unsigned char trap_jaws[0x50];static void* trap_registered[2];
static unsigned char pose_roots[3][0x200];
static float native_offset[2][3],grab_center_offset[2][3];
static float initial_hand_position[2][3];
static float joint_scale=1;
static void* weapon(void){return f.other;}
static void* weapon_grip(void){return f.item_root;}
static int hand_side(const void* h){assert(h==f.left || h==f.right);return h==f.right;}
static void* mirror_best(const void* h) {
    int s=hand_side(h);if(trap_reverse)s=!s;return pair_forced[hand_side(h)] ? pair_forced[hand_side(h)]:s ? f.foreign_grip:f.grip;
}
static void* mirror_current(const void* h) {
    void* p=hand_side(h)?held_right:held;
    return alive(p)?p:0;
}
static int mirror_free(const void* g,int side) {
    assert((g==f.grip || g==f.foreign_grip) && side>=0 && side<2);
    return pair_grips && pair_valid[g==f.foreign_grip] && (trap_enabled || g==(side?f.foreign_grip:f.grip));
}
static int mirror_in_valid(const void* g){return pair_valid[g==f.foreign_grip];}
static struct ti_vec mirror_center(const void* g) {
    if(trap_enabled)return (struct ti_vec){95,g==f.foreign_grip?20:-20,107};
    const float* p=(const float*)((const char*)g+0x1d0);
    return (struct ti_vec){p[0]-1.9260292053222656f,p[1],p[2]+1.4894256591796875f}; // original Mirror GrabBox offset
}
static int mirror_los(const void* g,const float* e,const float* p) {
    assert(e[2]==160 && p[0]==0 && p[1]==0 && p[2]==0);pair_los++;return !pair_wall[g==f.foreign_grip];
}
static void mirror_hover(void* h,void* g,int on) {
    int s=hand_side(h),which=trap_reverse?!s:s;assert(g==(which?f.foreign_grip:f.grip));
    if(on){assert(!pair_forced[s]);pair_forced[s]=g;pair_hover_add[s]++;}
    else {assert(pair_forced[s]==g);pair_forced[s]=0;pair_hover_remove[s]++;}
}
static void mirror_press(void* h) {
    int s=hand_side(h);pair_pressed[s]++;((unsigned char*)h)[0x33d]=1;
    memcpy(initial_hand_position[s],pose_roots[s]+0x1d0,3*sizeof(float));
    if(pair_accept[s]){if(s)held_right=foreign_after_press?f.pawn:mirror_best(h);else held=mirror_best(h);}
    if(change_owner_on_press)active=f.other;
}
static void mirror_release(void* h) {
    int s=hand_side(h);pair_released[s]++;((unsigned char*)h)[0x33d]=0;
    if(s)held_right=0;else held=0;
}
static struct ti_vec fixture_grab_location(const void* h) {
    int s=hand_side(h);struct reload_tf root;memcpy(&root,pose_roots[s]+0x1c0,sizeof root);
    float d[3];q_rot(root.q,grab_center_offset[s],d);
    return (struct ti_vec){root.p[0]+d[0],root.p[1]+d[1],root.p[2]+d[2]};
}
static float fixture_release_distance(const void* grip,const struct ti_vec* p) {
    const float* center=(const float*)((const char*)grip+0x1d0);float values[3]={p->x,p->y,p->z},result=0;
    float trap_center[3]={grip==f.grip?100.005554f:94.999725f,grip==f.grip?-38.775063f:43.138641f,grip==f.grip?47.645920f:48.158997f};
    if(trap_enabled)center=trap_center;
    for(int i=0;i<3;i++){float outside=fmaxf(0,fabsf(values[i]-center[i])-(trap_enabled?(i==2?150:60):30));result+=outside*outside;}
    return result;
}
static struct reload_tf fixture_joint(const void* trap,int index) {
    assert(trap==f.parent && (index==0 || index==1));
    return (struct reload_tf){{0,0,0,1},{95,0,107},{joint_scale,joint_scale,joint_scale},{0,0}};
}
static void* mirror_class(void){return f.world_class;}
static void* trap_class(void){return f.pickup_class;}
static int native_is_dog(const void* p){assert(p==f.parent);return trap_dog;}
static void* native_holster(const void* item){assert(item==weapon());return f.holster_class;}
static int qs_binding(void* pawn,int slot,void** out) {
    assert(pawn==f.pawn);assert(slot==0 || slot==1 || slot==3);
    if(slot!=slot_held || held_right!=weapon_grip())return 0;
    out[0]=pawn;out[1]=weapon();out[2]=f.right;out[3]=weapon_grip();out[4]=f.prop_class;return 1;
}
static int qs_current(void** binding){return binding[1]==weapon() && held_right==binding[3];}
static int touch_quickselect_busy(void){return qs_busy;}
static void native_put_away(void* item,int immediate) {
    assert(item==weapon() && immediate==1 && pair_los>=2);holsters++;held_right=0;
}
static void mirror_ready(void) {
    world_ready();
    memset(&tm_hold,0,sizeof tm_hold);memset(weak_object,0,sizeof weak_object);
    memset(weak_serial,0,sizeof weak_serial);weak_count=0;
    *(void**)(f.parent+0x248)=f.grip;*(void**)(f.parent+0x260)=f.foreign_grip;
    *(void**)(f.foreign_grip+0x20)=f.parent;
    pair_forced[0]=pair_forced[1]=0;pair_valid[0]=pair_valid[1]=pair_accept[0]=pair_accept[1]=1;
    pair_wall[0]=pair_wall[1]=0;pair_grips=1;holsters=pair_los=qs_busy=0;slot_held=-1;
    foreign_after_press=change_owner_on_press=0;g_touch_context=1;g_touch_weapon_request=0;g_grip_latched=0;
    trap_enabled=trap_reverse=trap_dog=0;
    memset(pair_pressed,0,sizeof pair_pressed);memset(pair_released,0,sizeof pair_released);
    memset(pair_hover_add,0,sizeof pair_hover_add);memset(pair_hover_remove,0,sizeof pair_hover_remove);
    qs_ready=1;cu_weak_assign=assign_weak;cu_weak_get=resolve_weak;
    ti_best=mirror_best;ti_free=mirror_free;ti_in_valid=mirror_in_valid;ti_line_of_sight=mirror_los;
    ti_grab_center=mirror_center;ti_press=mirror_press;ti_release=mirror_release;
    f_GetCurrentGrip=mirror_current;f_SetGripForcedHover=mirror_hover;
    tm_mirror_class=mirror_class;tm_holster=native_put_away;tw_holster=native_holster;
    tm_trap_class=0;tm_dog_trap=0;
    tm_grab_location=fixture_grab_location;tm_release_distance=fixture_release_distance;tm_joint_transform=fixture_joint;
    memset(pose_roots,0,sizeof pose_roots);memset(native_offset,0,sizeof native_offset);memset(grab_center_offset,0,sizeof grab_center_offset);
    memset(&tm_pose_plan,0,sizeof tm_pose_plan);memset(&tm_pose_frame,0,sizeof tm_pose_frame);tm_pose_served=0;pose_yaw=pose_pitch=0;joint_scale=1;pose_frame++;
    struct reload_tf identity={{0,0,0,1},{0,0,0},{1,1,1},{0,0}};
    struct reload_tf head=identity;head.p[2]=160;memcpy(f.head+0x1c0,&head,sizeof head);
    struct reload_tf actor=identity;actor.p[0]=142.15716552734375f;memcpy(pose_roots[2]+0x1c0,&actor,sizeof actor);
    *(void**)(f.parent+0x130)=pose_roots[2];
    for(int s=0;s<3;s++){*(void**)(pose_roots[s]+0x10)=f.scene_class;*(void**)(pose_roots[s]+0x20)=s==2?f.parent:s?f.right:f.left;}
    for(int s=0;s<2;s++) {
        unsigned char* hand=s?f.right:f.left;*(void**)(hand+0x130)=pose_roots[s];*(float*)(hand+0x34c)=8;
        struct reload_tf initial=identity;initial.p[0]=30;initial.p[1]=s?20:-20;initial.p[2]=130;memcpy(pose_roots[s]+0x1c0,&initial,sizeof initial);
        initial.p[0]=95;initial.p[1]=s?20.29507064819336f:-19.56024742126465f;initial.p[2]=107;memcpy((s?f.foreign_grip:f.grip)+0x1c0,&initial,sizeof initial);
    }
    interact_mirror_hook=touch_mirror_interact;interact_mirror_tick=touch_mirror_tick;interact_sce_hook=0;
}
static void native_hand_frame(int s) {
        float position[3];
        if(touch_mirror_pose(s,clock_now,position)) {
            qf_t q;float raw_p[3];head_pose_node(2,&q,raw_p);
            struct reload_tf raw={{-q.z,q.x,q.y,-q.w},{-position[2]*100,position[0]*100,position[1]*100},{1,1,1},{0,0}};
            struct reload_tf raw_head={{-q.z,q.x,q.y,-q.w},{-raw_p[2]*100,raw_p[0]*100,raw_p[1]*100},{1,1,1},{0,0}},head;
            memcpy(&head,f.head+0x1c0,sizeof head);struct reload_tf world=reload_hand_pose(raw,raw_head,head);
            for(int i=0;i<3;i++)world.p[i]+=native_offset[s][i];memcpy(pose_roots[s]+0x1c0,&world,sizeof world);
        }
    touch_interact_tick(s?f.right:f.left,s,clock_now);
}
static void native_pose_frame(void) {
    pose_frame++;clock_now+=16;
    native_hand_frame(0);native_hand_frame(1);
}
static void request(void) {
    int previous=tm_hold.phase;legacy_request();
    if(!previous)for(int i=0;i<80 && tm_hold.phase==1;i++)native_pose_frame();
}
static void trap_ready(void) {
    mirror_ready();trap_enabled=trap_dog=1;set_class(f.pickup_class,7,0x530);
    *(void**)(f.parent+0x10)=f.pickup_class;memset(trap_jaws,0,sizeof trap_jaws);
    trap_registered[0]=f.grip;trap_registered[1]=f.foreign_grip;
    *(void**)(trap_jaws+0x10)=f.grip;*(void**)(trap_jaws+0x38)=f.foreign_grip;
    *(void**)(f.parent+0x200)=trap_registered;*(int*)(f.parent+0x208)=*(int*)(f.parent+0x20c)=2;
    *(void**)(f.parent+0x218)=trap_jaws;*(int*)(f.parent+0x220)=*(int*)(f.parent+0x224)=2;
    tm_trap_class=trap_class;tm_dog_trap=native_is_dog;
    *(float*)(f.grip+0x1d0)=95-46.5f;*(float*)(f.foreign_grip+0x1d0)=95+46.5f;
    *(float*)(f.grip+0x1d4)=*(float*)(f.foreign_grip+0x1d4)=0;
}
static void held_pair(void) {
    assert(tm_hold.object[0]==f.pawn && held==f.grip && held_right==f.foreign_grip);
    assert(tm_hold.owned_input==3 && g_cmd_until[29]==LONG_MAX && g_cmd_until[27]==LONG_MAX);
    for(int s=0;s<2;s++)assert(pair_pressed[s]==1 && !pair_released[s] && pair_hover_add[s]==1 && pair_hover_remove[s]==1 && !pair_forced[s]);
}
// The original Oculus dispatcher compares its own previous HandTrigger state,
// independently of the hand's IsGripDown. Actual ARM64 edges/guard execute in
// check_pair_grip_native_paths.py; this frame crosses the real adapter's claim.
static void controller_frame(int previous[2]) {
    for(int s=0;s<2;s++) {
        unsigned bit=1u<<(s?27:29);
        int next=g_cmd_until[s?27:29]>clock_now || (g_grip_latched&bit);
        void* h=s?(void*)f.right:(void*)f.left;
        if(previous[s] && !next)mirror_release(h);
        if(!previous[s] && next) {
            ((unsigned char*)h)[0x33d]=1;
            if(!mirror_current(h))mirror_press(h); // native IsGrabbingEnabled gate
        }
        previous[s]=next;
    }
}
static void test_pose_barrier(void) {
    trap_ready();
    *(float*)(pose_roots[0]+0x1d0)=-3.7f;
    struct ti_vec point=fixture_grab_location(f.left);
    // Old accepted remote pose is outside the native box by >GrabRadius+20cm:
    // the pinned original distance curve therefore releases immediately.
    assert(sqrtf(fixture_release_distance(f.grip,&point))-8>20);
    legacy_request();assert(tm_hold.phase==1 && !held && !held_right && !g_cmd_until[27] && !g_cmd_until[29]);
    for(int i=0;i<5;i++){pose_frame++;clock_now+=16;native_hand_frame(0);}
    assert(tm_hold.phase==1 && !held && !held_right && pair_pressed[0]==0 && pair_pressed[1]==0);
    float render_only[3];assert(touch_mirror_pose(1,clock_now,render_only));
    native_hand_frame(0);assert(tm_hold.phase==1 && !(tm_hold.seen&2) && !pair_pressed[0] && !pair_pressed[1]);
    for(int i=0;i<10 && tm_hold.phase==1;i++)native_pose_frame();assert(tm_hold.phase==2 && held && held_right);
    for(int i=0;i<3;i++)native_pose_frame();
    for(int s=0;s<2;s++)for(int i=0;i<3;i++)assert(fabsf(*(float*)(pose_roots[s]+0x1d0+4*i)-initial_hand_position[s][i])<.25f);
    request();
    // Existing controller-to-grab-center and a native dynamic offset remain in
    // the consumer; preparation converges before either authored InitializeGrab.
    mirror_ready();native_offset[0][0]=3;native_offset[1][1]=-2;
    grab_center_offset[0][2]=5;grab_center_offset[1][0]=4;
    request();held_pair();
    struct ti_vec grab_box=mirror_center(f.grip);
    assert(fabsf(grab_box.x-initial_hand_position[0][0])>1 && fabsf(grab_box.z-initial_hand_position[0][2])>1);
    for(int s=0;s<2;s++) {
        const float* target=(const float*)((s?f.foreign_grip:f.grip)+0x1d0);
        for(int i=0;i<3;i++)assert(fabsf(initial_hand_position[s][i]-target[i])<.25f);
        point=fixture_grab_location(s?f.right:f.left);assert(fixture_release_distance(s?f.foreign_grip:f.grip,&point)==0);
    }
    request();
    // Preparation owns only its pose. A new physical/native grab, including one
    // of these same handles, retains its own input and release lifecycle.
    mirror_ready();legacy_request();held=f.grip;f.left[0x33d]=1;g_cmd_until[29]=clock_now+500;
    tick();assert(!tm_hold.phase && held==f.grip && get_down(f.left) && g_cmd_until[29]>clock_now && !pair_released[0]);
    // A missing native hand update times out; no input press is manufactured.
    mirror_ready();legacy_request();clock_now+=1501;tick();assert(!tm_hold.phase && !pair_pressed[0] && !pair_pressed[1]);
    trap_ready();joint_scale=.1f;request();assert(tm_hold.phase==2 && held && held_right);request();
    trap_ready();joint_scale=0;request();assert(!tm_hold.phase && !held && !held_right && !pair_pressed[0] && !pair_pressed[1]);
    puts("PASS: actual pair pose prepares both native hands before press; old remote trap pose fails release volume; no first-held-frame jump; native grab-center/dynamic offsets, timeout and cleanup");
}
static void test_pose_packet(void) {
    mirror_ready();legacy_request();float a[3],b[3],expected[3];
    assert(touch_mirror_pose(0,clock_now,a));memcpy(expected,tm_pose_plan.position[1],sizeof expected);
    tm_pose_plan.position[1][0]+=1;assert(touch_mirror_pose(1,clock_now,b) && !memcmp(b,expected,sizeof b));
    pose_frame++;assert(touch_mirror_pose(1,clock_now,b) && b[0]==expected[0]+1);
    // Render's callback only reads copied values, even if every cached UObject
    // becomes an unusable opaque address before GameThread retires the packet.
    void* saved=tm_hold.object[0];tm_hold.object[0]=(void*)1;assert(touch_mirror_pose(0,clock_now,a));tm_hold.object[0]=saved;
    assert(!touch_mirror_pose(0,clock_now+251,a) && !touch_mirror_pose(0,clock_now-1,a));
    g_touch_cancel=1;assert(!touch_mirror_pose(0,clock_now,a));g_touch_cancel=0;
    tm_release();assert(!touch_mirror_pose(0,clock_now,a));
    puts("PASS: copied pose packet is coherent for both nodes per engine frame, never dereferences UObject on render, and rejects stale/future/cancelled/retired data");
}
static void test_world_to_ovr(void) {
    for(int y=-2;y<=2;y++)for(int p=-1;p<=1;p++) {
        float ya=y*.4f,pa=p*.3f;
        struct reload_tf head={{0,0,sinf(ya*.5f),cosf(ya*.5f)},{-371,123,160},{1,1,1},{0,0}},target=head;
        target.p[0]+=37;target.p[1]-=23;target.p[2]-=53;
        qf_t raw_q=q_mul((qf_t){0,sinf(.7f*.5f),0,cosf(.7f*.5f)},(qf_t){sinf(pa*.5f),0,0,cosf(pa*.5f)});
        float raw_p[3]={.14f,1.6f,-.03f},actual[3],delta[3],local[3],ovr[3],rotated[3];
        tm_raw_position(target,head,raw_q,raw_p,actual);
        for(int i=0;i<3;i++)delta[i]=target.p[i]-head.p[i];q_rot((qf_t){-head.q.x,-head.q.y,-head.q.z,head.q.w},delta,local);
        ovr[0]=local[1]*.01f;ovr[1]=local[2]*.01f;ovr[2]=-local[0]*.01f;q_rot(raw_q,ovr,rotated);
        for(int i=0;i<3;i++)assert(fabsf(actual[i]-(raw_p[i]+rotated[i]))<.00001f);
    }
    puts("PASS: actual reload transform composition maps translated/rotated native world heads to OVR metres with independent axis/quaternion reference");
}
static void emit_pose_samples(void) {
    for(int kind=1;kind<=2;kind++)for(int control=0;control<3;control++)for(int sign=-1;sign<=1;sign++) {
        if(kind==2 && control==2)continue;
        if(kind==1)mirror_ready();else trap_ready();request();
        if(control==0)pose_yaw=sign*20*.01745329252f;
        else if(control==1)pose_pitch=sign*20*.01745329252f;
        else *(float*)(f.head+0x1d4)+=sign*10;
        for(int i=0;i<3;i++)native_pose_frame();
        printf("{\"kind\":%d,\"control\":%d,\"sign\":%d,\"positions\":[",kind,control,sign);
        for(int s=0;s<2;s++) {float* p=(float*)(pose_roots[s]+0x1d0);printf("%s[%.9g,%.9g,%.9g]",s?",":"",p[0]-(kind==1?142.15716552734375f:95),p[1],p[2]-(kind==1?0:107));}
        puts("]}");request();
    }
}
int main(int argc,char** argv) {
    if(argc==2 && !strcmp(argv[1],"--pose-samples")){emit_pose_samples();return 0;}
    ordinary_interact_tests(argc,argv);
#ifdef MIRROR_NATIVE_FIXTURE
    verify_mirror_install_guard();
#endif
    test_pose_barrier();test_pose_packet();test_world_to_ovr();
    mirror_ready();request();held_pair();
    for(int i=0;i<10;i++){clock_now+=100;tick();}held_pair();
    request();assert(!tm_hold.object[0] && !held && !held_right && pair_released[0]==1 && pair_released[1]==1);
    // Both original pawn-gun slots and knife may enter through native holstering.
    const int slots[]={0,1,3};
    for(unsigned i=0;i<3;i++) {
        mirror_ready();slot_held=slots[i];held_right=weapon_grip();f.right[0x33d]=1;g_grip_latched=1u<<27;
        request();assert(holsters==1 && !g_grip_latched && held==f.grip && held_right==f.foreign_grip);
        assert(pair_released[1]==1 && pair_pressed[0]==1 && pair_pressed[1]==1);
        int previous[2]={0,1};
        for(int frame=0;frame<60;frame++){controller_frame(previous);tick();clock_now+=16;}
        assert(tm_hold.object[0] && held==f.grip && held_right==f.foreign_grip);
        assert(pair_pressed[0]==1 && pair_pressed[1]==1 && pair_released[0]==0 && pair_released[1]==1);
        request();assert(!g_cmd_until[27] && !g_cmd_until[29] && !held && !held_right);
    }
    // Expired inputs are restored, and a newer command retires the pair without
    // overwriting that command or leaving the other hand held indefinitely.
    mirror_ready();long prior_left=clock_now-50,prior_right=clock_now-20;
    g_cmd_until[29]=prior_left;g_cmd_until[27]=prior_right;request();
    request();assert(g_cmd_until[29]==prior_left && g_cmd_until[27]==prior_right);
    for(int s=0;s<2;s++) {
        mirror_ready();request();long foreign=clock_now+700;
        g_cmd_until[s?27:29]=foreign;tick();
        assert(!tm_hold.object[0] && !held && !held_right && g_cmd_until[s?27:29]==foreign && !g_cmd_until[s?29:27]);
    }
    // Native validity/LOS/nearness are resolved before putting a weapon away.
    for(int s=0;s<2;s++) {
        mirror_ready();slot_held=0;held_right=weapon_grip();pair_wall[s]=1;
        assert(!touch_mirror_interact(f.pawn,clock_now,0) && !holsters && held_right==weapon_grip());
        mirror_ready();slot_held=0;held_right=weapon_grip();pair_valid[s]=0;
        assert(!touch_mirror_interact(f.pawn,clock_now,0) && !holsters && held_right==weapon_grip());
    }
    mirror_ready();slot_held=0;held_right=weapon_grip();*(float*)(f.head+0x1d0)=-1000;
    assert(!touch_mirror_interact(f.pawn,clock_now,0) && !holsters);
    // Original Check wins when entering; another active world grip keeps its release path.
    mirror_ready();action=can_action=1;f.manager[0x3d9]=1;f.manager[0x3da]=1;request();
    assert(g_cmd_until[0]>clock_now && !tm_hold.object[0] && !pair_pressed[0] && !holsters);
    mirror_ready();held=f.grip;f.left[0x33d]=1;ti_hand=f.left;ti_pawn=f.pawn;ti_grip=f.grip;ti_side=0;request();
    assert(!held && !ti_hand && pair_released[0]==1 && !pair_pressed[1]);
    mirror_ready();held=f.grip;f.left[0x33d]=1;request();assert(!held && pair_released[0]==1 && !pair_pressed[1]);
    // Pause, cancel and a native ownership change release only this adapter's pair.
    for(int mode=0;mode<4;mode++) {
        mirror_ready();request();
        touch_interact_tick(f.inactive,0,clock_now);held_pair();
        if(mode==0)paused=1;else if(mode==1)g_touch_cancel=1;else if(mode==2)active=f.other;
        else {held_right=weapon_grip();f.right[0x33d]=1;}
        tick();assert(!tm_hold.object[0] && !held && pair_released[0]==1);
        if(mode==3)assert(held_right==weapon_grip() && get_down(f.right) && !pair_released[1]);
        else assert(!held_right && pair_released[1]==1);
    }
    mirror_ready();request();weak_serial[tm_hold.weak[4].index]++;
    weak_serial[tm_hold.weak[4].index]=-1;tick();assert(!tm_hold.object[0] && !get_down(f.left) && !get_down(f.right));
    // Failed second press rolls back the first; an unexpected new native hold is untouched.
    mirror_ready();pair_accept[1]=0;request();
    assert(!tm_hold.object[0] && !held && !held_right && pair_released[0]==1 && pair_released[1]==1 && !g_cmd_until[27] && !g_cmd_until[29]);
    mirror_ready();foreign_after_press=1;request();
    assert(!tm_hold.object[0] && !held && held_right==f.pawn && !pair_released[1]);
    mirror_ready();change_owner_on_press=1;request();
    assert(!tm_hold.object[0] && !held && !held_right && pair_pressed[0]==1 && !pair_pressed[1]);
    // Context/input owners prevent any new paired input.
    for(int mode=0;mode<7;mode++) {
        mirror_ready();
        if(mode==0)g_reload_phase=1;else if(mode==1)g_touch_reload_request=1;else if(mode==2)qs_busy=1;
        else if(mode==3)g_touch_weapon_request=2;else if(mode==4)*(int*)(f.right+0x468)=1;
        else if(mode==5)g_cmd_until[27]=clock_now+1;else g_grip_latched=1u<<29;
        assert(!touch_mirror_interact(f.pawn,clock_now,0) && !pair_pressed[0] && !pair_pressed[1] && !holsters);
    }
    // The dog trap uses its native TrapJaws records, not the mirror field layout.
    for(int reverse=0;reverse<2;reverse++) {
        trap_ready();trap_reverse=reverse;request();
        assert(tm_hold.object[0]==f.pawn && held==(reverse?f.foreign_grip:f.grip)
            && held_right==(reverse?f.grip:f.foreign_grip) && pair_pressed[0]==1 && pair_pressed[1]==1);
        request();assert(!held && !held_right && pair_released[0]==1 && pair_released[1]==1);
    }
    for(int mode=0;mode<7;mode++) {
        trap_ready();slot_held=0;held_right=weapon_grip();
        if(mode==0)trap_dog=0;else if(mode==1)*(int*)(f.parent+0x220)=1;
        else if(mode==2)*(int*)(f.parent+0x224)=1;else if(mode==3)*(void**)(f.parent+0x218)=0;
        else if(mode==4)trap_registered[1]=f.grip;else if(mode==5)*(int*)(f.parent+0x208)=1;
        else *(int*)(f.parent+0x20c)=17;
        assert(!touch_mirror_interact(f.pawn,clock_now,0) && !holsters && held_right==weapon_grip());
    }
    trap_ready();slot_held=0;held_right=weapon_grip();request();
    assert(holsters==1 && held==f.grip && held_right==f.foreign_grip);
    g_touch_cancel=1;tick();assert(!tm_hold.object[0] && !held && !held_right);
    mirror_ready();request();g_touch_context=2;touch_mirror_idle_tick();
    assert(!tm_hold.object[0] && !held && !held_right && pair_released[0]==1 && pair_released[1]==1);
    puts("PASS: actual mirror pair helper with native callback priority, two grip presses, rollback, weak ownership, cancellation and scoped pawn-weapon holstering");
    return 0;
}
