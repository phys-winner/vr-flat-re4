// Throwing the VR-only hand projectiles: the Del Lago spears (AVR4GamePlayerSpear, taken from the lake boat) and
// Ashley's lanterns (AVR4AshleyLantern). Both throw on the original grip release (PrimaryGripReleased), with the
// velocity taken by UThrowHelper::GetThrowParameters from the hand's recent transform history - so the phone
// version holds the grip, moves the virtual right hand forward like a throw and releases at speed.
//  * spear: Fire takes the boat's current spear (forced hover on its primary grip + held right grip), then throws
//  * lantern: a tap on a lantern picks it up the same way; Fire throws the held lantern
// Include after sce_touch (projection helpers), interact (ti_release) and world_touch.
#ifndef RE4FIX_THROW_H
#define RE4FIX_THROW_H

static void* (*tr_spear_class)(void), *(*tr_lantern_class)(void), *(*tr_boat_class)(void);
static int tr_ready;
static unsigned tr_logs;
static void* tr_boat;                         // live UVR4LakeBoatComponent (BeginPlay/EndPlay)
#define TR_MAX_LANTERNS 24
static void* tr_lanterns[TR_MAX_LANTERNS];
static int tr_lantern_count;
static pthread_mutex_t tr_mx = PTHREAD_MUTEX_INITIALIZER;
unsigned long g_tgt_tr_boat_begin, g_tgt_tr_boat_end, g_tgt_tr_lantern_begin;
// Tunables (dev console "throwpose <forward_m> <up_m> <ms>").
static float g_throw_forward = 0.80f, g_throw_up = 0.22f;
static int g_throw_ms = 140;
// Shared state. phase: 0 idle, 1 grabbing, 2 flicking, 3 released (cool-down).
static volatile long g_throw_request;          // input: Fire pressed while a throw is available
static volatile long g_throw_flick_start;      // GameThread -> pose: flick start (ms), 0 = none
static volatile unsigned long long g_throw_ui; // (ms << 32) | mode (1 spear available, 2 holding a throwable)
static struct { int phase; void* hand; void* grip; void* item; int owned_grip; long until, start; } tr_run;

__attribute__((naked)) static void gate_tr_boat_begin(void* self) {
    asm volatile(".inst 0xd10203ff\n" ".inst 0xa90557f6\n" ".inst 0xa9064ff4\n" ".inst 0xa9077bfd\n"
        "adrp x16, :got:g_tgt_tr_boat_begin\n" "ldr x16, [x16, :got_lo12:g_tgt_tr_boat_begin]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static void gate_tr_boat_end(void* self, int reason) {
    asm volatile(".inst 0xa9be4ff4\n" ".inst 0xa9017bfd\n" ".inst 0x910043fd\n" ".inst 0xaa0003f3\n"
        "adrp x16, :got:g_tgt_tr_boat_end\n" "ldr x16, [x16, :got_lo12:g_tgt_tr_boat_end]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static void gate_tr_lantern_begin(void* self) {
    asm volatile(".inst 0xd10203ff\n" ".inst 0xf9002bf5\n" ".inst 0xa9064ff4\n" ".inst 0xa9077bfd\n"
        "adrp x16, :got:g_tgt_tr_lantern_begin\n" "ldr x16, [x16, :got_lo12:g_tgt_tr_lantern_begin]\n" "ldr x16, [x16]\n" "br x16\n");
}
void hook_tr_boat_begin(void* self) { gate_tr_boat_begin(self); tr_boat = self; LOGF("THROW lake boat component %p", self); }
void hook_tr_boat_end(void* self, int reason) { if (tr_boat == self) tr_boat = 0; gate_tr_boat_end(self, reason); }
// AVR4AshleyLantern has no EndPlay override: dead entries are dropped by the weak-object check when a new one arrives.
void hook_tr_lantern_begin(void* self) {
    gate_tr_lantern_begin(self);
    pthread_mutex_lock(&tr_mx);
    int known = 0, kept = 0;
    for (int i = 0; i < tr_lantern_count; i++)
        if (tr_lanterns[i] != self && pv_alive(tr_lanterns[i])) tr_lanterns[kept++] = tr_lanterns[i];
    tr_lantern_count = kept;
    for (int i = 0; i < tr_lantern_count; i++) if (tr_lanterns[i] == self) known = 1;
    if (!known && tr_lantern_count < TR_MAX_LANTERNS) tr_lanterns[tr_lantern_count++] = self;
    pthread_mutex_unlock(&tr_mx);
}
// The registered grip of an item: its PrimaryGrip (+0x270) for props, otherwise a registered grip whose Outer is it.
static void* tr_item_grip(void* item) {
    void* grip = tr_spear_class && tw_typed(item, tr_spear_class(), 0x590) ? *(void* const*)((const char*)item + 0x270) : 0;
    if (grip && reload_grip_registered(grip) && *(void* const*)((const char*)grip + 0x20) == item) return grip;
    grip = 0;
    pthread_mutex_lock(&g_gripmx);
    for (int i = 0; i < g_ngrips && !grip; i++)
        if (*(void* const*)((const char*)g_grips[i].g + 0x20) == item) grip = g_grips[i].g;
    pthread_mutex_unlock(&g_gripmx);
    return grip;
}
// What the right hand holds: 1 spear, 2 lantern, 0 other/none.
static int tr_held_kind(void* hand, void** item_out, void** grip_out) {
    void* grip = f_GetCurrentGrip(hand);
    if (!grip || !reload_grip_registered(grip)) return 0;
    void* item = *(void* const*)((const char*)grip + 0x300);   // validity-checked grabbable parent (see interact.h)
    if (!item || !pv_alive(item)) return 0;
    int kind = tw_typed(item, tr_spear_class(), 0x590) ? 1 : tw_typed(item, tr_lantern_class(), 0x3d0) ? 2 : 0;
    if (kind) { *item_out = item; *grip_out = grip; }
    return kind;
}
static void* tr_boat_spear(void) {
    void* boat = tr_boat;
    if (!boat || !pv_alive(boat) || !tw_typed(boat, tr_boat_class(), 0x240)) return 0;
    void* spear = *(void* const*)((const char*)boat + 0x200);   // UVR4LakeBoatComponent::GetSpear
    return spear && pv_alive(spear) && tw_typed(spear, tr_spear_class(), 0x590) ? spear : 0;
}
static void tr_release_grip(void* hand) {
    if (tr_run.owned_grip) {
        long own = __atomic_load_n(&g_cmd_until[27], __ATOMIC_ACQUIRE);
        if (own == LONG_MAX) __atomic_store_n(&g_cmd_until[27], 0, __ATOMIC_RELEASE);
    } else if (hand && ti_down(hand)) {
        ti_release(hand);
        if (ti_hand == hand) interact_clear_hold(hand, 0);
    }
}
static void tr_reset(const char* why) {
    if (tr_run.phase) {
        if (tr_run.grip && tr_run.hand && reload_grip_registered(tr_run.grip)) f_SetGripForcedHover(tr_run.hand, tr_run.grip, 0);
        // Every phase except the released cool-down (3) may still own the held right grip (spear 1/2, lantern 4/5).
        if (tr_run.phase != 3 && tr_run.owned_grip && __atomic_load_n(&g_cmd_until[27], __ATOMIC_ACQUIRE) == LONG_MAX)
            __atomic_store_n(&g_cmd_until[27], 0, __ATOMIC_RELEASE);
        if (tr_logs < 40) { tr_logs++; LOGF("THROW reset (%s) phase=%d", why, tr_run.phase); }
    }
    memset(&tr_run, 0, sizeof tr_run);
    __atomic_store_n(&g_throw_flick_start, 0, __ATOMIC_RELEASE);
}
// Pose (GameThread, GetNodePoseState3): forward-and-up flick of the right hand while the throw runs.
static void touch_throw_pose(int side, long now, float off[3]) {
    long start = __atomic_load_n(&g_throw_flick_start, __ATOMIC_ACQUIRE);
    if (side != 1 || !start || now < start) return;
    long t = now - start, total = g_throw_ms > 40 && g_throw_ms < 600 ? g_throw_ms : 140;
    float e;
    if (t < total) { float u = (float)t / (float)total; e = u * u; }          // accelerate through the release
    else if (t < total + 260) e = 1.0f - (float)(t - total) / 260.0f;         // follow-through back to rest
    else return;
    off[2] -= g_throw_forward * e;   // OVR head frame: -z forward
    off[1] += g_throw_up * e;
}
// Input thread: Fire while a throw is available.
static int touch_throw_mode(long now) {
    unsigned long long ui = __atomic_load_n(&g_throw_ui, __ATOMIC_ACQUIRE);
    if (!ui || (uint32_t)((uint32_t)now - (uint32_t)(ui >> 32)) > 300) return 0;
    return (int)(ui & 3);
}
static int touch_throw_request(long now) {
    if (!touch_throw_mode(now)) return 0;
    __atomic_store_n(&g_throw_request, now, __ATOMIC_RELEASE);
    return 1;
}
static void touch_throw_hold_tick(void* hand, long now);
// GameThread (controller payload): the right-grip bit this module holds, so in-world menus can mask it.
static unsigned touch_throw_owned_grip_mask(void) {
    return tr_run.phase && tr_run.phase != 3 && tr_run.owned_grip ? 0x08000000u : 0;
}
// GameThread (menu hand tick): an inventory/pause/phone screen must never see the owned right grip held.
static void touch_throw_idle_tick(void) {
    if (tr_run.phase && g_touch_context != 1) tr_reset("menu");
}
// GameThread, right active game hand.
static void touch_throw_tick(void* pawn, void* hand, long now) {
    if (!tr_ready || !pawn || !hand) return;
    if (g_touch_context != 1 || g_touch_cancel || (rl_paused && rl_paused())) {
        if (tr_run.phase) tr_reset("context");
        __atomic_store_n(&g_throw_ui, 0, __ATOMIC_RELEASE);
        return;
    }
    void* item = 0, *grip = 0;
    int held = tr_held_kind(hand, &item, &grip);
    void* spear = held ? 0 : tr_boat_spear();
    int mode = held ? 2 : spear && !f_GetCurrentGrip(hand) ? 1 : 0;
    __atomic_store_n(&g_throw_ui, mode ? ((unsigned long long)(unsigned)now << 32) | (unsigned)mode : 0, __ATOMIC_RELEASE);
    if (tr_run.phase >= 4) { touch_throw_hold_tick(hand, now); return; }   // tapped lantern: pick-up / waiting for Fire
    long request = __atomic_exchange_n(&g_throw_request, 0, __ATOMIC_ACQ_REL);
    switch (tr_run.phase) {
    case 0:
        if (!request || now - request > 400 || now < request) return;
        if (held) {                                   // already holding: throw now
            tr_run.hand = hand; tr_run.item = item; tr_run.grip = grip; tr_run.owned_grip = 0;
            tr_run.phase = 2; tr_run.start = now; tr_run.until = now + 2000;
            __atomic_store_n(&g_throw_flick_start, now, __ATOMIC_RELEASE);
            if (tr_logs < 40) { tr_logs++; LOGF("THROW flick held item=%p kind=%d", item, held); }
            return;
        }
        if (mode == 1) {                              // take the boat's spear, then throw it
            // Like the range counter: the holster's own grip hands the item over; the spear grip is the fallback.
            void* holster = tr_boat ? *(void* const*)((const char*)tr_boat + 0x208) : 0;   // GetSpearHolster
            void* spear_grip = holster && pv_alive(holster) ? tr_item_grip(holster) : 0;
            if (!spear_grip) spear_grip = tr_item_grip(spear);
            if (!spear_grip || g_cmd_until[27] > now || (g_grip_latched & 0x08000000u)) return;
            f_SetGripForcedHover(hand, spear_grip, 1);
            __atomic_store_n(&g_cmd_until[27], LONG_MAX, __ATOMIC_RELEASE);   // owned right grip until the release
            tr_run.hand = hand; tr_run.item = spear; tr_run.grip = spear_grip; tr_run.owned_grip = 1;
            tr_run.phase = 1; tr_run.start = now; tr_run.until = now + 700;
            if (tr_logs < 40) { tr_logs++; LOGF("THROW grab boat spear=%p grip=%p", spear, spear_grip); }
        }
        return;
    case 1:                                           // grabbing the spear
        if (hand != tr_run.hand) return;
        if (now >= tr_run.until) { tr_reset("grab timeout"); return; }
        if (held != 1) return;                        // the holster hands over a spear: wait until one is in hand
        f_SetGripForcedHover(hand, tr_run.grip, 0);
        tr_run.grip = grip; tr_run.item = item;       // the spear's own grip now held by the owned right grip
        tr_run.phase = 2; tr_run.start = now; tr_run.until = now + 2000;
        __atomic_store_n(&g_throw_flick_start, now, __ATOMIC_RELEASE);
        if (tr_logs < 40) { tr_logs++; LOGF("THROW spear in hand, flick"); }
        return;
    case 2:                                           // flick: release at full speed
        if (hand != tr_run.hand) return;
        if (f_GetCurrentGrip(hand) != tr_run.grip) { tr_reset("item left the hand"); return; }
        if (now - tr_run.start < (long)g_throw_ms) return;
        tr_release_grip(hand);
        tr_run.phase = 3; tr_run.until = now + 450;
        if (tr_logs < 40) { tr_logs++; LOGF("THROW release item=%p after %ldms", tr_run.item, now - tr_run.start); }
        return;
    default:                                          // follow-through / cool-down
        if (now >= tr_run.until) {
            if (tr_logs < 40) { tr_logs++; LOGF("THROW done held_after=%p", f_GetCurrentGrip(hand)); }
            tr_reset("done");
        }
        return;
    }
}
// GameThread (world tap): pick up the lantern under the tap.
static int touch_throw_tap(void* hand, const struct reload_tf* camera, float tx, float ty, long now) {
    if (!tr_ready || !hand || tr_run.phase || f_GetCurrentGrip(hand) || g_cmd_until[27] > now) return 0;
    float limit = 0.12f * (g_comp_h ? (float)g_comp_h : 1080.0f), best_d2 = limit * limit;
    void* best = 0;
    pthread_mutex_lock(&tr_mx);
    for (int i = 0; i < tr_lantern_count; i++) {
        void* lantern = tr_lanterns[i];
        if (!pv_alive(lantern)) continue;
        void* root = *(void* const*)((const char*)lantern + 0x130);
        float world[3], sx, sy, depth;
        if (!root || !pv_alive(root) || !wt_component_location(root, world) || !wt_project(camera, world, &sx, &sy, &depth)
            || depth > 300.0f) continue;
        float d2 = (sx - tx) * (sx - tx) + (sy - ty) * (sy - ty);
        if (d2 < best_d2) { best_d2 = d2; best = lantern; }
    }
    pthread_mutex_unlock(&tr_mx);
    void* grip = best ? tr_item_grip(best) : 0;
    if (!grip) return 0;
    f_SetGripForcedHover(hand, grip, 1);
    __atomic_store_n(&g_cmd_until[27], LONG_MAX, __ATOMIC_RELEASE);
    // Phase 1 without a pending throw: once in hand the grip stays held until Fire throws it.
    tr_run.hand = hand; tr_run.item = best; tr_run.grip = grip; tr_run.owned_grip = 1;
    tr_run.phase = 4; tr_run.start = now; tr_run.until = now + 700;
    if (tr_logs < 40) { tr_logs++; LOGF("THROW pick up lantern=%p grip=%p", best, grip); }
    return 1;
}
// Lantern pick-up completion and the owned hold while it waits for Fire (phase 4/5 handled here).
static void touch_throw_hold_tick(void* hand, long now) {
    if (tr_run.phase != 4 && tr_run.phase != 5) return;
    if (hand != tr_run.hand) return;
    if (tr_run.phase == 4) {
        if (f_GetCurrentGrip(hand) == tr_run.grip) {
            f_SetGripForcedHover(hand, tr_run.grip, 0); tr_run.phase = 5;
            if (tr_logs < 40) { tr_logs++; LOGF("THROW lantern in hand"); }
        } else if (now >= tr_run.until) tr_reset("pick-up timeout");
        return;
    }
    if (f_GetCurrentGrip(hand) != tr_run.grip) { tr_reset("lantern left the hand"); return; }
    long request = __atomic_exchange_n(&g_throw_request, 0, __ATOMIC_ACQ_REL);
    if (request && now - request < 400 && now >= request) {
        tr_run.phase = 2; tr_run.start = now; tr_run.until = now + 2000;
        __atomic_store_n(&g_throw_flick_start, now, __ATOMIC_RELEASE);
        if (tr_logs < 40) { tr_logs++; LOGF("THROW flick lantern"); }
    }
}
static void install_touch_throw(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[4]; } check[] = {
        {0x62fcfc8, {0xd10203ff, 0xa90557f6, 0xa9064ff4, 0xa9077bfd}},   // UVR4LakeBoatComponent::BeginPlay
        {0x62fd4ec, {0xa9be4ff4, 0xa9017bfd, 0x910043fd, 0xaa0003f3}},   // UVR4LakeBoatComponent::EndPlay
        {0x6204c2c, {0xd10203ff, 0xf9002bf5, 0xa9064ff4, 0xa9077bfd}},   // AVR4AshleyLantern::BeginPlay
        {0x643b078, {0xd10183ff, 0xf90023f3, 0, 0}},                     // AVR4GamePlayerSpear::GetPrivateStaticClass
        {0x63bd270, {0xd10183ff, 0xf90023f3, 0, 0}},                     // AVR4AshleyLantern::GetPrivateStaticClass
        {0x6465b44, {0xd10183ff, 0xf90023f3, 0, 0}},                     // UVR4LakeBoatComponent::GetPrivateStaticClass
        {0x6465664, {0xf9401028, 0xf100011f, 0x9a880508, 0xf9001028}},   // execGetSpearHolster (field +0x208)
        {0x6465680, {0xf9401028, 0xf100011f, 0x9a880508, 0xf9001028}},   // execGetSpear (field +0x200)
    };
    tr_ready = 0;
    if (!base) return;
    for (unsigned i = 0; i < sizeof check / sizeof *check; i++) {
        unsigned words = check[i].first[2] || check[i].first[3] ? 4 : 2;
        if (memcmp((const void*)(base + check[i].addr), check[i].first, words * 4)) {
            LOGF("THROW disabled: opcode mismatch +%lx", check[i].addr); return;
        }
    }
    if (*(const unsigned*)(base + 0x6465690) != 0xf9410008) { LOGF("THROW disabled: spear field"); return; }   // ldr x8,[x0,#0x200]
    tr_spear_class = (void* (*)(void))(base + 0x643b078);
    tr_lantern_class = (void* (*)(void))(base + 0x63bd270);
    tr_boat_class = (void* (*)(void))(base + 0x6465b44);
    g_tgt_tr_boat_begin = base + 0x62fcfc8 + 16;
    g_tgt_tr_boat_end = base + 0x62fd4ec + 16;
    g_tgt_tr_lantern_begin = base + 0x6204c2c + 16;
    if (patch_code((void*)(base + 0x62fcfc8), (void*)&hook_tr_boat_begin) || patch_code((void*)(base + 0x62fd4ec), (void*)&hook_tr_boat_end)
        || patch_code((void*)(base + 0x6204c2c), (void*)&hook_tr_lantern_begin)) {
        LOGF("THROW disabled: boat/lantern hook"); return;
    }
    tr_ready = 1;
    LOGF("THROW ready: boat spears and lanterns thrown by the original grip release");
}
#endif
