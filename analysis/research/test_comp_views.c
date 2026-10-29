#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
// Values supplied from the installed official NDK vulkan_core.h by the checker.
_Static_assert(RE4_SDK_VK_SWIZZLE_IDENTITY==0, "SDK identity swizzle");
_Static_assert(RE4_SDK_VK_SWIZZLE_R==3 && RE4_SDK_VK_SWIZZLE_G==4
    && RE4_SDK_VK_SWIZZLE_B==5 && RE4_SDK_VK_SWIZZLE_A==6, "SDK explicit identity swizzles");
static const unsigned sdk_identity_components[]={RE4_SDK_VK_SWIZZLE_R,
    RE4_SDK_VK_SWIZZLE_G,RE4_SDK_VK_SWIZZLE_B,RE4_SDK_VK_SWIZZLE_A};
#define COMP_MAX 8
struct rptrack { void* rp; unsigned n; unsigned fmt0, load0, store0, init0, final0, dload, fmt1, init1, final1; };
struct vtrack { void* view; int sw; unsigned fmt; unsigned char info[80]; };
static struct rptrack g_rps[256]; static int g_nrps;
static struct vtrack g_views[64]; static int g_nviews;
static int g_comp_on,g_comp_disable,g_comp_mutable; static void* g_comp_dev;
static unsigned g_comp_n,g_comp_w,g_comp_h;
static void* g_comp_img[COMP_MAX];
static int (*r_vkCreateImageView)(void*,const void*,const void*,void**);
static int swidx_of_view(void* view) {
    for(int k=g_nviews-1;k>=0 && k>=g_nviews-64;k--)
        if(g_views[k%64].view==view)return g_views[k%64].sw;
    return -1;
}
static struct rptrack* rp_find(void* rp) {
    for(int k=g_nrps-1;k>=0 && k>=g_nrps-256;k--)
        if(g_rps[k%256].rp==rp)return &g_rps[k%256];
    return 0;
}
#include "../re4fix_comp_views.h"
static _Alignas(8) unsigned char ci[64],out_ci[64];
static void* atts[8]; static void* out_atts[8];
static const void* selected;
static int calls,result,success_null;
static unsigned requested_sw;
static int create_view(void* dev,const void* data,const void* allocator,void** out) {
    assert(dev==g_comp_dev && !allocator); calls++;
    const unsigned char* p=data;
    assert(*(const unsigned*)p==15 && !*(void* const*)(p+8));
    assert(*(void* const*)(p+24)==g_comp_img[requested_sw]);
    assert(*(const unsigned*)(p+32)==1 && *(const unsigned*)(p+36)==44);
    assert(*(const unsigned*)(p+56)==1 && !*(const unsigned*)(p+60));
    assert(*(const unsigned*)(p+64)==1 && !*(const unsigned*)(p+68));
    assert(*(const unsigned*)(p+72)==1);
    unsigned char expected[80];memcpy(expected,g_views[0].info,80);
    unsigned fmt=44;memcpy(expected+36,&fmt,4);assert(!memcmp(expected,p,80));
    *out=success_null ? 0 : (void*)(uintptr_t)(0x8000+requested_sw*0x100);
    return result;
}
static void ready(void) {
    memset(ci,0xa5,sizeof ci); memset(out_ci,0x5a,sizeof out_ci);
    memset(g_rps,0,sizeof g_rps);memset(g_views,0,sizeof g_views);
    memset(comp_linear_views,0,sizeof comp_linear_views); memset(out_atts,0x5a,sizeof out_atts);
    g_comp_on=1;g_comp_disable=0;g_comp_mutable=1;g_comp_dev=(void*)0x1000;g_comp_n=4;g_comp_w=2400;g_comp_h=1080;
    for(unsigned i=0;i<8;i++)g_comp_img[i]=(void*)(uintptr_t)(0x2000+i*0x100);
    g_nrps=1;g_rps[0].rp=(void*)0x4000;g_rps[0].n=1;g_rps[0].fmt0=44;
    g_nviews=1;g_views[0]=(struct vtrack){(void*)0x5000,0,50};
    unsigned char* vi=g_views[0].info;memset(vi,0xa5,80);
    *(unsigned*)vi=15;*(void**)(vi+8)=0;*(unsigned*)(vi+16)=0;
    *(void**)(vi+24)=g_comp_img[0];*(unsigned*)(vi+32)=1;*(unsigned*)(vi+36)=50;
    memset(vi+40,0,16);*(unsigned*)(vi+56)=1;*(unsigned*)(vi+60)=0;
    *(unsigned*)(vi+64)=1;*(unsigned*)(vi+68)=0;*(unsigned*)(vi+72)=1;
    memset(atts,0,sizeof atts);atts[0]=g_views[0].view;
    *(unsigned*)ci=37;*(void**)(ci+8)=0;*(unsigned*)(ci+16)=0;
    *(void**)(ci+24)=g_rps[0].rp;*(unsigned*)(ci+32)=1;*(void***)(ci+40)=atts;
    *(unsigned*)(ci+48)=2400;*(unsigned*)(ci+52)=1080;*(unsigned*)(ci+56)=1;
    selected=0;calls=result=success_null=0;requested_sw=0;r_vkCreateImageView=create_view;
}
static int invoke(void* dev) { return comp_linear_framebuffer(dev,ci,out_ci,out_atts,&selected); }
static void refused(void* dev) {
    unsigned char before_ci[64],before_out[64];void* before_atts[8];void* before_out_atts[8];
    memcpy(before_ci,ci,64);memcpy(before_out,out_ci,64);
    memcpy(before_atts,atts,sizeof atts);memcpy(before_out_atts,out_atts,sizeof out_atts);
    assert(!invoke(dev) && selected==ci && !calls && g_nviews==1);
    assert(!memcmp(before_ci,ci,64) && !memcmp(before_out,out_ci,64));
    assert(!memcmp(before_atts,atts,sizeof atts) && !memcmp(before_out_atts,out_atts,sizeof out_atts));
}
int main(void) {
    ready();unsigned char before_ci[64];void* before_atts[8];memcpy(before_ci,ci,64);memcpy(before_atts,atts,sizeof atts);
    assert(!invoke(g_comp_dev) && selected==out_ci && calls==1 && g_nviews==2);
    assert(out_atts[0]==(void*)0x8000 && *(void***)(out_ci+40)==out_atts);
    assert(!memcmp(ci,before_ci,64) && !memcmp(atts,before_atts,sizeof atts));
    memcpy(out_ci+40,before_ci+40,8);assert(!memcmp(out_ci,before_ci,64));
    assert(swidx_of_view((void*)0x8000)==0 && g_views[1].fmt==44);
    unsigned char expected[80];memcpy(expected,g_views[0].info,80);unsigned linear=44;
    memcpy(expected+36,&linear,4);assert(!memcmp(expected,g_views[1].info,80));
    assert(!invoke(g_comp_dev) && calls==1 && g_nviews==2);
    atts[0]=(void*)0x8000;assert(!invoke(g_comp_dev) && selected==ci && calls==1); // already-correct view
    for(unsigned sw=0;sw<8;sw++) {
        ready();g_comp_n=8;g_views[0].sw=(int)sw;requested_sw=sw;
        *(void**)(g_views[0].info+24)=g_comp_img[sw];
        assert(!invoke(g_comp_dev) && calls==1 && out_atts[0]==(void*)(uintptr_t)(0x8000+sw*0x100));
    }
    ready();g_nviews=65;
    assert(!invoke(g_comp_dev) && calls==1 && g_nviews==66 && g_views[1].fmt==44);
    ready();assert(!invoke(g_comp_dev) && calls==1);
    struct vtrack native_source=g_views[0];memset(g_views,0,sizeof g_views);
    g_nviews=65;g_views[0]=native_source; // live cached alias outlasted its 64-entry tracker slot
    assert(!invoke(g_comp_dev) && calls==1 && g_nviews==66 && swidx_of_view(out_atts[0])==0);
    assert(!memcmp(g_views[1].info,comp_linear_views[0].info,80));
    ready();g_nrps=257;g_rps[0]=(struct rptrack){.rp=(void*)0x4000,.n=1,.fmt0=44};
    assert(!invoke(g_comp_dev) && calls==1);
    ready();g_comp_on=0;refused(g_comp_dev);
    ready();g_comp_disable=1;refused(g_comp_dev);
    ready();g_comp_mutable=0;refused(g_comp_dev);
    ready();g_comp_n=0;refused(g_comp_dev);
    ready();g_comp_n=9;refused(g_comp_dev);
    ready();refused(0);
    ready();refused((void*)0x1001);
    ready();*(unsigned*)ci=0;refused(g_comp_dev);
    ready();*(void**)(ci+8)=(void*)0x9000;refused(g_comp_dev);
    ready();*(unsigned*)(ci+16)=1;refused(g_comp_dev);
    ready();*(unsigned*)(ci+32)=2;refused(g_comp_dev);
    ready();*(unsigned*)(ci+48)=0;refused(g_comp_dev);
    ready();*(unsigned*)(ci+48)=2401;refused(g_comp_dev);
    ready();*(unsigned*)(ci+52)=0;refused(g_comp_dev);
    ready();*(unsigned*)(ci+52)=1081;refused(g_comp_dev);
    ready();*(unsigned*)(ci+56)=2;refused(g_comp_dev);
    ready();*(void**)(ci+24)=(void*)0x9000;refused(g_comp_dev);
    ready();g_rps[0].n=2;refused(g_comp_dev);
    ready();g_rps[0].fmt0=37;refused(g_comp_dev);
    ready();g_rps[0].fmt0=50;refused(g_comp_dev);
    ready();*(void***)(ci+40)=0;refused(g_comp_dev);
    ready();atts[0]=0;refused(g_comp_dev);
    ready();atts[0]=(void*)0x9000;refused(g_comp_dev);
    ready();g_views[0].sw=-1;refused(g_comp_dev);
    ready();g_views[0].sw=4;refused(g_comp_dev);
    ready();g_views[0].sw=8;g_comp_n=9;refused(g_comp_dev);
    ready();g_comp_img[0]=0;refused(g_comp_dev);
    ready();g_views[0].fmt=44;refused(g_comp_dev);
    ready();g_views[0].fmt=37;refused(g_comp_dev);
    ready();memset(g_views[0].info,0,80);refused(g_comp_dev);
    ready();*(unsigned*)g_views[0].info=0;refused(g_comp_dev);
    ready();*(void**)(g_views[0].info+8)=(void*)0x9000;refused(g_comp_dev);
    ready();*(void**)(g_views[0].info+24)=(void*)0x9000;refused(g_comp_dev);
    for(int i=0;i<7;i++) {
        const int offsets[]={32,36,56,60,64,68,72};
        ready();*(unsigned*)(g_views[0].info+offsets[i])=2;refused(g_comp_dev);
    }
    for(int i=0;i<4;i++) {
        ready();*(unsigned*)(g_views[0].info+40+4*i)=1;refused(g_comp_dev);
        ready();*(unsigned*)(g_views[0].info+40+4*i)=4+i;refused(g_comp_dev); // prior off-by-one mapping
        ready();*(unsigned*)(g_views[0].info+40+4*i)=sdk_identity_components[i];
        *(unsigned*)(g_views[0].info+16)=1; // copied native successful creation flags
        assert(!invoke(g_comp_dev) && calls==1);
    }
    ready();memcpy(g_views[0].info+40,sdk_identity_components,sizeof sdk_identity_components);
    assert(!invoke(g_comp_dev) && calls==1);
    ready();*(unsigned*)(g_views[0].info+40)=RE4_SDK_VK_SWIZZLE_B;
    *(unsigned*)(g_views[0].info+48)=RE4_SDK_VK_SWIZZLE_R;refused(g_comp_dev); // actual R/B exchange
    ready();g_nviews=65;g_views[0]=(struct vtrack){0,0,0}; // stale ring entries are absent
    assert(!invoke(g_comp_dev) && selected==ci && !calls);
    ready();result=-2;assert(invoke(g_comp_dev)==-2 && selected==ci && calls==1 && !comp_linear_views[0].view && g_nviews==1);
    result=0;assert(!invoke(g_comp_dev) && calls==2 && selected==out_ci);
    ready();success_null=1;assert(invoke(g_comp_dev)==-3 && selected==ci && !comp_linear_views[0].view && g_nviews==1);
    ready();r_vkCreateImageView=0;assert(invoke(g_comp_dev)==-3 && selected==ci && !calls);
    ready();assert(!invoke(g_comp_dev) && calls==1);*(unsigned*)(g_views[0].info+16)=1;
    assert(invoke(g_comp_dev)==-11 && selected==ci && calls==1);
    ready();comp_linear_views[0].view=(void*)0x8000;comp_linear_views[0].dev=g_comp_dev;comp_linear_views[0].image=(void*)0x9000;
    assert(invoke(g_comp_dev)==-11 && selected==ci && !calls);
    ready();comp_linear_views[0].view=(void*)0x8000;comp_linear_views[0].dev=(void*)0x9000;comp_linear_views[0].image=g_comp_img[0];
    assert(invoke(g_comp_dev)==-11 && selected==ci && !calls);
    ready();assert(!invoke(g_comp_dev) && calls==1);
    comp_linear_view_destroyed(0);comp_linear_view_destroyed((void*)0x9000);
    assert(comp_linear_views[0].view==(void*)0x8000);
    comp_linear_view_destroyed((void*)0x8000);assert(!comp_linear_views[0].view && !comp_linear_views[0].dev && !comp_linear_views[0].image);
    // Root invalidates the tracked slot before the actual VkDestroyImageView.
    g_views[1].view=0;assert(!invoke(g_comp_dev) && calls==2);
    ready();assert(!comp_linear_framebuffer(g_comp_dev,0,out_ci,out_atts,&selected) && !selected && !calls);
    assert(!comp_linear_framebuffer(g_comp_dev,ci,0,out_atts,&selected) && selected==ci && !calls);
    assert(!comp_linear_framebuffer(g_comp_dev,ci,out_ci,0,&selected) && selected==ci && !calls);
    assert(!comp_linear_framebuffer(g_comp_dev,ci,out_ci,out_atts,0) && !calls);
    puts("PASS: actual compatible BGRA framebuffer helper, immutable native CI/attachments, effective view CI clone with format-only mutation, per-proxy cache/rings/destruction, refusal guards and propagated creation failure; native RP/PSO unchanged");
    return 0;
}
