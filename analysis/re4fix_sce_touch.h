// Touch operation of the VR-only levers and cranks (UVR4LeverComponent / UVR4CrankComponent : UVR4SceInteraction).
// In VR both are grabbed and moved by hand (Blueprint reads the hand); a phone has no hand motion, so the original
// completion functions are called instead:
//  * lever: UVR4LeverComponent::ActivateSwitch - toggles the original cEmSwitch (or the lever state) and runs
//    UVR4SceInteraction::ActivateEvent; the native lever tick then rotates the joint to its activated rest pose.
//  * crank: OnGrab, then AddDeltaAngleDegrees(delta, deltaTime) every frame - the original code moves the linked
//    objects, plays the crank sounds and runs ActivateEvent at the required angle - then OnRelease.
// Only enabled interactions are offered (the game enables them after its own prompt/story conditions).
// Include after world_touch (projection helpers) and interact.
#ifndef RE4FIX_SCE_TOUCH_H
#define RE4FIX_SCE_TOUCH_H

#define SCE_MAX 128
static void* sce_list[SCE_MAX];
static int sce_count;
static pthread_mutex_t sce_mx = PTHREAD_MUTEX_INITIALIZER;
static void* (*sce_lever_class)(void), *(*sce_crank_class)(void);
static void* (*sce_model_class)(void), *(*sce_base_class)(void);
static void (*sce_activate_switch)(void*);
static void (*sce_crank_add)(void*, float, float);
static void (*sce_crank_grab)(void*), (*sce_crank_release)(void*);
static float (*sce_crank_ratio)(const void*);
static int sce_ready;
static unsigned sce_logs;
// GameThread-owned candidate and running crank.
static void* sce_best; static int sce_best_kind; static long sce_best_ms; static float sce_best_sx, sce_best_sy;
static void* sce_crank; static long sce_crank_last, sce_crank_started;
// HUD snapshot: (ms << 32) | kind << 28 (1 lever, 2 crank) | running << 27 | progress permille.
static volatile unsigned long long g_sce_ui;
#define SCE_CRANK_DEG_PER_S 240.0f
unsigned long g_tgt_sce_begin, g_tgt_sce_end;

__attribute__((naked)) static void gate_sce_begin(void* self) {
    asm volatile(".inst 0xf81e0ff3\n" ".inst 0xa9017bfd\n" ".inst 0x910043fd\n" ".inst 0xaa0003f3\n"
        "adrp x16, :got:g_tgt_sce_begin\n" "ldr x16, [x16, :got_lo12:g_tgt_sce_begin]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static void gate_sce_end(void* self, int reason) {
    asm volatile(".inst 0xf81e0ff3\n" ".inst 0xa9017bfd\n" ".inst 0x910043fd\n" ".inst 0xaa0003f3\n"
        "adrp x16, :got:g_tgt_sce_end\n" "ldr x16, [x16, :got_lo12:g_tgt_sce_end]\n" "ldr x16, [x16]\n" "br x16\n");
}
void hook_sce_begin(void* self) {
    gate_sce_begin(self);
    pthread_mutex_lock(&sce_mx);
    int known = 0;
    for (int i = 0; i < sce_count; i++) if (sce_list[i] == self) known = 1;
    if (!known && sce_count < SCE_MAX) sce_list[sce_count++] = self;
    pthread_mutex_unlock(&sce_mx);
}
void hook_sce_end(void* self, int reason) {
    pthread_mutex_lock(&sce_mx);
    for (int i = 0; i < sce_count; i++) if (sce_list[i] == self) { sce_list[i] = sce_list[--sce_count]; break; }
    pthread_mutex_unlock(&sce_mx);
    if (sce_crank == self) sce_crank = 0;   // EndPlay runs on the GameThread: never touch it after this
    if (sce_best == self) sce_best = 0;
    gate_sce_end(self, reason);
}
// This authored two-ended lever has a second SCE receiver. Its original grip
// Blueprint chooses the endpoint from hand position; the one-action shortcut
// would always activate the primary receiver and prevent choosing the other end.
static int sce_bidirectional(const void* c) {
    if(!phone_inspect_ready || !sce_model_class || !sce_base_class)return 0;
    void* owner=*(void* const*)((const char*)c+0xa0);
    if(!pv_alive(owner) || !tw_typed(owner,sce_model_class(),0x220)
        || *(void* const*)((const char*)c+0x20)!=owner)return 0;
    const char* cls=*(const char* const*)((const char*)owner+0x10);int family=0;
    int bytes=*(const int*)(cls+0x58);
    for(unsigned depth=0;cls && depth<16;depth++) {
        char name[96];phone_inspect_fname(cls+0x18,name,sizeof name);
        if(!strcmp(name,"BidirectionalLever_StaticModel_BP1_C")){family=1;break;}
        const char* next=*(const char* const*)(cls+0x40);if(next==cls)break;cls=next;
    }
    if(!family)return 0;
    const char* fields[2]={cinema_field_property(owner,"VR4Lever","ObjectProperty"),
        cinema_field_property(owner,"SecondarySceInteraction","ObjectProperty")};
    void* objects[2];
    for(int i=0;i<2;i++) {
        const char* field=fields[i];int off=field ? *(const int*)(field+0x4c):-1;
        if(!field || *(const int*)(field+0x34)!=1 || *(const int*)(field+0x38)!=8
            || off<0x28 || off>bytes-8)return 0;
        objects[i]=*(void* const*)((const char*)owner+off);
    }
    void* secondary=objects[1];
    return objects[0]==c && secondary!=c && pv_alive(secondary)
        && tw_typed(secondary,sce_base_class(),0x260)
        && *(void* const*)((const char*)secondary+0x10)==sce_base_class()
        && *(void* const*)((const char*)secondary+0x20)==owner
        && *(void* const*)((const char*)secondary+0xa0)==owner;
}
// 1 lever, 2 crank, 0 not operable now (or operated through its original world grip).
static int sce_kind(const void* c) {
    if (!pv_alive(c)) return 0;
    int kind = tw_typed(c, sce_lever_class(), 0x384) ? 1 : tw_typed(c, sce_crank_class(), 0x350) ? 2 : 0;
    if (!kind) return 0;
    if(kind==1 && sce_bidirectional(c))return 0;
    const unsigned char* b = (const unsigned char*)c;
    // bEnabled (IsEnabled), not finished (+0x259), not activated (+0x25a); crank: not completed (+0x334).
    if (b[0x258] != 1 || b[0x259] || b[0x25a] || (kind == 2 && b[0x334])) return 0;
    return kind;
}
static int sce_location(const void* c, float out[3]) {
    void* owner = *(void* const*)((const char*)c + 0xa0);
    void* root = owner && pv_alive(owner) ? *(void* const*)((const char*)owner + 0x130) : 0;
    return root && pv_alive(root) && wt_component_location(root, out);
}
static void sce_crank_stop(const char* why) {
    if (!sce_crank) return;
    if (pv_alive(sce_crank)) sce_crank_release(sce_crank);
    if (sce_logs < 40) { sce_logs++; LOGF("SCE crank stop (%s) crank=%p", why, sce_crank); }
    sce_crank = 0;
}
static int sce_operate(void* c, int kind, long now, const char* source) {
    if (!c || kind != sce_kind(c)) return 0;
    if (kind == 1) {
        sce_activate_switch(c);
        if (sce_logs < 40) { sce_logs++; LOGF("SCE lever ActivateSwitch (%s) lever=%p activated=%u", source, c,
                                              *(const unsigned char*)((const char*)c + 0x25a)); }
        return 1;
    }
    if (sce_crank == c) { sce_crank_stop(source); return 1; }   // a second press lets go
    sce_crank_stop("switch");
    sce_crank_grab(c);
    sce_crank = c; sce_crank_last = now; sce_crank_started = now;
    if (sce_logs < 40) { sce_logs++; LOGF("SCE crank start (%s) crank=%p ratio=%.2f", source, c, sce_crank_ratio(c)); }
    return 1;
}
// GameThread, right active game hand tick: choose the operable lever/crank in front of the camera, run the crank.
static void touch_sce_tick(void* pawn, long now) {
    if (!sce_ready || !pawn || g_touch_context != 1 || g_touch_cancel || !aim_head || (rl_paused && rl_paused())) {
        sce_crank_stop("context"); sce_best = 0; __atomic_store_n(&g_sce_ui, 0, __ATOMIC_RELEASE); return;
    }
    struct reload_tf camera = aim_head(pawn);
    if (!reload_tf_valid(&camera)) return;
    camera.q = q_mul(camera.q, pose_camera_delta_ue());
    float forward[3]; q_rot(camera.q, (float[3]){1, 0, 0}, forward);
    void* best = 0; int best_kind = 0; float best_score = 1e9f, sx = 0, sy = 0;
    pthread_mutex_lock(&sce_mx);
    if (__atomic_exchange_n(&g_sce_diag_request, 0, __ATOMIC_ACQ_REL)) {   // dev console "sce"
        for (int i = 0; i < sce_count; i++) {
            void* c = sce_list[i];
            int alive = pv_alive(c), lever = alive && tw_typed(c, sce_lever_class(), 0x384), crank = alive && tw_typed(c, sce_crank_class(), 0x350);
            float world[3] = {0, 0, 0}; char owner[80] = "";
            if (alive) { sce_location(c, world); obj_name(*(void* const*)((const char*)c + 0xa0), owner, sizeof owner); }
            const unsigned char* b = (const unsigned char*)c;
            LOGF("SCE #%d %p lever=%d crank=%d owner='%s' enabled=%u done=%u activated=%u at=%.0f,%.0f,%.0f cam=%.0f,%.0f,%.0f",
                 i, c, lever, crank, owner, alive ? b[0x258] : 0, alive ? b[0x259] : 0, alive ? b[0x25a] : 0,
                 world[0], world[1], world[2], camera.p[0], camera.p[1], camera.p[2]);
        }
    }
    for (int i = 0; i < sce_count; i++) {
        void* c = sce_list[i];
        int kind = sce_kind(c);
        float world[3];
        if (!kind || !sce_location(c, world)) continue;
        float d[3] = {world[0] - camera.p[0], world[1] - camera.p[1], world[2] - camera.p[2]};
        float dist = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (!isfinite(dist) || dist > 230.0f || dist < 1.0f) continue;
        float facing = (d[0] * forward[0] + d[1] * forward[1] + d[2] * forward[2]) / dist;
        if (facing < 0.45f) continue;                       // within ~63 degrees of the view direction
        float score = dist * (2.0f - facing), px, py, depth;
        if (score < best_score && wt_project(&camera, world, &px, &py, &depth)) {
            best = c; best_kind = kind; best_score = score; sx = px; sy = py;
        }
    }
    pthread_mutex_unlock(&sce_mx);
    sce_best = best; sce_best_kind = best_kind; sce_best_ms = now; sce_best_sx = sx; sce_best_sy = sy;
    float progress = 0;
    if (sce_crank) {
        float world[3];
        int alive = sce_kind(sce_crank) == 2 && sce_location(sce_crank, world);
        float d2 = alive ? (world[0] - camera.p[0]) * (world[0] - camera.p[0]) + (world[1] - camera.p[1]) * (world[1] - camera.p[1])
                           + (world[2] - camera.p[2]) * (world[2] - camera.p[2]) : 0;
        if (!alive) {
            // Completion (ActivateEvent) or a story change disabled it: release once.
            sce_crank_stop(pv_alive(sce_crank) ? "completed/disabled" : "gone");
        } else if (d2 > 280.0f * 280.0f) sce_crank_stop("walked away");
        else {
            long dt = now - sce_crank_last;
            if (dt < 0) dt = 0; if (dt > 50) dt = 50;
            sce_crank_last = now;
            if (dt) sce_crank_add(sce_crank, SCE_CRANK_DEG_PER_S * (float)dt * 0.001f, (float)dt * 0.001f);
            progress = sce_crank && pv_alive(sce_crank) ? sce_crank_ratio(sce_crank) : 1.0f;
            if (!isfinite(progress) || progress < 0) progress = 0;
            if (progress > 1) progress = 1;
        }
    }
    int kind = sce_crank ? 2 : best_kind;
    unsigned long long ui = kind ? ((unsigned long long)(unsigned)now << 32) | ((unsigned long long)kind << 28)
        | ((unsigned long long)(sce_crank != 0) << 27) | (unsigned)(progress * 1000.0f) : 0;
    __atomic_store_n(&g_sce_ui, ui, __ATOMIC_RELEASE);
}
// GameThread (menu hand tick): a pause/inventory/phone screen lets go of a running crank.
static void touch_sce_idle_tick(void) {
    if (sce_crank && g_touch_context != 1) sce_crank_stop("menu");
}
// GameThread (interact helper, after native prompts): the interact icon operates the current candidate/crank.
static int touch_sce_interact(void* pawn, long now) {
    (void)pawn;
    if (!sce_ready) return 0;
    if (sce_crank) { sce_crank_stop("interact"); return 1; }
    if (!sce_best || now - sce_best_ms > 250) return 0;
    return sce_operate(sce_best, sce_best_kind, now, "interact");
}
// GameThread (world tap): a short tap on the projected lever/crank operates it.
static int touch_sce_tap(float x, float y, long now) {
    if (!sce_ready || !sce_best || now - sce_best_ms > 250) return 0;
    float limit = 0.14f * (g_comp_h ? (float)g_comp_h : 1080.0f);
    float dx = x - sce_best_sx, dy = y - sce_best_sy;
    if (dx * dx + dy * dy > limit * limit) return 0;
    return sce_operate(sce_best, sce_best_kind, now, "tap");
}
// Input/render thread: HUD state (icon swap, crank progress ring).
static int touch_sce_state(long now, int* kind, int* running, float* progress) {
    unsigned long long ui = __atomic_load_n(&g_sce_ui, __ATOMIC_ACQUIRE);
    if (!ui || (uint32_t)((uint32_t)now - (uint32_t)(ui >> 32)) > 300) return 0;
    *kind = (int)((ui >> 28) & 3); *running = (int)((ui >> 27) & 1); *progress = (float)(ui & 0x7ff) / 1000.0f;
    return *kind != 0;
}
static void install_sce_touch(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[4]; } check[] = {
        {0x63594d4, {0xf81e0ff3, 0xa9017bfd, 0x910043fd, 0xaa0003f3}},   // UVR4SceInteraction::BeginPlay
        {0x63594f8, {0xf81e0ff3, 0xa9017bfd, 0x910043fd, 0xaa0003f3}},   // UVR4SceInteraction::EndPlay
        {0x6302390, {0xa9be4ff4, 0xa9017bfd, 0x910043fd, 0x39496808}},   // UVR4LeverComponent::ActivateSwitch
        {0x6238fc0, {0xd10343ff, 0xfd0033ea, 0x6d0723e9, 0xa90867fa}},   // UVR4CrankComponent::AddDeltaAngleDegrees
        {0x6238f48, {0xb9428808, 0x3100051f, 0x54000180, 0xb0020949}},   // UVR4CrankComponent::OnGrab
        {0x6238f84, {0xb9428808, 0x3100051f, 0x54000180, 0xb0020949}},   // UVR4CrankComponent::OnRelease
        {0x6239324, {0xd10103ff, 0xfd000be8, 0xa9024ff4, 0xa9037bfd}},   // UVR4CrankComponent::GetCompletionRatio
        {0x6469b0c, {0xd10183ff, 0xf90023f3, 0, 0}},                     // UVR4LeverComponent::GetPrivateStaticClass
        {0x63dedb8, {0xd10183ff, 0xf90023f3, 0, 0}},                     // UVR4CrankComponent::GetPrivateStaticClass
        {0x64d205c, {0xd10183ff, 0xf90023f3, 0, 0}},                     // AVR4StaticModel::GetPrivateStaticClass
        {0x64c55c4, {0xd10183ff, 0xf90023f3, 0, 0}},                     // UVR4SceInteraction::GetPrivateStaticClass
    };
    sce_ready = 0;
    if (!base) return;
    for (unsigned i = 0; i < sizeof check / sizeof *check; i++) {
        unsigned words = check[i].first[2] || check[i].first[3] ? 4 : 2;
        if (memcmp((const void*)(base + check[i].addr), check[i].first, words * 4)) {
            LOGF("SCE disabled: opcode mismatch +%lx", check[i].addr); return;
        }
    }
    sce_lever_class = (void* (*)(void))(base + 0x6469b0c);
    sce_crank_class = (void* (*)(void))(base + 0x63dedb8);
    sce_model_class = (void* (*)(void))(base + 0x64d205c);
    sce_base_class = (void* (*)(void))(base + 0x64c55c4);
    sce_activate_switch = (void (*)(void*))(base + 0x6302390);
    sce_crank_add = (void (*)(void*, float, float))(base + 0x6238fc0);
    sce_crank_grab = (void (*)(void*))(base + 0x6238f48);
    sce_crank_release = (void (*)(void*))(base + 0x6238f84);
    sce_crank_ratio = (float (*)(const void*))(base + 0x6239324);
    g_tgt_sce_begin = base + 0x63594d4 + 16;
    g_tgt_sce_end = base + 0x63594f8 + 16;
    if (patch_code((void*)(base + 0x63594d4), (void*)&hook_sce_begin) || patch_code((void*)(base + 0x63594f8), (void*)&hook_sce_end)) {
        LOGF("SCE disabled: hook installation failed"); return;
    }
    sce_ready = 1;
    interact_sce_hook = touch_sce_interact;
    LOGF("SCE ready: touch levers (ActivateSwitch) and cranks (AddDeltaAngleDegrees)");
}
#endif
