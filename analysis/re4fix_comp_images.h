// Native swapchain users receive proxy images; only WSI presentation uses the physical images.
#ifndef RE4FIX_COMP_IMAGES_H
#define RE4FIX_COMP_IMAGES_H
#include <vulkan/vulkan_core.h>
#include <string.h>

static PFN_vkCmdPipelineBarrier comp_image_barrier;
static PFN_vkCmdCopyImage comp_image_copy;
static PFN_vkCmdBlitImage comp_image_blit;
static PFN_vkCmdResolveImage comp_image_resolve;

static int comp_proxy_index(VkImage image) {
    if (!g_comp_on || !image) return -1;
    for (unsigned i=0;i<g_comp_n && i<COMP_MAX;i++)
        if (image==(VkImage)g_comp_img[i]) return (int)i;
    return -1;
}

// A transfer/clear can supply the frame without any render pass targeting its proxy.
static void comp_native_write(VkImage image) {
    int index=comp_proxy_index(image);
    if (index<0) return;
    __atomic_store_n(&g_comp_drawn[index],1,__ATOMIC_RELEASE);
    __atomic_fetch_add(&g_comp_pass_serial[index],1,__ATOMIC_RELAXED);
}

static VKAPI_ATTR void VKAPI_CALL comp_native_barriers(VkCommandBuffer command,
    VkPipelineStageFlags source_stage,VkPipelineStageFlags destination_stage,VkDependencyFlags flags,
    uint32_t memory_count,const VkMemoryBarrier* memory,uint32_t buffer_count,const VkBufferMemoryBarrier* buffers,
    uint32_t image_count,const VkImageMemoryBarrier* images) {
    if (!comp_image_barrier) return;
    int patch=0;
    if (images) for (uint32_t i=0;i<image_count;i++) {
        int index=comp_proxy_index(images[i].image);
        if (index<0) continue;
        if (images[i].newLayout!=VK_IMAGE_LAYOUT_UNDEFINED
            && !__atomic_exchange_n(&g_comp_init[index],1,__ATOMIC_ACQ_REL))
            __atomic_fetch_add(&g_comp_inits,1,__ATOMIC_RELAXED);
        if (images[i].oldLayout==VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            || images[i].newLayout==VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) patch=1;
    }
    if (!patch) {
        comp_image_barrier(command,source_stage,destination_stage,flags,memory_count,memory,
            buffer_count,buffers,image_count,images);
        return;
    }
    // Standalone images have no presentation engine; the compositor restores this layout after sampling.
    VkImageMemoryBarrier mapped[image_count];
    memcpy(mapped,images,sizeof mapped);
    for (uint32_t i=0;i<image_count;i++) if (comp_proxy_index(mapped[i].image)>=0) {
        if (mapped[i].oldLayout==VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) mapped[i].oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        if (mapped[i].newLayout==VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) mapped[i].newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    comp_image_barrier(command,source_stage,destination_stage,flags,memory_count,memory,
        buffer_count,buffers,image_count,mapped);
}

static VKAPI_ATTR void VKAPI_CALL comp_native_copy(VkCommandBuffer command,VkImage source,VkImageLayout source_layout,
    VkImage destination,VkImageLayout destination_layout,uint32_t count,const VkImageCopy* regions) {
    if (!comp_image_copy) return;
    comp_image_copy(command,source,source_layout,destination,destination_layout,count,regions);
    if (count && regions) comp_native_write(destination);
}
static VKAPI_ATTR void VKAPI_CALL comp_native_blit(VkCommandBuffer command,VkImage source,VkImageLayout source_layout,
    VkImage destination,VkImageLayout destination_layout,uint32_t count,const VkImageBlit* regions,VkFilter filter) {
    if (!comp_image_blit) return;
    comp_image_blit(command,source,source_layout,destination,destination_layout,count,regions,filter);
    if (count && regions) comp_native_write(destination);
}
static VKAPI_ATTR void VKAPI_CALL comp_native_resolve(VkCommandBuffer command,VkImage source,VkImageLayout source_layout,
    VkImage destination,VkImageLayout destination_layout,uint32_t count,const VkImageResolve* regions) {
    if (!comp_image_resolve) return;
    comp_image_resolve(command,source,source_layout,destination,destination_layout,count,regions);
    if (count && regions) comp_native_write(destination);
}

// Apply only to native procedure lookups; comp_fn must keep returning the original driver procedures.
static void* comp_image_proc(const char* name,void* real) {
    if (!name || !real) return real;
    if (!strcmp(name,"vkCmdPipelineBarrier")) {
        if (real!=(void*)comp_native_barriers) comp_image_barrier=(PFN_vkCmdPipelineBarrier)real;
        return (void*)comp_native_barriers;
    }
    if (!strcmp(name,"vkCmdCopyImage")) {
        if (real!=(void*)comp_native_copy) comp_image_copy=(PFN_vkCmdCopyImage)real;
        return (void*)comp_native_copy;
    }
    if (!strcmp(name,"vkCmdBlitImage")) {
        if (real!=(void*)comp_native_blit) comp_image_blit=(PFN_vkCmdBlitImage)real;
        return (void*)comp_native_blit;
    }
    if (!strcmp(name,"vkCmdResolveImage")) {
        if (real!=(void*)comp_native_resolve) comp_image_resolve=(PFN_vkCmdResolveImage)real;
        return (void*)comp_native_resolve;
    }
    return real;
}
#endif
