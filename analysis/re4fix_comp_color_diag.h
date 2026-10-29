// Temporary acquired-proxy calibration; remove after the target GPU's color proof.
#ifndef RE4FIX_COMP_COLOR_DIAG_H
#define RE4FIX_COMP_COLOR_DIAG_H
#include <vulkan/vulkan_core.h>
static volatile int g_comp_color_request=-1; // 0=stop, 1..5=transfer, 6..13=clear RP44/50, 14..19=shader 1x/4x, 20/21=mutable44/50 MSAA shader attachment.
static int g_comp_color_mode,g_comp_color_frames;
static void (*comp_color_clear)(void*,void*,unsigned,const float*,unsigned,const void*);
static PFN_vkCreateRenderPass comp_color_create_pass;
static PFN_vkCreateImageView comp_color_create_view;
static PFN_vkCreateFramebuffer comp_color_create_framebuffer;
static PFN_vkCmdBeginRenderPass comp_color_begin;
static PFN_vkCmdEndRenderPass comp_color_end;
static VkRenderPass comp_color_pass[2];
static VkDevice comp_color_device;
static unsigned comp_color_width,comp_color_height;
static struct {VkImage image;VkImageView view;VkFramebuffer framebuffer;} comp_color_targets[2][COMP_MAX];

static int comp_color_render_functions(void) {
    if(!comp_color_create_pass) {
        comp_color_create_pass=(PFN_vkCreateRenderPass)comp_fn("vkCreateRenderPass");
        comp_color_create_view=(PFN_vkCreateImageView)comp_fn("vkCreateImageView");
        comp_color_create_framebuffer=(PFN_vkCreateFramebuffer)comp_fn("vkCreateFramebuffer");
        comp_color_begin=(PFN_vkCmdBeginRenderPass)comp_fn("vkCmdBeginRenderPass");
        comp_color_end=(PFN_vkCmdEndRenderPass)comp_fn("vkCmdEndRenderPass");
    }
    return comp_color_create_pass&&comp_color_create_view&&comp_color_create_framebuffer&&comp_color_begin&&comp_color_end;
}

static int comp_color_render(void* cmd,unsigned index,unsigned srgb,const float color[4]) {
    VkDevice device=(VkDevice)g_comp_dev;VkImage image=(VkImage)g_comp_img[index];
    if(!device||!g_comp_w||!g_comp_h||srgb>1)return 0;
    if(!comp_color_render_functions())return 0;
    if(comp_color_device&&(comp_color_device!=device||comp_color_width!=g_comp_w||comp_color_height!=g_comp_h))return 0;
    comp_color_device=device;comp_color_width=g_comp_w;comp_color_height=g_comp_h;
    VkFormat format=srgb?VK_FORMAT_B8G8R8A8_SRGB:VK_FORMAT_B8G8R8A8_UNORM;
    if(!comp_color_pass[srgb]) {
        VkAttachmentDescription attachment={0};attachment.format=format;attachment.samples=VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout=attachment.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference reference={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass={0};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount=1;subpass.pColorAttachments=&reference;
        VkSubpassDependency dependency={VK_SUBPASS_EXTERNAL,0,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,0};
        VkRenderPassCreateInfo info={VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};info.attachmentCount=1;info.pAttachments=&attachment;
        info.subpassCount=1;info.pSubpasses=&subpass;info.dependencyCount=1;info.pDependencies=&dependency;
        if(comp_color_create_pass(device,&info,0,&comp_color_pass[srgb])!=VK_SUCCESS||!comp_color_pass[srgb])return 0;
    }
    if(comp_color_targets[srgb][index].image&&comp_color_targets[srgb][index].image!=image)return 0;
    comp_color_targets[srgb][index].image=image;
    if(!comp_color_targets[srgb][index].view) {
        VkImageViewCreateInfo info={VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};info.image=image;info.viewType=VK_IMAGE_VIEW_TYPE_2D;info.format=format;
        info.subresourceRange=(VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        if(comp_color_create_view(device,&info,0,&comp_color_targets[srgb][index].view)!=VK_SUCCESS||!comp_color_targets[srgb][index].view)return 0;
    }
    if(!comp_color_targets[srgb][index].framebuffer) {
        VkFramebufferCreateInfo info={VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};info.renderPass=comp_color_pass[srgb];
        info.attachmentCount=1;info.pAttachments=&comp_color_targets[srgb][index].view;info.width=g_comp_w;info.height=g_comp_h;info.layers=1;
        if(comp_color_create_framebuffer(device,&info,0,&comp_color_targets[srgb][index].framebuffer)!=VK_SUCCESS||!comp_color_targets[srgb][index].framebuffer)return 0;
    }
    VkClearValue clear={0};memcpy(clear.color.float32,color,sizeof clear.color.float32);
    VkRenderPassBeginInfo begin={VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};begin.renderPass=comp_color_pass[srgb];
    begin.framebuffer=comp_color_targets[srgb][index].framebuffer;begin.renderArea.extent=(VkExtent2D){g_comp_w,g_comp_h};
    begin.clearValueCount=1;begin.pClearValues=&clear;
    comp_color_begin((VkCommandBuffer)cmd,&begin,VK_SUBPASS_CONTENTS_INLINE);comp_color_end((VkCommandBuffer)cmd);return 1;
}

// SPIR-V is generated from the retained GLSL in research/check_comp_color_shader.py.
// COMP_COLOR_SPIRV_BEGIN
static const uint32_t comp_color_vertex_spv[]={
    0x07230203,0x00010000,0x000d000a,0x0000002f,0x00000000,0x00020011,0x00000001,0x0006000b,
    0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
    0x0008000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x0000000c,0x00000018,0x00000020,
    0x00040047,0x0000000c,0x0000000b,0x0000002a,0x00040047,0x00000018,0x0000001e,0x00000000,
    0x00050048,0x0000001e,0x00000000,0x0000000b,0x00000000,0x00050048,0x0000001e,0x00000001,
    0x0000000b,0x00000001,0x00050048,0x0000001e,0x00000002,0x0000000b,0x00000003,0x00050048,
    0x0000001e,0x00000003,0x0000000b,0x00000004,0x00030047,0x0000001e,0x00000002,0x00020013,
    0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,
    0x00000007,0x00000006,0x00000002,0x00040015,0x0000000a,0x00000020,0x00000001,0x00040020,
    0x0000000b,0x00000001,0x0000000a,0x0004003b,0x0000000b,0x0000000c,0x00000001,0x0004002b,
    0x0000000a,0x0000000e,0x00000001,0x0004002b,0x0000000a,0x00000010,0x00000002,0x00040020,
    0x00000017,0x00000003,0x00000007,0x0004003b,0x00000017,0x00000018,0x00000003,0x00040017,
    0x0000001a,0x00000006,0x00000004,0x00040015,0x0000001b,0x00000020,0x00000000,0x0004002b,
    0x0000001b,0x0000001c,0x00000001,0x0004001c,0x0000001d,0x00000006,0x0000001c,0x0006001e,
    0x0000001e,0x0000001a,0x00000006,0x0000001d,0x0000001d,0x00040020,0x0000001f,0x00000003,
    0x0000001e,0x0004003b,0x0000001f,0x00000020,0x00000003,0x0004002b,0x0000000a,0x00000021,
    0x00000000,0x0004002b,0x00000006,0x00000023,0x40000000,0x0004002b,0x00000006,0x00000025,
    0x3f800000,0x0004002b,0x00000006,0x00000028,0x00000000,0x00040020,0x0000002c,0x00000003,
    0x0000001a,0x0005002c,0x00000007,0x0000002e,0x00000025,0x00000025,0x00050036,0x00000002,
    0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003d,0x0000000a,0x0000000d,
    0x0000000c,0x000500c4,0x0000000a,0x0000000f,0x0000000d,0x0000000e,0x000500c7,0x0000000a,
    0x00000011,0x0000000f,0x00000010,0x0004006f,0x00000006,0x00000012,0x00000011,0x000500c7,
    0x0000000a,0x00000014,0x0000000d,0x00000010,0x0004006f,0x00000006,0x00000015,0x00000014,
    0x00050050,0x00000007,0x00000016,0x00000012,0x00000015,0x0003003e,0x00000018,0x00000016,
    0x0005008e,0x00000007,0x00000024,0x00000016,0x00000023,0x00050083,0x00000007,0x00000027,
    0x00000024,0x0000002e,0x00050051,0x00000006,0x00000029,0x00000027,0x00000000,0x00050051,
    0x00000006,0x0000002a,0x00000027,0x00000001,0x00070050,0x0000001a,0x0000002b,0x00000029,
    0x0000002a,0x00000028,0x00000025,0x00050041,0x0000002c,0x0000002d,0x00000020,0x00000021,
    0x0003003e,0x0000002d,0x0000002b,0x000100fd,0x00010038,
};
static const uint32_t comp_color_fragment_spv[]={
    0x07230203,0x00010000,0x000d000a,0x00000033,0x00000000,0x00020011,0x00000001,0x0006000b,
    0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
    0x0007000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000c,0x00030010,
    0x00000004,0x00000007,0x00040047,0x00000009,0x0000001e,0x00000000,0x00040047,0x0000000c,
    0x0000001e,0x00000000,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,
    0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040020,0x00000008,
    0x00000003,0x00000007,0x0004003b,0x00000008,0x00000009,0x00000003,0x00040017,0x0000000a,
    0x00000006,0x00000002,0x00040020,0x0000000b,0x00000001,0x0000000a,0x0004003b,0x0000000b,
    0x0000000c,0x00000001,0x00040015,0x0000000d,0x00000020,0x00000000,0x0004002b,0x0000000d,
    0x0000000e,0x00000000,0x00040020,0x0000000f,0x00000001,0x00000006,0x0004002b,0x00000006,
    0x00000012,0x3e800000,0x00020014,0x00000013,0x0004002b,0x00000006,0x00000019,0x3f800000,
    0x0004002b,0x00000006,0x0000001a,0x00000000,0x0007002c,0x00000007,0x0000001b,0x00000019,
    0x0000001a,0x0000001a,0x00000019,0x0004002b,0x00000006,0x0000001f,0x3f000000,0x0007002c,
    0x00000007,0x00000024,0x0000001a,0x00000019,0x0000001a,0x00000019,0x0004002b,0x00000006,
    0x00000028,0x3f400000,0x0007002c,0x00000007,0x0000002a,0x0000001a,0x0000001a,0x00000019,
    0x00000019,0x0007002c,0x00000007,0x0000002b,0x00000012,0x00000012,0x00000012,0x00000019,
    0x00040017,0x0000002c,0x00000013,0x00000004,0x00050036,0x00000002,0x00000004,0x00000000,
    0x00000003,0x000200f8,0x00000005,0x00050041,0x0000000f,0x00000010,0x0000000c,0x0000000e,
    0x0004003d,0x00000006,0x00000011,0x00000010,0x000500b8,0x00000013,0x00000014,0x00000011,
    0x00000012,0x000300f7,0x00000018,0x00000000,0x000400fa,0x00000014,0x00000017,0x0000001c,
    0x000200f8,0x00000017,0x000200f9,0x00000018,0x000200f8,0x0000001c,0x000500b8,0x00000013,
    0x00000020,0x00000011,0x0000001f,0x000300f7,0x00000023,0x00000000,0x000400fa,0x00000020,
    0x00000022,0x00000025,0x000200f8,0x00000022,0x000200f9,0x00000023,0x000200f8,0x00000025,
    0x000500b8,0x00000013,0x00000029,0x00000011,0x00000028,0x00070050,0x0000002c,0x0000002d,
    0x00000029,0x00000029,0x00000029,0x00000029,0x000600a9,0x00000007,0x0000002e,0x0000002d,
    0x0000002a,0x0000002b,0x000200f9,0x00000023,0x000200f8,0x00000023,0x000700f5,0x00000007,
    0x00000031,0x00000024,0x00000022,0x0000002e,0x00000025,0x000200f9,0x00000018,0x000200f8,
    0x00000018,0x000700f5,0x00000007,0x00000032,0x0000001b,0x00000017,0x00000031,0x00000023,
    0x0003003e,0x00000009,0x00000032,0x000100fd,0x00010038,
};
// COMP_COLOR_SPIRV_END

static struct {
    PFN_vkCreateImage image;
    PFN_vkGetImageMemoryRequirements requirements;
    PFN_vkGetPhysicalDeviceMemoryProperties properties;
    PFN_vkAllocateMemory allocate;
    PFN_vkBindImageMemory bind_memory;
    PFN_vkCreateShaderModule shader;
    PFN_vkDestroyShaderModule destroy_shader;
    PFN_vkCreatePipelineLayout layout;
    PFN_vkCreateGraphicsPipelines pipeline;
    PFN_vkCmdBindPipeline bind_pipeline;
    PFN_vkCmdDraw draw;
    PFN_vkCmdCopyImage copy;
} comp_color_shader_api;
// Keep diagnostic targets until process exit: stopping a mode does not retire its pending GPU submissions.
static struct comp_color_shader_target {VkImage image;VkDeviceMemory memory;VkImageView view;int initialized;} comp_color_shader_owned[5];
static VkImage comp_color_shader_proxies[COMP_MAX];
static VkImageView comp_color_shader_proxy_views[COMP_MAX];
static VkFramebuffer comp_color_shader_frames[5][2][COMP_MAX];
static VkRenderPass comp_color_shader_passes[2];
static VkPipeline comp_color_shader_pipelines[2];
static VkPipelineLayout comp_color_shader_layout;
static int comp_color_shader_loaded;

static int comp_color_shader_functions(void) {
    if(!comp_color_shader_loaded) {
        comp_color_shader_loaded=1;
        #define COMP_COLOR_LOAD(member,type,name) comp_color_shader_api.member=(type)comp_fn(name)
        COMP_COLOR_LOAD(image,PFN_vkCreateImage,"vkCreateImage");
        COMP_COLOR_LOAD(requirements,PFN_vkGetImageMemoryRequirements,"vkGetImageMemoryRequirements");
        COMP_COLOR_LOAD(properties,PFN_vkGetPhysicalDeviceMemoryProperties,"vkGetPhysicalDeviceMemoryProperties");
        COMP_COLOR_LOAD(allocate,PFN_vkAllocateMemory,"vkAllocateMemory");
        COMP_COLOR_LOAD(bind_memory,PFN_vkBindImageMemory,"vkBindImageMemory");
        COMP_COLOR_LOAD(shader,PFN_vkCreateShaderModule,"vkCreateShaderModule");
        COMP_COLOR_LOAD(destroy_shader,PFN_vkDestroyShaderModule,"vkDestroyShaderModule");
        COMP_COLOR_LOAD(layout,PFN_vkCreatePipelineLayout,"vkCreatePipelineLayout");
        COMP_COLOR_LOAD(pipeline,PFN_vkCreateGraphicsPipelines,"vkCreateGraphicsPipelines");
        COMP_COLOR_LOAD(bind_pipeline,PFN_vkCmdBindPipeline,"vkCmdBindPipeline");
        COMP_COLOR_LOAD(draw,PFN_vkCmdDraw,"vkCmdDraw");
        COMP_COLOR_LOAD(copy,PFN_vkCmdCopyImage,"vkCmdCopyImage");
        #undef COMP_COLOR_LOAD
    }
    return comp_color_render_functions()&&comp_color_shader_api.image&&comp_color_shader_api.requirements
        &&comp_color_shader_api.properties&&comp_color_shader_api.allocate&&comp_color_shader_api.bind_memory
        &&comp_color_shader_api.shader&&comp_color_shader_api.destroy_shader&&comp_color_shader_api.layout
        &&comp_color_shader_api.pipeline&&comp_color_shader_api.bind_pipeline&&comp_color_shader_api.draw&&comp_color_shader_api.copy;
}

static int comp_color_shader_image(struct comp_color_shader_target* target,unsigned samples,int mutable_image,VkFormat format) {
    if(target->view)return 1;
    VkDevice device=(VkDevice)g_comp_dev;
    VkFormat formats[2]={VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_B8G8R8A8_SRGB};
    VkImageFormatListCreateInfo list={VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO};list.viewFormatCount=2;list.pViewFormats=formats;
    VkImageCreateInfo info={VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};info.imageType=VK_IMAGE_TYPE_2D;info.format=format;
    info.extent=(VkExtent3D){g_comp_w,g_comp_h,1};info.mipLevels=info.arrayLayers=1;info.samples=(VkSampleCountFlagBits)samples;
    info.tiling=VK_IMAGE_TILING_OPTIMAL;info.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if(mutable_image){info.flags=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;info.pNext=&list;}
    if(comp_color_shader_api.image(device,&info,0,&target->image)!=VK_SUCCESS||!target->image)return 0;
    VkMemoryRequirements requirements;comp_color_shader_api.requirements(device,target->image,&requirements);
    VkPhysicalDeviceMemoryProperties properties;comp_color_shader_api.properties((VkPhysicalDevice)g_phys_dev,&properties);
    unsigned type=properties.memoryTypeCount;
    for(unsigned i=0;i<properties.memoryTypeCount&&i<32;i++)
        if((requirements.memoryTypeBits&(1u<<i))&&(properties.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)){type=i;break;}
    if(type>=properties.memoryTypeCount)return 0;
    VkMemoryAllocateInfo allocation={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocation.allocationSize=requirements.size;allocation.memoryTypeIndex=type;
    if(comp_color_shader_api.allocate(device,&allocation,0,&target->memory)!=VK_SUCCESS||!target->memory
        ||comp_color_shader_api.bind_memory(device,target->image,target->memory,0)!=VK_SUCCESS)return 0;
    VkImageViewCreateInfo view={VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=target->image;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=VK_FORMAT_B8G8R8A8_SRGB;
    view.subresourceRange=(VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    return comp_color_create_view(device,&view,0,&target->view)==VK_SUCCESS&&target->view;
}

static int comp_color_shader_pipeline(unsigned msaa) {
    VkDevice device=(VkDevice)g_comp_dev;
    if(comp_color_shader_pipelines[msaa])return 1;
    if(!comp_color_shader_layout) {
        VkPipelineLayoutCreateInfo info={VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        if(comp_color_shader_api.layout(device,&info,0,&comp_color_shader_layout)!=VK_SUCCESS||!comp_color_shader_layout)return 0;
    }
    VkAttachmentDescription attachments[2]={{0},{0}};
    for(unsigned i=0;i<1+msaa;i++) {
        attachments[i].format=VK_FORMAT_B8G8R8A8_SRGB;attachments[i].samples=VK_SAMPLE_COUNT_1_BIT;
        attachments[i].loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachments[i].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        attachments[i].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachments[i].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[i].initialLayout=attachments[i].finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    if(msaa){attachments[0].samples=VK_SAMPLE_COUNT_4_BIT;attachments[0].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;attachments[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;}
    VkAttachmentReference color={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},resolve={1,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass={0};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;subpass.colorAttachmentCount=1;
    subpass.pColorAttachments=&color;if(msaa)subpass.pResolveAttachments=&resolve;
    VkSubpassDependency dependency={VK_SUBPASS_EXTERNAL,0,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,0};
    VkRenderPassCreateInfo render={VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};render.attachmentCount=1+msaa;render.pAttachments=attachments;
    render.subpassCount=1;render.pSubpasses=&subpass;render.dependencyCount=1;render.pDependencies=&dependency;
    if(!comp_color_shader_passes[msaa]&&(comp_color_create_pass(device,&render,0,&comp_color_shader_passes[msaa])!=VK_SUCCESS||!comp_color_shader_passes[msaa]))return 0;
    VkShaderModule shaders[2]={0};VkShaderModuleCreateInfo module={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    module.codeSize=sizeof comp_color_vertex_spv;module.pCode=comp_color_vertex_spv;
    if(comp_color_shader_api.shader(device,&module,0,&shaders[0])!=VK_SUCCESS||!shaders[0])return 0;
    module.codeSize=sizeof comp_color_fragment_spv;module.pCode=comp_color_fragment_spv;
    if(comp_color_shader_api.shader(device,&module,0,&shaders[1])!=VK_SUCCESS||!shaders[1]){comp_color_shader_api.destroy_shader(device,shaders[0],0);return 0;}
    VkPipelineShaderStageCreateInfo stages[2]={{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
    stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=shaders[0];stages[0].pName="main";
    stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=shaders[1];stages[1].pName="main";
    VkPipelineVertexInputStateCreateInfo vertex={VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo assembly={VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport viewport={0,0,(float)g_comp_w,(float)g_comp_h,0,1};VkRect2D scissor={{0,0},{g_comp_w,g_comp_h}};
    VkPipelineViewportStateCreateInfo view={VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};view.viewportCount=view.scissorCount=1;view.pViewports=&viewport;view.pScissors=&scissor;
    VkPipelineRasterizationStateCreateInfo raster={VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.lineWidth=1;
    VkPipelineMultisampleStateCreateInfo samples={VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};samples.rasterizationSamples=msaa?VK_SAMPLE_COUNT_4_BIT:VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend={0};blend.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blending={VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blending.attachmentCount=1;blending.pAttachments=&blend;
    VkGraphicsPipelineCreateInfo pipeline={VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};pipeline.stageCount=2;pipeline.pStages=stages;
    pipeline.pVertexInputState=&vertex;pipeline.pInputAssemblyState=&assembly;pipeline.pViewportState=&view;pipeline.pRasterizationState=&raster;
    pipeline.pMultisampleState=&samples;pipeline.pColorBlendState=&blending;pipeline.layout=comp_color_shader_layout;pipeline.renderPass=comp_color_shader_passes[msaa];
    VkResult result=comp_color_shader_api.pipeline(device,VK_NULL_HANDLE,1,&pipeline,0,&comp_color_shader_pipelines[msaa]);
    comp_color_shader_api.destroy_shader(device,shaders[0],0);comp_color_shader_api.destroy_shader(device,shaders[1],0);
    return result==VK_SUCCESS&&comp_color_shader_pipelines[msaa];
}

static int comp_color_shader_render(void* cmd,unsigned index,unsigned mode) {
    unsigned kind=mode>=20?0:(mode-14)/2,msaa=mode>=20?1:(mode-14)%2;
    unsigned msaa_slot=mode>=20?mode-17:2,frame_kind=mode>=20?mode-17:kind;
    if(kind>2||!g_phys_dev||!comp_color_shader_functions())return 0;
    VkDevice device=(VkDevice)g_comp_dev;VkImage proxy=(VkImage)g_comp_img[index];
    if(comp_color_device&&(comp_color_device!=device||comp_color_width!=g_comp_w||comp_color_height!=g_comp_h))return 0;
    comp_color_device=device;comp_color_width=g_comp_w;comp_color_height=g_comp_h;
    if(comp_color_shader_proxies[index]&&comp_color_shader_proxies[index]!=proxy)return 0;
    comp_color_shader_proxies[index]=proxy;
    if(!comp_color_shader_proxy_views[index]) {
        VkImageViewCreateInfo info={VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};info.image=proxy;info.viewType=VK_IMAGE_VIEW_TYPE_2D;info.format=VK_FORMAT_B8G8R8A8_SRGB;
        info.subresourceRange=(VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        if(comp_color_create_view(device,&info,0,&comp_color_shader_proxy_views[index])!=VK_SUCCESS||!comp_color_shader_proxy_views[index])return 0;
    }
    if(kind&&!comp_color_shader_image(&comp_color_shader_owned[kind-1],VK_SAMPLE_COUNT_1_BIT,kind==2,VK_FORMAT_B8G8R8A8_SRGB))return 0;
    if(msaa&&!comp_color_shader_image(&comp_color_shader_owned[msaa_slot],VK_SAMPLE_COUNT_4_BIT,mode>=20,mode==20?VK_FORMAT_B8G8R8A8_UNORM:VK_FORMAT_B8G8R8A8_SRGB))return 0;
    if(!comp_color_shader_pipeline(msaa))return 0;
    unsigned slot=kind?0:index;
    if(!comp_color_shader_frames[frame_kind][msaa][slot]) {
        VkImageView target=kind?comp_color_shader_owned[kind-1].view:comp_color_shader_proxy_views[index];
        VkImageView views[2]={msaa?comp_color_shader_owned[msaa_slot].view:target,target};
        VkFramebufferCreateInfo info={VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};info.renderPass=comp_color_shader_passes[msaa];
        info.attachmentCount=1+msaa;info.pAttachments=views;info.width=g_comp_w;info.height=g_comp_h;info.layers=1;
        if(comp_color_create_framebuffer(device,&info,0,&comp_color_shader_frames[frame_kind][msaa][slot])!=VK_SUCCESS||!comp_color_shader_frames[frame_kind][msaa][slot])return 0;
    }
    struct comp_color_shader_target* target=kind?&comp_color_shader_owned[kind-1]:0;
    if(target&&!target->initialized){comp_barrier(cmd,(void*)target->image,0,2,0,0x100,0x1,0x400);target->initialized=1;}
    VkRenderPassBeginInfo begin={VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};begin.renderPass=comp_color_shader_passes[msaa];
    begin.framebuffer=comp_color_shader_frames[frame_kind][msaa][slot];begin.renderArea.extent=(VkExtent2D){g_comp_w,g_comp_h};
    comp_color_begin((VkCommandBuffer)cmd,&begin,VK_SUBPASS_CONTENTS_INLINE);
    comp_color_shader_api.bind_pipeline((VkCommandBuffer)cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,comp_color_shader_pipelines[msaa]);
    comp_color_shader_api.draw((VkCommandBuffer)cmd,3,1,0,0);comp_color_end((VkCommandBuffer)cmd);
    if(target) {
        comp_barrier(cmd,(void*)target->image,2,6,0x100,0x800,0x400,0x1000);
        comp_barrier(cmd,(void*)proxy,2,7,0x100,0x1000,0x400,0x1000);
        VkImageCopy copy={0};copy.srcSubresource=(VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.dstSubresource=copy.srcSubresource;
        copy.extent=(VkExtent3D){g_comp_w,g_comp_h,1};
        comp_color_shader_api.copy((VkCommandBuffer)cmd,target->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,proxy,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        comp_barrier(cmd,(void*)target->image,6,2,0x800,0x100,0x1000,0x400);
        comp_barrier(cmd,(void*)proxy,7,2,0x1000,0x180,0x1000,0x400);
    }
    return 1;
}

static void comp_color_diag(void* cmd,unsigned index) {
    if(!cmd||!g_comp_on||g_comp_disable||!g_comp_mutable||index>=g_comp_n||index>=COMP_MAX
        ||!g_comp_img[index]||!g_own_imgs[index])return;
    int request=__atomic_exchange_n(&g_comp_color_request,-1,__ATOMIC_ACQ_REL);
    if(request>=0) {
        g_comp_color_mode=request<=21?request:0;
        g_comp_color_frames=g_comp_color_mode?360:0;
        if(g_comp_color_mode&&g_comp_color_mode<=5&&!comp_color_clear)
            comp_color_clear=(void(*)(void*,void*,unsigned,const float*,unsigned,const void*))comp_fn("vkCmdClearColorImage");
        if(g_comp_color_mode&&g_comp_color_mode<=5&&!comp_color_clear)g_comp_color_mode=g_comp_color_frames=0;
        LOGF("COMP COLOR diagnostic mode=%d frames=%d mutableBGRA44 sampleSRGB50",g_comp_color_mode,g_comp_color_frames);
        if(g_comp_color_mode>=14)LOGF("COMP COLOR shader bars left-to-right RGB/linearGray0.25 (sRGB screenshot expected255RGB/137gray); mode14=proxy1x 15=proxy4x 16=nonmutable50x1 17=nonmutable50x4 18=mutable50x1 19=mutable50x4 20=mutable44MSAA4 21=mutable50MSAA4");
    }
    if(!g_comp_color_mode||g_comp_color_frames<=0)return;
    float color[4]={0,0,0,1};
    if(g_comp_color_mode>=14) {
        if(!comp_color_shader_render(cmd,index,(unsigned)g_comp_color_mode)) {
            LOGF("COMP COLOR shader diagnostic failed mode=%d",g_comp_color_mode);g_comp_color_mode=g_comp_color_frames=0;return;
        }
    } else if(g_comp_color_mode>=6) {
        unsigned channel=(unsigned)(g_comp_color_mode-(g_comp_color_mode>=10?10:6));
        if(channel<3)color[channel]=1;else for(unsigned k=0;k<3;k++)color[k]=.25f;
        if(!comp_color_render(cmd,index,g_comp_color_mode>=10,color)) {
            LOGF("COMP COLOR render diagnostic failed mode=%d",g_comp_color_mode);g_comp_color_mode=g_comp_color_frames=0;return;
        }
    } else {
    if(g_comp_color_mode<=3)color[g_comp_color_mode-1]=1;
    else for(unsigned k=0;k<3;k++)color[k]=g_comp_color_mode==4?.25f:1;
    const unsigned range[5]={1,0,1,0,1};
    comp_barrier(cmd,g_comp_img[index],2,7,0x100,0x1000,0x400,0x1000);
    comp_color_clear(cmd,g_comp_img[index],7,color,1,range);
    comp_barrier(cmd,g_comp_img[index],7,2,0x1000,0x180,0x1000,0x400);
    }
    if(--g_comp_color_frames==0){g_comp_color_mode=0;LOGF("COMP COLOR diagnostic complete; native rendering restored");}
}
#endif
