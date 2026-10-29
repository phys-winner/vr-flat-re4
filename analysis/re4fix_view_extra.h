// Gameplay field of view, aim zoom and hold-to-skip cutscenes. Include after the cinema, interact and mobile menu
// helpers; both entry points run inside UVR4CameraComponent::GetCameraView (hook_cinema_view) on the GameThread.
#ifndef RE4FIX_VIEW_EXTRA_H
#define RE4FIX_VIEW_EXTRA_H

static int (*vx_can_skip)(void);          // AVR4CutsceneManager::EventCanBeSkipped (static)
static void (*vx_skip)(void);             // AVR4CutscenePlayerPawn::SkipCutscene (static, finds its own pawn)
static int (*vx_scope)(const void*);      // AVR4Bio4PlayerPawn::IsAimingThroughScope
static void* (*vx_bio4_class)(void);      // AVR4Bio4PlayerPawn::GetPrivateStaticClass
static unsigned vx_skip_logs, vx_fov_logs;
static float vx_fov_scale = 1.0f;         // applied scale of the authored FOV (eased)
static long vx_fov_ms;
static int vx_intro_done,vx_intro_seen,vx_intro_active,vx_intro_repeat;
struct vx_velocity {float x,y,z;};
static struct vx_velocity (*vx_velocity)(const void*);
static struct weapon_bob_state vx_bob;
static unsigned vx_bob_logs;
static void* vx_bob_pawn;
static unsigned long long vx_bob_name;
static void touch_gameplay_bob(void* pawn,void* view,long now) {
    const unsigned char* settings=qs_settings ? qs_settings() : 0;
    const char* cls=pawn ? *(const char* const*)((const char*)pawn+0x10) : 0;
    int size=cls ? *(const int*)(cls+0x58) : 0;
    unsigned long long name=0;if(pawn)memcpy(&name,(const char*)pawn+0x18,8);
    if(pawn!=vx_bob_pawn || name!=vx_bob_name){memset(&vx_bob,0,sizeof vx_bob);vx_bob_pawn=pawn;vx_bob_name=name;}
    if(!vx_velocity || !settings || settings[0x3a]!=1 || size<0x1458 || size>0x10000){memset(&vx_bob,0,sizeof vx_bob);return;}
    struct vx_velocity velocity=vx_velocity(pawn);
    float speed=hypotf(velocity.x,velocity.y),offset[3],position[3],yaw;
    // Vertical velocity includes stairs and slopes; only horizontal travel drives the step phase.
    if(!isfinite(velocity.z))speed=NAN;
    weapon_bob_step(&vx_bob,now,speed,1,offset);
    memcpy(position,view,sizeof position);memcpy(&yaw,(const char*)view+0x10,4);
    if(!isfinite(yaw)||!isfinite(position[0])||!isfinite(position[1])||!isfinite(position[2]))return;
    yaw*=0.01745329252f;position[0]-=sinf(yaw)*offset[1];position[1]+=cosf(yaw)*offset[1];position[2]+=offset[2];
    memcpy(view,position,sizeof position);
    if(vx_bob_logs<6 && (fabsf(offset[1])>.01f || fabsf(offset[2])>.01f)) {
        ++vx_bob_logs;LOGF("VIEWBOB native speed=%.2f offset=%.4f,%.4f applied=%.3f,%.3f,%.3f",speed,offset[1],offset[2],position[0],position[1],position[2]);
    }
}

// Hold-to-skip: the original "Hold To Skip" Blueprint calls SkipCutscene once the cutscene manager allows it.
static void touch_cutscene_skip_tick(void) {
    if (!cinema_on_game_thread()) return;
    if (!vx_can_skip || !vx_skip || !cinema_type || !cinema_active || cinema_type()!=3 || !cinema_active()) {
        __atomic_store_n(&g_cutscene_skippable_ms,0,__ATOMIC_RELEASE);
        __atomic_store_n(&g_cutscene_skip_request,0,__ATOMIC_RELEASE);
        vx_intro_active=0;return;
    }
    long now = ptr_now_ms();
    void* theatre=cinema_movie_probe(cinema_active(),0);
    int opening=theatre && cinema_movie_probe(cinema_active(),1)==theatre;
    if (opening && !vx_intro_active) {
        vx_intro_repeat=vx_intro_seen;vx_intro_seen=1;vx_intro_done=0;
    }
    vx_intro_active=opening;
    int can = theatre || (vx_can_skip() & 1);
    __atomic_store_n(&g_cutscene_skippable_ms,can ? now : 0,__ATOMIC_RELEASE);
    long request = __atomic_exchange_n(&g_cutscene_skip_request, 0, __ATOMIC_ACQ_REL);
    int automatic=opening && !vx_intro_done && (vx_intro_repeat || mobile_prefs_intro_skip());
    if (!request && !automatic) return;
    now=ptr_now_ms();
    if (can && (automatic || (now >= request && now - request < 600))) {
        int skipped=theatre ? cinema_movie_complete(theatre) : (vx_skip(),1);
        if (opening && skipped) vx_intro_done=1;
        if (vx_skip_logs < 16) { vx_skip_logs++; LOGF("CUTSCENE skip movie=%d automaticIntro=%d accepted=%d",theatre!=0,automatic,skipped); }
    } else if (vx_skip_logs < 16) { vx_skip_logs++; LOGF("CUTSCENE skip ignored: skippable=%d age=%ld", can, now - request); }
}

// FOV: the authored gameplay camera is 90 degrees horizontal (MaintainXFOV, mono projection from the view info).
// The phone option scales it; aiming narrows it slightly unless the view is already through a weapon scope.
static void touch_gameplay_fov_view(void* camera, void* view) {
    if (!view || !camera || !cinema_type || !cinema_active || !cinema_on_game_thread()) return;
    if (cinema_type() != 1) { vx_fov_scale = 1.0f; vx_fov_ms = 0; g_fov_h_deg = 90.0f; memset(&vx_bob,0,sizeof vx_bob); return; }
    void* pawn = cinema_active();
    if (!pawn || !vx_bio4_class || !interact_is_a(pawn, vx_bio4_class())
        || *(void* const*)((const char*)pawn + 0x2d8) != camera) return;   // only the player's own camera
    float authored;
    memcpy(&authored, (const char*)view + 0x18, 4);
    if (!isfinite(authored) || authored < 20.0f || authored > 150.0f) return;
    // Bio4 stays active for world dialogs and some pauses; those views keep the native projection.
    if (g_touch_context != 1 || menu_mode() || !rl_paused || !rl_player_valid || rl_paused() || !rl_player_valid()) {
        vx_fov_scale = 1.0f; vx_fov_ms = 0; g_fov_h_deg = authored; memset(&vx_bob,0,sizeof vx_bob); return;
    }
    long now = ptr_now_ms();
    touch_gameplay_bob(pawn,view,now);
    long dt = vx_fov_ms ? now - vx_fov_ms : 16;
    vx_fov_ms = now;
    if (dt < 0 || dt > 200) dt = 16;
    float target = (float)mobile_prefs_fov() / 90.0f;
    int aiming = g_touch_aim && g_touch_weapon == 2 && g_touch_context == 1 && !menu_mode();
    if (aiming && mobile_prefs_ads_zoom() && !(vx_scope && vx_scope(pawn))) target *= 0.84f;
    float k = (float)dt / 90.0f;
    vx_fov_scale += (target - vx_fov_scale) * (k > 1.0f ? 1.0f : k);
    float fov = authored * vx_fov_scale;
    if (!isfinite(fov) || fov < 20.0f || fov > 150.0f) return;
    memcpy((char*)view + 0x18, &fov, 4);
    g_fov_h_deg = fov;   // menu pointer rays and projected tap targets use the live gameplay FOV
    if (vx_fov_logs < 4 && fabsf(fov - authored) > 0.5f) {
        vx_fov_logs++;
        LOGF("FOV gameplay authored=%.1f applied=%.1f aiming=%d", authored, fov, aiming);
    }
}

static void touch_view_extra(void* camera, void* view) {
    touch_cutscene_skip_tick();
    touch_gameplay_fov_view(camera, view);
}
static void install_view_extra(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[4]; } check[] = {
        {0x62419d4, {0xd100c3ff, 0xf9000bf3, 0xa9027bfd, 0x910083fd}},   // AVR4CutsceneManager::EventCanBeSkipped
        {0x6245300, {0xf81e0ff3, 0xa9017bfd, 0x910043fd, 0x52800060}},   // AVR4CutscenePlayerPawn::SkipCutscene
        {0x62196e8, {0xd10143ff, 0xa90257f6, 0xa9034ff4, 0xa9047bfd}},   // AVR4Bio4PlayerPawn::IsAimingThroughScope
        {0x63c8a5c, {0xd10183ff, 0xf90023f3, 0x0, 0x0}},                 // AVR4Bio4PlayerPawn::GetPrivateStaticClass
    };
    for (unsigned i = 0; i < sizeof check / sizeof *check; i++) {
        unsigned words = check[i].first[2] ? 4 : 2;
        if (memcmp((const void*)(base + check[i].addr), check[i].first, words * 4)) {
            LOGF("VIEWEXTRA disabled: opcode mismatch +%lx", check[i].addr); return;
        }
    }
    vx_can_skip = (int (*)(void))(base + 0x62419d4);
    vx_skip = (void (*)(void))(base + 0x6245300);
    vx_scope = (int (*)(const void*))(base + 0x62196e8);
    vx_bio4_class = (void* (*)(void))(base + 0x63c8a5c);
    static const unsigned velocity_code[9]={0x52828988,0x8b080008,0xfd4a2000,0xfd400102,0xbd544801,
        0xbd400903,0x0e22d400,0x1e232822,0x5e0c0401};
    if(!memcmp((const void*)(base+0x6219800),velocity_code,sizeof velocity_code)
        && *(const unsigned*)(base+0x63c8ae4)==0x5282ca04)
        vx_velocity=(struct vx_velocity (*)(const void*))(base+0x6219800);
    cinema_view_extra = touch_view_extra;
    LOGF("VIEWEXTRA ready: gameplay FOV option, aim zoom, hold-to-skip cutscenes nativeMovementBob=%d",vx_velocity!=0);
}
#endif
