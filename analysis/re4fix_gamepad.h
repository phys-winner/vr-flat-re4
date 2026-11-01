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
} g_pad;
static pthread_mutex_t g_pad_mx = PTHREAD_MUTEX_INITIALIZER;
static volatile long g_pad_last_ms;
static int g_pad_device = -1;
static volatile unsigned g_pad_epoch;
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
static unsigned pad_buttons_locked(void) {
    unsigned buttons = g_pad.buttons;
    if (g_pad.hatx < -0.5f) buttons |= 1u << PAD_LEFT; if (g_pad.hatx > 0.5f) buttons |= 1u << PAD_RIGHT;
    if (g_pad.haty < -0.5f) buttons |= 1u << PAD_UP; if (g_pad.haty > 0.5f) buttons |= 1u << PAD_DOWN;
    if (fmaxf(g_pad.lt_axis, g_pad.brake) > 0.5f) buttons |= 1u << PAD_L2;
    if (fmaxf(g_pad.rt_axis, g_pad.gas) > 0.5f) buttons |= 1u << PAD_R2;
    return buttons;
}
static void pad_clear_locked(void) {
    memset(&g_pad, 0, sizeof g_pad);
    g_pad_device = -1;
    __atomic_store_n(&g_pad_last_ms, 0, __ATOMIC_RELEASE);
    __atomic_add_fetch(&g_pad_epoch, 1, __ATOMIC_ACQ_REL);
}
static void touch_gamepad_reset_device(int device) {
    pthread_mutex_lock(&g_pad_mx);
    int reset = device < 0 || device == g_pad_device;
    if (reset) pad_clear_locked();
    pthread_mutex_unlock(&g_pad_mx);
    if (!reset) return;
    pthread_mutex_lock(&g_imx);
    g_pad_held_ctl = 0; g_pad_move[0] = g_pad_move[1] = 0;
    g_aim_latched = 0; g_touch_aim = 0; g_btn_hold_until[C_FIRE] = 0;
    touch_grenade_fire_up(ptr_now_ms());
    recompute();
    pthread_mutex_unlock(&g_imx);
}
static void touch_gamepad_reset(void) { touch_gamepad_reset_device(-1); }
__attribute__((visibility("default")))
void Java_com_epicgames_ue4_MobileGamepad_nativeReset(void* env, void* type, int device) {
    (void)env; (void)type;
    touch_gamepad_reset_device(device);
}
// Mapped axes/buttons are consumed here: the game's own few gamepad bindings (Button A/B/Y, triggers, d-pad as
// the left VR stick) would otherwise act a second time. Anything unmapped still reaches the engine.
void hook_pad_axis(int device, int axis, float value) {
    float* target = 0;
    pthread_mutex_lock(&g_pad_mx);
    switch (axis) {      // Android MotionEvent axes forwarded by the engine
        case 0: target = &g_pad.lx; break;        case 1: target = &g_pad.ly; break;
        case 11: target = &g_pad.rx; break;       case 14: target = &g_pad.ry; break;   // Z / RZ: right stick
        case 17: target = &g_pad.lt_axis; break;  case 18: target = &g_pad.rt_axis; break;
        case 23: target = &g_pad.brake; break;    case 22: target = &g_pad.gas; break;
        case 15: target = &g_pad.hatx; break;     case 16: target = &g_pad.haty; break;
    }
    if (target && __atomic_load_n(&g_app_input_focus, __ATOMIC_ACQUIRE)) {
        if (!isfinite(value)) value = 0;
        value = fmaxf(-1, fminf(1, value));
        if (g_pad_device != device && fabsf(value) > 0.18f) { pad_clear_locked(); g_pad_device = device; }
        if (g_pad_device == device) {
            unsigned before = pad_buttons_locked();
            *target = value;
            unsigned after = pad_buttons_locked();
            g_pad.pressed |= after & ~before; g_pad.released |= before & ~after;
            __atomic_store_n(&g_pad_last_ms, ptr_now_ms(), __ATOMIC_RELEASE);
        }
    }
    pthread_mutex_unlock(&g_pad_mx);
    if (!target) gate_pad_axis(device, axis, value);
}
void hook_pad_button(int device, int button, int down) {
    int bit = pad_bit_of(button);
    if (bit < 0) { gate_pad_button(device, button, down); return; }
    pthread_mutex_lock(&g_pad_mx);
    if (__atomic_load_n(&g_app_input_focus, __ATOMIC_ACQUIRE)) {
        if (g_pad_device != device && down) { pad_clear_locked(); g_pad_device = device; }
        if (g_pad_device == device) {
            unsigned before = pad_buttons_locked(), mask = 1u << bit;
            if (down) g_pad.buttons |= mask; else g_pad.buttons &= ~mask;
            unsigned after = pad_buttons_locked();
            g_pad.pressed |= after & ~before; g_pad.released |= before & ~after;
            __atomic_store_n(&g_pad_last_ms, ptr_now_ms(), __ATOMIC_RELEASE);
        }
    }
    pthread_mutex_unlock(&g_pad_mx);
    if (g_pad_logs < 24) { g_pad_logs++; LOGF("PAD button key=%d down=%d", button, down); }
}
static int touch_gamepad_recent(long now) {
    long last = __atomic_load_n(&g_pad_last_ms, __ATOMIC_ACQUIRE);
    return last && now >= last && now - last < 8000;
}
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
    unsigned buttons = pad_buttons_locked(), pressed = g_pad.pressed, released = g_pad.released;
    unsigned epoch = __atomic_load_n(&g_pad_epoch, __ATOMIC_ACQUIRE);
    int connected = g_pad_device >= 0;
    g_pad.pressed = g_pad.released = 0;
    pthread_mutex_unlock(&g_pad_mx);
    int back = 0;
    pthread_mutex_lock(&g_imx);
    static unsigned prev, frame_epoch, blocked, stick_blocked;
    static int previous_context = -1, previous_menu = -1;
    int menu = menu_mode(), context = g_touch_context;
    sync_touch_context(menu);
    if (frame_epoch != epoch) { prev = blocked = stick_blocked = 0; frame_epoch = epoch; previous_context = -1; }
    if (!connected || epoch != __atomic_load_n(&g_pad_epoch, __ATOMIC_ACQUIRE)
        || !__atomic_load_n(&g_app_input_focus, __ATOMIC_ACQUIRE) || (context == 1 && g_touch_cancel)) {
        g_pad_held_ctl = 0; g_pad_move[0] = g_pad_move[1] = 0;
        prev = buttons; blocked = buttons; stick_blocked = 3;
        recompute(); pthread_mutex_unlock(&g_imx); return;
    }
    if (previous_context >= 0 && (context != previous_context || menu != previous_menu)) {
        blocked |= buttons; stick_blocked = (lx || ly ? 1 : 0) | (rx || ry ? 2 : 0);
        pressed = 0; g_pad_hold_a_ms = 0; g_pad_skip_done = 0;
    }
    previous_context = context; previous_menu = menu;
    blocked &= buttons;
    if (!lx && !ly) stick_blocked &= ~1u; if (!rx && !ry) stick_blocked &= ~2u;
    if (stick_blocked & 1) lx = ly = 0; if (stick_blocked & 2) rx = ry = 0;
    buttons &= ~blocked; pressed &= ~blocked;
    unsigned edge_down = (buttons & ~prev) | pressed, edge_up = (~buttons & prev) | released;
    prev = buttons;
    if (buttons || lx || ly || rx || ry) __atomic_store_n(&g_pad_last_ms, now, __ATOMIC_RELEASE);
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
        if (edge_down & (1u << PAD_A)) {
            // The authored title listens for A; ordinary menu entries listen for the pointer trigger.
            if ((touch_menu_title_mask(now)&1u) && ctl_visible(C_CONFIRM,menu))
                g_btn_hold_until[C_CONFIRM]=now+150;
            else pointer_click(g_ptr_u * g_tw, g_ptr_v * g_th, 100);
        }
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
        if ((buttons & (1u << PAD_A)) && ctl_visible(C_INTERACT,menu)) held |= 1u << C_INTERACT;
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
            else if (ctl_visible(C_FIRE, menu)) {
                touch_button_press(C_FIRE, menu);
                if (!touch_throw_mode(now)) g_btn_hold_until[C_FIRE] = now + 90;
            }
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
         g_pad.buttons, g_pad_last_ms ? ptr_now_ms() - g_pad_last_ms : -1L);
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
