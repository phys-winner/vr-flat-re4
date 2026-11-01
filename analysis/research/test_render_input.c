#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int original_calls,original_w=1200,original_h=536,patch_calls,patch_error;
static uintptr_t test_caller,patched_address;static void* patched_hook;
static void original(int* width,int* height,int hardware) {
    (void)hardware;++original_calls;if(width)*width=original_w;if(height)*height=original_h;
}
static int patch_code(void* address,void* hook) {
    ++patch_calls;patched_address=(uintptr_t)address;patched_hook=hook;return patch_error;
}
#define LOGF(...) ((void)0)
#define RE4_RENDER_INPUT_CALCULATE_GATE original
#define __builtin_return_address(level) ((void*)test_caller)
#include "../re4fix_render_input.h"
#undef __builtin_return_address
static void viewport(int pw,int ph,int rw,int rh) { Java_com_epicgames_ue4_MobileSettings_nativeViewportSizes(0,0,pw,ph,rw,rh); }
static void invoke(int* w,int* h,int hardware) { int before=original_calls;hook_render_input_calculate(w,h,hardware);assert(original_calls==before+1); }
static float native_coordinate(float raw,int denominator,int render) { return fminf(raw/denominator,1)*(render-1); }
int main(void) {
    render_input_base=0x70000000u;test_caller=render_input_base+0x54beb14;int w,h;
    invoke(&w,&h,1);assert(w==1200&&h==536);
    viewport(2400,1080,1200,536);invoke(&w,&h,1);assert(w==2400&&h==1080);
    assert(native_coordinate(1900,1200,1200)==1199&&native_coordinate(900,536,536)==535);
    assert(fabsf(native_coordinate(1900,w,1200)-949.208333f)<.001f);
    assert(fabsf(native_coordinate(900,h,536)-445.833333f)<.001f);
    assert(fabsf(native_coordinate(1560,w,1200)-779.35f)<.001f);
    assert(fabsf(native_coordinate(155,h,536)-76.782407f)<.001f);
    for(int scale=50;scale<=100;scale++) {
        int rw=2400*scale/800*8,rh=1080*scale/800*8;
        viewport(2400,1080,rw,rh);original_w=rw;original_h=rh;
        assert(((render_input_geometry>>32)&0xffffu)==(unsigned)rw&&((render_input_geometry>>48)&0xffffu)==(unsigned)rh);
        invoke(&w,&h,1);assert(w==2400&&h==1080);
        assert(fabsf(native_coordinate(1900,w,rw)/(rw-1)-1900.0f/2400)<1e-6f);
        assert(fabsf(native_coordinate(900,h,rh)/(rh-1)-900.0f/1080)<1e-6f);
    }
    const int panels[][2]={{2400,1080},{2414,1080},{2415,1081},{1080,2414},{33,35}};
    const int scales[]={50,67,75,85,100};
    for(unsigned panel=0;panel<sizeof panels/sizeof *panels;panel++)for(unsigned scale=0;scale<sizeof scales/sizeof *scales;scale++) {
        int pw=panels[panel][0],ph=panels[panel][1],rw=pw*scales[scale]/800*8,rh=ph*scales[scale]/800*8;
        if(rw<32)rw=32;if(rh<32)rh=32;
        viewport(pw,ph,rw,rh);
        assert((render_input_geometry&0xffffu)==(unsigned)pw&&((render_input_geometry>>16)&0xffffu)==(unsigned)ph);
        assert(((render_input_geometry>>32)&0xffffu)==(unsigned)rw&&((render_input_geometry>>48)&0xffffu)==(unsigned)rh);
        original_w=rw;original_h=rh;invoke(&w,&h,1);assert(w==pw&&h==ph);
    }
    original_w=2040;original_h=912;viewport(2400,1080,2040,912);
    invoke(&w,&h,1);assert(w==2400&&h==1080); // Native aligned render rect stays independent of the input denominator.
    uintptr_t saved=test_caller;test_caller++;invoke(&w,&h,1);assert(w==2040&&h==912);test_caller=saved;
    invoke(&w,&h,0);assert(w==2040&&h==912);
    invoke(&w,0,1);assert(w==2040);invoke(0,&h,1);assert(h==912);invoke(0,0,1);
    invoke(&w,&w,1);assert(w==912);
    original_w=0;invoke(&w,&h,1);assert(!w&&h==912);original_w=2040;
    original_h=0;invoke(&w,&h,1);assert(w==2040&&!h);original_h=912;
    viewport(2400,1080,1200,536);
    assert(fabsf(render_input_touch_x(10.0f*1199/2400,1200)-10.0f*2399/2400)<1e-5f);
    assert(fabsf(render_input_touch_y(10.0f*535/1080,536)-10.0f*1079/1080)<1e-5f);
    viewport(2400,1080,2040,912);
    assert(fabsf(render_input_touch_x(80.0f*2039/2400,2040)-80.0f*2399/2400)<1e-5f);
    assert(fabsf(render_input_touch_y(80.0f*911/1080,912)-80.0f*1079/1080)<1e-5f);
    assert(render_input_touch_x(30,2400)==30&&render_input_touch_y(30,1080)==30);
    assert(render_input_touch_x(30,0)==30&&render_input_touch_y(30,NAN)==30);
    assert(render_input_touch_x(30,2401)==30&&render_input_touch_y(30,400)==30);
    assert(!render_input_touch_x(INFINITY,1200)&&!render_input_touch_y(NAN,536));
    uint64_t before=render_input_geometry;
    const int bad[][4]={{0,1080,1200,536},{2400,0,1200,536},{16385,1080,1200,536},{2400,16385,1200,536},
        {2400,1080,2401,536},{2400,1080,1200,1081},{2400,1080,1198,536},{2400,1080,1200,538},
        {2400,1080,1200,1080},{2400,1080,1,1},{2400,1080,1800,810},{2414,1080,2414,1080},
        {2400,1080,1184,536},{2400,1080,1200,528},{2400,1080,1200,540},{2414,1080,2412,1080}};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;i++){viewport(bad[i][0],bad[i][1],bad[i][2],bad[i][3]);assert(render_input_geometry==before);}
    viewport(1080,2400,536,1200);invoke(&w,&h,1);assert(w==1080&&h==2400);
    viewport(2400,1080,2400,1080);original_w=2400;original_h=1080;invoke(&w,&h,1);
    assert(w==2400&&h==1080&&native_coordinate(1900,w,2400)==native_coordinate(1900,2400,2400));
    render_input_base=0;invoke(&w,&h,1);assert(w==2400&&h==1080);
    render_input_geometry=0;assert(render_input_touch_x(30,1200)==30&&render_input_touch_y(30,540)==30);
    unsigned char* image=calloc(1,0x6a1dec0+32);assert(image);uintptr_t base=(uintptr_t)image;
    for(unsigned i=0;i<sizeof render_input_checks/sizeof *render_input_checks;i++)
        memcpy(image+render_input_checks[i].address,render_input_checks[i].words,render_input_checks[i].count*4);
    assert(!install_render_input(0)&&!patch_calls);
    image[0x54beafc]^=1;assert(!install_render_input(base)&&!patch_calls);image[0x54beafc]^=1;
    patch_error=1;assert(!install_render_input(base)&&patch_calls==1&&!render_input_base&&!g_tgt_render_input_calculate);
    patch_error=0;assert(install_render_input(base)&&patch_calls==2&&render_input_base==base);
    assert(g_tgt_render_input_calculate==(unsigned long)(base+0x6a1dec0+16)&&patched_address==base+0x6a1dec0&&patched_hook==(void*)hook_render_input_calculate);
    assert(install_render_input(base)&&patch_calls==2&&!install_render_input(base+4));free(image);
    puts("PASS: production render-input geometry validation/atomic snapshot, original-first exact-caller-only normalization, reproduced clipped edge and corrected50..100%, aligned native rect,100%identity, unrelated/hardware0/null/zero/notready declines and guarded/idempotent installation/error propagation");
}
