// Physical gamepad -> the same phone actions as the touch HUD. The engine already receives Android gamepad events
// (its native input callback calls FAndroidInputInterface::JoystickAxisEvent/JoystickButtonEvent with the Android axis
// ids and key codes); hooking those two static functions gives the pad state without Java changes. The game itself
// only maps a few gamepad keys, so the pad drives the existing touch roles: sticks, Fire/Aim, icons, menu pointer.
// Include after re4fix_input.h.
#ifndef RE4FIX_GAMEPAD_H
#define RE4FIX_GAMEPAD_H

enum { PAD_A, PAD_B, PAD_X, PAD_Y, PAD_LB, PAD_RB, PAD_L2, PAD_R2, PAD_L3, PAD_R3, PAD_START, PAD_SELECT,
       PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT, PAD_COUNT };
static struct {
    float lx, ly, rx, ry, lt_axis, rt_axis, brake, gas, hatx, haty;
    unsigned buttons, pressed, released;   // PAD_* bits; pressed/released are edges consumed by the frame step
    long last_ms;
} g_pad;
static pthread_mutex_t g_pad_mx = PTHREAD_MUTEX_INITIALIZER;
static long g_pad_frame_ms, g_pad_hold_a_ms;
static int g_pad_skip_done, g_pad_logs;
unsigned long g_tgt_pad_axis, g_tgt_pad_button;

__attribute__((naked)) static void gate_pad_axis(int device, int axis, float value) {
    asm volatile(".inst 0xfc1d0fe8\n" ".inst 0xa9014ff4\n" ".inst 0xa9027bfd\n" ".inst 0x910083fd\n"
        "adrp x16, :got:g_tgt_pad_axis\n" "ldr x16, [x16, :got_lo12:g_tgt_pad_axis]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static void gate_pad_button(int device, int button, int down) {
    asm volatile(".inst 0xf81d0ff5\n" ".inst 0xa9014ff4\n" ".inst 0xa9027bfd\n" ".inst 0x910083fd\n"
        "adrp x16, :got:g_tgt_pad_button\n" "ldr x16, [x16, :got_lo12:g_tgt_pad_button]\n" "ldr x16, [x16]\n" "br x16\n");
}
static int pad_bit_of(int keycode) {
    switch (keycode) {   // Android KeyEvent codes
        case 96: return PAD_A; case 97: return PAD_B; case 99: return PAD_X; case 100: return PAD_Y;
        case 102: return PAD_LB; case 103: return PAD_RB; case 104: return PAD_L2; case 105: return PAD_R2;
        case 106: return PAD_L3; case 107: return PAD_R3; case 108: return PAD_START; case 109: return PAD_SELECT;
        case 19: return PAD_UP; case 20: return PAD_DOWN; case 21: return PAD_LEFT; case 22: return PAD_RIGHT;
        default: return -1;
    }
}
// Mapped axes/buttons are consumed here: the game's own few gamepad bindings (Button A/B/Y, triggers, d-pad as
// the left VR stick) would otherwise act a second time. Anything unmapped still reaches the engine.
void hook_pad_axis(int device, int axis, float value) {
    int mapped = 1;
    pthread_mutex_lock(&g_pad_mx);
    switch (axis) {      // Android MotionEvent axes forwarded by the engine
        case 0: g_pad.lx = value; break;        case 1: g_pad.ly = value; break;
        case 11: g_pad.rx = value; break;       case 14: g_pad.ry = value; break;   // Z / RZ: right stick
        case 17: g_pad.lt_axis = value; break;  case 18: g_pad.rt_axis = value; break;
        case 23: g_pad.brake = value; break;    case 22: g_pad.gas = value; break;
        case 15: g_pad.hatx = value; break;     case 16: g_pad.haty = value; break;
        default: mapped = 0; break;
    }
    if (mapped && !isfinite(value)) {   // never keep NaN/inf in the pad state
        if (axis == 0) g_pad.lx = 0; else if (axis == 1) g_pad.ly = 0; else if (axis == 11) g_pad.rx = 0; else if (axis == 14) g_pad.ry = 0;
        else if (axis == 17) g_pad.lt_axis = 0; else if (axis == 18) g_pad.rt_axis = 0; else if (axis == 23) g_pad.brake = 0;
        else if (axis == 22) g_pad.gas = 0; else if (axis == 15) g_pad.hatx = 0; else g_pad.haty = 0;
    }
    if (mapped && isfinite(value) && fabsf(value) > 0.3f) g_pad.last_ms = ptr_now_ms();
    pthread_mutex_unlock(&g_pad_mx);
    if (!mapped) gate_pad_axis(device, axis, value);
}
void hook_pad_button(int device, int button, int down) {
    int bit = pad_bit_of(button);
    if (bit < 0) { gate_pad_button(device, button, down); return; }
    pthread_mutex_lock(&g_pad_mx);
    unsigned mask = 1u << bit;
    if (down && !(g_pad.buttons & mask)) { g_pad.buttons |= mask; g_pad.pressed |= mask; }
    else if (!down && (g_pad.buttons & mask)) { g_pad.buttons &= ~mask; g_pad.released |= mask; }
    g_pad.last_ms = ptr_now_ms();
    pthread_mutex_unlock(&g_pad_mx);
    if (g_pad_logs < 24) { g_pad_logs++; LOGF("PAD button key=%d down=%d", button, down); }
}
static int touch_gamepad_recent(long now) { return g_pad.last_ms && now - g_pad.last_ms < 8000; }
// The touch HUD steps aside while a pad is in use (a touch brings it back at once).
static int touch_gamepad_hud_hidden(long now) {
    return touch_gamepad_recent(now) && now - __atomic_load_n(&g_last_touch_ms, __ATOMIC_ACQUIRE) > 3000;
}
static float pad_dead(float v) {
    const float dead = 0.18f;
    if (fabsf(v) < dead) return 0;
    float s = (fabsf(v) - dead) / (1.0f - dead);
    if (s > 1) s = 1;
    return v < 0 ? -s : s;
}
// Once per frame on the GameThread (fill_controller_state), BEFORE it takes g_imx.
static void touch_gamepad_frame(void) {
    long now = ptr_now_ms();
    if (now == g_pad_frame_ms) return;
    long dt = g_pad_frame_ms ? now - g_pad_frame_ms : 16;
    g_pad_frame_ms = now;
    if (dt < 0 || dt > 100) dt = 16;
    pthread_mutex_lock(&g_pad_mx);
    float lx = pad_dead(g_pad.lx), ly = pad_dead(g_pad.ly), rx = pad_dead(g_pad.rx), ry = pad_dead(g_pad.ry);
    float lt = fmaxf(g_pad.lt_axis, g_pad.brake), rt = fmaxf(g_pad.rt_axis, g_pad.gas);
    unsigned buttons = g_pad.buttons, pressed = g_pad.pressed, released = g_pad.released;
    if (g_pad.hatx < -0.5f) buttons |= 1u << PAD_LEFT; if (g_pad.hatx > 0.5f) buttons |= 1u << PAD_RIGHT;
    if (g_pad.haty < -0.5f) buttons |= 1u << PAD_UP;   if (g_pad.haty > 0.5f) buttons |= 1u << PAD_DOWN;
    if (lt > 0.5f || (buttons & (1u << PAD_L2))) buttons |= 1u << PAD_L2;
    if (rt > 0.5f || (buttons & (1u << PAD_R2))) buttons |= 1u << PAD_R2;
    g_pad.pressed = g_pad.released = 0;
    int active = touch_gamepad_recent(now);
    pthread_mutex_unlock(&g_pad_mx);
    static unsigned prev;
    unsigned edge_down = (buttons & ~prev) | pressed, edge_up = (~buttons & prev) | released;
    prev = buttons;
    if (!active && !edge_down) { pthread_mutex_lock(&g_imx); g_pad_held_ctl = 0; g_pad_move[0] = g_pad_move[1] = 0; pthread_mutex_unlock(&g_imx); return; }
    int back = 0;
    pthread_mutex_lock(&g_imx);
    int menu = menu_mode(), context = g_touch_context;
    sync_touch_context(menu);
    unsigned held = 0;
    if (menu && context != 3) {
        // Menus: the left stick / d-pad moves the native pointer, A clicks, B goes back.
        float mx = lx, my = ly;
        if (buttons & (1u << PAD_LEFT)) mx = -1; if (buttons & (1u << PAD_RIGHT)) mx = 1;
        if (buttons & (1u << PAD_UP)) my = -1;   if (buttons & (1u << PAD_DOWN)) my = 1;
        if (mx || my) {
            float u = g_ptr_u + mx * 0.55f * (float)dt * 0.001f, v = g_ptr_v + my * 0.55f * (float)dt * 0.001f * g_tw / g_th;
            g_ptr_u = u < 0.02f ? 0.02f : u > 0.98f ? 0.98f : u;
            g_ptr_v = v < 0.02f ? 0.02f : v > 0.98f ? 0.98f : v;
            g_ptr_until_ms = now + 800;
        }
        if (edge_down & (1u << PAD_A)) pointer_click(g_ptr_u * g_tw, g_ptr_v * g_th, 100);
        if (edge_down & (1u << PAD_B)) {
            back = 1;
            // First-time hint popups refuse Back; B counts as the "tap anywhere" that closes them via OK.
            __atomic_store_n(&g_menu_tap_ms, now, __ATOMIC_RELEASE);
        }
        g_pad_move[0] = g_pad_move[1] = 0;
    } else if (context == 5) {
        if (edge_down & ((1u << PAD_A) | (1u << PAD_R2))) touch_button_press(C_PHONE, menu);
        if (edge_down & (1u << PAD_B)) back = 1;
    } else if (context == 3) {
        // Cutscenes: A answers QTE prompts; holding A skips like the touch hold.
        if (edge_down & (1u << PAD_A)) {
            if (ctl_visible(C_INTERACT, menu)) touch_button_press(C_INTERACT, menu);
            g_pad_hold_a_ms = now; g_pad_skip_done = 0;
        }
        if ((buttons & (1u << PAD_A)) && !g_pad_skip_done && g_pad_hold_a_ms && now - g_pad_hold_a_ms > 900
            && now - __atomic_load_n(&g_cutscene_skippable_ms, __ATOMIC_ACQUIRE) < 400) {
            g_pad_skip_done = 1; __atomic_store_n(&g_cutscene_skip_request, now, __ATOMIC_RELEASE);
        }
        if (edge_down & (1u << PAD_START)) touch_button_press(C_PAUSE, menu);
    } else if (context == 1) {
        g_pad_move[0] = lx; g_pad_move[1] = -ly;            // Android stick: up is -1
        // Right stick: look rate with a squared response curve for fine aiming.
        if (rx || ry) {
            float sens = touch_mobile_settings_snapshot().sensitivity;
            float rate_yaw = 2.6f * sens, rate_pitch = 1.9f * sens;
            if (g_touch_aim) { rate_yaw *= 0.6f; rate_pitch *= 0.6f; }
            if (mobile_prefs_assist() && now - __atomic_load_n(&g_aim_painted_ms, __ATOMIC_ACQUIRE) < 150) { rate_yaw *= 0.6f; rate_pitch *= 0.6f; }
            float yaw = g_look_yaw - rx * fabsf(rx) * rate_yaw * (float)dt * 0.001f;
            float pitch = g_look_pitch - ry * fabsf(ry) * rate_pitch * (float)dt * 0.001f;
            if (yaw > 3.14159265f) yaw -= 6.2831853f; if (yaw < -3.14159265f) yaw += 6.2831853f;
            if (pitch > 1.40f) pitch = 1.40f; if (pitch < -1.40f) pitch = -1.40f;
            g_look_yaw = yaw; g_look_pitch = pitch;
        }
        static const struct { int pad, ctl; } press_map[] = {
            {PAD_A, C_INTERACT}, {PAD_B, C_KNIFE}, {PAD_X, C_RELOAD}, {PAD_Y, C_WEAPON}, {PAD_LB, C_WEAPON2},
            {PAD_RB, C_GRENADE}, {PAD_DOWN, C_HEAL}, {PAD_UP, C_ASHLEY}, {PAD_START, C_PAUSE}, {PAD_SELECT, C_INVENTORY},
        };
        for (unsigned i = 0; i < sizeof press_map / sizeof *press_map; i++)
            if ((edge_down & (1u << press_map[i].pad)) && ctl_visible(press_map[i].ctl, menu))
                touch_button_press(press_map[i].ctl, menu);
        if (edge_down & (1u << PAD_R3)) __atomic_store_n(&g_qturn_request_ms, now, __ATOMIC_RELEASE);
        // Fire (RT): the same paths as a finger on the Fire icon, including grenades and throws.
        struct touch_quickselect_state selected = touch_quickselect_state(now);
        if (edge_down & (1u << PAD_R2)) {
            if (touch_quickselect_ready() && selected.selected == 2) (void)touch_grenade_fire_down(now);
            else touch_button_press(C_FIRE, menu);
        }
        if (edge_up & (1u << PAD_R2)) touch_grenade_fire_up(now);
        if ((buttons & (1u << PAD_R2)) && !(touch_quickselect_ready() && selected.selected == 2)) held |= 1u << C_FIRE;
        // Aim (LT): held, or toggled when the native option says "Tap".
        if (mobile_prefs_aim_toggle()) { if (edge_down & (1u << PAD_L2)) touch_button_press(C_AIM, menu); }
        else if (buttons & (1u << PAD_L2)) held |= 1u << C_AIM;
    }
    if (!(context == 1 && !menu)) g_pad_move[0] = g_pad_move[1] = 0;
    g_pad_held_ctl = held;
    recompute();
    pthread_mutex_unlock(&g_imx);
    if (back) (void)touch_mobile_back();
}
static void touch_gamepad_log(void) {
    pthread_mutex_lock(&g_pad_mx);
    LOGF("PAD state L=%.2f,%.2f R=%.2f,%.2f LT=%.2f/%.2f RT=%.2f/%.2f hat=%.0f,%.0f buttons=%04x last=%ldms ago",
         g_pad.lx, g_pad.ly, g_pad.rx, g_pad.ry, g_pad.lt_axis, g_pad.brake, g_pad.rt_axis, g_pad.gas, g_pad.hatx, g_pad.haty,
         g_pad.buttons, g_pad.last_ms ? ptr_now_ms() - g_pad.last_ms : -1L);
    pthread_mutex_unlock(&g_pad_mx);
}
static void install_gamepad(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[4]; } check[] = {
        {0x6a16c04, {0xfc1d0fe8, 0xa9014ff4, 0xa9027bfd, 0x910083fd}},   // FAndroidInputInterface::JoystickAxisEvent
        {0x6a16f58, {0xf81d0ff5, 0xa9014ff4, 0xa9027bfd, 0x910083fd}},   // FAndroidInputInterface::JoystickButtonEvent
    };
    for (unsigned i = 0; i < sizeof check / sizeof *check; i++)
        if (memcmp((const void*)(base + check[i].addr), check[i].first, 16)) {
            LOGF("GAMEPAD disabled: opcode mismatch +%lx", check[i].addr); return;
        }
    g_tgt_pad_axis = base + 0x6a16c04 + 16;
    g_tgt_pad_button = base + 0x6a16f58 + 16;
    if (patch_code((void*)(base + 0x6a16c04), (void*)&hook_pad_axis) || patch_code((void*)(base + 0x6a16f58), (void*)&hook_pad_button)) {
        LOGF("GAMEPAD disabled: hook installation failed"); return;
    }
    LOGF("GAMEPAD ready: Android gamepad axes/buttons drive the phone controls");
}
#endif
