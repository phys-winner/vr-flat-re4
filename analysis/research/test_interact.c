#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static volatile int g_touch_interact_request, g_touch_cancel, g_touch_reload_request, g_reload_phase;
static volatile long g_cmd_until[32];
static volatile unsigned g_grip_latched;
static void* (*phone_player)(const void*);
static void* (*phone_active)(void);
static unsigned (*phone_type)(void);
static int (*rl_paused)(void), (*rl_player_valid)(void);
static void* (*f_GetCurrentGrip)(const void*);
static void (*f_SetGripForcedHover)(void*,void*,int);
static int g_gripmx,g_ngrips;
static struct {void* g;} g_grips[256];
static void pthread_mutex_lock(int* m){(void)m;}
static void pthread_mutex_unlock(int* m){(void)m;}
static int reload_grip_registered(void*);
static int fixture_game_thread=1;
static int cinema_on_game_thread(void){return fixture_game_thread;}
static void obj_name(const void* p,char* out,int size){(void)p;(void)size;out[0]=0;}
#define LOGF(...) ((void)0)
#include "../re4fix_interact.h"
#ifdef INTERACT_NATIVE_FIXTURE
#include INTERACT_NATIVE_FIXTURE
#endif

static struct {
    unsigned char left[0x800], right[0x800], inactive[0x800], pawn[0x1300], other[0x1300];
    unsigned char grip[0x480], foreign_grip[0x480], parent[0x540], manager[0x410],head[0x200],item_root[0x200];
    unsigned char grip_class[0x100], world_class[0x100], holster_class[0x100];
    unsigned char ammo_class[0x100], prop_class[0x100], manager_class[0x100], hand_class[0x100];
    unsigned char pickup_class[0x100],scene_class[0x100],pawn_class[0x100];
    void* vtable[0x748/8],*pawn_vtable[0x8b0/8],*chains[10][2];
} f __attribute__((aligned(16)));
static void* active, *candidate, *held, *held_right, *after_press,*right_result;
static unsigned type;
static int paused, valid, enabled, free_allowed, parent_valid, registered, action, can_action;
static int presses, releases, best_calls, release_allowed,presses_right,releases_right,free_side,right_free,hand_queries;
static long clock_now = 1000;
static void* forced;
static int pickup_capacity=1,pickup_wall,hover_adds,hover_removes,pickup_attempts,pickup_collects,pickup_destroy_on_press;
static int body_calls,world_los_calls;
static void* get_player(const void* h) { return h == f.inactive ? f.other : f.pawn; }
static void* get_active(void) { return active; }
static unsigned get_type(void) { return type; }
static int get_paused(void) { return paused; }
static int get_valid(void) { return valid; }
static void* get_current(const void* h) { assert(h == f.left || h == f.right);void* grip=h==f.left ? held:held_right;
    return grip==f.grip&&!registered ? 0:grip; } // Original weak/current getter rejects a destroyed grip.
static int get_enabled(const void* h) { assert(h == f.left || h == f.right); return enabled; }
static void* get_best(const void* h) { assert(h == f.left || h == f.right); best_calls++; return forced && !pickup_wall ? forced : candidate; }
static int reload_grip_registered(void* p) { return registered && (p == f.grip || p == f.foreign_grip); }
static void* get_parent(const void* g) { assert(g == f.grip || g == f.foreign_grip); return parent_valid ? f.parent+8 : 0; }
static int get_free(const void* g,int side) { assert(g == f.grip && (side==0 || side==1));free_side=side;return free_allowed && (!side || right_free); }
static int get_down(const void* h) { assert(h == f.left || h == f.right);return ((const unsigned char*)h)[0x33d]!=0; }
static void press(void* h) { assert(h == f.left || h == f.right);presses++;((unsigned char*)h)[0x33d]=1;
    if(*(void* const*)(f.parent+0x10)==f.pickup_class){
        pickup_attempts++;if(!pickup_capacity||pickup_wall)return;
        if(pickup_destroy_on_press){registered=0;forced=0;*(void**)(f.parent+0x10)=0;return;}
        f.parent[0x4c9]=f.parent[0x4ca]=1;
    }
    if(h==f.left)held=after_press;else {presses_right++;held_right=after_press;} }
static void release(void* h) { assert(h == f.left || h == f.right);releases++;((unsigned char*)h)[0x33d]=0;
    void* current=h==f.left?held:held_right;
    if(release_allowed&&current==f.grip&&registered&&*(void* const*)(f.parent+0x10)==f.pickup_class
        &&f.parent[0x4c9]&&f.parent[0x4ca])pickup_collects++;
    if(h==f.right)releases_right++;if(release_allowed){if(h==f.left)held=0;else held_right=0;} }
static void* get_hand(const void* pawn,unsigned char side){assert(pawn==f.pawn && side<=1);hand_queries++;return side ? right_result:f.left;}
static void* hand_class(void){return f.hand_class;}
static void* game_pawn_class(void){return f.pawn_class;}
static struct ti_vec body_origin(const void* pawn){assert(pawn==f.pawn);body_calls++;const float* p=(const float*)((const char*)pawn+0x4a4);return (struct ti_vec){p[0],p[1],p[2]};}
static void* holster_class(void) { return f.holster_class; }
static void* ammo_class(void) { return f.ammo_class; }
static void* prop_class(void) { return f.prop_class; }
static void* prop_pawn(const void* p) { assert(p == f.parent); return *(void* const*)(f.parent+0x3c0); }
static int action_active(void) { return action; }
static int action_allowed(void) { return can_action; }
static void* get_manager(void) { return f.manager; }
static void* get_manager_class(void) { return f.manager_class; }
static void set_class(unsigned char* cls,int slot,int size) {
    f.chains[slot][0] = cls+0x30;
    *(void**)(cls+0x30) = f.chains[slot]; *(int*)(cls+0x38) = 0; *(int*)(cls+0x58) = size;
}
static void ready(void) {
    memset(&f,0,sizeof f); memset((void*)g_cmd_until,0,sizeof g_cmd_until);
    set_class(f.grip_class,0,sizeof f.grip); set_class(f.world_class,1,sizeof f.parent);
    set_class(f.holster_class,2,sizeof f.parent); set_class(f.ammo_class,3,sizeof f.parent);
    set_class(f.prop_class,4,sizeof f.parent); set_class(f.manager_class,5,sizeof f.manager);
    set_class(f.hand_class,6,sizeof f.left);
    set_class(f.pawn_class,9,sizeof f.pawn);*(void**)(f.pawn+0x10)=f.pawn_class;
    *(void**)(f.pawn)=f.pawn_vtable;f.pawn_vtable[0x888/8]=(void*)body_origin;fixture_game_thread=1;
    *(void**)(f.left) = *(void**)(f.right) = f.vtable; f.vtable[0x740/8] = (void*)get_enabled;
    *(void**)(f.left+0x10)=*(void**)(f.right+0x10)=*(void**)(f.inactive+0x10)=f.hand_class;
    f.right[0x314] = 1;
    *(void**)(f.grip+0x10) = *(void**)(f.foreign_grip+0x10) = f.grip_class;
    *(void**)(f.grip+0x300) = *(void**)(f.foreign_grip+0x300) = f.parent;
    *(void**)(f.parent+0x10) = f.world_class; *(void**)(f.manager+0x10) = f.manager_class;
    active = f.pawn; type = 1; paused = 0; valid = enabled = free_allowed = parent_valid = registered = 1;
    action = can_action = 0; candidate = after_press = f.grip; held = held_right = 0; release_allowed = right_free = 1;
    right_result=f.right;presses = releases = best_calls = presses_right = releases_right = hand_queries = 0;free_side=-1;clock_now = 1000;
    g_touch_interact_request = g_touch_cancel = g_touch_reload_request = g_reload_phase = 0;
    g_grip_latched = 0x08000000u;
    ti_ready = 1; ti_hand = ti_pawn = ti_grip = ti_face_hand = ti_face_pawn = 0;
    ti_cancel_hold = 0; ti_face_bit = -1; ti_face_until = 0;
    ti_side=-1;ti_get_hand=get_hand;ti_hand_class=hand_class;
    phone_player = get_player; phone_active = get_active; phone_type = get_type;
    rl_paused = get_paused; rl_player_valid = get_valid; f_GetCurrentGrip = get_current;
    ti_best = get_best; ti_parent = get_parent; ti_free = get_free; ti_down = get_down;
    ti_press = press; ti_release = release; ti_holster_class = holster_class;
    ti_ammo_class = ammo_class; ti_prop_class = prop_class; ti_prop_pawn = prop_pawn;
    ti_action_active = action_active; ti_can_action = action_allowed; ti_manager = get_manager;
    ti_manager_class = get_manager_class;
    ti_pickup_class=ti_pickup_grip_class=ti_scene_class=0;ti_collectable=0;
    ti_grab_center=0;ti_in_valid=0;ti_line_of_sight=0;ti_door_class=0;ti_door_can_open=0;ti_door_open=0;
    ti_ray_origin=body_origin;ti_game_pawn_class=game_pawn_class;
    ti_pickup_sample_ms=0;ti_pickup_available_ms=0;g_ngrips=0;forced=0;f_SetGripForcedHover=0;
    pickup_capacity=1;pickup_wall=hover_adds=hover_removes=pickup_attempts=pickup_collects=pickup_destroy_on_press=0;
    body_calls=world_los_calls=0;
}
static void* pickup_class(void){return f.pickup_class;}static void* scene_class(void){return f.scene_class;}
static void* pickup_grip_class(void){return f.grip_class;}
static int collectable(const void* p){assert(p==f.parent);return f.parent[0x4c2]&&f.parent[0x4c3];}
static void set_hover(void* h,void* g,int on){assert((h==f.left||h==f.right)&&g==f.grip);
    if(on){assert(!forced);forced=g;hover_adds++;}else{assert(forced==g);forced=0;hover_removes++;}}
static void pickup_ready(void){
    ready();set_class(f.pickup_class,7,0x530);set_class(f.scene_class,8,0x200);
    *(void**)(f.parent+0x10)=f.pickup_class;*(void**)(f.head+0x10)=*(void**)(f.item_root+0x10)=f.scene_class;
    *(void**)(f.parent+0x130)=f.item_root;*(void**)(f.parent+0x260)=f.grip;
    *(void**)(f.grip+0x20)=f.parent;
    *(void**)(f.pawn+0x2d8)=f.head;*(void**)(f.head+0x20)=f.pawn;*(void**)(f.item_root+0x20)=f.parent;
    *(float*)(f.head+0x1cc)=*(float*)(f.item_root+0x1cc)=1;
    *(float*)(f.head+0x1d8)=160;*(float*)(f.item_root+0x1d0)=95;
    f.parent[0x4c2]=f.parent[0x4c3]=1;f.parent[0x4de]=2;
    ti_pickup_class=pickup_class;ti_pickup_grip_class=pickup_grip_class;ti_scene_class=scene_class;ti_collectable=collectable;
    f_SetGripForcedHover=set_hover;g_grips[0].g=f.grip;g_ngrips=1;candidate=0;
}
static int door_allowed,door_open_calls,native_grip_valid=1;
static struct ti_vec world_grab_center(const void* grip){assert(grip==f.grip);return (struct ti_vec){95,0,100};}
static int world_in_valid(const void* grip){assert(grip==f.grip);return native_grip_valid;}
static int world_line_of_sight(const void* grip,const float* eye,const float* body){assert(grip==f.grip&&eye[2]==160&&body[0]==0&&body[1]==0&&body[2]==0);world_los_calls++;return !pickup_wall;}
static void* door_class(void){return f.world_class;}
static int door_can_open(const void* door){assert(door==f.parent&&held==f.grip&&get_down(f.left));return door_allowed;}
static void door_open(void* door,float duration){assert(door==f.parent&&duration==.65f&&door_allowed);door_open_calls++;}
static void world_ready(void){
 pickup_ready();*(void**)(f.parent+0x10)=f.world_class;
 ti_grab_center=world_grab_center;ti_in_valid=world_in_valid;ti_line_of_sight=world_line_of_sight;
 ti_door_class=door_class;ti_door_can_open=door_can_open;ti_door_open=door_open;
 door_allowed=native_grip_valid=1;door_open_calls=0;
}
static void tick(void) { touch_interact_tick(f.left,0,clock_now); }
static void request(void) { g_touch_interact_request = 1; tick(); }
static void unchanged_objects(void (*operation)(void)) {
    unsigned char before[sizeof f]; memcpy(before,&f,sizeof f); operation();
    assert(!memcmp(before,&f,sizeof f));
}
static void inspect_grip(void){touch_interact_inspect_object(f.grip);}
static void test_read_only_grip_inspect(void){
    world_ready();g_touch_interact_request=g_touch_reload_request=1;held_right=f.foreign_grip;
    unchanged_objects(inspect_grip);
    assert(!presses && !releases && !hover_adds && !hover_removes && !door_open_calls
        && g_touch_interact_request==1 && g_touch_reload_request==1 && free_side==1 && hand_queries==2);
    world_ready();type=2;*(void**)(f.pawn+0x2d8)=(void*)1;unchanged_objects(inspect_grip);assert(!hand_queries);
    world_ready();*(void**)(f.head+0x20)=f.other;unchanged_objects(inspect_grip);assert(!hand_queries);
    world_ready();*(float*)(f.head+0x1cc)=0;unchanged_objects(inspect_grip);assert(!hand_queries);
    world_ready();ti_ready=0;free_side=-1;unchanged_objects(inspect_grip);assert(free_side==-1 && !hand_queries);
    world_ready();*(int*)(f.grip_class+0x58)=0x45f;free_side=-1;unchanged_objects(inspect_grip);assert(free_side==-1 && !hand_queries);
    world_ready();registered=0;touch_interact_inspect_object((void*)1);assert(!hand_queries);
}
static void test_native_los_contract(void){
    float eye[3]={0,0,160};world_ready();assert(interact_line_of_sight(f.grip,f.pawn,eye));
    assert(body_calls==1 && world_los_calls==1);
    for(int refusal=0;refusal<11;refusal++){
        world_ready();
        switch(refusal){
            case 0:fixture_game_thread=0;break;case 1:type=2;break;case 2:active=f.other;break;
            case 3:ti_ready=0;break;case 4:ti_ray_origin=0;break;case 5:ti_game_pawn_class=0;break;
            case 6:*(int*)(f.pawn_class+0x58)=0xb89;break;case 7:*(void**)(f.pawn+0x10)=f.world_class;break;
            case 8:f.pawn_vtable[0x888/8]=0;break;case 9:registered=0;break;case 10:eye[0]=NAN;break;
        }
        assert(!interact_line_of_sight(f.grip,f.pawn,eye) && !body_calls && !world_los_calls);eye[0]=0;
    }
    world_ready();*(float*)(f.pawn+0x4a4)=NAN;
    assert(!interact_line_of_sight(f.grip,f.pawn,eye) && body_calls==1 && !world_los_calls);
    world_ready();pickup_wall=1;assert(!interact_line_of_sight(f.grip,f.pawn,eye) && world_los_calls==1);
}
static void rejected(void) { unchanged_objects(request); assert(!presses && !releases); assert(g_touch_interact_request == 0); }
static void test_native_pickup_hover(void) {
    for(int fallback=0;fallback<2;fallback++)for(int side=0;side<2;side++) {
        pickup_ready();candidate=fallback?0:f.grip;
        if(side){held=f.pawn;g_grip_latched=0;}
        request();
        assert(pickup_attempts==1&&pickup_collects==1&&presses==1&&releases==1&&!ti_hand);
        assert((side?held_right:held)==0 && (!side||held==f.pawn));
        assert(!get_down(side?f.right:f.left));
        assert(hover_adds==fallback&&hover_removes==fallback&&!forced);
        // A native grab changes these flags. Re-classifying only afterwards
        // would miss the same item and leave it in the persistent world hold.
        assert(f.parent[0x4c9]&&f.parent[0x4ca]);
    }
    // A normal native hover may coexist with someone else's forced hover.
    // Its owner retains that entry; collecting the item never removes it here.
    pickup_ready();candidate=f.grip;forced=f.grip;*(int*)(f.left+0x468)=1;
    request();assert(pickup_collects==1&&presses==1&&releases==1&&forced==f.grip&&!hover_adds&&!hover_removes);
    pickup_ready();candidate=f.grip;f_SetGripForcedHover=0;
    request();assert(pickup_collects==1&&presses==1&&releases==1&&!ti_hand&&!hover_adds&&!hover_removes);
    for(int fallback=0;fallback<2;fallback++) {
        pickup_ready();candidate=fallback?0:f.grip;pickup_capacity=0;
        request();assert(pickup_attempts==1&&!pickup_collects&&presses==1&&releases==1&&!held&&!ti_hand&&!get_down(f.left));
        assert(hover_adds==fallback&&hover_removes==fallback);
        pickup_ready();candidate=fallback?0:f.grip;pickup_wall=1;
        request();assert(!pickup_collects&&!held&&!ti_hand&&!get_down(f.left));
        assert(fallback?(!presses&&hover_adds==1&&hover_removes==1):(presses==1&&releases==1&&!hover_adds&&!hover_removes));
        // A native callback may consume/despawn its own item immediately.
        // Only registry-validated grips may be read or lose forced hover.
        pickup_ready();candidate=fallback?0:f.grip;pickup_destroy_on_press=1;
        request();assert(presses==1&&releases==1&&!held&&!ti_hand&&!registered&&!forced&&!hover_removes);
        pickup_ready();candidate=fallback?0:f.grip;after_press=f.foreign_grip;
        request();assert(presses==1&&!releases&&!pickup_collects&&held==f.foreign_grip);
        assert(hover_adds==fallback&&hover_removes==fallback); // No release of the unexpected native hold.
    }
    // Existing foreign holds, reloads and input owners block the same new path.
    pickup_ready();candidate=f.grip;held=f.pawn;held_right=f.other;g_grip_latched=0;rejected();
    pickup_ready();candidate=f.grip;g_reload_phase=1;rejected();
    pickup_ready();candidate=f.grip;g_cmd_until[29]=1100;rejected();
    ready();request();assert(presses==1&&!releases&&ti_hand==f.left); // A lever remains a persistent normal world grip.
}
int main(int argc,char** argv) {
    (void)argv; if (argc == 999) install_touch_interact(0);
#ifdef INTERACT_NATIVE_FIXTURE
    verify_install_guard();
#endif
    test_read_only_grip_inspect();
    test_native_los_contract();
    ready(); g_touch_interact_request = 1;
    touch_interact_tick(0,0,clock_now); touch_interact_tick(f.right,1,clock_now);
    touch_interact_tick(f.inactive,0,clock_now); assert(g_touch_interact_request == 1 && !best_calls);
    type = 5; tick(); assert(g_touch_interact_request == 1); type = 1;
    paused = 1; tick(); assert(g_touch_interact_request == 1); paused = 0;
    ti_ready = 0; tick(); assert(g_touch_interact_request == 1); ti_ready = 1;
    request(); assert(presses == 1 && !releases && held == f.grip && ti_grip == f.grip && get_down(f.left));
    for (int i = 0; i < 10; i++) { clock_now += 100; tick(); }
    assert(presses == 1 && g_cmd_until[29] == 0 && g_grip_latched == 0x08000000u);
    touch_interact_tick(f.inactive,0,clock_now);
    assert(held == f.grip && ti_hand == f.left && !releases);
    request(); assert(releases == 1 && !held && !ti_hand && !get_down(f.left));
    ready(); request(); release_allowed = 0; request();
    assert(held == f.grip && !get_down(f.left) && releases == 1);
    request(); assert(releases == 2 && held == f.grip); // authored permission is never bypassed
    ready(); held = f.foreign_grip; request(); assert(releases == 1 && !presses); // explicit world drop
    ready(); held = f.grip; *(void**)(f.parent+0x10) = f.ammo_class; rejected();
    ready(); *(void**)(f.parent+0x10) = f.holster_class; rejected();
    ready(); *(void**)(f.parent+0x10) = f.ammo_class; rejected();
    ready(); *(void**)(f.parent+0x10) = f.prop_class; *(void**)(f.parent+0x3c0) = f.pawn; rejected();
    ready(); *(void**)(f.parent+0x10) = f.prop_class; *(void**)(f.parent+0x3c0) = f.other;
    request(); assert(presses == 1); // foreign/unowned prop still goes through authored grab
    ready(); candidate = 0; rejected(); ready(); registered = 0; rejected();
    ready(); parent_valid = 0; rejected(); ready(); *(int*)(f.grip_class+0x58) = 0x307; rejected();
    ready(); *(int*)(f.grip_class+0x58) = 0x10001; rejected();
    ready(); *(void**)(f.parent+0x10) = 0; rejected();
    ready(); *(int*)(f.world_class+0x58) = 0x27; rejected();
    ready(); enabled = 0; rejected(); ready(); free_allowed = 0; rejected();
    ready(); f.left[0x33d] = 1; rejected(); ready(); g_cmd_until[29] = clock_now+100; rejected();
    ready(); g_reload_phase = 1; rejected(); ready(); g_touch_reload_request = 1; rejected();
    ready(); g_grip_latched |= 0x20000000u; rejected();
    ready(); after_press = 0; request(); assert(presses == 1 && releases == 1 && !ti_hand && !get_down(f.left));
    ready(); request(); held = 0; tick(); assert(releases == 1 && !ti_hand && !get_down(f.left));
    ready(); request(); g_touch_cancel = 1; tick(); assert(releases == 1 && !held && !ti_hand);
    ready(); request(); g_touch_cancel = 1; touch_interact_tick(f.inactive,0,clock_now);
    assert(ti_hand == f.left && ti_cancel_hold && !releases);
    g_touch_cancel = 0; tick(); assert(releases == 1 && !held && !ti_hand);
    ready(); request(); active = f.other; tick(); assert(releases == 1 && !held && !ti_hand);
    ready(); action = can_action = 1; f.manager[0x3d9] = 1; g_cmd_until[0] = 900;
    unchanged_objects(request); assert(g_cmd_until[0] == 1180 && !presses && ti_face_bit == 0);
    touch_interact_tick(f.inactive,0,clock_now); assert(g_cmd_until[0] == 1180);
    clock_now = 1180; tick(); assert(g_cmd_until[0] == 900 && ti_face_bit == -1);
    ready(); action = can_action = 1; f.manager[0x3d9] = 3; request(); assert(g_cmd_until[8] == 1180);
    g_cmd_until[8] = 5000; g_touch_cancel = 1; tick(); assert(g_cmd_until[8] == 5000 && ti_face_bit == -1);
    ready(); action = can_action = 1; f.manager[0x3d9] = 1; request();
    g_cmd_until[0] = 0; g_touch_cancel = 1; tick(); assert(g_cmd_until[0] == 0);
    const int face_bits[] = {-1,0,1,8,9};
    for (unsigned command = 1; command <= 4; command++) {
        int bit = face_bits[command];
        ready(); action = can_action = 1; f.manager[0x3d9] = (unsigned char)command;
        g_touch_interact_request = 1;
        touch_interact_tick(f.inactive,0,clock_now);
        assert(g_touch_interact_request == 1 && ti_face_bit == -1);
        for (int other = 0; other < 32; other++) g_cmd_until[other] = 900;
        unchanged_objects(tick);
        assert(g_touch_interact_request == 0 && ti_face_bit == bit && ti_face_until == 1180);
        for (int other = 0; other < 32; other++) assert(g_cmd_until[other] == (other == bit ? 1180 : 900));
        assert(!presses && !releases && !best_calls && g_grip_latched == 0x08000000u);
        clock_now = 1100; unchanged_objects(request); // a repeated request never extends a live pulse
        assert(g_cmd_until[bit] == 1180 && ti_prior_face_until == 900);
        clock_now = 1180; unchanged_objects(tick); assert(g_cmd_until[bit] == 900 && ti_face_bit == -1);
        ready(); action = can_action = 1; f.manager[0x3d9] = (unsigned char)command; request();
        type = 2; unchanged_objects(tick); assert(g_cmd_until[bit] == 0 && ti_face_bit == -1);
        ready(); action = can_action = 1; f.manager[0x3d9] = (unsigned char)command; request();
        g_cmd_until[bit] = 5000; g_touch_cancel = 1; unchanged_objects(tick);
        assert(g_cmd_until[bit] == 5000 && ti_face_bit == -1); // unrelated/new owner survives cancellation
    }
    for (unsigned command = 0; command <= 16; command++) {
        if (command >= 1 && command <= 4) continue;
        ready(); action = can_action = 1; f.manager[0x3d9] = (unsigned char)command; rejected();
        for (int bit = 0; bit < 32; bit++) assert(g_cmd_until[bit] == 0);
        assert(!best_calls && ti_face_bit == -1 && interact_face_bit(command) == -1);
    }
    ready(); action = can_action = 1; f.manager[0x3d9] = 255; rejected();
    ready(); action = can_action = 1; f.manager[0x3d9] = 1; g_cmd_until[0] = 5000; rejected();
    ready(); action = can_action = 1; *(int*)(f.manager_class+0x58) = 0x3d9; rejected();
    ready(); action = can_action = 1; f.manager[0x3d9] = 1; f.manager[0x3da] = 7;
    unsigned kind=71; assert(interact_prompt(&kind)==1 && kind==7);
    unchanged_objects(request); assert(ti_face_bit==0 && g_cmd_until[0]==1180);
    ready(); action = can_action = 1; f.manager[0x3d9] = 1; f.manager[0x3da] = 48; rejected();
    ready(); action = can_action = 1; f.manager[0x3d9] = 1; f.manager[0x3da] = 72; rejected();
    ready(); action = can_action = 1; f.manager[0x3d9] = 1;
    *(void**)(f.manager+0x10)=f.world_class; rejected();
    ready(); assert(interact_face_bit(0) == -1 && interact_face_bit(16) == -1);
    ready();held=f.pawn;f.left[0x33d]=1;g_grip_latched=0x20000000u;request();
    assert(held==f.pawn && held_right==f.grip && presses_right==1 && ti_hand==f.right && ti_side==1 && free_side==1);
    touch_interact_tick(f.right,1,clock_now);assert(presses==1 && held==f.pawn);
    request();assert(held==f.pawn && !held_right && releases_right==1 && !ti_hand && get_down(f.left));
    ready();held=f.pawn;g_grip_latched=0;right_result=0;rejected();right_result=f.right;tick();assert(!presses);
    ready();held=f.pawn;g_grip_latched=0;right_result=f.inactive;rejected();
    ready();held=f.pawn;g_grip_latched=0;f.right[0x314]=0;rejected();
    ready();held=f.pawn;g_grip_latched=0;*(int*)(f.hand_class+0x58)=0x50f;rejected();
    ready();held=f.pawn;g_grip_latched=0;*(void**)(f.right+0x10)=f.world_class;rejected();
    ready();held=f.pawn;g_grip_latched=0;right_free=0;rejected();assert(free_side==1);
    ready();held=f.pawn;g_grip_latched=0;f.right[0x33d]=1;rejected();
    ready();held=f.pawn;g_grip_latched=0;held_right=f.other;rejected();
    ready();held=f.pawn;g_grip_latched=0x08000000u;rejected();
    ready();held=f.pawn;g_grip_latched=0;g_cmd_until[27]=1100;rejected();
    ready();held=f.pawn;g_grip_latched=0;g_reload_phase=1;rejected();
    ready();held=f.pawn;g_grip_latched=0;request();g_touch_cancel=1;tick();assert(!releases && ti_cancel_hold);
    touch_interact_tick(f.right,1,clock_now);assert(releases_right==1 && !held_right && held==f.pawn);
    ready();held=f.pawn;g_grip_latched=0;request();held_right=f.other;g_touch_cancel=1;
    touch_interact_tick(f.right,1,clock_now);assert(!releases && held==f.pawn && held_right==f.other && !ti_hand);
    ready();held=f.pawn;g_grip_latched=0;request();active=f.other;
    touch_interact_tick(f.right,1,clock_now);assert(releases_right==1 && held==f.pawn && !ti_hand);
    ready();held=f.pawn;g_grip_latched=0;request();release_allowed=0;request();assert(releases_right==1 && held_right==f.grip && held==f.pawn);
    ready();held=f.pawn;g_grip_latched=0;action=can_action=1;f.manager[0x3d9]=1;f.manager[0x3da]=7;
    unchanged_objects(request);assert(g_cmd_until[0]==1180 && !presses && !releases && !hand_queries && !best_calls);
    pickup_ready();tick();assert(interact_pickup_available(clock_now));
    request();assert(pickup_attempts==1&&presses==1&&releases==1&&!held&&!ti_hand&&!forced&&hover_adds==1&&hover_removes==1);
    assert(!interact_pickup_available(clock_now));
    pickup_ready();pickup_wall=1;request();assert(!presses&&!releases&&hover_adds==1&&hover_removes==1&&!forced);
    pickup_ready();pickup_capacity=0;request();assert(pickup_attempts==1&&presses==1&&releases==1&&!held&&!ti_hand&&!forced);
    pickup_ready();*(int*)(f.left+0x468)=1;request();assert(!presses&&!hover_adds);
    pickup_ready();*(float*)(f.item_root+0x1d0)=-40;request();assert(presses==1&&releases==1);
    pickup_ready();*(float*)(f.item_root+0x1d0)=-90;request();assert(!presses&&!hover_adds);
    pickup_ready();*(float*)(f.item_root+0x1d0)=131;request();assert(!presses);
    pickup_ready();*(float*)(f.item_root+0x1d8)=-31;request();assert(!presses);
    pickup_ready();*(float*)(f.item_root+0x1d0)=NAN;request();assert(!presses);
    for(int flag=0;flag<5;flag++){
        pickup_ready();if(flag<2)f.parent[flag?0x4c3:0x4c2]=0;else f.parent[flag==2?0x4c9:flag==3?0x4ca:0x4dc]=1;
        request();assert(!presses&&!hover_adds);
    }
    pickup_ready();registered=0;request();assert(!presses);
    pickup_ready();*(void**)(f.grip+0x300)=f.other;request();assert(!presses);
    pickup_ready();*(void**)(f.head+0x20)=f.other;request();assert(!presses);
    pickup_ready();tick();assert(interact_pickup_available(1300)&&!interact_pickup_available(1301)&&!interact_pickup_available(999));
    for(int angle=0;angle<360;angle+=3){
        float a=(float)angle*.01745329252f,eye[3]={10,20,160},forward[3]={cosf(a),sinf(a),0},point[3],score;
        for(int radius=0;radius<=140;radius+=10){
            point[0]=eye[0]+cosf(a)*radius;point[1]=eye[1]+sinf(a)*radius;point[2]=0;
            int eligible=interact_pickup_score(eye,forward,point,1,&score);
            if(radius!=130)assert(!!eligible==(radius<130));
            else if(!eligible)assert(fabsf(hypotf(point[0]-eye[0],point[1]-eye[1])-130)<.0001f);
            if(eligible)assert(isfinite(score));
        }
    }
    test_native_pickup_hover();
    world_ready();tick();assert(interact_pickup_available(clock_now));request();
    assert(door_open_calls==1&&presses==1&&releases==1&&!held&&!ti_hand&&!forced);
    world_ready();door_allowed=0;request();assert(!door_open_calls&&presses==1&&releases==1&&!held&&!ti_hand);
    world_ready();pickup_wall=1;request();assert(!door_open_calls&&!presses&&!hover_adds);
    world_ready();native_grip_valid=0;request();assert(!door_open_calls&&!presses);
    world_ready();*(int*)(f.left+0x468)=1;request();assert(!presses&&!hover_adds);

    puts("PASS: actual context/ownership/native world-grip press/drop, typed free right-hand pickup with dominant-left weapon intact, EHandedness eligibility and side masks, no stale retry or weapon autodrop; unchanged native face prompts and normal release guards");
    puts("PASS: nearby ground pickup prompt, one native grab/release, capacity/obstruction rejection delegated to native handlers, collected/ownership/nonfinite/foreign-hover/stale guards and 1800 facing-distance cases");
    puts("PASS: native BestGrip and nearby fallback one-tap collection for either free hand, pre-press pickup flags, own/foreign forced hover, no forced-hover API, native capacity/obstruction/despawn and foreign-hold lifecycle");
    puts("PASS: nearby world grip native LOS/valid-list gates, original door grab-side then CanOpenDoor/OpenDoor(.65), paired release; locked door, wall, invalid grip and foreign forced hover rejected");
    puts("PASS: explicit grip inspection preserves objects/input/hold state; native predicates for both sides, non-game context, foreign head, bad quaternion, disabled/unregistered and short-layout guards");
    puts("PASS: original view/ray-origin LOS arguments, thread/context/identity/class/layout/vtable/finite guards and unchanged native obstruction rejection");
    return 0;
}
