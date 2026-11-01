#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <vulkan/vulkan_core.h>
_Static_assert(sizeof(VkSwapchainCreateInfoKHR)==104,"native swapchain CI ABI");
_Static_assert(offsetof(VkSwapchainCreateInfoKHR,surface)==24,"native swapchain surface ABI");
_Static_assert(offsetof(VkSwapchainCreateInfoKHR,presentMode)==88,"native swapchain present mode ABI");
_Static_assert(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR==1000001000 && VK_PRESENT_MODE_FIFO_KHR==2,"native FIFO enum ABI");
#define PTR(n) ((void*)(uintptr_t)(n))
#define LOGF(...) ((void)0)
static long g_csc_calls;
static void* g_vk_instance,*g_phys_dev,*g_comp_surface,*g_window_sc;
static unsigned g_sc_fmt_req,g_sc_fmt_real,g_comp_w,g_comp_h,g_comp_usage_extra;
static int g_csc_lastfmt,g_csc_lastr;
static char g_surf_fmts[256];
static void* (*r_gipa)(void*,const char*);
static int (*r_vkSurfaceCaps)(void*,void*,void*);
static int (*r_vkSurfaceFormats)(void*,void*,unsigned*,void*);
static int (*r_vkCreateSwapchain)(void*,const void*,const void*,void**);
static int events[8],event_count,fifo,original_calls,result;
static void* created;
static const VkSwapchainCreateInfoKHR* borrowed;
static VkSwapchainCreateInfoKHR effective;
static void touch_mobile_settings_fifo(int enabled){assert(event_count<8);events[event_count++]=enabled;fifo=enabled;}
#include "swapchain_root.h"
static int caps(void* physical,void* surface,void* output){
    assert(physical==g_phys_dev && surface && !fifo && original_calls==0);
    VkSurfaceCapabilitiesKHR value={0};value.currentExtent=(VkExtent2D){2340,1080};value.supportedUsageFlags=0xff;
    memcpy(output,&value,sizeof value);return VK_SUCCESS;
}
static int formats(void* physical,void* surface,unsigned* count,void* output){
    assert(physical==g_phys_dev && surface && count && !fifo && original_calls==0);
    if(output){assert(*count>=2);VkSurfaceFormatKHR value[2]={{VK_FORMAT_R8G8B8A8_UNORM,0},{VK_FORMAT_R8G8B8A8_SRGB,0}};memcpy(output,value,sizeof value);}
    *count=2;return VK_SUCCESS;
}
static int original(void* device,const void* info,const void* allocator,void** output){
    assert(device==PTR(0x100) && allocator==PTR(0x200) && original_calls++==0 && !fifo);
    assert(event_count==1 && events[0]==0);events[event_count++]=9;
    if(borrowed){assert(info && info!=borrowed);memcpy(&effective,info,sizeof effective);}
    else assert(!info);
    if(output)*output=created;
    return result;
}
static VkSwapchainCreateInfoKHR ready(void){
    g_csc_calls=original_calls=event_count=0;fifo=1;memset(events,0xff,sizeof events);
    g_vk_instance=PTR(0x300);g_phys_dev=PTR(0x400);g_comp_surface=PTR(0x500);g_window_sc=PTR(0x600);
    result=VK_SUCCESS;created=PTR(0x700);r_vkCreateSwapchain=original;r_vkSurfaceCaps=caps;r_vkSurfaceFormats=formats;r_gipa=0;
    VkSwapchainCreateInfoKHR info={0};info.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;info.surface=(VkSurfaceKHR)g_comp_surface;
    info.imageFormat=VK_FORMAT_A2B10G10R10_UNORM_PACK32;info.imageColorSpace=(VkColorSpaceKHR)1000104008;
    info.imageExtent=(VkExtent2D){2340,1080};info.minImageCount=3;info.imageArrayLayers=1;
    info.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;info.presentMode=VK_PRESENT_MODE_FIFO_KHR;
    info.pNext=PTR(0x800);info.oldSwapchain=(VkSwapchainKHR)PTR(0x900);return info;
}
int main(void){
    for(int mode=0;mode<10;mode++){
        VkSwapchainCreateInfoKHR info=ready();void* output=PTR(0xa00);void** out=&output;int expected=mode==0;
        if(mode==1)info.presentMode=VK_PRESENT_MODE_MAILBOX_KHR;
        if(mode==2)info.surface=(VkSurfaceKHR)PTR(0x501);
        if(mode==3)g_comp_surface=0;
        if(mode==4)result=VK_ERROR_INITIALIZATION_FAILED;
        if(mode==5)out=0;
        if(mode==6)created=0;
        if(mode==7)info.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
        if(mode==8)info.surface=0;
        VkSwapchainCreateInfoKHR before=info;borrowed=mode==9?0:&info;
        int actual=h_vkCreateSwapchain(PTR(0x100),borrowed,PTR(0x200),out);
        assert(actual==result && original_calls==1 && g_csc_calls==1 && fifo==expected);
        assert(!memcmp(&info,&before,sizeof info));
        assert(event_count==(mode==9?2:3) && events[0]==0 && events[1]==9);
        if(mode!=9){
            assert(events[2]==expected);
            VkSwapchainCreateInfoKHR selected=info;selected.imageUsage|=0x13;
            if(info.surface){selected.imageFormat=VK_FORMAT_R8G8B8A8_SRGB;selected.imageColorSpace=0;}
            assert(!memcmp(&effective,&selected,sizeof effective));
        }
    }
    VkSwapchainCreateInfoKHR info=ready(),before=info;info.imageExtent=(VkExtent2D){0,0};before=info;borrowed=&info;
    void* output=0;assert(h_vkCreateSwapchain(PTR(0x100),borrowed,PTR(0x200),&output)==VK_SUCCESS);
    assert(fifo && effective.imageExtent.width==2340 && effective.imageExtent.height==1080 && !memcmp(&info,&before,sizeof info));
    puts("PASS: actual root swapchain wrapper clears FIFO before original/null input, confirms only successful owned typed FIFO with nonnull handle, ten success/refusal cases, original exactly once, borrowed CI unchanged, existing format/usage/extent adaptation retained");
    return 0;
}
