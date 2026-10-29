#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static volatile int g_touch_context,g_touch_cancel,g_touch_gt_tid;
static volatile int g_touch_interact_request,g_touch_reload_request,g_reload_phase;
static volatile long g_cmd_until[32];
static volatile unsigned g_grip_latched;
static void* (*phone_player)(const void*), *(*phone_active)(void);
static unsigned (*phone_type)(void);
static int (*rl_paused)(void), (*rl_player_valid)(void);
static void* (*f_GetCurrentGrip)(const void*);
static void (*f_SetGripForcedHover)(void*,void*,int);
static int g_gripmx,g_ngrips;
static struct{void* g;}g_grips[256];
static void pthread_mutex_lock(int* m){(void)m;}static void pthread_mutex_unlock(int* m){(void)m;}
static int reload_grip_registered(void* p) { return p!=0; }
static int fixture_tid;
static int cinema_on_game_thread(void){return fixture_tid==g_touch_gt_tid;}
static void obj_name(const void* p,char* out,int size){(void)p;(void)size;out[0]=0;}
#define SYS_gettid 0
#define syscall(x) ((long)fixture_tid)
#define LOGF(...) ((void)0)
#include "../re4fix_interact.h"
#include "../re4fix_hud_state.h"
#ifdef HUD_NATIVE_FIXTURE
#include HUD_NATIVE_FIXTURE
#endif

static struct {
    unsigned char hand[0x598], left[0x598], inactive[0x598], pawn[0x1650], other[0x1650];
    unsigned char manager[0x410], hand_class[0x100], hand_bp[0x100], foreign_class[0x100], manager_class[0x100];
    void* chain[4][2];
} f __attribute__((aligned(16)));
static int hp,max_hp,valid,paused,can_action,has_action,health_calls,manager_calls;
static unsigned type;
static void* active;
static void* current_hand_class(void) { return f.hand_class; }
static void* current_manager_class(void) { return f.manager_class; }
static void* bio4_pawn(void) { return f.pawn; }
static void* hand_pawn(const void* hand) { return hand==f.inactive ? f.other : f.pawn; }
static void* active_pawn(void) { return active; }
static unsigned active_type(void) { return type; }
static int player_valid(void) { return valid; }
static int is_paused(void) { return paused; }
static int current_hp(void) { ++health_calls; return hp; }
static int maximum_hp(void) { ++health_calls; return max_hp; }
static int action_active(void) { return has_action; }
static int action_allowed(void) { return can_action; }
static void* action_manager(void) { ++manager_calls; return f.manager; }
static void ready(void) {
    memset(&f,0,sizeof f); hs_snapshot=0; hs_logs=0;
    *(int*)(f.hand_class+0x58)=0x510; *(int*)(f.hand_class+0x38)=0;
    *(int*)(f.hand_bp+0x58)=0x598; *(int*)(f.hand_bp+0x38)=1;
    f.chain[0][0]=f.hand_class+0x30; f.chain[0][1]=f.hand_bp+0x30;
    *(void**)(f.hand_class+0x30)=*(void**)(f.hand_bp+0x30)=f.chain[0];
    *(int*)(f.manager_class+0x58)=0x410; *(int*)(f.manager_class+0x38)=0;
    f.chain[1][0]=f.manager_class+0x30; *(void**)(f.manager_class+0x30)=f.chain[1];
    *(int*)(f.foreign_class+0x58)=0x598; *(int*)(f.foreign_class+0x38)=0;
    f.chain[2][0]=f.foreign_class+0x30; *(void**)(f.foreign_class+0x30)=f.chain[2];
    *(void**)(f.hand+0x10)=*(void**)(f.left+0x10)=*(void**)(f.inactive+0x10)=f.hand_bp;
    *(void**)(f.manager+0x10)=f.manager_class; f.hand[0x314]=f.inactive[0x314]=1;
    hp=max_hp=1200; valid=1; paused=has_action=can_action=health_calls=manager_calls=0;
    type=1; active=f.pawn; fixture_tid=g_touch_gt_tid=123;
    g_touch_context=1; g_touch_cancel=0;
    phone_player=hand_pawn; phone_active=active_pawn; phone_type=active_type;
    rl_paused=is_paused; rl_player_valid=player_valid;
    hs_current=current_hp; hs_max=maximum_hp; hs_bio4=bio4_pawn; hs_hand_class=current_hand_class;
    ti_ready=1; ti_manager=action_manager; ti_manager_class=current_manager_class;
    ti_action_active=action_active; ti_can_action=action_allowed;
    ti_pickup_available_ms=0;
}
static void tick(void) {
    unsigned char before[sizeof f]; memcpy(before,&f,sizeof f);
    touch_hud_state_tick(f.hand,1000); assert(!memcmp(before,&f,sizeof f));
}
static void rejected(void) {
    struct mobile_hud_state s={1,1,1,1}; tick();
    assert(!touch_hud_state_read(1000,&s));
    assert(!s.current_health && !s.max_health && !s.command && !s.action);
}
int main(int argc,char** argv) {
    (void)argv; if (argc==999) { install_touch_interact(0); install_touch_hud_state(0); }
#ifdef HUD_NATIVE_FIXTURE
    verify_hud_install_guard();
#endif
    struct mobile_hud_state s;
    ready(); tick(); assert(health_calls==2 && !manager_calls);
    assert(touch_hud_state_read(1000,&s) && s.current_health==1200 && s.max_health==1200 && !s.command && s.action==71);
    assert(touch_hud_state_read(1500,&s)); assert(!touch_hud_state_read(1501,&s));
    assert(!touch_hud_state_read(999,&s)); assert(!touch_hud_state_read(0,&s)); assert(!touch_hud_state_read(1000,0));
    ready(); tick(); uint64_t good=hs_snapshot;
    fixture_tid=456; touch_hud_state_tick(f.hand,1010);
    assert(hs_snapshot==good && health_calls==2 && !manager_calls);
    fixture_tid=123; touch_hud_state_tick(f.inactive,1010); touch_hud_state_tick(f.left,1010);
    touch_hud_state_tick(0,1010); assert(hs_snapshot==good && health_calls==2);
    ready(); *(void**)(f.hand+0x10)=f.foreign_class; rejected(); assert(!health_calls);
    ready(); *(int*)(f.hand_bp+0x58)=0x50f; rejected(); assert(!health_calls);
    ready(); *(int*)(f.hand_class+0x58)=0x598; rejected(); assert(!health_calls);
    ready(); *(int*)(f.hand_bp+0x38)=1025; rejected(); assert(!health_calls);
    ready(); active=f.other; rejected(); assert(!health_calls);
    ready(); valid=0; rejected(); assert(!health_calls);
    ready(); paused=1; rejected(); assert(!health_calls);
    for(unsigned context=2;context<=9;context++) {
        ready(); tick(); type=context; g_touch_context=(int)context; rejected(); assert(!hs_snapshot && health_calls==2);
    }
    ready(); tick(); g_touch_cancel=2; rejected(); assert(!hs_snapshot);
    const int bad[][2]={{-1,1200},{1201,1200},{0,0},{0,-1},{1,32768}};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;i++) { ready(); hp=bad[i][0]; max_hp=bad[i][1]; rejected(); }
    for(int current=0;current<=1200;current+=75) {
        ready(); hp=current; tick(); assert(touch_hud_state_read(1000,&s) && s.current_health==current && s.max_health==1200);
    }
    ready(); hp=max_hp=32767; tick(); assert(touch_hud_state_read(1000,&s) && s.current_health==32767 && s.max_health==32767);
    ready(); tick(); tick(); assert(hs_logs==1 && health_calls==4);
    hp=820;tick();assert(hs_logs==2 && hs_last_current==820 && health_calls==6);
    hp=1200;tick();assert(hs_logs==3 && hs_last_current==1200 && health_calls==8);
    for(int damage=0;damage<40;damage++) {hp=1199-damage;tick();}
    assert(hs_logs==32 && health_calls==88);
    for(unsigned command=1;command<=4;command++) {
        ready(); has_action=can_action=1; f.manager[0x3d9]=(unsigned char)command; f.manager[0x3da]=7;
        tick(); assert(touch_hud_state_read(1000,&s) && s.command==command && s.action==7);
    }
    for(unsigned command=0;command<=16;command++) {
        if(command>=1 && command<=4) continue;
        ready(); has_action=can_action=1; f.manager[0x3d9]=(unsigned char)command; f.manager[0x3da]=7;
        tick(); assert(touch_hud_state_read(1000,&s) && !s.command && s.action==71);
    }
    ready(); has_action=can_action=1; f.manager[0x3d9]=1; f.manager[0x3da]=48;
    tick(); assert(touch_hud_state_read(1000,&s) && !s.command);
    ready(); has_action=can_action=1; f.manager[0x3d9]=1; f.manager[0x3da]=72;
    tick(); assert(touch_hud_state_read(1000,&s) && !s.command);
    ready(); has_action=can_action=1; f.manager[0x3d9]=1; f.manager[0x3da]=7;
    *(void**)(f.manager+0x10)=f.foreign_class; tick(); assert(touch_hud_state_read(1000,&s) && !s.command);
    ready();ti_pickup_available_ms=1000;tick();assert(touch_hud_state_read(1000,&s)&&s.command==1&&s.action==40);
    ti_pickup_available_ms=699;tick();assert(touch_hud_state_read(1000,&s)&&!s.command);
    ready();ti_pickup_available_ms=1000;has_action=can_action=1;f.manager[0x3d9]=1;f.manager[0x3da]=7;
    tick();assert(touch_hud_state_read(1000,&s)&&s.command==1&&s.action==7);
    ready(); hp=600; touch_hud_state_tick(f.hand,0xfffff0);
    assert(touch_hud_state_read(0x1000007,&s) && s.current_health==600);
    assert(!touch_hud_state_read(0x1000300,&s));
    puts("PASS: actual HUD C: HP0..32767, paired snapshot, damage ratio, 500ms/future/wrap freshness, native typed ownership/active right/GT/focus/menu guards, kick semantic A/B/X/Y, unsupported/NoDisp refusal; no UObject writes");
    return 0;
}
