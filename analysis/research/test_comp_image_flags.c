#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan_core.h>
_Static_assert(sizeof(VkImageCreateInfo)==88,"native image CI ABI");
_Static_assert(offsetof(VkImageCreateInfo,flags)==16,"native flags ABI");
_Static_assert(sizeof(VkImageFormatListCreateInfo)==32,"native format list ABI");
static uintptr_t g_ue4_base_cached,test_native_caller;
static int g_comp_on=1,g_comp_disable,g_comp_mutable=1;
static volatile int g_comp_format_list=1;
static unsigned g_comp_w=2400,g_comp_h=1080;
static void* g_comp_dev=(void*)(uintptr_t)0xabcdef0123456780ULL;
static unsigned calls,logs;
#define LOGF(...) (++logs)
// The hook itself stays actual C; this fixture supplies the otherwise machine-specific native LR.
#define __builtin_return_address(level) ((void*)test_native_caller)
#include "../re4fix_comp_image_flags.h"
#undef __builtin_return_address
#define H(type,n) ((type)(uintptr_t)(n))
static VkDevice device=H(VkDevice,0xabcdef0123456780ULL);
static const VkAllocationCallbacks* allocator=H(const VkAllocationCallbacks*,0xabcdef0123456790ULL);
static VkImage image=H(VkImage,0x1234567887654321ULL),output;
static VkImageCreateInfo original,expected;
static VkImageFormatListCreateInfo list;
static VkFormat formats[3];
static VkBaseInStructure tail;
static const VkImageCreateInfo* expected_pointer;
static int expected_copy,expected_added;
static VkResult result;
static VKAPI_ATTR VkResult VKAPI_CALL create_image(VkDevice dev,const VkImageCreateInfo* ci,const VkAllocationCallbacks* ac,VkImage* out){
 assert(dev==device&&ac==allocator&&out==&output);
 if(expected_copy)assert(ci!=expected_pointer);else assert(ci==expected_pointer);
 if(ci){
  if(expected_added){
   const VkImageFormatListCreateInfo* fresh=ci->pNext;
   assert(fresh&&fresh!=&list&&!((uintptr_t)fresh&7)&&fresh->sType==VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO
    &&fresh->pNext==list.pNext&&!fresh->viewFormatCount&&!fresh->pViewFormats);
   VkImageCreateInfo normalized=*ci;normalized.pNext=expected.pNext;assert(!memcmp(&normalized,&expected,sizeof expected));
  }else assert(!memcmp(ci,&expected,sizeof expected));
 }
 calls++;*out=image;return result;
}
static void reset_ci(void){
 memset(&original,0,sizeof original);original.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;original.imageType=VK_IMAGE_TYPE_2D;
 original.format=VK_FORMAT_B8G8R8A8_SRGB;original.extent=(VkExtent3D){2400,1080,1};original.mipLevels=original.arrayLayers=1;
 original.samples=VK_SAMPLE_COUNT_4_BIT;original.tiling=VK_IMAGE_TILING_OPTIMAL;original.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
 formats[0]=VK_FORMAT_B8G8R8A8_UNORM;formats[1]=VK_FORMAT_B8G8R8A8_SRGB;formats[2]=VK_FORMAT_R8G8B8A8_SRGB;
 list=(VkImageFormatListCreateInfo){VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO};list.viewFormatCount=2;list.pViewFormats=formats;
 original.pNext=&list;expected=original;expected.flags|=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;expected_copy=1;expected_added=0;expected_pointer=&original;
 g_ue4_base_cached=0x7000000000ULL;test_native_caller=g_ue4_base_cached+0x6545984;result=VK_SUCCESS;output=0;
 g_comp_mutable=g_comp_format_list=1;g_comp_on=g_comp_disable=0;g_comp_w=2400;g_comp_h=1080;g_comp_dev=(void*)device;
}
static void invoke(void){
 VkImageCreateInfo ci_before=original;VkImageFormatListCreateInfo list_before=list;
 VkFormat formats_before[3];memcpy(formats_before,formats,sizeof formats);
 unsigned before=calls;assert(h_comp_image_flags_CreateImage(device,&original,allocator,&output)==result);
 assert(calls==before+1&&output==image&&!memcmp(&original,&ci_before,sizeof original)
  &&!memcmp(&list,&list_before,sizeof list)&&!memcmp(formats,formats_before,sizeof formats));
}
int main(void){
 PFN_vkCreateImage abi=h_comp_image_flags_CreateImage;assert(abi);
 assert(comp_image_flags_proc(0,(void*)create_image)==(void*)create_image&&!comp_image_flags_proc("vkCreateImage",0));
 assert(comp_image_flags_proc("vkUnknown",(void*)create_image)==(void*)create_image);
 assert(comp_image_flags_proc("vkCreateImage",(void*)create_image)==(void*)abi);
 assert(comp_image_flags_proc("vkCreateImage",(void*)abi)==(void*)abi&&comp_image_flags_create_image==create_image&&!calls&&!logs);
 reset_ci();invoke();assert(logs==1&&g_comp_image_flags_patches==1);
 reset_ci();original.flags=VK_IMAGE_CREATE_ALIAS_BIT;expected=original;expected.flags|=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;invoke();
 reset_ci();tail=(VkBaseInStructure){VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,0};list.pNext=&tail;invoke();assert(list.pNext==&tail);
 for(unsigned i=0;i<7;i++){
  reset_ci();
  if(i==0){original.pNext=0;g_comp_on=0;}if(i==1)original.flags|=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
  if(i==2)list.sType=VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
  if(i==3)list.viewFormatCount=0;if(i==4)list.viewFormatCount=1;if(i==5)list.viewFormatCount=3;if(i==6)list.pViewFormats=0;
  expected=original;expected_copy=0;invoke();
 }
 reset_ci();original.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;expected=original;expected_copy=0;invoke();
 // The native generator computes its compatible pair; the repair adds no format/device/MSAA policy.
 for(unsigned samples=1;samples<=8;samples*=2){
  reset_ci();original.samples=(VkSampleCountFlagBits)samples;original.format=VK_FORMAT_R8G8B8A8_SRGB;
  formats[0]=VK_FORMAT_R8G8B8A8_UNORM;formats[1]=VK_FORMAT_R8G8B8A8_SRGB;
  expected=original;expected.flags|=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;invoke();
 }
 reset_ci();result=VK_ERROR_OUT_OF_DEVICE_MEMORY;invoke();assert(logs==6);
 reset_ci();expected=original;expected_copy=0;test_native_caller++;invoke();
 reset_ci();expected=original;expected_copy=0;g_ue4_base_cached=0;invoke();
 reset_ci();
 VkImageCreateInfo copied;
 VkImageFormatListCreateInfo added;
 assert(comp_image_flags_info(device,&original,&copied,&added,formats,g_ue4_base_cached+0x6545984)==&copied);
 assert(copied.flags==VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT&&copied.pNext==original.pNext&&copied.format==original.format);
 assert(comp_image_flags_info(device,&original,&copied,&added,formats,g_ue4_base_cached+0x6545980)==&original);
 assert(comp_image_flags_info(device,&original,&copied,&added,formats,g_ue4_base_cached+0x6545988)==&original);
 assert(!comp_image_flags_info(device,0,&copied,&added,formats,g_ue4_base_cached+0x6545984));
 assert(comp_image_flags_info(device,&original,0,&added,formats,g_ue4_base_cached+0x6545984)==&original);
 expected_pointer=0;expected_copy=0;unsigned before=calls;assert(h_comp_image_flags_CreateImage(device,0,allocator,&output)==VK_SUCCESS&&calls==before+1);
 // Native viewport MSAA2 and MSAA4 opt into mutable views without changing sRGB storage or the actual usage0x17.
 reset_ci();g_comp_on=1;original.pNext=0;expected=original;expected.flags|=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;invoke();
 assert(original.format==VK_FORMAT_B8G8R8A8_SRGB&&!original.pNext);
 image=H(VkImage,0x1234567887654322ULL);result=VK_ERROR_OUT_OF_DEVICE_MEMORY;invoke();
 result=VK_SUCCESS;invoke();
 reset_ci();g_comp_on=1;original.pNext=0;original.samples=VK_SAMPLE_COUNT_2_BIT;original.usage|=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
 expected=original;expected.flags|=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;invoke();assert(original.format==50&&original.usage==0x17);
 for(unsigned failure=0;failure<23;failure++){
  reset_ci();g_comp_on=1;original.pNext=0;
  if(failure==0)g_comp_on=0;if(failure==1)g_comp_disable=1;if(failure==2)g_comp_mutable=0;if(failure==3)g_comp_dev=(void*)0x999;
  if(failure==4)original.format=VK_FORMAT_R8G8B8A8_UNORM;if(failure==5)original.samples=VK_SAMPLE_COUNT_1_BIT;
  if(failure==6)original.imageType=VK_IMAGE_TYPE_1D;if(failure==7)original.extent.width++;if(failure==8)original.extent.height++;
  if(failure==9)original.extent.depth=2;if(failure==10)original.mipLevels=2;if(failure==11)original.arrayLayers=2;
  if(failure==12)original.tiling=VK_IMAGE_TILING_LINEAR;if(failure==13)original.usage=VK_IMAGE_USAGE_SAMPLED_BIT;
  if(failure==14)original.initialLayout=VK_IMAGE_LAYOUT_PREINITIALIZED;if(failure==15)original.sharingMode=VK_SHARING_MODE_CONCURRENT;
  if(failure==16)original.queueFamilyIndexCount=1;if(failure==17)original.pQueueFamilyIndices=(const uint32_t*)formats;
  if(failure==18)g_comp_w=0;if(failure==19)g_comp_h=0;if(failure==20)g_ue4_base_cached=0;if(failure==21)test_native_caller+=4;
  if(failure==22)original.samples=VK_SAMPLE_COUNT_8_BIT;
  expected=original;expected_copy=0;invoke();
 }
 reset_ci();g_comp_on=1;original.pNext=0;
 assert(comp_image_flags_info(device,&original,&copied,0,formats,test_native_caller)==&original);
 assert(comp_image_flags_info(device,&original,&copied,&added,0,test_native_caller)==&original);
 // Mutable-format is core; never introduce an unsupported optional format-list structure.
 for(unsigned samples=2;samples<=4;samples*=2){
  reset_ci();g_comp_on=1;original.pNext=0;original.samples=(VkSampleCountFlagBits)samples;g_comp_format_list=0;
  expected=original;expected.flags|=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;expected_added=0;invoke();
  assert(comp_image_flags_info(device,&original,&copied,0,0,test_native_caller)==&copied);
  assert(copied.flags==VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT&&!copied.pNext&&copied.format==50);
 }
 // Known viewport MSAA lists become unrestricted through a stack clone; original lists and tails are immutable.
 for(unsigned capability=0;capability<2;capability++)for(unsigned samples=2;samples<=4;samples*=2)
 for(unsigned mutable=0;mutable<2;mutable++)for(unsigned order=0;order<2;order++) {
  reset_ci();g_comp_on=1;g_comp_format_list=capability;original.samples=(VkSampleCountFlagBits)samples;
  if(mutable)original.flags|=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
  if(order){formats[0]=50;formats[1]=44;}
  tail=(VkBaseInStructure){VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,0};list.pNext=&tail;
  expected=original;expected.flags|=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;expected_added=1;invoke();
 }
 for(unsigned mutable=0;mutable<2;mutable++) {
  reset_ci();g_comp_on=1;list.viewFormatCount=0;list.pViewFormats=(const VkFormat*)(uintptr_t)1;
  if(mutable)original.flags|=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
  expected=original;expected.flags|=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;expected_copy=!mutable;invoke();
 }
 for(unsigned mismatch=0;mismatch<5;mismatch++) {
  reset_ci();g_comp_on=1;original.flags|=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
  if(mismatch==0)original.samples=VK_SAMPLE_COUNT_1_BIT;
  if(mismatch==1)original.samples=VK_SAMPLE_COUNT_8_BIT;
  if(mismatch==2)original.extent.width++;
  if(mismatch==3){formats[0]=VK_FORMAT_R8G8B8A8_UNORM;formats[1]=VK_FORMAT_R8G8B8A8_SRGB;}
  if(mismatch==4)original.pNext=0; // Already unrestricted: no redundant driver-CI rewrite.
  expected=original;expected_copy=0;invoke();
 }
 reset_ci();g_comp_on=1;expected=original;expected.flags|=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;expected_added=1;
 result=VK_ERROR_FORMAT_NOT_SUPPORTED;invoke();
 logs=g_comp_image_flags_patches=g_comp_image_flags_msaa_patches=g_comp_image_flags_msaa2_patches=0;
 for(unsigned phase=0;phase<3;phase++)for(unsigned i=0;i<20;i++){
  reset_ci();original.samples=phase==2?VK_SAMPLE_COUNT_2_BIT:phase?VK_SAMPLE_COUNT_4_BIT:VK_SAMPLE_COUNT_1_BIT;
  expected=original;expected.flags|=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;invoke();
 }
 assert(logs==12&&g_comp_image_flags_patches==60&&g_comp_image_flags_msaa_patches==20&&g_comp_image_flags_msaa2_patches==20);
 puts("PASS: actual native image flag header/SDK ABI and exact caller; viewportMSAA2/4 unrestricted core mutable for extension0/1, NULL-chain unchanged, known44/50 head cloned count0 with immutable original/tail and preexisting mutable bit handled; count0 ignores invalid format pointer; offscope headlist2 validity repair and23scope boundaries including MSAA1/8 unchanged; original once with exact errors/outputs and bounded logs");return 0;
}
