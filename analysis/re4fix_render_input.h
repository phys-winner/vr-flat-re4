// Android touch coordinates stay in View pixels; only the input callback's normalization denominator changes.
#ifndef RE4FIX_RENDER_INPUT_H
#define RE4FIX_RENDER_INPUT_H
#include <math.h>
#include <stdint.h>
#include <string.h>

static volatile uint64_t render_input_geometry;
static uintptr_t render_input_base;
static unsigned render_input_logs;
unsigned long g_tgt_render_input_calculate;

__attribute__((visibility("default")))
void Java_com_epicgames_ue4_MobileSettings_nativeViewportSizes(void* env,void* type,
    int physical_w,int physical_h,int render_w,int render_h) {
    (void)env;(void)type;
    if(physical_w<32||physical_h<32||physical_w>16384||physical_h>16384
        ||render_w<32||render_h<32||render_w>physical_w||render_h>physical_h
        ||(render_w&7)||(render_h&7)||render_w*2+16<physical_w||render_h*2+16<physical_h)return;
    int64_t aspect=(int64_t)render_w*physical_h-(int64_t)render_h*physical_w;
    if(aspect>8*(physical_w+physical_h)||aspect< -8*(physical_w+physical_h))return;
    uint64_t packed=(uint64_t)physical_w|((uint64_t)physical_h<<16)
        |((uint64_t)render_w<<32)|((uint64_t)render_h<<48);
    uint64_t before=__atomic_exchange_n(&render_input_geometry,packed,__ATOMIC_ACQ_REL);
    if(before!=packed&&__atomic_fetch_add(&render_input_logs,1,__ATOMIC_RELAXED)<8)
        LOGF("RENDER INPUT view=%dx%d render=%dx%d",physical_w,physical_h,render_w,render_h);
}

// Native normalization uses extent-1; preserve the existing 100% pixel calibration at every render scale.
static float render_input_touch_x(float delta,float render_width) {
    if(!isfinite(delta))return 0;
    unsigned physical=(unsigned)(__atomic_load_n(&render_input_geometry,__ATOMIC_ACQUIRE)&0xffffu);
    if(!physical||!isfinite(render_width)||render_width<32||render_width>physical||render_width<physical*.5f-16)return delta;
    return delta*(physical-1)/(render_width-1);
}
static float render_input_touch_y(float delta,float render_height) {
    if(!isfinite(delta))return 0;
    unsigned physical=(unsigned)((__atomic_load_n(&render_input_geometry,__ATOMIC_ACQUIRE)>>16)&0xffffu);
    if(!physical||!isfinite(render_height)||render_height<32||render_height>physical||render_height<physical*.5f-16)return delta;
    return delta*(physical-1)/(render_height-1);
}

#ifndef RE4_RENDER_INPUT_CALCULATE_GATE
__attribute__((naked)) static void gate_render_input_calculate(int* width,int* height,int hardware) {
    asm volatile(".inst 0xd105c3ff\n" ".inst 0xf90093fc\n" ".inst 0xa9135ff8\n" ".inst 0xa91457f6\n"
        "adrp x16, :got:g_tgt_render_input_calculate\n"
        "ldr x16, [x16, :got_lo12:g_tgt_render_input_calculate]\n"
        "ldr x16, [x16]\n" "br x16\n");
}
#else
#define gate_render_input_calculate RE4_RENDER_INPUT_CALCULATE_GATE
#endif

static __attribute__((noinline)) void hook_render_input_calculate(int* width,int* height,int hardware) {
    uintptr_t caller=(uintptr_t)__builtin_return_address(0);
    gate_render_input_calculate(width,height,hardware);
    if(!render_input_base||caller!=render_input_base+0x54beb14ul||!hardware||!width||!height
        ||width==height||*width<=0||*height<=0||*width>16384||*height>16384)return;
    uint64_t geometry=__atomic_load_n(&render_input_geometry,__ATOMIC_ACQUIRE);
    if(!geometry)return;
    // The original GetScreenRect still supplies the scaled render rect later in this same callback.
    *width=(int)(geometry&0xffffu);*height=(int)((geometry>>16)&0xffffu);
}

static const struct {uintptr_t address;unsigned count,words[12];} render_input_checks[]={
    {0x6a1dec0,4,{0xd105c3ff,0xf90093fc,0xa9135ff8,0xa91457f6}},
    {0x54beafc,6,{0x910093e0,0x910083e1,0x52800022,0xf90013ff,0x52800038,0x94557cec}},
    {0x54bef20,8,{0x951cd0a8,0xbd4027e1,0xaa1303e0,0xaa1603e1,0x5e21d821,0x1e211800,0x1e287800,0x1e29080b}},
    {0x54bef98,12,{0x951cd08a,0xbd4027e1,0x1e2e1009,0x510006e8,0xaa1303e0,0x5e21d821,0x1e211800,0x1e297800,0x1e220101,0xaa1803e1,0x1e210808,0x951cd083}},
};

static int install_render_input(uintptr_t base) {
    if(!base)return 0;
    if(render_input_base)return render_input_base==base;
    for(unsigned i=0;i<sizeof render_input_checks/sizeof *render_input_checks;i++)
        if(memcmp((const void*)(base+render_input_checks[i].address),render_input_checks[i].words,render_input_checks[i].count*4)) {
            LOGF("RENDER INPUT disabled: native preimage mismatch +%lx",(unsigned long)render_input_checks[i].address);return 0;
        }
    g_tgt_render_input_calculate=(unsigned long)(base+0x6a1dec0+16);
    __sync_synchronize();
    if(patch_code((void*)(base+0x6a1dec0),(void*)&hook_render_input_calculate)) {
        g_tgt_render_input_calculate=0;LOGF("RENDER INPUT disabled: hook installation failed");return 0;
    }
    render_input_base=base;
    LOGF("RENDER INPUT ready: physical View denominator only at Android touch callback +54beb14; render callers unchanged");return 1;
}
#endif
