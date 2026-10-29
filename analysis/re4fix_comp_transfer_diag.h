// Temporary read-only transfer trace. Native calls and their borrowed arguments stay exact.
#ifndef RE4FIX_COMP_TRANSFER_DIAG_H
#define RE4FIX_COMP_TRANSFER_DIAG_H
#include <vulkan/vulkan_core.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
static PFN_vkCreateImage comp_transfer_create_image;
static PFN_vkCmdCopyImage comp_transfer_copy_image;
static PFN_vkCmdBlitImage comp_transfer_blit_image;
static PFN_vkCmdResolveImage comp_transfer_resolve_image;
static pthread_mutex_t comp_transfer_mutex=PTHREAD_MUTEX_INITIALIZER;
static struct comp_transfer_image {VkImage image;VkFormat format;VkExtent3D extent;VkSampleCountFlagBits samples;unsigned kind,attachment;} comp_transfer_images[256];
static unsigned comp_transfer_next,comp_transfer_owned_logs,comp_transfer_large_logs;

static struct comp_transfer_image comp_transfer_find(VkImage image) {
    struct comp_transfer_image found={0};found.image=image;
    if(!image)return found;
    for(unsigned i=0;i<COMP_MAX;i++) {
        if(image==(VkImage)g_own_imgs[i]) {
            found.format=(VkFormat)g_sc_fmt_real;found.extent=(VkExtent3D){g_comp_w,g_comp_h,1};found.samples=VK_SAMPLE_COUNT_1_BIT;found.kind=3;return found;
        }
        if(image==(VkImage)g_comp_img[i]) {
            found.format=(VkFormat)(g_comp_mutable?44:g_comp_fmt);found.extent=(VkExtent3D){g_comp_w,g_comp_h,1};found.samples=VK_SAMPLE_COUNT_1_BIT;found.kind=2;return found;
        }
    }
    for(unsigned i=0;i<256;i++)if(comp_transfer_images[i].image==image)found=comp_transfer_images[i];
    return found;
}
static int comp_transfer_color8(VkFormat format) {
    return format>=VK_FORMAT_R8G8B8A8_UNORM&&format<=VK_FORMAT_A8B8G8R8_SRGB_PACK32;
}
static void comp_transfer_trace(const char* operation,void* caller,VkImage source,VkImageLayout source_layout,
    VkImage destination,VkImageLayout destination_layout,unsigned count,VkExtent3D extent) {
    struct comp_transfer_image src,dst;unsigned sequence=0,owned=0;
    pthread_mutex_lock(&comp_transfer_mutex);
    if(comp_transfer_owned_logs>=64&&comp_transfer_large_logs>=20){pthread_mutex_unlock(&comp_transfer_mutex);return;}
    src=comp_transfer_find(source);dst=comp_transfer_find(destination);
    int color=comp_transfer_color8(src.format)||comp_transfer_color8(dst.format);
    int fullscreen=g_comp_w&&g_comp_h&&extent.width==g_comp_w&&extent.height==g_comp_h;
    owned=src.kind>=2||dst.kind>=2;
    if(owned&&(color||fullscreen)&&comp_transfer_owned_logs<64)sequence=++comp_transfer_owned_logs;
    else if(!owned&&color&&extent.width>=512&&extent.height>=256&&comp_transfer_large_logs<20)sequence=++comp_transfer_large_logs;
    pthread_mutex_unlock(&comp_transfer_mutex);
    if(!sequence)return;
    const char* kinds[]={"unknown","image","proxy","WSI"};
    LOGF("COMP TRANSFER %s owned=%u n=%u caller=%p src=%p %s fmt=%u size=%ux%ux%u samples=%u layout=%u dst=%p %s fmt=%u size=%ux%ux%u samples=%u layout=%u regions=%u region0=%ux%ux%u",
        operation,owned,sequence,caller,(void*)(uintptr_t)source,kinds[src.kind],src.format,src.extent.width,src.extent.height,src.extent.depth,src.samples,source_layout,
        (void*)(uintptr_t)destination,kinds[dst.kind],dst.format,dst.extent.width,dst.extent.height,dst.extent.depth,dst.samples,destination_layout,count,extent.width,extent.height,extent.depth);
}
static VKAPI_ATTR VkResult VKAPI_CALL h_comp_transfer_CreateImage(VkDevice device,const VkImageCreateInfo* info,const VkAllocationCallbacks* allocator,VkImage* out) {
    if(!comp_transfer_create_image)return VK_ERROR_INITIALIZATION_FAILED;
    VkResult result=comp_transfer_create_image(device,info,allocator,out);
    if(result==VK_SUCCESS&&info&&info->sType==VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO&&out&&*out
        &&(info->format==VK_FORMAT_R8G8B8A8_UNORM||info->format==VK_FORMAT_R8G8B8A8_SRGB
            ||info->format==VK_FORMAT_B8G8R8A8_UNORM||info->format==VK_FORMAT_B8G8R8A8_SRGB)
        &&((info->extent.width>=512&&info->extent.height>=256)||(info->usage&VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))) {
        struct comp_transfer_image entry={*out,info->format,info->extent,info->samples,1,!!(info->usage&VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)};
        pthread_mutex_lock(&comp_transfer_mutex);
        unsigned slot=256;
        for(unsigned i=0;i<256;i++)if(comp_transfer_images[i].image==*out){slot=i;break;}
        // Texture loading cannot evict a live viewport render target from this bounded trace.
        if(slot==256)for(unsigned i=0;i<256;i++) {
            unsigned candidate=(comp_transfer_next+i)%256;
            if(!comp_transfer_images[candidate].attachment){slot=candidate;break;}
        }
        if(slot==256&&entry.attachment)slot=comp_transfer_next%256;
        if(slot<256){comp_transfer_images[slot]=entry;comp_transfer_next++;}
        pthread_mutex_unlock(&comp_transfer_mutex);
    }
    return result;
}
static VKAPI_ATTR __attribute__((noinline)) void VKAPI_CALL h_comp_transfer_CmdCopyImage(VkCommandBuffer command,VkImage source,VkImageLayout source_layout,
    VkImage destination,VkImageLayout destination_layout,uint32_t count,const VkImageCopy* regions) {
    VkExtent3D extent={0};if(count&&regions)extent=regions[0].extent;
    comp_transfer_trace("copy",__builtin_return_address(0),source,source_layout,destination,destination_layout,count,extent);
    if(comp_transfer_copy_image)comp_transfer_copy_image(command,source,source_layout,destination,destination_layout,count,regions);
}
static uint32_t comp_transfer_span(int32_t first,int32_t last) {
    int64_t delta=(int64_t)last-first;return (uint32_t)(delta<0?-delta:delta);
}
static VKAPI_ATTR __attribute__((noinline)) void VKAPI_CALL h_comp_transfer_CmdBlitImage(VkCommandBuffer command,VkImage source,VkImageLayout source_layout,
    VkImage destination,VkImageLayout destination_layout,uint32_t count,const VkImageBlit* regions,VkFilter filter) {
    VkExtent3D extent={0};
    if(count&&regions)extent=(VkExtent3D){comp_transfer_span(regions[0].dstOffsets[0].x,regions[0].dstOffsets[1].x),
        comp_transfer_span(regions[0].dstOffsets[0].y,regions[0].dstOffsets[1].y),comp_transfer_span(regions[0].dstOffsets[0].z,regions[0].dstOffsets[1].z)};
    comp_transfer_trace("blit",__builtin_return_address(0),source,source_layout,destination,destination_layout,count,extent);
    if(comp_transfer_blit_image)comp_transfer_blit_image(command,source,source_layout,destination,destination_layout,count,regions,filter);
}
static VKAPI_ATTR __attribute__((noinline)) void VKAPI_CALL h_comp_transfer_CmdResolveImage(VkCommandBuffer command,VkImage source,VkImageLayout source_layout,
    VkImage destination,VkImageLayout destination_layout,uint32_t count,const VkImageResolve* regions) {
    VkExtent3D extent={0};if(count&&regions)extent=regions[0].extent;
    comp_transfer_trace("resolve",__builtin_return_address(0),source,source_layout,destination,destination_layout,count,extent);
    if(comp_transfer_resolve_image)comp_transfer_resolve_image(command,source,source_layout,destination,destination_layout,count,regions);
}
// Call with the original result in every GIPA/GDPA/dlsym route; unknown/null results pass through.
static void* comp_transfer_proc(const char* name,void* real) {
    if(!name||!real)return real;
    if(!strcmp(name,"vkCreateImage")) {
        if(real!=(void*)h_comp_transfer_CreateImage)comp_transfer_create_image=(PFN_vkCreateImage)real;
        return (void*)h_comp_transfer_CreateImage;
    }
    if(!strcmp(name,"vkCmdCopyImage")) {
        if(real!=(void*)h_comp_transfer_CmdCopyImage)comp_transfer_copy_image=(PFN_vkCmdCopyImage)real;
        return (void*)h_comp_transfer_CmdCopyImage;
    }
    if(!strcmp(name,"vkCmdBlitImage")) {
        if(real!=(void*)h_comp_transfer_CmdBlitImage)comp_transfer_blit_image=(PFN_vkCmdBlitImage)real;
        return (void*)h_comp_transfer_CmdBlitImage;
    }
    if(!strcmp(name,"vkCmdResolveImage")) {
        if(real!=(void*)h_comp_transfer_CmdResolveImage)comp_transfer_resolve_image=(PFN_vkCmdResolveImage)real;
        return (void*)h_comp_transfer_CmdResolveImage;
    }
    return real;
}
#endif
