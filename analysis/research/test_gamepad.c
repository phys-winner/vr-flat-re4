#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef int pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER 0
static int pthread_mutex_lock(pthread_mutex_t* mutex) { assert(!*mutex); *mutex = 1; return 0; }
static int pthread_mutex_unlock(pthread_mutex_t* mutex) { assert(*mutex); *mutex = 0; return 0; }
static pthread_mutex_t g_imx;
static unsigned g_pad_held_ctl;
static float g_pad_move[2], g_look_yaw, g_look_pitch;
static volatile int g_app_input_focus = 1;
static long clock_ms = 1000, g_last_touch_ms, g_aim_painted_ms, g_menu_tap_ms;
static long g_cutscene_skippable_ms, g_cutscene_skip_request, g_qturn_request_ms;
static int g_touch_context = 1, g_touch_cancel, g_touch_aim, g_aim_latched;
static float g_ptr_u = .5f, g_ptr_v = .5f, g_tw = 2400, g_th = 1080;
static long g_ptr_until_ms;
enum { C_FIRE, C_INTERACT, C_BACK, C_INVENTORY, C_AIM, C_PAUSE, C_KNIFE, C_WEAPON, C_RELOAD, C_PHONE, C_CONFIRM,
       C_SETTINGS, C_WEAPON2, C_GRENADE, C_HEAL, C_ASHLEY, C_N };
static long g_btn_hold_until[C_N];
static int clicks[C_N], hidden[C_N], m_menu, aim_toggle, ready, selected, throwing, backs, recomputes;
static int grenade_down, grenade_up, pointer_clicks, axis_passthrough, button_passthrough;
static uint64_t fv_action_snapshot;
static float applied_move;
static long ptr_now_ms(void) { return clock_ms; }
static int menu_mode(void) { return m_menu; }
static void sync_touch_context(int menu) { (void)menu; }
static int ctl_visible(int ctl, int menu) { (void)menu; return !hidden[ctl]; }
#include "title_snapshot_under_test.h"
static void touch_button_press(int ctl, int menu) { (void)menu; ++clicks[ctl]; if (ctl == C_AIM && aim_toggle) g_aim_latched = !g_aim_latched; }
static void pointer_click(float x, float y, int ms) { assert(x >= 0 && x <= g_tw && y >= 0 && y <= g_th && ms == 100); ++pointer_clicks; }
static int touch_mobile_back(void) { ++backs; return 1; }
struct touch_mobile_settings { float sensitivity; };
static struct touch_mobile_settings touch_mobile_settings_snapshot(void) { return (struct touch_mobile_settings){1}; }
static int mobile_prefs_assist(void) { return 0; }
static int mobile_prefs_aim_toggle(void) { return aim_toggle; }
struct touch_quickselect_state { int selected, fire_side; };
static struct touch_quickselect_state touch_quickselect_state(long now) { (void)now; return (struct touch_quickselect_state){selected, 0}; }
static int touch_quickselect_ready(void) { return ready; }
static int touch_grenade_fire_down(long now) { (void)now; ++grenade_down; return 1; }
static void touch_grenade_fire_up(long now) { (void)now; ++grenade_up; }
static int touch_throw_mode(long now) { (void)now; return throwing; }
static void recompute(void) { ++recomputes; applied_move = g_pad_move[0]; g_touch_aim = g_aim_latched || (g_pad_held_ctl & (1u << C_AIM)) != 0; }
static void gate_pad_axis(int device, int axis, float value) { (void)device; (void)axis; (void)value; ++axis_passthrough; }
static void gate_pad_button(int device, int button, int down) { (void)device; (void)button; (void)down; ++button_passthrough; }
static int patch_code(void* target, void* hook) { (void)target; (void)hook; return 0; }
#define LOGF(...) ((void)0)
#include "gamepad_under_test.h"

static void step(int ms) { clock_ms += ms; touch_gamepad_frame(); }
static void reset(void) {
    touch_gamepad_reset();
    memset(clicks, 0, sizeof clicks); memset(hidden, 0, sizeof hidden);
    memset(g_btn_hold_until, 0, sizeof g_btn_hold_until);
    g_touch_context = 1; m_menu = aim_toggle = ready = selected = throwing = 0;fv_action_snapshot=0;
    g_touch_cancel = 0; g_app_input_focus = 1;
    grenade_down = grenade_up = pointer_clicks = backs = 0;
    step(16);
}
int main(void) {
    reset();
    hook_pad_button(7,96,1);step(16);
    assert(clicks[C_INTERACT]==1 && (g_pad_held_ctl&(1u<<C_INTERACT)));
    step(9000);assert(clicks[C_INTERACT]==1 && (g_pad_held_ctl&(1u<<C_INTERACT)));
    hook_pad_button(7,96,0);step(16);assert(!(g_pad_held_ctl&(1u<<C_INTERACT)));
    reset();hidden[C_INTERACT]=1;
    hook_pad_button(7,96,1);step(16);assert(!clicks[C_INTERACT] && !(g_pad_held_ctl&(1u<<C_INTERACT)));
    reset();hook_pad_button(7,96,1);step(16);g_touch_context=3;step(16);
    assert(!(g_pad_held_ctl&(1u<<C_INTERACT)));
    reset();
    hook_pad_axis(7, 0, .59f); step(16); assert(fabsf(applied_move - .5f) < 1e-5f);
    step(9000); assert(fabsf(applied_move - .5f) < 1e-5f && touch_gamepad_recent(clock_ms));
    hook_pad_axis(7, 0, 0); step(16); assert(!applied_move);
    hook_pad_axis(7, 11, 1); step(100); assert(g_look_yaw < 0);
    hook_pad_axis(7, 11, NAN); step(16); assert(!g_pad.rx);
    hook_pad_axis(7, 1, INFINITY); assert(!g_pad.ly);
    hook_pad_axis(7, 0, 4); assert(g_pad.lx == 1);
    hook_pad_axis(7, 98, 1); hook_pad_button(7, 66, 1);
    assert(axis_passthrough == 1 && button_passthrough == 1);

    reset();
    hook_pad_axis(7, 18, 1); hook_pad_axis(7, 18, 0); step(16);
    assert(clicks[C_FIRE] == 1 && g_btn_hold_until[C_FIRE] == clock_ms + 90);
    assert(!(g_pad_held_ctl & (1u << C_FIRE)));
    hook_pad_axis(7, 18, 1); hook_pad_button(7, 105, 1); step(16);
    assert(clicks[C_FIRE] == 2 && (g_pad_held_ctl & (1u << C_FIRE)));
    hook_pad_axis(7, 18, 0); step(16); assert(g_pad_held_ctl & (1u << C_FIRE));
    hook_pad_button(7, 105, 0); step(16); assert(!(g_pad_held_ctl & (1u << C_FIRE)));
    hook_pad_axis(7, 17, 1); hook_pad_axis(7, 23, 1); step(16); assert(g_touch_aim);
    hook_pad_axis(7, 17, 0); step(16); assert(g_touch_aim);
    hook_pad_axis(7, 23, 0); step(16); assert(!g_touch_aim);
    hook_pad_axis(7, 22, 1); step(9000); assert(g_pad_held_ctl & (1u << C_FIRE));
    hook_pad_axis(7, 22, 0); step(16); assert(!(g_pad_held_ctl & (1u << C_FIRE)));
    hook_pad_axis(7, 16, -1); hook_pad_axis(7, 16, 0); step(16); assert(clicks[C_ASHLEY] == 1);
    hook_pad_button(7, 20, 1); step(16); assert(clicks[C_HEAL] == 1);

    reset(); ready = 1; selected = 2;
    hook_pad_axis(7, 18, 1); hook_pad_axis(7, 18, 0); step(16);
    assert(grenade_down == 1 && grenade_up == 1 && !clicks[C_FIRE] && !g_btn_hold_until[C_FIRE]);
    reset(); throwing = 1;
    hook_pad_button(7, 105, 1); step(16); assert(clicks[C_FIRE] == 1 && !g_btn_hold_until[C_FIRE]);

    reset();
    hook_pad_axis(7, 0, 1); hook_pad_axis(7, 17, 1); hook_pad_axis(7, 18, 1); step(16);
    assert(applied_move == 1 && g_touch_aim && g_pad_held_ctl);
    Java_com_epicgames_ue4_MobileGamepad_nativeReset(0, 0, 99); step(16); assert(applied_move == 1);
    int before = recomputes;
    Java_com_epicgames_ue4_MobileGamepad_nativeReset(0, 0, 7);
    assert(!applied_move && !g_touch_aim && !g_pad_held_ctl && !g_pad.buttons && recomputes > before);
    step(16); assert(!applied_move && !g_touch_aim && !g_pad_held_ctl);
    g_app_input_focus = 0;
    hook_pad_axis(7, 0, 1); hook_pad_button(7, 105, 1); step(16);
    assert(g_pad_device == -1 && !applied_move && !g_pad_held_ctl);
    g_app_input_focus = 1; step(16); assert(!applied_move && !g_pad_held_ctl);
    hook_pad_button(7, 105, 1); step(16); assert(g_pad_held_ctl & (1u << C_FIRE));

    reset();
    hook_pad_axis(7, 0, 1); hook_pad_axis(7, 17, 1); hook_pad_axis(7, 18, 1); step(16);
    g_touch_context = 2; m_menu = 1; step(16);
    assert(!applied_move && !g_pad_held_ctl && !g_touch_aim);
    g_touch_context = 1; m_menu = 0; step(16);
    assert(!applied_move && !g_pad_held_ctl && !g_touch_aim);
    step(16); assert(!applied_move && !g_pad_held_ctl);
    hook_pad_axis(7, 0, 0); hook_pad_axis(7, 17, 0); hook_pad_axis(7, 18, 0); step(16);
    hook_pad_axis(7, 0, 1); hook_pad_axis(7, 17, 1); hook_pad_axis(7, 18, 1); step(16);
    assert(applied_move == 1 && g_touch_aim && (g_pad_held_ctl & (1u << C_FIRE)));

    reset(); g_touch_context = 2; m_menu = 1; step(16);
    hook_pad_button(7, 96, 1); step(16); assert(pointer_clicks == 1);
    hook_pad_button(7, 96, 0); hook_pad_button(7, 97, 1); step(16); assert(backs == 1);
    hook_pad_axis(7, 0, 1); hook_pad_axis(7, 1, -1);
    for (int i = 0; i < 100; ++i) step(16);
    assert(g_ptr_u == .98f && g_ptr_v == .02f);
    for(int mask=1;mask<=5;mask+=4) {
        reset();g_touch_context=7;m_menu=1;fv_action_snapshot=((uint64_t)clock_ms<<8)|(7u<<4)|8u|mask;step(16);
        hook_pad_button(7,96,1);hook_pad_button(7,96,0);step(16);
        assert(g_btn_hold_until[C_CONFIRM]==clock_ms+150&&!pointer_clicks);
        long pulse=g_btn_hold_until[C_CONFIRM];step(16);assert(g_btn_hold_until[C_CONFIRM]==pulse);
        hook_pad_button(7,96,1);step(16);pulse=g_btn_hold_until[C_CONFIRM];step(1000);
        assert(g_btn_hold_until[C_CONFIRM]==pulse&&pulse<clock_ms&&!pointer_clicks);
    }
    reset();g_touch_context=7;m_menu=1;step(16);
    hook_pad_button(7,96,1);step(16);assert(pointer_clicks==1&&!g_btn_hold_until[C_CONFIRM]);
    reset();g_touch_context=7;m_menu=1;fv_action_snapshot=((uint64_t)clock_ms<<8)|(7u<<4)|9u;hidden[C_CONFIRM]=1;step(16);
    hook_pad_button(7,96,1);step(16);assert(pointer_clicks==1&&!g_btn_hold_until[C_CONFIRM]);
    for(unsigned failure=0;failure<4;failure++) {
        reset();g_touch_context=7;m_menu=1;
        long stamp=clock_ms+(failure==0?-401:failure==1?100:0);
        fv_action_snapshot=((uint64_t)stamp<<8)|((failure==2?2u:7u)<<4)|(failure==3?1u:9u);
        step(16);hook_pad_button(7,96,1);step(16);
        assert(pointer_clicks==1&&!g_btn_hold_until[C_CONFIRM]);
    }
    reset(); g_touch_context = 5; step(16);
    hook_pad_button(7, 96, 1); step(16); assert(clicks[C_PHONE] == 1);
    reset(); g_touch_context = 3; step(16);
    hook_pad_button(7, 96, 1); step(16); assert(clicks[C_INTERACT] == 1);
    g_cutscene_skippable_ms = clock_ms + 1000; step(1000);
    assert(g_cutscene_skip_request == clock_ms);
    long request = g_cutscene_skip_request; step(16); assert(request == g_cutscene_skip_request);
    reset(); aim_toggle = 1;
    hook_pad_axis(7, 17, 1); step(16); assert(g_touch_aim);
    hook_pad_axis(7, 17, 0); step(16); assert(g_touch_aim);
    hook_pad_axis(7, 17, 1); step(16); assert(!g_touch_aim);
    puts("PASS: production pad axis/key mapping, trigger/hat pulse edges, 9s holds, analog/digital aggregation, bounds, native title A150ms once-only pulse/start+skip, ordinary menu pointer, phone/cinema actions, focus/disconnect release, context neutral rearm, tap aim and original native passthrough");
    return 0;
}
