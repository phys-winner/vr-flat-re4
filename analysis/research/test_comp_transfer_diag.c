#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan_core.h>
#define COMP_MAX 8
static void* g_own_imgs[8]={(void*)0xabcdef0123456780ULL},*g_comp_img[8]={(void*)0xabcdef0123456790ULL};
static unsigned g_sc_fmt_real=43,g_comp_w=2400,g_comp_h=1080,g_comp_fmt=50;
static int g_comp_mutable=1;
static unsigned log_calls,create_calls,copy_calls,blit_calls,resolve_calls;
static void test_log(const char* format,...);
#define LOGF(...) test_log(__VA_ARGS__)
#include "../re4fix_comp_transfer_diag.h"
#ifndef RE4_TEST_MUTEX_LOCKED
#define RE4_TEST_MUTEX_LOCKED(m) 0
#endif
static VkDevice device=(VkDevice)(uintptr_t)0x1234567812345678ULL;
static VkCommandBuffer command=(VkCommandBuffer)(uintptr_t)0xfedcba9876543210ULL;
static VkImage source=(VkImage)(uintptr_t)0x7654321098765432ULL,destination;
static VkImageCreateInfo info={VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
static const VkAllocationCallbacks* allocator=(const VkAllocationCallbacks*)(uintptr_t)0x1234567887654321ULL;
static VkResult create_result;
static VkImage output;
static VkImageCopy copies[2];static VkImageBlit blits[2];static VkImageResolve resolves[2];
static unsigned expected_count=2;static const void* expected_regions;
static void test_log(const char* format,...){assert(!RE4_TEST_MUTEX_LOCKED(comp_transfer_mutex)&&strstr(format,"COMP TRANSFER"));log_calls++;}
static VKAPI_ATTR VkResult VKAPI_CALL create(VkDevice d,const VkImageCreateInfo* ci,const VkAllocationCallbacks* a,VkImage* out){assert(!RE4_TEST_MUTEX_LOCKED(comp_transfer_mutex)&&d==device&&ci==&info&&a==allocator&&out==&output);create_calls++;*out=source;return create_result;}
static void arguments(VkCommandBuffer c,VkImage s,VkImageLayout sl,VkImage d,VkImageLayout dl,unsigned count,const void* regions){assert(!RE4_TEST_MUTEX_LOCKED(comp_transfer_mutex)&&c==command&&s==source&&d==destination&&sl==VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL&&dl==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL&&count==expected_count&&regions==expected_regions);}
static VKAPI_ATTR void VKAPI_CALL copy(VkCommandBuffer c,VkImage s,VkImageLayout sl,VkImage d,VkImageLayout dl,uint32_t count,const VkImageCopy* r){arguments(c,s,sl,d,dl,count,r);copy_calls++;}
static VKAPI_ATTR void VKAPI_CALL blit(VkCommandBuffer c,VkImage s,VkImageLayout sl,VkImage d,VkImageLayout dl,uint32_t count,const VkImageBlit* r,VkFilter f){arguments(c,s,sl,d,dl,count,r);assert(f==VK_FILTER_LINEAR);blit_calls++;}
static VKAPI_ATTR void VKAPI_CALL resolve(VkCommandBuffer c,VkImage s,VkImageLayout sl,VkImage d,VkImageLayout dl,uint32_t count,const VkImageResolve* r){arguments(c,s,sl,d,dl,count,r);resolve_calls++;}
static void do_copy(void){expected_regions=copies;h_comp_transfer_CmdCopyImage(command,source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,destination,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,expected_count,copies);}
int main(void){
 assert(comp_transfer_proc(0,(void*)copy)==(void*)copy&&!comp_transfer_proc("vkCreateImage",0));assert(comp_transfer_proc("vkOther",(void*)copy)==(void*)copy);
 assert(comp_transfer_proc("vkCreateImage",(void*)create)==(void*)h_comp_transfer_CreateImage);
 assert(comp_transfer_proc("vkCmdCopyImage",(void*)copy)==(void*)h_comp_transfer_CmdCopyImage);
 assert(comp_transfer_proc("vkCmdBlitImage",(void*)blit)==(void*)h_comp_transfer_CmdBlitImage);
 assert(comp_transfer_proc("vkCmdResolveImage",(void*)resolve)==(void*)h_comp_transfer_CmdResolveImage);
 assert(comp_transfer_proc("vkCmdCopyImage",(void*)h_comp_transfer_CmdCopyImage)==(void*)h_comp_transfer_CmdCopyImage&&comp_transfer_copy_image==copy);
 info.format=VK_FORMAT_R8G8B8A8_SRGB;info.extent=(VkExtent3D){2400,1080,1};info.samples=VK_SAMPLE_COUNT_4_BIT;
 VkImageCreateInfo before=info;create_result=VK_ERROR_OUT_OF_DEVICE_MEMORY;assert(h_comp_transfer_CreateImage(device,&info,allocator,&output)==create_result&&!comp_transfer_next&&output==source);
 create_result=VK_SUCCESS;assert(h_comp_transfer_CreateImage(device,&info,allocator,&output)==VK_SUCCESS&&comp_transfer_next==1&&!memcmp(&before,&info,sizeof info));
 struct comp_transfer_image known=comp_transfer_find(source);assert(known.format==43&&known.samples==VK_SAMPLE_COUNT_4_BIT&&known.extent.width==2400&&known.kind==1);
 assert(comp_transfer_find((VkImage)g_own_imgs[0]).kind==3&&comp_transfer_find((VkImage)g_comp_img[0]).kind==2);
 copies[0].extent=resolves[0].extent=(VkExtent3D){2400,1080,1};blits[0].dstOffsets[0]=(VkOffset3D){2400,1080,1};
 VkImageCopy copy_before[2];memcpy(copy_before,copies,sizeof copies);destination=(VkImage)g_comp_img[0];do_copy();assert(copy_calls==1&&log_calls==1&&!memcmp(copy_before,copies,sizeof copies));
 expected_regions=blits;h_comp_transfer_CmdBlitImage(command,source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,destination,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,2,blits,VK_FILTER_LINEAR);assert(blit_calls==1&&log_calls==2);
 expected_regions=resolves;h_comp_transfer_CmdResolveImage(command,source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,destination,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,2,resolves);assert(resolve_calls==1&&log_calls==3);
 assert(comp_transfer_span(INT32_MIN,INT32_MAX)==UINT32_MAX);
 for(unsigned i=0;i<100;i++)do_copy();assert(copy_calls==101&&comp_transfer_owned_logs==64&&log_calls==64);
 destination=(VkImage)(uintptr_t)0x1234567800000001ULL;for(unsigned i=0;i<40;i++)do_copy();assert(copy_calls==141&&comp_transfer_large_logs==20&&log_calls==84);
 expected_count=0;expected_regions=0;h_comp_transfer_CmdCopyImage(command,source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,destination,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,0);assert(copy_calls==142&&log_calls==84);
 for(unsigned i=0;i<300;i++){source=(VkImage)(uintptr_t)(0x100000000ULL+i);assert(h_comp_transfer_CreateImage(device,&info,allocator,&output)==VK_SUCCESS);}
 assert(comp_transfer_next==301&&create_calls==302&&comp_transfer_find(source).format==43);
 VkImage viewport=(VkImage)(uintptr_t)0x9876543200000001ULL;source=viewport;info.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
 assert(h_comp_transfer_CreateImage(device,&info,allocator,&output)==VK_SUCCESS);
 info.usage=0;
 for(unsigned i=0;i<700;i++){source=(VkImage)(uintptr_t)(0x200000000ULL+i);assert(h_comp_transfer_CreateImage(device,&info,allocator,&output)==VK_SUCCESS);}
 assert(comp_transfer_find(viewport).format==43&&comp_transfer_find(viewport).attachment);
 unsigned retained=comp_transfer_next;info.format=VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
 assert(h_comp_transfer_CreateImage(device,&info,allocator,&output)==VK_SUCCESS&&comp_transfer_next==retained);
 info.format=VK_FORMAT_R8G8B8A8_UNORM;info.extent=(VkExtent3D){32,32,1};
 assert(h_comp_transfer_CreateImage(device,&info,allocator,&output)==VK_SUCCESS&&comp_transfer_next==retained);
 puts("PASS: actual read-only transfer hooks, SDK prototypes, exact once-only forwarding of 64-bit handles/layouts/counts/borrowed pointers, no native calls under mutex, unchanged inputs, bounded256 registry/64owned+20large logs, original result propagation and idempotent proc routes");return 0;
}
