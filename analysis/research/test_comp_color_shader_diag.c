#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan_core.h>
#define COMP_MAX 8
#define H(type,n) ((type)(uintptr_t)(n))
static int g_comp_on=1,g_comp_disable,g_comp_mutable=1;
static unsigned g_comp_n=4,g_comp_w=2400,g_comp_h=1080;
static void *g_comp_dev=(void*)0x777,*g_phys_dev=(void*)0x778;
static void* g_comp_img[COMP_MAX]={(void*)0x100,(void*)0x200,(void*)0x300,(void*)0x400};
static void* g_own_imgs[COMP_MAX]={(void*)0x110,(void*)0x210,(void*)0x310,(void*)0x410};
static unsigned lookups,logs,images_created,views_created,passes_created,frames_created,shader_calls,shader_destroyed,pipeline_calls,draws,copies;
static unsigned expected_kind,expected_msaa,expected_index,expected_linear_msaa,expected_mutable_msaa,pipeline_failure;
static void* command=(void*)0x1234567812345678ULL;
static struct {VkImage image;unsigned samples,mutable,format;} image_data[5];
static struct {VkImageView view;VkImage image;} view_data[9];
static struct {VkFramebuffer frame;VkImage target;unsigned msaa,linear_msaa,mutable_msaa;} frame_data[20];
static VkRenderPass passes[2];
static VkPipeline pipelines[2];
static char events[32];static unsigned event_count;
#define LOGF(...) (++logs)
static void event(char letter){assert(event_count+1<sizeof events);events[event_count++]=letter;events[event_count]=0;}
static unsigned image_index(VkImage image){for(unsigned i=0;i<images_created;i++)if(image_data[i].image==image)return i;assert(0);return 0;}
static VkImage view_image(VkImageView view){for(unsigned i=0;i<views_created;i++)if(view_data[i].view==view)return view_data[i].image;assert(0);return 0;}
static int proxy_index(VkImage image){for(unsigned i=0;i<g_comp_n;i++)if((VkImage)g_comp_img[i]==image)return (int)i;return -1;}
static void comp_barrier(void* cmd,void* img,unsigned old,unsigned next,unsigned src,unsigned dst,unsigned ss,unsigned ds){
 assert(cmd==command);VkImage image=(VkImage)img;
 if(old==VK_IMAGE_LAYOUT_UNDEFINED){assert(proxy_index(image)<0&&image_data[image_index(image)].samples==1&&next==2&&!src&&dst==0x100&&ss==1&&ds==0x400);event('Z');}
 else if(old==2&&next==6){assert(proxy_index(image)<0&&src==0x100&&dst==0x800&&ss==0x400&&ds==0x1000);event('S');}
 else if(old==2&&next==7){assert(proxy_index(image)==(int)expected_index&&src==0x100&&dst==0x1000&&ss==0x400&&ds==0x1000);event('T');}
 else if(old==6&&next==2){assert(proxy_index(image)<0&&src==0x800&&dst==0x100&&ss==0x1000&&ds==0x400);event('U');}
 else if(old==7&&next==2){assert(proxy_index(image)==(int)expected_index&&src==0x1000&&dst==0x180&&ss==0x1000&&ds==0x400);event('V');}
 else assert(0);
}
static VkResult create_image(VkDevice dev,const VkImageCreateInfo* ci,const VkAllocationCallbacks* allocator,VkImage* out){
 assert(dev==(VkDevice)g_comp_dev&&!allocator&&images_created<5&&ci->sType==VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
 assert(ci->imageType==VK_IMAGE_TYPE_2D&&ci->format==(expected_linear_msaa?VK_FORMAT_B8G8R8A8_UNORM:VK_FORMAT_B8G8R8A8_SRGB)&&ci->extent.width==2400&&ci->extent.height==1080&&ci->extent.depth==1);
 assert(ci->mipLevels==1&&ci->arrayLayers==1&&(ci->samples==VK_SAMPLE_COUNT_1_BIT||ci->samples==VK_SAMPLE_COUNT_4_BIT));
 assert(ci->tiling==VK_IMAGE_TILING_OPTIMAL&&ci->usage==(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
 assert(ci->sharingMode==VK_SHARING_MODE_EXCLUSIVE&&!ci->queueFamilyIndexCount&&!ci->pQueueFamilyIndices&&ci->initialLayout==VK_IMAGE_LAYOUT_UNDEFINED);
 assert(ci->flags==0||ci->flags==VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT);
 if(ci->flags){const VkImageFormatListCreateInfo* list=ci->pNext;assert(list&&list->sType==VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO&&!list->pNext&&list->viewFormatCount==2&&list->pViewFormats[0]==44&&list->pViewFormats[1]==50&&(ci->samples==1||expected_mutable_msaa));}
 else assert(!ci->pNext);
 *out=H(VkImage,0x3000+images_created);image_data[images_created].image=*out;image_data[images_created].samples=ci->samples;image_data[images_created].mutable=!!ci->flags;image_data[images_created].format=ci->format;images_created++;return VK_SUCCESS;
}
static void requirements(VkDevice dev,VkImage image,VkMemoryRequirements* out){assert(dev==(VkDevice)g_comp_dev);image_index(image);*out=(VkMemoryRequirements){0x100000,4096,2};}
static void properties(VkPhysicalDevice dev,VkPhysicalDeviceMemoryProperties* out){assert(dev==(VkPhysicalDevice)g_phys_dev);memset(out,0,sizeof *out);out->memoryTypeCount=2;out->memoryTypes[1].propertyFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;}
static VkResult allocate(VkDevice dev,const VkMemoryAllocateInfo* ci,const VkAllocationCallbacks* allocator,VkDeviceMemory* out){assert(dev==(VkDevice)g_comp_dev&&!allocator&&ci->sType==VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO&&!ci->pNext&&ci->allocationSize==0x100000&&ci->memoryTypeIndex==1);*out=H(VkDeviceMemory,0x3500+images_created);return VK_SUCCESS;}
static VkResult bind_memory(VkDevice dev,VkImage image,VkDeviceMemory memory,VkDeviceSize offset){assert(dev==(VkDevice)g_comp_dev&&memory&&!offset);image_index(image);return VK_SUCCESS;}
static VkResult create_view(VkDevice dev,const VkImageViewCreateInfo* ci,const VkAllocationCallbacks* allocator,VkImageView* out){
 assert(dev==(VkDevice)g_comp_dev&&!allocator&&views_created<9&&ci->sType==VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO&&!ci->pNext&&!ci->flags);
 assert(ci->viewType==VK_IMAGE_VIEW_TYPE_2D&&ci->format==50&&!ci->components.r&&!ci->components.g&&!ci->components.b&&!ci->components.a);
 assert(ci->subresourceRange.aspectMask==1&&!ci->subresourceRange.baseMipLevel&&ci->subresourceRange.levelCount==1&&!ci->subresourceRange.baseArrayLayer&&ci->subresourceRange.layerCount==1);
 if(proxy_index(ci->image)<0)image_index(ci->image);
 *out=H(VkImageView,0x4000+views_created);view_data[views_created].view=*out;view_data[views_created].image=ci->image;views_created++;return VK_SUCCESS;
}
static VkResult create_pass(VkDevice dev,const VkRenderPassCreateInfo* ci,const VkAllocationCallbacks* allocator,VkRenderPass* out){
 assert(dev==(VkDevice)g_comp_dev&&!allocator&&ci->sType==VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO&&!ci->pNext&&!ci->flags);
 unsigned msaa=ci->attachmentCount-1;assert(msaa<=1&&msaa==expected_msaa&&ci->subpassCount==1&&ci->dependencyCount==1);
 for(unsigned i=0;i<ci->attachmentCount;i++){const VkAttachmentDescription* a=&ci->pAttachments[i];assert(a->format==50&&a->samples==((msaa&&!i)?4:1)&&a->loadOp==VK_ATTACHMENT_LOAD_OP_DONT_CARE&&a->storeOp==((msaa&&!i)?VK_ATTACHMENT_STORE_OP_DONT_CARE:VK_ATTACHMENT_STORE_OP_STORE));assert(a->initialLayout==((msaa&&!i)?VK_IMAGE_LAYOUT_UNDEFINED:VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)&&a->finalLayout==2);}
 const VkSubpassDescription* sub=ci->pSubpasses;assert(sub->pipelineBindPoint==VK_PIPELINE_BIND_POINT_GRAPHICS&&sub->colorAttachmentCount==1&&!sub->pColorAttachments->attachment&&sub->pColorAttachments->layout==2);
 if(msaa)assert(sub->pResolveAttachments&&sub->pResolveAttachments->attachment==1&&sub->pResolveAttachments->layout==2);else assert(!sub->pResolveAttachments);
 const VkSubpassDependency* dep=ci->pDependencies;assert(dep->srcSubpass==VK_SUBPASS_EXTERNAL&&!dep->dstSubpass&&dep->srcStageMask==0x400&&dep->dstStageMask==0x400&&dep->srcAccessMask==0x100&&dep->dstAccessMask==0x100);
 *out=passes[msaa]=H(VkRenderPass,0x5000+msaa);passes_created++;return VK_SUCCESS;
}
static VkResult create_framebuffer(VkDevice dev,const VkFramebufferCreateInfo* ci,const VkAllocationCallbacks* allocator,VkFramebuffer* out){
 assert(dev==(VkDevice)g_comp_dev&&!allocator&&frames_created<20&&ci->sType==VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO&&!ci->pNext&&!ci->flags);
 assert(ci->renderPass==passes[expected_msaa]&&ci->attachmentCount==1+expected_msaa&&ci->width==2400&&ci->height==1080&&ci->layers==1);
 VkImage target=view_image(ci->pAttachments[expected_msaa]);
 if(expected_kind)assert(proxy_index(target)<0&&image_data[image_index(target)].samples==1&&image_data[image_index(target)].mutable==(expected_kind==2));
 else assert(target==(VkImage)g_comp_img[expected_index]);
 if(expected_msaa){unsigned i=image_index(view_image(ci->pAttachments[0]));assert(image_data[i].samples==4&&image_data[i].format==(expected_linear_msaa?44:50)&&image_data[i].mutable==expected_mutable_msaa);}
 *out=H(VkFramebuffer,0x6000+frames_created);frame_data[frames_created].frame=*out;frame_data[frames_created].target=target;frame_data[frames_created].msaa=expected_msaa;frame_data[frames_created].linear_msaa=expected_linear_msaa;frame_data[frames_created].mutable_msaa=expected_mutable_msaa;frames_created++;return VK_SUCCESS;
}
static VkResult shader(VkDevice dev,const VkShaderModuleCreateInfo* ci,const VkAllocationCallbacks* allocator,VkShaderModule* out){assert(dev==(VkDevice)g_comp_dev&&!allocator&&ci->sType==VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO&&!ci->pNext&&!ci->flags&&ci->codeSize>100&&ci->pCode&&ci->pCode[0]==0x07230203);*out=H(VkShaderModule,0x7000+shader_calls++);return VK_SUCCESS;}
static void destroy_shader(VkDevice dev,VkShaderModule module,const VkAllocationCallbacks* allocator){assert(dev==(VkDevice)g_comp_dev&&module&&!allocator);shader_destroyed++;}
static VkResult layout(VkDevice dev,const VkPipelineLayoutCreateInfo* ci,const VkAllocationCallbacks* allocator,VkPipelineLayout* out){assert(dev==(VkDevice)g_comp_dev&&!allocator&&ci->sType==VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO&&!ci->pNext&&!ci->flags&&!ci->setLayoutCount&&!ci->pSetLayouts&&!ci->pushConstantRangeCount&&!ci->pPushConstantRanges);*out=H(VkPipelineLayout,0x8000);return VK_SUCCESS;}
static VkResult pipeline(VkDevice dev,VkPipelineCache cache,uint32_t count,const VkGraphicsPipelineCreateInfo* ci,const VkAllocationCallbacks* allocator,VkPipeline* out){
 assert(dev==(VkDevice)g_comp_dev&&!cache&&count==1&&!allocator&&ci->sType==VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO&&!ci->pNext&&!ci->flags);
 assert(ci->stageCount==2&&ci->pStages[0].stage==VK_SHADER_STAGE_VERTEX_BIT&&ci->pStages[1].stage==VK_SHADER_STAGE_FRAGMENT_BIT&&!strcmp(ci->pStages[0].pName,"main")&&!strcmp(ci->pStages[1].pName,"main"));
 assert(!ci->pVertexInputState->vertexBindingDescriptionCount&&!ci->pVertexInputState->vertexAttributeDescriptionCount&&ci->pInputAssemblyState->topology==VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
 assert(ci->pViewportState->viewportCount==1&&ci->pViewportState->scissorCount==1&&ci->pViewportState->pViewports->width==2400&&ci->pViewportState->pViewports->height==1080&&ci->pViewportState->pViewports->minDepth==0&&ci->pViewportState->pViewports->maxDepth==1);
 assert(ci->pViewportState->pScissors->extent.width==2400&&ci->pViewportState->pScissors->extent.height==1080);
 assert(ci->pRasterizationState->polygonMode==VK_POLYGON_MODE_FILL&&ci->pRasterizationState->cullMode==VK_CULL_MODE_NONE&&ci->pRasterizationState->lineWidth==1&&!ci->pRasterizationState->rasterizerDiscardEnable);
 assert(ci->pMultisampleState->rasterizationSamples==(expected_msaa?4:1)&&!ci->pMultisampleState->sampleShadingEnable&&!ci->pMultisampleState->alphaToCoverageEnable);
 assert(ci->pColorBlendState->attachmentCount==1&&!ci->pColorBlendState->pAttachments->blendEnable&&ci->pColorBlendState->pAttachments->colorWriteMask==15);
 assert(!ci->pDepthStencilState&&!ci->pTessellationState&&!ci->pDynamicState&&ci->layout==H(VkPipelineLayout,0x8000)&&ci->renderPass==passes[expected_msaa]&&!ci->subpass);
 pipeline_calls++;if(pipeline_failure)return VK_ERROR_OUT_OF_DEVICE_MEMORY;
 *out=pipelines[expected_msaa]=H(VkPipeline,0x9000+expected_msaa);return VK_SUCCESS;
}
static void begin(VkCommandBuffer cmd,const VkRenderPassBeginInfo* ci,VkSubpassContents contents){
 assert(cmd==(VkCommandBuffer)command&&contents==VK_SUBPASS_CONTENTS_INLINE&&ci->sType==VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO&&!ci->pNext&&ci->renderPass==passes[expected_msaa]);
 assert(ci->renderArea.extent.width==2400&&ci->renderArea.extent.height==1080&&!ci->renderArea.offset.x&&!ci->renderArea.offset.y&&!ci->clearValueCount&&!ci->pClearValues);
 unsigned i=0;for(;i<frames_created;i++)if(frame_data[i].frame==ci->framebuffer)break;assert(i<frames_created&&frame_data[i].msaa==expected_msaa&&frame_data[i].linear_msaa==expected_linear_msaa&&frame_data[i].mutable_msaa==expected_mutable_msaa);
 if(expected_kind)assert(proxy_index(frame_data[i].target)<0&&image_data[image_index(frame_data[i].target)].mutable==(expected_kind==2));else assert(frame_data[i].target==(VkImage)g_comp_img[expected_index]);event('B');
}
static void bind_pipeline(VkCommandBuffer cmd,VkPipelineBindPoint point,VkPipeline pipe){assert(cmd==(VkCommandBuffer)command&&point==VK_PIPELINE_BIND_POINT_GRAPHICS&&pipe==pipelines[expected_msaa]);event('P');}
static void draw(VkCommandBuffer cmd,uint32_t vertices,uint32_t instances,uint32_t first,uint32_t first_instance){assert(cmd==(VkCommandBuffer)command&&vertices==3&&instances==1&&!first&&!first_instance);draws++;event('D');}
static void end(VkCommandBuffer cmd){assert(cmd==(VkCommandBuffer)command);event('E');}
static void copy(VkCommandBuffer cmd,VkImage src,VkImageLayout sl,VkImage dst,VkImageLayout dl,uint32_t count,const VkImageCopy* region){
 assert(cmd==(VkCommandBuffer)command&&sl==6&&dst==(VkImage)g_comp_img[expected_index]&&dl==7&&count==1&&region);
 assert(image_data[image_index(src)].samples==1&&image_data[image_index(src)].mutable==(expected_kind==2));
 assert(region->srcSubresource.aspectMask==1&&!region->srcSubresource.mipLevel&&!region->srcSubresource.baseArrayLayer&&region->srcSubresource.layerCount==1&&!memcmp(&region->srcSubresource,&region->dstSubresource,sizeof region->srcSubresource));
 assert(!region->srcOffset.x&&!region->srcOffset.y&&!region->srcOffset.z&&!region->dstOffset.x&&!region->dstOffset.y&&!region->dstOffset.z&&region->extent.width==2400&&region->extent.height==1080&&region->extent.depth==1);copies++;event('C');
}
static void* comp_fn(const char* name){
 lookups++;
 #define PROC(n,f) if(!strcmp(name,n))return (void*)f
 PROC("vkCreateRenderPass",create_pass);PROC("vkCreateImageView",create_view);PROC("vkCreateFramebuffer",create_framebuffer);
 PROC("vkCmdBeginRenderPass",begin);PROC("vkCmdEndRenderPass",end);PROC("vkCreateImage",create_image);
 PROC("vkGetImageMemoryRequirements",requirements);PROC("vkGetPhysicalDeviceMemoryProperties",properties);PROC("vkAllocateMemory",allocate);PROC("vkBindImageMemory",bind_memory);
 PROC("vkCreateShaderModule",shader);PROC("vkDestroyShaderModule",destroy_shader);PROC("vkCreatePipelineLayout",layout);PROC("vkCreateGraphicsPipelines",pipeline);
 PROC("vkCmdBindPipeline",bind_pipeline);PROC("vkCmdDraw",draw);PROC("vkCmdCopyImage",copy);
 #undef PROC
 assert(0);return 0;
}
#include "../re4fix_comp_color_diag.h"
static void frame(unsigned mode,unsigned index){expected_kind=mode>=20?0:(mode-14)/2;expected_msaa=mode>=20?1:(mode-14)%2;expected_linear_msaa=mode==20;expected_mutable_msaa=mode>=20;expected_index=index;event_count=0;events[0]=0;comp_color_diag(command,index);}
int main(void){
 comp_color_diag(command,0);assert(!lookups&&!images_created&&!draws&&!copies);
 for(unsigned mode=14;mode<=21;mode++){
  g_comp_color_request=(int)mode;unsigned before=draws,previous_images=images_created;
  for(unsigned i=0;i<360;i++){
   frame(mode,i%4);
   if(mode<16||mode>=20)assert(!strcmp(events,"BPDE"));
   else assert(!strcmp(events,i==0?"ZBPDESTCUV":"BPDESTCUV")||!strcmp(events,"BPDESTCUV"));
  }
  assert(draws==before+360&&!g_comp_color_mode&&!g_comp_color_frames);
  frame(mode,0);assert(!event_count&&images_created>=previous_images);
 }
 assert(images_created==5&&views_created==9&&passes_created==2&&frames_created==20&&shader_calls==4&&shader_destroyed==4&&pipeline_calls==2);
 assert(draws==2880&&copies==1440&&lookups==17);
 // Common own targets are synchronized and reused for every acquired index; no per-frame allocation.
 g_comp_color_request=18;frame(18,0);unsigned creations=images_created;g_comp_color_request=0;frame(18,1);assert(!event_count&&!g_comp_color_mode&&images_created==creations);
 for(unsigned failure=0;failure<4;failure++){
  g_comp_color_request=14;
  if(failure==0)g_comp_dev=(void*)0x999;if(failure==1)g_comp_w=2401;if(failure==2)g_comp_img[0]=(void*)0x999;if(failure==3)g_phys_dev=0;
  frame(14,0);assert(!event_count&&!g_comp_color_mode&&!g_comp_color_frames);
  g_comp_dev=(void*)0x777;g_comp_w=2400;g_comp_img[0]=(void*)0x100;g_phys_dev=(void*)0x778;
 }
 // A failed pipeline creation records no GPU commands and releases both temporary shader modules.
 comp_color_shader_pipelines[0]=0;pipeline_failure=1;g_comp_color_request=14;frame(14,0);
 assert(!event_count&&!g_comp_color_mode&&!g_comp_color_frames&&shader_calls==6&&shader_destroyed==6);
 puts("PASS: explicit-only descriptor-free shader RGB/gray probe, exact native SDK image/memory/identity-view/render-pass/pipeline CI,1x+MSAA4 resolve for proxy44/nonmutable50/mutable50 targets plus mutable44/50 MSAA attachments, full source/destination copy dependencies, bounded5 owned images/20framebuffers/2pipelines,360frames+stop, no per-frame allocation, device/extent/proxy/physical-device/error refusal");return 0;
}
