#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

typedef struct {float x,y,z,w;} qf_t;
struct __attribute__((aligned(16))) reload_tf {qf_t q;float p[3],s[3],padding[2];};
static qf_t q_mul(qf_t a,qf_t b) {
    return (qf_t){a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
static qf_t fixture_unit(qf_t q) {
    float n=sqrtf(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);assert(n>0);
    return (qf_t){q.x/n,q.y/n,q.z/n,q.w/n};
}
static void q_rot(qf_t q,const float v[3],float out[3]) {
    float t[3]={2*(q.y*v[2]-q.z*v[1]),2*(q.z*v[0]-q.x*v[2]),2*(q.x*v[1]-q.y*v[0])};
    out[0]=v[0]+q.w*t[0]+q.y*t[2]-q.z*t[1];out[1]=v[1]+q.w*t[1]+q.z*t[0]-q.x*t[2];out[2]=v[2]+q.w*t[2]+q.x*t[1]-q.y*t[0];
}
static int reload_tf_valid(const struct reload_tf* tf) {
    float n=tf->q.x*tf->q.x+tf->q.y*tf->q.y+tf->q.z*tf->q.z+tf->q.w*tf->q.w;
    if(!isfinite(n)||fabsf(n-1)>0.01f)return 0;
    for(int i=0;i<3;i++)if(!isfinite(tf->p[i])||!isfinite(tf->s[i])||fabsf(tf->s[i]-1)>0.01f)return 0;
    return 1;
}
static int reload_layout(const void* object,int min) {
    const char* cls=object?*(const char* const*)((const char*)object+0x10):0;
    int size=cls?*(const int*)(cls+0x58):0;return size>=min&&size<=0x10000;
}
static int reload_is_a(const void* object,const void* expected) {
    const char* cls=object?*(const char* const*)((const char*)object+0x10):0;
    if(!cls||!expected)return 0;
    int d=*(const int*)((const char*)expected+0x38),child=*(const int*)(cls+0x38);
    const void* const* chain=*(const void* const* const*)(cls+0x30);
    return d>=0&&d<=128&&child>=d&&child<=256&&chain&&chain[d]==(const char*)expected+0x30;
}
static int reload_grip_registered(void*);
static void* (*tw_hand_class)(void),*(*tw_pawn_class)(void),*(*tw_grip_class)(void);
static void* (*phone_player)(const void*),*(*phone_active)(void),*(*rl_left)(const void*),*(*rl_primary)(const void*),*(*f_GetCurrentGrip)(const void*);
static unsigned (*phone_type)(void);
static int (*rl_paused)(void),(*rl_player_valid)(void),(*rl_interactivity)(const void*),(*rl_reloading)(void*);
static void (*rl_world_transform)(void*,const struct reload_tf*,int,void*,int),(*f_SetGripForcedHover)(void*,void*,int);
static void* (*ti_parent)(const void*),*(*ti_best)(const void*);
static int (*ti_free)(const void*,int),(*ti_down)(const void*);
static void (*ti_press)(void*),(*ti_release)(void*);
static int ti_ready,g_touch_gt_tid=41,g_touch_context,g_touch_cancel,g_touch_weapon_request,g_touch_reload_request,g_touch_interact_request,g_reload_phase;
static int g_touch_inventory_request,g_touch_pause;
static unsigned g_grip_latched;
static long g_cmd_until[32],fixture_clock;
static int fixture_tid=41;
static int fixture_menu;
#define gettid() fixture_tid
static long ptr_now_ms(void){return fixture_clock;}
static int ptr_menu_now(void){return fixture_menu;}
#define LOGF(...) ((void)0)
static int fixture_patch_calls,fixture_patch_failure;
static int patch_code(void* target,void* hook){assert(target&&hook);fixture_patch_calls++;return fixture_patch_failure;}
#if !defined(__aarch64__)
static void gate_wc_shotgun_pre(void* gun,float dt);
#endif
#include "../re4fix_weapon_cycle.h"
#ifdef CYCLE_NATIVE_FIXTURE
#include CYCLE_NATIVE_FIXTURE
#endif

static struct {
    unsigned char left[0x600],right[0x600],inactive[0x600],pawn[0x1600],gun[0x1300],group[0x280];
    unsigned char primary[0x460],forest[0x460],forest2[0x460],foreign[0x460];
    unsigned char roots[4][0x200],head[0x200],classes[6][0x100],settings[0xb8];
    void* chains[6][1];void* vtable[0x748/8];void* forced[8];void* group_items[4];
} f __attribute__((aligned(16)));
static void* equipped,*active_pawn,*left_grip,*forest_hand,*best_result;
static int down,paused,valid,interactivity,reloading,recoil,ammo_held,blending,flourishing,registered,free_grip,in_parent,accept;
static int world_calls,press_calls,release_calls,hover_adds,hover_removes,pulls,pushes,can_accept,cancel_best,expire_best,rename_best,cancel_valid,valid_calls;
static int native_distance_releases,tracking_enabled;
static int follow_native_attachment,primary_world_calls,primary_set_cancel;
static int native_pre_calls;
static float native_interstage_pitch;
static int cancel_on_release;
static struct wc_vec local_forestock(const void*,float*,const void*);
static struct reload_tf* tf(void* root){return (struct reload_tf*)((char*)root+0x1c0);}
static void* hand_class(void){return f.classes[0];}static void* pawn_class(void){return f.classes[1];}
static void* grip_class(void){return f.classes[2];}static void* gun_class(void){return f.classes[3];}
static void* scene_class(void){return f.classes[4];}static void* group_class(void){return f.classes[5];}
static void* player(const void* h){return h==f.left||h==f.right ? f.pawn:0;}
static void* active(void){return active_pawn;}static unsigned type(void){return g_touch_context;}
static void* left(const void* p){assert(p==f.pawn);return f.left[0x314]?f.right:f.left;}static void* right(const void* p){assert(p==f.pawn);return f.left[0x314]?f.left:f.right;}
static void* primary(const void* g){assert(g==f.gun);return f.right;}
static void* current(const void* h){return h==f.right?f.primary:h==f.left?left_grip:0;}
static void* touch_equipped_gun(void* h){return h==f.right?equipped:0;}
static const void* settings(void){return f.settings;}static const void* group_grips(const void* g){assert(g==f.group);return f.group+0x1f8;}
static int is_paused(void){return paused;}static int is_valid(void){return valid;}static int interactive(const void* g){assert(g==f.gun);return interactivity;}
static int is_reloading(void* g){assert(g==f.gun);return reloading;}static int is_recoil(const void* g){assert(g==f.gun);return recoil;}
static int held_ammo(const void* g,float time){assert(g==f.gun&&time==0);return ammo_held;}
static int is_blend(const void* g){assert(g==f.gun);return blending;}static int is_flourish(const void* g){assert(g==f.gun);return flourishing;}
static int in_valid(const void* g){assert(g==f.forest||g==f.forest2);valid_calls++;if(cancel_valid&&valid_calls>=cancel_valid)g_touch_cancel=1;return in_parent;}
static int is_free(const void* g,int side){assert(side==f.left[0x314]&&(g==f.forest||g==f.forest2));return free_grip;}
static int is_down(const void* h){assert(h==f.left);return down;}static void* parent(const void* g){return *(void* const*)((const char*)g+0x300);}
static int can_props(const void* h){assert(h==f.left);return can_accept;}
static void* forestock(const void* g){assert(g==f.gun);return forest_hand;}
static void* best(const void* h){assert(h==f.left);if(cancel_best)g_touch_cancel=1;if(expire_best)fixture_clock=2800;
    if(rename_best)*(uint64_t*)(f.forest+0x18)^=1ULL<<40;return best_result;}
static int reload_grip_registered(void* g){return (g==f.primary&&(registered&1))||(g==f.forest&&(registered&2))||(g==f.forest2&&(registered&4));}
static struct reload_tf target(const void* g){assert(g==f.forest||g==f.forest2);return *tf(f.roots[3]);}
static struct wc_vec grab_location(const void* h){assert(h==f.left);struct reload_tf t=*tf(f.roots[0]);return (struct wc_vec){t.p[0]+2,t.p[1],t.p[2]};}
static struct wc_vec endpoint(const void* g,int end){assert(g==f.gun);int off=f.gun[0x1161]?end?0x1118:0x10f4:end?0x1124:0x1100;
    struct wc_vec v;memcpy(&v,f.gun+off,12);return v;}
static struct wc_vec start(const void* g){return endpoint(g,0);}static struct wc_vec end(const void* g){return endpoint(g,1);}
static void set_world(void* root,const struct reload_tf* p,int sweep,void* hit,int teleport){
    assert((root==f.roots[0]||root==f.roots[2])&&!sweep&&!hit&&!teleport&&reload_tf_valid(p));world_calls++;
    if(root==f.roots[2]) {
        assert(follow_native_attachment);primary_world_calls++;
        struct reload_tf old=*tf(root);qf_t delta=q_mul(p->q,(qf_t){-old.q.x,-old.q.y,-old.q.z,old.q.w});
        // Original Prop::PrimaryGripGrabbed attaches the gun to this actor.
        // Its forestock target follows it, but the other tracking root does not.
        for(int i=0;i<2;i++) {
            struct reload_tf* attached=tf(f.roots[i?3:1]);float v[3],w[3];
            for(int j=0;j<3;j++)v[j]=attached->p[j]-old.p[j];q_rot(delta,v,w);
            for(int j=0;j<3;j++)attached->p[j]=p->p[j]+w[j];attached->q=q_mul(delta,attached->q);
        }
        if(primary_set_cancel)g_touch_cancel=1;
    }
    *tf(root)=*p;
}
static void set_hover(void* h,void* g,int on){assert(h==f.left&&g==f.forest);int* n=(int*)(f.left+0x468);
    if(on){assert(*n==0);f.forced[(*n)++]=g;hover_adds++;}else{for(int i=0;i<*n;i++)if(f.forced[i]==g){memmove(f.forced+i,f.forced+i+1,(size_t)(--*n-i)*8);break;}hover_removes++;}}
static void press(void* h){assert(h==f.left&&!left_grip&&!down&&best_result==f.forest&&free_grip&&in_parent);press_calls++;down=1;
    if(!accept)return;left_grip=f.forest;forest_hand=f.left;struct reload_tf t=*tf(f.roots[0]),gun=*tf(f.roots[1]);
    float delta[3],local[3];for(int i=0;i<3;i++)delta[i]=t.p[i]-gun.p[i];qf_t inv={-gun.q.x,-gun.q.y,-gun.q.z,gun.q.w};q_rot(inv,delta,local);
    // Original STR D0 [gun+10f4+88] and STR S1 [gun+1184]:
    // the native FVector is at 117c,1180,1184, without 16-byte padding.
    struct wc_vec a=start(f.gun);float begin[3]={a.x,a.y,a.z};for(int i=0;i<3;i++)*(float*)(f.gun+0x117c+4*i)=local[i]-begin[i];
    if(f.gun[0x1160]==1){struct wc_vec z=end(f.gun);float finish[3]={z.x,z.y,z.z};for(int i=0;i<3;i++)*(float*)(f.gun+0x117c+4*i)=local[i]-finish[i];}
    if(f.gun[0x113c]){pushes++;f.gun[0x1160]=f.gun[0x1161]=0;}
}
static void release(void* h){assert(h==f.left&&down&&(left_grip==f.forest||!left_grip));release_calls++;down=0;left_grip=forest_hand=0;if(cancel_on_release)g_touch_cancel=1;}
static void ready(void){
    memset(&f,0,sizeof f);memset(&wc_cycle,0,sizeof wc_cycle);memset(&wc_return,0,sizeof wc_return);memset(g_cmd_until,0,sizeof g_cmd_until);
    touch_weapon_cycle_input_reset();
    int sizes[6]={0x600,0x1600,0x460,0x1300,0x200,0x280};
    for(int i=0;i<6;i++){*(int*)(f.classes[i]+0x58)=sizes[i];f.chains[i][0]=f.classes[i]+0x30;*(void**)(f.classes[i]+0x30)=f.chains[i];}
    unsigned char* objects[11]={f.left,f.right,f.pawn,f.gun,f.primary,f.forest,f.roots[0],f.roots[1],f.roots[2],f.group,f.roots[3]};
    int cls[11]={0,0,1,3,2,2,4,4,4,5,4};
    for(int i=0;i<11;i++){*(void**)(objects[i]+0x10)=f.classes[cls[i]];*(uint64_t*)(objects[i]+0x18)=(uint64_t)i+1;}
    *(void**)(f.head+0x10)=f.classes[4];*(void**)(f.head+0x20)=f.pawn;*(uint64_t*)(f.head+0x18)=12;
    *(void**)(f.pawn+0x2d8)=f.head;*tf(f.head)=(struct reload_tf){{0,0,0,1},{0,0,0},{1,1,1},{0,0}};
    *(void**)(f.inactive+0x10)=f.classes[0];*(void**)(f.forest2+0x10)=f.classes[2];
    f.right[0x314]=1;f.settings[0x3d]=1;
    *(void**)(f.gun+0x3c0)=f.pawn;*(void**)(f.gun+0x270)=f.primary;*(void**)(f.gun+0xd38)=f.group;*(void**)(f.gun+0xd40)=f.forest;
    unsigned char* grips[3]={f.primary,f.forest,f.forest2};for(int i=0;i<3;i++){*(void**)(grips[i]+0x20)=*(void**)(grips[i]+0x300)=f.gun;*(void**)(grips[i]+0x230)=f.roots[3];}
    *(void**)(f.group+0x20)=f.gun;*(void**)(f.group+0x1f8)=f.group_items;*(int*)(f.group+0x204)=4;
    *(void**)(f.left+0x130)=f.roots[0];*(void**)(f.gun+0x130)=f.roots[1];*(void**)(f.right+0x130)=f.roots[2];
    void* owners[4]={f.left,f.gun,f.right,f.gun};for(int i=0;i<4;i++){*(void**)(f.roots[i]+0x20)=owners[i];*tf(f.roots[i])=(struct reload_tf){{0,0,0,1},{0,0,0},{1,1,1},{0,0}};}
    struct wc_vec a={30,10,-10},z={20,10,-10};memcpy(f.gun+0x10f4,&a,12);memcpy(f.gun+0x1118,&z,12);memcpy(f.gun+0x1100,&a,12);memcpy(f.gun+0x1124,&z,12);
    memcpy(tf(f.roots[3])->p,&a,12);tf(f.roots[0])->p[0]=-20;f.gun[0x1161]=1;*(int*)(f.gun+0xd90)=7;
    *(void**)(f.left+0x460)=f.forced;*(int*)(f.left+0x46c)=8;*(void**)f.left=f.vtable;f.vtable[0x740/8]=can_props;
    tw_hand_class=hand_class;tw_pawn_class=pawn_class;tw_grip_class=grip_class;
    phone_player=player;phone_active=active;phone_type=type;rl_left=left;rl_primary=primary;f_GetCurrentGrip=current;
    rl_paused=is_paused;rl_player_valid=is_valid;rl_interactivity=interactive;rl_reloading=is_reloading;
    rl_world_transform=set_world;f_SetGripForcedHover=set_hover;ti_parent=parent;ti_best=best;ti_free=is_free;ti_down=is_down;ti_press=press;ti_release=release;
    wc_shotgun_class=gun_class;wc_scene_class=scene_class;wc_group_class=group_class;wc_right=right;wc_forestock_hand=forestock;
    wc_settings=settings;wc_group_grips=group_grips;wc_in_valid_grips=in_valid;wc_recoiling=is_recoil;
    wc_ammo_held=held_ammo;wc_blending=is_blend;wc_flourishing=is_flourish;
    wc_grip_target=target;wc_grab_location=grab_location;wc_start=start;wc_end=end;
    wc_local_forestock=local_forestock;
    fixture_tid=g_touch_gt_tid=41;g_touch_context=1;g_touch_cancel=g_touch_weapon_request=g_touch_reload_request=g_touch_interact_request=g_reload_phase=0;
    g_touch_inventory_request=g_touch_pause=0;g_grip_latched=0;equipped=f.gun;active_pawn=f.pawn;left_grip=forest_hand=0;best_result=f.forest;
    down=paused=reloading=recoil=ammo_held=blending=flourishing=0;valid=free_grip=in_parent=accept=can_accept=1;interactivity=2;registered=7;
    world_calls=press_calls=release_calls=hover_adds=hover_removes=pulls=pushes=cancel_best=expire_best=rename_best=cancel_valid=valid_calls=0;wc_logs=0;fixture_clock=1000;
    native_distance_releases=0;tracking_enabled=1;fixture_menu=0;
    follow_native_attachment=primary_world_calls=primary_set_cancel=0;
    native_pre_calls=0;wc_native_pose_logs=0;
    native_interstage_pitch=0;
    cancel_on_release=0;
}
static float native_local_fraction(void){struct reload_tf h=*tf(f.roots[0]),g=*tf(f.roots[1]);float delta[3],local[3];
    for(int i=0;i<3;i++)delta[i]=h.p[i]-g.p[i];qf_t inv={-g.q.x,-g.q.y,-g.q.z,g.q.w};q_rot(inv,delta,local);
    struct wc_vec a=start(f.gun),z=end(f.gun);float begin[3]={a.x,a.y,a.z},finish[3]={z.x,z.y,z.z},dot=0,length=0;
    for(int i=0;i<3;i++){float d=finish[i]-begin[i];local[i]-=*(float*)(f.gun+0x117c+4*i);dot+=(local[i]-begin[i])*d;length+=d*d;}
    return fminf(1,fmaxf(0,dot/length));
}
static struct wc_vec local_forestock(const void* gun,float* parameter,const void* hand) {
    assert(gun==f.gun&&hand==f.left&&parameter);*parameter=native_local_fraction();return (struct wc_vec){0,0,0};
}
static void original_native_pre(void* gun,float dt) {
    assert(gun==f.gun&&fabsf(dt-.01f)<.000001f);native_pre_calls++;
    if(left_grip!=f.forest||forest_hand!=f.left)return;
    float fraction=native_local_fraction();
    if(f.gun[0x1160]==0&&f.gun[0x1161]==1&&fraction==1){pulls++;f.gun[0x1160]=1;f.gun[0x1161]=2;}
    else if(f.gun[0x1160]==1&&fraction==0){pushes++;f.gun[0x1160]=0;f.gun[0x1161]=*(int*)(f.gun+0xd90)>0?0:2;}
}
#if !defined(__aarch64__)
static void gate_wc_shotgun_pre(void* gun,float dt) {original_native_pre(gun,dt);}
#endif
static void native_frame(void){
    // The native hand/input update precedes the phone post-Tick adapter. A
    // released controller grip clears its hold before the pump can progress.
    unsigned bit=f.left[0x314]?27:29;
    unsigned held=touch_weapon_cycle_input_mask(fixture_clock,0);
    if(down&&g_cmd_until[bit]<=fixture_clock&&!(g_grip_latched&(1u<<bit))&&!(held&(1u<<bit)))release(f.left);
    if(left_grip!=f.forest||forest_hand!=f.left)return;
    if(tracking_enabled){
        // The original tracking update replaces the hand root before native
        // GrabTick. Read the same OVR publication consumed by production h5.
        float off[3]={-.18f,-.20f,.20f};touch_weapon_cycle_pose(f.left[0x314],fixture_clock,off);
        float local[3]={-off[2]*100,off[0]*100,off[1]*100},world[3];q_rot(tf(f.head)->q,local,world);
        for(int i=0;i<3;i++)tf(f.roots[0])->p[i]=tf(f.head)->p[i]+world[i];
        struct wc_vec grab=grab_location(f.left);float point[3]={grab.x,grab.y,grab.z},distance=0;
        for(int i=0;i<3;i++){float d=point[i]-tf(f.roots[3])->p[i];distance+=d*d;}
        // This fixture's release curve expires immediately beyond 25cm.
        // Like native GripIKTarget::GrabTick it releases the grip, not input.
        if(distance>25.0f*25.0f){native_distance_releases++;left_grip=forest_hand=0;return;}
    }
    if(native_interstage_pitch) {
        // Original UpdateTwoHandedGrip calls SetActorRotation independently
        // of the primary hand, after the preceding phone post-hand placement.
        struct reload_tf before=*tf(f.roots[1]);
        qf_t turn={0,sinf(native_interstage_pitch/2),0,cosf(native_interstage_pitch/2)};
        tf(f.roots[1])->q=fixture_unit(q_mul(before.q,turn));
        float local[3],world[3],delta[3];
        for(int i=0;i<3;i++)delta[i]=tf(f.roots[3])->p[i]-before.p[i];
        q_rot((qf_t){-before.q.x,-before.q.y,-before.q.z,before.q.w},delta,local);
        q_rot(tf(f.roots[1])->q,local,world);
        for(int i=0;i<3;i++)tf(f.roots[3])->p[i]=before.p[i]+world[i];
        tf(f.roots[3])->q=q_mul(tf(f.roots[3])->q,turn);
    }
    hook_wc_shotgun_pre(f.gun,.01f);
}
static void tick(long now){fixture_clock=now;touch_weapon_cycle_tick(f.left,f.left[0x314],now);assert(*(int*)(f.gun+0xd90)==7);}
static void begin(void){ready();tick(1000);assert(press_calls==1&&wc_cycle.phase==1&&down&&left_grip==f.forest&&hover_adds==1);}
static void test_cycle(void){
    begin();for(long now=1010;now<1600&&touch_weapon_cycle_busy();now+=10){native_frame();tick(now);}assert(pulls==1&&pushes==1&&release_calls==1&&!down&&!left_grip&&!touch_weapon_cycle_busy()&&hover_removes==1);
    tick(1700);assert(press_calls==1);f.gun[0x1161]=1;tick(1800);assert(press_calls==2&&touch_weapon_cycle_busy());
    begin();g_touch_cancel=1;tick(1020);assert(!touch_weapon_cycle_busy()&&release_calls==1&&hover_removes==1);
    begin();g_touch_context=2;tick(1020);assert(!touch_weapon_cycle_busy()&&release_calls==1);
    begin();g_touch_inventory_request=1;tick(1020);assert(!touch_weapon_cycle_busy()&&release_calls==1);
    g_touch_inventory_request=0;tick(1100);assert(press_calls==1);
    tick(1480);assert(touch_weapon_cycle_busy()&&press_calls==2); // Same spent chamber resumes after inventory cancellation.
    for(long time=1490;time<2200&&touch_weapon_cycle_busy();time+=10){native_frame();tick(time);}
    assert(pulls==1&&pushes==1&&release_calls==2&&!touch_weapon_cycle_busy());
    begin();touch_weapon_cycle_cancel(f.right);assert(touch_weapon_cycle_busy());touch_weapon_cycle_cancel(f.left);assert(!touch_weapon_cycle_busy()&&release_calls==1);
    begin();tick(999);assert(!touch_weapon_cycle_busy()&&release_calls==1);tick(1100);assert(press_calls==1);
    begin();tick(2800);assert(!touch_weapon_cycle_busy()&&release_calls==1);tick(3000);assert(press_calls==1);
    touch_weapon_cycle_tick(f.right,1,3010);tick(3020);assert(press_calls==1&&wc_cycle.failed);
    begin();left_grip=f.foreign;tick(1020);assert(!touch_weapon_cycle_busy()&&!release_calls&&down&&left_grip==f.foreign&&hover_removes==1);
    begin();left_grip=forest_hand=0;tick(1020);assert(!touch_weapon_cycle_busy()&&release_calls==1&&!down);
    begin();g_cmd_until[29]=1100;tick(1020);assert(!touch_weapon_cycle_busy()&&!release_calls&&down&&left_grip==f.forest);
    begin();g_grip_latched=0x20000000u;tick(1020);assert(!touch_weapon_cycle_busy()&&!release_calls&&down&&left_grip==f.forest);
    begin();f.forced[1]=f.foreign;*(int*)(f.left+0x468)=2;tick(1020);assert(!touch_weapon_cycle_busy()&&release_calls==1&&*(int*)(f.left+0x468)==1&&f.forced[0]==f.foreign);
    begin();fixture_tid=42;tick(1020);assert(touch_weapon_cycle_busy()&&!release_calls);fixture_tid=41;
    touch_weapon_cycle_tick(f.inactive,0,1020);assert(touch_weapon_cycle_busy()&&!release_calls);
    begin();for(int i=0;i<12;i++){begin();*(uint64_t*)((char*)wc_cycle.binding.object[i]+0x18)^=1ULL<<40;tick(1020);assert(!touch_weapon_cycle_busy());if(i!=0&&i!=5)assert(release_calls==1);}
}
static void test_held_input(void){
    for(int side=0;side<2;side++){
        ready();f.left[0x314]=(unsigned char)side;f.right[0x314]=(unsigned char)(1-side);
        unsigned bit=side?27:29;g_cmd_until[bit]=900;tick(1000);
        assert(down&&g_cmd_until[bit]==900&&touch_weapon_cycle_busy());
        assert(touch_weapon_cycle_input_mask(1000,0)==(1u<<bit));
        assert(!touch_weapon_cycle_input_mask(1000,1)&&!touch_weapon_cycle_input_mask(999,0)&&!touch_weapon_cycle_input_mask(1251,0));
        int* gates[]={&g_touch_cancel,&g_touch_weapon_request,&g_touch_reload_request,&g_touch_interact_request,
            &g_touch_inventory_request,&g_touch_pause};
        for(unsigned i=0;i<sizeof gates/sizeof *gates;i++){
            *gates[i]=1;assert(!touch_weapon_cycle_input_mask(1000,0));*gates[i]=0;
        }
        for(int context=2;context<=7;context++){
            g_touch_context=context;assert(!touch_weapon_cycle_input_mask(1000,0));
        }
        g_touch_context=1;g_reload_phase=1;assert(!touch_weapon_cycle_input_mask(1000,0));g_reload_phase=0;
        long finished=0;
        for(long time=1010;time<1700&&touch_weapon_cycle_busy();time+=10){
            fixture_clock=time;native_frame();assert(down);tick(time);finished=time;
        }
        assert(finished>=1430&&pulls==1&&pushes==1&&release_calls==1&&!down&&!native_distance_releases);
        assert(!touch_weapon_cycle_busy()&&g_cmd_until[bit]==900&&!touch_weapon_cycle_input_mask(finished,0));
        ready();f.left[0x314]=(unsigned char)side;f.right[0x314]=(unsigned char)(1-side);tick(1000);
        g_touch_inventory_request=1;tick(1020);assert(!touch_weapon_cycle_busy()&&!g_cmd_until[bit]&&!down);
        ready();f.left[0x314]=(unsigned char)side;f.right[0x314]=(unsigned char)(1-side);tick(1000);
        g_cmd_until[bit]=9999;tick(1020);
        assert(!touch_weapon_cycle_busy()&&g_cmd_until[bit]==9999&&down&&left_grip==f.forest&&!release_calls);
        ready();f.left[0x314]=(unsigned char)side;f.right[0x314]=(unsigned char)(1-side);tick(1000);
        touch_weapon_cycle_input_reset();assert(!touch_weapon_cycle_input_mask(1000,0));
        fixture_clock=1016;native_frame();tick(1016);
        assert(!touch_weapon_cycle_busy()&&!down&&release_calls==1);
    }
    begin();__atomic_store_n(&wc_tracking_pose.stamp,0,__ATOMIC_RELEASE);fixture_clock=1016;native_frame();
    assert(native_distance_releases==1&&down&&!left_grip&&touch_weapon_cycle_input_mask(1016,0));
    tick(1016);assert(!touch_weapon_cycle_busy()&&!down&&release_calls==1);
}
static void test_tracking_pose(void){
    for(int side=0;side<2;side++)for(int frame=0;frame<60;frame++){
        ready();f.left[0x314]=(unsigned char)side;f.right[0x314]=(unsigned char)(1-side);
        float a=.071f*frame,b=.013f*frame;
        qf_t rotation={-sinf(a/2)*sinf(b/2),cosf(a/2)*sinf(b/2),sinf(a/2)*cosf(b/2),cosf(a/2)*cosf(b/2)};
        tf(f.head)->q=tf(f.roots[1])->q=rotation;
        for(int i=0;i<3;i++)tf(f.head)->p[i]=tf(f.roots[1])->p[i]=tf(f.roots[2])->p[i]=(float)(i+1)*frame;
        float target_world[3];q_rot(rotation,(float[3]){30,10,-10},target_world);
        for(int i=0;i<3;i++)tf(f.roots[3])->p[i]=tf(f.head)->p[i]+target_world[i];
        tick(1000);assert(touch_weapon_cycle_busy());
        float off[3]={9,8,7},original[3];memcpy(original,off,sizeof off);
        touch_weapon_cycle_pose(1-side,1000,off);assert(!memcmp(off,original,sizeof off));
        touch_weapon_cycle_pose(side,999,off);assert(!memcmp(off,original,sizeof off));
        touch_weapon_cycle_pose(side,1251,off);assert(!memcmp(off,original,sizeof off));
        fixture_menu=1;touch_weapon_cycle_pose(side,1000,off);fixture_menu=0;assert(!memcmp(off,original,sizeof off));
        touch_weapon_cycle_pose(side,1000,off);
        float local[3]={-100*off[2],100*off[0],100*off[1]},world[3];q_rot(rotation,local,world);
        for(int i=0;i<3;i++)assert(fabsf(tf(f.head)->p[i]+world[i]-tf(f.roots[0])->p[i])<.0001f);
        for(long time=1010;time<1700&&touch_weapon_cycle_busy();time+=10){fixture_clock=time;native_frame();tick(time);}
        assert(!touch_weapon_cycle_busy()&&pulls==1&&pushes==1&&!native_distance_releases&&release_calls==1&&!down);
    }
    ready();*(void**)(f.head+0x20)=f.gun;tick(1000);assert(!touch_weapon_cycle_busy()&&!press_calls);
    ready();tf(f.head)->q.w=NAN;tick(1000);assert(!touch_weapon_cycle_busy()&&!press_calls);
    ready();tf(f.head)->p[0]=10000;tick(1000);assert(!touch_weapon_cycle_busy()&&!press_calls);
}
static void test_refusals(void){
    int* blockers[]={&g_touch_cancel,&g_touch_weapon_request,&g_touch_reload_request,&g_touch_interact_request,&g_touch_inventory_request,&g_touch_pause,&g_reload_phase,&paused,&reloading,&recoil,&ammo_held,&blending,&flourishing};
    for(unsigned i=0;i<sizeof blockers/sizeof *blockers;i++){ready();*blockers[i]=1;tick(1000);assert(!press_calls&&!world_calls);}
    ready();f.settings[0x3d]=0;tick(1000);assert(!press_calls&&!world_calls);
    ready();valid=0;tick(1000);assert(!press_calls&&!world_calls);
    ready();interactivity=1;tick(1000);assert(!press_calls&&!world_calls);
    ready();g_grip_latched=0x20000000u;tick(1000);assert(!press_calls&&!world_calls);
    ready();g_cmd_until[29]=1100;tick(1000);assert(!press_calls&&!world_calls);
    ready();left_grip=f.foreign;tick(1000);assert(!press_calls&&!world_calls);
    ready();down=1;tick(1000);assert(!press_calls&&!world_calls);
    ready();f.forced[0]=f.foreign;*(int*)(f.left+0x468)=1;tick(1000);assert(!press_calls&&!world_calls);
    ready();in_parent=0;tick(1000);assert(!press_calls&&!world_calls);
    ready();free_grip=0;tick(1000);assert(!press_calls&&!world_calls);
    ready();can_accept=0;tick(1000);assert(!press_calls&&!world_calls);
    ready();registered=1;tick(1000);assert(!press_calls&&!world_calls);
    ready();*(int*)(f.classes[3]+0x58)=0x119f;tick(1000);assert(!press_calls&&!world_calls);
    ready();*(void**)(f.forest+0x300)=f.pawn;tick(1000);assert(!press_calls&&!world_calls);
    ready();*(void**)(f.roots[3]+0x20)=f.pawn;tick(1000);assert(!press_calls&&!world_calls);
    ready();active_pawn=0;tick(1000);assert(!press_calls&&!world_calls);
    ready();f.gun[0x1161]=0;tick(1000);assert(!press_calls&&!world_calls);
    ready();f.gun[0x1161]=2;tick(1000);assert(!press_calls&&!world_calls);
    ready();best_result=f.foreign;tick(1000);assert(!press_calls&&hover_adds==1&&hover_removes==1&&!touch_weapon_cycle_busy());tick(1020);assert(hover_adds==1);
    ready();accept=0;tick(1000);assert(press_calls==1&&release_calls==1&&!down&&!touch_weapon_cycle_busy());tick(1020);assert(press_calls==1);
    ready();cancel_best=1;tick(1000);assert(!press_calls&&hover_removes==1);
    ready();expire_best=1;tick(1000);assert(!press_calls&&hover_removes==1);
    ready();rename_best=1;tick(1000);assert(!press_calls&&!touch_weapon_cycle_busy());
    for(int site=1;site<=3;site++){ready();cancel_valid=site;tick(1000);assert(!press_calls);}
    ready();f.group_items[0]=f.forest;*(int*)(f.group+0x200)=1;tick(1000);assert(press_calls==1);
    ready();*(int*)(f.group+0x200)=33;tick(1000);assert(!press_calls&&!world_calls);
    ready();f.gun[0x1160]=1;f.gun[0x1161]=2;tick(1000);assert(press_calls==1&&wc_cycle.phase==2);for(long now=1010;now<1300&&touch_weapon_cycle_busy();now+=10){native_frame();tick(now);}assert(pushes==1&&release_calls==1);
}
static void test_geometry(void){
    for(int side=0;side<2;side++)for(int i=0;i<300;i++){ready();f.left[0x314]=(unsigned char)side;f.right[0x314]=(unsigned char)(1-side);float angle=(float)i*.047f;tf(f.roots[1])->q=(qf_t){0,0,sinf(angle/2),cosf(angle/2)};
        tf(f.roots[1])->p[0]=tf(f.roots[2])->p[0]=(float)i*2;tf(f.roots[1])->p[1]=tf(f.roots[2])->p[1]=-(float)i;struct wc_binding b;assert(wc_bind(f.left,&b));
        memcpy(tf(f.head)->p,tf(f.roots[1])->p,12);
        left_grip=f.forest;forest_hand=f.left;down=1;
        const float native_offset[3]={3,-2,1};memcpy(f.gun+0x117c,native_offset,12);
        // 1188 is the next field, not FVector padding or the vector's Z.
        // Poison it so the historical +4-byte shift cannot pass this test.
        *(float*)(f.gun+0x1188)=INFINITY;
        assert(*(float*)(f.gun+0x117c)==3&&*(float*)(f.gun+0x1180)==-2&&*(float*)(f.gun+0x1184)==1&&isinf(*(float*)(f.gun+0x1188)));
        for(int k=0;k<3;k++){float t=(float)k/2;assert(wc_stroke(&b,t,0));float fraction=native_local_fraction();assert(k==2?fraction==1:fabsf(fraction-t)<.001f);}
        assert(wc_stroke(&b,1,1)&&native_local_fraction()==0);
        unsigned char before[sizeof f.gun];memcpy(before,f.gun,sizeof before);assert(wc_stroke(&b,.5f,1));assert(!memcmp(before,f.gun,sizeof before));
    }
    ready();tf(f.roots[3])->p[0]=10000;tick(1000);assert(!world_calls&&!press_calls);
    ready();tf(f.roots[3])->s[2]=0;tick(1000);assert(!world_calls&&!press_calls);
    ready();tf(f.roots[0])->q.w=NAN;tick(1000);assert(!world_calls&&!press_calls);
    begin();*(float*)(f.gun+0x117c)=INFINITY;tick(1020);assert(release_calls==1&&!touch_weapon_cycle_busy());
    begin();*(float*)(f.gun+0x1118)=NAN;tick(1020);assert(release_calls==1&&!touch_weapon_cycle_busy());
}
static void test_left_primary(void){
    ready();f.left[0x314]=1;f.right[0x314]=0;tick(1000);assert(press_calls==1&&touch_weapon_cycle_busy());
    for(long now=1010;now<1600&&touch_weapon_cycle_busy();now+=10){native_frame();tick(now);}
    assert(pulls==1&&pushes==1&&release_calls==1&&!down&&!touch_weapon_cycle_busy());
    ready();f.left[0x314]=1;f.right[0x314]=0;g_cmd_until[27]=1100;tick(1000);assert(!press_calls&&!world_calls);
    ready();f.left[0x314]=1;f.right[0x314]=0;g_grip_latched=0x08000000u;tick(1000);assert(!press_calls&&!world_calls);
    ready();f.left[0x314]=1;f.right[0x314]=0;tick(1000);g_cmd_until[27]=1100;tick(1020);assert(!release_calls&&down&&!touch_weapon_cycle_busy());
    ready();f.left[0x314]=1;f.right[0x314]=0;tick(1000);g_touch_cancel=1;tick(1020);assert(release_calls==1&&!touch_weapon_cycle_busy());
    ready();f.left[0x314]=1;f.right[0x314]=0;f.group_items[0]=f.forest;*(int*)(f.group+0x200)=1;tick(1000);assert(press_calls==1);
    ready();f.left[0x314]=1;f.right[0x314]=0;tick(1000);fixture_clock=1020;touch_weapon_cycle_tick(f.left,0,1020);assert(touch_weapon_cycle_busy()&&!release_calls);
    touch_weapon_cycle_cancel(f.left);assert(release_calls==1&&!touch_weapon_cycle_busy());
}
static void test_primary_pose_follow(void) {
    for(int side=0;side<2;side++) {
        ready();f.left[0x314]=(unsigned char)side;f.right[0x314]=(unsigned char)(1-side);
        struct reload_tf reference;assert(touch_weapon_cycle_pose_reference(f.right,f.gun,&reference)==f.roots[1]);
        assert(!memcmp(&reference,tf(f.roots[1]),sizeof reference));
        assert(!touch_weapon_cycle_pose_reference(f.left,f.gun,&reference));
        *(void**)(f.roots[1]+0x20)=f.pawn;assert(!touch_weapon_cycle_pose_reference(f.right,f.gun,&reference));*(void**)(f.roots[1]+0x20)=f.gun;
        tick(1000);follow_native_attachment=1;long completed=0;
        for(long now=1010;now<1700&&touch_weapon_cycle_busy();now+=10) {
            fixture_clock=now;native_frame();tick(now);completed=now;
            if(!touch_weapon_cycle_busy())break;
            struct reload_tf before=*tf(f.roots[2]),after=before;
            float a=.04f*sinf((float)(now-1000)*.02f);
            after.q=(qf_t){0,sinf(a*.5f),0,cosf(a*.5f)};
            after.p[0]=3*sinf(a);after.p[1]=4*cosf(a);
            float native_fraction=native_local_fraction();
            assert(touch_weapon_cycle_primary_transform(f.right,f.gun,f.roots[2],&before,&after));
            assert(fabsf(native_local_fraction()-native_fraction)<.0001f);
            // Independent original tracked-point invariant: the pump hand and
            // native forestock target retain their relative world distance.
            struct wc_vec grab=grab_location(f.left);float d[3]={grab.x-tf(f.roots[3])->p[0],grab.y-tf(f.roots[3])->p[1],grab.z-tf(f.roots[3])->p[2]};
            assert(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]<25*25);
            assert(!memcmp(tf(f.roots[2]),&after,sizeof after));
        }
        assert(completed>=1430&&pulls==1&&pushes==1&&primary_world_calls>35&&!native_distance_releases&&!down);
    }
    begin();follow_native_attachment=1;
    struct reload_tf before=*tf(f.roots[2]),after=before;after.p[1]=5;
    int previous=world_calls;
    assert(!touch_weapon_cycle_primary_transform(f.left,f.gun,f.roots[2],&before,&after)&&world_calls==previous);
    g_touch_inventory_request=1;
    assert(!touch_weapon_cycle_primary_transform(f.right,f.gun,f.roots[2],&before,&after)&&world_calls==previous);
    g_touch_inventory_request=0;primary_set_cancel=1;
    assert(touch_weapon_cycle_primary_transform(f.right,f.gun,f.roots[2],&before,&after));
    assert(world_calls==previous+1&&!touch_weapon_cycle_busy()&&!down&&!touch_weapon_cycle_input_mask(1000,0));
}
static void test_native_pre_order(void) {
    for(int side=0;side<2;side++)for(int ads=0;ads<2;ads++) {
        ready();f.left[0x314]=(unsigned char)side;f.right[0x314]=(unsigned char)(1-side);
        // Fresh p345 campaign shotgun's authored segments and independent
        // packed grab offset. Its 2% endpoint margin is only 0.2cm.
        struct wc_vec a={-5,0,0},z={-15,0,0};
        memcpy(f.gun+0x10f4,&a,12);memcpy(f.gun+0x1118,&z,12);
        memcpy(f.gun+0x1100,&a,12);z.x=-25;memcpy(f.gun+0x1124,&z,12);
        const float offset[3]={44.5258217f,.50919998f,-.27271339f};
        qf_t base={0,sinf(ads?.13f:0),0,cosf(ads?.13f:0)};
        tf(f.head)->q=tf(f.roots[1])->q=base;
        for(int i=0;i<3;i++)tf(f.head)->p[i]=(float[3]){-8492.56055f,-3858.38501f,256.42355f}[i];
        float view[3]={ads?30:20,side?-15:15,-20},world[3];q_rot(base,view,world);
        for(int i=0;i<3;i++)tf(f.roots[1])->p[i]=tf(f.roots[2])->p[i]=tf(f.head)->p[i]+world[i];
        q_rot(base,(float[3]){a.x+offset[0],offset[1],offset[2]},world);
        for(int i=0;i<3;i++)tf(f.roots[3])->p[i]=tf(f.roots[1])->p[i]+world[i];
        tf(f.roots[3])->p[0]+=2; // Independent controller-to-grab centre.
        tick(1000);assert(touch_weapon_cycle_busy());follow_native_attachment=1;
        for(int i=0;i<3;i++)assert(fabsf(*(float*)(f.gun+0x117c+4*i)-offset[i])<.001f);
        native_interstage_pitch=6.0f*.01745329252f;long completed=0;
        for(long now=1010;now<1900&&touch_weapon_cycle_busy();now+=10) {
            fixture_clock=now;native_frame();tick(now);completed=now;
            if(!touch_weapon_cycle_busy())break;
            // FPS presentation restores the bore after native two-hand solve,
            // carrying the independent off-hand with the attached gun.
            struct reload_tf before=*tf(f.roots[2]),after=before;
            qf_t gunq=tf(f.roots[1])->q;
            qf_t restore=q_mul(base,(qf_t){-gunq.x,-gunq.y,-gunq.z,gunq.w});
            after.q=fixture_unit(q_mul(restore,before.q));
            assert(touch_weapon_cycle_primary_transform(f.right,f.gun,f.roots[2],&before,&after));
            gunq=tf(f.roots[1])->q;
            float dot=gunq.x*base.x+gunq.y*base.y+gunq.z*base.z+gunq.w*base.w;
            float norm=gunq.x*gunq.x+gunq.y*gunq.y+gunq.z*gunq.z+gunq.w*gunq.w;
            assert(fabsf(dot)/sqrtf(norm)>.99999f);
        }
        assert(completed>=1430&&completed<1500&&pulls==1&&pushes==1&&!touch_weapon_cycle_busy());
        assert(!down&&!left_grip&&!native_distance_releases&&native_pre_calls>40&&wc_native_pose_logs>=2);
        assert(*(int*)(f.gun+0xd90)==7);
    }
    begin();int old=world_calls;
    fixture_tid=42;wc_native_pre_pose(f.gun,1010);assert(world_calls==old&&touch_weapon_cycle_busy());fixture_tid=41;
    wc_native_pre_pose(f.foreign,1010);assert(world_calls==old&&touch_weapon_cycle_busy());
    fixture_menu=1;wc_native_pre_pose(f.gun,1010);assert(world_calls==old&&!touch_weapon_cycle_busy()&&!down);
    begin();old=world_calls;g_touch_inventory_request=1;
    wc_native_pre_pose(f.gun,1010);assert(world_calls==old&&!touch_weapon_cycle_busy()&&!down);
    begin();old=world_calls;wc_native_pre_pose(f.gun,1251);
    assert(world_calls==old&&!touch_weapon_cycle_busy()&&!down); // No stale input publication.
    ready();wc_shotgun_class=0;hook_wc_shotgun_pre(f.gun,.01f);
    assert(native_pre_calls==1&&!world_calls&&!press_calls); // Original always runs outside adapter scope.
}
static void test_no_instant_autopush(void){
 for(int side=0;side<2;side++)for(int open=0;open<2;open++) {
  ready();f.left[0x314]=(unsigned char)side;f.right[0x314]=(unsigned char)(1-side);
  f.gun[0x113c]=1;f.gun[0x1160]=(unsigned char)open;f.gun[0x1161]=open?2:1;
  tick(1000);assert(press_calls==1&&f.gun[0x113c]==1&&!pushes&&touch_weapon_cycle_busy());
  long finished=0;
  for(long now=1010;now<1700&&touch_weapon_cycle_busy();now+=10){native_frame();tick(now);finished=now;}
  assert(!touch_weapon_cycle_busy()&&pushes==1&&f.gun[0x113c]==1&&release_calls==1);
  assert(finished>=(open?1200:1430));
 }
}
static long finish_for_return(int side) {
 ready();f.left[0x314]=(unsigned char)side;f.right[0x314]=(unsigned char)(1-side);tick(1000);
 long finished=0;
 for(long now=1010;now<1700&&touch_weapon_cycle_busy();now+=10){fixture_clock=now;native_frame();tick(now);finished=now;}
 assert(finished>=1430&&!touch_weapon_cycle_busy()&&wc_return.active&&!down&&!left_grip&&release_calls==1);
 assert(!touch_weapon_cycle_input_mask(finished,0)&&!*(int*)(f.left+0x468));return finished;
}
static void test_smooth_return(void) {
 for(int side=0;side<2;side++)for(int step=0;step<3;step++) {
  long start_time=finish_for_return(side);float from[3],park[3]={side?.18f:-.18f,-.20f,.20f};
  unsigned bits[3];for(int i=0;i<3;i++)bits[i]=wc_return_pose.bits[i];memcpy(from,bits,sizeof from);
  float previous[3];memcpy(previous,from,sizeof previous);float distance=0;
  for(int i=0;i<3;i++){float d=park[i]-from[i];distance+=d*d;}distance=sqrtf(distance);assert(distance>.3f);
  const int dt[3]={8,16,33};long last=0;
  // Normal grip eligibility may disappear as this released hand leaves the pump.
  free_grip=in_parent=0;
  for(long elapsed=0;elapsed<=220+dt[step];elapsed+=dt[step]) {
   fixture_clock=start_time+elapsed;tick(fixture_clock);
   float result[3];memcpy(result,park,sizeof result);touch_weapon_cycle_pose(side,fixture_clock,result);
   float t=fminf(1,(float)elapsed/220);t=t*t*(3-2*t);float jump=0;
   for(int i=0;i<3;i++){assert(fabsf(result[i]-(from[i]+(park[i]-from[i])*t))<.000002f);float d=result[i]-previous[i];jump+=d*d;}
   assert(sqrtf(jump)<=distance*1.5f*(elapsed-last)/220.0f+.00001f);
   if(!elapsed)assert(!memcmp(result,from,sizeof result));
   memcpy(previous,result,sizeof previous);last=elapsed;
   assert(!touch_weapon_cycle_input_mask(fixture_clock,0)&&!touch_weapon_cycle_busy()&&!down&&!left_grip&&release_calls==1&&press_calls==1);
   assert(*(int*)(f.gun+0xd90)==7&&f.gun[0x1160]==0&&f.gun[0x1161]==0);
  }
  assert(!wc_return.active&&!memcmp(previous,park,sizeof previous));
 }
 for(int side=0;side<2;side++) {
  long now=finish_for_return(side);float off[3]={.4f,.5f,.6f},original[3];memcpy(original,off,sizeof off);
  touch_weapon_cycle_pose(1-side,now,off);assert(!memcmp(off,original,sizeof off));
  touch_weapon_cycle_pose(side,now-1,off);assert(!memcmp(off,original,sizeof off));
  touch_weapon_cycle_pose(side,now+251,off);assert(!memcmp(off,original,sizeof off));
  fixture_menu=1;touch_weapon_cycle_pose(side,now,off);assert(!memcmp(off,original,sizeof off));fixture_menu=0;
  touch_weapon_cycle_input_reset();touch_weapon_cycle_pose(side,now,off);assert(!memcmp(off,original,sizeof off));
  tick(now+10);assert(!wc_return.active&&!wc_return_pose.stamp);
  now=finish_for_return(side);g_cmd_until[side?27:29]=now+500;touch_weapon_cycle_pose(side,now,off);assert(!memcmp(off,original,sizeof off));tick(now+10);assert(!wc_return.active);
  now=finish_for_return(side);left_grip=f.foreign;down=1;tick(now+10);assert(!wc_return.active&&left_grip==f.foreign&&down&&release_calls==1);
  now=finish_for_return(side);touch_weapon_cycle_cancel(f.left);assert(!wc_return.active&&!wc_return_pose.stamp);
  for(int gate=0;gate<6;gate++) {
   now=finish_for_return(side);int* guards[]={&g_touch_cancel,&g_touch_weapon_request,&g_touch_reload_request,&g_touch_interact_request,&g_touch_inventory_request,&g_touch_pause};
   *guards[gate]=1;touch_weapon_cycle_pose(side,now,off);assert(!memcmp(off,original,sizeof off));tick(now+10);assert(!wc_return.active);
  }
  for(int i=0;i<12;i++) {
   now=finish_for_return(side);*(uint64_t*)((char*)wc_cycle.binding.object[i]+0x18)^=1ULL<<40;
   tick(now+10);touch_weapon_cycle_pose(side,now+10,off);assert(!wc_return.active&&!memcmp(off,original,sizeof off)&&release_calls==1);
  }
 }
 begin();cancel_on_release=1;
 for(long now=1010;now<1700&&touch_weapon_cycle_busy();now+=10){fixture_clock=now;native_frame();tick(now);}
 assert(!wc_return.active&&!wc_return_pose.stamp&&release_calls==1);
 puts("PASS: successful pump releases native grip/input immediately then withdraws tracking hand continuously over220ms for both hands/8,16,33ms frames; no ammo/state/input extension, all12 identity/gate/foreign/reset/callback cancellations");
}
int main(void){
#ifdef CYCLE_NATIVE_FIXTURE
    verify_cycle_install_guard();
#endif
    test_smooth_return();test_no_instant_autopush();test_cycle();test_held_input();test_tracking_pose();test_refusals();test_geometry();test_left_primary();test_primary_pose_follow();test_native_pre_order();puts("PASS: actual pump header native endpoint feedback for both primary hands, paired release, foreign ownership, full names, cancellation/timers and600 geometry cases; ammo unchanged");
    puts("PASS: timed pump retains controller grip through native input frames, >=430ms two-stroke completion, fresh/menu/context mask gates, untouched command input, inventory cleanup and foreign takeover for both hands");
    puts("PASS: actual tracking-point replacement prevents native distance release with gripDown1; 120 complete translated/yaw/pitch cycles both hands, head identity/cm-to-OVR metre roundtrip, wrongside/stale/menu/invalid-head gates");
    puts("PASS: both-hand complete pump cycles with changing primary FPS pose and native attached gun/target, invariant original stroke parameter, tracking distance, typed actor reference and setter-time cancellation");
    puts("PASS: actual p345 campaign segments/packed offset and world coordinates, independent native rotation between hand/PreBio ticks, hip/ADS both hands, 430..500ms original endpoint completion and original unconditional pre-tick");return 0;
}
