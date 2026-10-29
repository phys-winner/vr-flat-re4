#ifndef RE4FIX_COMP_FRAMEBUFFER_LIFECYCLE_H
#define RE4FIX_COMP_FRAMEBUFFER_LIFECYCLE_H
#include <stdint.h>
#include <string.h>

unsigned long g_tgt_comp_fb_match,g_tgt_comp_fb_depth;
static unsigned long comp_fb_lifecycle_base;
static const struct {unsigned long address;unsigned count,words[10];} comp_fb_lifecycle_checks[]={
    {0x65257a0,8,{0xd10203ff,0xa9026ffc,0xa90367fa,0xa9045ff8,0xa90557f6,0xa9064ff4,0xa9077bfd,0x9101c3fd}},
    {0x65258f4,3,{0xf94002e9,0xd1000519,0xf8797938}},
    {0x6525900,10,{0xb9405f0a,0x3400010a,0x9101830b,0xf940016c,0xeb13019f,0x540000e0,0xf100054a,0x9100216b,0x54ffff61,0xf940730a}},
    {0x655af14,6,{0xad070000,0xad030000,0xad040000,0xad050000,0xad060000,0x290b7c08}},
    {0x655afc0,5,{0xf9400c08,0x8b170e69,0x9b146aea,0xaa0003fc,0xf9003128}},
    {0x655b1a8,3,{0xf9400c08,0x8b170e69,0xf9005128}},
    {0x655ba24,7,{0xf9400008,0xf9405319,0xf9402908,0xd63f0100,0xf9400c08,0xeb08033f,0x54000181}},
    {0x6525960,9,{0xaa1403e1,0x51000508,0xb9000ae8,0x9400d793,0xf940eaa8,0xeb08031f,0x54fffb01,0xf900eabf,0x17ffffd6}},
    {0x655b7e0,8,{0x528000e1,0xf90007e8,0xf9402402,0xaa1503e0,0x97fea722,0xb9403268,0xf900267f,0x7100051f}},
};

#ifndef RE4FIX_COMP_FB_SCAN_GATE
__attribute__((naked)) static void gate_comp_fb_resolve_scan(void) {
    // Resolve images occupy the second initialized eight-entry array; NULL keeps the original color-count path.
    asm volatile(
        ".inst 0xd503249f\n" // BTI j: entry is reached through patch_code's indirect jump.
        "ldr w10, [x24, #0x5c]\n"
        "cbz x19, 1f\n"
        "mov w10, #16\n"
        "1: cbz w10, 2f\n"
        "add x11, x24, #0x60\n"
        "3: ldr x12, [x11]\n"
        "cmp x12, x19\n"
        "b.eq 4f\n"
        "subs x10, x10, #1\n"
        "add x11, x11, #8\n"
        "b.ne 3b\n"
        "b 2f\n"
        "4: adrp x16, :got:g_tgt_comp_fb_match\n"
        "ldr x16, [x16, :got_lo12:g_tgt_comp_fb_match]\n"
        "ldr x16, [x16]\n"
        "br x16\n"
        "2: adrp x16, :got:g_tgt_comp_fb_depth\n"
        "ldr x16, [x16, :got_lo12:g_tgt_comp_fb_depth]\n"
        "ldr x16, [x16]\n"
        "br x16\n");
}
#else
#define gate_comp_fb_resolve_scan RE4FIX_COMP_FB_SCAN_GATE
#endif

static int install_comp_framebuffer_lifecycle(unsigned long base) {
    if(!base)return 0;
    if(comp_fb_lifecycle_base)return comp_fb_lifecycle_base==base;
    for(unsigned i=0;i<sizeof comp_fb_lifecycle_checks/sizeof *comp_fb_lifecycle_checks;i++)
        if(memcmp((const void*)(uintptr_t)(base+comp_fb_lifecycle_checks[i].address),comp_fb_lifecycle_checks[i].words,comp_fb_lifecycle_checks[i].count*4)) {
            LOGF("COMP framebuffer lifecycle disabled: native opcode mismatch +%lx",comp_fb_lifecycle_checks[i].address);return 0;
        }
    g_tgt_comp_fb_match=base+0x6525930;g_tgt_comp_fb_depth=base+0x6525924;
    __sync_synchronize();
    if(patch_code((void*)(uintptr_t)(base+0x6525900),(void*)&gate_comp_fb_resolve_scan)) {
        g_tgt_comp_fb_match=g_tgt_comp_fb_depth=0;
        LOGF("COMP framebuffer lifecycle disabled: native hook installation failed");return 0;
    }
    comp_fb_lifecycle_base=base;
    LOGF("COMP framebuffer lifecycle ready: native deletion covers color/resolve arrays and preserves NULL behavior");return 1;
}
#endif
