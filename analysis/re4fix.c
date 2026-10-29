// libre4fix.so - RE4VR flat port helper (stable).
// dlsym GOT hook in libUE4 + Vulkan call/extension logging + shader platform forcing.
#define LOG_TAG "RE4FIX-N"
#include <android/log.h>
#include <dlfcn.h>
#include <link.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <unwind.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#include <stdarg.h>
static volatile int g_flog_fd = -2;
static long g_flog_t0;
static void flog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void flog(const char* fmt, ...) {
    char b[700];
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    long ms = ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
    if (!g_flog_t0) g_flog_t0 = ms;
    int o = snprintf(b, sizeof b, "%6ld.%03ld [%d] ", (ms - g_flog_t0) / 1000, (ms - g_flog_t0) % 1000, gettid());
    va_list ap, android_ap; va_start(ap, fmt);
    va_copy(android_ap, ap);
    __android_log_vprint(ANDROID_LOG_INFO, LOG_TAG, fmt, android_ap);
    va_end(android_ap);
    int n = vsnprintf(b + o, sizeof b - o - 2, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    n += o; if (n > (int)sizeof b - 2) n = (int)sizeof b - 2;
    b[n++] = '\n';
    if (g_flog_fd == -2) {
        int fd = open("/data/data/com.Armature.VR4/re4fix.log", O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_CLOEXEC, 0644);
        if (!__sync_bool_compare_and_swap(&g_flog_fd, -2, fd) && fd >= 0) close(fd);
    }
    if (g_flog_fd >= 0) { ssize_t w = write(g_flog_fd, b, (size_t)n); (void)w; }
}
#define LOGF(...) flog(__VA_ARGS__)
#include "re4fix_mobile_text.h"
static volatile long g_in_attach, g_in_get, g_in_pre, g_in_act, g_in_motion, g_tq_calls;
static long g_tn;
static volatile float g_look_yaw, g_look_pitch;   // radians; +yaw = look left, +pitch = look up (OVR frame)
static volatile long g_lobby_menu_ms;             // GameThread Lobby front-end confirmation (extras tick)
static volatile float g_menu_yaw_off, g_menu_pitch;   // in-world menu framing (re4fix_flat_view.h)
static volatile int g_menu_view_active;
static volatile long g_menu_view_ms;
static volatile long g_menu_tap_ms;   // last menu pointer tap (input thread)
static volatile long g_heal_holster_ms;   // GameThread: the healing item sits in a reachable holster (no QuickSelect)
static volatile long g_watch_since_ms, g_watch_until_ms;   // hold the health dial: left wrist raised to the watch
static volatile int g_mm_row_trace;
// In-world UI screens shown during gameplay (range console, quit prompt): GameThread publishes the projected
// screen position of every enabled control; a tap near one clicks it natively instead of a laser pointer pulse.
struct ui_target { float x, y; void* component; };
static struct ui_target g_ui_targets[16];
static int g_ui_target_count;
static long g_ui_targets_ms;
static pthread_mutex_t g_ui_target_mx = PTHREAD_MUTEX_INITIALIZER;
static void* volatile g_ui_direct_click;
static volatile long g_pose_calls[12];
static volatile float g_ptr_u = 0.5f, g_ptr_v = 0.5f;   // pointer target in normalised surface coords
static volatile int g_ptr_down;                         // right-half touches currently down
static volatile long g_ptr_until_ms;                    // pointer mode lingers after release
static float g_fov_h_deg = 90.0f;                       // horizontal camera FOV (UE default, MaintainXFOV)
static int ptr_active(void);
static long ptr_now_ms(void);
static volatile long g_menu_tick_ms;                    // last AVR4MenuPlayerHand::Tick (CLOCK_MONOTONIC ms)
static volatile long g_menu_ticks;
// dev/scripted control: command-file buttons, hand overrides and hand animations
static volatile long g_cmd_until[32];                   // per ovrpButton bit: held until (ms)
static volatile float g_cmd_lstick[2]; static volatile long g_cmd_lstick_until;
static volatile float g_cmd_rstick[2]; static volatile long g_cmd_rstick_until;
static volatile int g_hand_ovr_on[2]; static volatile float g_hand_ovr[2][3];   // head-relative hand offset override
static volatile int g_anim_kind; static volatile long g_anim_t0, g_anim_len;      // 1 = knife swing (right hand)
static volatile float g_anim_amp = 0.40f, g_anim_period = 700.0f;
static volatile int g_touch_weapon_request, g_touch_weapon, g_touch_pause, g_touch_cancel;
static volatile int g_touch_reload_request, g_reload_phase;
static volatile int g_touch_inventory_request, g_touch_interact_request, g_touch_aim;
static volatile int g_touch_back_request;
static int mobile_prefs_aim_walk(void);
static int mobile_prefs_intro_skip(void);
static int mobile_prefs_start_confirm(void);
static int mobile_prefs_hide_gamepad_hints(void);
static int mobile_prefs_weapon_fov(void);
static volatile int g_pawn_diag_request;
static volatile int g_touch_context, g_phone_request;
static volatile long g_phone_zoom_start;
static volatile int g_touch_gt_tid;
static volatile int g_aim_diag_request;
static volatile int g_prompt_visual_diag_request;
static volatile int g_qs_diag_request;
static unsigned long long g_game_tick_side[2],g_inactive_tick_side[2];
static volatile long g_knife_diag_until;
// Input thread -> GameThread requests and GameThread -> HUD/input state (ms timestamps).
static volatile long g_cutscene_skip_request;    // hold-to-skip finished on the touch screen
static volatile long g_cutscene_skippable_ms;    // last frame the active cutscene event accepted a skip
static volatile long g_aim_painted_ms;           // the held gun's laser sight painted an enemy (aim assist)
static volatile long g_qturn_request_ms;         // double tap / gamepad: quick 180-degree turn
static void touch_cutscene_skip_tick(void);
static void touch_heal_progress_tick(void* hand,long now);
static void touch_gameplay_fov_view(void* camera, void* view);
static int touch_sce_interact(void* pawn, long now);
static int touch_sce_tap(float x, float y, long now);
static void touch_sce_idle_tick(void);
static void touch_mirror_idle_tick(void);
static void touch_tutorial_tick(long now);
static void touch_throw_idle_tick(void);
static void touch_gamepad_log(void);
static volatile int g_sce_diag_request;
static void touch_map_zoom_tick(long now);
static volatile unsigned g_grip_latched;
static volatile long g_weapon_grip_at;
static void touch_pause_tick(void* hand);
static void touch_unpause_tick(void* hand);
static void touch_phone_tick(void* hand);
static void phone_view_inspect(void* pawn,void* device);
static void* cinema_subtitle_probe(void* pawn);
static int touch_menu_active(void* hand);
static void touch_menu_visual_tick(void* hand);
static void touch_menu_grip_tick(void* hand);
static void touch_menu_grip_cancel(void* hand);
static void touch_new_game_pawn(void* pawn);
static void touch_log_state(void);
static int touch_reload_user_fire(long now);
static void* touch_equipped_gun(void* hand);
static int touch_holster_hover_allowed(void* holster, void* hand, long now);
static float knife_cut_offset(long elapsed);
static void touch_qte_pose(int side,long now,float off[3]);
static int touch_grenade_pose(int side,long now,float off[3]);
static void touch_throw_pose(int side,long now,float off[3]);
static int touch_mirror_pose(int side,long now,float position[3]);
static void touch_weapon_cycle_pose(int side,long now,float off[3]);
static int touch_grenade_busy(long now);
static unsigned touch_grenade_owned_grip_mask(void);
static void touch_grenade_cancel(void);
static void touch_grenade_cancel_tick(void* hand,int side,long now);
static void touch_mobile_settings_tick(void);
static void touch_flat_controls_tick(void);
static void touch_mobile_android_tick(void);
static void touch_menu_view_tick(long now);
static volatile int g_project_diag_request;
static volatile int g_ui_dump_request, g_ui_click_request;
static char g_ui_click_pat[48];
static void touch_ui_diag_tick(void);
static void touch_comfort_wizard_tick(long now);
static void touch_inspect_tick(void);
static void touch_ui_targets_tick(long now);
static void touch_mobile_menu_tick(void);
static void touch_mobile_extras_tick(long now);
static void touch_extras_save_tick(long now);
static int touch_mobile_back(void);
static void touch_prompt_visual_tick(void);
static void touch_prompt_visual_diag(void);
static int touch_weapon_primary_side(long now);
static int touch_grenade_selected_side(long now);
static void apply_touch_graphics(unsigned long base);
void graphics_audit_device(const void* phys, const void* ci, int result);
void graphics_audit_sampler(const void* ci, int result);
static int (*r_touch_CreateSampler)(void*, const void*, const void*, void**);
int h_touch_CreateSampler(void* dev, const void* ci, const void* ac, void** out) {
    int r = r_touch_CreateSampler(dev, ci, ac, out);
    graphics_audit_sampler(ci, r);
    return r;
}
static char g_force_pat[48]; static volatile long g_force_until;                 // force holster hover (name substring)
static void* g_mobile_holster, *g_mobile_holster_hand;
static volatile long g_mobile_holster_until;

typedef struct {
    unsigned char e_ident[16];
    unsigned short e_type, e_machine;
    unsigned int e_version;
    unsigned long e_entry, e_phoff, e_shoff;
    unsigned int e_flags, e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} E_;
typedef struct { unsigned int p_type, p_flags; unsigned long p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align; } P_;
typedef struct { unsigned long d_tag, d_val; } D_;
typedef struct { unsigned long r_offset, r_info; long r_addend; } R_;
typedef struct { unsigned int st_name; unsigned char st_info, st_other; unsigned short st_shndx; unsigned long st_value, st_size; } S_;

static int phdr_cb(struct dl_phdr_info* info, size_t sz, void* data) {
    if (info->dlpi_name && strstr(info->dlpi_name, "libUE4.so")) {
        if (info->dlpi_addr > 0x7000000000ULL && *(unsigned int*)info->dlpi_addr == 0x464C457F) {
            *(unsigned long*)data = info->dlpi_addr;
            return 1;
        }
    }
    return 0;
}
static unsigned long ue4_base() {
    for (int i = 0; i < 1200; i++) {
        unsigned long b = 0;
        if (dl_iterate_phdr(phdr_cb, &b)) return b;
        usleep(50000);
    }
    return 0;
}


static int write_mem(void* addr, void* val) {
    int fd = open("/proc/self/mem", O_RDWR);
    if (fd < 0) return -1;
    int ok = 0;
    if (lseek64(fd, (off64_t)(unsigned long)addr, SEEK_SET) >= 0)
        ok = (write(fd, &val, sizeof(val)) == sizeof(val));
    close(fd);
    return ok ? 0 : -2;
}

static unsigned long find_got(unsigned long base, const char* want) {
    E_* eh = (E_*)base;
    unsigned long rela = 0, relasz = 0, jmprel = 0, pltrelsz = 0, symtab = 0, strtab = 0;
    for (int i = 0; i < eh->e_phnum; i++) {
        P_* ph = (P_*)((char*)eh + eh->e_phoff) + i;
        if (ph->p_type == 2) {
            D_* dyn = (D_*)(base + ph->p_vaddr);
            for (D_* d = dyn; d->d_tag; d++) {
                if (d->d_tag == 7) rela = d->d_val;
                else if (d->d_tag == 8) relasz = d->d_val;
                else if (d->d_tag == 23) jmprel = d->d_val;
                else if (d->d_tag == 2) pltrelsz = d->d_val;
                else if (d->d_tag == 6) symtab = d->d_val;
                else if (d->d_tag == 5) strtab = d->d_val;
            }
            break;
        }
    }
    if (!symtab || !strtab) return 0;
    struct { unsigned long ptr, sz; } tabs[2] = {{rela, relasz}, {jmprel, pltrelsz}};
    for (int t = 0; t < 2; t++) {
        if (!tabs[t].ptr || !tabs[t].sz) continue;
        R_* r = (R_*)(base + tabs[t].ptr);
        for (unsigned long i = 0; i < tabs[t].sz / sizeof(R_); i++) {
            unsigned sym = r[i].r_info >> 32;
            if (!sym) continue;
            const char* nm = (const char*)(base + strtab + ((S_*)(base + symtab))[sym].st_name);
            if (nm[0] == 'd' && strcmp(nm, want) == 0)
                return base + r[i].r_offset;
        }
    }
    return 0;
}


static int protect_mem(void* target, size_t size, int protection) {
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0 || (page_size & (page_size - 1)) || !size) return -1;
    uintptr_t mask = (uintptr_t)page_size - 1, address = (uintptr_t)target;
    if (size > SIZE_MAX - (address & mask) - mask) return -1;
    size_t span = ((address & mask) + size + mask) & ~mask;
    return mprotect((void*)(address & ~mask), span, protection);
}

static int patch_code(void* target, void* hook) {
    if (protect_mem(target, 16, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) return -1;
    unsigned int insns[2] = {0x58000051, 0xD61F0220}; // ldr x17,[pc,#8]; br x17
    memcpy(target, insns, 8);
    memcpy((char*)target + 8, &hook, 8);
    __builtin___clear_cache((char*)target, (char*)target + 16);
    return 0;
}

unsigned long g_ue4_base_cached;
#include "re4fix_packaged_obb.h"
#include "re4fix_mobile_android.h"
#include "re4fix_loading.h"
static unsigned fv_title_action_mask(long now,unsigned type);
static unsigned (*phone_type)(void);
#include "re4fix_haptics.h"

static void* (*r_dlsym)(void*, const char*);
static void (*r_vkCmdDraw)(void*, unsigned, unsigned, unsigned, unsigned);
static void (*r_vkCmdDrawIdx)(void*, unsigned, unsigned, unsigned, int, unsigned);

static void* (*r_vkci)(const void*, const void*, void**);
struct VkSubmitInfoShim {
    unsigned sType; unsigned pad;
    const void* pNext;
    unsigned waitSemaphoreCount;
    const void* pWaitSemaphores;
    const void* pWaitDstStageMask;
    unsigned signalSemaphoreCount;
    const void* pSignalSemaphores;
    unsigned commandBufferCount;
    const void* pCommandBuffers;
};
int h_vkAcquire6(void* dev, void* sc, long long t, void* sem, void* fence, void* img);
int h_vkPresent(void* q, void* si, void* f);
void* h_vkGetInstanceProcAddr(void* inst, const char* name);
void* h_vkGetDeviceProcAddr(void* dev, const char* name);
int h_vkQueueSubmit(void* q, unsigned n, const void* infos, void* fence);
void h_vkCmdBeginRenderPass(void* cmd, const void* bi, unsigned c);
int h_vkCreateSwapchain(void* dev, const void* ci, const void* ac, void** out);
int h_vkGetSCI(void* dev, void* sc, unsigned* pcount, void** imgs);
void h_vkCmdDraw(void* c, unsigned vc, unsigned inst, unsigned fv, unsigned fi);
void h_vkCmdDrawIdx(void* c, unsigned ic, unsigned inst, unsigned first, int voff, unsigned finst);
int h_vkCreateDevice(const void* phys, const void* ci, const void* ac, void** out);
void h_vkCmdClearColorImage(void* cmd, void* img, int layout, const void* col, unsigned n, const void* ranges);
int h_vkCreateImageView(void* dev, const void* ci, const void* ac, void** out);
int h_vkCreateSwapchain(void* dev, const void* ci, const void* ac, void** out);
int w_GetSysInt(int status);
float w_GetSysFloat(int status);
int w_GetPropInt(void* ctx, int prop, int def);
float w_GetPropFloat(void* ctx, int prop, float def);
void w_WaitFrame(void* ctx, double t);
double w_GetPredTime(void* ctx, double t);
void w_SubmitFrame2(void* ctx, const void* desc);
void h_ovrpEndFrame_stub(void);

static void log_ext_list(const char* tag, const char* const* names, unsigned int count) {
    for (unsigned int i = 0; i < count && i < 40; i++)
        LOGI("  %s[%d]=%s", tag, i, names ? names[i] : "(null)");
}

int h_vkCreateInstance(const void* ci, const void* ac, void** out);
void* h_vkGetInstanceProcAddr(void* inst, const char* name);
int h_vkAcquire6(void* dev, void* sc, long long t, void* sem, void* fence, void* img);
int h_vkPresent(void* q, void* si, void* f);
void* h_vkGetDeviceProcAddr(void* dev, const char* name);

static void* (*r_vkci)(const void*, const void*, void**);
static void* (*r_gipa)(void*, const char*);
static void* (*r_gdpa)(void*, const char*);
static void* comp_image_flags_proc(const char*,void*);
static int (*r_vkAcquire)(void*, void*, long long, void*, void*, void*);
static int (*r_vkPresent)(void*, void*, void*);
static int (*r_vkQueueSubmit2)(void*, unsigned, const void*, void*);
static int (*r_vkCreateSwapchain)(void*, const void*, const void*, void**);
static void (*r_vkCmdBeginRenderPass)(void*, const void*, unsigned);
static int (*r_vkGetSCI)(void*, void*, unsigned*, void**);
static void (*r_vkCmdClearColorImage)(void*, void*, int, const void*, unsigned, const void*);
static int (*r_vkCreateImageView)(void*, const void*, const void*, void**);
static int (*r_vkCreateFB)(void*, const void*, const void*, void**);
static void (*r_vkExecCmds)(void*, unsigned, const void**);
static void (*r_vkBeginRendering)(void*, const void*);
int h_vkCreateFB(void* dev, const void* ci, const void* ac, void** out);
void h_vkExecCmds(void* cmd, unsigned n, const void** cms);
void h_vkBeginRendering(void* cmd, const void* bi);
int h_vkCreateImageView(void* dev, const void* ci, const void* ac, void** out);
void h_vkCmdClearColorImage(void* cmd, void* img, int layout, const void* col, unsigned n, const void* ranges);
void h_vkCmdDraw(void* c, unsigned vc, unsigned inst, unsigned fv, unsigned fi);
void h_vkCmdDrawIdx(void* c, unsigned ic, unsigned inst, unsigned first, int voff, unsigned finst);
static int (*r_vkCreateDevice)(const void*, const void*, const void*, void**);
static int (*r_vkSurfaceCaps)(void*, void*, void*);
static int (*r_vkSurfaceFormats)(void*, void*, unsigned*, void*);
static int (*r_vkCreateAndroidSurface)(void*, const void*, const void*, void**);
static void (*r_vkDestroyImageView)(void*, void*, const void*);
int h_vkSurfaceFormats(void*, void*, unsigned*, void*);
int h_vkCreateAndroidSurface(void*, const void*, const void*, void**);
void h_vkDestroyImageView(void*, void*, const void*);
static void comp_capture_device_formats(const void*,int);
static const void* comp_prepare_device_formats(void*,const void*,unsigned char[72],const char*[257]);
int h_vkCreateDevice(const void* phys, const void* ci, const void* ac, void** out);
int h_vkGetSCI(void* dev, void* sc, unsigned* pcount, void** imgs);
static long g_present_count, g_acquire_count, g_submit_count, g_rp_count;
static int g_to_count, g_gdpa_names, g_gipa_names;
static void* g_window_sc;
static int g_ovrp_log, g_vrapi_log;
static int g_window_sc_log;
static void* ovrp_dispatch(const char* name);
unsigned long g_tgt_iel;
int g_iel_done;
// once-only InitializeEyeLayer_RenderThread: first call runs the original, later calls return immediately
__attribute__((naked)) static void hook_iel_impl(void) {
    asm volatile(
        "adrp x16, :got:g_iel_done\n"
        "ldr x16, [x16, :got_lo12:g_iel_done]\n"
        "ldr w17, [x16]\n"
        "cmp w17, #1\n"
        "b.ge 9f\n"
        "add w17, w17, #1\n"
        "str w17, [x16]\n"
        ".inst 0xd10183ff\n"
        ".inst 0xa90357f6\n"
        ".inst 0xa9044ff4\n"
        ".inst 0xa9057bfd\n"
        "adrp x16, :got:g_tgt_iel\n"
        "ldr x16, [x16, :got_lo12:g_tgt_iel]\n"
        "ldr x16, [x16]\n"
        "br x16\n"
        "9:\n"
        "ret\n"
        );
}

// guard RHICreateAliasedTexture(x8=sret, x1=TRefCountPtr&): null x1 -> return null w/o crash
unsigned long g_tgt_alias;
// ULocalPlayer::GetProjectionData(this, Viewport*, pass, FSceneViewProjectionData& out)
// The engine's stereo device is broken under our fake session; produce a standard
// mono perspective so the world renders. out: ViewOrigin@0(FVector 24B),
// ViewRotation@24(FRotator 12B), ... ProjectionMatrix@?~0x38(FMatrix 64B row-major).
int hook_getprojdata(void* self, void* vp, int pass, void* out) {
    static long n = 0;
    if (++n <= 3) LOGI("GetProjectionData(%p,pass=%d,out=%p) -> custom mono", self, pass, out);
    if (!out) return 0;
    unsigned char* p = (unsigned char*)out;
    // FSceneViewProjectionData (UE4.25): ProjectionMatrix@0, ViewProjectionMatrix@0x40, ... (matrices FIRST)
    float fov = 90.0f * 3.14159265f / 360.0f;
    float f = 1.0f / tanf(fov);
    float aspect = 1320.0f / 1264.0f;
    float zn = 0.05f, zf = 500.0f;
    for (int mi = 0; mi < 2; mi++) {
        float* m = (float*)(p + mi * 0x40);
        for (int i = 0; i < 16; i++) m[i] = 0.0f;
        m[0] = f / aspect;
        m[5] = f;
        m[10] = zf;
        m[11] = 1.0f;
        m[14] = -2.0f * zf * zn / (zf - zn);
    }
    // layout from original disasm: ViewRect@0x90, ConstrainedViewRect@0xa0 (FIntRect 4x s32)
    int* vr = (int*)(p + 0x90);
    vr[0] = 0; vr[1] = 0; vr[2] = 1320; vr[3] = 1264;
    int* cr = (int*)(p + 0xa0);
    cr[0] = 0; cr[1] = 0; cr[2] = 1320; cr[3] = 1264;
    return 1;
}
void hook_settrackingorigin(void* self, int origin) {  // BP-called; internal state broken under fake session -> no-op
    static long n = 0;
    if (++n <= 3) LOGI("SetTrackingOrigin(%p,%d) no-op", self, origin);
}
int hook_initdevice(void* self) {   // FOculusHMD::InitDevice — vtable call crashes on fake session; report success
    static long n = 0;
    if (++n <= 3) LOGI("InitDevice(%p) -> true (stub)", self);
    return 1;
}
__attribute__((naked)) static void hook_alias_impl(void) {
    asm volatile(
        "cbz x1, 9f\n"
        ".inst 0xa9bd57f6\n"
        ".inst 0xa9014ff4\n"
        ".inst 0xa9027bfd\n"
        ".inst 0x910083fd\n"
        "adrp x16, :got:g_tgt_alias\n"
        "ldr x16, [x16, :got_lo12:g_tgt_alias]\n"
        "ldr x16, [x16]\n"
        "br x16\n"
        "9:\n"
        "str xzr, [x8]\n"
        "mov x0, xzr\n"
        "ret\n"
        );
}

unsigned long g_tgt_art;
int g_sc_gen;
unsigned long g_tgt_iel;
int g_iel_done;
// naked entry: zero the two TRefCountPtr& stack args ([sp+8],[sp+16]) so the
// original operator= never Releases stack garbage; then run gate.
// pure naked: zero param9/10 TRefCountPtr& ([sp+0],[sp+8] per caller stp) then gate
// full hook with logging: guard cp/layer nulls (deny), zero TRefCountPtr&
// stack slots, then run the original. Registers fully saved around the C call.
void* g_hmd_instance;
#include "re4fix_tracking.h"
// null the StereoRenderingDevice slot inside GEngine (mono path, valid single view)
static void null_stereo_device() {
    unsigned long ge = *(unsigned long*)(g_ue4_base_cached + 0xaa75db0);
    if (!ge) return;
    volatile unsigned long* stereo = (volatile unsigned long*)(ge + 0xaf0);
    if (*stereo) {
        *stereo = 0;
        LOGI("GEngine+af0 StereoRenderingDevice nulled (mono mode)");
    }
}
long (*r_EnableHMD)(void*, int);
void art_log(long cp, long layer, long self);
void art_log(long cp, long layer, long self) {
    static long pass = 0, deny_cp = 0, deny_layer = 0;
    if (self && !g_hmd_instance) {
        g_hmd_instance = (void*)self;           // capture even on deny path (race fix)
        if (g_ue4_base_cached) null_stereo_device();
    }
    if (!cp) {
        if (++deny_cp <= 8 || (deny_cp % 2000) == 0) LOGI("ART deny cpNULL #%ld (pass=%ld,denyLayer=%ld)", deny_cp, pass, deny_layer);
    } else if (!layer) {
        if (++deny_layer <= 8 || (deny_layer % 2000) == 0) LOGI("ART deny layerNULL #%ld (pass=%ld)", deny_layer, pass);
    } else {
        if (pass == 0) {
            g_hmd_instance = (void*)self;
            LOGI("ART: HMD instance captured %p", (void*)self);
            if (g_ue4_base_cached) null_stereo_device();
        }
        if (++pass <= 8 || (pass % 2000) == 0) LOGI("ART pass #%ld cp=%p", pass, (void*)cp);
    }
}
__attribute__((naked)) static void hook_art_impl(void) {
    asm volatile(
        "ldr x9, [x0, #0x258]\n"
        "cbz x9, 9f\n"
        "ldr x10, [x9, #0x270]\n"
        "cbz x10, 9f\n"
        "stp x29, x30, [sp, #-0xa0]!\n"
        "stp x0, x1, [sp, #0x10]\n"
        "stp x2, x3, [sp, #0x20]\n"
        "stp x4, x5, [sp, #0x30]\n"
        "stp x6, x7, [sp, #0x40]\n"
        "stp x8, x9, [sp, #0x50]\n"
        "stp x10, x11, [sp, #0x60]\n"
        "stp x12, x13, [sp, #0x70]\n"
        "stp x14, x15, [sp, #0x80]\n"
        "stp x16, x17, [sp, #0x90]\n"
        "mov x2, x0\n"
        "mov x0, x9\n"
        "mov x1, x10\n"
        "bl art_log\n"
        "ldp x16, x17, [sp, #0x90]\n"
        "ldp x14, x15, [sp, #0x80]\n"
        "ldp x12, x13, [sp, #0x70]\n"
        "ldp x10, x11, [sp, #0x60]\n"
        "ldp x8, x9, [sp, #0x50]\n"
        "ldp x6, x7, [sp, #0x40]\n"
        "ldp x4, x5, [sp, #0x30]\n"
        "ldp x2, x3, [sp, #0x20]\n"
        "ldp x0, x1, [sp, #0x10]\n"
        "ldp x29, x30, [sp], #0xa0\n"
        "ldr x11, [sp, #0]\n"
        "ldr x12, [sp, #8]\n"
        "cmp x11, #0x10000\n"
        "b.lo 1f\n"
        "str xzr, [x11]\n"
        "1:\n"
        "cmp x12, #0x10000\n"
        "b.lo 2f\n"
        "str xzr, [x12]\n"
        "2:\n"
        ".inst 0xa9bd57f6\n"
        ".inst 0xa9014ff4\n"
        ".inst 0xa9027bfd\n"
        ".inst 0x910083fd\n"
        "adrp x16, :got:g_tgt_art\n"
        "ldr x16, [x16, :got_lo12:g_tgt_art]\n"
        "ldr x16, [x16]\n"
        "br x16\n"
        "9:\n"
        "mov w0, #0\n"
        "ret\n"
        );
}
int h_vkCreateInstance(const void* ci, const void* ac, void** out);
void* h_vkGetInstanceProcAddr(void* inst, const char* name);
int h_vkAcquire6(void* dev, void* sc, long long t, void* sem, void* fence, void* img);
int h_vkPresent(void* q, void* si, void* f);
void* h_vkGetDeviceProcAddr(void* dev, const char* name);

int h_vkCreateInstance(const void* ci, const void* ac, void** out);
void* h_vkGetInstanceProcAddr(void* inst, const char* name);
int h_vkAcquire6(void* dev, void* sc, long long t, void* sem, void* fence, void* img);
int h_vkPresent(void* q, void* si, void* f);
void* h_vkGetDeviceProcAddr(void* dev, const char* name);


int h_vkCreateInstance(const void* ci, const void* ac, void** out);
void* h_vkGetInstanceProcAddr(void* inst, const char* name);
int h_vkAcquire6(void* dev, void* sc, long long t, void* sem, void* fence, void* img);
int h_vkPresent(void* q, void* si, void* f);
void* h_vkGetDeviceProcAddr(void* dev, const char* name);

static void* (*r_vkci)(const void*, const void*, void**);
static void* (*r_gipa)(void*, const char*);
static void* (*r_gdpa)(void*, const char*);
static int (*r_vkAcquire)(void*, void*, long long, void*, void*, void*);
static int (*r_vkPresent)(void*, void*, void*);
static int (*r_vkQueueSubmit2)(void*, unsigned, const void*, void*);
static int (*r_vkCreateSwapchain)(void*, const void*, const void*, void**);
static void (*r_vkCmdBeginRenderPass)(void*, const void*, unsigned);
static int (*r_vkGetSCI)(void*, void*, unsigned*, void**);
static void (*r_vkCmdClearColorImage)(void*, void*, int, const void*, unsigned, const void*);
static int (*r_vkCreateImageView)(void*, const void*, const void*, void**);
static int (*r_vkCreateFB)(void*, const void*, const void*, void**);
static void (*r_vkExecCmds)(void*, unsigned, const void**);
static void (*r_vkBeginRendering)(void*, const void*);
int h_vkCreateFB(void* dev, const void* ci, const void* ac, void** out);
void h_vkExecCmds(void* cmd, unsigned n, const void** cms);
void h_vkBeginRendering(void* cmd, const void* bi);
int h_vkCreateImageView(void* dev, const void* ci, const void* ac, void** out);
void h_vkCmdClearColorImage(void* cmd, void* img, int layout, const void* col, unsigned n, const void* ranges);
void h_vkCmdDraw(void* c, unsigned vc, unsigned inst, unsigned fv, unsigned fi);
void h_vkCmdDrawIdx(void* c, unsigned ic, unsigned inst, unsigned first, int voff, unsigned finst);
static int (*r_vkCreateDevice)(const void*, const void*, const void*, void**);
static int (*r_vkSurfaceCaps)(void*, void*, void*);
int h_vkCreateDevice(const void* phys, const void* ci, const void* ac, void** out);
int h_vkGetSCI(void* dev, void* sc, unsigned* pcount, void** imgs);
static long g_present_count, g_acquire_count, g_submit_count, g_rp_count;
static int g_to_count, g_gdpa_names, g_gipa_names;
static void* g_window_sc;
static int g_ovrp_log, g_vrapi_log;
static int g_window_sc_log;
static void* ovrp_dispatch(const char* name);
unsigned long g_tgt_art;
unsigned long g_tgt_iel;
int g_iel_done;
// naked entry: zero the two TRefCountPtr& stack args ([sp+8],[sp+16]) so the
// original operator= never Releases stack garbage; then run gate.
// pure naked: zero param9/10 TRefCountPtr& ([sp+0],[sp+8] per caller stp) then gate
int h_vkCreateInstance(const void* ci, const void* ac, void** out) {
    if (!r_vkci) {
        void* h = dlopen("libvulkan.so", RTLD_LOCAL | RTLD_LAZY);
        r_vkci = (void* (*)(const void*, const void*, void**))(h ? dlsym(h, "vkCreateInstance") : 0);
    }
    if (ci) {
        const char* const* exts = *(const char* const* const*)((const char*)ci + 40);
        unsigned int n = *(const unsigned int*)((const char*)ci + 36);
        LOGI("vkCreateInstance exts=%d", n);
        log_ext_list("inst", exts, n);
    }
    int res = r_vkci ? (int)(long)r_vkci(ci, ac, out) : -3;
    LOGI("vkCreateInstance -> %d", res);
    return res;
}

void* h_vkGetDeviceProcAddr(void* dev, const char* name) {
    if (!r_gdpa) return 0;
    void* r = r_gdpa(dev, name);
    void* image=comp_image_flags_proc(name,r);if(image!=r)return image;
    if (!name) return r;
    if (r && !strcmp(name, "vkCreateSampler")) { r_touch_CreateSampler = r; return (void*)&h_touch_CreateSampler; }
    if (g_gdpa_names < 40) {
        g_gdpa_names++;
        LOGI("gdpa(%s)=%p", name, r);
    }
    if (strcmp(name, "vkCmdBeginRenderPass") == 0 && r) {
        r_vkCmdBeginRenderPass = (void (*)(void*, const void*, unsigned))r;
        return (void*)&h_vkCmdBeginRenderPass;
    }
    if (strcmp(name, "vkQueueSubmit") == 0 && r) {
        r_vkQueueSubmit2 = (int (*)(void*, unsigned, const void*, void*))r;
        return (void*)&h_vkQueueSubmit;
    }
    return r;
}

static void* g_own_imgs[8];
static unsigned g_win_img_count;
static volatile unsigned g_acq_mask;
static unsigned g_sc_fmt_req, g_sc_fmt_real;
static char g_surf_fmts[256];
static volatile long g_csc_calls, g_gsi_calls; static volatile int g_csc_lastfmt, g_csc_lastr;   // engine-requested vs actually-created swapchain format
struct rptrack { void* rp; unsigned n; unsigned fmt0, load0, store0, init0, final0, dload, fmt1, init1, final1; };
static struct rptrack g_rps[256]; static int g_nrps;
static int (*r_vkCreateRenderPass)(void*, const void*, const void*, void**);
int h_vkCreateRenderPass(void* dev, const void* ci, const void* ac, void** out);
static __thread void* t_pass_rp;
static int g_magenta = 0;   // experiment: swapchain-pass clear colour -> magenta   // window swapchain images currently acquired (acquire..present)

// ---------- render-pass tracer (correct Vulkan struct offsets) ----------
struct vtrack { void* view; int sw; unsigned fmt; unsigned char info[80]; };
static struct vtrack g_views[64]; static int g_nviews;
struct fbtrack { void* fb; unsigned w, h, natt; int sw; };
static struct fbtrack g_fbs[512]; static int g_nfbs;
static __thread int t_pass_sw = -2, t_pass_draws, t_pass_maxic;
static __thread unsigned t_pass_w, t_pass_h;
static __thread float t_pass_clear[4];
static long g_pass_log, g_pass_total;
static void (*r_vkCmdEndRenderPass)(void*);
void h_vkCmdEndRenderPass(void* cmd);
static int swidx_of_image(void* img) {
    for (unsigned i = 0; i < g_win_img_count && i < 8; i++) if (g_own_imgs[i] == img) return (int)i;
    return -1;
}
static int swidx_of_view(void* v) {
    for (int k = g_nviews - 1; k >= 0 && k >= g_nviews - 64; k--) if (g_views[k % 64].view == v) return g_views[k % 64].sw;
    return -1;
}
static struct rptrack* rp_find(void* rp) {
    for (int k = g_nrps - 1; k >= 0 && k >= g_nrps - 256; k--) if (g_rps[k % 256].rp == rp) return &g_rps[k % 256];
    return 0;
}
static int fb_sw(void* fb) {
    for (int k = g_nfbs - 1; k >= 0 && k >= g_nfbs - 512; k--) if (g_fbs[k % 512].fb == fb) return g_fbs[k % 512].sw;
    return -1;
}
void* g_vk_instance;
void* h_vkGetInstanceProcAddr(void* inst, const char* name) {
    if (inst) g_vk_instance = inst;
    if (!r_gipa) {
        void* h = dlopen("libvulkan.so", RTLD_LOCAL | RTLD_LAZY);
        r_gipa = (void* (*)(void*, const char*))(h ? dlsym(h, "vkGetInstanceProcAddr") : 0);
    }
    void* r = r_gipa ? r_gipa(inst, name) : 0;
    void* image=comp_image_flags_proc(name,r);if(image!=r)return image;
    if (!name) return r;
    if (r && !strcmp(name, "vkCreateSampler")) { r_touch_CreateSampler = r; return (void*)&h_touch_CreateSampler; }
    if (g_gipa_names < 120) {
        g_gipa_names++;
        LOGI("gipa(%s)=%p", name, r);
    }
    if (strcmp(name, "vkAcquireNextImageKHR") == 0) { r_vkAcquire = (int (*)(void*, void*, long long, void*, void*, void*))r; return (void*)&h_vkAcquire6; }
    if (strcmp(name, "vkQueuePresentKHR") == 0) { r_vkPresent = (int (*)(void*, void*, void*))r; return (void*)&h_vkPresent; }
    if (strcmp(name, "vkQueueSubmit") == 0) { r_vkQueueSubmit2 = (int (*)(void*, unsigned, const void*, void*))r; return (void*)&h_vkQueueSubmit; }
    if (strcmp(name, "vkCmdBeginRenderPass") == 0 && r) { r_vkCmdBeginRenderPass = (void (*)(void*, const void*, unsigned))r; return (void*)&h_vkCmdBeginRenderPass; }
    if (strcmp(name, "vkCmdEndRenderPass") == 0 && r) { r_vkCmdEndRenderPass = (void (*)(void*))r; return (void*)&h_vkCmdEndRenderPass; }
    if (strcmp(name, "vkCreateRenderPass") == 0 && r) { r_vkCreateRenderPass = (int (*)(void*, const void*, const void*, void**))r; return (void*)&h_vkCreateRenderPass; }
    if (strcmp(name, "vkCreateSwapchainKHR") == 0) { r_vkCreateSwapchain = (int (*)(void*, const void*, const void*, void**))r; return (void*)&h_vkCreateSwapchain; }
    if (strcmp(name, "vkGetDeviceProcAddr") == 0) { r_gdpa = (void* (*)(void*, const char*))r; return (void*)&h_vkGetDeviceProcAddr; }
    if (strcmp(name, "vkGetSwapchainImagesKHR") == 0 && r) { r_vkGetSCI = (int (*)(void*, void*, unsigned*, void**))r; return (void*)&h_vkGetSCI; }
    if (strcmp(name, "vkCmdDraw") == 0 && r) { r_vkCmdDraw = (void (*)(void*, unsigned, unsigned, unsigned, unsigned))r; return (void*)&h_vkCmdDraw; }
    if (strcmp(name, "vkCmdClearColorImage") == 0 && r) { r_vkCmdClearColorImage = (void (*)(void*, void*, int, const void*, unsigned, const void*))r; return (void*)&h_vkCmdClearColorImage; }
    if (strcmp(name, "vkCreateImageView") == 0 && r) { r_vkCreateImageView = (int (*)(void*, const void*, const void*, void**))r; return (void*)&h_vkCreateImageView; }
    if (strcmp(name, "vkCreateFramebuffer") == 0 && r) { r_vkCreateFB = (int (*)(void*, const void*, const void*, void**))r; return (void*)&h_vkCreateFB; }
    if (strcmp(name, "vkCmdExecuteCommands") == 0 && r) { r_vkExecCmds = (void (*)(void*, unsigned, const void**))r; return (void*)&h_vkExecCmds; }
    if (strcmp(name, "vkCmdBeginRendering") == 0 && r) { r_vkBeginRendering = (void (*)(void*, const void*))r; return (void*)&h_vkBeginRendering; }
    if (strcmp(name, "vkCmdBeginRenderingKHR") == 0 && r) { r_vkBeginRendering = (void (*)(void*, const void*))r; return (void*)&h_vkBeginRendering; }
    if (strcmp(name, "vkCmdDrawIndexed") == 0 && r) { r_vkCmdDrawIdx = (void (*)(void*, unsigned, unsigned, unsigned, int, unsigned))r; return (void*)&h_vkCmdDrawIdx; }
    if (strcmp(name, "vkCreateDevice") == 0) { r_vkCreateDevice = (int (*)(const void*, const void*, const void*, void**))r; return (void*)&h_vkCreateDevice; }
    if (r && !strcmp(name, "vkGetPhysicalDeviceSurfaceFormatsKHR")) { r_vkSurfaceFormats = r; return (void*)&h_vkSurfaceFormats; }
    if (r && !strcmp(name, "vkCreateAndroidSurfaceKHR")) { r_vkCreateAndroidSurface = r; return (void*)&h_vkCreateAndroidSurface; }
    if (r && !strcmp(name, "vkDestroyImageView")) { r_vkDestroyImageView = r; return (void*)&h_vkDestroyImageView; }
    return r;
}

static void* g_phys_dev;         // VkPhysicalDevice captured at vkCreateDevice
static void* g_vk_device;
static volatile int g_comp_format_list;
int h_vkCreateDevice(const void* phys, const void* ci, const void* ac, void** out) {
    g_phys_dev = (void*)phys;
    unsigned char copied_ci[72] __attribute__((aligned(8)));
    const char* names[257];
    const void* selected_ci=comp_prepare_device_formats((void*)phys,ci,copied_ci,names);
    int r = r_vkCreateDevice(phys, selected_ci, ac, out);
    comp_capture_device_formats(selected_ci,r);
    graphics_audit_device(phys, selected_ci, r);
    if (r == 0 && out) g_vk_device = *out;
    LOGF("vkCreateDevice -> %d phys=%p dev=%p", r, g_phys_dev, out ? *out : 0);
    return r;
}
#include "re4fix_comp.h"
#include "re4fix_comp_surface.h"
#include "re4fix_comp_views.h"
#include "re4fix_comp_image_flags.h"
#include "re4fix_comp_framebuffer_lifecycle.h"

void h_vkDestroyImageView(void* dev,void* view,const void* allocator) {
    for (unsigned i=0;i<64;i++) if (g_views[i].view==view) {
        g_views[i].view=0;g_views[i].sw=-1;g_views[i].fmt=0;
        memset(g_views[i].info,0,sizeof g_views[i].info);
    }
    comp_linear_view_destroyed(view);
    r_vkDestroyImageView(dev,view,allocator);
}

static long g_draw_n, g_drawidx_n;
// FORWARDING draw hooks. The previous versions only counted and never called the driver, so every
// draw since session 3 was silently dropped -> the swapchain only ever held clears (black screen).
static void (*r_vkCmdDraw)(void*, unsigned, unsigned, unsigned, unsigned);
static void (*r_vkCmdDrawIdx)(void*, unsigned, unsigned, unsigned, int, unsigned);
static long g_sec_draws, g_sec_tris, g_sec_maxic, g_sec_t0, g_sec_logs;
static void draw_stats(unsigned ic) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    long now = ts.tv_sec;
    if (!g_sec_t0) g_sec_t0 = now;
    g_sec_draws++; g_sec_tris += ic / 3; if ((long)ic > g_sec_maxic) g_sec_maxic = ic;
    if (now != g_sec_t0) {
        if (g_sec_logs < 200 && (now % 5) == 0) { g_sec_logs++; LOGF("DRAWSTAT draws/s=%ld tris/s=%ld max_ic=%ld | csc=%ld(fmt=%d r=%d req=%u real=%u) gsi=%ld imgs=%u surf=[%s] | comp on=%d dis=%d views=%ld inits=%ld blits=%ld skip=%ld fail=%ld err=%d", g_sec_draws, g_sec_tris, g_sec_maxic, g_csc_calls, g_csc_lastfmt, g_csc_lastr, g_sc_fmt_req, g_sc_fmt_real, g_gsi_calls, g_win_img_count, g_surf_fmts, g_comp_on, g_comp_disable, g_comp_views, g_comp_inits, g_comp_blits, g_comp_skip, g_comp_fail, g_comp_last_err);
            LOGF("POSE head=%ld eyeC=%ld eyeL=%ld handL=%ld handR=%ld yaw=%.1f pitch=%.1f ptr=%d(%.2f,%.2f)", g_pose_calls[9], g_pose_calls[2], g_pose_calls[0], g_pose_calls[3], g_pose_calls[4], g_look_yaw * 57.29578f, g_look_pitch * 57.29578f, ptr_active(), g_ptr_u, g_ptr_v);
            LOGF("MODE menu_ticks=%ld menu=%d", g_menu_ticks, (int)(ptr_now_ms() - g_menu_tick_ms < 300));
            LOGF("INSTAT attach=%ld get=%ld motion=%ld pre=%ld act=%ld tq=%ld tn=%ld", g_in_attach, g_in_get, g_in_motion, g_in_pre, g_in_act, g_tq_calls, g_tn); }
        g_sec_t0 = now; g_sec_draws = 0; g_sec_tris = 0; g_sec_maxic = 0;
    }
}
void h_vkCmdDraw(void* c, unsigned vc, unsigned inst, unsigned fv, unsigned fi) {
    g_draw_n++;
    draw_stats(vc);
    t_pass_draws++; if ((int)vc > t_pass_maxic) t_pass_maxic = (int)vc;
    r_vkCmdDraw(c, vc, inst, fv, fi);
}
void h_vkCmdDrawIdx(void* c, unsigned ic, unsigned inst, unsigned first, int voff, unsigned finst) {
    g_drawidx_n++;
    draw_stats(ic);
    t_pass_draws++; if ((int)ic > t_pass_maxic) t_pass_maxic = (int)ic;
    r_vkCmdDrawIdx(c, ic, inst, first, voff, finst);
}

// VkFramebufferCreateInfo: renderPass@16, attachmentCount@24, pAttachments@32(VkImageView*), w@40, h@44
int h_vkCreateFB(void* dev, const void* ci, const void* ac, void** out) {
    // VkFramebufferCreateInfo: attachmentCount@32 pAttachments@40 width@48 height@52
    _Alignas(8) unsigned char compatible[64];void* attachments[8];const void* selected=ci;
    int compatibility=comp_linear_framebuffer(dev,ci,compatible,attachments,&selected);
    if (compatibility) { LOGF("COMP linear framebuffer view failed=%d",compatibility);return compatibility; }
    ci=selected;
    int r = r_vkCreateFB(dev, ci, ac, out);
    if (r == 0 && ci && out) {
        unsigned n = *(const unsigned*)((const char*)ci + 32);
        void* const* atts = *(void* const* const*)((const char*)ci + 40);
        int sw = -1;
        for (unsigned i = 0; i < n && i < 8 && atts; i++) { int k = swidx_of_view(atts[i]); if (k >= 0) sw = k; }
        struct fbtrack* f = &g_fbs[g_nfbs % 512]; g_nfbs++;
        f->fb = *out; f->w = *(const unsigned*)((const char*)ci + 48); f->h = *(const unsigned*)((const char*)ci + 52); f->natt = n; f->sw = sw;
        if (sw >= 0) {
            void* rp = *(void* const*)((const char*)ci + 24);
            for (unsigned i = 0; i < n && i < 8 && atts; i++) {
                for (int v = 0; v < g_nviews && v < 64; v++) if (g_views[v].view == atts[i]) {
                    unsigned rpfmt = 0;
                    struct rptrack* rt = rp_find(rp);
                    if (rt) rpfmt = (i == 0) ? rt->fmt0 : (i == 1 ? rt->fmt1 : 0);
                    LOGF("FBSW fb=%p %ux%u att[%u]=swapchain#%d viewfmt=%u rp=%p rpfmt[%u]=%u %s", *out, f->w, f->h, i,
                         g_views[v].sw, g_views[v].fmt, rp, i, rpfmt, (rpfmt && rpfmt != g_views[v].fmt) ? "MISMATCH" : "");
                }
            }
        }
    }
    return r;
}
static long g_exec_n;
void h_vkExecCmds(void* cmd, unsigned n, const void** cms) {
    g_exec_n++;
    if (g_exec_n <= 5 || (g_exec_n % 5000) == 0) LOGI("vkCmdExecuteCommands #%ld n=%u", g_exec_n, n);
    r_vkExecCmds(cmd, n, cms);
}
static long g_br_n;
void h_vkBeginRendering(void* cmd, const void* bi) {
    g_br_n++;
    if (g_br_n <= 5 || (g_br_n % 5000) == 0) LOGI("vkCmdBeginRendering #%ld", g_br_n);
    r_vkBeginRendering(cmd, bi);
}

static void** g_win_images;      // VkImage[] of the window swapchain

static unsigned g_win_img_count;
// diag: log/fix image view creation for window swapchain images
int h_vkCreateImageView(void* dev, const void* ci, const void* ac, void** out) {
    // VkImageViewCreateInfo: image@24 format@36
    unsigned char vbuf[80];
    int sw = ci ? swidx_of_image(*(void* const*)((const char*)ci + 24)) : -1;
    unsigned vfmt0 = ci ? *(const unsigned*)((const char*)ci + 36) : 0;
    int proxied = 0;
    if (sw >= 0 && vfmt0 == g_sc_fmt_req) {
        if (g_comp_on) { const void* c2 = comp_view_ci(ci, sw, vbuf); proxied = (c2 != ci); ci = c2; }
        else if (g_sc_fmt_real && g_sc_fmt_real != g_sc_fmt_req) { memcpy(vbuf, ci, 80); *(unsigned*)(vbuf + 36) = g_sc_fmt_real; ci = vbuf; }
    }
    int r = r_vkCreateImageView(dev, ci, ac, out);
    if (r == 0 && ci && out && sw >= 0) {
        unsigned vf = *(const unsigned*)((const char*)ci + 36);
        int k = g_nviews % 64;
        g_views[k].view = *out; g_views[k].sw = proxied ? sw : -1 - sw; g_views[k].fmt = vf;
        memcpy(g_views[k].info,ci,sizeof g_views[k].info);
        __sync_synchronize(); g_nviews++;
        LOGF("PASS view for swapchain img #%d fmt=%u->%u view=%p %s", sw, vfmt0, vf, *out, proxied ? "PROXY" : "direct");
    } else if (r != 0 && sw >= 0) {
        LOGF("PASS view for swapchain img #%d FAILED r=%d (proxied=%d)", sw, r, proxied);
    }
    return r;
}


// Adreno refuses vkCmdClearColorImage on WSI swapchain images; skip those
// clears (first frame shows stale content, game draws over it afterwards)
void h_vkCmdClearColorImage(void* cmd, void* img, int layout, const void* col, unsigned n, const void* ranges) {
    // Skip clears of window-swapchain images: the HMD eye layer wraps them (GetLayerTexture2) and the RHI's
    // FVulkanSurface::InitialClear on such a non-acquired WSI image crashes the Adreno driver.
    for (unsigned i = 0; i < g_win_img_count && i < 8; i++)
        if (g_win_images && g_win_images[i] == img) {
            static long ok = 0, sk = 0;
            if (g_acq_mask & (1u << i)) { if (++ok <= 3) LOGI("clear acquired swapchain img #%u", i); break; }
            if (++sk <= 5) LOGI("skip clear of NON-acquired swapchain img #%u (mask=%x)", i, g_acq_mask);
            return;
        }
    r_vkCmdClearColorImage(cmd, img, layout, col, n, ranges);
}


static int (*r_vkGetSCI)(void*, void*, unsigned*, void**);
static int (*r_vkCreateDevice)(const void*, const void*, const void*, void**);
static int (*r_vkSurfaceCaps)(void*, void*, void*);
int h_vkCreateDevice(const void* phys, const void* ci, const void* ac, void** out);
static void* g_own_imgs[8];
int h_vkGetSCI(void* dev, void* sc, unsigned* pcount, void** imgs) {
    int r = r_vkGetSCI(dev, sc, pcount, imgs);
    g_gsi_calls++;
    LOGF("GETSCI sc=%p imgs=%p count=%u r=%d", sc, (void*)imgs, pcount ? *pcount : 0, r);
    if (r == 0 && imgs && pcount && *pcount && *pcount <= 8) {
        for (unsigned i = 0; i < *pcount; i++) g_own_imgs[i] = imgs[i];
        g_win_images = g_own_imgs; g_win_img_count = *pcount; g_acq_mask = 0;
        if (sc != g_window_sc) { g_sc_gen++; g_iel_done = 0; }  // new swapchain: allow eye-layer re-init
        g_window_sc = sc;
        LOGF("window swapchain images: %u (captured: %p %p %p %p)", *pcount,
             imgs[0], *pcount > 1 ? imgs[1] : 0, *pcount > 2 ? imgs[2] : 0, *pcount > 3 ? imgs[3] : 0);
        comp_create(dev, *pcount);
    }
    return r;
}

int h_vkCreateSwapchain(void* dev, const void* ci, const void* ac, void** out) {
    // VkSwapchainCreateInfoKHR: surface@24 minImageCount@32 imageFormat@36 imageColorSpace@40
    // imageExtent@44/48 imageArrayLayers@52 imageUsage@56 ... presentMode@88, sizeof = 104
    g_csc_calls++;
    if (!ci) return r_vkCreateSwapchain(dev, ci, ac, out);
    unsigned char buf[104];
    memcpy(buf, ci, 104);
    unsigned* usage = (unsigned*)(buf + 56);
    unsigned u0 = *usage;
    unsigned w = *(unsigned*)(buf + 44), h = *(unsigned*)(buf + 48);
    unsigned want = u0 | 0x13;   // + TRANSFER_SRC | TRANSFER_DST | COLOR_ATTACHMENT
    if (!r_vkSurfaceCaps && r_gipa && g_vk_instance)
        r_vkSurfaceCaps = (int (*)(void*, void*, void*))r_gipa(g_vk_instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    void* surface = *(void**)(buf + 24);
    unsigned caps[13] = {0};    // VkSurfaceCapabilitiesKHR (52 B); supportedUsageFlags = caps[12]
    int cr = -1;
    if (r_vkSurfaceCaps && g_phys_dev && surface) cr = r_vkSurfaceCaps(g_phys_dev, surface, caps);
    if (cr == 0) {
        want &= (caps[12] | u0);
        if ((w == 0 || h == 0 || w > 8192 || h > 8192) && caps[2] && caps[2] != 0xffffffffu) {
            w = caps[2]; h = caps[3];
            *(unsigned*)(buf + 44) = w; *(unsigned*)(buf + 48) = h;
        }
    }
    *usage = want;
    {   // The scene resolves MSAA (B8G8R8A8_SRGB, fmt 50) straight into the backbuffer, but the engine asks for an
        // A2B10G10R10 (64) swapchain -> framebuffer view/render-pass format mismatch -> scrambled colours.
        if (!r_vkSurfaceFormats && r_gipa && g_vk_instance)
            r_vkSurfaceFormats = r_gipa(g_vk_instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
        unsigned req = *(unsigned*)(buf + 36), pick = req;
        if (r_vkSurfaceFormats && g_phys_dev && surface) {
            unsigned n = 0, f[2 * 64];
            if (r_vkSurfaceFormats(g_phys_dev, surface, &n, 0) == 0 && n > 0) {
                if (n > 64) n = 64;
                if (r_vkSurfaceFormats(g_phys_dev, surface, &n, f) == 0 || n) {
                    char lst[256]; int o = 0;
                    for (unsigned i = 0; i < n && o < 240; i++) o += snprintf(lst + o, 256 - o, "%u/%u ", f[2 * i], f[2 * i + 1]);
                    LOGF("surface formats (fmt/colorspace): %s", lst);
                    memcpy(g_surf_fmts, lst, sizeof g_surf_fmts);
                    for (unsigned i = 0; i < n; i++) if (f[2 * i] == 43 && f[2 * i + 1] == 0) pick = 43;
                    for (unsigned i = 0; i < n; i++) if (f[2 * i] == 50 && f[2 * i + 1] == 0) pick = 50;
                }
            }
        }
        g_sc_fmt_req = req; g_sc_fmt_real = pick;
        if (pick != req) { *(unsigned*)(buf + 36) = pick; *(unsigned*)(buf + 40) = 0; LOGF("swapchain format %u -> %u", req, pick); }
        g_comp_w = *(unsigned*)(buf + 44); g_comp_h = *(unsigned*)(buf + 48);
        g_comp_usage_extra = u0 & 0x80u;   // INPUT_ATTACHMENT
    }
    int r = r_vkCreateSwapchain(dev, buf, ac, out);
    g_csc_lastfmt = (int)*(unsigned*)(buf + 36); g_csc_lastr = r;
    LOGF("vkCreateSwapchain -> %d fmt=%u cs=%u %ux%u minIC=%u usage 0x%x->0x%x (surf.usage=0x%x caps=%d) sc=%p",
         r, *(unsigned*)(buf + 36), *(unsigned*)(buf + 40), w, h, *(unsigned*)(buf + 32), u0, want, caps[12], cr, out ? *out : 0);
    if (r == 0 && out) g_window_sc = *out;
    return r;
}

int h_vkQueueSubmit(void* q, unsigned n, const void* infos, void* fence) {
    g_submit_count++;
    if (g_submit_count <= 4) LOGI("SUB lr=%p", __builtin_return_address(0));
    if (g_submit_count <= 10) {
        const struct VkSubmitInfoShim* si = (const struct VkSubmitInfoShim*)infos;
        for (unsigned i = 0; i < n && i < 4; i++)
            LOGI("vkQueueSubmit #%ld n=%u cb=%u waits=%u sigs=%u", g_submit_count, n,
                 si[i].commandBufferCount, si[i].waitSemaphoreCount, si[i].signalSemaphoreCount);
    } else if ((g_submit_count % 500) == 0) {
        LOGI("vkQueueSubmit #%ld", g_submit_count);
    }
    return r_vkQueueSubmit2(q, n, infos, fence);
}

int h_vkCreateRenderPass(void* dev, const void* ci, const void* ac, void** out) {
    // VkRenderPassCreateInfo: attachmentCount@20 pAttachments@24; VkAttachmentDescription (36 B):
    // format@4 samples@8 loadOp@12 storeOp@16 stencilLoad@20 stencilStore@24 initialLayout@28 finalLayout@32
    int r = r_vkCreateRenderPass(dev, ci, ac, out);
    if (r == 0 && ci && out) {
        unsigned n = *(const unsigned*)((const char*)ci + 20);
        const unsigned char* a = *(const unsigned char* const*)((const char*)ci + 24);
        struct rptrack* t = &g_rps[g_nrps % 256]; g_nrps++;
        memset(t, 0, sizeof *t); t->rp = *out; t->n = n;
        if (a && n) {
            t->fmt0 = *(const unsigned*)(a + 4); t->load0 = *(const unsigned*)(a + 12); t->store0 = *(const unsigned*)(a + 16);
            t->init0 = *(const unsigned*)(a + 28); t->final0 = *(const unsigned*)(a + 32);
            if (n > 1) { t->fmt1 = *(const unsigned*)(a + 36 + 4); t->dload = *(const unsigned*)(a + 36 + 12);
                         t->init1 = *(const unsigned*)(a + 36 + 28); t->final1 = *(const unsigned*)(a + 36 + 32); }
        }
        if (g_nrps <= 60) LOGF("RP %p atts=%u att0{fmt=%u load=%u store=%u init=%u final=%u} att1{fmt=%u load=%u init=%u final=%u}",
                               *out, n, t->fmt0, t->load0, t->store0, t->init0, t->final0, t->fmt1, t->dload, t->init1, t->final1);
    }
    return r;
}
void h_vkCmdBeginRenderPass(void* cmd, const void* bi, unsigned c) {
    // VkRenderPassBeginInfo: framebuffer@24 renderArea.extent@40 clearValueCount@48 pClearValues@56
    t_pass_sw = -3; t_pass_w = t_pass_h = 0; t_pass_draws = 0; t_pass_maxic = 0;
    t_pass_clear[0] = t_pass_clear[1] = t_pass_clear[2] = t_pass_clear[3] = -1.0f;
    if (bi) {
        void* fbh = *(void* const*)((const char*)bi + 24);
        t_pass_sw = fb_sw(fbh);
        if (t_pass_sw >= 0) comp_begin_pass(cmd, t_pass_sw);
        t_pass_w = *(const unsigned*)((const char*)bi + 40); t_pass_h = *(const unsigned*)((const char*)bi + 44);
        unsigned ncv = *(const unsigned*)((const char*)bi + 48);
        const float* cv = *(const float* const*)((const char*)bi + 56);
        if (ncv && cv) { t_pass_clear[0] = cv[0]; t_pass_clear[1] = cv[1]; t_pass_clear[2] = cv[2]; t_pass_clear[3] = cv[3]; }
        t_pass_rp = *(void* const*)((const char*)bi + 16);
        if (g_magenta && t_pass_sw >= 0 && ncv && cv && ncv <= 8) {
            unsigned char bcopy[64]; float cvals[8 * 4];
            memcpy(bcopy, bi, 64); memcpy(cvals, cv, ncv * 16);
            cvals[0] = 1.0f; cvals[1] = 0.0f; cvals[2] = 1.0f; cvals[3] = 1.0f;
            *(const float**)(bcopy + 56) = cvals;
            g_rp_count++;
            r_vkCmdBeginRenderPass(cmd, bcopy, c);
            return;
        }
    }
    g_rp_count++;
    r_vkCmdBeginRenderPass(cmd, bi, c);
}
void h_vkCmdEndRenderPass(void* cmd) {
    g_pass_total++;
    if (g_pass_log < 30 || (t_pass_sw >= 0 && (g_pass_total % 1800) == 0)) {
        g_pass_log++;
        unsigned ld = 99, st = 99, fm = 0;
        struct rptrack* rt = rp_find(t_pass_rp);
        if (rt) { ld = rt->load0; st = rt->store0; fm = rt->fmt0; }
        LOGI("PASS rp att0 fmt=%u load=%u store=%u", fm, ld, st);
        LOGI("PASS end #%ld target=%s%d %ux%u draws=%d max_ic=%d clear0={%.2f,%.2f,%.2f,%.2f}", g_pass_total,
             t_pass_sw >= 0 ? "SWAPCHAIN#" : (t_pass_sw == -1 ? "offscreen" : "unknown"), t_pass_sw,
             t_pass_w, t_pass_h, t_pass_draws, t_pass_maxic,
             t_pass_clear[0], t_pass_clear[1], t_pass_clear[2], t_pass_clear[3]);
    }
    t_pass_sw = -2;
    r_vkCmdEndRenderPass(cmd);
}



// ---------- inline trampolines into libUE4 (16-byte prologue steal) ----------
unsigned long g_tgt_ifr, g_tgt_create;  // target+16 addresses (exported for GOT)

// gates: execute stolen prologue instructions, then jump to target+16
__attribute__((naked)) static void gate_ifr(unsigned int plat) {
    asm volatile(
        ".inst 0xd101c3ff\n"  // sub sp, sp, #0x70
        ".inst 0xa9035ff8\n"  // stp x24,x23,[sp,#0x30]
        ".inst 0xa90457f6\n"  // stp x22,x21,[sp,#0x20]
        ".inst 0xa9054ff4\n"  // stp x20,x19,[sp,#0x10]
        "adrp x16, :got:g_tgt_ifr\n"
        "ldr x16, [x16, :got_lo12:g_tgt_ifr]\n"
        "ldr x16, [x16]\n"
        "br x16\n");
}

__attribute__((naked)) static void* gate_create(unsigned int plat, void* dir, void* name) {
    asm volatile(
        ".inst 0xd10343ff\n"  // sub sp, sp, #0xd0
        ".inst 0xa90867fa\n"  // stp x26,x25,[sp,#0x40]
        ".inst 0xa9095ff8\n"  // stp x24,x23,[sp,#0x30]
        ".inst 0xa90a57f6\n"  // stp x22,x21,[sp,#0x20]
        "adrp x16, :got:g_tgt_create\n"
        "ldr x16, [x16, :got_lo12:g_tgt_create]\n"
        "ldr x16, [x16]\n"
        "br x16\n");
}

static void u16_to_ascii(const unsigned short* w, char* out, int maxn) {
    if (!w) { out[0] = 0; return; }
    int i = 0;
    for (; i < maxn - 1 && w[i]; i++) out[i] = (w[i] < 128) ? (char)w[i] : '?';
    out[i] = 0;
}

static void hook_ifr(unsigned int plat) {
    LOGI(">> FShaderCodeLibrary::InitForRuntime(%u)", plat);
    gate_ifr(plat);
    unsigned long b = g_ue4_base_cached;
    if (b) {
        unsigned long arr = *(unsigned long*)(b + 0xa37e428); // GGlobalShaderMap array
        if (arr) {
            void* gm = *(void**)(arr + 8ULL * 19);
            LOGI("<< InitForRuntime done; GGlobalShaderMap[19]=%p", gm);
        } else {
            LOGI("<< InitForRuntime done; GGlobalShaderMap array is NULL");
        }
    }
}

static void* hook_create(unsigned int plat, void* dir, void* name) {
    char d[128], n[128];
    const unsigned short* wd = dir ? *(const unsigned short**)dir : 0;
    const unsigned short* wn = name ? *(const unsigned short**)name : 0;
    u16_to_ascii(wd, d, 128);
    u16_to_ascii(wn, n, 128);
    LOGI(">> FShaderLibraryInstance::Create(%u, dir=%s, name=%s)", plat, d, n);
    void* r = gate_create(plat, dir, name);
    LOGI("<< Create returned %p", r);
    return r;
}


unsigned long g_tgt_lfta;
__attribute__((naked)) static long gate_lfta(void* arr, const unsigned short* fn, int fl) {
    asm volatile(
        ".inst 0xf81d0ff5\n"
        ".inst 0xa9014ff4\n"
        ".inst 0xa9027bfd\n"
        ".inst 0x910083fd\n"
        "adrp x16, :got:g_tgt_lfta\n"
        "ldr x16, [x16, :got_lo12:g_tgt_lfta]\n"
        "ldr x16, [x16]\n"
        "br x16\n");
}


unsigned long g_tgt_cgsm;
__attribute__((naked)) static void gate_cgsm(unsigned int plat, void* tp, char b) {
    asm volatile(
        ".inst 0xa9ba6ffc\n"
        ".inst 0xa90167fa\n"
        ".inst 0xa9025ff8\n"
        ".inst 0xa90357f6\n"
        "adrp x16, :got:g_tgt_cgsm\n"
        "ldr x16, [x16, :got_lo12:g_tgt_cgsm]\n"
        "ldr x16, [x16]\n"
        "br x16\n");
}

void hook_cgsm(unsigned int plat, void* tp, char b) {
    LOGI(">> CGSM lr=%p", __builtin_return_address(0));
    LOGI(">> CGSM regs: w0=%u w1=%p w2=%u w3=%u", plat, tp, b,
         ({ unsigned int _v; __asm__("mov %w0, w3":"=r"(_v)); _v; }));
    LOGI(">> CompileGlobalShaderMap(platform=%u, b=%d)", plat, b);
    gate_cgsm(plat, tp, b);
    LOGI("<< CompileGlobalShaderMap done");
}


void hook_ree(const unsigned short* reason) {
    char n[200];
    u16_to_ascii(reason, n, 200);
    LOGI(">> RequestEngineExit(%s) BLOCKED lr=%p", n, __builtin_return_address(0));
    // do not call gate: swallow the exit request entirely
}

// ---------- render-path tracers ----------
unsigned long g_tgt_cscrt;
static unsigned long* g_csc_imgbuf;
void fill_csc_textures(void* tarray);
// TArray<u64>{Data@0,Num@8,Max@16}: rewrite to our window swapchain images so
// the plugin's eye layer renders straight into presented frames.
void fill_csc_textures(void* tarray) {
    static long n = 0;
    if (!tarray) return;
    unsigned long* t = (unsigned long*)tarray;
    unsigned long data = t[0];
    unsigned num = (unsigned)(t[1] & 0xffffffff);
    if (n < 5) {
        n++;
        LOGI("CreateSwapChain: TArray Data=%p Num=%u (rewrite elements)", (void*)data, num);
    }
    if (!data || !num || num > 16) return;
    unsigned long* elems = (unsigned long*)data;
    unsigned cnt = g_win_img_count ? g_win_img_count : 1;
    for (unsigned i = 0; i < num && i < 1; i++)
        elems[i] = (unsigned long)g_win_images[i % cnt];
    t[1] = 1;
}
__attribute__((naked)) static void gate_cscrt(void) {
    asm volatile(
        ".inst 0xd10403ff\n"
        ".inst 0xa90a6ffc\n"
        ".inst 0xa90b67fa\n"
        ".inst 0xa90c5ff8\n"
        "adrp x16, :got:g_tgt_cscrt\n"
        "ldr x16, [x16, :got_lo12:g_tgt_cscrt]\n"
        "ldr x16, [x16]\n"
        "br x16\n");
}
// naked entry: preserve sp/x30 (stack args) AND all caller-saved regs across the fill call.
// w5/w6/w7 = NumMips/NumSamples/NumSamplesTileMem: every swapchain here wraps a one-level, one-sample
// window image, so they are forced to 1 (garbage counts made Mali's vkCreateImageView allocate gigabytes).
__attribute__((naked)) void hook_cscrt_impl(void) {
    asm volatile(
        "mov w5, #1\n"
        "mov w6, #1\n"
        "mov w7, #1\n"
        "ldr x9, [sp, #8]\n"
        "cbz x9, 1f\n"
        "stp x29, x30, [sp, #-0xa0]!\n"
        "stp x0, x1, [sp, #0x10]\n"
        "stp x2, x3, [sp, #0x20]\n"
        "stp x4, x5, [sp, #0x30]\n"
        "stp x6, x7, [sp, #0x40]\n"
        "stp x8, x9, [sp, #0x50]\n"
        "stp x10, x11, [sp, #0x60]\n"
        "stp x12, x13, [sp, #0x70]\n"
        "stp x14, x15, [sp, #0x80]\n"
        "stp x16, x17, [sp, #0x90]\n"
        "mov x0, x9\n"
        "bl fill_csc_textures\n"
        "ldp x16, x17, [sp, #0x90]\n"
        "ldp x14, x15, [sp, #0x80]\n"
        "ldp x12, x13, [sp, #0x70]\n"
        "ldp x10, x11, [sp, #0x60]\n"
        "ldp x8, x9, [sp, #0x50]\n"
        "ldp x6, x7, [sp, #0x40]\n"
        "ldp x4, x5, [sp, #0x30]\n"
        "ldp x2, x3, [sp, #0x20]\n"
        "ldp x0, x1, [sp, #0x10]\n"
        "ldp x29, x30, [sp], #0xa0\n"
        "b hook_cscrt_gate\n"
        "1:\n"
        "b hook_cscrt_gate\n"
        );
}
__attribute__((naked)) void hook_cscrt_gate(void) {
    asm volatile(
        ".inst 0xd10403ff\n"  // sub sp, sp, #0x100
        ".inst 0xa90a6ffc\n"  // stp x28,x27,[sp,#0xa0]
        ".inst 0xa90b67fa\n"  // stp x26,x25,[sp,#0xb0]
        ".inst 0xa90c5ff8\n"  // stp x24,x23,[sp,#0xc0]
        "adrp x16, :got:g_tgt_cscrt\n"
        "ldr x16, [x16, :got_lo12:g_tgt_cscrt]\n"
        "ldr x16, [x16]\n"
        "br x16\n"
        );
}

unsigned long g_tgt_art;
int g_sc_gen;
unsigned long g_tgt_iel;
int g_iel_done;

unsigned long g_tgt_cgv;
__attribute__((naked)) static void gate_cgv(void* self, void* client) {
    asm volatile(
        ".inst 0xd10243ff\n"  // sub sp, sp, #0x90
        ".inst 0xa9036ffc\n"  // stp x28,x27,[sp,#0x30]
        ".inst 0xa90467fa\n"  // stp x26,x25,[sp,#0x40]
        ".inst 0xa9055ff8\n"  // stp x24,x23,[sp,#0x50]
        "adrp x16, :got:g_tgt_cgv\n"
        "ldr x16, [x16, :got_lo12:g_tgt_cgv]\n"
        "ldr x16, [x16]\n"
        "br x16\n");
}
void hook_cgv(void* self, void* client) {
    LOGI("CreateGameViewport client=%p", client);
    if (client) {
        // FViewportClient subobject vptr at +0x2a0; Draw = slot 93
        void** sub = (void**)((char*)client + 0x2a0);
        void* vt = *sub;
        void* draw = vt ? ((void**)vt)[93] : 0;
        LOGI("client vptr(+0x2a0)=%p vt=%p Draw[93]=%p (base+0x%lx)",
             (void*)sub, vt, draw, (unsigned long)draw - g_ue4_base_cached);
    }
    gate_cgv(self, client);
}

unsigned long g_tgt_gvpdraw, g_tgt_beginrvf, g_tgt_calcsv, g_tgt_csvo;

// ULocalPlayer::CalcSceneViewInitOptions(InitOptions*, Viewport*, Drawer, StereoPass) @0x89b2950
__attribute__((naked)) static void gate_csvo(void* self, void* io, void* vp, void* dr, int pass) {
    asm volatile(
        ".inst 0xf81c0ff7\n"  // str x23,[sp,#-0x20]!
        ".inst 0xa90157f6\n"  // stp x22,x21,[sp,#0x10]
        ".inst 0xa9024ff4\n"  // stp x20,x19,[sp,#0x20]
        ".inst 0xa9037bfd\n"  // stp x29,x30,[sp,#0x30]
        "adrp x16, :got:g_tgt_csvo\n"
        "ldr x16, [x16, :got_lo12:g_tgt_csvo]\n"
        "ldr x16, [x16]\n"
        "br x16\n");
}
void hook_csvo(void* self, void* io, void* vp, void* dr, int pass) {
    static long n = 0;
    n++;
    if (n <= 3 || (n % 600) == 0) {
        // FSceneViewInitOptions: ViewFamily@0, ViewLocation@8(FVector 24B), ViewRotation@32(12),
        // ViewOrigin@?, ... Extent/ViewRect location varies; dump first 128 bytes as 8 u32 words
        unsigned* p = (unsigned*)io;
        LOGI("CSVO #%ld io=%p: %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x",
             n, io, p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11]);
    }
    gate_csvo(self, io, vp, dr, pass);
}


__attribute__((naked)) static void gate_gvpdraw(void* self, void* vp, void* cv) {
    asm volatile(
        ".inst 0xfc170fec\n"  // pacibsp
        ".inst 0x6d012beb\n"
        ".inst 0x6d0223e9\n"
        ".inst 0xa9036ffc\n"
        "adrp x16, :got:g_tgt_gvpdraw\n"
        "ldr x16, [x16, :got_lo12:g_tgt_gvpdraw]\n"
        "ldr x16, [x16]\n"
        "br x16\n");
}
void hook_gvpdraw(void* self, void* vp, void* cv) {
    static long n = 0;
    n++;
    if (n <= 10 || (n % 300) == 0) LOGI("UGameViewportClient::Draw #%ld", n);
    gate_gvpdraw(self, vp, cv);
}

__attribute__((naked)) static void gate_beginrvf(void* mod, void* cv, void* fam) {
    asm volatile(
        ".inst 0xd101c3ff\n"  // sub sp, sp, #0x70
        ".inst 0xa9016ffc\n"  // stp x28,x27,[sp,#0x10]
        ".inst 0xa90267fa\n"  // stp x26,x25,[sp,#0x20]
        ".inst 0xa9035ff8\n"  // stp x24,x23,[sp,#0x30]
        "adrp x16, :got:g_tgt_beginrvf\n"
        "ldr x16, [x16, :got_lo12:g_tgt_beginrvf]\n"
        "ldr x16, [x16]\n"
        "br x16\n");
}
void hook_beginrvf(void* mod, void* cv, void* fam) {
    static long n = 0;
    n++;
    if (n <= 10 || (n % 300) == 0) LOGI("BeginRenderingViewFamily #%ld canvas=%p fam=%p", n, cv, fam);
    gate_beginrvf(mod, cv, fam);
}

__attribute__((naked)) static void gate_calcsv(void* self, void* fam, void* loc, void* rot, void* vp, void* dr, int pass) {
    asm volatile(
        ".inst 0xfc190fe8\n"  // paciasp
        ".inst 0xa9016ffc\n"  // stp x28,x27,[sp,#0x10]
        ".inst 0xa90267fa\n"  // stp x26,x25,[sp,#0x20]
        ".inst 0xa9035ff8\n"  // stp x24,x23,[sp,#0x30]
        "adrp x16, :got:g_tgt_calcsv\n"
        "ldr x16, [x16, :got_lo12:g_tgt_calcsv]\n"
        "ldr x16, [x16]\n"
        "br x16\n");
}
void hook_calcsv(void* self, void* fam, void* loc, void* rot, void* vp, void* dr, int pass) {
    static long n = 0;
    n++;
    if (n <= 10 || (n % 300) == 0) LOGI("ULocalPlayer::CalcSceneView #%ld pass=%d", n, pass);
    gate_calcsv(self, fam, loc, rot, vp, dr, pass);
}

int hook_lfta(void* arr, const unsigned short* fn, int fl) {
    char n[160];
    u16_to_ascii(fn, n, 160);
    int r = (int)(long)gate_lfta(arr, fn, fl);
    if (strstr(n, "Global") || strstr(n, "Shader"))
        LOGI("LoadFileToArray(%s) = %d", n, r);
    return r;
}

// ---------- file open tracing via GOT ----------
static int (*r_openat)(int, const char*, int, ...);
static int h_openat(int dirfd, const char* path, int flags, ...) {
    int r = r_openat ? r_openat(dirfd, path, flags, 0) : -1;
    if (path && strstr(path, "ShaderArchive"))
        LOGI("openat(%s)=%d", path, r);
    return r;
}
static FILE* (*r_fopen)(const char*, const char*);
static FILE* h_fopen(const char* path, const char* mode) {
    FILE* r = r_fopen ? r_fopen(path, mode) : 0;
    if (path && (strstr(path, "Shader") || strstr(path, ".pak") || strstr(path, ".ini") || strstr(path, "VR4/")))
        LOGI("fopen(%s,%s)=%p", path, mode, r);
    return r;
}
static int (*r_open)(const char*, int, ...);
static int h_open(const char* path, int flags, ...) {
    int mode = 0;
    int r = r_open ? r_open(path, flags, mode) : -1;
    if (path && (strstr(path, "Shader") || strstr(path, ".pak")))
        LOGI("open(%s)=%d", path, r);
    return r;
}

static void* g_acq_queue;

int h_vkPresent(void* q, void* si, void* f) {
    g_acq_queue = q;
    if (g_present_count < 3) LOGI("PRS lr=%p", __builtin_return_address(0));
    unsigned char pbuf[64]; void* semslot = 0;
    const void* si2 = comp_present(q, si, pbuf, &semslot);
    int r = r_vkPresent ? r_vkPresent(q, (void*)si2, f) : 0;
    if (si) {
        unsigned nsc_ = *(const unsigned*)((const char*)si + 32);
        void* const* psc_ = *(void* const* const*)((const char*)si + 40);
        const unsigned* pix_ = *(const unsigned* const*)((const char*)si + 48);
        for (unsigned k = 0; k < nsc_ && psc_ && pix_; k++)
            if (psc_[k] == g_window_sc && pix_[k] < 32) g_acq_mask &= ~(1u << pix_[k]);
    }
    if (r == 1000001003) r = 0; // VK_SUBOPTIMAL_KHR -> hide: UE4's recreate path crashes on Adreno
    loading_present(r);
    g_present_count++;
    if (g_present_count <= 10 || (g_present_count % 100) == 0) {
        // VkPresentInfoKHR: waitSemaphoreCount@16, pWaitSemaphores@24, swapchainCount@32, pSwapchains@40, pImageIndices@48
        unsigned nsc = si ? *(const unsigned*)((const char*)si + 32) : 0;
        void* const* psc = si ? *(void* const* const*)((const char*)si + 40) : 0;
        const unsigned* pidx = si ? *(const unsigned* const*)((const char*)si + 48) : 0;
        LOGI("vkQueuePresentKHR #%ld -> %d nsc=%u sc0=%p idx0=%u (win_sc=%p)",
             g_present_count, r, nsc, (nsc && psc) ? psc[0] : 0, (nsc && pidx) ? pidx[0] : 9999, g_window_sc);
    }
    return r;
}

// REAL acquire passthrough. Window is visible+focused (verified via dumpsys):
// if a fresh process does real acquire + real present, BLAST should cycle
// buffers at vsync like any normal Vulkan app. Timeout capped at 200ms so a
// stalled compositor degrades into visible log lines instead of a silent hang;
// on timeout we honestly return VK_TIMEOUT (UE4 retries) and never fabricate
// an image index (fake idx + real present crashed Mali FPendingBarrier).
#define VK_TIMEOUT_VAL 5

static _Unwind_Reason_Code uw_cb(struct _Unwind_Context* ctx, void* arg) {
    void** ips = (void**)arg;
    unsigned long pc = (unsigned long)_Unwind_GetIP(ctx);
    if (pc == 0 || (unsigned long)ips[0] >= 15) return _URC_END_OF_STACK;
    ips[1 + (unsigned long)ips[0]] = (void*)pc;
    ips[0] = (void*)((unsigned long)ips[0] + 1);
    return _URC_NO_REASON;
}
static void log_stack(const char* tag) {
    void* ips[16] = {0};
    _Unwind_Backtrace(uw_cb, ips);
    for (unsigned long i = 1; i <= ips[0] && i < 16; i++)
        LOGI("%s[%lu]=%p", tag, i - 1, ips[i]);
}

int h_vkAcquire6(void* dev, void* sc, long long t, void* sem, void* fence, void* img) {
    if (!r_vkAcquire) { if (img) *(int*)img = 0; return 0; }
    // pass UE4's own timeout through: capping it once produced VK_TIMEOUT which
    // made UE4 recreate the swapchain and crash in FVulkanFramebuffer (null view)
    int r = r_vkAcquire(dev, sc, t, sem, fence, img);
    if ((r == 0 || r == 1000001003) && img && sc == g_window_sc) { int ix = *(int*)img; if (ix >= 0 && ix < 32) g_acq_mask |= (1u << ix); }
    g_acquire_count++;
    if (r != 0) g_to_count++;
    if (g_acquire_count <= 2) log_stack("ACQST");
    if (g_acquire_count <= 10 || (g_acquire_count % 100) == 0 ||
        (r != 0 && (g_to_count <= 20 || (g_to_count % 100) == 0)))
        LOGI("vkAcquireNextImage #%ld -> %d idx=%d (to=%d)", g_acquire_count, r,
             img ? *(int*)img : -1, g_to_count);
    return r;
}

static void* (*r_ovrpEndFrame)();
static long h_ovrpEndFrame(long a, long b, long c, long d, long e) {
    static long n = 0;
    if (++n == 1 || (n % 2000) == 1) LOGI("ovrp_EndFrame #%ld", n);
    return (long)(r_ovrpEndFrame ? r_ovrpEndFrame(a, b, c, d, e) : 0);
}

// ---------- vrapi wrappers (shadow the libvrapi.so stubs with logging) ----------
static long g_si_n, g_sf_n, g_pi_n, g_pf_n, g_wf_n, g_dt_n, g_sf2_n, g_pt2_n;

int w_GetSysInt(int status) {
    g_si_n++;
    if (g_si_n <= 40 || (g_si_n % 2000) == 0) LOGI("GetSystemStatusInt(%d)=1 #%ld", status, g_si_n);
    return 1;
}
float w_GetSysFloat(int status) {
    g_sf_n++;
    if (g_sf_n <= 20) LOGI("GetSystemStatusFloat(%d)=1.0", status);
    return 1.0f;
}
int w_GetPropInt(void* ctx, int prop, int def) {
    g_pi_n++;
    if (g_pi_n <= 40) LOGI("GetPropertyInt(%d) def=%d", prop, def);
    return def;
}
float w_GetPropFloat(void* ctx, int prop, float def) {
    g_pf_n++;
    if (g_pf_n <= 40) LOGI("GetPropertyFloat(%d) def=%f", prop, def);
    return def;
}
void w_WaitFrame(void* ctx, double t) {
    g_wf_n++;
    if (g_wf_n <= 5 || (g_wf_n % 300) == 0) LOGI("vrapi_WaitFrame #%ld t=%.4f", g_wf_n, t);
}
double w_GetPredTime(void* ctx, double t) {
    g_dt_n++;
    if (g_dt_n <= 5) LOGI("vrapi_GetPredictedDisplayTime(%.4f)=0.015625", t);
    return 0.015625;
}
void w_SubmitFrame2(void* ctx, const void* desc) {
    g_sf2_n++;
    if (g_sf2_n <= 5 || (g_sf2_n % 300) == 0)
        LOGI("vrapi_SubmitFrame2 #%ld layers=%d", g_sf2_n, desc ? *(const int*)((const char*)desc + 8) : -1);
}

static void fill_tracking2(float* buf);

void fill_tracking2_wrap(void* ctx, double t, float* buf) {
    (void)ctx; (void)t;
    g_pt2_n++;
    if (g_pt2_n <= 5 || (g_pt2_n % 300) == 0) LOGI("vrapi_GetPredictedTracking2 #%ld", g_pt2_n);
    fill_tracking2(buf);
}

// AAPCS64: large struct return -> x8 = hidden buffer; keep x0 (ctx) and d0 (time)
__attribute__((naked)) static void w_GetPredTracking2(void) {
    asm volatile(
        "stp x29, x30, [sp, #-16]!\n"
        "mov x29, sp\n"
        "mov x2, x8\n"
        "stp d8, d9, [sp, #-16]!\n"
        "bl fill_tracking2_wrap\n"
        "ldp d8, d9, [sp], #16\n"
        "ldp x29, x30, [sp], #16\n"
        "ret\n");
}

// ovrTracking2: { ovrPosef HeadPose(32); ovrMatrix4f Eye[2](128); ovrMatrix4f Projection[2](128) }
static void fill_tracking2(float* buf) {
    // identity quaternion + standing head position
    buf[0] = 0; buf[1] = 0; buf[2] = 0; buf[3] = 1;
    buf[4] = 0; buf[5] = 1.5f; buf[6] = 0; buf[7] = 0;
    // Eye[2]: column-major view matrices, eyes at +-0.032m, looking -Z
    for (int e = 0; e < 2; e++) {
        float* m = buf + 8 + e * 16;
        for (int i = 0; i < 16; i++) m[i] = 0;
        m[0] = 1; m[5] = 1; m[10] = 1; m[15] = 1;
        m[12] = (e ? 0.032f : -0.032f); m[13] = -1.5f; m[14] = 0;
    }
    // Projection[2]: 80deg fov, aspect 1, near .05 far 500, GL-style RH
    float fov = 80.0f * 3.14159265f / 360.0f;
    float f = 1.0f / tanf(fov);
    float nr = 0.05f, fr = 500.0f;
    for (int e = 0; e < 2; e++) {
        float* m = buf + 8 + 32 + e * 16;
        for (int i = 0; i < 16; i++) m[i] = 0;
        m[0] = f; m[5] = f; m[10] = (fr + nr) / (nr - fr); m[11] = -1;
        m[14] = 2 * fr * nr / (nr - fr);
    }
}

static void* h_dlsym(void* h, const char* name) {
    void* real = r_dlsym(h, name);
    void* image=comp_image_flags_proc(name,real);if(image!=real)return image;
    if (!name) return real;
    if (name[0] == 'o' && name[1] == 'v' && name[2] == 'r' && g_ovrp_log < 80) {
        g_ovrp_log++;
        LOGI("dlsym(%s)=%p", name, real);
    }
    if (name[0] == 'v' && strstr(name, "vrapi_") == name && g_vrapi_log < 80) {
        g_vrapi_log++;
        LOGI("dlsym(%s)=%p", name, real);
    }
    if (strstr(name, "vrapi_GetSystemStatusInt")) return (void*)&w_GetSysInt;
    if (strstr(name, "vrapi_GetSystemStatusFloat")) return (void*)&w_GetSysFloat;
    if (strstr(name, "vrapi_GetPropertyInt")) return (void*)&w_GetPropInt;
    if (strstr(name, "vrapi_GetPropertyFloat")) return (void*)&w_GetPropFloat;
    if (strstr(name, "vrapi_WaitFrame")) return (void*)&w_WaitFrame;
    if (strstr(name, "vrapi_GetPredictedDisplayTime")) return (void*)&w_GetPredTime;
    if (strstr(name, "vrapi_SubmitFrame2")) return (void*)&w_SubmitFrame2;
    if (strstr(name, "vrapi_GetPredictedTracking2")) return (void*)&w_GetPredTracking2;
    if (name[0] == 'o' && strstr(name, "ovrp_") == name) {
        void* d = ovrp_dispatch(name);
        if (d) {
            if (g_ovrp_log < 100) { g_ovrp_log++; LOGI("ovrp hook: %s", name); }
            return d;
        }
    }
    if (strstr(name, "ovrp_EndFrame")) { r_ovrpEndFrame = (void*(*)(long,long,long,long,long))real; return (void*)&h_ovrpEndFrame; }
    if (!strcmp(name, "vkQueuePresentKHR")) { r_vkPresent = (int(*)(void*,void*,void*))real; return (void*)&h_vkPresent; }
    if (!strcmp(name, "vkCreateSwapchainKHR") && real) { LOGI("dlsym(vkCreateSwapchainKHR) -> hook"); r_vkCreateSwapchain = (int (*)(void*, const void*, const void*, void**))real; return (void*)&h_vkCreateSwapchain; }
    if (!strcmp(name, "vkGetSwapchainImagesKHR") && real) { LOGI("dlsym(vkGetSwapchainImagesKHR) -> hook"); r_vkGetSCI = (int (*)(void*, void*, unsigned*, void**))real; return (void*)&h_vkGetSCI; }
    if (!strcmp(name, "vkCreateImageView") && real) { r_vkCreateImageView = (int (*)(void*, const void*, const void*, void**))real; return (void*)&h_vkCreateImageView; }
    if (!strcmp(name, "vkGetPhysicalDeviceSurfaceFormatsKHR") && real) { r_vkSurfaceFormats = real; return (void*)&h_vkSurfaceFormats; }
    if (!strcmp(name, "vkCreateAndroidSurfaceKHR") && real) { r_vkCreateAndroidSurface = real; return (void*)&h_vkCreateAndroidSurface; }
    if (!strcmp(name, "vkDestroyImageView") && real) { r_vkDestroyImageView = real; return (void*)&h_vkDestroyImageView; }
    if (!strcmp(name, "vkAcquireNextImageKHR")) { r_vkAcquire = (int(*)(void*,void*,long long,void*,void*,void*))real; return (void*)&h_vkAcquire6; }
    if (!strcmp(name, "vkQueueSubmit")) { r_vkQueueSubmit2 = (int(*)(void*,unsigned,const void*,void*))real; return (void*)&h_vkQueueSubmit; }
    if (!strcmp(name, "vkCreateInstance")) return (void*)&h_vkCreateInstance;
    if (!strcmp(name, "vkGetInstanceProcAddr")) return (void*)&h_vkGetInstanceProcAddr;
    if (!strcmp(name, "vkGetDeviceProcAddr")) return (void*)&h_vkGetDeviceProcAddr;
    return real;
}

static void* watch_shader(void* arg) {
    unsigned long base = ue4_base();
    if (!base) return 0;
    volatile int* plat = (volatile int*)(base + 0xa413238);
    volatile int* maxrhi = (volatile int*)(base + 0xa7507e0); // GMaxRHIShaderPlatform
    for (int i = 0; i < 400000; i++) {
        for (int k = 0; k < 4; k++) {
            int v = plat[k];
            if (v != 21 && v != 0 && v != -1) {
                plat[k] = 21;
                LOGI("shaderplat[%d]: %d -> 21", k, v);
            }
        }
        int mv = *maxrhi;
        if (mv != 21 && mv != 0 && mv != -1) {
            *maxrhi = 21;
            LOGI("GMaxRHIShaderPlatform: %d -> 21", mv);
        }
        volatile int* fl = (volatile int*)(base + 0xa413248);
        static int last_fl = -99;
        if (*fl != last_fl) { last_fl = *fl; LOGI("GMaxRHIFeatureLevel=%d", *fl); }
        usleep(250);
    }
    return 0;
}

// ---------- ovrp_* full replacement (OVRPlugin emulation) ----------
static long g_ovrp_n[96];

struct ovrp_default { const char* name; long long ret; };
static const struct ovrp_default g_ovrp_defaults[] = {
    {"ovrp_PreInitialize3", 1}, {"ovrp_GetInitialized", 1}, {"ovrp_Initialize5", 1},
    {"ovrp_Shutdown2", 1}, {"ovrp_GetDominantHand", 1},
    {"ovrp_GetTrackingOrientationSupported2", 1}, {"ovrp_GetTrackingOrientationEnabled2", 1},
    {"ovrp_GetTrackingPositionSupported2", 1}, {"ovrp_GetTrackingPositionEnabled2", 1},
    {"ovrp_GetControllerState4", 0}, {"ovrp_GetControllerState3", 0},
    {"ovrp_WaitToBeginFrame", 1}, {"ovrp_BeginFrame4", 1}, {"ovrp_EndFrame4", 1},
    {"ovrp_Update3", 1},
    {"ovrp_GetAppShouldQuit2", 0},
    {"ovrp_GetAppShouldRecreateDistortionWindow2", 0},
    {"ovrp_GetAppShouldPause", 0},
    {"ovrp_GetBoundaryVisible", 0},
    {0, 0}
};
static long long defret_for(const char* n) {
    for (int i = 0; g_ovrp_defaults[i].name; i++)
        if (strcmp(g_ovrp_defaults[i].name, n) == 0) return g_ovrp_defaults[i].ret;
    return 0;  // ovrpResult: success == 0!
}
#define MAXOVRP 96
static const char* g_ovrp_names[MAXOVRP];
static int g_ovrp_count;

int h5_ovrp_GetSessionStatus(void* s);
int h5_ovrp_GetSystemHeadsetTypeId(void);
void* h5_ovrp_GetVersion2(void);
int h5_ovrp_GetEyePixelsPerTanAngleAtCenter2(void* a, void* b);
int h5_ovrp_GetHmdToEyeOffset2(void* out);
void fill_layer_desc(void* out);
int h5_ovrp_SetupLayer(void* a, void* b, void* c);
int h5_ovrp_WaitToBeginFrame(long frameIndex);
int h5_ovrp_GetNodePoseState3(long a, long b, long c, long d);
long h_bool_true(long out) { if ((unsigned long)out > 0x10000) *(int*)out = 1; return 0; }
// ovrp_GetAppHasInputFocus: AVR4GameMode::UpdateHasInputFocus reads it every frame and, on loss, fades the sound out
// and calls its own SuspendForFocusLoss (game paused) - the Quest system-menu behaviour. The window focus state
// (UE app events, re4fix_pause.h) drives it; any new touch restores it as a failsafe.
static volatile int g_app_input_focus = 1;
long h_app_input_focus(long out) {
    if ((unsigned long)out > 0x10000) *(int*)out = __atomic_load_n(&g_app_input_focus, __ATOMIC_ACQUIRE) ? 1 : 0;
    return 0;
}
long h_node_bool_true(long node, long out) { (void)node; if ((unsigned long)out > 0x10000) *(int*)out = 1; return 0; }
int h5_ovrp_GetControllerState4(long ctrl, long state);
void fill_controller_state(unsigned char* p, unsigned mask);
int h5_ovrp_GetControllerState4(long ctrl, long state) {
    static long n=0; if(++n<=6||(n%5000)==0) LOGI("GetControllerState4(mask=%lx state=%p) #%ld",ctrl,(void*)state,n);
    if((unsigned long)state<=0x10000) return 0;
    fill_controller_state((unsigned char*)state,(unsigned)ctrl);
    return 0;
}
long h5_ovrp_GetControllerConnected4(long ctrl) { (void)ctrl; return 1; }
long h5_ovrp_EnumerateInputDevices(long cnt) { if ((unsigned long)cnt > 0x10000) *(int*)cnt = 2; return 2; }
long h_bool_false(long out) { if ((unsigned long)out > 0x10000) *(int*)out = 0; return 0; }
long h_bool_true_x1(long a, long out) { if ((unsigned long)out > 0x10000) *(int*)out = 1; (void)a; return 0; }
long h_ret1(void) { return 1; }  // direct ovrpBool return, no out param
int h5_ovrp_GetLayerTexture2(void* layer, int stage, int eye, void* out);
int h5_ovrp_GetLayerTextureStageCount2(long layerId, int* out);
int h5_calc_eye_desc(long layout, float scale, long mips, long samples, long fmt, long dfmt, long flags, unsigned* desc);

#define OVRP_GEN(IDX) long h_ovrp_##IDX(long a, long b, long c, long d, long e, long f) { if (g_ovrp_n[IDX] < 3 || (g_ovrp_n[IDX] % 500) == 0) LOGI("ovrp %s #%ld (%lx %lx %lx)", g_ovrp_names[IDX], g_ovrp_n[IDX] + 1, a, b, c); g_ovrp_n[IDX]++; return (long)defret_for(g_ovrp_names[IDX]); }
OVRP_GEN(0) OVRP_GEN(1) OVRP_GEN(2) OVRP_GEN(3) OVRP_GEN(4) OVRP_GEN(5) OVRP_GEN(6)
OVRP_GEN(7) OVRP_GEN(8) OVRP_GEN(9) OVRP_GEN(10) OVRP_GEN(11) OVRP_GEN(12)
OVRP_GEN(13) OVRP_GEN(14) OVRP_GEN(15) OVRP_GEN(16) OVRP_GEN(17) OVRP_GEN(18)
OVRP_GEN(19) OVRP_GEN(20) OVRP_GEN(21) OVRP_GEN(22) OVRP_GEN(23) OVRP_GEN(24)
OVRP_GEN(25) OVRP_GEN(26) OVRP_GEN(27) OVRP_GEN(28) OVRP_GEN(29) OVRP_GEN(30)
OVRP_GEN(31) OVRP_GEN(32) OVRP_GEN(33) OVRP_GEN(34) OVRP_GEN(35) OVRP_GEN(36)
OVRP_GEN(37) OVRP_GEN(38) OVRP_GEN(39) OVRP_GEN(40) OVRP_GEN(41) OVRP_GEN(42)
OVRP_GEN(43) OVRP_GEN(44) OVRP_GEN(45) OVRP_GEN(46) OVRP_GEN(47) OVRP_GEN(48)
OVRP_GEN(49) OVRP_GEN(50) OVRP_GEN(51) OVRP_GEN(52) OVRP_GEN(53) OVRP_GEN(54)
OVRP_GEN(55) OVRP_GEN(56) OVRP_GEN(57) OVRP_GEN(58) OVRP_GEN(59) OVRP_GEN(60)
OVRP_GEN(61) OVRP_GEN(62) OVRP_GEN(63) OVRP_GEN(64) OVRP_GEN(65) OVRP_GEN(66)
OVRP_GEN(67) OVRP_GEN(68) OVRP_GEN(69) OVRP_GEN(70) OVRP_GEN(71) OVRP_GEN(72)
OVRP_GEN(73) OVRP_GEN(74) OVRP_GEN(75) OVRP_GEN(76) OVRP_GEN(77) OVRP_GEN(78)
OVRP_GEN(79) OVRP_GEN(80) OVRP_GEN(81) OVRP_GEN(82) OVRP_GEN(83) OVRP_GEN(84)
OVRP_GEN(85) OVRP_GEN(86) OVRP_GEN(87) OVRP_GEN(88) OVRP_GEN(89) OVRP_GEN(90)
OVRP_GEN(91) OVRP_GEN(92) OVRP_GEN(93) OVRP_GEN(94) OVRP_GEN(95)

int h5_ovrp_WaitToBeginFrame(long frameIndex) {
    static long n = 0;
    n++;
    // real plugin blocks until vsync; pace: "not ready" for first 120 polls (~2s at 60hz)
    int ready = n > 120;
    if (n <= 5 || (n % 500) == 0) LOGI("WaitToBeginFrame #%ld -> %d", n, ready);
    return ready;
}

// ovrpPoseStatef (88 B, verified from FOculusInput::GetControllerOrientationAndPosition @0x5c51f64 which hands
// sp+0 straight to ConvertPose_Internal(const ovrpPosef&)): Pose{Orientation xyzw @0, Position xyz @16},
// Velocity @28, Acceleration @40, AngularVelocity @52, AngularAcceleration @64, double Time @80.
// OVR frame: +x right, +y up, -z forward. ovrpNode: EyeL 0, EyeR 1, EyeCenter 2, HandL 3, HandR 4, Head 9.
typedef struct { float x, y, z, w; } qf_t;
static qf_t q_mul(qf_t a, qf_t b) {
    qf_t r;
    r.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
    r.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    r.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
    r.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
    return r;
}
static void q_rot(qf_t q, const float v[3], float o[3]) {
    float d = q.x * v[0] + q.y * v[1] + q.z * v[2], uu = q.x * q.x + q.y * q.y + q.z * q.z, k = q.w * q.w - uu;
    float cx = q.y * v[2] - q.z * v[1], cy = q.z * v[0] - q.x * v[2], cz = q.x * v[1] - q.y * v[0];
    o[0] = 2 * d * q.x + k * v[0] + 2 * q.w * cx;
    o[1] = 2 * d * q.y + k * v[1] + 2 * q.w * cy;
    o[2] = 2 * d * q.z + k * v[2] + 2 * q.w * cz;
}
static float g_eye_height = 1.60f;                                   // metres above the tracking origin
static const float g_hand_off[2][3] = { {-0.20f, -0.30f, -0.30f},    // left hand, head-relative
                                        { 0.20f, -0.26f, -0.36f} };  // right hand, head-relative
#include "re4fix_mobile_view.h"
#include "re4fix_weapon_pose.h"
static void head_angles_now(float* yaw, float* pitch);
// One look sample per native GameThread frame, including repeated head/hand queries and slow frames.
// The OVR frame argument is always zero here; the engine's real GFrameCounter owns the boundary.
static pthread_mutex_t g_pose_latch_lock = PTHREAD_MUTEX_INITIALIZER;
static long g_pose_latch_ns;
static unsigned g_pose_latch_served;
static unsigned long long g_pose_latch_frame;
static float g_pose_latch_yaw, g_pose_latch_pitch, g_pose_prev_yaw, g_pose_prev_pitch;
static volatile int g_pose_latch_on = 1, g_pose_trace;
static void pose_angles(long node, float* yaw, float* pitch) {
    if (!g_pose_latch_on) { head_angles_now(yaw, pitch); return; }
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    long now = ts.tv_sec * 1000000000L + ts.tv_nsec;
    unsigned bit = node >= 0 && node < 32 ? 1u << node : 0;
    pthread_mutex_lock(&g_pose_latch_lock);
    unsigned long long frame = g_pose_frame_counter ? __atomic_load_n(g_pose_frame_counter,__ATOMIC_ACQUIRE) : 0;
    if (!g_pose_latch_ns || (g_pose_frame_counter ? frame != g_pose_latch_frame
        : (g_pose_latch_served & bit) || now - g_pose_latch_ns > 10000000L || now < g_pose_latch_ns)) {
        g_pose_latch_ns = now; g_pose_latch_served = 0;
        g_pose_latch_frame = frame;
        g_pose_prev_yaw = g_pose_latch_yaw; g_pose_prev_pitch = g_pose_latch_pitch;
        head_angles_now(&g_pose_latch_yaw, &g_pose_latch_pitch);
    }
    g_pose_latch_served |= bit;
    *yaw = g_pose_latch_yaw; *pitch = g_pose_latch_pitch;
    pthread_mutex_unlock(&g_pose_latch_lock);
}
// The pause case is authored for a wide VR field of view: its side doors (option descriptions, Accept/Reset)
// spill past a 90 degree phone view. Step the view back so the whole case fits.
static volatile float g_pause_back = 0.14f;
// During game-thread ticks the camera component still holds the previous frame's HMD rotation (GetCameraView
// applies the new pose after the ticks; measured: camYaw(N) + latchYaw(N-1) == const). Anything placed relative
// to the camera in a tick must apply this delta (UE frame, right-multiplied) or it trails the view by one frame.
static qf_t pose_camera_delta_ue(void) {
    qf_t identity = {0, 0, 0, 1};
    if (!g_pose_latch_on) return identity;
    pthread_mutex_lock(&g_pose_latch_lock);
    float y0 = g_pose_prev_yaw, p0 = g_pose_prev_pitch, y1 = g_pose_latch_yaw, p1 = g_pose_latch_pitch;
    pthread_mutex_unlock(&g_pose_latch_lock);
    qf_t a = q_mul((qf_t){0, sinf(y0 * 0.5f), 0, cosf(y0 * 0.5f)}, (qf_t){sinf(p0 * 0.5f), 0, 0, cosf(p0 * 0.5f)});
    qf_t b = q_mul((qf_t){0, sinf(y1 * 0.5f), 0, cosf(y1 * 0.5f)}, (qf_t){sinf(p1 * 0.5f), 0, 0, cosf(p1 * 0.5f)});
    qf_t d = q_mul((qf_t){-a.x, -a.y, -a.z, a.w}, b);
    qf_t ue = {-d.z, d.x, d.y, -d.w};   // OculusHMD ToFQuat: OVR (x right, y up, -z fwd) -> UE (x fwd, y right, z up)
    return ue;
}
static void head_pose_node(long node, qf_t* q, float pos[3]) {
    float y, pt;
    pose_angles(node, &y, &pt);
    qf_t qy = {0, sinf(y * 0.5f), 0, cosf(y * 0.5f)}, qp = {sinf(pt * 0.5f), 0, 0, cosf(pt * 0.5f)};
    *q = q_mul(qy, qp);
    pos[0] = 0; pos[1] = g_eye_height; pos[2] = 0;
    if (g_touch_context == 2 || g_touch_context == 8) {
        float back[3] = {0, 0, g_pause_back}, w[3];
        q_rot(qy, back, w); pos[0] += w[0]; pos[2] += w[2];
    }
}
// Quick 180-degree turn: an eased yaw offset owned by the pose (GameThread) path. A finished turn is folded into
// g_yaw_turned, which only this path writes, so touch writers of g_look_yaw never race with it.
static float g_yaw_turned, g_qturn_progress;
static long g_qturn_start, g_qturn_seen;
// Wrist-watch view weight: 220 ms up, held until the deadline, 220 ms down (smoothstep); gameplay only.
static float watch_pose_weight(long now) {
    long since = __atomic_load_n(&g_watch_since_ms, __ATOMIC_ACQUIRE), until = __atomic_load_n(&g_watch_until_ms, __ATOMIC_ACQUIRE);
    if (!since || now < since || g_touch_context != 1) return 0;
    long raised_until = now < until ? now : until;
    float up = fmaxf(0, fminf((float)(raised_until - since) / 220.0f, 1.0f));
    float down = fmaxf(0, fminf(1.0f - (float)(now > until ? now - until : 0) / 220.0f, 1.0f));
    return up * up * (3.0f - 2.0f * up) * down * down * (3.0f - 2.0f * down);
}
static float quick_turn_offset(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    long now = ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
    long request = __atomic_load_n(&g_qturn_request_ms, __ATOMIC_ACQUIRE);
    if (request && request != g_qturn_seen) {
        g_qturn_seen = request;
        if (!g_qturn_start && now - request < 400 && g_touch_context == 1) { g_qturn_start = now; g_qturn_progress = 0; }
    }
    if (!g_qturn_start) return 0;
    if (g_touch_context != 1) { g_qturn_start = 0; return 0; }
    float t = (float)(now - g_qturn_start) / 220.0f;
    if (t >= 1.0f) {
        g_yaw_turned += 3.14159265f;
        if (g_yaw_turned > 3.14159265f) g_yaw_turned -= 6.2831853f;
        g_qturn_start = 0;
        return 0;
    }
    if (t < 0) t = 0;
    return 3.14159265f * t * t * (3.0f - 2.0f * t);
}
static void head_angles_now(float* yaw, float* pitch) {
    float y = g_look_yaw + g_yaw_turned + quick_turn_offset(), pt = mobile_view_pitch(g_touch_context,g_look_pitch);
    if(g_touch_context==4 || (g_touch_context==7 && ptr_now_ms()-g_lobby_menu_ms<500))y=0;
    if (g_menu_view_active) {
        struct timespec now_ts; clock_gettime(CLOCK_MONOTONIC, &now_ts);
        long now_ms_value = now_ts.tv_sec * 1000L + now_ts.tv_nsec / 1000000L;
        if (now_ms_value >= g_menu_view_ms && now_ms_value - g_menu_view_ms < 300
            && (g_touch_context == 1 || g_touch_context == 4 || g_touch_context == 6 || g_touch_context == 7 || g_touch_context == 8)) {
            y += g_menu_yaw_off; pt = g_menu_pitch;
        }
    }
    *yaw = y; *pitch = pt;
}
static long ptr_now_ms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L; }
static int ptr_active(void) { return (g_touch_context == 5 || (g_touch_context != 3 && ptr_now_ms() - g_menu_tick_ms < 300) || !g_touch_weapon) && (g_ptr_down > 0 || ptr_now_ms() < g_ptr_until_ms); }
static int ptr_menu_now(void) { return g_touch_context == 5 || (g_touch_context != 3 && ptr_now_ms() - g_menu_tick_ms < 300); }
static int touch_menu_pointer_allowed(int node) {
    return node==4 && ptr_active() && !(touch_grenade_owned_grip_mask()&0x08000000u);
}
int h5_ovrp_GetNodePoseState3(long step, long frame, long node, long out) {
    static long n = 0;
    if (++n <= 3) LOGF("GetNodePoseState3(step=%ld frame=%ld node=%ld out=%lx)", step, frame, node, out);
    if ((unsigned long)out <= 0x10000) return -1000;
    if (node >= 0 && node < 12) g_pose_calls[node]++;
    qf_t q; float hp[3], pos[3];
    head_pose_node(node, &q, hp);
    if (g_pose_trace > 0) {
        __atomic_sub_fetch(&g_pose_trace, 1, __ATOMIC_RELAXED);
        float a = 2.0f * atan2f(q.y, q.w) * 57.29578f;
        LOGF("POSETRACE tid=%d step=%d frame=%d node=%ld yaw=%.3f look=%.3f", (int)syscall(SYS_gettid),
             (int)step, (int)frame, node, a, g_look_yaw * 57.29578f);
    }
    pos[0] = hp[0]; pos[1] = hp[1]; pos[2] = hp[2];
    if (node == 0 || node == 1) {
        float e[3] = {node == 0 ? -0.032f : 0.032f, 0, 0}, w[3];
        q_rot(q, e, w); pos[0] += w[0]; pos[1] += w[1]; pos[2] += w[2];
    } else if (touch_menu_pointer_allowed((int)node)) {
        float W = g_comp_w ? (float)g_comp_w : 2167.0f, H = g_comp_h ? (float)g_comp_h : 1080.0f;
        float th = tanf(g_fov_h_deg * 0.5f * 0.01745329f), tv = th * H / W;
        float d[3] = {(g_ptr_u * 2.0f - 1.0f) * th, (1.0f - g_ptr_v * 2.0f) * tv, -1.0f};
        float m = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]); d[0] /= m; d[1] /= m; d[2] /= m;
        float a = atan2f(-d[0], -d[2]), b = asinf(d[1]);
        qf_t qa = {0, sinf(a * 0.5f), 0, cosf(a * 0.5f)}, qb = {sinf(b * 0.5f), 0, 0, cosf(b * 0.5f)};
        float back[3] = {-d[0] * 0.35f, -d[1] * 0.35f, -d[2] * 0.35f}, w[3];
        q_rot(q, back, w); pos[0] += w[0]; pos[1] += w[1]; pos[2] += w[2];
        q = q_mul(q, q_mul(qa, qb));
    } else if ((node == 3 || node == 4) && ptr_menu_now() && g_touch_context != 5 && !g_hand_ovr_on[node - 3]) {
        // Idle menu hands park below/behind the view and point backward: no stray laser hover/selection.
        float off[3] = {node == 3 ? -0.15f : 0.15f, -0.55f, 0.30f}, w[3];
        q_rot(q, off, w); pos[0] += w[0]; pos[1] += w[1]; pos[2] += w[2];
        qf_t back = {0, 1, 0, 0};
        q = q_mul(q, back);
    } else if (node == 3 || node == 4) {
        int hi = node - 3;
        float off[3] = {g_hand_off[hi][0], g_hand_off[hi][1], g_hand_off[hi][2]}, w[3];
        qf_t position_q = q;
        if (hi == 0 && g_touch_context == 5) {
            float approach=mobile_phone_approach(ptr_now_ms(),
                __atomic_load_n(&g_phone_zoom_start,__ATOMIC_ACQUIRE));
            float wq[4];
            mobile_phone_pose(approach,off,wq);
            qf_t wrist = {wq[0], wq[1], wq[2], wq[3]};
            q = q_mul(q, wrist);
        }
        int primary_side=touch_weapon_primary_side(ptr_now_ms());
        if (hi == primary_side && g_touch_weapon == 2) { off[0] = 0; off[1] = -0.14f; off[2] = -0.42f; }
        if (hi == primary_side && g_touch_context == 1 && g_touch_weapon == 3) {
            off[0] = hi ? 0.22f : -0.22f; off[1] = -0.12f; off[2] = -0.40f;
        }
        int knife_hand = hi == primary_side && g_touch_context == 1 && g_touch_weapon == 1;
        if (knife_hand) {
            long now = ptr_now_ms();
            long slash = g_anim_kind == 2 ? now - g_anim_t0 : -1;
            struct wpose kp = knife_pose(slash, now);
            off[0] = hi ? kp.x : -kp.x; off[1] = kp.y; off[2] = kp.z;
            q = q_mul(q, q_euler_deg(hi ? kp.yaw : -kp.yaw, kp.pitch, hi ? kp.roll : -kp.roll));
        }
        if(g_touch_context==1 && hi==touch_grenade_selected_side(ptr_now_ms())) {
            float aspect=g_comp_w && g_comp_h ? (float)g_comp_w/(float)g_comp_h:2400.0f/1080.0f;
            grenade_held_pose(hi,g_fov_h_deg,aspect,off);
        }
        if (g_hand_ovr_on[hi]) { off[0] = g_hand_ovr[hi][0]; off[1] = g_hand_ovr[hi][1]; off[2] = g_hand_ovr[hi][2]; }
        if (hi == primary_side && g_anim_kind) {
            long el = ptr_now_ms() - g_anim_t0;
            if (el >= 0 && el < g_anim_len) {
                if (g_anim_kind == 2) { if (!knife_hand) off[1] += knife_cut_offset(el); }
                else {
                    float s = sinf((float)el * 6.2831853f / g_anim_period);
                    off[0] += g_anim_amp * s;
                    qf_t wrist = {0, sinf(0.65f * s), 0, cosf(0.65f * s)};
                    q = q_mul(q, wrist);
                }
            }
            else if (g_anim_kind != 2) g_anim_kind = 0;
        }
        touch_qte_pose(hi,ptr_now_ms(),off);
        (void)touch_grenade_pose(hi,ptr_now_ms(),off);
        touch_throw_pose(hi,ptr_now_ms(),off);
        if (hi == 0) {   // wrist watch: the left forearm comes up with the watch face toward the view
            float w = watch_pose_weight(ptr_now_ms());
            if (w > 0) {
                static const float watch_off[3] = {-0.25f, 0.065f, -0.40f};
                for (int k = 0; k < 3; k++) off[k] += (watch_off[k] - off[k]) * w;
                q = q_mul(q, q_euler_deg(0, 75.0f * w, -90.0f * w));
            }
        }
        if (g_hand_rot_ovr_on[hi]) q = q_mul(q, q_euler_deg(g_hand_rot_ovr[hi][0], g_hand_rot_ovr[hi][1], g_hand_rot_ovr[hi][2]));
        touch_weapon_cycle_pose(hi,ptr_now_ms(),off);
        q_rot(position_q, off, w); pos[0] += w[0]; pos[1] += w[1]; pos[2] += w[2];
        (void)touch_mirror_pose(hi,ptr_now_ms(),pos);
    }
    unsigned char* raw = (unsigned char*)out;
    memset(raw, 0, 88);
    float* f = (float*)raw;
    f[0] = q.x; f[1] = q.y; f[2] = q.z; f[3] = q.w;
    f[4] = pos[0]; f[5] = pos[1]; f[6] = pos[2];
    if (node >= 0 && node < 12) {   // Velocity @28 from the last sample at least 8 ms old
        static float vpos[12][3], vvel[12][3]; static long vms[12];
        long nm = ptr_now_ms(), dt = nm - vms[node];
        if (dt >= 8) {
            if (dt < 200) for (int k = 0; k < 3; k++) vvel[node][k] = (pos[k] - vpos[node][k]) * 1000.0f / (float)dt;
            else vvel[node][0] = vvel[node][1] = vvel[node][2] = 0;
            vpos[node][0] = pos[0]; vpos[node][1] = pos[1]; vpos[node][2] = pos[2]; vms[node] = nm;
        }
        f[7] = vvel[node][0]; f[8] = vvel[node][1]; f[9] = vvel[node][2];
    }
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    *(double*)(raw + 80) = (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
    return 0;
}

static void* (*g_ovrp_thunks[MAXOVRP])(long, long, long, long, long, long);
static void ovrp_thunk_init() {
    void* t[MAXOVRP] = {(void*)h_ovrp_0,(void*)h_ovrp_1,(void*)h_ovrp_2,(void*)h_ovrp_3,(void*)h_ovrp_4,(void*)h_ovrp_5,(void*)h_ovrp_6,(void*)h_ovrp_7,(void*)h_ovrp_8,(void*)h_ovrp_9,(void*)h_ovrp_10,(void*)h_ovrp_11,(void*)h_ovrp_12,(void*)h_ovrp_13,(void*)h_ovrp_14,(void*)h_ovrp_15,(void*)h_ovrp_16,(void*)h_ovrp_17,(void*)h_ovrp_18,(void*)h_ovrp_19,(void*)h_ovrp_20,(void*)h_ovrp_21,(void*)h_ovrp_22,(void*)h_ovrp_23,(void*)h_ovrp_24,(void*)h_ovrp_25,(void*)h_ovrp_26,(void*)h_ovrp_27,(void*)h_ovrp_28,(void*)h_ovrp_29,(void*)h_ovrp_30,(void*)h_ovrp_31,(void*)h_ovrp_32,(void*)h_ovrp_33,(void*)h_ovrp_34,(void*)h_ovrp_35,(void*)h_ovrp_36,(void*)h_ovrp_37,(void*)h_ovrp_38,(void*)h_ovrp_39,(void*)h_ovrp_40,(void*)h_ovrp_41,(void*)h_ovrp_42,(void*)h_ovrp_43,(void*)h_ovrp_44,(void*)h_ovrp_45,(void*)h_ovrp_46,(void*)h_ovrp_47,(void*)h_ovrp_48,(void*)h_ovrp_49,(void*)h_ovrp_50,(void*)h_ovrp_51,(void*)h_ovrp_52,(void*)h_ovrp_53,(void*)h_ovrp_54,(void*)h_ovrp_55,(void*)h_ovrp_56,(void*)h_ovrp_57,(void*)h_ovrp_58,(void*)h_ovrp_59,(void*)h_ovrp_60,(void*)h_ovrp_61,(void*)h_ovrp_62,(void*)h_ovrp_63,(void*)h_ovrp_64,(void*)h_ovrp_65,(void*)h_ovrp_66,(void*)h_ovrp_67,(void*)h_ovrp_68,(void*)h_ovrp_69,(void*)h_ovrp_70,(void*)h_ovrp_71,(void*)h_ovrp_72,(void*)h_ovrp_73,(void*)h_ovrp_74,(void*)h_ovrp_75,(void*)h_ovrp_76,(void*)h_ovrp_77,(void*)h_ovrp_78,(void*)h_ovrp_79,(void*)h_ovrp_80,(void*)h_ovrp_81,(void*)h_ovrp_82,(void*)h_ovrp_83,(void*)h_ovrp_84,(void*)h_ovrp_85,(void*)h_ovrp_86,(void*)h_ovrp_87,(void*)h_ovrp_88,(void*)h_ovrp_89,(void*)h_ovrp_90,(void*)h_ovrp_91,(void*)h_ovrp_92,(void*)h_ovrp_93,(void*)h_ovrp_94,(void*)h_ovrp_95};
    for (int i = 0; i < MAXOVRP; i++) g_ovrp_thunks[i] = t[i];
}

static void* ovrp_dispatch(const char* name) {
    if (strstr(name, "GetSessionStatus")) return (void*)&h5_ovrp_GetSessionStatus;
    if (strstr(name, "GetSystemHeadsetTypeId")) return (void*)&h5_ovrp_GetSystemHeadsetTypeId;
    if (strstr(name, "GetVersion2")) return (void*)&h5_ovrp_GetVersion2;
    if (strstr(name, "GetNativeSDKVersion2")) return (void*)&h5_ovrp_GetVersion2;
    if (strstr(name, "GetEyePixelsPerTanAngleAtCenter")) return (void*)&h5_ovrp_GetEyePixelsPerTanAngleAtCenter2;
    if (strstr(name, "GetHmdToEyeOffset")) return (void*)&h5_ovrp_GetHmdToEyeOffset2;
    if (strstr(name, "WaitToBeginFrame")) return (void*)&h5_ovrp_WaitToBeginFrame;
    if (strstr(name, "GetNodePresent")) return (void*)&h_node_bool_true;
    if (strstr(name, "GetNodePoseState3")) return (void*)&h5_ovrp_GetNodePoseState3;
    if (strstr(name, "GetControllerState4")) return (void*)&h5_ovrp_GetControllerState4;
    if (strstr(name, "GetControllerConnected4")) return (void*)&h5_ovrp_GetControllerConnected4;
    if (strstr(name, "EnumerateInputDevices")) return (void*)&h5_ovrp_EnumerateInputDevices;
    if (strstr(name, "GetAppShouldQuit")) return (void*)&h_bool_false;
    if (strstr(name, "GetAppShouldRecenter")) return (void*)&h_bool_false;
    if (strstr(name, "GetAppShouldPause")) return (void*)&h_bool_false;
    if (strstr(name, "RecreateDistortionWindow")) return (void*)&h_bool_false;
    if (strstr(name, "GetUserPresent")) return (void*)&h_bool_true;
    if (strstr(name, "GetAppHasVrFocus")) return (void*)&h_bool_true;
    if (strstr(name, "GetAppHasInputFocus")) return (void*)&h_app_input_focus;
    if (strstr(name, "GetNodePositionValid")) return (void*)&h_node_bool_true;
    if (strstr(name, "GetNodeOrientationValid")) return (void*)&h_node_bool_true;
    if (strstr(name, "GetNodePositionTracked")) return (void*)&h_node_bool_true;
    if (strstr(name, "GetNodeOrientationTracked")) return (void*)&h_node_bool_true;
    if (strstr(name, "GetTrackingOriginSupported")) return (void*)&h_ret1;
    if (strstr(name, "GetInitialized")) return (void*)&h_ret1;   // ROOT FIX: no-arg ovrpBool; h_bool_true wrote 1 into x0(=FOculusHMD) and smashed its vtable every frame
void h5_ovrp_CalculateEyeLayerDesc2_fn(void);
    if (strstr(name, "CalculateEyeLayerDesc")) return (void*)&h5_calc_eye_desc;
    if (strstr(name, "SetupLayer")) return (void*)&h5_ovrp_SetupLayer;
    if (strstr(name, "GetLayerTexture2")) return (void*)&h5_ovrp_GetLayerTexture2;
    if (strstr(name, "GetLayerTextureStageCount")) return (void*)&h5_ovrp_GetLayerTextureStageCount2;
    if (strstr(name, "SetControllerVibration2")) return (void*)&h5_ovrp_SetControllerVibration2;
    if (strstr(name, "SetControllerHaptics2")) return (void*)&h5_ovrp_SetControllerHaptics2;
    if (g_ovrp_count < MAXOVRP) {
        g_ovrp_names[g_ovrp_count] = strdup(name);
        g_ovrp_count++;
        return g_ovrp_thunks[g_ovrp_count - 1];
    }
    return 0;
}

static const char g_ovrp_ver[] = "1.49.0";
static const char g_ovrp_sdk[] = "1.49.0";
int h5_ovrp_GetSessionStatus(void* s) {
    static long n = 0;
    if (++n <= 3 || (n % 2000) == 0) LOGI("ovrp_GetSessionStatus #%ld -> all-good", n);
    if (s) {
        // ovrpSessionStatus bools: HMDPresent..UserPresent(0-6)=true,
        // DisplayLost/ShouldQuit/ShouldRecenter(7-9)=false, HasInputFocus(10+)=true
        unsigned char* p = (unsigned char*)s;
        for (int i = 0; i < 24; i++) p[i] = (i >= 7 && i <= 9) ? 0 : 1;
    }
    return 1;
}
int h5_ovrp_GetSystemHeadsetTypeId(void) {
    static long n = 0;
    if (++n <= 2) LOGI("GetSystemHeadsetTypeId -> QUEST2");
    return 0x305;
}
void* h5_ovrp_GetVersion2(void) { return (void*)g_ovrp_ver; }
int h5_ovrp_GetEyePixelsPerTanAngleAtCenter2(void* a, void* b) {
    float* f = (float*)b;
    if (f) { f[0] = 800.0f; f[1] = 800.0f; f[2] = 800.0f; f[3] = 800.0f; }
    static long n = 0;
    if (++n <= 3) LOGI("GetEyePixelsPerTanAngleAtCenter -> 800s");
    return 1;
}
int h5_ovrp_GetHmdToEyeOffset2(void* out) {
    float* f = (float*)out;
    if (f) { f[0] = -0.032f; f[1] = 0; f[2] = 0; f[3] = 0.032f; f[4] = 0; f[5] = 0; }
    static long n = 0;
    if (++n <= 3) LOGI("GetHmdToEyeOffset -> +-0.032");
    return 1;
}
// ovrpLayerDesc returned via hidden sret (x8): fill EyeFov desc, StageCount=1
__attribute__((naked)) void h5_ovrp_CalculateEyeLayerDesc2_fn(void) {
    asm volatile(
        "mov x0, x8\n"
        "adrp x16, :got:g_iel_done\n"
        "b fill_layer_desc\n"
        );
}
void fill_layer_desc(void* out);
void fill_layer_desc(void* out) {
    static long n = 0;
    if (++n <= 3) LOGI("CalcEyeLayerDesc: sret=%p -> fill StageCount=1", out);
    if ((unsigned long)out <= 0x10000) return;
    unsigned* u = (unsigned*)out;
    u[0] = 2;   // ovrpLayerDescType_EyeFov
    u[1] = 10;  // format R8G8B8A8
    u[2] = 1320; u[3] = 1264;
    u[4] = 1;   // mip levels
    u[5] = 1;   // sample count
    u[6] = 0; u[7] = 0; u[8] = 0;
    u[9] = 1;   // StageCount = 1
}
int h5_ovrp_SetupLayer(void* a, void* b, void* c) {
    static long n = 0;
    if (++n <= 8) {
        LOGI("ovrp_SetupLayer #%ld (%p,%p,%p) -> id 0x11", n, a, b, c);
        if ((unsigned long)b > 0x10000) { unsigned* d = (unsigned*)b;
            LOGI("  layerdesc: shape=%u layout=%u size=%ux%u mips=%u samp=%u fmt=%u flags=%x", d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7]); }
    }
    if ((unsigned long)c > 0x10000) *(unsigned*)c = 0x11;
    return 1;
}
int h5_ovrp_GetLayerTexture2(void* layer, int stage, int eye, void* out) {
    static long n = 0;
    if (++n <= 5 || (n % 500) == 0)
        LOGI("ovrp_GetLayerTexture2 #%ld layer=%p stage=%d eye=%d out=%p", n, layer, stage, eye, out);
    // the plugin stores *(void**)out directly as its TArray element and later
    // passes it to RHICreateTexture2DFromResource as the VkImage handle
    if (out && g_win_images && g_win_img_count) {
        static long imglog = 0;
        if (++imglog <= 4)
            LOGI("GLT2 stage=%d -> VkImage=%p (arr0=%p)", stage,
                 g_win_images[stage % g_win_img_count], g_win_images[0]);
        static unsigned long texbuf[8][8];
        int st = stage & 7;
        unsigned* u = (unsigned*)texbuf[st];
        u[0] = 5;
        u[1] = 0;
        u[2] = 1320; u[3] = 1264; u[4] = 1; u[5] = 1;
        unsigned long* q = (unsigned long*)((char*)u + 24);
        q[0] = (unsigned long)g_win_images[stage % g_win_img_count];
        q[1] = (unsigned long)g_vk_device;
        *(void**)out = (void*)(unsigned long)g_win_images[stage % g_win_img_count];
    }
    return 2;
}
// ovrpResult ovrp_GetLayerTextureStageCount(int layerId, int* outCount): the old stub never wrote
// *outCount, so FLayer's TextureCount was stack garbage -> 0 at the ~15s layer re-init -> empty
// texture array -> null aliased texture -> FXRSwapChain/Render null cascade.
int h5_ovrp_GetLayerTextureStageCount2(long layerId, int* out) {
    static long n = 0;
    int was = ((unsigned long)out > 0x10000) ? *out : -99999;
    if ((unsigned long)out > 0x10000) *out = 1;
    if (++n <= 12 || (n % 200) == 0) LOGI("StageCount(layer=%ld out=%p was=%d) -> 1 #%ld", layerId, (void*)out, was, n);
    return 0;
}
// ovrp_CalculateEyeLayerDesc2(layout, float scale, mips, samples, fmt, depthFmt, flags, ovrpLayerDesc_EyeFov* desc):
// desc is x6 (float scale goes in s0). The desc used to be left as stack garbage: FLayer then wrapped the
// window image with garbage NumMips/NumSamples (Mali's vkCreateImageView zero-fills per-level state for
// ~2^28 levels -> 7 GB and a SIGKILL) and a bogus depth swapchain (D24S8 view of a colour image).
// FLayer::SetEyeLayerDesc copies 0x7c bytes; bHasDepth = DepthFormat(+0x68) != 10 (ovrpTextureFormat_None).
// TextureSize/MaxViewportSize stay 0 as before: the flat path never renders through the eye layer.
int h5_calc_eye_desc(long layout, float scale, long mips, long samples, long fmt, long dfmt, long flags, unsigned* desc) {
    static long n = 0;
    if (++n <= 6) {
        LOGI("CalcEyeLayerDesc2(layout=%ld scale=%.2f mips=%ld samp=%ld fmt=%ld dfmt=%ld flags=%lx desc=%p) #%ld",
             layout, scale, mips, samples, fmt, dfmt, flags, (void*)desc, n);
    }
    if ((unsigned long)desc <= 0x10000) return 0;
    memset(desc, 0, 0x7c);
    desc[0] = 3;                // ovrpShape_EyeFov (FLayer::Initialize_RenderThread compares 3)
    desc[1] = (unsigned)layout;
    desc[4] = 1;                // MipLevels: the wrapped window image has one level
    desc[5] = 1;                // SampleCount
    desc[6] = (unsigned)fmt;
    desc[7] = (unsigned)flags;
    desc[0x68 / 4] = 10;        // DepthFormat None: no depth swapchain around a colour window image
    return 0;
}

// ---------- libvrapi runtime patch: feed window swapchain images as VR eye textures ----------
// The game renders its VR eyes into vrapi texture swapchains. We hand it the
// WINDOW swapchain VkImages, so eye renders land directly in presented frames.
static unsigned long g_vrapi_base;
static int g_vrapi_patched;
static long g_tsc3_calls, g_buf_calls, g_sf2_calls;
static int g_tsc3_w, g_tsc3_h, g_tsc3_bc;

void* h_vr_CreateTSC3(void* a, void* b, void* c, int w, int h, int lv, int fc, int bc, int fl) {
    g_tsc3_calls++;
    g_tsc3_w = w; g_tsc3_h = h; g_tsc3_bc = bc;
    if (g_tsc3_calls <= 5)
        LOGI("vrapi_CreateTextureSwapChain3 #%ld %dx%d bc=%d fl=%x -> FAKE", g_tsc3_calls, w, h, bc, fl);
    return (void*)0x7efa11;
}

int h_vr_TSCLen(void* chain) { return (int)g_win_img_count ? (int)g_win_img_count : 3; }

int h_vr_TSCBufVulkan(void* a0, void* a1, void* a2) {
    // signature probe: log first 10 raw args, then guess (chain, index, out)
    g_buf_calls++;
    if (g_buf_calls <= 10)
        LOGI("vrapi_GetTextureSwapChainBufferVulkan #%ld a0=%p a1=%p a2=%p", g_buf_calls, a0, a1, a2);
    // assume last pointer arg is VkImage* out
    void** out = (void**)a2;
    if (out && g_win_images && g_win_img_count) {
        unsigned idx = 0;
        // a1 may be chain or index depending on arity; treat small int as index
        if ((unsigned long)a1 < 16) idx = (unsigned long)a1;
        *out = g_win_images[idx % g_win_img_count];
    }
    return 0;
}

void h_vr_SubmitFrame2(void* ctx, const void* desc) {
    g_sf2_calls++;
    if (g_sf2_calls <= 5 || (g_sf2_calls % 300) == 0)
        LOGI("vrapi_SubmitFrame2 #%ld layers=%d", g_sf2_calls, desc ? *(const int*)((const char*)desc + 8) : -1);
}

static long g_spi_n, g_ssi_n;
int h_vr_GetSysPropInt(void* a, void* b_) {
    g_spi_n++;
    if (g_spi_n <= 60 || (g_spi_n % 200) == 0)
        LOGI("vrapi_GetSystemPropertyInt(a=%p, prop=%ld) -> 0  #%ld", a, (long)b_, g_spi_n);
    return 0;
}
float h_vr_GetSysPropFloat(void* a, void* b_) {
    g_spi_n++;
    if (g_spi_n <= 40) LOGI("vrapi_GetSystemPropertyFloat(a=%p, prop=%ld)", a, (long)b_);
    return 1.0f;
}
int h_vr_GetSysStatusInt2(void* a, void* b_) {
    g_ssi_n++;
    if (g_ssi_n <= 40 || (g_ssi_n % 500) == 0)
        LOGI("vrapi_GetSystemStatusInt(a=%p, status=%ld) -> 1  #%ld", a, (long)b_, g_ssi_n);
    return 1;
}

static int h_vr_EnumInput(void* ovr, void* desc) { (void)ovr; (void)desc; return 1; }
static int h_vr_CurInputState2(void* ovr, void* dev, void* state) {
    if ((unsigned long)state > 0x10000) {
        unsigned char* p = (unsigned char*)state;
        for (int i = 0; i < 128; i++) p[i] = 0;
        unsigned* u = (unsigned*)p;
        u[0] = 1; u[1] = 1;  // connected, tracked
        float* quat = (float*)(p + 8);
        quat[3] = 1.0f;
        float* pos = (float*)(p + 24);
        pos[0] = 0.20f; pos[1] = -0.30f; pos[2] = -0.50f;
    }
    return 0;
}
static void patch_vrapi() {
    unsigned long b = g_vrapi_base;
    patch_code((void*)(b + 0x10210), (void*)&h_vr_EnumInput);
    patch_code((void*)(b + 0x10260), (void*)&h_vr_CurInputState2);
    patch_code((void*)(b + 0xfe04), (void*)&h_vr_GetSysPropInt);   // GetSystemPropertyInt
    patch_code((void*)(b + 0xfe14), (void*)&h_vr_GetSysPropFloat); // GetSystemPropertyFloat
    patch_code((void*)(b + 0xfe54), (void*)&h_vr_GetSysStatusInt2);// GetSystemStatusInt
    // probe both arities of GetTextureSwapChainBufferVulkan? keep single for now
    patch_code((void*)(b + 0x10008), (void*)&h_vr_CreateTSC3);   // CreateTextureSwapChain3
    patch_code((void*)(b + 0xffe4), (void*)&h_vr_CreateTSC3);    // CreateTextureSwapChain (similar args)
    patch_code((void*)(b + 0xfff8), (void*)&h_vr_CreateTSC3);    // CreateTextureSwapChain2
    patch_code((void*)(b + 0x10090), (void*)&h_vr_TSCLen);       // GetTextureSwapChainLength
    patch_code((void*)(b + 0x100b0), (void*)&h_vr_TSCBufVulkan); // GetTextureSwapChainBufferVulkan
    patch_code((void*)(b + 0x10170), (void*)&h_vr_SubmitFrame2); // SubmitFrame2
    g_vrapi_patched = 1;
    LOGI("libvrapi patched @%lx", b);
}

static int vrapi_phdr_cb(struct dl_phdr_info* info, size_t sz, void* data) {
    if (info->dlpi_name && strstr(info->dlpi_name, "libvrapi.so")) {
        if (*(unsigned int*)info->dlpi_addr == 0x464C457F) {
            *(unsigned long*)data = info->dlpi_addr;
            return 1;
        }
    }
    return 0;
}

static void* watch_vrapi(void* arg) {
    for (int i = 0; i < 2400 && !g_vrapi_patched; i++) {
        unsigned long b = 0;
        if (dl_iterate_phdr(vrapi_phdr_cb, &b)) {
            g_vrapi_base = b;
            patch_vrapi();
        }
        usleep(50000);
    }
    return 0;
}
static void* enable_stereo_thread(void* arg) {
    unsigned long base = g_ue4_base_cached;
    if (!base) return 0;
    // persistent: keep trying to null GEngine's stereo slot until it sticks
    for (int i = 0; i < 1200; i++) {
        if (!g_hmd_instance) {
            // try scanning anyway using EnableHMD result path: capture via known plugin singleton
        }
        null_stereo_device();
        usleep(50000);
    }
    return 0;
}




// ---- input path diagnostics: does the native AInputQueue deliver anything, and does UE4 process it? ----
static void (*r_AIQ_attach)(void*, void*, int, void*, void*);
static int (*r_AIQ_getEvent)(void*, void**);
static int (*r_AIQ_pre)(void*, void*);
static int (*r_AME_getAction)(const void*);
static int (*p_AIE_getType)(const void*);
static int (*p_AIE_getSource)(const void*);
static void h_AIQ_attach(void* q, void* looper, int ident, void* cb, void* data) {
    g_in_attach++;
    LOGF("INPUT attachLooper q=%p looper=%p ident=%d cb=%p data=%p", q, looper, ident, cb, data);
    r_AIQ_attach(q, looper, ident, cb, data);
}
static int h_AIQ_getEvent(void* q, void** ev) {
    int r = r_AIQ_getEvent(q, ev);
    if (r >= 0 && ev && *ev) {
        g_in_get++;
        int ty = p_AIE_getType ? p_AIE_getType(*ev) : -1, src = p_AIE_getSource ? p_AIE_getSource(*ev) : -1;
        if (ty == 2) g_in_motion++;
        if (g_in_get <= 40) LOGF("INPUT getEvent q=%p r=%d type=%d src=0x%x", q, r, ty, src);
    }
    return r;
}
static int h_AIQ_pre(void* q, void* ev) {
    int r = r_AIQ_pre(q, ev);
    g_in_pre++;
    if (g_in_pre <= 10) LOGF("INPUT preDispatch -> %d", r);
    return r;
}
static int h_AME_getAction(const void* e) {
    int a = r_AME_getAction(e);
    g_in_act++;
    if (g_in_act <= 20) LOGF("INPUT AMotionEvent_getAction -> 0x%x", a);
    return a;
}

unsigned long g_tgt_mhtick;
__attribute__((naked)) static void gate_mhtick(void* self, float dt) {
    asm volatile(
        ".inst 0xf81e0ff3\n" ".inst 0xa9017bfd\n" ".inst 0x910043fd\n" ".inst 0xaa0003f3\n"
        "adrp x16, :got:g_tgt_mhtick\n" "ldr x16, [x16, :got_lo12:g_tgt_mhtick]\n" "ldr x16, [x16]\n" "br x16\n");
}
void hook_mhtick(void* self, float dt) {
    __atomic_store_n(&g_touch_gt_tid,(int)syscall(SYS_gettid),__ATOMIC_RELEASE);
    __atomic_store_n(&g_gt_alive_ms,ptr_now_ms(),__ATOMIC_RELEASE);
    touch_tracking_tick();
    apply_touch_graphics(g_ue4_base_cached);
    touch_phone_tick(self);
    touch_heal_progress_tick(self,ptr_now_ms());
    if(g_touch_context!=1 && touch_grenade_owned_grip_mask())touch_grenade_cancel();
    touch_grenade_cancel_tick(0,0,ptr_now_ms());
    // Pooled menu hands keep ticking after native unpause.
    if (touch_menu_active(self)) {
        struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
        g_menu_tick_ms = ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
        g_menu_ticks++;
    }
    if(!touch_grenade_owned_grip_mask())touch_pause_tick(self);
    touch_unpause_tick(self);
    touch_sce_idle_tick();
    touch_mirror_idle_tick();
    touch_throw_idle_tick();
    gate_mhtick(self, dt);
    touch_menu_view_tick(ptr_now_ms());
    touch_ui_diag_tick();
    touch_inspect_tick();
    touch_ui_targets_tick(ptr_now_ms());
    touch_map_zoom_tick(ptr_now_ms());
    touch_comfort_wizard_tick(ptr_now_ms());
    touch_mobile_menu_tick();
    touch_mobile_settings_tick();
    touch_flat_controls_tick();
    touch_mobile_android_tick();
    touch_mobile_extras_tick(ptr_now_ms());
    touch_extras_save_tick(ptr_now_ms());
    touch_prompt_visual_tick();
    touch_menu_grip_tick(self);
    touch_menu_visual_tick(self);
}

// ---- holster hover: AVR4GamePlayerPropHolster::IsHandHovering @0x62c0ed0 (returns bool) ----
// (this, out<FAngle>* x1, AVR4GamePlayerHand* x2, FVector* x3..x6, FAngle s0, FAngle s1). Logs every holster once by
// name and can force "hovered" for the right hand so a GRIP press grabs that holster's item without reaching.
static void (*f_FName_ToString)(const void* name, void* fstr);
static void* (*f_GetCurrentGrip)(const void* hand);
static void obj_name(const void* obj, char* out, int n) {
    out[0] = 0;
    if (!obj || !f_FName_ToString) return;
    struct { unsigned short* data; int num, max; } fs = {0, 0, 0};
    f_FName_ToString((const char*)obj + 0x18, &fs);
    if (fs.data && fs.num > 0) u16_to_ascii(fs.data, out, n);
}
#define MAXHOL 24
static struct { const void* h; char name[64]; long calls, hits; } g_hol[MAXHOL];
static int g_nhol;
static pthread_mutex_t g_holmx = PTHREAD_MUTEX_INITIALIZER;
static int hol_index(const void* h) {
    for (int i = 0; i < g_nhol; i++) if (g_hol[i].h == h) return i;
    pthread_mutex_lock(&g_holmx);
    int k = -1;
    if (g_nhol < MAXHOL) {
        k = g_nhol; g_hol[k].h = h; obj_name(h, g_hol[k].name, 64); g_hol[k].calls = g_hol[k].hits = 0;
        __sync_synchronize(); g_nhol++;
        LOGF("HOLSTER #%d %p '%s'", k, h, g_hol[k].name);
    }
    pthread_mutex_unlock(&g_holmx);
    return k;
}
unsigned long g_tgt_hover;
__attribute__((naked)) static int gate_hover(void* self, float* out, void* hand, const float* a, const float* b,
                                              const float* c, const float* d, float a1, float a2) {
    asm volatile(".inst 0xd10283ff\n" ".inst 0x6d0423e9\n" ".inst 0xa90567fa\n" ".inst 0xa9065ff8\n"
        "adrp x16, :got:g_tgt_hover\n" "ldr x16, [x16, :got_lo12:g_tgt_hover]\n" "ldr x16, [x16]\n" "br x16\n");
}
static volatile long g_hover_forced;
int hook_hover(void* self, float* out, void* hand, const float* a, const float* b, const float* c, const float* d, float a1, float a2) {
    int r = gate_hover(self, out, hand, a, b, c, d, a1, a2);
    int k = hol_index(self);
    if (k >= 0) { g_hol[k].calls++; if (r) g_hol[k].hits++; }
    if (!r && touch_holster_hover_allowed(self, hand, ptr_now_ms())) {
        if (out) *out = 0.0f;
        return 1;
    }
    if (!r && k >= 0 && hand && g_force_pat[0] && ptr_now_ms() < g_force_until &&
        *(const unsigned char*)((const char*)hand + 0x314) == 1 && strstr(g_hol[k].name, g_force_pat) &&
        !(f_GetCurrentGrip && f_GetCurrentGrip(hand))) {
        if (out) *out = 0.0f;
        r = 1;
        if (++g_hover_forced <= 5) LOGF("HOVER forced: holster '%s' right hand %p", g_hol[k].name, hand);
    }
    return r;
}


// ---- grip registry + forced hover on the game thread ----
// UVR4GamePlayerGrip::BeginPlay @0x627f168 / EndPlay @0x627f9dc record every grip component with its owner name.
// AVR4GamePlayerHand::Tick @0x6298468 applies pending SetGripForcedHover(hand, grip, bool) requests (game thread).
#define MAXGRIP 256
struct griprec { void* g; char owner[64]; char comp[40]; };
static struct griprec g_grips[MAXGRIP];
static int g_ngrips;
static pthread_mutex_t g_gripmx = PTHREAD_MUTEX_INITIALIZER;
static void (*f_SetGripForcedHover)(void* hand, void* grip, int on);
static char g_fh_pat[48]; static volatile long g_fh_until; static volatile int g_fh_side = 1;   // request
static void* g_fh_applied_grip; static void* g_fh_applied_hand;                                   // applied state
unsigned long g_tgt_gripbp, g_tgt_gripep, g_tgt_ghtick;
__attribute__((naked)) static void gate_gripbp(void* self) {
    asm volatile(".inst 0xa9ba6ffc\n" ".inst 0xa90167fa\n" ".inst 0xa9025ff8\n" ".inst 0xa90357f6\n"
        "adrp x16, :got:g_tgt_gripbp\n" "ldr x16, [x16, :got_lo12:g_tgt_gripbp]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static void gate_gripep(void* self, int reason) {
    asm volatile(".inst 0xa9be4ff4\n" ".inst 0xa9017bfd\n" ".inst 0x910043fd\n" ".inst 0x2a0103f3\n"
        "adrp x16, :got:g_tgt_gripep\n" "ldr x16, [x16, :got_lo12:g_tgt_gripep]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static void gate_ghtick(void* self, float dt) {
    asm volatile(".inst 0xd10183ff\n" ".inst 0x6d0223e9\n" ".inst 0xa90357f6\n" ".inst 0xa9044ff4\n"
        "adrp x16, :got:g_tgt_ghtick\n" "ldr x16, [x16, :got_lo12:g_tgt_ghtick]\n" "ldr x16, [x16]\n" "br x16\n");
}
void hook_gripbp(void* self) {
    gate_gripbp(self);
    pthread_mutex_lock(&g_gripmx);
    if (g_ngrips < MAXGRIP) {
        struct griprec* r = &g_grips[g_ngrips++];
        r->g = self;
        obj_name(*(void* const*)((const char*)self + 0x20), r->owner, sizeof r->owner);   // UObject::OuterPrivate
        obj_name(self, r->comp, sizeof r->comp);
        if (g_ngrips <= 200) LOGF("GRIP+ %p owner='%s' comp='%s' (n=%d)", self, r->owner, r->comp, g_ngrips);
    }
    pthread_mutex_unlock(&g_gripmx);
}
void hook_gripep(void* self, int reason) {
    pthread_mutex_lock(&g_gripmx);
    for (int i = 0; i < g_ngrips; i++) if (g_grips[i].g == self) { g_grips[i] = g_grips[--g_ngrips]; break; }
    if (g_fh_applied_grip == self) g_fh_applied_grip = 0;
    pthread_mutex_unlock(&g_gripmx);
    gate_gripep(self, reason);
}
static void* grip_find(const char* pat) {
    void* g = 0;
    pthread_mutex_lock(&g_gripmx);
    for (int i = 0; i < g_ngrips && !g; i++) if (strstr(g_grips[i].owner, pat) || strstr(g_grips[i].comp, pat)) g = g_grips[i].g;
    pthread_mutex_unlock(&g_gripmx);
    return g;
}
#include "re4fix_reload.h"
#include "re4fix_state_diag.h"
#include "re4fix_phone.h"
#include "re4fix_cinema.h"
#include "re4fix_mobile_extras.h"
#include "re4fix_extras_save.h"
#include "re4fix_mobile_settings.h"
#include "re4fix_vr_comfort.h"
#include "re4fix_cutscene_weapon.h"
#include "re4fix_menu_visual.h"
#include "re4fix_unpause.h"
#include "re4fix_inventory.h"
#include "re4fix_interact.h"
#include "re4fix_hud_state.h"
#include "re4fix_qte.h"
#include "re4fix_prompt_visual.h"
static void touch_prompt_visual_diag(void) {
    if(!__atomic_exchange_n(&g_prompt_visual_diag_request,0,__ATOMIC_ACQ_REL))return;
    struct prompt_visual_targets v=touch_prompt_visual_targets(1);
    phone_inspect_objects(v.floater,v.count ? v.controller[0] : 0,0);
}
static int touch_motion_pose_busy(void) {
    long now=ptr_now_ms();return touch_qte_pose_busy(now)||touch_grenade_busy(now);
}
#include "re4fix_controller.h"
#include "re4fix_weapon.h"
#include "re4fix_quickselect.h"
#include "re4fix_mirror_touch.h"
#include "re4fix_heal_progress.h"
#include "re4fix_grenade.h"
#include "re4fix_ashley.h"
static int touch_weapon_primary_side(long now) {
    struct touch_quickselect_state state=touch_quickselect_state(now);
    return state.fire_side>=0 ? state.fire_side : 1;
}
static int touch_grenade_selected_side(long now) {
    struct touch_quickselect_state state=touch_quickselect_state(now);
    return state.selected==2 ? state.fire_side : -1;
}
#include "re4fix_flat_controls.h"
#include "re4fix_weapon_cycle.h"
#include "re4fix_reload_family.h"
#include "re4fix_body.h"
#include "re4fix_knife_diag.h"
#include "re4fix_aim_diag.h"
#include "re4fix_aim.h"
#include "re4fix_world_touch.h"
#include "re4fix_sce_touch.h"
#include "re4fix_throw.h"
#include "re4fix_hud_extra.h"
#include "re4fix_flat_view.h"
#include "re4fix_weapon_ui.h"
#include "re4fix_map_zoom.h"
#include "re4fix_typewriter_touch.h"
static volatile long g_ghtick_n;
void hook_ghtick(void* self, float dt) {
    __atomic_store_n(&g_touch_gt_tid,(int)syscall(SYS_gettid),__ATOMIC_RELEASE);
    __atomic_store_n(&g_gt_alive_ms,ptr_now_ms(),__ATOMIC_RELEASE);
    int cancel_side=*(const unsigned char*)((const char*)self+0x314);
    touch_grenade_cancel_tick(self,cancel_side,ptr_now_ms());
    touch_tracking_tick();
    apply_touch_graphics(g_ue4_base_cached);
    if(!touch_grenade_owned_grip_mask())touch_pause_tick(self);
    touch_unpause_tick(self);
    if (*(const unsigned char*)((const char*)self + 0x314)==1) {
        touch_mobile_settings_tick();
        touch_flat_controls_tick();
    }
    if (cancel_side<2 && (g_touch_cancel&(1<<cancel_side))) touch_weapon_cycle_cancel(self);
    gate_ghtick(self, dt);
    int side = *(const unsigned char*)((const char*)self + 0x314);
    long nw = ptr_now_ms();
    void* hand_pawn = phone_player ? phone_player(self) : 0;
    int active_game_hand = hand_pawn && phone_active && phone_type && phone_type() == 1
        && hand_pawn == phone_active();
    if(side<2)__atomic_add_fetch(active_game_hand ? &g_game_tick_side[side]:&g_inactive_tick_side[side],1,__ATOMIC_RELAXED);
    if (side == 1 && active_game_hand) touch_new_game_pawn(hand_pawn);
    if (side == 1 && active_game_hand) {
        touch_tutorial_tick(nw);
        touch_sce_tick(hand_pawn, nw);   // before the world tap: a tap may operate the lever/crank chosen here
        touch_throw_tick(hand_pawn, self, nw);
        touch_world_tick(hand_pawn, self, nw); touch_holster_visual_tick(nw); touch_project_diag(hand_pawn);
    }
    touch_controller_tick(self);
    touch_body_tick(self);
    g_ghtick_n++;
    touch_phone_tick(self);
    touch_hud_state_tick(self,nw);
    touch_heal_progress_tick(self,nw);
    if(side==1)touch_prompt_visual_diag();
    touch_pawn_diag(self);
    touch_knife_diag(self);
    unsigned qs_cancel_before=__atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE);
    touch_quickselect_tick(self,nw);
    touch_ashley_tick(self,nw);
    touch_hud_ammo_tick(self,nw);
    touch_weapon_ui_tick(self);
    touch_grenade_tick(self,side,nw);
    touch_weapon_cycle_tick(self,side,nw);
    touch_reload_tick(self, side, nw);
    if(!touch_grenade_owned_grip_mask())touch_inventory_tick(self);
    touch_qte_tick(self,side,nw);
    touch_interact_tick(self, side, nw);
    touch_aim_tick(self);
    touch_aim_diag(self);
    touch_mobile_android_tick();
    touch_mobile_extras_tick(nw);
    touch_extras_save_tick(nw);
    if(side==1)touch_prompt_visual_tick();
    if (side < 2 && (__atomic_fetch_and(&g_touch_cancel, ~(1 << side), __ATOMIC_ACQ_REL) & (1 << side))) {
        touch_weapon_retry_cancel(self);
        touch_weapon_cycle_cancel(self);
        g_fh_until = 0; g_fh_pat[0] = 0;
        if (g_fh_applied_grip && g_fh_applied_hand == self) {
            f_SetGripForcedHover(self, g_fh_applied_grip, 0);
            g_fh_applied_grip = 0; g_fh_applied_hand = 0;
        }
    }
    if(side==1 && __atomic_load_n(&g_qs_diag_request,__ATOMIC_ACQUIRE)>0) {
        __atomic_sub_fetch(&g_qs_diag_request,1,__ATOMIC_ACQ_REL);
        unsigned long long raw=__atomic_load_n(&qs_ui,__ATOMIC_ACQUIRE);
        LOGF("QSSTATEGT self=%p owner=%p active=%p current=%d ready=%d cancel=%u/%u paused=%d valid=%d raw=%016llx age=%u ticks=%llu,%llu inactive=%llu,%llu",
            self,hand_pawn,phone_active ? phone_active():0,active_game_hand,qs_ready,qs_cancel_before,
            __atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE),rl_paused ? rl_paused():-1,rl_player_valid ? rl_player_valid():-1,
            raw,(unsigned)nw-(unsigned)(raw>>32),__atomic_load_n(&g_game_tick_side[0],__ATOMIC_RELAXED),
            __atomic_load_n(&g_game_tick_side[1],__ATOMIC_RELAXED),__atomic_load_n(&g_inactive_tick_side[0],__ATOMIC_RELAXED),
            __atomic_load_n(&g_inactive_tick_side[1],__ATOMIC_RELAXED));
    }
    if (side == 1 && active_game_hand && !touch_quickselect_live(nw)) {
        static long heal_probe_ms;
        if (nw >= heal_probe_ms) {
            struct tw_binding heal;
            heal_probe_ms = nw + 250;
            if (tw_binding(self, 3, &heal)) __atomic_store_n(&g_heal_holster_ms, nw, __ATOMIC_RELEASE);
        }
        int weapon = __atomic_exchange_n(&g_touch_weapon_request, 0, __ATOMIC_ACQ_REL);
        if (weapon) {
            __atomic_store_n(&g_mobile_holster_until, 0, __ATOMIC_RELEASE);
            g_fh_pat[0] = 0;
            g_fh_until = nw + 1800; g_fh_side = 1;
            if (g_fh_applied_grip && g_fh_applied_hand == self) {
                f_SetGripForcedHover(self, g_fh_applied_grip, 0);
                g_fh_applied_grip = 0; g_fh_applied_hand = 0;
            }
            void* grip = touch_weapon_grip(self, weapon);
            if (grip) {
                g_mobile_holster = *(void**)((char*)grip+0x20);
                g_mobile_holster_hand = self;
                __atomic_store_n(&g_mobile_holster_until, g_fh_until, __ATOMIC_RELEASE);
                f_SetGripForcedHover(self, grip, 1);
                g_fh_applied_grip = grip; g_fh_applied_hand = self;
            }
            LOGF("WEAPON request=%d assigned_holster_grip=%p hand=%p", weapon, grip, self);
            touch_weapon_request_started(self, weapon, nw);
        }
    }
    if (!touch_quickselect_live(nw) && touch_weapon_retry_tick(self, nw)) g_fh_until = nw;
    if (g_fh_applied_grip && g_fh_applied_hand == self && nw >= g_fh_until) {
        f_SetGripForcedHover(self, g_fh_applied_grip, 0);
        LOGF("FHOVER off grip=%p hand=%p", g_fh_applied_grip, self);
        g_fh_applied_grip = 0; g_fh_applied_hand = 0;
    }
    if (active_game_hand && !g_fh_applied_grip && g_fh_pat[0] && nw < g_fh_until && side == g_fh_side) {
        void* g = grip_find(g_fh_pat);
        if (g) {
            f_SetGripForcedHover(self, g, 1);
            g_fh_applied_grip = g; g_fh_applied_hand = self;
            LOGF("FHOVER on '%s' grip=%p hand=%p side=%d", g_fh_pat, g, self, side);
        } else { LOGF("FHOVER: no grip matches '%s'", g_fh_pat); g_fh_pat[0] = 0; }
    }
}

// ---- command console: adb shell run-as com.Armature.VR4 sh -c 'echo "btn A 200" > re4cmd.txt' ----
static int btn_bit_of(const char* n) {
    static const struct { const char* n; unsigned b; } t[] = {
        {"A", 0x1u}, {"B", 0x2u}, {"R3", 0x4u}, {"X", 0x100u}, {"Y", 0x200u}, {"L3", 0x400u}, {"MENU", 0x00100000u},
        {"RT", 0x04000000u}, {"RG", 0x08000000u}, {"LT", 0x10000000u}, {"LG", 0x20000000u}};
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) if (!strcmp(n, t[i].n)) { unsigned b = t[i].b; int k = 0; while (!(b & 1u)) { b >>= 1; k++; } return k; }
    return -1;
}
static void devcmd_exec(char* line) {
    char c[16] = {0}, a1[48] = {0}; float f1 = 0, f2 = 0, f3 = 0, f4 = 0;
    int n = sscanf(line, "%15s %47s %f %f %f %f", c, a1, &f1, &f2, &f3, &f4);
    if (n < 1) return;
    long nw = ptr_now_ms();
    if (!strcmp(c, "btn") && n >= 3) {              // btn <A|B|X|Y|MENU|L3|R3|RT|RG|LT|LG> <ms>
        int k = btn_bit_of(a1); if (k >= 0) g_cmd_until[k] = nw + (long)f1;
    } else if (!strcmp(c, "stick") && n >= 5 && isfinite(f1) && isfinite(f2) && isfinite(f3) && fabsf(f1) <= 1 && fabsf(f2) <= 1 && f3 >= 0 && f3 <= 60000) {
        if (!strcmp(a1, "r")) { g_cmd_rstick[0] = f1; g_cmd_rstick[1] = f2; g_cmd_rstick_until = nw + (long)f3; }
        else if (!strcmp(a1, "l")) { g_cmd_lstick[0] = f1; g_cmd_lstick[1] = f2; g_cmd_lstick_until = nw + (long)f3; }
    } else if (!strcmp(c, "hand") && n >= 2) {      // hand <l|r> <x> <y> <z> | hand <l|r> off
        int hi = a1[0] == 'l' ? 0 : 1;
        if (n >= 5) { g_hand_ovr[hi][0] = f1; g_hand_ovr[hi][1] = f2; g_hand_ovr[hi][2] = f3; g_hand_ovr_on[hi] = 1; }
        else g_hand_ovr_on[hi] = 0;
    } else if (!strcmp(c, "handrot") && n >= 2) {   // handrot <l|r> <yaw> <pitch> <roll> | handrot <l|r> off
        int hi = a1[0] == 'l' ? 0 : 1;
        if (n >= 5) { g_hand_rot_ovr[hi][0] = f1; g_hand_rot_ovr[hi][1] = f2; g_hand_rot_ovr[hi][2] = f3; g_hand_rot_ovr_on[hi] = 1; }
        else g_hand_rot_ovr_on[hi] = 0;
    } else if (!strcmp(c, "kpose")) {
        weapon_pose_command(line);
    } else if ((!strcmp(c, "inspect") || !strcmp(c, "inspectowner")) && n >= 2) {   // inspect[owner] <hex ptr>
        g_inspect_owner = !strcmp(c, "inspectowner");
        __atomic_store_n(&g_inspect_ptr, (void*)strtoull(a1, 0, 16), __ATOMIC_RELEASE);
    } else if (!strcmp(c, "menurows")) {             // log option rows of the next options table build
        g_mm_row_trace = 1;
    } else if (!strcmp(c, "pauseback") && n >= 2) {  // pauseback <metres>: pause-case view step back
        g_pause_back = (float)atof(a1); LOGF("PAUSEBACK %.3f", g_pause_back);
    } else if (!strcmp(c, "posetrace") && n >= 2) {  // posetrace <calls>: log the next node pose queries
        g_pose_trace = atoi(a1);
    } else if (!strcmp(c, "poselatch") && n >= 2) {  // poselatch <0|1>: per-frame look latch A/B
        g_pose_latch_on = atoi(a1) != 0;
        LOGF("POSELATCH %d", g_pose_latch_on);
    } else if (!strcmp(c, "gunpose") && n >= 5 && strcmp(a1, "reload")) {    // gunpose <hip|ads|draw> x y z [pitch_deg] | gunpose drawms n
        float* dst = !strcmp(a1, "hip") ? g_gun_hip : !strcmp(a1, "ads") ? g_gun_ads : !strcmp(a1, "draw") ? g_gun_draw : 0;
        if (dst) { dst[0] = f1; dst[1] = f2; dst[2] = f3; if (dst == g_gun_draw && n >= 6) dst[3] = f4; }
        LOGF("GUNPOSE hip=%.1f,%.1f,%.1f ads=%.1f,%.1f,%.1f draw=%.1f,%.1f,%.1f,%.0fdeg %dms", g_gun_hip[0], g_gun_hip[1], g_gun_hip[2],
             g_gun_ads[0], g_gun_ads[1], g_gun_ads[2], g_gun_draw[0], g_gun_draw[1], g_gun_draw[2], g_gun_draw[3], g_gun_draw_ms);
    } else if (!strcmp(c, "gunpose") && n >= 3 && !strcmp(a1, "reload")) {   // gunpose reload fwd right up roll lift
        float v[5]; int m = sscanf(line, "%*s %*s %f %f %f %f %f", &v[0], &v[1], &v[2], &v[3], &v[4]);
        for (int i = 0; i < m && i < 5; i++) g_gun_reload[i] = v[i];
        LOGF("GUNPOSE reload=%.1f,%.1f,%.1f roll=%.0f lift=%.0f", g_gun_reload[0], g_gun_reload[1], g_gun_reload[2], g_gun_reload[3], g_gun_reload[4]);
    } else if (!strcmp(c, "reloadms") && n >= 4) {   // reloadms <approach> <tilt_in> <tilt_out>
        g_reload_approach_ms = (int)atof(a1); g_gun_reload_ms[0] = (int)f1; g_gun_reload_ms[1] = (int)f2;
        LOGF("RELOADMS approach=%d in=%d out=%d", g_reload_approach_ms, g_gun_reload_ms[0], g_gun_reload_ms[1]);
    } else if (!strcmp(c, "gunpose") && n >= 3 && !strcmp(a1, "drawms")) {
        g_gun_draw_ms = (int)f1;
    } else if (!strcmp(c, "swing")) {                // swing <ms> [amp_m] [period_ms]
        g_anim_len = n >= 2 ? (long)atof(a1) : 1200;
        if (n >= 3) g_anim_amp = f1;
        if (n >= 4) g_anim_period = f2 > 50 ? f2 : 50;
        g_anim_t0 = nw; g_anim_kind = 1;
    } else if (!strcmp(c, "wait") && n >= 2) {       // wait <ms> (inside a batch)
        long w = (long)atof(a1); if (w > 0 && w < 10000) usleep((useconds_t)w * 1000);
    } else if (!strcmp(c, "force") && n >= 3) {      // force <holster-name-substring> <ms>
        snprintf(g_force_pat, sizeof g_force_pat, "%s", a1); g_force_until = nw + (long)f1;
    } else if (!strcmp(c, "look") && n >= 3) {       // look <yaw_deg> <pitch_deg>
        g_look_yaw = (float)atof(a1) * 0.01745329f; g_look_pitch = f1 * 0.01745329f;
    } else if (!strcmp(c, "grips")) {
        pthread_mutex_lock(&g_gripmx);
        for (int i = 0; i < g_ngrips; i++) LOGF("GRIP #%d %p owner='%s' comp='%s'", i, g_grips[i].g, g_grips[i].owner, g_grips[i].comp);
        pthread_mutex_unlock(&g_gripmx);
    } else if (!strcmp(c, "fhover") && n >= 3) {    // fhover <owner/comp substring> <ms> [side 0|1]
        snprintf(g_fh_pat, sizeof g_fh_pat, "%s", a1); g_fh_until = nw + (long)f1; g_fh_side = n >= 4 ? (int)f2 : 1;
    } else if (!strcmp(c, "holsters")) {
        for (int i = 0; i < g_nhol; i++) LOGF("HOLSTER #%d %p '%s' calls=%ld hits=%ld", i, g_hol[i].h, g_hol[i].name, g_hol[i].calls, g_hol[i].hits);
    } else if (!strcmp(c, "grenadediag")) {
        __atomic_store_n(&g_grenade_diag_request,1,__ATOMIC_RELEASE);
    } else if (!strcmp(c, "state")) {
        touch_log_state();
        __atomic_store_n(&g_tracking_diag_request, 1, __ATOMIC_RELEASE);
        __atomic_store_n(&g_pawn_diag_request, 1, __ATOMIC_RELEASE);
        __atomic_store_n(&g_phone_request, 2, __ATOMIC_RELEASE);
        __atomic_store_n(&g_aim_diag_request, 1, __ATOMIC_RELEASE);
        __atomic_store_n(&g_prompt_visual_diag_request, 1, __ATOMIC_RELEASE);
        __atomic_store_n(&g_qs_diag_request, 4, __ATOMIC_RELEASE);
    } else if (!strcmp(c, "knifediag") && n >= 2) {
        char* end = 0;
        float ms = strtof(a1, &end);
        if (end != a1 && !*end && isfinite(ms) && ms >= 0 && ms <= 5000) g_knife_diag_until = nw + (long)ms;
    } else if (!strcmp(c, "uiscreen")) {
        __atomic_store_n(&g_ui_dump_request, 1, __ATOMIC_RELEASE);
    } else if (!strcmp(c, "uiclick") && n >= 2) {
        snprintf(g_ui_click_pat, sizeof g_ui_click_pat, "%s", a1);
        __atomic_store_n(&g_ui_click_request, 1, __ATOMIC_RELEASE);
    } else if (!strcmp(c, "project")) {
        __atomic_store_n(&g_project_diag_request, 1, __ATOMIC_RELEASE);
    } else if (!strcmp(c, "inspectphone")) {
        __atomic_store_n(&g_phone_request, 3, __ATOMIC_RELEASE);
    } else if (!strcmp(c, "throwpose") && n >= 4) {   // throwpose <forward_m> <up_m> <ms>: spear/lantern flick
        g_throw_forward = (float)atof(a1); g_throw_up = f1; g_throw_ms = (int)f2;
        LOGF("THROWPOSE forward=%.2f up=%.2f ms=%d", g_throw_forward, g_throw_up, g_throw_ms);
    } else if (!strcmp(c, "sce")) {                  // sce: log the registered levers/cranks once (GameThread)
        __atomic_store_n(&g_sce_diag_request, 1, __ATOMIC_RELEASE);
    } else if (!strcmp(c, "skip")) {                 // skip: the hold-to-skip request without touching
        __atomic_store_n(&g_cutscene_skip_request, nw, __ATOMIC_RELEASE);
    } else if (!strcmp(c, "qturn")) {                // qturn: quick 180-degree turn
        __atomic_store_n(&g_qturn_request_ms, nw, __ATOMIC_RELEASE);
    } else if (!strcmp(c, "tutorial")) {             // tutorial: dynamic tutorial manager and current instruction
        void* manager = g_ue4_base_cached ? *(void* const*)(g_ue4_base_cached + 0xa562440) : 0;   // spInstance
        void* current = manager ? *(void* const*)((const char*)manager + 0x298) : 0;
        char name[96] = "", cls[96] = "";
        if (current) { obj_name(current, name, sizeof name); obj_name(*(void* const*)((const char*)current + 0x10), cls, sizeof cls); }
        LOGF("TUTORIAL manager=%p current=%p '%s' class='%s'", manager, current, name, cls);
    } else if (!strcmp(c, "pausestate")) {           // pausestate: Bio4 pause reasons and touch gates
        char reasons[8] = "-------";
        for (int r = 0; tw_paused_for && r < 7; r++) reasons[r] = tw_paused_for(r) ? '0' + r : '-';
        // UGameplayStatics::IsGamePaused(world context): the engine pause used by AVR4GameMode::SuspendForFocusLoss.
        static const unsigned is_paused_words[2] = {0xa9bf7bfd, 0x910003fd};
        int engine_paused = -1;
        void* context = phone_active ? phone_active() : 0;
        if (context && g_ue4_base_cached && !memcmp((const void*)(g_ue4_base_cached + 0x8892914), is_paused_words, 8))
            engine_paused = ((int (*)(const void*))(g_ue4_base_cached + 0x8892914))(context) & 1;
        LOGF("PAUSESTATE paused=%d reasons=%s engine_paused=%d focus=%d valid=%d cancel=%d weapon=%d request=%d type=%u",
             rl_paused ? rl_paused() : -1, reasons, engine_paused, g_app_input_focus, rl_player_valid ? rl_player_valid() : -1,
             g_touch_cancel, g_touch_weapon, g_touch_weapon_request, phone_type ? phone_type() : 0);
    } else if (!strcmp(c, "pad")) {                  // pad: log the gamepad state
        touch_gamepad_log();
    } else if (!strcmp(c, "focus") && n >= 2) {      // focus <0|1>: emulate window input focus
        __atomic_store_n(&g_app_input_focus, atoi(a1) != 0, __ATOMIC_RELEASE);
        LOGF("APPFOCUS forced input_focus=%d", g_app_input_focus);
    }
    LOGF("CMD '%s' (n=%d)", line, n);
}
static void* devcmd_thread(void* arg) {
    (void)arg;
    const char* path = "/data/data/com.Armature.VR4/re4cmd.txt";
    for (;;) {
        usleep(50000);
        int fd = open(path, O_RDONLY | O_CLOEXEC);
        if (fd < 0) continue;
        char buf[1024]; int n = (int)read(fd, buf, sizeof buf - 1); close(fd); unlink(path);
        if (n <= 0) continue;
        buf[n] = 0;
        char* save = 0;
        for (char* line = strtok_r(buf, "\n;", &save); line; line = strtok_r(0, "\n;", &save)) if (line[0]) devcmd_exec(line);
    }
    return 0;
}
#include "re4fix_input.h"
#include "re4fix_gamepad.h"
#include "re4fix_tutorial.h"
#include "re4fix_menu_grip.h"
#include "re4fix_graphics.h"
#include "re4fix_pause.h"
#include "re4fix_mobile_menu.h"
#include "re4fix_enemy_aggression.h"
#include "re4fix_view_extra.h"
#include "re4fix_media_diag.h"

unsigned long g_tgt_loadmap, g_tgt_beginplay;
__attribute__((naked)) static int gate_loadmap(void* e, void* wc, void* url, void* pend, void* err) {
    asm volatile(".inst 0xd10703ff\n" ".inst 0xfd00a3ea\n" ".inst 0x6d1523e9\n" ".inst 0xa9166ffc\n"
        "adrp x16, :got:g_tgt_loadmap\n" "ldr x16, [x16, :got_lo12:g_tgt_loadmap]\n" "ldr x16, [x16]\n" "br x16\n");
}
int hook_loadmap(void* e, void* wc, void* url, void* pend, void* err) {
    char m[160] = "?";
    if ((unsigned long)url > 0x10000) u16_to_ascii(*(const unsigned short**)((char*)url + 0x28), m, 160);
    LOGI("LoadMap -> '%s'", m);
    loading_map_begin(m);
    int r = gate_loadmap(e, wc, url, pend, err);
    loading_map_end(m,r&1);
    LOGI("LoadMap '%s' done -> %d", m, r & 1);
    return r;
}
__attribute__((naked)) static void gate_beginplay(void* w) {
    asm volatile(".inst 0xf81e0ff3\n" ".inst 0xa9017bfd\n" ".inst 0x910043fd\n" ".inst 0xaa0003f3\n"
        "adrp x16, :got:g_tgt_beginplay\n" "ldr x16, [x16, :got_lo12:g_tgt_beginplay]\n" "ldr x16, [x16]\n" "br x16\n");
}
void hook_beginplay(void* w) { static long n = 0; LOGI("UWorld::BeginPlay #%ld world=%p", ++n, w); gate_beginplay(w); }

unsigned long g_tgt_browse, g_tgt_splshow, g_tgt_splhide;
__attribute__((naked)) static int gate_browse(void* e, void* wc, void* url, void* err) {
    asm volatile(".inst 0xa9ba6ffc\n" ".inst 0xa90167fa\n" ".inst 0xa9025ff8\n" ".inst 0xa90357f6\n"
        "adrp x16, :got:g_tgt_browse\n" "ldr x16, [x16, :got_lo12:g_tgt_browse]\n" "ldr x16, [x16]\n" "br x16\n");
}
int hook_browse(void* e, void* wc, void* url, void* err) {
    char m[160] = "?";
    if ((unsigned long)url > 0x10000) u16_to_ascii(*(const unsigned short**)((char*)url + 0x28), m, 160);
    LOGI("Browse -> '%s'", m);
    int r = gate_browse(e, wc, url, err);
    LOGI("Browse '%s' -> %d", m, r);
    return r;
}
__attribute__((naked)) static void gate_splshow(void* s) {
    asm volatile(".inst 0xfc190fe8\n" ".inst 0xa9016ffc\n" ".inst 0xa90267fa\n" ".inst 0xa9035ff8\n"
        "adrp x16, :got:g_tgt_splshow\n" "ldr x16, [x16, :got_lo12:g_tgt_splshow]\n" "ldr x16, [x16]\n" "br x16\n");
}
void hook_splshow(void* sp) { static long n = 0; if (++n <= 20) LOGI("FSplash::DoShow #%ld", n); gate_splshow(sp); }
__attribute__((naked)) static void gate_splhide(void* s) {
    asm volatile(".inst 0xd10103ff\n" ".inst 0xa9024ff4\n" ".inst 0xa9037bfd\n" ".inst 0x9100c3fd\n"
        "adrp x16, :got:g_tgt_splhide\n" "ldr x16, [x16, :got_lo12:g_tgt_splhide]\n" "ldr x16, [x16]\n" "br x16\n");
}
void hook_splhide(void* sp) { static long n = 0; if (++n <= 20) LOGI("FSplash::DoHide #%ld", n); gate_splhide(sp); }

// Null ONLY GEngine->StereoRenderingDevice (TSharedPtr object @+0xaf0). XRSystem (+0xb00) stays valid so the
// game logic keeps its HMD (nulling XRSystem makes it ForceQuit). With no stereo-rendering device the viewport is
// a plain flat viewport: no HMD custom present, no spectator-screen overwrite, no eye-layer render target.
static void* stereo_null_thread(void* a) {
    while (!g_ue4_base_cached) usleep(20000);
    unsigned long base = g_ue4_base_cached;
    long n = 0;
    for (int i = 0; i < 40000; i++) {
        unsigned long ge = *(unsigned long*)(base + 0xaa75db0);
        if (ge > 0x1000000000ULL && ge < 0x8000000000ULL) {
            volatile unsigned long* sd = (volatile unsigned long*)(ge + 0xaf0);
            if (*sd) {
                if (n < 4) LOGI("StereoRenderingDevice null: ge=%lx dev=%lx ctrl=%lx xrs=%lx", ge, (unsigned long)*sd,
                                *(unsigned long*)(ge + 0xaf8), *(unsigned long*)(ge + 0xb00));
                *sd = 0; n++;
            }
        }
        usleep(2000);
    }
    return 0;
}

// UOculusEntitlementCallbackProxy::Activate: no Oculus platform service on a phone -> it would broadcast
// OnFailure. Report EPrivilegeResults::NoFailures (0) -> OnSuccess.Broadcast().
static void (*g_onpriv)(void*, void*, int, unsigned);
void hook_entitle(void* self) {
    LOGI("VerifyEntitlement.Activate(%p) -> OnSuccess", self);
    g_onpriv(self, self, 0, 0);
}

// Patch the engine's VulkanDynamicAPI function-pointer table directly (robust no matter how it was filled).
static void* dynapi_thread(void* a) {
    while (!g_ue4_base_cached) usleep(20000);
    unsigned long b = g_ue4_base_cached;
    void** p_csc = (void**)(b + 0xa5e2808);   // VulkanDynamicAPI::vkCreateSwapchainKHR
    void** p_gsi = (void**)(b + 0xa5e2818);   // VulkanDynamicAPI::vkGetSwapchainImagesKHR
    void** p_civ = (void**)(b + 0xa5e2588);   // VulkanDynamicAPI::vkCreateImageView
    void** p_formats = (void**)(b + 0xa5e2848);
    void** p_android_surface = (void**)(b + 0xa5e2870);
    void** p_destroy_view = (void**)(b + 0xa5e2590);
    int done = 0;
    for (int i = 0; i < 30000 && done != 63; i++) {
        if (!(done & 1) && *p_csc) { LOGI("DYNAPI vkCreateSwapchainKHR=%p (hook %p)", *p_csc, (void*)&h_vkCreateSwapchain);
            if (*p_csc != (void*)&h_vkCreateSwapchain) { r_vkCreateSwapchain = (int (*)(void*, const void*, const void*, void**))*p_csc; *p_csc = (void*)&h_vkCreateSwapchain; } done |= 1; }
        if (!(done & 2) && *p_gsi) { LOGI("DYNAPI vkGetSwapchainImagesKHR=%p (hook %p)", *p_gsi, (void*)&h_vkGetSCI);
            if (*p_gsi != (void*)&h_vkGetSCI) { r_vkGetSCI = (int (*)(void*, void*, unsigned*, void**))*p_gsi; *p_gsi = (void*)&h_vkGetSCI; } done |= 2; }
        if (!(done & 4) && *p_civ) { LOGI("DYNAPI vkCreateImageView=%p (hook %p)", *p_civ, (void*)&h_vkCreateImageView);
            if (*p_civ != (void*)&h_vkCreateImageView) { r_vkCreateImageView = (int (*)(void*, const void*, const void*, void**))*p_civ; *p_civ = (void*)&h_vkCreateImageView; } done |= 4; }
        if (!(done & 8) && *p_formats) {
            if (*p_formats != (void*)&h_vkSurfaceFormats) { r_vkSurfaceFormats = *p_formats; *p_formats = (void*)&h_vkSurfaceFormats; } done |= 8; }
        if (!(done & 16) && *p_android_surface) {
            if (*p_android_surface != (void*)&h_vkCreateAndroidSurface) { r_vkCreateAndroidSurface = *p_android_surface; *p_android_surface = (void*)&h_vkCreateAndroidSurface; } done |= 16; }
        if (!(done & 32) && *p_destroy_view) {
            if (*p_destroy_view != (void*)&h_vkDestroyImageView) { r_vkDestroyImageView = *p_destroy_view; *p_destroy_view = (void*)&h_vkDestroyImageView; } done |= 32; }
        usleep(500);
    }
    return 0;
}

static void* main_thread(void* arg) {
    unsigned long base = ue4_base();
    if (!base) { LOGI("no libUE4"); return 0; }
    LOGI("libUE4 base=%lx", base);
    g_ue4_base_cached = base;
    install_comp_framebuffer_lifecycle(base);
    install_packaged_obb(base);
    install_mobile_android(base);
    install_loading(base);
    unsigned long slot = 0;
    for (int t = 0; t < 100 && !slot; t++) {
        slot = find_got(base, "dlsym");
        if (!slot) usleep(50000);
    }
    if (!slot) { LOGI("no dlsym slot"); return 0; }
    r_dlsym = (void* (*)(void*, const char*))dlsym(RTLD_DEFAULT, "dlsym");
    int rc = write_mem((void*)slot, (void*)&h_dlsym);
    void* now = *(void**)slot;
    LOGI("dlsym GOT +%lx rc=%d now=%p want=%p", slot - base, rc, now, (void*)&h_dlsym);
    if (now != (void*)&h_dlsym) {
        if (protect_mem((void*)slot, sizeof(void*), PROT_READ | PROT_WRITE) == 0) {
            *(void**)slot = (void*)&h_dlsym;
            protect_mem((void*)slot, sizeof(void*), PROT_READ);
            LOGI("mprotect fallback ok=%d", *(void**)slot == (void*)&h_dlsym);
        }
    }

    // inline trampolines for shader library tracing
    g_tgt_ifr = base + 0x6f98aa8 + 16;
    g_tgt_create = base + 0x6f9ade0 + 16;
    patch_code((void*)(base + 0x6f98aa8), (void*)&hook_ifr);
    patch_code((void*)(base + 0x6f9ade0), (void*)&hook_create);
    g_tgt_lfta = base + 0x6742364 + 16;
    patch_code((void*)(base + 0x6742364), (void*)&hook_lfta);
    g_tgt_cgsm = base + 0x861c9cc + 16;
    patch_code((void*)(base + 0x861c9cc), (void*)&hook_cgsm);
    patch_code((void*)(base + 0x67299b8), (void*)&hook_ree);  // RequestEngineExit -> blocked
    // render-path tracers
    g_tgt_cscrt = base + 0x5c25d40 + 16;
    patch_code((void*)(base + 0x5c25d40), (void*)&hook_cscrt_impl);
    g_tgt_art = base + 0x5c16b70 + 16;
    patch_code((void*)(base + 0x5c16b70), (void*)&hook_art_impl);
    g_tgt_alias = base + 0x6531668 + 16;
    patch_code((void*)(base + 0x6531668), (void*)&hook_alias_impl);
    patch_code((void*)(base + 0x5c133c0), (void*)&hook_initdevice);  // InitDevice -> true
    patch_code((void*)(base + 0x5c12ce8), (void*)&hook_settrackingorigin);  // SetTrackingOrigin no-op
    // GetProjectionData: original (XRSystem vtable valid now; the old hook wrote a perspective into ViewRotationMatrix)
    g_tgt_iel = base + 0x5c1cc24 + 16;
    patch_code((void*)(base + 0x5c1cc24), (void*)&hook_iel_impl);
    g_tgt_iel = base + 0x5c1cc24 + 16;
    g_tgt_cgv = base + 0x8878488 + 16;
    patch_code((void*)(base + 0x8878488), (void*)&hook_cgv);
    g_tgt_gvpdraw = base + 0x88ab8b0 + 16;
    patch_code((void*)(base + 0x88ab8b0), (void*)&hook_gvpdraw);
    g_tgt_beginrvf = base + 0x78c7630 + 16;
    patch_code((void*)(base + 0x78c7630), (void*)&hook_beginrvf);
    g_tgt_csvo = base + 0x89b2950 + 16;
    patch_code((void*)(base + 0x89b2950), (void*)&hook_csvo);
    g_tgt_calcsv = base + 0x89b2bb0 + 16;
    patch_code((void*)(base + 0x89b2bb0), (void*)&hook_calcsv);
    g_tgt_touch = base + 0x6a169ac + 16;
    f_FName_ToString = (void (*)(const void*, void*))(base + 0x67c885c);
    f_GetCurrentGrip = (void* (*)(const void*))(base + 0x629a774);
    f_SetGripForcedHover = (void (*)(void*, void*, int))(base + 0x629a7cc);
    install_touch_pause(base);
    install_touch_tracking(base);
    install_touch_reload(base);
    install_pawn_diag(base);
    install_touch_phone(base);
    install_touch_cutscene_weapon(base);
    install_phone_view(base);
    install_touch_cinema(base);
    install_touch_mobile_extras(base);
    install_touch_extras_save(base);
    install_touch_vr_comfort(base);   // class only: Tunneling follows the native option (default None)
    install_touch_mobile_settings(base);
    install_touch_menu_visual(base);
    install_touch_menu_grip(base);
    install_touch_unpause(base);
    install_touch_inventory(base);
    install_touch_interact(base);
    install_touch_hud_state(base);
    install_touch_qte(base);
    install_touch_prompt_visual(base);
    install_touch_controller(base);
    install_touch_weapon(base);
    install_touch_quickselect(base);
    install_touch_mirror(base);
    install_touch_heal_progress(base);
    install_touch_grenade(base);
    install_touch_ashley(base);
    install_touch_flat_controls(base);
    install_touch_weapon_cycle(base);
    install_touch_reload_family(base);
    install_touch_body(base);
    install_touch_knife_diag(base);
    install_touch_aim_diag(base);
    install_touch_aim(base);
    install_touch_scope(base);
    install_world_touch(base);
    install_sce_touch(base);
    install_touch_throw(base);
    install_hud_extra(base);
    install_flat_view(base);
    install_map_zoom(base);
    install_touch_typewriter(base);
    install_mobile_menu(base);
    install_enemy_aggression(base);
    install_view_extra(base);
    install_gamepad(base);
    install_tutorial(base);
    install_media_diag(base);
    patch_code((void*)(base + 0x6a169ac), (void*)&hook_touch);
    g_tgt_mhtick = base + 0x6310630 + 16; patch_code((void*)(base + 0x6310630), (void*)&hook_mhtick);   // AVR4MenuPlayerHand::Tick
    g_tgt_hover = base + 0x62c0ed0 + 16; patch_code((void*)(base + 0x62c0ed0), (void*)&hook_hover);   // AVR4GamePlayerPropHolster::IsHandHovering
    g_tgt_gripbp = base + 0x627f168 + 16; patch_code((void*)(base + 0x627f168), (void*)&hook_gripbp);   // UVR4GamePlayerGrip::BeginPlay
    g_tgt_gripep = base + 0x627f9dc + 16; patch_code((void*)(base + 0x627f9dc), (void*)&hook_gripep);   // UVR4GamePlayerGrip::EndPlay
    g_tgt_ghtick = base + 0x6298468 + 16; patch_code((void*)(base + 0x6298468), (void*)&hook_ghtick);   // AVR4GamePlayerHand::Tick
    { pthread_t tc; pthread_create(&tc, 0, devcmd_thread, 0); }   // FAndroidInputInterface::QueueTouchInput
    {
        unsigned int ret0[2] = {0x52800000, 0xd65f03c0};   // UEngine::IsStereoscopic3D -> false (stable mono base)
        void* iss = (void*)(base + 0x8c0b660);
        if (protect_mem(iss, sizeof ret0, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
            memcpy(iss, ret0, 8); __builtin___clear_cache((char*)iss, (char*)iss + 8);
            LOGI("IsStereoscopic3D -> false (mono)");
        }
    }
    {
        // FOculusHMD::ShouldUseSeparateRenderTarget -> false (method 0x5c16894 + IStereoRendering thunk 0x5c168a0):
        // the viewport then renders straight into the RHI backbuffer (the acquired swapchain image) instead of
        // the HMD eye layer that was aliased to swapchain image 0 only.
        unsigned int ret0[2] = {0x52800000, 0xd65f03c0};
        unsigned long addrs[2] = {base + 0x5c16894, base + 0x5c168a0};
        for (int k = 0; k < 2; k++) {
            void* t = (void*)addrs[k];
            if (protect_mem(t, sizeof ret0, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) { memcpy(t, ret0, 8); __builtin___clear_cache((char*)t, (char*)t + 8); }
        }
        LOGI("ShouldUseSeparateRenderTarget -> false (render to backbuffer)");
    }
    g_onpriv = (void (*)(void*, void*, int, unsigned))(base + 0x54e95ac);   // OnUserPrivilegeCompleteDelegate
    patch_code((void*)(base + 0x54e91cc), (void*)&hook_entitle);             // UOculusEntitlementCallbackProxy::Activate
    { pthread_t ts; pthread_create(&ts, 0, stereo_null_thread, 0); }
    { pthread_t td; pthread_create(&td, 0, dynapi_thread, 0); }
    {   // 0x8c0a908: str x8,[x19,#0xaf0] -> str xzr,[x19,#0xaf0]  (StereoRenderingDevice object stays null)
        unsigned int ins = 0xf9057a7f;
        void* t = (void*)(base + 0x8c0a908);
        if (protect_mem(t, sizeof ins, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
            memcpy(t, &ins, 4); __builtin___clear_cache((char*)t, (char*)t + 4);
            LOGI("InitializeHMDDevice: StereoRenderingDevice store -> null");
        }
    }
    g_tgt_browse = base + 0x8c120ec + 16;  patch_code((void*)(base + 0x8c120ec), (void*)&hook_browse);
    g_tgt_splshow = base + 0x5c31734 + 16; patch_code((void*)(base + 0x5c31734), (void*)&hook_splshow);
    g_tgt_splhide = base + 0x5c33728 + 16; patch_code((void*)(base + 0x5c33728), (void*)&hook_splhide);
    g_tgt_loadmap = base + 0x8c14598 + 16;
    patch_code((void*)(base + 0x8c14598), (void*)&hook_loadmap);
    g_tgt_beginplay = base + 0x8c50668 + 16;
    patch_code((void*)(base + 0x8c50668), (void*)&hook_beginplay);
    LOGI("trampolines installed (mono+draws-forwarded+touch)");
    // also hook fopen/open GOT for file tracing
    {
        p_AIE_getType = (int (*)(const void*))dlsym(RTLD_DEFAULT, "AInputEvent_getType");
        p_AIE_getSource = (int (*)(const void*))dlsym(RTLD_DEFAULT, "AInputEvent_getSource");
        const char* in_names[4] = {"AInputQueue_attachLooper", "AInputQueue_getEvent", "AInputQueue_preDispatchEvent", "AMotionEvent_getAction"};
        void* in_h[4] = {(void*)&h_AIQ_attach, (void*)&h_AIQ_getEvent, (void*)&h_AIQ_pre, (void*)&h_AME_getAction};
        void** in_r[4] = {(void**)&r_AIQ_attach, (void**)&r_AIQ_getEvent, (void**)&r_AIQ_pre, (void**)&r_AME_getAction};
        for (int i = 0; i < 4; i++) {
            unsigned long gs = find_got(base, in_names[i]);
            if (gs) { *in_r[i] = *(void**)gs; write_mem((void*)gs, in_h[i]); LOGF("%s GOT hooked +%lx", in_names[i], gs - base); }
            else LOGF("%s GOT slot not found", in_names[i]);
        }
    }
    const char* fnames[3] = {"fopen", "openat", "open"};
    void* fh[3] = {(void*)&h_fopen, (void*)&h_openat, (void*)&h_open};
    void** freal[3] = {(void**)&r_fopen, (void**)&r_openat, (void**)&r_open};
    for (int i = 0; i < 3; i++) {
        unsigned long fs = 0;
        for (int t = 0; t < 20 && !fs; t++) fs = find_got(base, fnames[i]);
        if (fs) {
            *freal[i] = *(void**)fs;
            write_mem((void*)fs, fh[i]);
            LOGI("%s GOT hooked +%lx", fnames[i], fs - base);
        }
    }
    return 0;
}

__attribute__((constructor)) static void on_load() {
    LOGI("re4fix loaded");
    pthread_t t1, t2;
    pthread_create(&t1, 0, main_thread, 0);
    pthread_create(&t2, 0, watch_shader, 0);
    pthread_t t3;
    pthread_create(&t3, 0, watch_vrapi, 0);
    pthread_t t4;
    pthread_create(&t4, 0, enable_stereo_thread, 0);
    ovrp_thunk_init();
}
