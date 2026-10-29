// Expose the compositor's BGRA backbuffer only on the game's own Android surface.
#ifndef RE4FIX_COMP_SURFACE_H
#define RE4FIX_COMP_SURFACE_H
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_android.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void* g_comp_surface;
static unsigned g_comp_surface_logs;
static unsigned g_comp_prepare_logs;
static const void* comp_device_format_result(const void* selected,const char* outcome,int result,unsigned count) {
    if (g_comp_prepare_logs<4) {
        ++g_comp_prepare_logs;LOGF("COMP prepare image_format_list %s result=%d count=%u",outcome,result,count);
    }
    return selected;
}

// Append only a driver-supported optional extension to the borrowed native CI.
// No additional extension or instance/API-version assumption is required.
static const void* comp_prepare_device_formats(void* physical,const void* info,
    unsigned char copied_ci[72],const char* names[257]) {
    const VkDeviceCreateInfo* ci=info;
    if (!physical || !ci || !copied_ci || !names || ((uintptr_t)copied_ci&7)
        || ci->sType!=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO || ci->enabledExtensionCount>256
        || (ci->enabledExtensionCount && !ci->ppEnabledExtensionNames))
        return comp_device_format_result(info,"invalid CI",0,0);
    int already=0;
    for (unsigned i=0;i<ci->enabledExtensionCount;i++) {
        const char* name=ci->ppEnabledExtensionNames[i];
        if (!name) return comp_device_format_result(info,"invalid enabled names",0,ci->enabledExtensionCount);
        unsigned length=0;
        while (length<VK_MAX_EXTENSION_NAME_SIZE && name[length]) ++length;
        if (!length || length==VK_MAX_EXTENSION_NAME_SIZE)
            return comp_device_format_result(info,"invalid enabled names",0,ci->enabledExtensionCount);
        already|=!strcmp(name,"VK_KHR_image_format_list");
    }
    if (already) return comp_device_format_result(info,"already enabled",0,ci->enabledExtensionCount);
    if (!r_gipa || !g_vk_instance) return comp_device_format_result(info,"enumerator missing",0,0);
    PFN_vkEnumerateDeviceExtensionProperties enumerate=
        (PFN_vkEnumerateDeviceExtensionProperties)r_gipa(g_vk_instance,"vkEnumerateDeviceExtensionProperties");
    if (!enumerate) return comp_device_format_result(info,"enumerator missing",0,0);
    uint32_t count=0;
    VkResult result=enumerate((VkPhysicalDevice)physical,0,&count,0);
    if (result!=VK_SUCCESS || !count || count>256)
        return comp_device_format_result(info,"enumeration refused",result,count);
    uint32_t capacity=count;
    VkExtensionProperties* properties=calloc(capacity,sizeof *properties);
    if (!properties) return comp_device_format_result(info,"allocation failed",VK_ERROR_OUT_OF_HOST_MEMORY,count);
    result=enumerate((VkPhysicalDevice)physical,0,&count,properties);
    int supported=0;
    if (result==VK_SUCCESS && count && count<=capacity) {
        for (uint32_t i=0;i<count;i++) {
            if (!properties[i].extensionName[0]
                || !memchr(properties[i].extensionName,0,VK_MAX_EXTENSION_NAME_SIZE)) {
                supported=0;break;
            }
            supported|=!strcmp(properties[i].extensionName,"VK_KHR_image_format_list");
        }
    }
    free(properties);
    if (!supported) return comp_device_format_result(info,
        result==VK_SUCCESS && count && count<=capacity ? "unsupported or invalid list" : "enumeration refused",result,count);
    VkDeviceCreateInfo selected=*ci;
    for (unsigned i=0;i<ci->enabledExtensionCount;i++) names[i]=ci->ppEnabledExtensionNames[i];
    names[ci->enabledExtensionCount]="VK_KHR_image_format_list";
    selected.enabledExtensionCount++;
    selected.ppEnabledExtensionNames=names;
    memcpy(copied_ci,&selected,sizeof selected);
    return comp_device_format_result(copied_ci,"appended",0,selected.enabledExtensionCount);
}

static void comp_capture_device_formats(const void* info,int result) {
    int enabled=0;
    const VkDeviceCreateInfo* ci=info;
    if (!result && ci && ci->sType==VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO
        && ci->enabledExtensionCount<=257 && ci->ppEnabledExtensionNames) {
        for (unsigned i=0;i<ci->enabledExtensionCount;i++)
            if (ci->ppEnabledExtensionNames[i]
                && !strcmp(ci->ppEnabledExtensionNames[i],"VK_KHR_image_format_list")) enabled=1;
    }
    __atomic_store_n(&g_comp_format_list,enabled,__ATOMIC_RELEASE);
    LOGF("COMP native image_format_list enabled=%d result=%d",enabled,result);
}

int h_vkCreateAndroidSurface(void* instance,const void* info,const void* allocator,void** out) {
    int result=r_vkCreateAndroidSurface(instance,info,allocator,out);
    const VkAndroidSurfaceCreateInfoKHR* ci=info;
    if (!result && out && *out && ci && ci->sType==VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR
        && !ci->pNext && ci->window)
        __atomic_store_n(&g_comp_surface,*out,__ATOMIC_RELEASE);
    return result;
}

int h_vkSurfaceFormats(void* physical,void* surface,unsigned* count,void* formats) {
    if (!count || physical!=g_phys_dev || !physical || !surface
        || !__atomic_load_n(&g_comp_format_list,__ATOMIC_ACQUIRE)
        || surface!=__atomic_load_n(&g_comp_surface,__ATOMIC_ACQUIRE) || g_comp_disable)
        return r_vkSurfaceFormats(physical,surface,count,formats);
    VkSurfaceFormatKHR real[64];unsigned size=64;
    int result=r_vkSurfaceFormats(physical,surface,&size,real);
    if (result || !size || size>=64)
        return r_vkSurfaceFormats(physical,surface,count,formats);
    int srgb=0;
    for (unsigned i=0;i<size;i++) {
        if (real[i].format==VK_FORMAT_B8G8R8A8_UNORM)
            return r_vkSurfaceFormats(physical,surface,count,formats);
        srgb|=real[i].format==VK_FORMAT_R8G8B8A8_SRGB
            && real[i].colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }
    void (*properties)(void*,unsigned,void*)=r_gipa && g_vk_instance
        ? r_gipa(g_vk_instance,"vkGetPhysicalDeviceFormatProperties") : 0;
    VkFormatProperties linear={0},scene={0},display={0};
    if (srgb && properties) {
        properties(physical,VK_FORMAT_B8G8R8A8_UNORM,&linear);
        properties(physical,VK_FORMAT_B8G8R8A8_SRGB,&scene);
        properties(physical,VK_FORMAT_R8G8B8A8_SRGB,&display);
    }
    unsigned needed=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    unsigned view_needed=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    if ((linear.optimalTilingFeatures&view_needed)!=view_needed
        || (scene.optimalTilingFeatures&needed)!=needed
        || !(display.optimalTilingFeatures&VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
        return r_vkSurfaceFormats(physical,surface,count,formats);
    if (!formats) { *count=size+1;return VK_SUCCESS; }
    unsigned capacity=*count,written=capacity<size+1 ? capacity : size+1;
    VkSurfaceFormatKHR* output=formats;
    for (unsigned i=0;i<written;i++)
        output[i]=i<size ? real[i] : (VkSurfaceFormatKHR){VK_FORMAT_B8G8R8A8_UNORM,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    *count=written;
    if (g_comp_surface_logs<2) {
        ++g_comp_surface_logs;LOGF("COMP surface logical BGRA44 over native RGBA43 surface=%p count=%u",surface,size+1);
    }
    return written<size+1 ? VK_INCOMPLETE : VK_SUCCESS;
}
#endif
