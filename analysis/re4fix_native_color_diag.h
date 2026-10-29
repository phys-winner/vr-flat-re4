// Temporary RGB shader draw inside the actual native pass, including its own MSAA resolve path.
#ifndef RE4FIX_NATIVE_COLOR_DIAG_H
#define RE4FIX_NATIVE_COLOR_DIAG_H
#include <pthread.h>

static volatile int g_native_color_request=-1;
static unsigned native_color_frames;
static int native_color_active;
static PFN_vkCmdBindPipeline native_color_bind;
static pthread_mutex_t native_color_bind_mutex=PTHREAD_MUTEX_INITIALIZER;
static struct {VkCommandBuffer command;VkPipeline pipeline;} native_color_bindings[64];
static unsigned native_color_binding_next;
static VkDevice native_color_device;
static unsigned native_color_width,native_color_height;
static VkPipelineLayout native_color_layout;
static struct {VkRenderPass pass;unsigned samples;VkPipeline pipeline;} native_color_pipelines[16];

static VKAPI_ATTR void VKAPI_CALL h_native_color_BindPipeline(VkCommandBuffer command,VkPipelineBindPoint point,VkPipeline pipeline) {
    if(!native_color_bind)return;
    native_color_bind(command,point,pipeline);
    if(point!=VK_PIPELINE_BIND_POINT_GRAPHICS||!command)return;
    pthread_mutex_lock(&native_color_bind_mutex);
    unsigned slot=64;
    for(unsigned i=0;i<64;i++)if(native_color_bindings[i].command==command){slot=i;break;}
    if(slot==64)slot=native_color_binding_next++%64;
    native_color_bindings[slot].command=command;native_color_bindings[slot].pipeline=pipeline;
    pthread_mutex_unlock(&native_color_bind_mutex);
}
static VkPipeline native_color_previous(VkCommandBuffer command) {
    VkPipeline pipeline=VK_NULL_HANDLE;
    pthread_mutex_lock(&native_color_bind_mutex);
    for(unsigned i=0;i<64;i++)if(native_color_bindings[i].command==command){pipeline=native_color_bindings[i].pipeline;break;}
    pthread_mutex_unlock(&native_color_bind_mutex);
    return pipeline;
}
static void* native_color_proc(const char* name,void* real) {
    if(!name||!real||strcmp(name,"vkCmdBindPipeline"))return real;
    if(real!=(void*)h_native_color_BindPipeline)native_color_bind=(PFN_vkCmdBindPipeline)real;
    return (void*)h_native_color_BindPipeline;
}

static VkPipeline native_color_pipeline(VkRenderPass pass,unsigned sample_count) {
    VkDevice device=(VkDevice)g_comp_dev;
    if(!comp_color_shader_functions())return VK_NULL_HANDLE;
    if(native_color_device&&(native_color_device!=device||native_color_width!=g_comp_w||native_color_height!=g_comp_h))return VK_NULL_HANDLE;
    native_color_device=device;native_color_width=g_comp_w;native_color_height=g_comp_h;
    unsigned slot=16;
    for(unsigned i=0;i<16;i++) {
        if(native_color_pipelines[i].pass==pass&&native_color_pipelines[i].samples==sample_count)return native_color_pipelines[i].pipeline;
        if(slot==16&&!native_color_pipelines[i].pass)slot=i;
    }
    if(slot==16)return VK_NULL_HANDLE;
    if(!native_color_layout) {
        VkPipelineLayoutCreateInfo info={VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        if(comp_color_shader_api.layout(device,&info,0,&native_color_layout)!=VK_SUCCESS||!native_color_layout)return VK_NULL_HANDLE;
    }
    VkShaderModule shaders[2]={0};VkShaderModuleCreateInfo module={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    module.codeSize=sizeof comp_color_vertex_spv;module.pCode=comp_color_vertex_spv;
    if(comp_color_shader_api.shader(device,&module,0,&shaders[0])!=VK_SUCCESS||!shaders[0])return VK_NULL_HANDLE;
    module.codeSize=sizeof comp_color_fragment_spv;module.pCode=comp_color_fragment_spv;
    if(comp_color_shader_api.shader(device,&module,0,&shaders[1])!=VK_SUCCESS||!shaders[1]){comp_color_shader_api.destroy_shader(device,shaders[0],0);return VK_NULL_HANDLE;}
    VkPipelineShaderStageCreateInfo stages[2]={{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
    stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=shaders[0];stages[0].pName="main";
    stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=shaders[1];stages[1].pName="main";
    VkPipelineVertexInputStateCreateInfo vertex={VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo assembly={VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport viewport={0,0,(float)g_comp_w,(float)g_comp_h,0,1};VkRect2D scissor={{0,0},{g_comp_w,g_comp_h}};
    VkPipelineViewportStateCreateInfo view={VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};view.viewportCount=view.scissorCount=1;view.pViewports=&viewport;view.pScissors=&scissor;
    VkPipelineRasterizationStateCreateInfo raster={VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.lineWidth=1;
    VkPipelineMultisampleStateCreateInfo samples={VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};samples.rasterizationSamples=(VkSampleCountFlagBits)sample_count;
    VkPipelineDepthStencilStateCreateInfo depth={VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};depth.depthCompareOp=VK_COMPARE_OP_ALWAYS;
    VkPipelineColorBlendAttachmentState blend={0};blend.colorWriteMask=15;
    VkPipelineColorBlendStateCreateInfo blending={VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blending.attachmentCount=1;blending.pAttachments=&blend;
    VkGraphicsPipelineCreateInfo info={VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};info.stageCount=2;info.pStages=stages;
    info.pVertexInputState=&vertex;info.pInputAssemblyState=&assembly;info.pViewportState=&view;info.pRasterizationState=&raster;
    info.pMultisampleState=&samples;info.pDepthStencilState=&depth;info.pColorBlendState=&blending;info.layout=native_color_layout;info.renderPass=pass;
    VkPipeline pipeline=VK_NULL_HANDLE;
    VkResult result=comp_color_shader_api.pipeline(device,VK_NULL_HANDLE,1,&info,0,&pipeline);
    comp_color_shader_api.destroy_shader(device,shaders[0],0);comp_color_shader_api.destroy_shader(device,shaders[1],0);
    if(result!=VK_SUCCESS||!pipeline)return VK_NULL_HANDLE;
    // Retain diagnostic pipelines until process exit: their latest submissions may still be pending.
    native_color_pipelines[slot].pass=pass;native_color_pipelines[slot].samples=sample_count;native_color_pipelines[slot].pipeline=pipeline;
    LOGF("NATIVE COLOR pipeline slot=%u rp=%p samples=%u subpass=0",slot,(void*)pass,sample_count);
    return pipeline;
}

// Immediately before the original vkCmdEndRenderPass; metadata describes native subpass0/color attachment0.
static void native_color_diag(void* command,int swapchain,void* render_pass,unsigned format,unsigned samples,unsigned subpasses,unsigned colors) {
    int request=__atomic_exchange_n(&g_native_color_request,-1,__ATOMIC_ACQ_REL);
    if(request>=0) {
        native_color_active=request==1;native_color_frames=native_color_active?360:0;
        LOGF("NATIVE COLOR active=%d matchingPasses=%u RGB/linearGray0.25",native_color_active,native_color_frames);
    }
    if(!native_color_active||!native_color_frames)return;
    if(!command||swapchain<0||!render_pass||format!=50||(samples!=1&&samples!=4)||subpasses!=1||colors!=1)return;
    if(!g_comp_on||g_comp_disable||!g_comp_dev||!g_comp_w||!g_comp_h)return;
    VkPipeline previous=native_color_previous((VkCommandBuffer)command);
    if(!previous)return;
    VkPipeline pipeline=native_color_pipeline((VkRenderPass)render_pass,samples);
    if(!pipeline){native_color_active=0;native_color_frames=0;LOGF("NATIVE COLOR unavailable pipeline/cache/context; stopped");return;}
    comp_color_shader_api.bind_pipeline((VkCommandBuffer)command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
    comp_color_shader_api.draw((VkCommandBuffer)command,3,1,0,0);
    // Native RHI caches its binding across passes; restore it without touching descriptors/dynamic state/push constants.
    comp_color_shader_api.bind_pipeline((VkCommandBuffer)command,VK_PIPELINE_BIND_POINT_GRAPHICS,previous);
    if(--native_color_frames==0){native_color_active=0;LOGF("NATIVE COLOR complete; native rendering restored");}
}
#endif
