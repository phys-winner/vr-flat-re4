#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <vulkan/vulkan_core.h>
#define COMP_MAX 8
static int g_comp_on=1,g_comp_disable,g_comp_mutable=1;
static unsigned g_comp_n=4;
static void* g_comp_dev=(void*)0x777;
static void* g_phys_dev=(void*)0x778;
static unsigned g_comp_w=2400,g_comp_h=1080;
static void* g_comp_img[COMP_MAX]={(void*)0x100,(void*)0x200,(void*)0x300,(void*)0x400};
static void* g_own_imgs[COMP_MAX]={(void*)0x110,(void*)0x210,(void*)0x310,(void*)0x410};
static unsigned step,clears,lookups,logs;
static void* wanted_cmd=(void*)0x1000;
static unsigned wanted_index;
static float expected_color[4];
static unsigned wanted_srgb,passes,views,framebuffers,rendered;
#define LOGF(...) (++logs)
static void comp_barrier(void* cmd,void* image,unsigned old,unsigned next,unsigned src,unsigned dst,unsigned srcstage,unsigned dststage){
 assert(cmd==wanted_cmd&&image==g_comp_img[wanted_index]);
 assert(step==0||step==2);
 assert(old==(step?VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
 assert(next==(step?VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
 assert(src==(step?VK_ACCESS_TRANSFER_WRITE_BIT:VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT));
 assert(dst==(step?VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT:VK_ACCESS_TRANSFER_WRITE_BIT));
 assert(srcstage==(step?VK_PIPELINE_STAGE_TRANSFER_BIT:VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT));
 assert(dststage==(step?VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT:VK_PIPELINE_STAGE_TRANSFER_BIT));step++;
}
static void clear(void* cmd,void* image,unsigned layout,const float* color,unsigned count,const void* subresources){
 assert(step++==1&&cmd==wanted_cmd&&image==g_comp_img[wanted_index]&&layout==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL&&count==1);
 const VkImageSubresourceRange* range=subresources;
 assert(range->aspectMask==VK_IMAGE_ASPECT_COLOR_BIT&&!range->baseMipLevel&&range->levelCount==1&&!range->baseArrayLayer&&range->layerCount==1);
 assert(!memcmp(color,expected_color,sizeof expected_color));clears++;
}
static VkRenderPass pass_handle(unsigned srgb){return (VkRenderPass)(uintptr_t)(0xa000+srgb*0x100);}
static VkImageView view_handle(unsigned srgb,unsigned index){return (VkImageView)(uintptr_t)(0xb000+srgb*0x100+index*8);}
static VkFramebuffer framebuffer_handle(unsigned srgb,unsigned index){return (VkFramebuffer)(uintptr_t)(0xc000+srgb*0x100+index*8);}
static VkResult create_pass(VkDevice device,const VkRenderPassCreateInfo* info,const VkAllocationCallbacks* allocator,VkRenderPass* output){
 assert(device==(VkDevice)g_comp_dev&&!allocator&&info->sType==VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO&&!info->pNext&&!info->flags);
 assert(info->attachmentCount==1&&info->subpassCount==1&&info->dependencyCount==1);
 const VkAttachmentDescription* attachment=info->pAttachments;
 assert(attachment->format==(wanted_srgb?VK_FORMAT_B8G8R8A8_SRGB:VK_FORMAT_B8G8R8A8_UNORM)&&attachment->samples==VK_SAMPLE_COUNT_1_BIT);
 assert(attachment->loadOp==VK_ATTACHMENT_LOAD_OP_CLEAR&&attachment->storeOp==VK_ATTACHMENT_STORE_OP_STORE);
 assert(attachment->initialLayout==VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL&&attachment->finalLayout==VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
 const VkSubpassDescription* subpass=info->pSubpasses;
 assert(subpass->pipelineBindPoint==VK_PIPELINE_BIND_POINT_GRAPHICS&&subpass->colorAttachmentCount==1&&!subpass->pColorAttachments->attachment);
 assert(subpass->pColorAttachments->layout==VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
 const VkSubpassDependency* dependency=info->pDependencies;
 assert(dependency->srcSubpass==VK_SUBPASS_EXTERNAL&&!dependency->dstSubpass);
 assert(dependency->srcStageMask==VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT&&dependency->dstStageMask==VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
 assert(dependency->srcAccessMask==VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT&&dependency->dstAccessMask==VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
 *output=pass_handle(wanted_srgb);passes++;return VK_SUCCESS;
}
static VkResult create_view(VkDevice device,const VkImageViewCreateInfo* info,const VkAllocationCallbacks* allocator,VkImageView* output){
 assert(device==(VkDevice)g_comp_dev&&!allocator&&info->sType==VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO&&!info->pNext&&!info->flags);
 assert(info->image==(VkImage)g_comp_img[wanted_index]&&info->viewType==VK_IMAGE_VIEW_TYPE_2D);
 assert(info->format==(wanted_srgb?VK_FORMAT_B8G8R8A8_SRGB:VK_FORMAT_B8G8R8A8_UNORM));
 assert(info->components.r==VK_COMPONENT_SWIZZLE_IDENTITY&&info->components.g==VK_COMPONENT_SWIZZLE_IDENTITY&&info->components.b==VK_COMPONENT_SWIZZLE_IDENTITY&&info->components.a==VK_COMPONENT_SWIZZLE_IDENTITY);
 assert(info->subresourceRange.aspectMask==VK_IMAGE_ASPECT_COLOR_BIT&&info->subresourceRange.levelCount==1&&info->subresourceRange.layerCount==1&&!info->subresourceRange.baseMipLevel&&!info->subresourceRange.baseArrayLayer);
 *output=view_handle(wanted_srgb,wanted_index);views++;return VK_SUCCESS;
}
static VkResult create_framebuffer(VkDevice device,const VkFramebufferCreateInfo* info,const VkAllocationCallbacks* allocator,VkFramebuffer* output){
 assert(device==(VkDevice)g_comp_dev&&!allocator&&info->sType==VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO&&!info->pNext&&!info->flags);
 assert(info->renderPass==pass_handle(wanted_srgb)&&info->attachmentCount==1&&*info->pAttachments==view_handle(wanted_srgb,wanted_index));
 assert(info->width==2400&&info->height==1080&&info->layers==1);
 *output=framebuffer_handle(wanted_srgb,wanted_index);framebuffers++;return VK_SUCCESS;
}
static void begin_pass(VkCommandBuffer cmd,const VkRenderPassBeginInfo* info,VkSubpassContents contents){
 assert(step++==0&&cmd==(VkCommandBuffer)wanted_cmd&&contents==VK_SUBPASS_CONTENTS_INLINE);
 assert(info->sType==VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO&&!info->pNext&&info->renderPass==pass_handle(wanted_srgb)&&info->framebuffer==framebuffer_handle(wanted_srgb,wanted_index));
 assert(!info->renderArea.offset.x&&!info->renderArea.offset.y&&info->renderArea.extent.width==2400&&info->renderArea.extent.height==1080);
 assert(info->clearValueCount==1&&!memcmp(info->pClearValues->color.float32,expected_color,sizeof expected_color));
}
static void end_pass(VkCommandBuffer cmd){assert(step++==1&&cmd==(VkCommandBuffer)wanted_cmd);rendered++;}
static void* comp_fn(const char* name){
 lookups++;
 if(!strcmp(name,"vkCmdClearColorImage"))return (void*)clear;
 if(!strcmp(name,"vkCreateRenderPass"))return (void*)create_pass;
 if(!strcmp(name,"vkCreateImageView"))return (void*)create_view;
 if(!strcmp(name,"vkCreateFramebuffer"))return (void*)create_framebuffer;
 if(!strcmp(name,"vkCmdBeginRenderPass"))return (void*)begin_pass;
 if(!strcmp(name,"vkCmdEndRenderPass"))return (void*)end_pass;
 assert(0);return 0;
}
#include "../re4fix_comp_color_diag.h"
static void frame(unsigned index){wanted_index=index;step=0;comp_color_diag(wanted_cmd,index);}
int main(void){
 frame(0);assert(!step&&!clears&&!lookups);
 for(int mode=1;mode<=5;mode++){
  memset(expected_color,0,sizeof expected_color);expected_color[3]=1;
  if(mode<=3)expected_color[mode-1]=1;else for(unsigned k=0;k<3;k++)expected_color[k]=mode==4?.25f:1;
  g_comp_color_request=mode;
  unsigned old=clears;
  for(unsigned i=0;i<360;i++){frame(i%4);assert(step==3);}
  frame(0);assert(!step&&clears==old+360&&!g_comp_color_mode&&!g_comp_color_frames);
 }
 assert(lookups==1&&logs==10);
 for(int mode=6;mode<=13;mode++){
  wanted_srgb=mode>=10;unsigned channel=(unsigned)(mode-(wanted_srgb?10:6));
  memset(expected_color,0,sizeof expected_color);expected_color[3]=1;
  if(channel<3)expected_color[channel]=1;else for(unsigned k=0;k<3;k++)expected_color[k]=.25f;
  g_comp_color_request=mode;unsigned before=rendered;
  for(unsigned i=0;i<360;i++){frame(i%4);assert(step==2);}
  frame(0);assert(!step&&rendered==before+360&&!g_comp_color_mode&&!g_comp_color_frames);
 }
 assert(lookups==6&&passes==2&&views==8&&framebuffers==8&&logs==26);
 // Resource reuse refuses another proxy generation, device or extent before any pass begins.
 for(unsigned mode=0;mode<3;mode++){
  g_comp_color_request=6;
  if(mode==0)g_comp_img[0]=(void*)0x999;if(mode==1)g_comp_dev=(void*)0x999;if(mode==2)g_comp_w=2401;
  frame(0);assert(!step&&!g_comp_color_mode&&!g_comp_color_frames);
  g_comp_img[0]=(void*)0x100;g_comp_dev=(void*)0x777;g_comp_w=2400;
 }
 memset(expected_color,0,sizeof expected_color);expected_color[0]=expected_color[3]=1;
 g_comp_color_request=1;frame(0);g_comp_color_request=0;frame(1);assert(!step&&!g_comp_color_mode);
 for(int mode=0;mode<6;mode++){
  g_comp_color_request=1;
  if(mode==0)g_comp_on=0;if(mode==1)g_comp_disable=1;if(mode==2)g_comp_mutable=0;
  if(mode==3)g_comp_img[2]=0;if(mode==4)g_own_imgs[2]=0;
  frame(mode==5?8:2);assert(!step&&g_comp_color_request==1);
  g_comp_on=g_comp_mutable=1;g_comp_disable=0;g_comp_img[2]=(void*)0x300;g_own_imgs[2]=(void*)0x310;
 }
 g_comp_color_request=999;frame(0);assert(!step&&!g_comp_color_mode);
 puts("PASS: actual temporary transfer plus native RP44/RP50 RGB/gray calibration, exact SDK CI/clear/identity views/layout/dependency, cache once per acquired proxy/format, 360-frame bound, stop and generation/device/extent/context refusal");
 return 0;
}
