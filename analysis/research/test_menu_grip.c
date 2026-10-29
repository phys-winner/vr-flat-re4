// Execute the actual input header, direct native-menu adapter and native Back seam.
#define main input_baseline_main
#include "../test_touch.c"
#undef main
static void* (*phone_player)(const void*);
static void* (*phone_active)(void);
#include "../re4fix_menu_grip.h"
#include "../re4fix_unpause.h"

static struct {
    unsigned char hand[0x580], left[0x580], inactive[0x580], game_hand[0x580];
    unsigned char pawn[0x1300], other[0x1300], button[0x340], actor[0x340], screen[0x400];
    unsigned char hand_class[0x80], pawn_class[0x80], button_class[0x80], screen_class[0x80];
    unsigned char native_hand[0x80], native_pawn[0x80], native_pause[0x80];
    unsigned char native_button[0x80], native_screen[0x80], native_pause_screen[0x80];
    void* hand_chain[2]; void* pawn_chain[3]; void* button_chain[2]; void* screen_chain[3];
    void* hand_vtable[150]; void* button_vtable[150];
} m __attribute__((aligned(16)));
static void* native_active;
static unsigned native_type;
static int hover_success, receiver_enabled, presses, releases, hovers, native_getters;
static int back_calls, back_accept, events[16], event_count;
static void unlocked(void) { assert(!pthread_mutex_trylock(&g_imx)); pthread_mutex_unlock(&g_imx); }
static void event(int value) { assert(event_count<16); events[event_count++]=value; }
static void* native_player(const void* hand) {
    native_getters++; unlocked();
    return hand==m.hand || hand==m.left ? m.pawn : hand==m.inactive ? m.other : 0;
}
static void* active_getter(void) { native_getters++; unlocked(); return native_active; }
static unsigned type_getter(void) { native_getters++; return native_type; }
static void* hand_class_getter(void) { unlocked(); return m.native_hand; }
static void* pawn_class_getter(void) { unlocked(); return m.native_pawn; }
static void* button_class_getter(void) { unlocked(); return m.native_button; }
static void* screen_class_getter(void) { unlocked(); return m.native_screen; }
static void* pause_class_getter(void) { return m.native_pause; }
static void* pause_screen_getter(void) { return m.native_pause_screen; }
static int native_hover(struct mg_ui_object* object,struct mg_ui_hit* hit,void* hand) {
    unlocked(); assert(hand==m.hand); hovers++;
    if (!hover_success) return 0;
    object->actor=m.actor; object->component=m.button; object->screen=m.screen;
    hit->position[0]=1; hit->distance=3;
    return 1;
}
static int enabled_getter(const void* component) { unlocked(); assert(component==m.button); return receiver_enabled; }
static void press(void* hand) { unlocked(); assert(hand==m.hand); ((unsigned char*)hand)[0x33d]=1; presses++; event(1); }
static void release(void* hand) { unlocked(); assert(hand==m.hand); ((unsigned char*)hand)[0x33d]=0; releases++; event(2); }
static int back(unsigned char reason) { unlocked(); assert(!m.hand[0x33d] && !mg_owned.hand && !g_menu_pointer.serial); assert(reason==0); back_calls++; event(3); return back_accept; }
static void type_class(unsigned char* cls,int bytes,int depth,void** chain) {
    *(int*)(cls+0x58)=bytes; *(int*)(cls+0x38)=depth; *(void***)(cls+0x30)=chain;
}
static void ready(void) {
    touch_reset(); memset(&m,0,sizeof m); memset(&mg_owned,0,sizeof mg_owned);
    mg_seen_hand=mg_seen_pawn=0; mg_seen_hand_name=mg_seen_pawn_name=0;
    m.hand_chain[0]=m.native_hand+0x30; m.hand_chain[1]=m.hand_class+0x30;
    m.pawn_chain[0]=m.native_pawn+0x30; m.pawn_chain[1]=m.native_pause+0x30; m.pawn_chain[2]=m.pawn_class+0x30;
    m.button_chain[0]=m.native_button+0x30; m.button_chain[1]=m.button_class+0x30;
    m.screen_chain[0]=m.native_screen+0x30; m.screen_chain[1]=m.native_pause_screen+0x30; m.screen_chain[2]=m.screen_class+0x30;
    type_class(m.native_hand,0x570,0,0); type_class(m.hand_class,sizeof m.hand,1,m.hand_chain);
    type_class(m.native_pawn,0xbb0,0,0); type_class(m.native_pause,0x11b8,1,0); type_class(m.pawn_class,sizeof m.pawn,2,m.pawn_chain);
    type_class(m.native_button,0x330,0,0); type_class(m.button_class,sizeof m.button,1,m.button_chain);
    type_class(m.native_screen,0x248,0,0); type_class(m.native_pause_screen,0x258,1,0); type_class(m.screen_class,sizeof m.screen,2,m.screen_chain);
    *(void**)(m.hand+0x10)=*(void**)(m.left+0x10)=*(void**)(m.inactive+0x10)=m.hand_class;
    *(void**)(m.pawn+0x10)=m.pawn_class; *(void**)(m.button+0x10)=m.button_class; *(void**)(m.screen+0x10)=m.screen_class;
    *(unsigned long long*)(m.hand+0x18)=0x10000002ULL; *(unsigned long long*)(m.pawn+0x18)=0x30000004ULL;
    *(void**)(m.button+0xa0)=m.actor; *(void**)(m.button+0x308)=m.screen;
    *(void**)(m.pawn+0x11b0)=m.screen; m.button[0x320]=2;
    m.hand[0x314]=m.inactive[0x314]=1; m.left[0x314]=0; m.game_hand[0x33d]=1;
    *(void***)(m.hand)=*(void***)(m.left)=*(void***)(m.inactive)=m.hand_vtable;
    *(void***)(m.button)=m.button_vtable;
    m.hand_vtable[0x218/8]=mg_actor_event_address=(void*)press;
    m.button_vtable[0x470/8]=mg_visible_address=(void*)enabled_getter;
    mg_hand_class=hand_class_getter; mg_pawn_class=pawn_class_getter;
    mg_button_class=button_class_getter; mg_screen_class=screen_class_getter;
    mg_hover=native_hover; mg_enabled=enabled_getter; mg_press=press; mg_release=release;
    phone_player=native_player; phone_active=active_getter; phone_type=type_getter;
    unpause_pawn_class=pause_class_getter; unpause_screen_class=pause_screen_getter; unpause_try=back;
    g_touch_gt_tid=input_test_thread=41; native_type=g_touch_context=2; native_active=m.pawn;
    g_menu_tick_ms=now_ms(); g_menu_grip_ready=1; hover_success=receiver_enabled=1;
    g_touch_weapon=2; g_grip_latched=OB_RHAND;
    presses=releases=hovers=native_getters=back_calls=event_count=0; back_accept=1;
}
static void down(void) { send_touch(0,1,930,520); }
static void due(void) { g_menu_pointer.press_ms=now_ms()-1; g_menu_tick_ms=now_ms(); }
static void neutral_rt(void) { unsigned char state[96]; fill_controller_state(state,3); assert(((float*)state)[5]==0 && ((float*)state)[7]==1); }
static void held(void) {
    down(); g_menu_pointer.press_ms=now_ms()+60000; touch_menu_grip_tick(m.hand); assert(!presses && !hovers);
    due(); neutral_rt(); touch_menu_grip_tick(m.hand);
    assert(presses==1 && !releases && m.hand[0x33d] && mg_owned.hand==m.hand && g_menu_pointer.action==MP_GRIP);
}
static void verify_preserved(void) { assert(g_grip_latched==OB_RHAND && g_touch_weapon==2 && m.game_hand[0x33d]==1); }
static void fallback(void) {
    down(); due(); touch_menu_grip_tick(m.hand); assert(!presses && !releases && g_menu_pointer.action==MP_TRIGGER);
    unsigned char state[96]; fill_controller_state(state,3); assert(((float*)state)[5]==1 && ((float*)state)[7]==1);
    send_touch(2,1,930,520); fill_controller_state(state,3); assert(((float*)state)[5]==0);
    verify_preserved();
}
static void rejected_hand(void* hand) { down(); due(); touch_menu_grip_tick(hand); assert(!presses && !hovers && !releases); verify_preserved(); }
#ifdef MENU_GRIP_NATIVE_FIXTURE
#include MENU_GRIP_NATIVE_FIXTURE
#endif
int main(int argc,char** argv) {
    (void)argv;
    if (argc==999) install_touch_menu_grip(0);
#ifdef MENU_GRIP_NATIVE_FIXTURE
    verify_native_installer();
#endif
    ready(); held(); neutral_rt(); send_touch(1,1,1280,740); touch_menu_grip_tick(m.hand);
    assert(presses==1 && !releases && g_ptr_u==1280/g_tw); neutral_rt();
    send_touch(2,1,1280,740); touch_menu_grip_tick(m.hand); assert(releases==1 && !m.hand[0x33d]);
    touch_menu_grip_tick(m.hand); assert(releases==1 && presses==1); neutral_rt(); verify_preserved();

    ready(); down(); send_touch(2,1,930,520); assert(g_menu_pointer.tap && !g_menu_pointer.down);
    g_menu_pointer.press_ms=g_tap_press_ms=now_ms()-1; g_menu_pointer.release_ms=g_tap_until_ms=now_ms()+60000;
    neutral_rt(); touch_menu_grip_tick(m.hand); assert(presses==1); neutral_rt();
    g_menu_pointer.release_ms=g_tap_until_ms=now_ms()-1; touch_menu_grip_tick(m.hand);
    assert(releases==1); neutral_rt(); verify_preserved();

    ready(); hover_success=0; down(); send_touch(2,1,930,520);
    long delay=g_menu_pointer.press_ms-g_vt[0].t0ms;
    assert(delay==120 && g_menu_pointer.release_ms-g_menu_pointer.press_ms==150);
    g_menu_pointer.press_ms=g_tap_press_ms=now_ms()-1;
    g_menu_pointer.release_ms=g_tap_until_ms=now_ms()+60000;
    neutral_rt(); touch_menu_grip_tick(m.hand); assert(!presses && g_menu_pointer.action==MP_TRIGGER);
    unsigned char tap_state[96]; fill_controller_state(tap_state,3); assert(((float*)tap_state)[5]==1);
    g_menu_pointer.release_ms=g_tap_until_ms=now_ms()-1; fill_controller_state(tap_state,3);
    assert(((float*)tap_state)[5]==0 && !presses && !releases); verify_preserved();

    ready(); hover_success=0; fallback(); ready(); receiver_enabled=0; fallback();
    ready(); m.button[0x320]=0; fallback(); ready(); *(void**)(m.button+0xa0)=m.other; fallback();
    ready(); *(void**)(m.button+0x308)=m.other; fallback();
    ready(); *(int*)(m.button_class+0x58)=0x32f; fallback();
    ready(); m.button_chain[0]=m.native_screen+0x30; fallback();
    ready(); *(void**)(m.button+0x10)=0; fallback();
    ready(); m.button_vtable[0x470/8]=0; fallback();
    ready(); *(void**)(m.screen+0x10)=0; fallback();
    ready(); m.screen_chain[0]=m.native_pawn+0x30; fallback();
    ready(); *(int*)(m.screen_class+0x58)=0x247; fallback();

    ready(); rejected_hand(0); ready(); rejected_hand(m.left); ready(); rejected_hand(m.inactive);
    ready(); down(); due(); unsigned pending_serial=g_menu_pointer.serial;
    touch_menu_grip_tick(m.left); touch_menu_grip_tick(m.inactive);
    assert(g_menu_pointer.serial==pending_serial && g_menu_pointer.action==MP_PENDING && !presses);
    touch_menu_grip_tick(m.hand); assert(presses==1); touch_menu_grip_tick(m.hand); assert(presses==1);
    send_touch(2,1,930,520); touch_menu_grip_tick(m.hand); assert(releases==1); verify_preserved();
    ready(); *(int*)(m.hand_class+0x58)=0x56f; rejected_hand(m.hand);
    ready(); m.hand_chain[0]=m.native_button+0x30; rejected_hand(m.hand);
    ready(); *(int*)(m.native_hand+0x38)=129; rejected_hand(m.hand);
    ready(); m.hand_vtable[0x218/8]=0; rejected_hand(m.hand);
    ready(); native_type=1; rejected_hand(m.hand); ready(); native_type=3; rejected_hand(m.hand);
    ready(); native_type=5; rejected_hand(m.hand); ready(); native_active=m.other; rejected_hand(m.hand);
    ready(); g_touch_gt_tid=0; rejected_hand(m.hand); assert(!native_getters);
    ready(); input_test_thread=42; rejected_hand(m.hand); assert(!native_getters);
    ready(); g_menu_grip_ready=0; rejected_hand(m.hand); assert(!native_getters);

    // Cancel before the actual native Back seam, including native modal refusal.
    ready(); held(); tap(C_BACK); assert(g_touch_back_request && !g_menu_pointer.serial && m.hand[0x33d]);
    touch_unpause_tick(m.hand); assert(presses==1 && releases==1 && back_calls==1 && !g_touch_back_request);
    assert(event_count==3 && events[0]==1 && events[1]==2 && events[2]==3); verify_preserved();
    touch_unpause_tick(m.hand); assert(back_calls==1 && releases==1);
    ready(); held(); back_accept=0; tap(C_BACK); touch_unpause_tick(m.hand);
    assert(releases==1 && back_calls==1 && !g_touch_back_request); verify_preserved();
    ready(); held(); touch_menu_grip_cancel(m.inactive); assert(!releases && mg_owned.hand==m.hand);
    touch_menu_grip_cancel(m.left); assert(!releases); touch_menu_grip_cancel(m.hand); assert(releases==1); verify_preserved();

    ready(); held(); touch_reset(); touch_menu_grip_tick(m.hand); assert(releases==1 && !m.hand[0x33d]);
    touch_menu_grip_tick(m.hand); assert(releases==1 && presses==1);
    ready(); held(); native_type=g_touch_context=1; unsigned char state[96]; fill_controller_state(state,3);
    touch_menu_grip_tick(m.hand); assert(!releases && !((float*)state)[5]);
    native_type=g_touch_context=2; g_menu_tick_ms=now_ms(); touch_menu_grip_tick(m.hand); assert(releases==1); verify_preserved();
    ready(); held(); native_active=m.other; touch_menu_grip_tick(m.hand); assert(!releases);
    native_active=m.pawn; send_touch(2,1,1280,740); touch_menu_grip_tick(m.hand); assert(releases==1); verify_preserved();
    // Reused pointer/new full FName and different active pawn never invoke a former object's release.
    ready(); held(); *(unsigned long long*)(m.hand+0x18)^=1ULL<<32; touch_menu_grip_tick(m.hand);
    assert(!releases && !mg_owned.hand && !g_menu_pointer.serial); verify_preserved();
    ready(); held(); *(unsigned long long*)(m.pawn+0x18)^=1ULL<<32; touch_menu_grip_tick(m.hand);
    assert(!releases && !mg_owned.hand && !g_menu_pointer.serial); verify_preserved();
    ready(); down(); due(); mg_seen_hand=(void*)1; mg_seen_pawn=(void*)1; touch_menu_grip_tick(m.hand);
    assert(!presses && !releases && !g_menu_pointer.serial); verify_preserved();
    ready(); held(); mg_owned.hand=(void*)1; touch_menu_grip_tick(m.hand); assert(!releases && !mg_owned.hand); verify_preserved();
    ready(); native_type=g_touch_context=4; held(); send_touch(2,1,1280,740); touch_menu_grip_tick(m.hand); assert(releases==1);
    ready(); native_type=g_touch_context=7; held(); send_touch(2,1,1280,740); touch_menu_grip_tick(m.hand); assert(releases==1);
    puts("PASS: actual input/menuGrip/Back C; native callbacks outside mutex, single Grip without RT, hold/MOVE/UP, fast tap, generic fallback, typed/owner/thread guards, release before native Back/modal refusal, focus/context/reentry/fullName/stale-pointer cancellation, preserved gameplay RG/weapon/grip and no repeat");
    return 0;
}
