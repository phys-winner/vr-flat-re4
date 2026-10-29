// Touch adaptation of in-world VR interaction surfaces. Include after prompt_visual, interact and aim_diag.
//  * AVR4FloaterUI prompts ("EXIT", "OPEN" ...) follow the head with RotationSmoothing; on a phone the
//    authored damped swing makes them drift into view. Smoothing 0 keeps them fixed on screen.
//  * A short tap on the prompt presses its authored command (same path as the action icon).
//  * Authored prompt commands and UMG artwork are retained; prompt_visual owns optional controller-mesh hiding.
//  * UVR4ActionButtonComponent world buttons (bells, wall buttons) are activated by tapping them on screen:
//    the original ActivateButton() runs only when the original CanPressButton() allows it.
#ifndef RE4FIX_WORLD_TOUCH_H
#define RE4FIX_WORLD_TOUCH_H

#define WT_MAX_BUTTONS 64
static void* wt_buttons[WT_MAX_BUTTONS];
static int wt_button_count;
static pthread_mutex_t wt_mx = PTHREAD_MUTEX_INITIALIZER;
static int (*wt_can_press)(const void*);
static void (*wt_activate)(void*);
static int (*wt_floater_displaying)(const void*);
static volatile unsigned long long g_world_tap;      // (ms << 32) | (x << 16) | y, surface pixels
static volatile float g_prompt_sx, g_prompt_sy;       // projected prompt widget centre
static volatile long g_prompt_seen_ms;
static unsigned wt_logs;
unsigned long g_tgt_ab_begin, g_tgt_ab_end, g_tgt_floater_cmd, g_tgt_floater_level;

__attribute__((naked)) static void gate_ab_begin(void* self) {
    asm volatile(".inst 0xf81e0ff3\n" ".inst 0xa9017bfd\n" ".inst 0x910043fd\n" ".inst 0xaa0003f3\n"
        "adrp x16, :got:g_tgt_ab_begin\n" "ldr x16, [x16, :got_lo12:g_tgt_ab_begin]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static void gate_ab_end(void* self, int reason) {
    asm volatile(".inst 0xf81e0ff3\n" ".inst 0xa9017bfd\n" ".inst 0x910043fd\n" ".inst 0xaa0003f3\n"
        "adrp x16, :got:g_tgt_ab_end\n" "ldr x16, [x16, :got_lo12:g_tgt_ab_end]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static void gate_floater_cmd(void* self, const void* text, int command, int action, int qte, int size) {
    asm volatile(".inst 0xd10283ff\n" ".inst 0xf90023fb\n" ".inst 0xa90567fa\n" ".inst 0xa9065ff8\n"
        "adrp x16, :got:g_tgt_floater_cmd\n" "ldr x16, [x16, :got_lo12:g_tgt_floater_cmd]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static void gate_floater_level(void* self, const void* text, int command) {
    asm volatile(".inst 0xd10203ff\n" ".inst 0xa9045ff8\n" ".inst 0xa90557f6\n" ".inst 0xa9064ff4\n"
        "adrp x16, :got:g_tgt_floater_level\n" "ldr x16, [x16, :got_lo12:g_tgt_floater_level]\n" "ldr x16, [x16]\n" "br x16\n");
}
static int wt_owner_name(const void* component, char* out, int n) {
    out[0] = 0;
    void* owner = component ? *(void* const*)((const char*)component + 0x20) : 0;
    if (owner) obj_name(owner, out, n);
    return owner != 0;
}
void hook_ab_begin(void* self) {
    gate_ab_begin(self);
    pthread_mutex_lock(&wt_mx);
    int known = 0;
    for (int i = 0; i < wt_button_count; i++) if (wt_buttons[i] == self) known = 1;
    if (!known && wt_button_count < WT_MAX_BUTTONS) wt_buttons[wt_button_count++] = self;
    pthread_mutex_unlock(&wt_mx);
    if (wt_logs < 40) { char owner[96]; wt_owner_name(self, owner, sizeof owner); wt_logs++; LOGF("WORLDBUTTON + %p owner='%s'", self, owner); }
}
void hook_ab_end(void* self, int reason) {
    pthread_mutex_lock(&wt_mx);
    for (int i = 0; i < wt_button_count; i++) if (wt_buttons[i] == self) { wt_buttons[i] = wt_buttons[--wt_button_count]; break; }
    pthread_mutex_unlock(&wt_mx);
    gate_ab_end(self, reason);
}
// Keep native command semantics; a delayed, bounded tree dump is diagnostic only.
static volatile int g_prompt_tree_pending;
void hook_floater_cmd(void* self, const void* text, int command, int action, int qte, int size) {
    gate_floater_cmd(self, text, command, action, qte, size);
    __atomic_store_n(&g_prompt_tree_pending, 6, __ATOMIC_RELEASE);
    if (wt_logs < 70) { wt_logs++; LOGF("WORLDPROMPT command=%d action=%d qte=%d size=%d", command, action, qte, size); }
}
void hook_floater_level(void* self, const void* text, int command) {
    gate_floater_level(self, text, command);
    __atomic_store_n(&g_prompt_tree_pending, 6, __ATOMIC_RELEASE);
    if (wt_logs < 70) { wt_logs++; LOGF("WORLDPROMPT level command=%d", command); }
}
// ---- Read-only UMG diagnostics ----
static void* (*wt_panel_class)(void), *(*wt_user_class)(void);
static void (*wt_set_visibility)(void*, unsigned char);
static unsigned wt_tree_dumps;
static void wt_widget_walk(void* widget, int depth, int* budget) {
    if (!widget || depth > 10 || --*budget < 0 || !pv_alive(widget)) return;
    char name[96] = "", cls[80] = "";
    obj_name(widget, name, sizeof name);
    obj_name(*(void* const*)((const char*)widget + 0x10), cls, sizeof cls);
    LOGF("WORLDPROMPT tree %*s%s '%s' vis=%u %p", depth * 2, "", cls, name,
         *(const unsigned char*)((const char*)widget + 0xc3), widget);
    if (wt_user_class && interact_is_a(widget, wt_user_class())) {
        void* tree = *(void* const*)((const char*)widget + 0x1a8);
        if (tree && pv_alive(tree)) wt_widget_walk(*(void* const*)((const char*)tree + 0x28), depth + 1, budget);
    } else if (wt_panel_class && interact_is_a(widget, wt_panel_class())) {
        struct { void** data; int count, max; } slots;
        memcpy(&slots, (const char*)widget + 0x108, sizeof slots);
        if (slots.count < 0 || slots.count > 64 || slots.max < slots.count || (slots.count && !slots.data)) return;
        for (int i = 0; i < slots.count; i++) {
            void* slot = slots.data[i];
            if (slot && pv_alive(slot)) wt_widget_walk(*(void* const*)((const char*)slot + 0x30), depth + 1, budget);
        }
    }
}

static int wt_project(const struct reload_tf* camera, const float world[3], float* sx, float* sy, float* depth) {
    qf_t inverse = {-camera->q.x, -camera->q.y, -camera->q.z, camera->q.w};
    float d[3] = {world[0] - camera->p[0], world[1] - camera->p[1], world[2] - camera->p[2]}, v[3];
    q_rot(inverse, d, v);                     // UE view: X forward, Y right, Z up (centimetres)
    if (!isfinite(v[0]) || v[0] < 5.0f) return 0;
    float W = g_comp_w ? (float)g_comp_w : 2400.0f, H = g_comp_h ? (float)g_comp_h : 1080.0f;
    float th = tanf(g_fov_h_deg * 0.5f * 0.01745329f), tv = th * H / W;
    *sx = (0.5f + 0.5f * (v[1] / v[0]) / th) * W;
    *sy = (0.5f - 0.5f * (v[2] / v[0]) / tv) * H;
    if (depth) *depth = v[0];
    return isfinite(*sx) && isfinite(*sy);
}
static int wt_component_location(const void* component, float out[3]) {
    if (!component) return 0;
    const char* cls = *(const char* const*)((const char*)component + 0x10);
    int size = cls ? *(const int*)(cls + 0x58) : 0;
    if (size < 0x1f0 || size > 0x10000) return 0;
    struct reload_tf tf; memcpy(&tf, (const char*)component + 0x1c0, sizeof tf);
    memcpy(out, tf.p, 12);
    return isfinite(out[0]) && isfinite(out[1]) && isfinite(out[2]);
}
// Input thread: a short still tap in gameplay that did not hit a HUD control.
static void touch_world_tap(float x, float y) {
    if (x < 0 || y < 0 || x > 65535 || y > 65535) return;
    unsigned long long packed = ((unsigned long long)(unsigned)ptr_now_ms() << 32) | ((unsigned)x << 16) | (unsigned)y;
    __atomic_store_n(&g_world_tap, packed, __ATOMIC_RELEASE);
}
// Input thread: is (x, y) on the projected prompt? (fresh GameThread projection only)
static int touch_throw_tap(void* hand, const struct reload_tf* camera, float tx, float ty, long now);
static int touch_prompt_hit(float x, float y) {
    if (ptr_now_ms() - __atomic_load_n(&g_prompt_seen_ms, __ATOMIC_ACQUIRE) > 300) return 0;
    float W = g_comp_w ? (float)g_comp_w : 2400.0f, H = g_comp_h ? (float)g_comp_h : 1080.0f;
    return fabsf(x - g_prompt_sx) < 0.17f * W && y > g_prompt_sy - 0.17f * H && y < g_prompt_sy + 0.05f * H;
}
// GameThread, right active game hand tick.
// Shooting-range challenge weapons wait in a world holster on the counter: a tap on one grabs it with the right
// hand the way a VR hand does (forced hover on the holster grip + one right-grip pulse); the native holster
// handler hands the gun over; confirmed grabs use the native Quick Select retention before the pulse ends.
static struct { void* hand; void* grip; long off_at; } wt_grab;
static void wt_grab_tick(void* hand, long now) {
    if (!wt_grab.grip || hand != wt_grab.hand) return;
    if (now < wt_grab.off_at) {touch_quickselect_world_retention(hand);return;}
    if (reload_grip_registered(wt_grab.grip)) f_SetGripForcedHover(hand, wt_grab.grip, 0);
    void* held = f_GetCurrentGrip(hand);
    LOGF("WORLDGRAB done grip=%p held=%p", wt_grab.grip, held);
    memset(&wt_grab, 0, sizeof wt_grab);
}
static int wt_range_grab(void* hand, const struct reload_tf* camera, float tx, float ty, long now) {
    if (!hand || wt_grab.grip || f_GetCurrentGrip(hand) || g_cmd_until[27] > now) return 0;
    float limit = 0.12f * (g_comp_h ? (float)g_comp_h : 1080.0f), best_d2 = limit * limit;
    void* best = 0;
    pthread_mutex_lock(&g_gripmx);
    for (int i = 0; i < g_ngrips; i++) {
        if (strcmp(g_grips[i].comp, "HolsterGrip") || strncmp(g_grips[i].owner, "ShootingRangeHolster", 20)) continue;
        void* holster = *(void* const*)((const char*)g_grips[i].g + 0x20);
        void* root = holster ? *(void* const*)((const char*)holster + 0x130) : 0;
        float world[3], sx, sy, depth;
        if (!root || !wt_component_location(root, world) || !wt_project(camera, world, &sx, &sy, &depth) || depth > 450.0f) continue;
        float d2 = (sx - tx) * (sx - tx) + (sy - ty) * (sy - ty);
        if (d2 < best_d2) { best_d2 = d2; best = g_grips[i].g; }
    }
    pthread_mutex_unlock(&g_gripmx);
    if (!best || !reload_grip_registered(best)) return 0;
    f_SetGripForcedHover(hand, best, 1);
    g_cmd_until[27] = now + 350;            // right grip pulse (OB_RHAND)
    wt_grab.hand = hand; wt_grab.grip = best; wt_grab.off_at = now + 700;
    LOGF("WORLDGRAB range weapon holster grip=%p distance=%.0fpx", best, sqrtf(best_d2));
    return 1;
}
static void touch_world_tick(void* pawn, void* hand, long now) {
    wt_grab_tick(hand, now);
    if (!pawn || !aim_head || g_touch_context != 1) return;
    struct reload_tf camera = aim_head(pawn);
    if (!reload_tf_valid(&camera)) return;
    camera.q = q_mul(camera.q, pose_camera_delta_ue());
    // Prompt: fixed on screen, centre published for tap hits.
    void* floater = touch_prompt_visual_probe();
    if (floater) {
        *(float*)((char*)floater + 0x22c) = 0.0f;    // AVR4FloaterUI::RotationSmoothing
        int shown = wt_floater_displaying ? wt_floater_displaying(floater) : 0;
        float best_x = 0, best_y = 0; int found = 0;
        if (shown) {
            void* queue[32] = {*(void* const*)((const char*)floater + 0x130)}; unsigned count = 1;
            for (unsigned i = 0; i < count; i++) {
                void* component = queue[i];
                if (!pv_component(component, floater, 0x150)) continue;
                char kind[64] = "";
                phone_inspect_fname((const char*)*(void* const*)((const char*)component + 0x10) + 0x18, kind, sizeof kind);
                float world[3], sx, sy, depth;
                if (!strcmp(kind, "WidgetComponent") && wt_component_location(component, world)
                    && wt_project(&camera, world, &sx, &sy, &depth)) {
                    float W = g_comp_w ? (float)g_comp_w : 2400.0f, H = g_comp_h ? (float)g_comp_h : 1080.0f;
                    int on_screen = sx >= 0 && sx <= W && sy >= 0 && sy <= H;
                    if (on_screen && (!found || sy > best_y)) { best_x = sx; best_y = sy; found = 1; }
                    if (wt_logs < 60) { wt_logs++; LOGF("WORLDPROMPT widget=%p screen=%.0f,%.0f depth=%.1f", component, sx, sy, depth); }
                    // Inspect the authored tree without changing caption, glyph or QTE visibility.
                    void* user = *(void* const*)((const char*)component + 0x498);
                    if (user && pv_alive(user)) {
                        int budget = 160;
                        if (wt_tree_dumps < 3 && __atomic_load_n(&g_prompt_tree_pending, __ATOMIC_ACQUIRE) == 1) {
                            wt_tree_dumps++; wt_widget_walk(user, 0, &budget);
                        }
                    }
                }
                struct { void** data; int count, max; } children;
                memcpy(&children, (const char*)component + 0xd0, sizeof children);
                if (children.count < 0 || children.count > 32 || children.max < children.count || (children.count && !children.data)) continue;
                for (int j = 0; j < children.count && count < 32; j++) queue[count++] = children.data[j];
            }
        }
        if (found) { g_prompt_sx = best_x; g_prompt_sy = best_y; __atomic_store_n(&g_prompt_seen_ms, now, __ATOMIC_RELEASE); }
        int pending = __atomic_load_n(&g_prompt_tree_pending, __ATOMIC_ACQUIRE);
        if (pending > 0) __atomic_store_n(&g_prompt_tree_pending, pending - 1, __ATOMIC_RELEASE);
    }
    unsigned long long tap = __atomic_exchange_n(&g_world_tap, 0, __ATOMIC_ACQ_REL);
    if (!tap || (unsigned)now - (unsigned)(tap >> 32) > 600) return;
    float tx = (float)((tap >> 16) & 0xffff), ty = (float)(tap & 0xffff);
    if (touch_sce_tap(tx, ty, now)) return;
    if (touch_throw_tap(hand, &camera, tx, ty, now)) return;
    if (wt_range_grab(hand, &camera, tx, ty, now)) return;
    // Nearest pressable world button within ~9% of the screen height of the tap.
    pthread_mutex_lock(&wt_mx);
    void* best = 0; float best_d2 = 0;
    float limit = 0.09f * (g_comp_h ? (float)g_comp_h : 1080.0f);
    for (int i = 0; i < wt_button_count; i++) {
        void* button = wt_buttons[i];
        if (!pv_alive(button) || !wt_can_press(button)) continue;
        void* volumes[3] = {*(void* const*)((const char*)button + 0x320), *(void* const*)((const char*)button + 0x328),
                            *(void* const*)((const char*)button + 0x310)};
        for (int v = 0; v < 3; v++) {
            float world[3], sx, sy, depth;
            if (!volumes[v] || !pv_alive(volumes[v]) || !wt_component_location(volumes[v], world)
                || !wt_project(&camera, world, &sx, &sy, &depth) || depth > 450.0f) continue;
            float d2 = (sx - tx) * (sx - tx) + (sy - ty) * (sy - ty);
            if (d2 <= limit * limit && (!best || d2 < best_d2)) { best = button; best_d2 = d2; }
            break;
        }
    }
    pthread_mutex_unlock(&wt_mx);
    if (best) {
        char owner[96]; wt_owner_name(best, owner, sizeof owner);
        wt_activate(best);
        LOGF("WORLDBUTTON tap activated %p owner='%s' distance=%.0fpx", best, owner, sqrtf(best_d2));
    } else if (wt_logs < 80) { wt_logs++; LOGF("WORLDBUTTON tap %.0f,%.0f: no pressable button nearby (%d known)", tx, ty, wt_button_count); }
}
static void install_world_touch(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[4]; } check[] = {
        {0x61f84f0, {0xf81e0ff3, 0xa9017bfd, 0x910043fd, 0xaa0003f3}},   // UVR4ActionButtonComponent::BeginPlay
        {0x61f8520, {0xf81e0ff3, 0xa9017bfd, 0x910043fd, 0xaa0003f3}},   // UVR4ActionButtonComponent::EndPlay
        {0x64057c0, {0xd10283ff, 0xf90023fb, 0xa90567fa, 0xa9065ff8}},   // AVR4FloaterUI::SetInteractionCommand
        {0x6405b64, {0xd10203ff, 0xa9045ff8, 0xa90557f6, 0xa9064ff4}},   // AVR4FloaterUI::SetLevelPrompt
        {0x61f9100, {0x39496408, 0x34000068, 0x2a1f03e0, 0xd65f03c0}},   // CanPressButton
        {0x61f92b4, {0xd10103ff, 0xf9000bf5, 0xa9024ff4, 0xa9037bfd}},   // ActivateButton
    };
    for (unsigned i = 0; i < sizeof check / sizeof *check; i++)
        if (memcmp((const void*)(base + check[i].addr), check[i].first, 16)) {
            LOGF("WORLDTOUCH disabled: opcode mismatch +%lx", check[i].addr); return;
        }
    if (*(const unsigned*)(base + 0x9d934f8 + 0x24) != 0x22c) { LOGF("WORLDTOUCH disabled: RotationSmoothing offset"); return; }
    wt_can_press = (int (*)(const void*))(base + 0x61f9100);
    wt_activate = (void (*)(void*))(base + 0x61f92b4);
    wt_floater_displaying = (int (*)(const void*))(base + 0x64055f0);
    wt_panel_class = (void* (*)(void))(base + 0x7e1495c);
    wt_user_class = (void* (*)(void))(base + 0x7e37bec);
    wt_set_visibility = (void (*)(void*, unsigned char))(base + 0x7da4280);
    g_tgt_ab_begin = base + 0x61f84f0 + 16; patch_code((void*)(base + 0x61f84f0), (void*)&hook_ab_begin);
    g_tgt_ab_end = base + 0x61f8520 + 16; patch_code((void*)(base + 0x61f8520), (void*)&hook_ab_end);
    g_tgt_floater_cmd = base + 0x64057c0 + 16; patch_code((void*)(base + 0x64057c0), (void*)&hook_floater_cmd);
    g_tgt_floater_level = base + 0x6405b64 + 16; patch_code((void*)(base + 0x6405b64), (void*)&hook_floater_level);
    LOGF("WORLDTOUCH ready: fixed prompts, authored command artwork, prompt tap and tappable world buttons");
}
#endif
