// Two-finger pinch on the pause-menu map. In VR the map zooms while both grips are held and the two pointers move
// apart (FVR4MapUIScreen_ScaleState); a phone pinch drives the same AVR4MapUIScreen::ZoomAroundMapCoord around the
// current view centre. Include after flat_view (UI manager access) and prompt_visual (pv_alive).
#ifndef RE4FIX_MAP_ZOOM_H
#define RE4FIX_MAP_ZOOM_H

static void* (*mz_map_class)(void);
static void (*mz_zoom)(void*, const float*, float);
static volatile long g_map_screen_ms;        // GameThread: the top UI screen is the map
static volatile unsigned g_map_zoom_bits;    // input thread: pending zoom factor (float bits), 0 = none
static unsigned mz_logs;

// Input thread: multiply the pending factor (> 1 zooms in).
static void touch_map_zoom_add(float ratio) {
    if (!isfinite(ratio) || ratio <= 0.0f) return;
    unsigned old = __atomic_load_n(&g_map_zoom_bits, __ATOMIC_ACQUIRE), next;
    do {
        float f = 1.0f;
        if (old) memcpy(&f, &old, 4);
        f *= ratio;
        if (f < 0.25f) f = 0.25f; if (f > 4.0f) f = 4.0f;
        memcpy(&next, &f, 4);
    } while (!__atomic_compare_exchange_n(&g_map_zoom_bits, &old, next, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
}
static int touch_map_active(long now) {
    return now - __atomic_load_n(&g_map_screen_ms, __ATOMIC_ACQUIRE) < 300;
}
// GameThread (menu hand tick).
static void touch_map_zoom_tick(long now) {
    if (!mz_zoom || !fv_ui_manager || !fv_peek_screen) return;
    void* manager = fv_ui_manager();
    void* screen = manager ? fv_peek_screen(manager) : 0;
    if (!screen || !pv_alive(screen) || !tw_typed(screen, mz_map_class(), 0x388)) return;
    __atomic_store_n(&g_map_screen_ms, now, __ATOMIC_RELEASE);
    unsigned bits = __atomic_exchange_n(&g_map_zoom_bits, 0, __ATOMIC_ACQ_REL);
    if (!bits) return;
    float factor, scale, centre[3];
    memcpy(&factor, &bits, 4);
    memcpy(&scale, (const char*)screen + 0x370, 4);           // visible fraction of the map, 1 = fully zoomed out
    memcpy(centre, (const char*)screen + 0x364, 12);          // visible centre in map coordinates
    void* area = *(void* const*)((const char*)screen + 0x2b0);   // map area data (zoom limits); required by the zoom
    if (!area || !isfinite(factor) || !isfinite(scale) || scale <= 0.0f || scale > 8.0f
        || !isfinite(centre[0]) || !isfinite(centre[1]) || !isfinite(centre[2])) return;
    mz_zoom(screen, centre, scale / factor);
    if (mz_logs < 12) { mz_logs++; LOGF("MAPZOOM pinch factor=%.3f scale %.3f -> %.3f", factor, scale,
                                       *(const float*)((const char*)screen + 0x370)); }
}
static void install_map_zoom(unsigned long base) {
    static const unsigned zoom[4] = {0xd10143ff, 0xf9001bf3, 0xa9047bfd, 0x910103fd};   // ZoomAroundMapCoord
    static const unsigned cls[2] = {0xd10183ff, 0xf90023f3};                             // GetPrivateStaticClass
    // Field proof inside ZoomAroundMapCoord: add x9,x0,#0x364 (centre); ldr x8,[x0,#0x2b0] (area); str s0,[x0,#0x370] (scale).
    static const unsigned centre = 0x910d9009, area = 0xf9415808, scale = 0xbd037000;
    if (memcmp((const void*)(base + 0x630c2bc), zoom, 16) || memcmp((const void*)(base + 0x6471928), cls, 8)
        || *(const unsigned*)(base + 0x630c2dc) != centre || *(const unsigned*)(base + 0x630c2e4) != area
        || *(const unsigned*)(base + 0x630c304) != scale) {
        LOGF("MAPZOOM disabled: opcode mismatch"); return;
    }
    mz_map_class = (void* (*)(void))(base + 0x6471928);
    mz_zoom = (void (*)(void*, const float*, float))(base + 0x630c2bc);
    LOGF("MAPZOOM ready: pinch drives AVR4MapUIScreen::ZoomAroundMapCoord");
}
#endif
