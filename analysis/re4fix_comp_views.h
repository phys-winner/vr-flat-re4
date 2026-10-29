// Owned proxy views for the native linear BGRA window pass. The proxy image
// must be created with MUTABLE_FORMAT; native render passes and PSOs stay intact.
#ifndef RE4FIX_COMP_VIEWS_H
#define RE4FIX_COMP_VIEWS_H
#include <string.h>

static struct { void* dev; void* image; void* view; unsigned char info[80]; } comp_linear_views[COMP_MAX];

static void comp_linear_view_destroyed(void* view) {
    if (!view) return;
    for (unsigned i=0;i<COMP_MAX;i++) if (comp_linear_views[i].view==view)
        memset(&comp_linear_views[i],0,sizeof comp_linear_views[i]);
}

// Same serialized RHI resource-creation thread and process-only proxy lifetime
// as comp_create/g_views. Never replace an offscreen or untracked attachment.
static int comp_linear_framebuffer(void* dev, const void* ci,
    unsigned char out_ci[64], void* out_atts[8], const void** selected) {
    if (!selected) return 0;
    *selected=ci;
    if (!ci || !out_ci || !out_atts || !g_comp_on || g_comp_disable || !g_comp_mutable || !dev || dev!=g_comp_dev
        || !g_comp_n || g_comp_n>COMP_MAX
        || *(const unsigned*)ci!=37 || *(void* const*)((const char*)ci+8)
        || *(const unsigned*)((const char*)ci+16)
        || *(const unsigned*)((const char*)ci+32)!=1) return 0;
    unsigned width=*(const unsigned*)((const char*)ci+48);
    unsigned height=*(const unsigned*)((const char*)ci+52);
    if (!width || !height || width>g_comp_w || height>g_comp_h
        || *(const unsigned*)((const char*)ci+56)!=1) return 0;
    struct rptrack* rp=rp_find(*(void* const*)((const char*)ci+24));
    if (!rp || rp->n!=1 || rp->fmt0!=44) return 0;
    void* const* atts=*(void* const* const*)((const char*)ci+40);
    if (!atts || !atts[0]) return 0;
    int sw=swidx_of_view(atts[0]);
    if (sw<0 || (unsigned)sw>=g_comp_n || sw>=COMP_MAX || !g_comp_img[sw]) return 0;
    struct vtrack* source=0;
    for (int k=g_nviews-1;k>=0 && k>=g_nviews-64;k--)
        if (g_views[k%64].view==atts[0]) { source=&g_views[k%64]; break; }
    if (!source || source->sw!=sw || source->fmt!=50) return 0;
    const unsigned char* original=source->info;
    if (*(const unsigned*)original!=15 || *(void* const*)(original+8)
        || *(void* const*)(original+24)!=g_comp_img[sw]
        || *(const unsigned*)(original+32)!=1 || *(const unsigned*)(original+36)!=50
        || *(const unsigned*)(original+56)!=1 || *(const unsigned*)(original+60)
        || *(const unsigned*)(original+64)!=1 || *(const unsigned*)(original+68)
        || *(const unsigned*)(original+72)!=1) return 0;
    for (unsigned i=0;i<4;i++) {
        unsigned component=*(const unsigned*)(original+40+4*i);
        if (component && component!=3+i) return 0; // identity or explicit R/G/B/A
    }
    if (comp_linear_views[sw].view && (comp_linear_views[sw].dev!=dev
        || comp_linear_views[sw].image!=g_comp_img[sw]
        || *(const unsigned*)(comp_linear_views[sw].info+16)!=*(const unsigned*)(original+16))) return -11;
    if (!comp_linear_views[sw].view) {
        if (!r_vkCreateImageView) return -3;
        _Alignas(8) unsigned char view_ci[80]; memcpy(view_ci,original,sizeof view_ci);
        *(unsigned*)(view_ci+36)=44; // same BGRA channels; all other CI bytes stay native
        void* view=0;
        int result=r_vkCreateImageView(dev,view_ci,0,&view);
        if (result) return result;
        if (!view) return -3;
        comp_linear_views[sw].dev=dev; comp_linear_views[sw].image=g_comp_img[sw];
        comp_linear_views[sw].view=view;
        memcpy(comp_linear_views[sw].info,view_ci,sizeof view_ci);
    }
    int tracked=0;
    for (int k=g_nviews-1;k>=0 && k>=g_nviews-64;k--)
        if (g_views[k%64].view==comp_linear_views[sw].view) {
            tracked=g_views[k%64].sw==sw && g_views[k%64].fmt==44
                && !memcmp(g_views[k%64].info,comp_linear_views[sw].info,80);
            break;
        }
    if (!tracked) {
        int k=g_nviews%64;
        g_views[k].view=comp_linear_views[sw].view; g_views[k].sw=sw; g_views[k].fmt=44;
        memcpy(g_views[k].info,comp_linear_views[sw].info,80);
        __sync_synchronize(); g_nviews++;
    }
    memcpy(out_ci,ci,64);
    out_atts[0]=comp_linear_views[sw].view;
    void* const* replacement=out_atts;
    memcpy(out_ci+40,&replacement,sizeof replacement);
    *selected=out_ci;
    return 0;
}
#endif
