// Reuse the actual embedded shaders and SDK driver fixtures of the owned color probe.
#define main owned_color_fixture_main
#include "test_comp_color_shader_diag.c"
#undef main
#include "../re4fix_native_color_diag.h"
#ifndef RE4_TEST_MUTEX_LOCKED
#define RE4_TEST_MUTEX_LOCKED(m) 0
#endif
static unsigned native_binds,native_draws,native_creates,native_sequence,native_expected_samples;
static VkCommandBuffer forwarded_command;
static VkPipeline forwarded_pipeline,native_expected_previous;
static VkPipelineBindPoint forwarded_point;
static VkRenderPass native_expected_pass;
static VkRenderPass created_passes[16];
static VkPipeline created_pipelines[16];
static void unlocked(void){assert(!RE4_TEST_MUTEX_LOCKED(native_color_bind_mutex));}
static void native_bind(VkCommandBuffer cmd,VkPipelineBindPoint point,VkPipeline pipeline){
 unlocked();native_binds++;forwarded_command=cmd;forwarded_point=point;forwarded_pipeline=pipeline;
}
static VkPipeline expected_pipeline(void){for(unsigned i=0;i<native_creates;i++)if(created_passes[i]==native_expected_pass)return created_pipelines[i];assert(0);return 0;}
static void raw_bind(VkCommandBuffer cmd,VkPipelineBindPoint point,VkPipeline pipeline){
 unlocked();assert(cmd==(VkCommandBuffer)command&&point==VK_PIPELINE_BIND_POINT_GRAPHICS);
 if(native_sequence==0){assert(pipeline==expected_pipeline());native_sequence=1;}
 else {assert(native_sequence==2&&pipeline==native_expected_previous);native_sequence=3;}
}
static void raw_draw(VkCommandBuffer cmd,uint32_t vertices,uint32_t instances,uint32_t first,uint32_t first_instance){
 unlocked();assert(native_sequence==1&&cmd==(VkCommandBuffer)command&&vertices==3&&instances==1&&!first&&!first_instance);native_sequence=2;native_draws++;
}
static VkResult native_pipeline_create(VkDevice dev,VkPipelineCache cache,uint32_t count,const VkGraphicsPipelineCreateInfo* ci,const VkAllocationCallbacks* allocator,VkPipeline* out){
 unlocked();assert(native_creates<16&&dev==(VkDevice)g_comp_dev&&!cache&&count==1&&!allocator);
 assert(ci->sType==VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO&&!ci->pNext&&!ci->flags&&ci->renderPass==native_expected_pass&&!ci->subpass);
 assert(ci->stageCount==2&&ci->pStages[0].stage==VK_SHADER_STAGE_VERTEX_BIT&&ci->pStages[1].stage==VK_SHADER_STAGE_FRAGMENT_BIT&&!strcmp(ci->pStages[0].pName,"main")&&!strcmp(ci->pStages[1].pName,"main"));
 assert(ci->pVertexInputState&&!ci->pVertexInputState->vertexBindingDescriptionCount&&!ci->pVertexInputState->vertexAttributeDescriptionCount);
 assert(ci->pInputAssemblyState->topology==VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST&&!ci->pInputAssemblyState->primitiveRestartEnable);
 assert(ci->pViewportState->viewportCount==1&&ci->pViewportState->scissorCount==1&&ci->pViewportState->pViewports->width==2400&&ci->pViewportState->pViewports->height==1080);
 assert(ci->pViewportState->pScissors->extent.width==2400&&ci->pViewportState->pScissors->extent.height==1080&&!ci->pDynamicState);
 assert(ci->pRasterizationState->cullMode==VK_CULL_MODE_NONE&&!ci->pRasterizationState->rasterizerDiscardEnable&&ci->pRasterizationState->lineWidth==1);
 assert(ci->pMultisampleState->rasterizationSamples==native_expected_samples&&!ci->pMultisampleState->sampleShadingEnable);
 const VkPipelineDepthStencilStateCreateInfo* depth=ci->pDepthStencilState;
 assert(depth&&depth->sType==VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO&&!depth->depthTestEnable&&!depth->depthWriteEnable&&!depth->depthBoundsTestEnable&&!depth->stencilTestEnable);
 assert(ci->pColorBlendState->attachmentCount==1&&!ci->pColorBlendState->pAttachments->blendEnable&&ci->pColorBlendState->pAttachments->colorWriteMask==15);
 assert(ci->layout==H(VkPipelineLayout,0x8000));
 if(pipeline_failure)return VK_ERROR_OUT_OF_DEVICE_MEMORY;
 *out=H(VkPipeline,0xd000+native_creates);created_passes[native_creates]=native_expected_pass;created_pipelines[native_creates]=*out;native_creates++;return VK_SUCCESS;
}
static void invoke(unsigned samples){native_sequence=0;native_expected_samples=samples;native_color_diag(command,1,(void*)native_expected_pass,50,samples,1,1);}
int main(void){
 native_expected_pass=H(VkRenderPass,0xe000);native_expected_previous=H(VkPipeline,0xf000);
 native_color_diag(command,1,(void*)native_expected_pass,50,4,1,1);
 assert(!lookups&&!images_created&&!passes_created&&!frames_created&&!shader_calls&&!draws&&!native_draws);
 PFN_vkCmdBindPipeline abi=h_native_color_BindPipeline;assert(abi);
 assert(native_color_proc(0,(void*)native_bind)==(void*)native_bind&&!native_color_proc("vkCmdBindPipeline",0));
 assert(native_color_proc("vkOther",(void*)native_bind)==(void*)native_bind);
 assert(native_color_proc("vkCmdBindPipeline",(void*)native_bind)==(void*)abi);
 assert(native_color_proc("vkCmdBindPipeline",(void*)abi)==(void*)abi&&native_color_bind==native_bind&&!native_binds);
 h_native_color_BindPipeline((VkCommandBuffer)command,VK_PIPELINE_BIND_POINT_COMPUTE,H(VkPipeline,0xf100));
 assert(native_binds==1&&forwarded_command==(VkCommandBuffer)command&&forwarded_point==VK_PIPELINE_BIND_POINT_COMPUTE&&forwarded_pipeline==H(VkPipeline,0xf100)&&!native_color_previous((VkCommandBuffer)command));
 h_native_color_BindPipeline((VkCommandBuffer)command,VK_PIPELINE_BIND_POINT_GRAPHICS,native_expected_previous);
 assert(native_binds==2&&native_color_previous((VkCommandBuffer)command)==native_expected_previous);
 h_native_color_BindPipeline((VkCommandBuffer)command,VK_PIPELINE_BIND_POINT_COMPUTE,H(VkPipeline,0xf101));
 assert(native_color_previous((VkCommandBuffer)command)==native_expected_previous);
 // Supply mock raw driver procedures through the same shared shader API used by production.
 assert(comp_color_shader_functions());comp_color_shader_api.pipeline=native_pipeline_create;
 comp_color_shader_api.bind_pipeline=raw_bind;comp_color_shader_api.draw=raw_draw;
 g_native_color_request=1;
 native_color_diag(command,-1,(void*)native_expected_pass,50,4,1,1);assert(native_color_frames==360&&!native_draws);
 native_color_diag(command,1,(void*)native_expected_pass,44,4,1,1);
 native_color_diag(command,1,(void*)native_expected_pass,50,2,1,1);
 native_color_diag(command,1,(void*)native_expected_pass,50,4,2,1);
 native_color_diag(command,1,(void*)native_expected_pass,50,4,1,2);
 native_color_diag(0,1,(void*)native_expected_pass,50,4,1,1);
 native_color_diag(command,1,0,50,4,1,1);
 native_color_diag((void*)0x999,1,(void*)native_expected_pass,50,4,1,1);
 for(unsigned failure=0;failure<5;failure++){
  if(failure==0)g_comp_on=0;if(failure==1)g_comp_disable=1;if(failure==2)g_comp_dev=0;if(failure==3)g_comp_w=0;if(failure==4)g_comp_h=0;
  invoke(4);assert(!native_sequence);
  g_comp_on=1;g_comp_disable=0;g_comp_dev=(void*)0x777;g_comp_w=2400;g_comp_h=1080;
 }
 assert(native_color_frames==360&&!native_draws&&!native_creates&&!images_created&&!passes_created&&!frames_created);
 for(unsigned i=0;i<360;i++){
  invoke(4);assert(native_sequence==3&&native_color_previous((VkCommandBuffer)command)==native_expected_previous);
 }
 assert(native_draws==360&&native_creates==1&&shader_calls==2&&shader_destroyed==2&&!native_color_active&&!native_color_frames);
 invoke(4);assert(!native_sequence&&native_draws==360);
 g_native_color_request=1;invoke(4);assert(native_sequence==3&&native_creates==1);
 g_native_color_request=0;invoke(4);assert(!native_sequence&&!native_color_active);
 // Actual depth-enabled native passes still receive an explicitly disabled depth/stencil pipeline state.
 native_expected_pass=H(VkRenderPass,0xe001);g_native_color_request=1;invoke(1);assert(native_sequence==3&&native_creates==2);
 for(unsigned failure=0;failure<3;failure++){
  g_native_color_request=1;
  if(failure==0)g_comp_dev=(void*)0x778;if(failure==1)g_comp_w++;if(failure==2)g_comp_h++;
  invoke(1);assert(!native_sequence&&!native_color_active&&!native_color_frames&&native_creates==2);
  g_comp_dev=(void*)0x777;g_comp_w=2400;g_comp_h=1080;
 }
 pipeline_failure=1;native_expected_pass=H(VkRenderPass,0xe002);g_native_color_request=1;invoke(4);
 assert(!native_sequence&&!native_color_active&&shader_calls==6&&shader_destroyed==6&&native_creates==2);pipeline_failure=0;
 for(unsigned i=2;i<16;i++){
  native_expected_pass=H(VkRenderPass,0xe000+i);g_native_color_request=1;invoke(i&1?1:4);assert(native_sequence==3);
 }
 assert(native_creates==16&&!images_created&&!passes_created&&!frames_created&&shader_calls==shader_destroyed);
 native_expected_pass=H(VkRenderPass,0xefff);g_native_color_request=1;invoke(4);assert(!native_sequence&&!native_color_active&&native_creates==16);
 // Bounded tracker refuses an evicted command rather than restoring a different command's binding.
 for(unsigned i=0;i<70;i++)h_native_color_BindPipeline(H(VkCommandBuffer,0x10000+i),VK_PIPELINE_BIND_POINT_GRAPHICS,H(VkPipeline,0x20000+i));
 assert(!native_color_previous((VkCommandBuffer)command));
 assert(native_color_previous(H(VkCommandBuffer,0x10000+69))==H(VkPipeline,0x20000+69));
 puts("PASS: actual native-pass RGB probe; exact SDK Pipeline/Bind ABI; nativeRP/subpass0+samples1/4 and disabled depth/stencil; no own images/framebuffers/renderpasses/resolves; explicit-only360matchingpass/stop/scope guards; exact native graphics binding restored after each draw; descriptor/dynamic/push state untouched;16pipeline/64command bounds; failure/device/extent/cache refusal and native GPU forwarding outside mutex");return 0;
}
