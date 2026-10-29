// Existing game/menu hand Tick hooks consume pause on their active pawn's GameThread.
static void* (*f_touch_hand_player)(const void* self);
static void (*f_touch_pause_pressed)(void* self);
static int (*f_touch_pawn_active)(const void* self);
unsigned long g_tgt_touch_app_event;

__attribute__((naked)) static void gate_touch_app_event(void* self, int state, void* data) {
    asm volatile(
        ".inst 0xd104c3ff\n" ".inst 0xa90f5ffc\n"
        ".inst 0xa91057f6\n" ".inst 0xa9114ff4\n"
        "adrp x16, :got:g_tgt_touch_app_event\n"
        "ldr x16, [x16, :got_lo12:g_tgt_touch_app_event]\n"
        "ldr x16, [x16]\n" "br x16\n");
}

void hook_touch_app_event(void* self, int state, void* data) {
    // UE4 states: window destroyed=3, pause=6, resume=7, stop=8, focus lost=10, focus gained=11.
    if (state == 3 || state == 6 || state == 8 || state == 10) {
        touch_reset();
        // The game's own focus suspension (sound fade + pause) runs while the window has no input focus.
        __atomic_store_n(&g_app_input_focus, 0, __ATOMIC_RELEASE);
    } else if (state == 11) __atomic_store_n(&g_app_input_focus, 1, __ATOMIC_RELEASE);
    if (state == 10 || state == 11) LOGF("APPFOCUS state=%d input_focus=%d", state, g_app_input_focus);
    gate_touch_app_event(self, state, data);
}

static void touch_pause_tick(void* hand) {
    if (!f_touch_hand_player || !__atomic_load_n(&g_touch_pause, __ATOMIC_ACQUIRE)) return;
    void* pawn = f_touch_hand_player(hand);
    if (pawn && f_touch_pawn_active(pawn) && __atomic_exchange_n(&g_touch_pause, 0, __ATOMIC_ACQ_REL)) {
        f_touch_pause_pressed(pawn);
        LOGF("touch pause queued on active pawn=%p", pawn);
    }
}

static void install_touch_pause(unsigned long base) {
    const unsigned int player[2] = {0xf9417800, 0xd65f03c0};
    const unsigned int pause[3] = {0x52800028, 0x39130008, 0xd65f03c0};
    const unsigned int active[2] = {0x39550000, 0xd65f03c0};
    const unsigned int event[4] = {0xd104c3ff, 0xa90f5ffc, 0xa91057f6, 0xa9114ff4};
    if (memcmp((void*)(base + 0x633d030), player, sizeof player) ||
        memcmp((void*)(base + 0x6340b0c), pause, sizeof pause) ||
        memcmp((void*)(base + 0x6341174), active, sizeof active) ||
        memcmp((void*)(base + 0x54ad814), event, sizeof event)) {
        LOGI("touch pause/reset disabled: native preimage mismatch");
        return;
    }
    f_touch_pause_pressed = (void (*)(void*))(base + 0x6340b0c);
    f_touch_pawn_active = (int (*)(const void*))(base + 0x6341174);
    f_touch_hand_player = (void* (*)(const void*))(base + 0x633d030);
    LOGI("touch pause installed: existing hand Tick hooks");
    g_tgt_touch_app_event = base + 0x54ad824;
    if (patch_code((void*)(base + 0x54ad814), (void*)&hook_touch_app_event)) {
        LOGI("touch focus reset disabled: EnqueueAppEvent patch failed");
        return;
    }
    LOGI("touch focus reset installed: window destroy/pause/stop/focus lost");
}
