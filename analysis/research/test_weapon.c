#include <assert.h>
#include <stdio.h>
#include <string.h>

static void* (*phone_player)(const void*);
static void* (*phone_active)(void);
static void* (*rl_gun1)(const void*);
static unsigned (*phone_type)(void);
static void* (*f_GetCurrentGrip)(const void*);
static void* g_mobile_holster, *g_mobile_holster_hand;
static long g_mobile_holster_until;
static volatile int g_touch_gt_tid,g_touch_context,g_touch_cancel,g_touch_weapon_request,g_touch_weapon;
static volatile int g_touch_reload_request,g_reload_phase;
static void* (*ti_prop_class)(void), *(*ti_holster_class)(void), *(*ti_parent)(const void*), *(*ti_best)(const void*);
static int (*ti_down)(const void*), (*ti_free)(const void*,int), (*rl_paused)(void), (*rl_player_valid)(void);
static void (*ti_press)(void*), (*ti_release)(void*);
static void* (*rl_primary)(const void*);
static int ti_ready,fixture_tid=41;
static long fixture_clock;
static long ptr_now_ms(void){return fixture_clock;}
#define gettid() fixture_tid
static int reload_grip_registered(void* grip);
#define LOGF(...) ((void)0)
#include "../re4fix_weapon.h"
#ifdef WEAPON_NATIVE_FIXTURE
#include WEAPON_NATIVE_FIXTURE
#endif

static struct {
    unsigned char hand[0x570], inactive[0x570], pawn[0x1300], other[0x1300];
    unsigned char knife[0x700], gun[0xe00], knife_holster[0x400], gun_holster[0x400];
    unsigned char knife_grip[0x460], gun_grip[0x460], knife_primary[0x460],gun_primary[0x460];
    unsigned char item_class[0x100],holster_class[0x100],hand_class[0x100],pawn_class[0x100],grip_class[0x100];
    void* chains[5][1];void* props[2][1];void* vtable[0x748/8];
} f __attribute__((aligned(16)));
static unsigned active_type = 1;
static void* active_pawn;
static void* knife_item;
static void* gun_item;
static unsigned registered = 3;
static int item_calls;
static void* held_grip;
static void* best_grip,*primary_hand;
static int native_down,allowed=1,paused,valid=1,free_grip=1,press_calls,release_calls,accept_press,cancel_in_best,expire_in_best;
static void* get_current_grip(const void* hand) { (void)hand; return held_grip; }
static void* hand_class(void){return f.hand_class;}
static void* pawn_class(void){return f.pawn_class;}
static void* grip_class(void){return f.grip_class;}
static void* prop_class(void){return f.item_class;}
static void* holster_class(void){return f.holster_class;}
static void* parent(const void* grip){return *(void* const*)((const char*)grip+0x300);}
static void* best(const void* hand){assert(hand==f.hand);if(cancel_in_best)g_touch_cancel=2;if(expire_in_best)fixture_clock=2800;return best_grip;}
static void* primary(const void* item){assert(item==f.gun || item==f.knife);return primary_hand;}
static int down(const void* hand){assert(hand==f.hand);return native_down;}
static int free_to_grab(const void* grip,int side){assert(grip==f.gun_grip && side==1);return free_grip;}
static int can_accept(const void* hand){assert(hand==f.hand);return allowed;}
static int is_paused(void){return paused;}
static unsigned pause_reasons;
static int paused_for(int reason){assert(reason>=0 && reason<7);return (int)(pause_reasons>>reason&1u);}
static int player_valid(void){return valid;}
static void press(void* hand){
    assert(hand==f.hand && !held_grip && native_down && allowed && best_grip==f.gun_grip);
    press_calls++;if(accept_press){held_grip=f.gun_primary;primary_hand=f.hand;}
}
static void release(void* hand){assert(hand==f.hand && !held_grip);release_calls++;native_down=0;}
static void* get_player(const void* hand) {
    return hand == f.hand ? f.pawn : hand == f.inactive ? f.other : 0;
}
static void* get_active(void) { return active_pawn; }
static unsigned get_type(void) { return active_type; }
static void* get_knife(const void* pawn) { assert(pawn == f.pawn); item_calls++; return knife_item; }
static void* get_gun(const void* pawn) { assert(pawn == f.pawn); item_calls++; return gun_item; }
static void* get_holster(const void* item) {
    assert(item == f.knife || item == f.gun);
    return *(void* const*)((const char*)item+0x3d0);
}
static int reload_grip_registered(void* grip) {
    return ((registered & 1) && grip == f.knife_grip) || ((registered & 2) && grip == f.gun_grip)
        || ((registered & 4) && grip==f.knife_primary) || ((registered & 8) && grip==f.gun_primary);
}
static void ready(void) {
    memset(&f,0,sizeof f);
    f.hand[0x314] = f.inactive[0x314] = 1;
    active_pawn = f.pawn; active_type = 1; registered = 15;
    knife_item = f.knife; gun_item = f.gun;
    *(void**)(f.knife+0x10) = *(void**)(f.gun+0x10) = f.item_class;
    *(int*)(f.item_class+0x58) = sizeof f.gun;
    *(void**)(f.knife+0x3c0) = *(void**)(f.gun+0x3c0) = f.pawn;
    *(void**)(f.knife+0x3d0) = f.knife_holster; *(void**)(f.gun+0x3d0) = f.gun_holster;
    *(void**)(f.knife_holster+0x10) = *(void**)(f.gun_holster+0x10) = f.holster_class;
    *(int*)(f.holster_class+0x58) = sizeof f.gun_holster;
    *(void**)(f.knife_holster+0x348) = *(void**)(f.gun_holster+0x348) = f.pawn;
    *(void**)(f.knife_holster+0x248) = f.knife_grip;
    *(void**)(f.gun_holster+0x248) = f.gun_grip;
    *(void**)(f.knife_grip+0x20) = f.knife_holster;
    *(void**)(f.gun_grip+0x20) = f.gun_holster;
    *(void**)(f.knife+0x270)=f.knife_primary;*(void**)(f.gun+0x270)=f.gun_primary;
    *(void**)(f.knife_primary+0x20)=f.knife;*(void**)(f.gun_primary+0x20)=f.gun;
    *(void**)(f.knife_grip+0x300)=f.knife_holster;*(void**)(f.gun_grip+0x300)=f.gun_holster;
    *(void**)(f.knife_primary+0x300)=f.knife;*(void**)(f.gun_primary+0x300)=f.gun;
    unsigned char* classes[]={f.hand_class,f.pawn_class,f.item_class,f.holster_class,f.grip_class};
    int sizes[]={0x570,0x1300,0xe00,0x400,0x460};
    for(int i=0;i<5;i++){
        *(int*)(classes[i]+0x58)=sizes[i];*(int*)(classes[i]+0x38)=0;
        f.chains[i][0]=classes[i]+0x30;*(void**)(classes[i]+0x30)=f.chains[i];
    }
    *(void**)(f.hand+0x10)=*(void**)(f.inactive+0x10)=f.hand_class;
    *(void**)(f.pawn+0x10)=*(void**)(f.other+0x10)=f.pawn_class;
    *(void**)(f.knife_grip+0x10)=*(void**)(f.gun_grip+0x10)=f.grip_class;
    *(void**)(f.knife_primary+0x10)=*(void**)(f.gun_primary+0x10)=f.grip_class;
    f.props[0][0]=f.knife;f.props[1][0]=f.gun;
    *(void**)(f.knife_holster+0x350)=f.props[0];*(void**)(f.gun_holster+0x350)=f.props[1];
    *(int*)(f.knife_holster+0x358)=*(int*)(f.gun_holster+0x358)=1;
    *(int*)(f.knife_holster+0x35c)=*(int*)(f.gun_holster+0x35c)=1;
    *(void**)f.hand=f.vtable;f.vtable[0x740/8]=can_accept;
    tw_hand_class=hand_class;tw_pawn_class=pawn_class;tw_grip_class=grip_class;
    tw_knife=get_knife;tw_holster=get_holster;ti_prop_class=prop_class;ti_holster_class=holster_class;
    ti_parent=parent;ti_best=best;ti_down=down;ti_free=free_to_grab;ti_press=press;ti_release=release;
    rl_primary=primary;rl_paused=is_paused;rl_player_valid=player_valid;f_GetCurrentGrip=get_current_grip;
    tw_paused_for=0;pause_reasons=0;tw_consumable=0;
    fixture_tid=g_touch_gt_tid=41;g_touch_context=1;g_touch_cancel=g_touch_weapon_request=0;g_touch_weapon=2;
    g_touch_reload_request=g_reload_phase=0;ti_ready=1;
    held_grip=best_grip=primary_hand=0;native_down=0;allowed=valid=free_grip=1;paused=0;
    press_calls=release_calls=accept_press=cancel_in_best=expire_in_best=0;fixture_clock=1000;
    g_mobile_holster=g_mobile_holster_hand=0;g_mobile_holster_until=0;memset(&tw_retry,0,sizeof tw_retry);tw_retry_logs=0;
}
static void expect(void* hand, int weapon, void* grip) {
    unsigned char before[sizeof f];
    memcpy(before,&f,sizeof f);
    assert(touch_weapon_grip(hand,weapon) == grip);
    assert(!memcmp(before,&f,sizeof f));
}
static void start_retry(void) {
    ready();g_mobile_holster=f.gun_holster;g_mobile_holster_hand=f.hand;g_mobile_holster_until=2800;
    touch_weapon_request_started(f.hand,2,1000);
    assert(tw_retry.weapon==2 && tw_retry.until==2800 && tw_retry.binding.object[5]==f.gun_primary);
}
static int tick(long now) {
    unsigned char before[sizeof f];memcpy(before,&f,sizeof f);
    fixture_clock=now;
    int result=touch_weapon_retry_tick(f.hand,now);assert(!memcmp(before,&f,sizeof f));return result;
}
static void test_retry(void) {
    start_retry();assert(!tick(1250) && !press_calls);
    native_down=1;allowed=0;assert(!tick(1500) && !press_calls);
    allowed=1;best_grip=f.knife_grip;assert(!tick(1750) && !press_calls);
    best_grip=f.gun_grip;assert(!tick(2000) && press_calls==1 && !held_grip);
    accept_press=1;assert(tick(2250) && press_calls==2 && held_grip==f.gun_primary && !tw_retry.weapon && !g_mobile_holster_until);
    assert(!tick(2500) && press_calls==2 && !release_calls);
    start_retry();held_grip=f.gun_primary;primary_hand=f.hand;assert(tick(1001) && !press_calls);
    start_retry();held_grip=f.knife_primary;primary_hand=f.hand;
    touch_weapon_request_started(f.hand,2,1000);assert(tw_retry.waiting_release && !tick(1001));
    held_grip=0;primary_hand=0;native_down=1;best_grip=f.gun_grip;accept_press=1;
    assert(tick(1250) && press_calls==1 && held_grip==f.gun_primary);
    start_retry();held_grip=f.knife_primary;native_down=1;assert(tick(1250) && !press_calls && !release_calls);
    start_retry();held_grip=f.gun_primary;primary_hand=f.inactive;assert(tick(1250) && !press_calls && !release_calls);
    start_retry();native_down=1;best_grip=f.gun_grip;
    for(long now=1250;now<2800;now+=250)assert(!tick(now));
    assert(press_calls==7 && tick(2800) && release_calls==1 && !native_down);
    assert(!tick(9999) && press_calls==7 && release_calls==1 && !g_mobile_holster_until);
    start_retry();fixture_tid=42;assert(!tick(1250) && tw_retry.weapon==2 && !press_calls);
    touch_weapon_request_started(f.hand,1,1300);touch_weapon_retry_cancel(f.hand);assert(tw_retry.weapon==2);
    fixture_tid=41;assert(!touch_weapon_retry_tick(f.inactive,1250) && tw_retry.weapon==2);
    touch_weapon_retry_cancel(f.inactive);assert(tw_retry.weapon==2);
    touch_weapon_retry_cancel(f.hand);assert(!tw_retry.weapon && !g_mobile_holster_until);
    volatile int* local_blockers[]={&g_touch_cancel,&g_touch_weapon_request,&g_touch_reload_request,&g_reload_phase};
    for(unsigned i=0;i<sizeof local_blockers/sizeof *local_blockers;i++){
        start_retry();*local_blockers[i]=1;assert(tick(1250) && !press_calls && !tw_retry.weapon);
    }
    start_retry();g_touch_weapon=1;assert(tick(1250) && !press_calls);
    start_retry();active_type=2;assert(tick(1250) && !press_calls);
    start_retry();active_pawn=f.other;assert(tick(1250) && !press_calls);
    start_retry();paused=1;assert(tick(1250) && !press_calls);
    // Weapon-management tutorial: the world is paused for Game/Communicator only and the native grab stays live.
    start_retry();paused=1;tw_paused_for=paused_for;pause_reasons=1u<<1;native_down=1;best_grip=f.gun_grip;accept_press=1;
    assert(tick(1250) && press_calls==1 && held_grip==f.gun_primary);
    start_retry();paused=1;tw_paused_for=paused_for;pause_reasons=(1u<<1)|(1u<<3);assert(tick(1250) && !press_calls);
    start_retry();paused=1;tw_paused_for=paused_for;pause_reasons=1u<<2;native_down=1;best_grip=f.gun_grip;accept_press=1;
    assert(tick(1250) && press_calls==1 && held_grip==f.gun_primary);   // tutorial inside the first call
    for(int blocked=0;blocked<7;blocked++){
        if(blocked==1 || blocked==2)continue;
        start_retry();paused=1;tw_paused_for=paused_for;pause_reasons=(1u<<2)|(1u<<blocked);native_down=1;best_grip=f.gun_grip;
        assert(tick(1250) && !press_calls);
    }
    start_retry();paused=1;tw_paused_for=paused_for;pause_reasons=0;assert(tick(1250) && !press_calls);
    start_retry();valid=0;assert(tick(1250) && !press_calls);
    start_retry();assert(tick(999) && !press_calls);
    for(int i=0;i<6;i++){
        start_retry();unsigned long long name=1ULL<<40;
        memcpy((char*)tw_retry.binding.object[i]+0x18,&name,8);assert(tick(1250) && !press_calls);
    }
    start_retry();registered&=~8u;assert(tick(1250) && !press_calls);
    start_retry();f.props[1][0]=f.knife;assert(tick(1250) && !press_calls);
    start_retry();*(void**)(f.gun_primary+0x300)=f.knife;assert(tick(1250) && !press_calls);
    start_retry();*(int*)(f.grip_class+0x58)=0x45f;assert(tick(1250) && !press_calls);
    start_retry();*(int*)(f.hand_class+0x58)=0x50f;assert(tick(1250) && !press_calls);
    start_retry();native_down=1;best_grip=f.gun_grip;free_grip=0;assert(!tick(1250) && !press_calls);
    start_retry();native_down=1;best_grip=f.gun_grip;cancel_in_best=1;assert(tick(1250) && !press_calls);
    start_retry();native_down=1;best_grip=f.gun_grip;expire_in_best=1;
    assert(tick(1250) && !press_calls && release_calls==1 && !native_down);
    start_retry();g_mobile_holster=f.knife_holster;g_mobile_holster_until=9000;
    assert(!tick(1250) && !tw_retry.weapon && !press_calls && g_mobile_holster_until==9000);
    start_retry();tw_retry_logs=7;for(int i=0;i<20;i++){tw_retry.reason=0;tw_retry_log(3,0,0,1,0);}assert(tw_retry_logs==8);
    puts("PASS: actual weapon retry: missed edge -> later permission/exact best -> real primary confirmation; at most7 normal presses/1800ms, native release on empty timeout, no late grab; current/foreign/GT/Home/context/new selection/clock/full FName/slot/registry/owner/type refusal; final cancellation before press, foreign deadline preserved, immutable objects and8-log bound");
}
int main(int argc,char** argv) {
    (void)argv;
#ifdef WEAPON_NATIVE_FIXTURE
    verify_weapon_install_guard();
#endif
    if (argc == 999) install_touch_weapon(0);
    phone_player = get_player; phone_active = get_active; phone_type = get_type; rl_gun1 = get_gun;
    ready();tw_knife=0;expect(f.hand,1,0);
    tw_knife = get_knife; tw_holster = get_holster;
    expect(f.hand,1,f.knife_grip); expect(f.hand,2,f.gun_grip);
    int calls = item_calls;
    expect(0,1,0); expect(f.inactive,1,0); expect(f.hand,0,0); expect(f.hand,3,0); expect(f.hand,4,0);
    f.hand[0x314] = 0; expect(f.hand,1,0); f.hand[0x314] = 1;
    active_type = 5; expect(f.hand,1,0); active_type = 1;
    active_pawn = 0; expect(f.hand,1,0); active_pawn = f.pawn;
    assert(item_calls == calls);
    knife_item = 0; expect(f.hand,1,0); gun_item = 0; expect(f.hand,2,0);
    ready(); *(void**)(f.gun+0x3d0) = 0; expect(f.hand,2,0);
    ready(); *(void**)(f.gun+0x3c0) = f.other; expect(f.hand,2,0);
    ready(); *(void**)(f.gun_holster+0x348) = f.other; expect(f.hand,2,0);
    ready(); *(void**)(f.gun_holster+0x248) = 0; expect(f.hand,2,0);
    ready(); registered = 5; expect(f.hand,2,0); expect(f.hand,1,f.knife_grip);
    ready(); *(void**)(f.gun_grip+0x20) = f.knife_holster; expect(f.hand,2,0);
    ready(); *(void**)(f.gun+0x10) = 0; expect(f.hand,2,0);
    ready(); *(int*)(f.item_class+0x58) = 0x3d7; expect(f.hand,2,0);
    ready(); *(int*)(f.item_class+0x58) = 0x10001; expect(f.hand,2,0);
    ready(); *(void**)(f.gun_holster+0x10) = 0; expect(f.hand,2,0);
    ready(); *(int*)(f.holster_class+0x58) = 0x34f; expect(f.hand,2,0);
    ready(); *(int*)(f.holster_class+0x58) = 0x10001; expect(f.hand,2,0);
    ready(); rl_gun1 = 0; expect(f.hand,2,0); expect(f.hand,1,f.knife_grip); rl_gun1 = get_gun;
    phone_player = 0; expect(f.hand,1,0); phone_player = get_player;
    phone_active = 0; expect(f.hand,1,0); phone_active = get_active;
    phone_type = 0; expect(f.hand,1,0); phone_type = get_type;
    tw_holster = 0; expect(f.hand,1,0); tw_holster = get_holster;
    ready(); expect(f.hand,1,f.knife_grip); expect(f.hand,2,f.gun_grip);
    // Healing item (weapon 3): the same holster binding through AVR4GamePlayerPawn::GetConsumable.
    tw_consumable = get_knife; expect(f.hand,3,f.knife_grip); expect(f.hand,4,0);
    knife_item = 0; expect(f.hand,3,0); ready();
    f_GetCurrentGrip = get_current_grip;
    g_mobile_holster = f.gun_holster; g_mobile_holster_hand = f.hand; g_mobile_holster_until = 1000;
    assert(touch_holster_hover_allowed(f.gun_holster,f.hand,999));
    assert(!touch_holster_hover_allowed(f.gun_holster,f.hand,1000));
    assert(!touch_holster_hover_allowed(f.knife_holster,f.hand,999));
    assert(!touch_holster_hover_allowed(f.gun_holster,f.inactive,999));
    held_grip = f.knife_grip; assert(!touch_holster_hover_allowed(f.gun_holster,f.hand,999)); held_grip = 0;
    active_pawn = f.other; assert(!touch_holster_hover_allowed(f.gun_holster,f.hand,999)); active_pawn = f.pawn;
    active_type = 5; assert(!touch_holster_hover_allowed(f.gun_holster,f.hand,999)); active_type = 1;
    assert(!touch_holster_hover_allowed(0,f.hand,999));
    test_retry();
    puts("PASS: exact assigned knife/Gun1 holster, active/right/type/null/size/owner/registry guards, immutable objects");
    return 0;
}
