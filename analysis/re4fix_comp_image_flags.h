// Match native MSAA sources to the compositor's mutable BGRA resolve contract, preserving their sRGB storage.
#ifndef RE4FIX_COMP_IMAGE_FLAGS_H
#define RE4FIX_COMP_IMAGE_FLAGS_H
#include <vulkan/vulkan_core.h>
#include <stdint.h>
#include <string.h>

static PFN_vkCreateImage comp_image_flags_create_image;
static unsigned g_comp_image_flags_patches,g_comp_image_flags_msaa_patches,g_comp_image_flags_msaa2_patches;

static const VkImageCreateInfo* comp_image_flags_info(VkDevice device,const VkImageCreateInfo* ci,VkImageCreateInfo* copy,
    VkImageFormatListCreateInfo* added,const VkFormat formats[2],uintptr_t caller) {
    if(!ci||!copy||ci->sType!=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO
        ||!g_ue4_base_cached||caller!=g_ue4_base_cached+0x6545984ul)return ci;
    const VkImageFormatListCreateInfo* list=ci->pNext;
    int head=list&&list->sType==VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO;
    int viewport=g_comp_on&&!g_comp_disable&&g_comp_mutable&&device==(VkDevice)g_comp_dev
        &&ci->format==VK_FORMAT_B8G8R8A8_SRGB&&(ci->samples==VK_SAMPLE_COUNT_2_BIT||ci->samples==VK_SAMPLE_COUNT_4_BIT)
        &&ci->imageType==VK_IMAGE_TYPE_2D&&g_comp_w&&g_comp_h
        &&ci->extent.width==g_comp_w&&ci->extent.height==g_comp_h&&ci->extent.depth==1
        &&ci->mipLevels==1&&ci->arrayLayers==1&&ci->tiling==VK_IMAGE_TILING_OPTIMAL
        &&(ci->usage&VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)&&ci->initialLayout==VK_IMAGE_LAYOUT_UNDEFINED
        &&ci->sharingMode==VK_SHARING_MODE_EXCLUSIVE&&!ci->queueFamilyIndexCount&&!ci->pQueueFamilyIndices;
    if(viewport) {
        if((!added||!formats)&&(list||__atomic_load_n(&g_comp_format_list,__ATOMIC_ACQUIRE)))return ci;
        int known=head&&(!list->viewFormatCount||(list->viewFormatCount==2&&list->pViewFormats
            &&((list->pViewFormats[0]==formats[0]&&list->pViewFormats[1]==formats[1])
                ||(list->pViewFormats[0]==formats[1]&&list->pViewFormats[1]==formats[0]))));
        if(!list||known) {
            if((ci->flags&VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT)&&(!list||!list->viewFormatCount))return ci;
            *copy=*ci;copy->flags|=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
            if(list&&list->viewFormatCount) {
                // Keep the borrowed chain immutable; zero count requests the core unrestricted mutable contract.
                *added=*list;added->viewFormatCount=0;added->pViewFormats=0;copy->pNext=added;
            }
            return copy;
        }
    }
    // Preserve the existing validity repair for all other native two-format lists.
    if((ci->flags&VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT)||!head||list->viewFormatCount!=2||!list->pViewFormats)return ci;
    *copy=*ci;copy->flags|=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    return copy;
}

static VKAPI_ATTR __attribute__((noinline)) VkResult VKAPI_CALL h_comp_image_flags_CreateImage(VkDevice device,const VkImageCreateInfo* ci,
    const VkAllocationCallbacks* allocator,VkImage* output) {
    if(!comp_image_flags_create_image)return VK_ERROR_INITIALIZATION_FAILED;
    VkImageCreateInfo copy;
    VkImageFormatListCreateInfo added;
    const VkFormat formats[2]={VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_B8G8R8A8_SRGB};
    uintptr_t caller=(uintptr_t)__builtin_return_address(0);
    const VkImageCreateInfo* selected=comp_image_flags_info(device,ci,&copy,&added,formats,caller);
    VkResult result=comp_image_flags_create_image(device,selected,allocator,output);
    if(selected!=ci) {
        int log=__atomic_fetch_add(&g_comp_image_flags_patches,1,__ATOMIC_RELAXED)<4;
        if(ci->samples==VK_SAMPLE_COUNT_4_BIT)
            log|=__atomic_fetch_add(&g_comp_image_flags_msaa_patches,1,__ATOMIC_RELAXED)<4;
        if(ci->samples==VK_SAMPLE_COUNT_2_BIT)
            log|=__atomic_fetch_add(&g_comp_image_flags_msaa2_patches,1,__ATOMIC_RELAXED)<4;
        if(log) {
            const VkImageFormatListCreateInfo* list=selected->pNext;
            LOGF("COMP image flags native mutable repair format=%u->%u samples=%u %ux%ux%u originalFlags=%x selectedFlags=%x list=%u [%u,%u] originalList=%u format_list_enabled=%d pNext=%p result=%d image=%p",
                ci->format,copy.format,ci->samples,ci->extent.width,ci->extent.height,ci->extent.depth,ci->flags,copy.flags,
                list?list->viewFormatCount:0,list&&list->viewFormatCount&&list->pViewFormats?list->pViewFormats[0]:0,
                list&&list->viewFormatCount>1&&list->pViewFormats?list->pViewFormats[1]:0,
                ci->pNext?((const VkImageFormatListCreateInfo*)ci->pNext)->viewFormatCount:0,
                __atomic_load_n(&g_comp_format_list,__ATOMIC_ACQUIRE),ci->pNext,result,result==VK_SUCCESS&&output?(void*)*output:0);
        }
    }
    return result;
}

// Native GIPA/GDPA/dlsym only; compositor comp_fn continues to use the original driver function.
static void* comp_image_flags_proc(const char* name,void* real) {
    if(!name||!real||strcmp(name,"vkCreateImage"))return real;
    if(real!=(void*)h_comp_image_flags_CreateImage)comp_image_flags_create_image=(PFN_vkCreateImage)real;
    return (void*)h_comp_image_flags_CreateImage;
}
#endif
