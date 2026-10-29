// Include after phone getters; caller declares g_touch_back_request and calls on existing GT hand Tick.
static int (*unpause_try)(unsigned char);
static void* (*unpause_pawn_class)(void);
static void* (*unpause_screen_class)(void);
static int (*unpause_ui_back)(void*); // 0: ordinary pause; positive: native UI Back; negative: modal/refused UI Back.

static int unpause_layout(const void* object, const void* expected, int minimum) {
    const char* cls = object ? *(const char* const*)((const char*)object+0x10) : 0;
    if (!cls || !expected) return 0;
    int size = *(const int*)(cls+0x58), expected_size = *(const int*)((const char*)expected+0x58);
    int depth = *(const int*)((const char*)expected+0x38), actual_depth = *(const int*)(cls+0x38);
    const void* const* chain = *(const void* const**)(cls+0x30);
    return size >= minimum && size <= 0x10000 && expected_size >= minimum && expected_size <= size
        && depth >= 0 && actual_depth <= 1024 && depth <= actual_depth && chain
        && chain[depth] == (const char*)expected+0x30;
}
static void touch_unpause_tick(void* hand) {
    int (*try_unpause)(unsigned char) = __atomic_load_n(&unpause_try,__ATOMIC_ACQUIRE);
    if (!try_unpause || !phone_player || !phone_active || !phone_type || !hand
        || *(const unsigned char*)((const char*)hand+0x314) != 1
        || !__atomic_load_n(&g_touch_back_request,__ATOMIC_ACQUIRE)) return;
    void* pawn = phone_player(hand);
    if (!pawn || pawn != phone_active()) return; // Pooled inactive hands cannot consume Back.
    unsigned type = phone_type();
    if (!__atomic_exchange_n(&g_touch_back_request,0,__ATOMIC_ACQ_REL)) return;
    if (type != 2 || !unpause_layout(pawn,unpause_pawn_class(),0x11b8)) {
        LOGF("UNPAUSE unavailable: active type=%u or PausePawn layout mismatch",type); return;
    }
    void* screen = *(void* const*)((const char*)pawn+0x11b0);
    if (!unpause_layout(screen,unpause_screen_class(),0x28)) {
        LOGF("UNPAUSE unavailable: missing or invalid PauseUIScreen"); return;
    }
    touch_menu_grip_cancel(hand); // Release only our owned menu drag before the native modal/animation gate.
    int menu_back=unpause_ui_back ? unpause_ui_back(screen):0;
    if (menu_back) {
        LOGF("UNPAUSE options native Back result=%d pawn=%p screen=%p",menu_back,pawn,screen);
        return;
    }
    // Static TryUnpause retains pause availability, animation state and dynamic UI/modal permission.
    int accepted = try_unpause(0); // EVR4TryUnpauseReason::Game; stock UpdateQueuedPause uses the same route.
    LOGF("UNPAUSE native Game accepted=%d pawn=%p screen=%p",accepted,pawn,screen);
}
static void install_touch_unpause(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[2]; } check[] = {
        {0x63324a0,{0xa9be4ff4,0xa9017bfd}}, {0x63324b0,{0x52800020,0x97fa7bce}},
        {0x6332544,{0x52823448,0x38686a88}}, {0x6332554,{0xf948da80,0x9405d652}},
        {0x6332628,{0x2a1f03e0,0x97ffff9d}}, {0x64a676c,{0x394013e0,0x97fa2f4c}},
        {0x64a7008,{0xd10183ff,0xf90023f3}}, {0x64a8138,{0xd10183ff,0xf90023f3}},
        {0x64a7ea0,{0xd100c3ff,0xa9014ff4}}, {0x64a7ee0,{0xf9410d08,0xd63f0100}},
        {0x633d030,{0xf9417800,0xd65f03c0}}, {0x63380dc,{0xd0020168,0xf9431508}},
        {0x6338aec,{0xd0020168,0xf9431508}}, {0x687d798,{0x294b2408,0x2a0203f6}},
        {0x633d038,{0x394c5000,0xd65f03c0}},
    };
    if (!base || __atomic_load_n(&unpause_try,__ATOMIC_ACQUIRE)) return;
    for (unsigned i = 0; i < sizeof check/sizeof check[0]; i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,sizeof check[i].first)) {
            LOGF("UNPAUSE disabled: opcode mismatch +%lx",check[i].addr); return;
        }
    if (*(const long long*)(base+0x9df5528) != 0 || *(const unsigned*)(base+0x9df629c) != 0x11b0
        || phone_player != (void* (*)(const void*))(base+0x633d030)
        || phone_active != (void* (*)(void))(base+0x63380dc)
        || phone_type != (unsigned (*)(void))(base+0x6338aec)) {
        LOGF("UNPAUSE disabled: enum, UI field or shared getter mismatch"); return;
    }
    unpause_pawn_class = (void* (*)(void))(base+0x64a7008);
    unpause_screen_class = (void* (*)(void))(base+0x64a8138);
    __atomic_store_n(&unpause_try,(int (*)(unsigned char))(base+0x63324a0),__ATOMIC_RELEASE);
    LOGF("UNPAUSE installed: explicit active PausePawn Back, native Game reason");
}
