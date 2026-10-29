// re4fix_comp.h - flat-port mini compositor.
//
// Problem: the mobile renderer resolves MSAA straight into the backbuffer using a B8G8R8A8_SRGB (fmt 50)
// render pass, but this Android surface only offers formats 37/43/4/97/64, and the engine creates its swapchain
// as A2B10G10R10 (64): every backbuffer framebuffer had a view/render-pass format mismatch -> scrambled colours.
//
// Fix: each swapchain image i gets a proxy image (linear44 storage when mutable, same extent). Every image view the engine creates on
// swapchain image i with the engine's swapchain format is created on proxy i instead (sRGB50 -> matches the render
// pass). At present time the existing overlay pass samples proxy i onto the real RGBA sRGB image, then draws
// touch controls. The sampled pass preserves the scene's RGB channels and sRGB transfer.
//
// Proxy layout protocol: the engine never sees the proxies, so it never transitions them. The first render pass
// that targets proxy i gets an UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL barrier recorded in front of it; after every
// presentation the proxy is put back into COLOR_ATTACHMENT_OPTIMAL, the layout the backbuffer render pass expects.
//
// Included from re4fix.c after the Vulkan hook globals (r_gipa, g_vk_instance, r_vkQueueSubmit2, g_own_imgs,
// g_window_sc, swidx_of_image, g_phys_dev, LOGF).

#include "re4fix_overlay.h"
int touch_overlay_quads(struct ovl_quad* q, int max);   // re4fix_input.h

#define COMP_MAX 8
#define VK_LAYOUT_UNDEFINED 0u
#define VK_LAYOUT_COLOR_ATT 2u
#define VK_LAYOUT_SHADER_READ 5u
#define VK_LAYOUT_PRESENT 1000001002u

static int g_comp_on;                  // proxies created and active
static int g_comp_disable;             // kill switch (creation failed / swapchain mismatch)
static int g_comp_mutable;
static void* g_comp_dev;               // engine VkDevice that owns the swapchain
static unsigned g_comp_w, g_comp_h, g_comp_n;
static unsigned g_comp_created_w,g_comp_created_h;
static long g_comp_swapchain_generation;
static void* g_comp_window_img[COMP_MAX];
static unsigned g_comp_fmt = 50;       // proxy format = scene colour format (B8G8R8A8_SRGB)
static unsigned g_comp_usage_extra;    // engine-requested usage bits we mirror (INPUT_ATTACHMENT)
static void* g_comp_img[COMP_MAX];     // proxy VkImage
static void* g_comp_mem[COMP_MAX];
static void* g_comp_cmd[COMP_MAX];     // VkCommandBuffer per swapchain image
static void* g_comp_fence[COMP_MAX];
static void* g_comp_sem[COMP_MAX];     // presentation pass done -> real present waits on it
static volatile int g_comp_init[COMP_MAX];   // proxy has been transitioned out of UNDEFINED
static volatile int g_comp_drawn[COMP_MAX];  // a render pass has targeted proxy i since creation
static unsigned long g_comp_pass_serial[COMP_MAX],g_comp_present_serial[COMP_MAX];
static unsigned g_comp_fresh_logs,g_comp_gap_logs;
static void* g_comp_pool;
static long g_comp_blits, g_comp_fail, g_comp_skip, g_comp_views, g_comp_inits;
static int g_comp_last_err;

static int  (*c_CreateImage)(void*, const void*, const void*, void**);
static void (*c_GetImageMemReq)(void*, void*, void*);
static int  (*c_AllocateMemory)(void*, const void*, const void*, void**);
static int  (*c_BindImageMemory)(void*, void*, void*, unsigned long);
static void (*c_GetPhysMemProps)(void*, void*);
static int  (*c_CreateCommandPool)(void*, const void*, const void*, void**);
static int  (*c_AllocCmdBufs)(void*, const void*, void**);
static int  (*c_BeginCmd)(void*, const void*);
static int  (*c_EndCmd)(void*);
static int  (*c_ResetCmd)(void*, unsigned);
static void (*c_CmdBarrier)(void*, unsigned, unsigned, unsigned, unsigned, const void*, unsigned, const void*, unsigned, const void*);
static int  (*c_CreateSemaphore)(void*, const void*, const void*, void**);
static int  (*c_CreateFence)(void*, const void*, const void*, void**);
static int  (*c_WaitFences)(void*, unsigned, void* const*, unsigned, unsigned long);
static int  (*c_ResetFences)(void*, unsigned, void* const*);

static void* comp_fn(const char* n) { return (r_gipa && g_vk_instance) ? r_gipa(g_vk_instance, n) : 0; }

static int comp_resolve(void) {
    if (c_CreateImage && c_CreateFence) return 1;
    c_CreateImage = (int (*)(void*, const void*, const void*, void**))comp_fn("vkCreateImage");
    c_GetImageMemReq = (void (*)(void*, void*, void*))comp_fn("vkGetImageMemoryRequirements");
    c_AllocateMemory = (int (*)(void*, const void*, const void*, void**))comp_fn("vkAllocateMemory");
    c_BindImageMemory = (int (*)(void*, void*, void*, unsigned long))comp_fn("vkBindImageMemory");
    c_GetPhysMemProps = (void (*)(void*, void*))comp_fn("vkGetPhysicalDeviceMemoryProperties");
    c_CreateCommandPool = (int (*)(void*, const void*, const void*, void**))comp_fn("vkCreateCommandPool");
    c_AllocCmdBufs = (int (*)(void*, const void*, void**))comp_fn("vkAllocateCommandBuffers");
    c_BeginCmd = (int (*)(void*, const void*))comp_fn("vkBeginCommandBuffer");
    c_EndCmd = (int (*)(void*))comp_fn("vkEndCommandBuffer");
    c_ResetCmd = (int (*)(void*, unsigned))comp_fn("vkResetCommandBuffer");
    c_CmdBarrier = (void (*)(void*, unsigned, unsigned, unsigned, unsigned, const void*, unsigned, const void*, unsigned, const void*))comp_fn("vkCmdPipelineBarrier");
    c_CreateSemaphore = (int (*)(void*, const void*, const void*, void**))comp_fn("vkCreateSemaphore");
    c_CreateFence = (int (*)(void*, const void*, const void*, void**))comp_fn("vkCreateFence");
    c_WaitFences = (int (*)(void*, unsigned, void* const*, unsigned, unsigned long))comp_fn("vkWaitForFences");
    c_ResetFences = (int (*)(void*, unsigned, void* const*))comp_fn("vkResetFences");
    return c_CreateImage && c_GetImageMemReq && c_AllocateMemory && c_BindImageMemory && c_GetPhysMemProps &&
           c_CreateCommandPool && c_AllocCmdBufs && c_BeginCmd && c_EndCmd && c_ResetCmd && c_CmdBarrier &&
           c_CreateSemaphore && c_CreateFence && c_WaitFences && c_ResetFences;
}

// VkImageMemoryBarrier (72 B): srcAccess@16 dstAccess@20 oldLayout@24 newLayout@28 srcQF@32 dstQF@36 image@40
// subresourceRange@48 {aspect, baseMip, levelCount, baseLayer, layerCount}
static void comp_barrier(void* cmd, void* img, unsigned oldL, unsigned newL, unsigned srcAcc, unsigned dstAcc,
                         unsigned srcStage, unsigned dstStage) {
    unsigned char b[72]; memset(b, 0, sizeof b);
    *(unsigned*)(b + 0) = 45;                         // VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER
    *(unsigned*)(b + 16) = srcAcc; *(unsigned*)(b + 20) = dstAcc;
    *(unsigned*)(b + 24) = oldL;  *(unsigned*)(b + 28) = newL;
    *(unsigned*)(b + 32) = 0xffffffffu; *(unsigned*)(b + 36) = 0xffffffffu;
    *(void**)(b + 40) = img;
    *(unsigned*)(b + 48) = 1; *(unsigned*)(b + 56) = 1; *(unsigned*)(b + 64) = 1;   // colour, 1 mip, 1 layer
    c_CmdBarrier(cmd, srcStage, dstStage, 0, 0, 0, 0, 0, 1, b);
}

// Called from vkGetSwapchainImagesKHR once the images are known.
static void comp_create(void* dev, unsigned n) {
    if (g_comp_disable) return;
    if (g_comp_on) {
        if (n == g_comp_n && dev == g_comp_dev
            && g_comp_w==g_comp_created_w && g_comp_h==g_comp_created_h) {
            if (g_comp_swapchain_generation==g_csc_calls
                && !memcmp(g_comp_window_img,g_own_imgs,n*sizeof g_own_imgs[0])) return;
            void* semaphores[COMP_MAX]={0};
            unsigned char sci[24]={0};*(unsigned*)sci=9;
            for (unsigned i=0;i<n;i++) {
                int result=c_CreateSemaphore(dev,sci,0,&semaphores[i]);
                if (result || !semaphores[i]) {
                    g_comp_last_err=result ? result : -3;g_comp_fail++;
                    g_comp_on=0;g_comp_disable=1;
                    LOGF("COMP: replacement present semaphore %u failed %d",i,g_comp_last_err);return;
                }
            }
            if (!ovl_rebind(g_own_imgs,n)) {
                g_comp_on=0;g_comp_disable=1;g_comp_fail++;g_comp_last_err=-3;
                LOGF("COMP: replacement overlay rebind failed; compositor disabled");return;
            }
            // shortcut: old presents can outlive submit fences; retain retired semaphores until process exit, reclaim with presentation fences.
            memcpy(g_comp_sem,semaphores,n*sizeof semaphores[0]);
            memcpy(g_comp_window_img,g_own_imgs,n*sizeof g_own_imgs[0]);
            g_comp_swapchain_generation=g_csc_calls;
            LOGF("COMP: replacement WSI images, keeping %u proxies with fresh present semaphores",n);
            return;
        }
        LOGF("COMP: swapchain changed (n %u->%u extent %ux%u->%ux%u) - compositor disabled",
             g_comp_n,n,g_comp_created_w,g_comp_created_h,g_comp_w,g_comp_h);
        g_comp_on = 0; g_comp_disable = 1; return;
    }
    if (!n || n > COMP_MAX || !g_comp_w || !g_comp_h || !g_phys_dev) { LOGF("COMP: bad params n=%u %ux%u phys=%p", n, g_comp_w, g_comp_h, g_phys_dev); return; }
    if (!comp_resolve()) { LOGF("COMP: missing Vulkan entry points"); g_comp_disable = 1; return; }
    g_comp_mutable=__atomic_load_n(&g_comp_format_list,__ATOMIC_ACQUIRE);
    unsigned char mp[528]; memset(mp, 0, sizeof mp);   // VkPhysicalDeviceMemoryProperties = 520 B
    c_GetPhysMemProps(g_phys_dev, mp);
    unsigned ntypes = *(unsigned*)mp;
    unsigned char pci[24]; memset(pci, 0, sizeof pci);
    *(unsigned*)pci = 39; *(unsigned*)(pci + 16) = 2; *(unsigned*)(pci + 20) = 0;   // RESET_COMMAND_BUFFER, family 0
    int r = c_CreateCommandPool(dev, pci, 0, &g_comp_pool);
    if (r != 0) { LOGF("COMP: command pool failed %d", r); g_comp_disable = 1; return; }
    for (unsigned i = 0; i < n; i++) {
        unsigned char ic[88]; memset(ic, 0, sizeof ic);   // VkImageCreateInfo
        *(unsigned*)(ic + 0) = 14; *(unsigned*)(ic + 20) = 1;
        *(unsigned*)(ic + 24) = g_comp_mutable ? 44u : g_comp_fmt; // Native UE4 uses linear storage with an sRGB scene view.
        _Alignas(8) unsigned char list[32]={0};unsigned view_formats[2]={44,50};
        if (g_comp_mutable) {
            *(unsigned*)list=1000147000u;*(unsigned*)(list+16)=2;
            *(unsigned**)(list+24)=view_formats;
            *(void**)(ic+8)=list;
            *(unsigned*)(ic+16)=8; // Restrict mutable storage to the native linear/sRGB BGRA pair.
        }
        *(unsigned*)(ic + 28) = g_comp_w; *(unsigned*)(ic + 32) = g_comp_h; *(unsigned*)(ic + 36) = 1;
        *(unsigned*)(ic + 40) = 1; *(unsigned*)(ic + 44) = 1; *(unsigned*)(ic + 48) = 1; *(unsigned*)(ic + 52) = 0;
        *(unsigned*)(ic + 56) = 0x10u | 0x1u | 0x2u | 0x4u | g_comp_usage_extra;   // COLOR_ATT|XFER_SRC|XFER_DST|SAMPLED
        r = c_CreateImage(dev, ic, 0, &g_comp_img[i]);
        if (r != 0) { LOGF("COMP: image %u failed %d", i, r); g_comp_disable = 1; return; }
        unsigned char req[24]; memset(req, 0, sizeof req);   // VkMemoryRequirements: size@0 align@8 bits@16
        c_GetImageMemReq(dev, g_comp_img[i], req);
        unsigned bits = *(unsigned*)(req + 16), type = 0xffffffffu;
        for (unsigned t = 0; t < ntypes && t < 32; t++)
            if ((bits & (1u << t)) && (*(unsigned*)(mp + 4 + t * 8) & 1u)) { type = t; break; }   // DEVICE_LOCAL
        if (type == 0xffffffffu) for (unsigned t = 0; t < 32; t++) if (bits & (1u << t)) { type = t; break; }
        unsigned char ai[32]; memset(ai, 0, sizeof ai);   // VkMemoryAllocateInfo
        *(unsigned*)ai = 5; *(unsigned long*)(ai + 16) = *(unsigned long*)req; *(unsigned*)(ai + 24) = type;
        r = c_AllocateMemory(dev, ai, 0, &g_comp_mem[i]);
        if (r == 0) r = c_BindImageMemory(dev, g_comp_img[i], g_comp_mem[i], 0);
        if (r != 0) { LOGF("COMP: memory %u failed %d (size=%lu type=%u)", i, r, *(unsigned long*)req, type); g_comp_disable = 1; return; }
        unsigned char sci[24]; memset(sci, 0, sizeof sci); *(unsigned*)sci = 9;
        unsigned char fci[24]; memset(fci, 0, sizeof fci); *(unsigned*)fci = 8; *(unsigned*)(fci + 16) = 1;   // signaled
        if (c_CreateSemaphore(dev, sci, 0, &g_comp_sem[i]) != 0 || c_CreateFence(dev, fci, 0, &g_comp_fence[i]) != 0) {
            LOGF("COMP: sync objects %u failed", i); g_comp_disable = 1; return;
        }
        g_comp_init[i] = 0; g_comp_drawn[i] = 0;
        LOGF("COMP: proxy %u img=%p mem=%lu type=%u", i, g_comp_img[i], *(unsigned long*)req, type);
    }
    unsigned char cai[32]; memset(cai, 0, sizeof cai);   // VkCommandBufferAllocateInfo
    *(unsigned*)cai = 40; *(void**)(cai + 16) = g_comp_pool; *(unsigned*)(cai + 24) = 0; *(unsigned*)(cai + 28) = n;
    r = c_AllocCmdBufs(dev, cai, g_comp_cmd);
    if (r != 0) { LOGF("COMP: command buffers failed %d", r); g_comp_disable = 1; return; }
    g_comp_dev = dev; g_comp_n = n;
    g_comp_created_w=g_comp_w;g_comp_created_h=g_comp_h;
    g_comp_swapchain_generation=g_csc_calls;
    memcpy(g_comp_window_img,g_own_imgs,n*sizeof g_own_imgs[0]);
    __sync_synchronize();
    g_comp_on = 1;
    LOGF("COMP: %u proxies %ux%u fmt=%u ready", n, g_comp_w, g_comp_h, g_comp_fmt);
    int ok = ovl_init((void* (*)(void*, const char*))r_gipa, g_vk_instance, g_phys_dev, dev, 0, g_comp_w, g_comp_h,
                      g_sc_fmt_real ? g_sc_fmt_real : 43, n, g_own_imgs, g_comp_img,g_comp_mutable);
    LOGF("COMP: overlay init -> %d", ok);
    if (!ok) { g_comp_on=0;g_comp_disable=1; }
}

// vkCreateImageView on swapchain image i (engine swapchain format) -> same view on proxy i, proxy format.
static const void* comp_view_ci(const void* ci, int sw, unsigned char* buf80) {
    if (!g_comp_on || sw < 0 || (unsigned)sw >= g_comp_n) return ci;
    memcpy(buf80, ci, 80);
    *(void**)(buf80 + 24) = g_comp_img[sw];
    *(unsigned*)(buf80 + 36) = g_comp_fmt;
    g_comp_views++;
    return buf80;
}

// From vkCmdBeginRenderPass when the framebuffer targets proxy sw: first use -> UNDEFINED -> COLOR_ATTACHMENT.
static void comp_begin_pass(void* cmd, int sw) {
    if (!g_comp_on || sw < 0 || (unsigned)sw >= g_comp_n) return;
    if (!g_comp_init[sw]) {
        comp_barrier(cmd, g_comp_img[sw], VK_LAYOUT_UNDEFINED, VK_LAYOUT_COLOR_ATT, 0, 0x180, 0x1, 0x400);
        g_comp_init[sw] = 1; g_comp_inits++;
    }
    g_comp_drawn[sw] = 1;
    __atomic_fetch_add(&g_comp_pass_serial[sw],1,__ATOMIC_RELAXED);
}

// Before the real present: sample proxy and draw controls in one pass; returns a patched
// VkPresentInfoKHR (in pbuf, 64 B) whose single wait semaphore is our pass-done semaphore.
// VkPresentInfoKHR: waitCount@16 pWaits@24 swapchainCount@32 pSwapchains@40 pImageIndices@48 pResults@56
static const void* comp_present(void* q, const void* si, unsigned char* pbuf, void** semslot) {
    if (!g_comp_on || !si) return si;
    unsigned nsc = *(const unsigned*)((const char*)si + 32);
    void* const* psc = *(void* const* const*)((const char*)si + 40);
    const unsigned* pix = *(const unsigned* const*)((const char*)si + 48);
    if (nsc != 1 || !psc || !pix || psc[0] != g_window_sc) return si;
    unsigned i = pix[0];
    if (i >= g_comp_n || !g_comp_init[i] || !g_comp_drawn[i]) { g_comp_skip++; return si; }
    unsigned nw = *(const unsigned*)((const char*)si + 16);
    void* const* ws = *(void* const* const*)((const char*)si + 24);
    if (nw>16 || (nw && !ws)) return si;
    int r = c_WaitFences(g_comp_dev, 1, &g_comp_fence[i], 1, 2000000000ul);
    if (r != 0) { g_comp_fail++; g_comp_last_err = r; return si; }
    void* cmd = g_comp_cmd[i];
    r=c_ResetCmd(cmd, 0);
    if (r != 0) { g_comp_fail++; g_comp_last_err = r; return si; }
    unsigned char bi[32]; memset(bi, 0, sizeof bi); *(unsigned*)bi = 42; *(unsigned*)(bi + 16) = 1;   // ONE_TIME_SUBMIT
    r = c_BeginCmd(cmd, bi);
    if (r != 0) { g_comp_fail++; g_comp_last_err = r; return si; }
    comp_barrier(cmd, g_comp_img[i], VK_LAYOUT_COLOR_ATT, VK_LAYOUT_SHADER_READ, 0x100, 0x20, 0x400, 0x80);
    comp_barrier(cmd, g_own_imgs[i], VK_LAYOUT_UNDEFINED, VK_LAYOUT_COLOR_ATT, 0, 0x100, 0x1, 0x400);
    struct ovl_quad oq[48]; int nq = touch_overlay_quads(oq, 48);
    ovl_record(cmd, i, oq, nq);
    comp_barrier(cmd, g_comp_img[i], VK_LAYOUT_SHADER_READ, VK_LAYOUT_COLOR_ATT, 0x20, 0x180, 0x80, 0x400);
    r = c_EndCmd(cmd);
    if (r != 0) { g_comp_fail++; g_comp_last_err = r; return si; }
    r=c_ResetFences(g_comp_dev, 1, &g_comp_fence[i]);
    if (r != 0) { g_comp_fail++; g_comp_last_err = r; return si; }
    unsigned stages[16]; for (int k = 0; k < 16; k++) stages[k] = 0x1u;   // Gate the complete sampled pass and lazy atlas upload.
    unsigned char sub[72]; memset(sub, 0, sizeof sub);   // VkSubmitInfo
    *(unsigned*)sub = 4; *(unsigned*)(sub + 16) = nw; *(void* const**)(sub + 24) = ws; *(unsigned**)(sub + 32) = stages;
    *(unsigned*)(sub + 40) = 1; *(void**)(sub + 48) = &g_comp_cmd[i];
    *(unsigned*)(sub + 56) = 1; *(void**)(sub + 64) = &g_comp_sem[i];
    r = r_vkQueueSubmit2(q, 1, sub, g_comp_fence[i]);
    if (r != 0) {
        g_comp_fail++; g_comp_last_err = r;
        if (g_comp_fail <= 5) LOGF("COMP: submit failed %d", r);
        // the fence was reset and nothing will signal it any more: turn the compositor off instead of
        // deadlocking in vkWaitForFences on the next present of this image
        g_comp_on = 0; g_comp_disable = 1;
        return si;
    }
    memcpy(pbuf, si, 64);
    *(unsigned*)(pbuf + 16) = 1; *semslot = g_comp_sem[i]; *(void***)(pbuf + 24) = semslot;
    g_comp_blits++;
    unsigned long serial=__atomic_load_n(&g_comp_pass_serial[i],__ATOMIC_RELAXED);
    unsigned long fresh=serial-g_comp_present_serial[i];g_comp_present_serial[i]=serial;
    if (g_comp_fresh_logs<12 || (!fresh && g_comp_gap_logs<16) || !(g_comp_blits%1800)) {
        ++g_comp_fresh_logs;if(!fresh)++g_comp_gap_logs;
        LOGF("COMPFRAME frame=%ld idx=%u fresh=%lu serial=%lu views=%d rps=%d fbs=%d",
            g_comp_blits,i,fresh,serial,g_nviews,g_nrps,g_nfbs);
    }
    if (g_comp_blits <= 3) LOGF("COMP: sampled frame #%ld proxy %u -> swapchain img %p", g_comp_blits, i, g_own_imgs[i]);
    return pbuf;
}
