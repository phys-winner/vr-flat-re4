#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
struct ANativeWindow;
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_android.h>
_Static_assert(sizeof(VkSurfaceFormatKHR)==8,"native surface pair ABI");
_Static_assert(sizeof(VkFormatProperties)==12,"native format properties ABI");
_Static_assert(sizeof(VkAndroidSurfaceCreateInfoKHR)==32,"native Android surface CI ABI");
_Static_assert(sizeof(VkDeviceCreateInfo)==72,"native device CI ABI");
_Static_assert(offsetof(VkDeviceCreateInfo,enabledExtensionCount)==48,"native device count ABI");
_Static_assert(offsetof(VkDeviceCreateInfo,ppEnabledExtensionNames)==56,"native device names ABI");
_Static_assert(sizeof(VkImageFormatListCreateInfo)==32,"native format list ABI");
_Static_assert(offsetof(VkImageFormatListCreateInfo,viewFormatCount)==16,"native list count ABI");
_Static_assert(offsetof(VkImageFormatListCreateInfo,pViewFormats)==24,"native list formats ABI");
_Static_assert(sizeof(VkExtensionProperties)==260,"native device extension properties ABI");
static void* g_phys_dev,*g_vk_instance;
static int g_comp_disable;
static volatile int g_comp_format_list;
static int (*r_vkCreateAndroidSurface)(void*,const void*,const void*,void**);
static int (*r_vkSurfaceFormats)(void*,void*,unsigned*,void*);
static void* (*r_gipa)(void*,const char*);
static unsigned log_calls;
static void log_message(const char* format,...) {(void)format;log_calls++;}
#define LOGF(...) log_message(__VA_ARGS__)
static void* fixture_calloc(size_t count,size_t size);
static void fixture_free(void* pointer);
#define calloc fixture_calloc
#define free fixture_free
#include "../re4fix_comp_surface.h"
#undef calloc
#undef free
#define PTR(n) ((void*)(uintptr_t)(n))
static VkSurfaceFormatKHR driver_formats[65];
static unsigned driver_size,driver_calls,property_calls,lookup_calls,create_calls;
static unsigned linear_features,scene_features,present_features;
static int driver_result,properties_missing,create_result;
static void* captured_before,*created_surface;
static const void* expected_info,*expected_allocator;
static void* expected_instance;
static VkExtensionProperties device_extensions[256];
static unsigned enumerate_calls,enum_count,enum_written,allocation_calls,free_calls;
static int enum_first_result,enum_fill_result,enum_missing,allocation_failure;
static void* fixture_calloc(size_t count,size_t size) {
    allocation_calls++;assert(count && count<=256 && size==sizeof(VkExtensionProperties));
    return allocation_failure ? 0 : calloc(count,size);
}
static void fixture_free(void* pointer) {assert(pointer);free_calls++;free(pointer);}
static VkResult VKAPI_CALL original_enumerate(VkPhysicalDevice physical,const char* layer,uint32_t* count,VkExtensionProperties* output) {
    assert((void*)physical==g_phys_dev && !layer && count);enumerate_calls++;
    if(!output) {*count=enum_count;return (VkResult)enum_first_result;}
    unsigned capacity=*count,copy=enum_written<capacity ? enum_written : capacity;
    if(copy)memcpy(output,device_extensions,copy*sizeof *output);
    *count=enum_written;return (VkResult)enum_fill_result;
}
static int original_create(void* instance,const void* info,const void* allocator,void** out) {
    create_calls++;assert(instance==expected_instance && info==expected_info && allocator==expected_allocator);
    assert(g_comp_surface==captured_before);if(out)*out=created_surface;
    return create_result;
}
static int original_formats(void* physical,void* surface,unsigned* count,void* output) {
    (void)physical;(void)surface;driver_calls++;
    if(!count)return VK_ERROR_INITIALIZATION_FAILED;
    if(driver_result)return driver_result;
    if(!output) {*count=driver_size;return VK_SUCCESS;}
    unsigned n=*count<driver_size ? *count : driver_size;
    if(n)memcpy(output,driver_formats,n*sizeof *driver_formats);
    *count=n;return n<driver_size ? VK_INCOMPLETE : VK_SUCCESS;
}
static void original_properties(void* physical,unsigned format,void* output) {
    assert(physical==g_phys_dev);property_calls++;
    VkFormatProperties* p=output;memset(p,0,sizeof *p);
    assert(format==VK_FORMAT_B8G8R8A8_UNORM || format==VK_FORMAT_B8G8R8A8_SRGB || format==VK_FORMAT_R8G8B8A8_SRGB);
    p->optimalTilingFeatures=format==VK_FORMAT_B8G8R8A8_UNORM ? linear_features
        : format==VK_FORMAT_B8G8R8A8_SRGB ? scene_features : present_features;
}
static void* original_gipa(void* instance,const char* name) {
    assert(instance==g_vk_instance);lookup_calls++;
    if(!strcmp(name,"vkEnumerateDeviceExtensionProperties"))return enum_missing ? 0 : (void*)&original_enumerate;
    assert(!strcmp(name,"vkGetPhysicalDeviceFormatProperties"));
    return properties_missing ? 0 : (void*)&original_properties;
}
static void reset(void) {
    memset(driver_formats,0,sizeof driver_formats);
    driver_formats[0]=(VkSurfaceFormatKHR){VK_FORMAT_R8G8B8A8_UNORM,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    driver_formats[1]=(VkSurfaceFormatKHR){VK_FORMAT_R8G8B8A8_SRGB,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    driver_formats[2]=(VkSurfaceFormatKHR){VK_FORMAT_A2B10G10R10_UNORM_PACK32,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    driver_size=3;driver_calls=property_calls=lookup_calls=create_calls=log_calls=0;
    linear_features=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    scene_features=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    present_features=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    driver_result=create_result=properties_missing=g_comp_disable=0;
    g_comp_format_list=1;
    enumerate_calls=allocation_calls=free_calls=0;enum_count=enum_written=1;
    enum_first_result=enum_fill_result=enum_missing=allocation_failure=0;
    memset(device_extensions,0,sizeof device_extensions);
    memcpy(device_extensions[0].extensionName,"VK_KHR_image_format_list",sizeof "VK_KHR_image_format_list");device_extensions[0].specVersion=1;
    g_phys_dev=PTR(0x111);g_vk_instance=PTR(0x222);g_comp_surface=PTR(0x333);g_comp_surface_logs=0;
    r_vkSurfaceFormats=original_formats;r_vkCreateAndroidSurface=original_create;r_gipa=original_gipa;
}
static void passthrough(void* physical,void* surface,unsigned expected_calls) {
    VkSurfaceFormatKHR before[65],output[65];memcpy(before,driver_formats,sizeof before);
    memset(output,0xa5,sizeof output);unsigned count=65;
    int result=h_vkSurfaceFormats(physical,surface,&count,output);
    assert(result==driver_result && driver_calls==expected_calls && !g_comp_surface_logs);
    if(!driver_result)assert(count==driver_size && !memcmp(output,driver_formats,count*sizeof *output));
    else {unsigned char poison[sizeof output];memset(poison,0xa5,sizeof poison);assert(count==65 && !memcmp(output,poison,sizeof output));}
    assert(!memcmp(before,driver_formats,sizeof before));
}
static void create_case(VkAndroidSurfaceCreateInfoKHR info,int result,void* surface,int expect_capture,int with_out) {
    reset();captured_before=g_comp_surface;created_surface=surface;create_result=result;
    expected_instance=g_vk_instance;expected_info=&info;expected_allocator=PTR(0x444);
    VkAndroidSurfaceCreateInfoKHR before=info;void* out=0;
    int actual=h_vkCreateAndroidSurface(expected_instance,&info,expected_allocator,with_out ? &out : 0);
    assert(actual==result && create_calls==1 && !memcmp(&before,&info,sizeof info));
    assert(g_comp_surface==(expect_capture ? created_surface : captured_before));
    assert(!driver_calls && !property_calls);
}
static void prepare_case(const VkDeviceCreateInfo* info,void* physical,int prepared) {
    _Alignas(8) unsigned char storage[80],poison[80];memset(storage,0xa5,sizeof storage);memcpy(poison,storage,sizeof poison);
    const char* names[258];for(unsigned i=0;i<258;i++)names[i]=(const char*)PTR(0x999);
    VkDeviceCreateInfo before;
    if(info)before=*info;
    const char* source_names[256];unsigned source_count=info ? info->enabledExtensionCount : 0;
    int save_names=info && source_count<=256 && info->ppEnabledExtensionNames;
    if(save_names)memcpy(source_names,info->ppEnabledExtensionNames,source_count*sizeof *source_names);
    VkExtensionProperties properties_before[256];memcpy(properties_before,device_extensions,sizeof properties_before);
    const void* result=comp_prepare_device_formats(physical,info,storage,names);
    assert(result==(prepared ? (const void*)storage : info));
    if(info)assert(!memcmp(info,&before,sizeof before));
    if(save_names)assert(!memcmp(source_names,info->ppEnabledExtensionNames,source_count*sizeof *source_names));
    assert(!memcmp(properties_before,device_extensions,sizeof properties_before));
    assert(!memcmp(storage+72,poison+72,8));
    if(prepared) {
        VkDeviceCreateInfo expected=before;expected.enabledExtensionCount++;expected.ppEnabledExtensionNames=names;
        assert(!memcmp(storage,&expected,sizeof expected));
        for(unsigned i=0;i<source_count;i++)assert(names[i]==source_names[i]);
        assert(!strcmp(names[source_count],"VK_KHR_image_format_list"));
        for(unsigned i=source_count+1;i<258;i++)assert(names[i]==(const char*)PTR(0x999));
        comp_capture_device_formats(result,VK_SUCCESS);assert(g_comp_format_list==1);
    } else {
        assert(!memcmp(storage,poison,sizeof storage));
        for(unsigned i=0;i<258;i++)assert(names[i]==(const char*)PTR(0x999));
    }
    assert(!create_calls && !driver_calls && !property_calls);
}
static void prepare_device(void) {
    const char* input[257];for(unsigned i=0;i<257;i++)input[i]="VK_KHR_swapchain";
    input[0]="VK_KHR_maintenance2";
    VkDeviceQueueCreateInfo queue={0};VkPhysicalDeviceFeatures features={0};
    VkDeviceCreateInfo ci={0};ci.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.pNext=PTR(0x123);ci.flags=17;ci.queueCreateInfoCount=1;ci.pQueueCreateInfos=&queue;
    ci.enabledLayerCount=1;ci.ppEnabledLayerNames=input;ci.pEnabledFeatures=&features;
    ci.enabledExtensionCount=2;ci.ppEnabledExtensionNames=input;
    reset();prepare_case(&ci,g_phys_dev,1);assert(enumerate_calls==2 && allocation_calls==1 && free_calls==1 && lookup_calls==1);
    reset();input[0]="VK_KHR_swapchain";prepare_case(&ci,g_phys_dev,1);input[0]="VK_KHR_maintenance2"; // image_format_list has no extension dependency
    reset();ci.enabledExtensionCount=256;prepare_case(&ci,g_phys_dev,1);ci.enabledExtensionCount=2;
    reset();prepare_case(0,g_phys_dev,0);assert(!lookup_calls);
    reset();prepare_case(&ci,0,0);assert(!lookup_calls);
    reset();ci.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;prepare_case(&ci,g_phys_dev,0);ci.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    reset();ci.enabledExtensionCount=257;prepare_case(&ci,g_phys_dev,0);assert(!lookup_calls);ci.enabledExtensionCount=2;
    reset();ci.ppEnabledExtensionNames=0;prepare_case(&ci,g_phys_dev,0);ci.ppEnabledExtensionNames=input;
    reset();ci.enabledExtensionCount=0;ci.ppEnabledExtensionNames=0;prepare_case(&ci,g_phys_dev,1);ci.enabledExtensionCount=2;ci.ppEnabledExtensionNames=input;
    const char* invalid[]={0,""};
    for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];i++) {
        reset();input[0]=invalid[i];prepare_case(&ci,g_phys_dev,0);assert(!lookup_calls);
    }
    char unterminated[256];memset(unterminated,'x',sizeof unterminated);
    reset();input[0]=unterminated;prepare_case(&ci,g_phys_dev,0);assert(!lookup_calls);input[0]="VK_KHR_maintenance2";
    reset();input[1]=0;prepare_case(&ci,g_phys_dev,0);assert(!lookup_calls);input[1]="VK_KHR_swapchain";
    reset();input[1]="VK_KHR_image_format_list";prepare_case(&ci,g_phys_dev,0);assert(!lookup_calls);input[1]="VK_KHR_swapchain";
    reset();r_gipa=0;prepare_case(&ci,g_phys_dev,0);assert(!lookup_calls);
    reset();g_vk_instance=0;prepare_case(&ci,g_phys_dev,0);assert(!lookup_calls);
    reset();enum_missing=1;prepare_case(&ci,g_phys_dev,0);assert(lookup_calls==1 && !enumerate_calls);
    int errors[]={VK_ERROR_INITIALIZATION_FAILED,VK_INCOMPLETE};
    for(unsigned i=0;i<2;i++) {
        reset();enum_first_result=errors[i];prepare_case(&ci,g_phys_dev,0);assert(enumerate_calls==1 && !allocation_calls);
        reset();enum_fill_result=errors[i];prepare_case(&ci,g_phys_dev,0);assert(enumerate_calls==2 && allocation_calls==1 && free_calls==1);
    }
    reset();enum_count=0;prepare_case(&ci,g_phys_dev,0);assert(!allocation_calls);
    reset();enum_count=257;prepare_case(&ci,g_phys_dev,0);assert(!allocation_calls);
    reset();allocation_failure=1;prepare_case(&ci,g_phys_dev,0);assert(enumerate_calls==1 && allocation_calls==1 && !free_calls);
    reset();enum_written=0;prepare_case(&ci,g_phys_dev,0);assert(free_calls==1);
    reset();enum_written=2;prepare_case(&ci,g_phys_dev,0);assert(free_calls==1); // success with impossible larger result is refused
    reset();memcpy(device_extensions[0].extensionName,"VK_KHR_image_format_list_suffix",sizeof "VK_KHR_image_format_list_suffix");prepare_case(&ci,g_phys_dev,0);
    reset();memset(device_extensions[0].extensionName,'x',256);prepare_case(&ci,g_phys_dev,0);
    reset();device_extensions[0].extensionName[0]=0;prepare_case(&ci,g_phys_dev,0);
    reset();enum_count=enum_written=2;prepare_case(&ci,g_phys_dev,0); // later malformed empty name refuses even earlier match
    reset();enum_count=enum_written=256;
    for(unsigned i=0;i<256;i++)memcpy(device_extensions[i].extensionName,"VK_KHR_swapchain",sizeof "VK_KHR_swapchain");
    memcpy(device_extensions[255].extensionName,"VK_KHR_image_format_list",sizeof "VK_KHR_image_format_list");prepare_case(&ci,g_phys_dev,1);
    reset();_Alignas(8) unsigned char out[80];const char* output[257];
    assert(comp_prepare_device_formats(g_phys_dev,&ci,0,output)==&ci);
    assert(comp_prepare_device_formats(g_phys_dev,&ci,out,0)==&ci);
    assert(comp_prepare_device_formats(g_phys_dev,&ci,out+1,output)==&ci && !lookup_calls);
    reset();g_comp_prepare_logs=0;input[1]="VK_KHR_image_format_list";
    for(unsigned i=0;i<10;i++)assert(comp_prepare_device_formats(g_phys_dev,&ci,out,output)==&ci);
    assert(g_comp_prepare_logs==4 && log_calls==4 && !lookup_calls);input[1]="VK_KHR_swapchain";
}
static void capture_case(const VkDeviceCreateInfo* info,int result,int expected) {
    reset();VkDeviceCreateInfo before;
    if(info)before=*info;
    void* surface=g_comp_surface;
    comp_capture_device_formats(info,result);
    assert(g_comp_format_list==expected && log_calls==1);
    assert(!driver_calls && !property_calls && !lookup_calls && !create_calls && g_comp_surface==surface);
    if(info)assert(!memcmp(info,&before,sizeof before));
}
static void device_formats(void) {
    const char* extensions[257];
    for(unsigned i=0;i<257;i++)extensions[i]="VK_KHR_swapchain";
    extensions[0]="VK_KHR_image_format_list";
    VkDeviceCreateInfo ci={0};ci.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.enabledExtensionCount=1;ci.ppEnabledExtensionNames=extensions;
    capture_case(&ci,VK_SUCCESS,1);
    capture_case(&ci,VK_ERROR_INITIALIZATION_FAILED,0);capture_case(&ci,VK_INCOMPLETE,0);
    capture_case(0,VK_SUCCESS,0);
    ci.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;capture_case(&ci,VK_SUCCESS,0);
    ci.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.enabledExtensionCount=0;capture_case(&ci,VK_SUCCESS,0);
    ci.enabledExtensionCount=258;ci.ppEnabledExtensionNames=(const char* const*)PTR(0x999);
    capture_case(&ci,VK_SUCCESS,0); // upper bound must refuse before reading any element
    ci.enabledExtensionCount=1;ci.ppEnabledExtensionNames=0;capture_case(&ci,VK_SUCCESS,0);
    ci.ppEnabledExtensionNames=extensions;
    const char* rejected[]={0,"","VK_KHR_swapchain","VK_KHR_image_format_list2",
        "VK_KHR_image_format_lis","vk_khr_image_format_list","VK_KHR_image_format_list "};
    for(unsigned i=0;i<sizeof rejected/sizeof rejected[0];i++) {
        extensions[0]=rejected[i];capture_case(&ci,VK_SUCCESS,0);
    }
    // Native pNext is legal and borrowed; capture inspects only the enabled extension array.
    ci.pNext=PTR(0x999);extensions[0]="VK_KHR_image_format_list";capture_case(&ci,VK_SUCCESS,1);
    ci.pNext=0;ci.enabledExtensionCount=256;extensions[0]="VK_KHR_swapchain";
    extensions[255]="VK_KHR_image_format_list";capture_case(&ci,VK_SUCCESS,1);
    extensions[255]=0;capture_case(&ci,VK_SUCCESS,0);
    ci.enabledExtensionCount=257;extensions[256]="VK_KHR_image_format_list";capture_case(&ci,VK_SUCCESS,1);
    // A successful capture actually opens the exact owned-surface route; a later refusal closes it.
    ci.enabledExtensionCount=1;extensions[0]="VK_KHR_image_format_list";
    reset();g_comp_format_list=0;comp_capture_device_formats(&ci,VK_SUCCESS);
    unsigned count=0;assert(!h_vkSurfaceFormats(g_phys_dev,g_comp_surface,&count,0) && count==4);
    driver_calls=property_calls=lookup_calls=log_calls=0;g_comp_surface_logs=0;
    comp_capture_device_formats(&ci,VK_ERROR_INITIALIZATION_FAILED);
    passthrough(g_phys_dev,g_comp_surface,1);assert(!property_calls && !lookup_calls);
}
int main(void) {
    prepare_device();
    device_formats();
    reset();unsigned count=123;VkSurfaceFormatKHR before[65];memcpy(before,driver_formats,sizeof before);
    assert(h_vkSurfaceFormats(g_phys_dev,g_comp_surface,&count,0)==VK_SUCCESS && count==4);
    assert(driver_calls==1 && property_calls==3 && !memcmp(before,driver_formats,sizeof before));
    // All capacities: native order preserved, exactly one logical entry, no overwrite outside capacity.
    for(unsigned capacity=0;capacity<=7;capacity++) {
        reset();unsigned char storage[sizeof(VkSurfaceFormatKHR)*9] __attribute__((aligned(8)));memset(storage,0xa5,sizeof storage);
        VkSurfaceFormatKHR* output=(VkSurfaceFormatKHR*)(storage+sizeof(VkSurfaceFormatKHR));count=capacity;
        int result=h_vkSurfaceFormats(g_phys_dev,g_comp_surface,&count,output);
        unsigned written=capacity<4 ? capacity : 4;assert(count==written && result==(capacity<4 ? VK_INCOMPLETE : VK_SUCCESS));
        for(unsigned i=0;i<written;i++) {
            VkSurfaceFormatKHR expected=i<3 ? driver_formats[i] : (VkSurfaceFormatKHR){VK_FORMAT_B8G8R8A8_UNORM,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
            assert(!memcmp(&output[i],&expected,sizeof expected));
        }
        for(unsigned i=0;i<sizeof(VkSurfaceFormatKHR);i++)assert(storage[i]==0xa5);
        for(unsigned i=(written+1)*sizeof(VkSurfaceFormatKHR);i<sizeof storage;i++)assert(storage[i]==0xa5);
        assert(driver_calls==1 && property_calls==3 && lookup_calls==1);
    }
    reset();passthrough(PTR(0x999),g_comp_surface,1);assert(!lookup_calls && !property_calls);
    reset();passthrough(g_phys_dev,PTR(0x999),1);assert(!lookup_calls && !property_calls);
    reset();passthrough(0,g_comp_surface,1);reset();passthrough(g_phys_dev,0,1);
    reset();g_comp_surface=0;passthrough(g_phys_dev,PTR(0x333),1);
    reset();g_phys_dev=0;passthrough(PTR(0x111),g_comp_surface,1);
    reset();g_comp_disable=1;passthrough(g_phys_dev,g_comp_surface,1);
    reset();g_comp_format_list=0;passthrough(g_phys_dev,g_comp_surface,1);assert(!lookup_calls && !property_calls);
    reset();assert(h_vkSurfaceFormats(g_phys_dev,g_comp_surface,0,0)==VK_ERROR_INITIALIZATION_FAILED && driver_calls==1);
    reset();driver_result=VK_ERROR_SURFACE_LOST_KHR;passthrough(g_phys_dev,g_comp_surface,2);
    reset();driver_result=VK_INCOMPLETE;passthrough(g_phys_dev,g_comp_surface,2);
    reset();driver_size=0;passthrough(g_phys_dev,g_comp_surface,2);
    reset();driver_size=64;passthrough(g_phys_dev,g_comp_surface,2);
    reset();driver_size=65;passthrough(g_phys_dev,g_comp_surface,2);
    reset();driver_formats[2]=(VkSurfaceFormatKHR){VK_FORMAT_B8G8R8A8_UNORM,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    passthrough(g_phys_dev,g_comp_surface,2);assert(!lookup_calls && !property_calls);
    reset();driver_formats[2]=(VkSurfaceFormatKHR){VK_FORMAT_B8G8R8A8_UNORM,VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT};
    passthrough(g_phys_dev,g_comp_surface,2);assert(!lookup_calls && !property_calls);
    reset();driver_formats[1].format=VK_FORMAT_R8G8B8A8_UNORM;passthrough(g_phys_dev,g_comp_surface,2);assert(!property_calls);
    reset();driver_formats[1].colorSpace=VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;passthrough(g_phys_dev,g_comp_surface,2);assert(!property_calls);
    reset();r_gipa=0;passthrough(g_phys_dev,g_comp_surface,2);
    reset();g_vk_instance=0;passthrough(g_phys_dev,g_comp_surface,2);
    reset();properties_missing=1;passthrough(g_phys_dev,g_comp_surface,2);assert(!property_calls);
    reset();linear_features=0;passthrough(g_phys_dev,g_comp_surface,2);
    reset();linear_features&=~VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;passthrough(g_phys_dev,g_comp_surface,2);
    reset();linear_features&=~VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;passthrough(g_phys_dev,g_comp_surface,2);
    reset();present_features=0;passthrough(g_phys_dev,g_comp_surface,2);
    unsigned needed[]={VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT,VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT};
    for(unsigned i=0;i<2;i++) {reset();scene_features&=~needed[i];passthrough(g_phys_dev,g_comp_surface,2);}
    // Foreign calls never change the driver's real list or the captured game surface.
    reset();void* selected=g_comp_surface;passthrough(g_phys_dev,PTR(0x999),1);assert(g_comp_surface==selected);
    VkAndroidSurfaceCreateInfoKHR info={VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,0,0,(struct ANativeWindow*)PTR(0x555)};
    create_case(info,VK_SUCCESS,PTR(0x666),1,1);
    create_case(info,VK_ERROR_INITIALIZATION_FAILED,PTR(0x666),0,1);
    create_case(info,VK_SUCCESS,0,0,1);create_case(info,VK_SUCCESS,PTR(0x666),0,0);
    info.pNext=PTR(0x777);create_case(info,VK_SUCCESS,PTR(0x666),0,1);info.pNext=0;
    info.window=0;create_case(info,VK_SUCCESS,PTR(0x666),0,1);
    info.window=(struct ANativeWindow*)PTR(0x555);info.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    create_case(info,VK_SUCCESS,PTR(0x666),0,1);
    reset();captured_before=g_comp_surface;created_surface=PTR(0x666);expected_instance=g_vk_instance;expected_info=0;expected_allocator=0;
    void* out=0;assert(h_vkCreateAndroidSurface(expected_instance,0,0,&out)==VK_SUCCESS && g_comp_surface==captured_before);
    // Original-first creation establishes the exact surface accepted by the next native formats call.
    info.sType=VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;create_case(info,VK_SUCCESS,PTR(0x666),1,1);
    count=0;assert(h_vkSurfaceFormats(g_phys_dev,PTR(0x666),&count,0)==VK_SUCCESS && count==4);
    driver_calls=0;passthrough(g_phys_dev,PTR(0x333),1);
    reset();VkSurfaceFormatKHR output[4];for(int i=0;i<10;i++){count=4;assert(!h_vkSurfaceFormats(g_phys_dev,g_comp_surface,&count,output));}
    assert(g_comp_surface_logs==2 && log_calls==2);
    puts("PASS: actual compositor surface helper; supported extension-only device CI clone with no extra dependency (including empty original extension list), two bounded original queries and allocation/error/refusal guards; exact native capture/gate; original-first Android surface; bounded BGRA44 alias; all borrowed inputs and driver formats unchanged");
}
