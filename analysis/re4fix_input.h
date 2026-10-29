#include <time.h>
#include "re4fix_overlay.h"
#include "overlay/overlay_cells.h"
// Mobile actions are translated to the game's existing hand and pawn input internally.
// Included from re4fix.c after the compositor. Coordinates are game-surface pixels (= swapchain extent).
// Modern fullscreen uses 2400x1080 on the test phone; legacy surface was 2167x1080 with a 171 px left inset.
//
// Gameplay mode:
//   * left half (outside buttons): floating move stick (base appears under the thumb) -> left thumbstick
//   * right half (outside buttons): drag = look; direct taps click menu entries only
//   * action icons: fire, hold to aim, reload, knife, weapon, interact, inventory and pause
// Menus use direct touch, confirm/back; the communicator has its own answer icon.

// ---- virtual control state ----
static unsigned touch_weapon_cycle_input_mask(long now,int menu);
static void touch_weapon_cycle_input_reset(void);
static volatile float g_lstick_x, g_lstick_y, g_rstick_x, g_rstick_y;
static volatile unsigned g_buttons;            // ovrpButton bits from held on-screen buttons
static volatile float g_trig_l, g_trig_r, g_grip_l, g_grip_r;
static float g_last_lx, g_last_ly, g_last_rt, g_last_rg;
static unsigned g_last_mask, g_last_bits;
static float g_tw = 2167.0f, g_th = 1080.0f;   // surface size (refreshed from the swapchain extent)
static int g_autopress = 0;                    // input experiment switch

// ovrpButton bits (OVRPlugin 1.49); trigger/grip "bits" are ours, the analog values are what UE4 reads
#define OB_A 0x1u
#define OB_B 0x2u
#define OB_RTHUMB 0x4u
#define OB_X 0x100u
#define OB_Y 0x200u
#define OB_LTHUMB 0x400u
#define OB_START  0x00100000u
#define OB_RINDEX 0x04000000u
#define OB_RHAND  0x08000000u
#define OB_LINDEX 0x10000000u
#define OB_LHAND  0x20000000u

// ---- on-screen controls: centre (cx*W, cy*H), radius r*H ----
struct ctl { float cx, cy, r; int cell; unsigned bit; int menu; };
enum { C_FIRE, C_INTERACT, C_BACK, C_INVENTORY, C_AIM, C_PAUSE, C_KNIFE, C_WEAPON, C_RELOAD, C_PHONE, C_CONFIRM,
       C_SETTINGS, C_WEAPON2, C_GRENADE, C_HEAL, C_ASHLEY, C_N };
static const struct ctl g_ctl[C_N] = {
    /*  cx      cy      r      cell        bit        menu */
    { 0.895f, 0.610f, 0.093f, OVL_FIRE,      OB_RINDEX, 0 },
    { 0.735f, 0.850f, 0.072f, OVL_INTERACT,  0,         0 },
    { 0.935f, 0.840f, 0.072f, OVL_BACK,      OB_B,      1 },
    { 0.130f, 0.100f, 0.045f, OVL_INVENTORY, 0,         0 },
    { 0.735f, 0.610f, 0.065f, OVL_AIM,       0,         0 },
    { 0.055f, 0.100f, 0.045f, OVL_PAUSE,     0,         0 },
    { 0.600f, 0.120f, 0.044f, OVL_KNIFE,     0,         0 },
    { 0.680f, 0.120f, 0.044f, OVL_WEAPON,    0,         0 },
    { 0.925f, 0.850f, 0.066f, OVL_RELOAD,    0,         0 },
    { 0.795f, 0.840f, 0.072f, OVL_PHONE,     0,         1 },
    { 0.795f, 0.840f, 0.072f, OVL_CONFIRM,   OB_A,      1 },
    { 0.055f, 0.100f, 0.045f, OVL_SETTINGS,  0,         1 },
    { 0.760f, 0.120f, 0.044f, OVL_WEAPON2,   0,         0 },
    { 0.840f, 0.120f, 0.044f, OVL_GRENADE,   0,         0 },
    { 0.920f, 0.120f, 0.044f, OVL_HEAL,      0,         0 },
    { 0.520f, 0.120f, 0.044f, OVL_ASHLEY_FOLLOW, 0,     0 },   // Ashley Wait/Follow (only while she can be ordered)
};
#define STICK_DEF_U 0.105f      // idle stick base position
#define STICK_DEF_V 0.790f
#define STICK_R     0.110f      // stick travel radius (units of H)
#define LOOK_SENS   0.0026f     // radians per surface pixel

#define RT_NONE 0
#define RT_LSTICK 1
#define RT_RSTICK 2             // look / pointer
#define RT_MENU_POINTER (-1)    // one owned native menu pointer, never gameplay look
#define RT_PINCH (-3)           // two fingers on the pause-menu map: zoom
#define RT_WATCH (-4)           // hold the health dial to raise the wrist; a drag becomes movement
#define RT_BTN 3                // + control index
#define MAXT 10
struct vt { int on, h, role, grenade, skip; float x0, y0, x, y, lx, ly; long t0ms; };
static struct vt g_vt[MAXT];
static struct { int active, h1, h2; float last_d; } g_pinch;
static pthread_mutex_t g_imx = PTHREAD_MUTEX_INITIALIZER;
// Gamepad (re4fix_gamepad.h): controls held by the pad and its dead-zoned left stick, both under g_imx.
static unsigned g_pad_held_ctl;
static float g_pad_move[2];
static volatile long g_last_touch_ms;     // last touch event: a finger brings the touch HUD back at once
static void touch_gamepad_frame(void);
static int touch_gamepad_recent(long now);
static int touch_gamepad_hud_hidden(long now);
static long g_tlog;
static volatile long g_tap_press_ms, g_tap_until_ms;   // pointer click: right trigger pulse window
static volatile long g_btn_hold_until[C_N];            // minimum press time for quick taps on buttons
static int g_fire_slot=-1,g_fire_side=-1;
static long g_knife_swing_until;
static int g_watch_held;
static int g_aim_latched;                     // tap-to-aim state (native menu "Aiming: Tap")
static void touch_world_tap(float x, float y);
static int touch_prompt_hit(float x, float y);
static int touch_tutorial_gesture(int z, long now);   // re4fix_tutorial.h
static int touch_tutorial_reload_hint(long now);
static int mobile_prefs_aim_toggle(void);
static int mobile_prefs_assist(void);
static int mobile_prefs_qturn(void);
static int mobile_prefs_aim_walk(void);
static int mobile_prefs_gyro(void);
static int mobile_prefs_gyro_sensitivity(void);
static void touch_health_flat_presented(long now);
// Hold-to-skip (cutscenes): the finger must stay still this long; the ring fills after a short grace period.
#define SKIP_HOLD_GRACE_MS 200
#define SKIP_HOLD_FILL_MS 700
static int touch_reload_user_fire(long now) {
    pthread_mutex_lock(&g_imx);
    int held=(g_buttons & (OB_RINDEX|OB_LINDEX))!=0 || now<g_btn_hold_until[C_FIRE];
    pthread_mutex_unlock(&g_imx);
    return held;
}
enum { MP_CANCEL=-2, MP_PENDING=-1, MP_TRIGGER=0, MP_GRIP=2 };
static struct { int down, h, pressed, action, tap; unsigned serial; long press_ms,release_ms; } g_menu_pointer;
static unsigned g_menu_pointer_serial;
static volatile int g_menu_grip_ready;
struct menu_pointer_request { unsigned serial; int active,pressed,action; long press_ms; };
static int g_input_context = -1, g_input_menu;
// GameThread-only identity of the last actual Bio4 pawn. Cached pointers stay opaque.
static void* g_touch_last_game_pawn;
static unsigned long long g_touch_last_game_name;
static void touch_refresh_context_gt(void) {
    // The native getter consults GUObjectArray; only the hook-recorded GameThread may call it.
    int gt = __atomic_load_n(&g_touch_gt_tid, __ATOMIC_ACQUIRE);
    if (gt > 0 && gettid() == gt && phone_type)
        __atomic_store_n(&g_touch_context, (int)phone_type(), __ATOMIC_RELEASE);
}
static long now_ms(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L; }
// Native menu hands of the ACTIVE pawn tick only while an in-world menu is open; this includes
// Bio4 (context 1) overlays such as the shooting-range quit prompt. Cutscenes (3) keep gameplay rules.
static int menu_mode(void){ return g_touch_context == 5 || (g_touch_context != 3 && (now_ms() - g_menu_tick_ms) < 300); }
static int menu_grip_context(void){ return g_touch_context==2 || g_touch_context==4 || g_touch_context==7; }
static void sync_touch_context(int menu);
static int menu_pointer_active(long now){
    return g_menu_pointer.serial && (g_menu_pointer.down || (g_menu_pointer.tap && now<g_menu_pointer.release_ms));
}
static void menu_pointer_cancel_locked(void) {
    for (int i=0;i<MAXT;i++) if (g_vt[i].on && g_vt[i].role==RT_MENU_POINTER)
        g_vt[i].role=RT_NONE;
    memset(&g_menu_pointer,0,sizeof g_menu_pointer);
    g_ptr_down=0; g_ptr_until_ms=0; g_tap_press_ms=g_tap_until_ms=0;
}
// These two local seams never call native code while holding the input mutex.
static void touch_menu_pointer_snapshot(struct menu_pointer_request* out) {
    pthread_mutex_lock(&g_imx);
    int menu=menu_mode(); sync_touch_context(menu);
    out->serial=g_menu_pointer.serial; out->pressed=g_menu_pointer.pressed;
    out->action=g_menu_pointer.action; out->press_ms=g_menu_pointer.press_ms;
    out->active=menu && menu_grip_context() && menu_pointer_active(now_ms());
    pthread_mutex_unlock(&g_imx);
}
static int touch_menu_pointer_choose(unsigned serial,int action) {
    int claim=0;
    pthread_mutex_lock(&g_imx);
    long now=now_ms(); int menu=menu_mode(); sync_touch_context(menu);
    if (action==MP_CANCEL && serial==g_menu_pointer.serial) {
        menu_pointer_cancel_locked();
    }
    if ((action==MP_TRIGGER || action==MP_GRIP) && serial==g_menu_pointer.serial
        && menu && menu_grip_context() && menu_pointer_active(now)
        && now>=g_menu_pointer.press_ms && !g_menu_pointer.pressed) {
        if (g_menu_pointer.action==MP_PENDING) g_menu_pointer.action=action;
        if (g_menu_pointer.action==MP_GRIP) { g_menu_pointer.pressed=1; claim=1; }
    }
    pthread_mutex_unlock(&g_imx);
    return claim;
}
static int ctl_visible(int z, int menu) {
    if (g_touch_context == 5) return z == C_PHONE && (touch_phone_answer_visible(now_ms()) || touch_phone_skip_visible(now_ms()));
    // Phone settings live in the native Options menu; the old gear button is retired.
    if (z == C_SETTINGS) return 0;
    if (g_touch_context == 2) return z == C_BACK;
    if (menu) {
        unsigned mask=touch_menu_toolbar_mask(now_ms());
        return (z==C_CONFIRM && (mask&1u)) || (z==C_BACK && (mask&2u));
    }
    if (g_touch_context == 3) {
        struct mobile_action_state action;
        return z==C_INTERACT && touch_action_state(now_ms(),&action);
    }
    if (g_touch_context != 1) return 0;
    if(touch_motion_pose_busy())return z==C_PAUSE || z==C_INTERACT;
    if (z == C_BACK || z == C_PHONE || z == C_CONFIRM || z==C_SETTINGS) return 0;
    if (z==C_WEAPON2 || z==C_GRENADE || z==C_HEAL) {
        struct touch_quickselect_state state=touch_quickselect_state(now_ms());
        int slot=z==C_WEAPON2 ? 1 : z==C_GRENADE ? 2 : 6;
        // No native favourites (first-run tutorial): the healing item is grabbed from its own holster.
        if (z==C_HEAL && !touch_quickselect_live(now_ms()))
            return g_touch_weapon==3 || now_ms()-__atomic_load_n(&g_heal_holster_ms,__ATOMIC_ACQUIRE)<1000;
        return (state.available&(1u<<slot)) || state.selected==slot;
    }
    if ((z == C_AIM || z == C_RELOAD) && g_touch_weapon != 2) return 0;
    if (z == C_ASHLEY) return touch_ashley_state(now_ms()).available;
    return 1;
}
static unsigned ctl_bit(int z) {
    if(z==C_FIRE && touch_motion_pose_busy())return 0;
    if(z==C_FIRE && touch_throw_mode(now_ms()))return 0;   // spear/lantern: Fire throws instead of pulling a trigger
    if (z==C_BACK && g_touch_context==2)return 0;
    if (z==C_FIRE && touch_quickselect_live(now_ms())) {
        struct touch_quickselect_state state=touch_quickselect_state(now_ms());
        if (state.selected<0 || state.fire_side<0 || state.selected==2 || touch_quickselect_busy()
            || state.selected!=g_fire_slot || state.fire_side!=g_fire_side)return 0;
        return state.fire_side ? OB_RINDEX : OB_LINDEX;
    }
    return g_ctl[z].bit;
}

static float knife_cut_offset(long elapsed) {
    if (elapsed < 0 || elapsed >= 640) return 0;
    if (elapsed < 200) return 1.135f * (float)elapsed / 200;
    if (elapsed < 340) return 1.135f - 1.75f * (float)(elapsed - 200) / 140;
    if (elapsed < 440) return -0.615f;
    return -0.615f + 0.615f * (float)(elapsed - 440) / 200;
}

static void touch_knife_swing(long now) {
    if (now < g_knife_swing_until) return;
    g_anim_len = knife_slash_length();
    g_anim_t0 = now; g_anim_kind = 2;
    g_knife_swing_until = now + g_anim_len;
    LOGF("TOUCH knife swing");
}

static void touch_watch_hold(int held, long now) {
    if (held) {
        if (!g_watch_held) __atomic_store_n(&g_watch_since_ms, now, __ATOMIC_RELEASE);
        __atomic_store_n(&g_watch_until_ms, now + 500, __ATOMIC_RELEASE);
    } else if (g_watch_held) __atomic_store_n(&g_watch_until_ms, now, __ATOMIC_RELEASE);
    g_watch_held = held;
}

static struct ctl ctl_geometry(int z) {
    struct ctl c=g_ctl[z];
    if (z==C_BACK && g_touch_context==2) { c.cx=.965f; c.cy=.925f; c.r=.048f; }
    struct touch_mobile_settings settings=touch_mobile_settings_snapshot();
    float minx=-1,maxx=1,miny=-1,maxy=1;
    int menu=menu_mode();
    // Clamp against the layout, so equipping a weapon cannot move the remaining buttons.
    for(int i=0;i<C_N;i++)if(g_ctl[i].menu==menu) {
        struct ctl a=g_ctl[i];
        if(i==C_BACK && g_touch_context==2){a.cx=.965f;a.cy=.925f;a.r=.048f;}
        float ry=a.r*settings.size*1.15f,rx=ry*g_th/g_tw;
        minx=fmaxf(minx,rx-a.cx);maxx=fminf(maxx,1-rx-a.cx);
        miny=fmaxf(miny,ry-a.cy);maxy=fminf(maxy,1-ry-a.cy);
    }
    c.r*=settings.size;
    c.cx+=fmaxf(minx,fminf(maxx,settings.x));
    c.cy+=fmaxf(miny,fminf(maxy,settings.y));
    return c;
}
static int ctl_at(float x, float y, int menu) {
    for (int i = 0; i < C_N; i++) {
        if (!ctl_visible(i, menu)) continue;
        struct ctl c=ctl_geometry(i);
        float dx = x - c.cx * g_tw, dy = y - c.cy * g_th, rr = c.r * g_th * 1.15f;
        if (dx * dx + dy * dy <= rr * rr) return i;
    }
    return -1;
}
static void sync_touch_context(int menu) {
    int context = g_touch_context;
    if (g_input_context >= 0 && (context != g_input_context || menu != g_input_menu)) {
        // An existing finger cannot become a new action after a screen transition.
        for (int i = 0; i < MAXT; i++) g_vt[i].role = RT_NONE;
        g_pinch.active = 0;
        memset((void*)g_btn_hold_until, 0, sizeof g_btn_hold_until);
        g_tap_press_ms = g_tap_until_ms = 0;
        g_ptr_down = 0; g_ptr_until_ms = 0;
        memset(&g_menu_pointer, 0, sizeof g_menu_pointer);
        g_anim_kind = 0; g_knife_swing_until = 0; g_touch_aim = 0; g_aim_latched = 0;
        g_watch_held = 0; g_watch_since_ms = g_watch_until_ms = 0;
        touch_grenade_cancel();touch_grenade_fire_up(now_ms());
        touch_weapon_cycle_input_reset();
        for(int i=0;i<MAXT;i++)g_vt[i].grenade=0;
        touch_quickselect_cancel();
    }
    g_input_context = context; g_input_menu = menu;
}
static struct vt* vt_find(int h){ for(int i=0;i<MAXT;i++) if(g_vt[i].on&&g_vt[i].h==h) return &g_vt[i]; return 0; }
static struct vt* vt_new(int h){ for(int i=0;i<MAXT;i++) if(!g_vt[i].on){ memset(&g_vt[i],0,sizeof g_vt[i]); g_vt[i].on=1; g_vt[i].h=h; return &g_vt[i]; } return 0; }

static void stick_vec(const struct vt* t, float* ox, float* oy){
    float rad = 58.0f * (float)__atomic_load_n(&g_touch_density_milli,__ATOMIC_ACQUIRE) / 1000.0f * touch_mobile_settings_snapshot().size;
    float dx = t->x - t->x0, dy = t->y - t->y0, m = hypotf(dx,dy);
    float magnitude=fmaxf(0,fminf(1,(m/rad-.12f)/.88f));
    *ox = m>0 ? dx/m*magnitude : 0; *oy = m>0 ? -dy/m*magnitude : 0;
}

static void recompute(void){
    unsigned b = 0; float lx = 0, ly = 0; int aim = 0, watch = 0, menu = menu_mode();
    sync_touch_context(menu);
    if(touch_qte_pose_busy(now_ms())) {
        g_btn_hold_until[C_FIRE]=0;g_fire_slot=g_fire_side=-1;g_aim_latched=0;
        for(int i=0;i<MAXT;i++)if(g_vt[i].role==RT_BTN+C_FIRE || g_vt[i].role==RT_BTN+C_AIM)g_vt[i].role=RT_NONE;
    }
    for (int i = 0; i < MAXT; i++) {
        struct vt* t = &g_vt[i]; if (!t->on) continue;
        if (t->role == RT_WATCH && !menu && g_touch_context == 1) {
            watch = 1;
            float dx=t->x-t->x0,dy=t->y-t->y0;
            if(t->x<.46f*g_tw && dx*dx+dy*dy>900.0f)stick_vec(t,&lx,&ly);
        }
        if (t->role == RT_LSTICK && !menu && g_touch_context != 3) stick_vec(t, &lx, &ly);
        else if (t->role >= RT_BTN && ctl_visible(t->role - RT_BTN, menu)) {
            b |= ctl_bit(t->role - RT_BTN);
            if (t->role == RT_BTN + C_AIM && !mobile_prefs_aim_toggle()) aim = 1;
        }
    }
    // Gamepad: held controls and the left stick when no finger moves.
    for (int z = 0; z < C_N; z++)
        if ((g_pad_held_ctl & (1u << z)) && ctl_visible(z, menu)) {
            b |= ctl_bit(z);
            if (z == C_AIM && !mobile_prefs_aim_toggle()) aim = 1;
        }
    if (!lx && !ly && !menu && g_touch_context == 1) { lx = g_pad_move[0]; ly = g_pad_move[1]; }
    if (!mobile_prefs_aim_toggle() || !ctl_visible(C_AIM, menu)) g_aim_latched = 0;
    else if (g_aim_latched) aim = 1;
    if (aim && mobile_prefs_aim_walk()) {
        float length=hypotf(lx,ly);
        if (length>.45f) { lx*=.45f/length; ly*=.45f/length; }
    }
    g_lstick_x = lx; g_lstick_y = ly; g_rstick_x = 0; g_rstick_y = 0;
    g_trig_r = (b & OB_RINDEX) ? 1.0f : 0.0f; g_trig_l = (b & OB_LINDEX) ? 1.0f : 0.0f;
    g_grip_r = (b & OB_RHAND) ? 1.0f : 0.0f;  g_grip_l = (b & OB_LHAND) ? 1.0f : 0.0f;
    g_buttons = b;
    g_touch_aim = aim;
    long now = now_ms();
    touch_watch_hold(watch, now);
    if (!menu && g_touch_weapon == 1 && ((b & (OB_RINDEX|OB_LINDEX)) || now < g_btn_hold_until[C_FIRE]))
        touch_knife_swing(now);
}

static void touch_weapon_select(int weapon) {
    g_anim_kind = 0; g_knife_swing_until = 0; g_aim_latched = 0;
    __atomic_store_n(&g_mobile_holster_until, 0, __ATOMIC_RELEASE);
    g_btn_hold_until[C_FIRE] = 0;
    g_tap_press_ms = g_tap_until_ms = 0;
    g_reload_phase = 0;
    __atomic_store_n(&g_touch_reload_request, 0, __ATOMIC_RELEASE);
    g_grip_latched &= ~OB_RHAND;
    g_touch_weapon = weapon;
    g_weapon_grip_at = -1;   // begin the delay only after UE receives a released right grip
    __atomic_store_n(&g_touch_weapon_request, weapon, __ATOMIC_RELEASE);
    LOGF("TOUCH weapon=%d", weapon);
}
static void touch_grenade_retire_fingers(void) {
    touch_grenade_fire_up(now_ms());
    for(int i=0;i<MAXT;i++)if(g_vt[i].grenade){g_vt[i].grenade=2;g_vt[i].role=RT_NONE;}
}
static void touch_lower_aim(void) {
    g_touch_aim=0;g_aim_latched=0;g_btn_hold_until[C_AIM]=0;
    g_pad_held_ctl&=~(1u<<C_AIM);
    for(int i=0;i<MAXT;i++)if(g_vt[i].role==RT_BTN+C_AIM)g_vt[i].role=RT_NONE;
}
static void touch_button_press(int z, int menu) {
    if(z==C_INVENTORY || z==C_INTERACT || z==C_PAUSE)touch_lower_aim();
    if ((z==C_KNIFE || z==C_WEAPON || z==C_WEAPON2 || z==C_GRENADE || z==C_HEAL) && !menu && touch_tutorial_gesture(z,now_ms())) return;
    if (z==C_KNIFE || z==C_WEAPON || z==C_WEAPON2 || z==C_GRENADE || z==C_HEAL) {
        int slot=z==C_KNIFE ? 3 : z==C_WEAPON ? 0 : z==C_WEAPON2 ? 1 : z==C_GRENADE ? 2 : 6;
        if(touch_quickselect_live(now_ms())) {
            if(touch_quickselect_request(slot,now_ms())) {
                touch_grenade_cancel();touch_grenade_retire_fingers();
                g_anim_kind=0;g_touch_aim=0;g_aim_latched=0;g_btn_hold_until[C_FIRE]=0;
                g_weapon_grip_at=0;g_grip_latched&=~(OB_RHAND|OB_LHAND);
                __atomic_store_n(&g_touch_weapon_request,0,__ATOMIC_RELEASE);
                __atomic_store_n(&g_mobile_holster_until,0,__ATOMIC_RELEASE);
                g_fire_slot=g_fire_side=-1;
                for(int i=0;i<MAXT;i++)if(g_vt[i].role==RT_BTN+C_FIRE || g_vt[i].role==RT_BTN+C_AIM)g_vt[i].role=RT_NONE;
            }
        } else if(z==C_KNIFE || z==C_WEAPON || z==C_HEAL)touch_weapon_select(z==C_KNIFE ? 1 : z==C_WEAPON ? 2 : 3);
    }
    else if (z == C_PAUSE) { touch_grenade_cancel();touch_grenade_retire_fingers();g_reload_phase = 0; __atomic_store_n(&g_touch_pause, 1, __ATOMIC_RELEASE); }
    else if (z == C_INVENTORY) {
        touch_grenade_cancel();touch_grenade_retire_fingers();
        g_btn_hold_until[C_FIRE]=0;g_fire_slot=g_fire_side=-1;g_pad_held_ctl&=~(1u<<C_FIRE);
        for(int i=0;i<MAXT;i++)if(g_vt[i].role==RT_BTN+C_FIRE)g_vt[i].role=RT_NONE;
        __atomic_store_n(&g_touch_inventory_request, 1, __ATOMIC_RELEASE);
    }
    else if (z == C_INTERACT) {touch_grenade_cancel();touch_grenade_retire_fingers();g_anim_kind=0;__atomic_store_n(&g_touch_interact_request, 1, __ATOMIC_RELEASE);}
    else if (z == C_RELOAD && !touch_motion_pose_busy()) { g_aim_latched = 0; __atomic_store_n(&g_touch_reload_request, 1, __ATOMIC_RELEASE); }
    else if (z == C_AIM && !menu && mobile_prefs_aim_toggle()) g_aim_latched = !g_aim_latched;
    else if (z == C_PHONE) __atomic_store_n(&g_phone_request, 1, __ATOMIC_RELEASE);
    else if (z == C_ASHLEY) { int ok = touch_ashley_request(now_ms()); LOGF("TOUCH Ashley command request=%d", ok); }
    else if (z == C_BACK && g_touch_context == 2) {
        menu_pointer_cancel_locked();
        __atomic_store_n(&g_touch_back_request, 1, __ATOMIC_RELEASE);
    }
    else if (z == C_FIRE && !menu && touch_throw_request(now_ms())) LOGF("TOUCH throw requested");
    else if (z == C_FIRE && !menu) {
        struct touch_quickselect_state state=touch_quickselect_state(now_ms());
        g_fire_slot=state.selected;g_fire_side=state.fire_side;
        if(g_touch_weapon==1 && (!touch_quickselect_live(now_ms()) || (!touch_quickselect_busy() && state.selected==3))) {
            touch_knife_swing(now_ms());
        }
    }
}

static void touch_reset(void) {
    touch_typewriter_cancel();
    touch_weapon_cycle_input_reset();
    touch_qte_reset();
    touch_quickselect_cancel();
    __atomic_store_n(&g_touch_cancel,3,__ATOMIC_RELEASE);
    unsigned grenade_preserve=touch_grenade_input_reset();
    touch_grenade_fire_up(now_ms());
    pthread_mutex_lock(&g_imx);
    memset(g_vt, 0, sizeof g_vt);
    g_fire_slot=g_fire_side=-1;
    memset((void*)g_btn_hold_until, 0, sizeof g_btn_hold_until);
    for(int bit=0;bit<32;bit++)if(!(grenade_preserve&(1u<<bit)))__atomic_store_n(&g_cmd_until[bit],0,__ATOMIC_RELEASE);
    g_grip_latched = 0; g_weapon_grip_at = 0; g_touch_weapon = 0;
    __atomic_store_n(&g_mobile_holster_until, 0, __ATOMIC_RELEASE);
    g_ptr_down = 0; g_ptr_until_ms = 0; g_tap_press_ms = g_tap_until_ms = 0;
    memset(&g_menu_pointer, 0, sizeof g_menu_pointer);
    g_cmd_lstick_until = g_cmd_rstick_until = 0; g_anim_kind = 0; g_knife_swing_until = 0;
    g_watch_held = 0; g_watch_since_ms = g_watch_until_ms = 0;
    g_reload_phase = 0;
    __atomic_store_n(&g_touch_reload_request, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_touch_weapon_request, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_touch_pause, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_touch_cancel, 3, __ATOMIC_RELEASE);
    __atomic_store_n(&g_phone_request, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_touch_inventory_request, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_touch_interact_request, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_touch_back_request, 0, __ATOMIC_RELEASE);
    g_touch_aim = 0; g_aim_latched = 0;
    recompute();
    pthread_mutex_unlock(&g_imx);
}

// Caller has proved owned active right GameHand/type1; never call this for phone/menu pawns.
static void touch_new_game_pawn(void* pawn) {
    if (!pawn) return;
    // UObjectBase::AddObject stores the complete 8-byte FName at +0x18.
    // Read only the supplied current object, never the cached previous address.
    unsigned long long name;
    memcpy(&name, (const char*)pawn+0x18, sizeof name);
    if (g_touch_last_game_pawn
        && (pawn != g_touch_last_game_pawn || name != g_touch_last_game_name)) {
        touch_reset();
        LOGF("TOUCH new game pawn=%p previous=%p; input reset",pawn,g_touch_last_game_pawn);
    }
    g_touch_last_game_pawn = pawn; g_touch_last_game_name = name;
}

// The native wrist display remains up only while a finger holds the health dial.
static int touch_watch_dial_hit(float x, float y) {
    struct mobile_hud_state hud;
    if (!touch_hud_state_read(now_ms(), &hud)) return 0;
    float dx = x - g_tw * .055f, dy = y - g_th * .25f, r = g_th * .060f * 1.3f;
    return dx * dx + dy * dy < r * r;
}
static void pointer_click(float x, float y, long dur){
    long nw = now_ms(), st = nw + (dur < 200 ? 120 : 40);
    g_ptr_u = x / g_tw; g_ptr_v = y / g_th;
    g_tap_press_ms = st; g_tap_until_ms = st + 150; g_ptr_until_ms = st + 650;
    __atomic_store_n(&g_menu_tap_ms, nw, __ATOMIC_RELEASE);
    LOGF("TAP -> trigger at %.0f,%.0f (held %ldms)", x, y, dur);
}
static void touch_drag_look(struct vt* t,float x,float y,int menu) {
    if(menu || g_touch_context!=1)return;
    float dx=x-t->lx,dy=y-t->ly;
    float limit=.5f*g_tw,length=hypotf(dx,dy);
    if(length>limit){dx*=limit/length;dy*=limit/length;}
    // Android can batch MOVE history; distance must not depend on its dispatch timing.
    float sensitivity=LOOK_SENS*touch_mobile_settings_snapshot().sensitivity;
    if (mobile_prefs_assist() && now_ms()-__atomic_load_n(&g_aim_painted_ms,__ATOMIC_ACQUIRE)<150)
        sensitivity*=g_touch_aim ? .55f : .75f;
    g_look_yaw=remainderf(g_look_yaw-dx*sensitivity,6.2831853f);
    g_look_pitch=fmaxf(-1.40f,fminf(1.40f,g_look_pitch-dy*sensitivity));
}

__attribute__((visibility("default")))
unsigned char Java_com_epicgames_ue4_MobileSettings_nativeGyroActive(void* env,void* type) {
    (void)env;(void)type;
    return mobile_prefs_gyro() && g_app_input_focus && !g_touch_cancel && g_touch_context==1 && g_touch_aim && !menu_mode();
}
__attribute__((visibility("default")))
void Java_com_epicgames_ue4_MobileSettings_nativeGyro(void* env,void* type,float yaw,float pitch) {
    if(!isfinite(yaw) || !isfinite(pitch) || fabsf(yaw)>.5f || fabsf(pitch)>.5f)return;
    pthread_mutex_lock(&g_imx);
    if(Java_com_epicgames_ue4_MobileSettings_nativeGyroActive(env,type)) {
        float gain=(float)mobile_prefs_gyro_sensitivity()/100.0f;
        g_look_yaw=remainderf(g_look_yaw-yaw*gain,6.2831853f);
        g_look_pitch=fmaxf(-1.40f,fminf(1.40f,g_look_pitch-pitch*gain));
    }
    pthread_mutex_unlock(&g_imx);
}

// TouchInput (UE4.25): DeviceId@0 Handle@4 Type@8 (0=Began 1=Moved 2=Ended) LastPosition@12 Position@20; stride 28
void touch_process(const void* ta){
    if (!ta) return;
    const unsigned char* data = *(const unsigned char* const*)ta;
    int num = *(const int*)((const char*)ta + 8);
    if (!data || num <= 0 || num > 64) return;
    if (g_comp_w && g_comp_h) { g_tw = (float)g_comp_w; g_th = (float)g_comp_h; }
    int menu = menu_mode();
    pthread_mutex_lock(&g_imx);
    sync_touch_context(menu);
    for (int i = 0; i < num; i++) {
        const unsigned char* e = data + i * 28;
        int h = *(const int*)(e + 4), type = *(const int*)(e + 8);
        float x = *(const float*)(e + 20), y = *(const float*)(e + 24);
        if (type < 0 || type > 2 || !isfinite(x) || !isfinite(y)) continue;
        if (touch_typewriter_event(h,type,x,y,g_tw,g_th,now_ms())) continue;
        g_tn++;
        if (g_tlog < 30) { g_tlog++; LOGF("touch h=%d t=%d %.0f,%.0f W=%.0f H=%.0f menu=%d", h, type, x, y, g_tw, g_th, menu); }
        struct vt* t = vt_find(h);
        __atomic_store_n(&g_last_touch_ms, now_ms(), __ATOMIC_RELEASE);
        if (type == 0) {
            // A finger on the game window proves input focus (failsafe if a focus-gained event was missed).
            if (!__atomic_load_n(&g_app_input_focus, __ATOMIC_ACQUIRE)) __atomic_store_n(&g_app_input_focus, 1, __ATOMIC_RELEASE);
            if(t && t->grenade)continue;
            if (!t) t = vt_new(h);
            if (!t) continue;
            if (t->role == RT_RSTICK || t->role == RT_MENU_POINTER) {
                if (g_ptr_down > 0) g_ptr_down--;
                if (t->role == RT_MENU_POINTER && g_menu_pointer.down && g_menu_pointer.h == h)
                    memset(&g_menu_pointer, 0, sizeof g_menu_pointer);
            }
            t->x0 = t->x = t->lx = x; t->y0 = t->y = t->ly = y; t->t0ms = now_ms();
            if(g_touch_context==3 && now_ms()-__atomic_load_n(&g_cutscene_skippable_ms,__ATOMIC_ACQUIRE)<400
                && hypotf(x-(g_tw-g_th*.09f),y-g_th*.88f)<=g_th*.065f) {
                t->role=RT_NONE;t->skip=1;
                __atomic_store_n(&g_cutscene_skip_request,now_ms(),__ATOMIC_RELEASE);
                continue;
            }
            int z = ctl_at(x, y, menu);
            if (z<0 && g_touch_context==5 && ctl_visible(C_PHONE,menu) && mobile_phone_hit(x/g_tw,y/g_th)) z=C_PHONE;
            if (z<0 && !menu && g_touch_context==1 && touch_prompt_hit(x,y)) z=C_INTERACT;   // tap the on-screen prompt
            if (z >= 0) {
                struct touch_quickselect_state selected=touch_quickselect_state(t->t0ms);
                if(z==C_FIRE && !menu && touch_quickselect_ready() && selected.selected==2) {
                    t->grenade=touch_grenade_fire_down(t->t0ms)?1:2;t->role=RT_NONE;
                    g_btn_hold_until[C_FIRE]=0;g_fire_slot=g_fire_side=-1;
                } else {t->role=RT_BTN+z;touch_button_press(z,menu);}
            }
            else if (menu && g_touch_context != 3) {
                if (g_menu_pointer.down) {
                    // A second finger on the map turns the pointer into a pinch (native map zoom).
                    struct vt* first = vt_find(g_menu_pointer.h);
                    if (first && first != t && !g_pinch.active && touch_map_active(t->t0ms)) {
                        menu_pointer_cancel_locked();
                        first->role = RT_PINCH; t->role = RT_PINCH;
                        float dx = x - first->x, dy = y - first->y;
                        g_pinch.active = 1; g_pinch.h1 = first->h; g_pinch.h2 = h; g_pinch.last_d = sqrtf(dx * dx + dy * dy);
                        LOGF("TOUCH map pinch start d=%.0f", g_pinch.last_d);
                        continue;
                    }
                    t->role = RT_NONE; continue;
                }
                t->role = RT_MENU_POINTER;
                g_menu_pointer.down = 1; g_menu_pointer.h = h;
                g_menu_pointer.pressed = 0; g_menu_pointer.press_ms = t->t0ms + 120;
                g_menu_pointer.tap=0; g_menu_pointer.release_ms=0;
                if (++g_menu_pointer_serial==0) ++g_menu_pointer_serial;
                g_menu_pointer.serial=g_menu_pointer_serial;
                g_menu_pointer.action=__atomic_load_n(&g_menu_grip_ready,__ATOMIC_ACQUIRE)
                    && menu_grip_context() ? MP_PENDING : MP_TRIGGER;
                g_tap_press_ms = g_tap_until_ms = 0;
                g_ptr_u = x / g_tw; g_ptr_v = y / g_th; g_ptr_down++;
            }
            else if (!menu && g_touch_context == 1 && touch_watch_dial_hit(x, y)) t->role = RT_WATCH;
            else if (!menu && g_touch_context == 1 && x < 0.40f * g_tw) {
                t->role = RT_LSTICK;
            }
            else { t->role = RT_RSTICK; g_ptr_u = x / g_tw; g_ptr_v = y / g_th; g_ptr_down++; }
        } else if (type == 1) {
            if (!t) continue;
            if (t->role == RT_WATCH && (x - t->x0) * (x - t->x0) + (y - t->y0) * (y - t->y0) > 30.0f * 30.0f) {
                if(x>=.46f*g_tw){touch_drag_look(t,x,y,menu);}
            }
            if (t->role == RT_RSTICK || (t->role == RT_BTN+C_FIRE && g_touch_weapon==1)) {
                touch_drag_look(t,x,y,menu);
                g_ptr_u = x / g_tw; g_ptr_v = y / g_th;
            } else if (t->role == RT_MENU_POINTER && g_menu_pointer.down && g_menu_pointer.h == h) {
                g_ptr_u = x / g_tw; g_ptr_v = y / g_th;
            }
            t->lx = x; t->ly = y; t->x = x; t->y = y;
            if (t->role == RT_PINCH && g_pinch.active) {
                struct vt* a = vt_find(g_pinch.h1), *b = vt_find(g_pinch.h2);
                if (a && b) {
                    float dx = a->x - b->x, dy = a->y - b->y, d = sqrtf(dx * dx + dy * dy);
                    if (g_pinch.last_d > 40.0f && d > 40.0f && fabsf(d / g_pinch.last_d - 1.0f) > 0.01f) {
                        touch_map_zoom_add(d / g_pinch.last_d); g_pinch.last_d = d;
                    }
                }
            }
        } else {
            if (t) {
                if(t->grenade==1)touch_grenade_fire_up(now_ms());t->grenade=0;
                float dx = x - t->x0, dy = y - t->y0; long dur = now_ms() - t->t0ms;
                int still = dx * dx + dy * dy < 30.0f * 30.0f;
                if (t->role == RT_PINCH) {
                    // Either finger ends the pinch; the other one never becomes a pointer or a click.
                    for (int k = 0; k < MAXT; k++) if (g_vt[k].on && g_vt[k].role == RT_PINCH) g_vt[k].role = RT_NONE;
                    g_pinch.active = 0;
                } else if (t->role == RT_MENU_POINTER) {
                    int pressed = g_menu_pointer.down && g_menu_pointer.h == h && g_menu_pointer.pressed;
                    int own=g_menu_pointer.down && g_menu_pointer.h==h;
                    g_ptr_u = x / g_tw; g_ptr_v = y / g_th;
                    if (g_ptr_down > 0) g_ptr_down--;
                    g_ptr_until_ms = now_ms() + 700;
                    void* direct = 0;
                    if (own && !pressed && menu && still && dur < 1500) {
                        // Click the nearest freshly published native projected action.
                        float limit = 0.075f * g_th, best = limit * limit;
                        pthread_mutex_lock(&g_ui_target_mx);
                        if (now_ms() - g_ui_targets_ms < 300)
                            for (int k = 0; k < g_ui_target_count; k++) {
                                float ex = g_ui_targets[k].x - x, ey = g_ui_targets[k].y - y;
                                if (ex * ex + ey * ey < best) { best = ex * ex + ey * ey; direct = g_ui_targets[k].component; }
                            }
                        pthread_mutex_unlock(&g_ui_target_mx);
                    }
                    if (direct) {
                        __atomic_store_n(&g_ui_direct_click, direct, __ATOMIC_RELEASE);
                        memset(&g_menu_pointer,0,sizeof g_menu_pointer);
                        LOGF("TAP -> in-world UI control at %.0f,%.0f", x, y);
                    } else if (own && !pressed && menu && still && dur < 1500) {
                        pointer_click(x,y,dur);
                        g_menu_pointer.down=0; g_menu_pointer.tap=1;
                        g_menu_pointer.press_ms=g_tap_press_ms;
                        g_menu_pointer.release_ms=g_tap_until_ms;
                    } else if (own) memset(&g_menu_pointer,0,sizeof g_menu_pointer);
                } else if (t->role == RT_RSTICK) {
                    if (g_ptr_down > 0) g_ptr_down--;
                    g_ptr_until_ms = now_ms() + 700;
                    if (!menu && g_touch_context == 1 && still && dur < 300) {
                        // A second short tap close to the first one on the look side turns the view around.
                        static long last_tap_ms; static float last_tap_x, last_tap_y;
                        long tap_ms = now_ms();
                        float ex = x - last_tap_x, ey = y - last_tap_y;
                        if (mobile_prefs_qturn() && last_tap_ms && tap_ms - last_tap_ms < 320 && ex * ex + ey * ey < 160.0f * 160.0f) {
                            __atomic_store_n(&g_qturn_request_ms, tap_ms, __ATOMIC_RELEASE);
                            last_tap_ms = 0;
                            LOGF("TOUCH quick 180 turn (double tap)");
                        } else {
                            last_tap_ms = tap_ms; last_tap_x = x; last_tap_y = y;
                            touch_world_tap(x, y);
                        }
                    }
                } else if (t->role == RT_LSTICK && !menu && g_touch_context == 1 && still && dur < 220) {
                    touch_world_tap(x, y);
                } else if (t->role >= RT_BTN) {
                    int z = t->role - RT_BTN;
                    if (ctl_bit(z) && ctl_visible(z, menu)) g_btn_hold_until[z] = t->t0ms + (z==C_FIRE ? 90 : 150);
                }
                t->on = 0;
            }
        }
    }
    recompute();
    pthread_mutex_unlock(&g_imx);
}

// static FAndroidInputInterface::QueueTouchInput(const TArray<TouchInput>&) @0x6a169ac
// prologue: str x21,[sp,#-0x30]!; stp x20,x19,[sp,#0x10]; stp x29,x30,[sp,#0x20]; add x29,sp,#0x20
unsigned long g_tgt_touch;
#ifndef RE4_INPUT_TEST
__attribute__((naked)) static void gate_touch(const void* arr){
    asm volatile(
        ".inst 0xf81d0ff5\n" ".inst 0xa9014ff4\n" ".inst 0xa9027bfd\n" ".inst 0x910083fd\n"
        "adrp x16, :got:g_tgt_touch\n" "ldr x16, [x16, :got_lo12:g_tgt_touch]\n" "ldr x16, [x16]\n" "br x16\n");
}
// QueueTouchInput is a STATIC member (UE4.25 FAndroidInputInterface): x0 = const TArray<TouchInput>&
// Original UE touch forwarding deadlocks Home/CANCEL against Oculus render flush.
void hook_touch(const void* arr){ g_tq_calls++; touch_process(arr); }
#endif

// ---- overlay quads (called from the compositor right before present) ----
static int ovl_add(struct ovl_quad* q, int n, int max, int cell, float cx, float cy, float rad, float a) {
    if (n >= max) return n;
    struct ovl_quad* o = &q[n];
    o->x0 = (cx - rad) / g_tw * 2.0f - 1.0f; o->x1 = (cx + rad) / g_tw * 2.0f - 1.0f;
    o->y0 = (cy - rad) / g_th * 2.0f - 1.0f; o->y1 = (cy + rad) / g_th * 2.0f - 1.0f;
    ovl_cell_uv(cell, &o->u0, &o->v0, &o->u1, &o->v1);
    o->r = o->g = o->b = 1.0f; o->a = a;
    return n + 1;
}
static float touch_health_colour(int current,int maximum) {
    // Match the original status graph's integer thirds, including non-divisible maximum HP.
    return current>2*(maximum/3) ? 2.0f : current>maximum/3 ? 1.0f : 0.0f;
}
int touch_overlay_quads(struct ovl_quad* q, int max) {
    int n = 0, menu = menu_mode();
    long nw = now_ms();
    struct mobile_hud_state hud;
    int health_valid=touch_hud_state_read(nw,&hud);
    pthread_mutex_lock(&g_imx);
    unsigned held = g_buttons | g_grip_latched;
    for (int z = 0; z < C_N; z++) if (nw < g_btn_hold_until[z]) held |= ctl_bit(z);
    int pad_hidden = touch_gamepad_hud_hidden(nw);   // a gamepad in use hides the touch icons
    for (int i = 0; i < C_N && !pad_hidden; i++) {
        struct ctl geometry=ctl_geometry(i);
        const struct ctl* c = &geometry;
        if (!ctl_visible(i, menu)) continue;
        float cx = c->cx * g_tw, cy = c->cy * g_th, rad = c->r * g_th;
        struct touch_quickselect_state selected=touch_quickselect_state(nw);
        int slot=i==C_KNIFE ? 3 : i==C_WEAPON ? 0 : i==C_WEAPON2 ? 1 : i==C_GRENADE ? 2 : i==C_HEAL ? 6 : -1;
        int on = (held & ctl_bit(i)) != 0 || (slot>=0 && selected.selected==slot)
            || (!touch_quickselect_live(nw) && ((i==C_KNIFE && g_touch_weapon==1)||(i==C_WEAPON && g_touch_weapon==2)||(i==C_HEAL && g_touch_weapon==3)))
            || (i == C_RELOAD && g_reload_phase) || (i == C_AIM && g_touch_aim);
        int cell=c->cell;
        if(i==C_CONFIRM && (touch_menu_toolbar_mask(nw)&4u))cell=OVL_SKIP;
        if(i==C_PHONE && touch_phone_skip_visible(nw))cell=OVL_SKIP;
        struct mobile_action_state action;
        if (i==C_INTERACT && touch_action_state(nw,&action)) {
            if(action.action==7 || action.action==57 || action.action==44)cell=OVL_KICK;
            else if(action.qte && (action.command==9 || action.command>=10))cell=OVL_ACTION_MOTION;
        } else if(i==C_INTERACT && health_valid && hud.command
            && (hud.action==7 || hud.action==57 || hud.action==44))cell=OVL_KICK;
        int sce_kind_now=0,sce_running=0;float sce_progress=0;
        if (i==C_INTERACT && cell==c->cell && touch_sce_state(nw,&sce_kind_now,&sce_running,&sce_progress))
            cell=sce_kind_now==1 ? OVL_LEVER : OVL_CRANK;   // a reachable VR lever/crank: the icon operates it
        if (i==C_FIRE && selected.selected==6)cell=OVL_HEAL;
        if (i==C_FIRE && selected.selected==2)cell=OVL_GRENADE;
        if (i==C_ASHLEY && touch_ashley_state(nw).next==1)cell=OVL_ASHLEY_WAIT;   // the order a tap will give
        if (i==C_FIRE && touch_throw_mode(nw))cell=OVL_THROW;
        float alpha=on ? 1.0f : 0.80f;
        if (i==C_RELOAD && !on && touch_hud_reload_needed(nw))
            alpha=0.575f+0.225f*cosf((float)(nw%2400)*0.002617993878f);
        n = ovl_add(q, n, max, cell, cx, cy, rad, alpha);
        if (on) n = ovl_add(q, n, max, OVL_RING, cx, cy, rad, 1.0f);
        if (sce_running) {   // crank progress around the icon
            int before = n;
            n = ovl_add(q, n, max, OVL_RING, cx, cy, rad * 1.12f, 1.0f);
            if (n > before) { q[before].r = sce_progress; q[before].a = -2.0f; }
        }
    }
    if (!menu && g_touch_context == 1 && g_touch_weapon == 2)
        n = ovl_add(q, n, max, OVL_DOT, g_tw * 0.5f, g_th * 0.5f, g_th * 0.008f, 0.85f);
    float heal_progress;
    if(touch_heal_progress_read(nw,&heal_progress)) {
        n=ovl_add(q,n,max,OVL_HEAL,g_tw*.5f,g_th*.5f,g_th*.025f,1.0f);
        int before=n;
        n=ovl_add(q,n,max,OVL_RING,g_tw*.5f,g_th*.5f,g_th*.052f,1.0f);
        if(n>before){q[before].r=heal_progress;q[before].a=-2.0f;}
    }
    if (!menu && g_touch_context==1 && health_valid) {
        int before=n;
        n=ovl_add(q,n,max,OVL_HEALTH,g_tw*.055f,g_th*.25f,g_th*.060f,1.0f);
        if (n>before) {
            q[before].r=(float)hud.current_health/(float)hud.max_health;
            q[before].g=touch_health_colour(hud.current_health,hud.max_health); q[before].a=-1.0f;
            touch_health_flat_presented(nw);
        }
    }
    int ashley_hp,ashley_max;
    if (!menu && g_touch_context==1 && touch_ashley_health(nw,&ashley_hp,&ashley_max)) {   // her own dial, beside Leon's
        int before=n;float ax=g_tw*.055f+g_th*.125f,ay=g_th*.25f;
        n=ovl_add(q,n,max,OVL_HEALTH,ax,ay,g_th*.052f,1.0f);
        if (n>before) {
            q[before].r=(float)ashley_hp/(float)ashley_max;
            q[before].g=touch_health_colour(ashley_hp,ashley_max); q[before].a=-1.0f;
        }
        n=ovl_add(q,n,max,OVL_ASHLEY_FOLLOW,ax+g_th*.045f,ay-g_th*.045f,g_th*.022f,0.9f);
    }
    if (g_touch_context==3 && nw-__atomic_load_n(&g_cutscene_skippable_ms,__ATOMIC_ACQUIRE)<400) {
        // Hold anywhere to skip (the original "Hold To Skip"): a ring fills under the still finger.
        n=ovl_add(q,n,max,OVL_SKIP,g_tw-g_th*.09f,g_th*.88f,g_th*.045f,.55f);
        for (int i=0;i<MAXT;i++) {
            struct vt* s=&g_vt[i];
            if (!s->on || s->role!=RT_RSTICK || s->skip) continue;
            float dx=s->x-s->x0,dy=s->y-s->y0;
            if (dx*dx+dy*dy>30.0f*30.0f) { s->skip=2; continue; }
            long held=nw-s->t0ms-SKIP_HOLD_GRACE_MS;
            if (held<0) continue;
            float progress=(float)held/(float)SKIP_HOLD_FILL_MS;
            int before=n;
            n=ovl_add(q,n,max,OVL_RING,s->x,s->y,g_th*.075f,1.0f);
            if (n>before) { q[before].r=progress>1.0f ? 1.0f : progress; q[before].a=-2.0f; }
            if (progress>=1.0f) {
                s->skip=1;__atomic_store_n(&g_cutscene_skip_request,nw,__ATOMIC_RELEASE);
                LOGF("TOUCH hold-to-skip completed");
            }
        }
    }
    if (menu && touch_gamepad_recent(nw) && nw < g_ptr_until_ms)   // gamepad pointer in menus
        n = ovl_add(q, n, max, OVL_DOT, g_ptr_u * g_tw, g_ptr_v * g_th, g_th * 0.018f, 0.95f);
    if (!pad_hidden && !menu && g_touch_context==1 && ctl_visible(C_RELOAD,0) && touch_tutorial_reload_hint(nw)) {
        struct ctl c=ctl_geometry(C_RELOAD);
        float x=c.cx*g_tw,y=c.cy*g_th-(c.r+.008f)*g_th;
        for (int i=0;i<4;i++) {   // short downward arrow ends just above the actual Reload hitbox
            float half=i ? (.024f-.006f*(float)i)*g_th : .003f*g_th;
            float top=y-(i ? .016f-.004f*(float)i : .018f)*g_th;
            float bottom=i ? top+.004f*g_th : y-.012f*g_th;
            int before=n;
            n=ovl_add(q,n,max,OVL_SOLID,x,(top+bottom)*.5f,half,.95f);
            if (n>before) { q[before].y0=top/g_th*2-1; q[before].y1=bottom/g_th*2-1; }
        }
    }
    pthread_mutex_unlock(&g_imx);
    return n;
}

// ovrpControllerState4 (96B): ConnectedControllerTypes@0 Buttons@4 Touches@8 NearTouches@12
// IndexTrigger[2]@16 HandTrigger[2]@24 Thumbstick[2]@32 Touchpad[2]@48 Battery[2]@64 Recenter[2]@66 Reserved@68.
// SendControllerEvents passes a 120B stack buffer, so a full 96B fill is safe.
void fill_controller_state(unsigned char* p, unsigned mask){
    memset(p, 0, 96);
    unsigned conn = mask & 0x3u;             // only LTouch(1)|RTouch(2) ever connected
    unsigned* u = (unsigned*)p; float* f = (float*)p;
    u[0] = conn;
    if (!conn) return;
    unsigned lm = OB_X | OB_Y | OB_LTHUMB | OB_START | OB_LINDEX | OB_LHAND;
    unsigned rm = OB_A | OB_B | OB_RTHUMB | OB_RINDEX | OB_RHAND;
    unsigned keep = 0; if (conn & 1) keep |= lm; if (conn & 2) keep |= rm;
    touch_gamepad_frame();   // takes g_imx itself; once per frame
    pthread_mutex_lock(&g_imx);
    touch_refresh_context_gt();
    // A tap during the first frame after unpause may precede the native Quick Select publisher.
    int pending_weapon=__atomic_load_n(&g_touch_weapon_request,__ATOMIC_ACQUIRE);
    if(g_touch_context==1 && !g_touch_cancel && g_app_input_focus && !menu_mode()
        && pending_weapon>=1 && pending_weapon<=3 && touch_quickselect_live(now_ms()))
        touch_button_press(pending_weapon==1 ? C_KNIFE : pending_weapon==2 ? C_WEAPON : C_HEAL,0);
    if(touch_quickselect_ready() && g_touch_context==1) {
        struct touch_quickselect_state selected=touch_quickselect_state(now_ms());
        if(selected.selected>=0)g_touch_weapon=selected.selected==3 ? 1 : selected.selected==0 || selected.selected==1 || selected.selected==QS_HELD_WORLD_GUN ? 2 : 3;
    }
    {   // first-person draw animation when the knife becomes the held weapon
        static int last_weapon;
        if (g_touch_weapon != last_weapon) { if (g_touch_weapon == 1) knife_pose_drawn(now_ms()); last_weapon = g_touch_weapon; }
    }
    recompute();
    if (!touch_quickselect_live(now_ms()) && (conn & 2) && !__atomic_load_n(&g_touch_weapon_request, __ATOMIC_ACQUIRE)) {
        if (g_weapon_grip_at < 0) g_weapon_grip_at = now_ms() + 250;
        else if (g_weapon_grip_at && now_ms() >= g_weapon_grip_at) { g_grip_latched |= OB_RHAND; g_weapon_grip_at = 0; }
    }
    unsigned btn = g_buttons | g_grip_latched;
    {
        long nm = now_ms();
        if (menu_mode() && g_touch_context != 3
            && (!g_menu_pointer.tap || g_menu_pointer.action==MP_TRIGGER)
            && nm >= g_tap_press_ms && nm < g_tap_until_ms) btn |= OB_RINDEX;
        if ((conn & 2) && menu_mode() && g_touch_context != 3
            && g_menu_pointer.down && g_menu_pointer.action==MP_TRIGGER && nm >= g_menu_pointer.press_ms) {
            btn |= OB_RINDEX; g_menu_pointer.pressed = 1;
        }
        for (int z = 0; z < C_N; z++) if (ctl_visible(z, menu_mode()) && nm < g_btn_hold_until[z]) btn |= ctl_bit(z);
        for (int k = 0; k < 32; k++) if (nm < g_cmd_until[k]) btn |= (1u << k);
        btn|=touch_weapon_cycle_input_mask(nm,menu_mode());
    }
    (void)g_autopress;
    unsigned menu_excluded=menu_mode() && g_touch_context!=3 ? touch_grenade_owned_grip_mask()|touch_throw_owned_grip_mask() : 0;
    btn&=~menu_excluded;
    u[1] = btn & keep;
    if (conn & 1) {
        f[4] = (btn & OB_LINDEX) ? 1.0f : g_trig_l; f[6] = (menu_excluded & OB_LHAND) ? 0 : (btn & OB_LHAND) ? 1.0f : g_grip_l;
        f[8] = g_lstick_x; f[9] = g_lstick_y; p[64] = 100;
        if (now_ms() < g_cmd_lstick_until) { f[8] = g_cmd_lstick[0]; f[9] = g_cmd_lstick[1]; }
        g_last_lx = f[8]; g_last_ly = f[9];
    }
    if (conn & 2) {
        f[5] = (btn & OB_RINDEX) ? 1.0f : g_trig_r; f[7] = (menu_excluded & OB_RHAND) ? 0 : (btn & OB_RHAND) ? 1.0f : g_grip_r;
        f[10] = g_rstick_x; f[11] = g_rstick_y; p[65] = 100;
        if (now_ms() < g_cmd_rstick_until) { f[10] = g_cmd_rstick[0]; f[11] = g_cmd_rstick[1]; }
        g_last_rt = f[5]; g_last_rg = f[7];
    }
    g_last_mask = mask; g_last_bits = u[1];
    pthread_mutex_unlock(&g_imx);
}

static int touch_mobile_back(void) {
    pthread_mutex_lock(&g_imx);
    int context=g_touch_context,handled=context>=1 && context<=7;
    sync_touch_context(menu_mode());
    if(context==1 || context==3)touch_button_press(C_PAUSE,0);
    else if(context==2)touch_button_press(C_BACK,1);
    else if(context==4 || context==7){
        menu_pointer_cancel_locked();g_btn_hold_until[C_BACK]=now_ms()+150;
    }
    recompute();pthread_mutex_unlock(&g_imx);
    return handled;
}
static void touch_log_state(void) {
    pthread_mutex_lock(&g_imx);
    int bound_slot=g_fire_slot,bound_side=g_fire_side;
    LOGF("INPUTSTATE mask=%08x bits=%08x stick=%.2f,%.2f RT=%.1f RG=%.1f latch=%08x weapon=%d pending=%d grip_at=%ld cmd_stick_until=%ld now=%ld menu=%d context=%d aim=%d",
         g_last_mask, g_last_bits, g_last_lx, g_last_ly, g_last_rt, g_last_rg, g_grip_latched,
         g_touch_weapon, g_touch_weapon_request, g_weapon_grip_at, g_cmd_lstick_until, now_ms(), menu_mode(), g_touch_context, g_touch_aim);
    pthread_mutex_unlock(&g_imx);
    struct touch_quickselect_state selected=touch_quickselect_state(now_ms());
    LOGF("INPUTSELECT selected=%d side=%d mode=%d available=%03x bound=%d,%d busy=%d motion=%d,%d",
         selected.selected,selected.fire_side,selected.mode,selected.available,bound_slot,bound_side,
         touch_quickselect_busy(),touch_qte_pose_busy(now_ms()),touch_grenade_busy(now_ms()));
    struct mobile_hud_state hud;
    if (touch_hud_state_read(now_ms(),&hud))
        LOGF("HUDSTATE HP=%d/%d action=%u command=%u",hud.current_health,hud.max_health,hud.action,hud.command);
}
