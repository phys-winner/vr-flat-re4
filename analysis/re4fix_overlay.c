// re4fix_overlay.c - touch-control overlay renderer (alpha-blended textured quads from an icon atlas).
// Samples the owned scene before drawing HUD quads inside the compositor command buffer.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <string.h>
#include <android/log.h>
#include "re4fix_overlay.h"
#include "overlay/overlay_assets.h"

#define OLOG(...) __android_log_print(ANDROID_LOG_INFO, "RE4FIX-N", __VA_ARGS__)
#define OVL_MAXIMG 8

static PFN_vkGetDeviceProcAddr p_GetDeviceProcAddr;
static PFN_vkGetPhysicalDeviceMemoryProperties p_GetPhysMemProps;
static PFN_vkCreateRenderPass p_CreateRenderPass;
static PFN_vkCreateImageView p_CreateImageView;
static PFN_vkCreateFramebuffer p_CreateFramebuffer;
static PFN_vkCreateShaderModule p_CreateShaderModule;
static PFN_vkCreateDescriptorSetLayout p_CreateDSL;
static PFN_vkCreatePipelineLayout p_CreatePipelineLayout;
static PFN_vkCreateGraphicsPipelines p_CreateGraphicsPipelines;
static PFN_vkCreateImage p_CreateImage;
static PFN_vkGetImageMemoryRequirements p_GetImageMemReq;
static PFN_vkCreateBuffer p_CreateBuffer;
static PFN_vkGetBufferMemoryRequirements p_GetBufferMemReq;
static PFN_vkAllocateMemory p_AllocateMemory;
static PFN_vkBindImageMemory p_BindImageMemory;
static PFN_vkBindBufferMemory p_BindBufferMemory;
static PFN_vkMapMemory p_MapMemory;
static PFN_vkCreateSampler p_CreateSampler;
static PFN_vkCreateDescriptorPool p_CreateDescriptorPool;
static PFN_vkAllocateDescriptorSets p_AllocateDescriptorSets;
static PFN_vkUpdateDescriptorSets p_UpdateDescriptorSets;
static PFN_vkCmdPipelineBarrier p_CmdPipelineBarrier;
static PFN_vkCmdCopyBufferToImage p_CmdCopyBufferToImage;
static PFN_vkCmdBeginRenderPass p_CmdBeginRenderPass;
static PFN_vkCmdEndRenderPass p_CmdEndRenderPass;
static PFN_vkCmdBindPipeline p_CmdBindPipeline;
static PFN_vkCmdBindDescriptorSets p_CmdBindDescriptorSets;
static PFN_vkCmdPushConstants p_CmdPushConstants;
static PFN_vkCmdDraw p_CmdDraw;

static VkDevice s_dev;
static VkRenderPass s_rp;
static VkImageView s_views[OVL_MAXIMG];
static VkImageView s_scene_views[OVL_MAXIMG];
static VkFramebuffer s_fbs[OVL_MAXIMG];
static VkDescriptorSetLayout s_dsl;
static VkPipelineLayout s_pl;
static VkPipeline s_pipe;
static VkImage s_atlas;
static VkImageView s_atlas_view;
static VkSampler s_sampler;
static VkSampler s_scene_sampler;
static VkBuffer s_stage;
static VkDescriptorPool s_dpool;
static VkDescriptorSet s_ds;
static VkDescriptorSet s_scene_ds[OVL_MAXIMG];
static unsigned s_w, s_h, s_n, s_fmt;
static int s_ready, s_uploaded;

int ovl_ready(void) { return s_ready; }

// Window swapchain recreated with the same image count (app resumed from background, another activity
// returned): the attachment views/framebuffers belonged to the destroyed images. Build them for the new
// images; the old objects are left alone (their images are gone, they are never referenced again).
int ovl_rebind(void* const* images, unsigned n) {
    if (!s_ready || !images || n != s_n || !p_CreateImageView || !p_CreateFramebuffer) return 0;
    for (unsigned i = 0; i < n; i++) {
        VkImageViewCreateInfo vci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image = (VkImage)images[i]; vci.viewType = VK_IMAGE_VIEW_TYPE_2D; vci.format = (VkFormat)s_fmt;
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; vci.subresourceRange.levelCount = 1; vci.subresourceRange.layerCount = 1;
        VkImageView view; VkFramebuffer fb; VkResult r;
        if (!images[i] || (r = p_CreateImageView(s_dev, &vci, 0, &view)) != VK_SUCCESS) { OLOG("OVL: rebind view %u", i); return 0; }
        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = s_rp; fci.attachmentCount = 1; fci.pAttachments = &view; fci.width = s_w; fci.height = s_h; fci.layers = 1;
        if ((r = p_CreateFramebuffer(s_dev, &fci, 0, &fb)) != VK_SUCCESS) { OLOG("OVL: rebind fb %u %d", i, r); return 0; }
        s_views[i] = view; s_fbs[i] = fb;
    }
    OLOG("OVL: rebound %u swapchain framebuffers", n);
    return 1;
}

void ovl_cell_uv(int cell, float* u0, float* v0, float* u1, float* v1) {
    const float cw = 1.0f / OVL_CELLS_PER_ROW, inset = 0.5f / OVL_ATLAS_W;
    int cx = cell % OVL_CELLS_PER_ROW, cy = cell / OVL_CELLS_PER_ROW;
    *u0 = cx * cw + inset; *v0 = cy * cw + inset; *u1 = (cx + 1) * cw - inset; *v1 = (cy + 1) * cw - inset;
}

static uint32_t mem_type(VkPhysicalDevice phys, uint32_t bits, VkMemoryPropertyFlags want) {
    VkPhysicalDeviceMemoryProperties mp;
    p_GetPhysMemProps(phys, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++) if (bits & (1u << i)) return i;
    return 0;
}

#define LOADI(var, name) var = (void*)gipa(instance, name); if (!var) { OLOG("OVL: missing %s", name); return 0; }

int ovl_init(void* (*gipa)(void*, const char*), void* instance, void* physv, void* devv, unsigned qfamily,
             unsigned w, unsigned h, unsigned fmt, unsigned n, void* const* images, void* const* proxy_images, int mutable_proxy) {
    (void)qfamily;
    (void)mutable_proxy;
    if (s_ready) return 1;
    if (!gipa || !instance || !physv || !devv || !images || !proxy_images
        || !w || !h || w>16384 || h>16384 || n == 0 || n > OVL_MAXIMG) return 0;
    for (unsigned i=0;i<n;i++) {
        if (!images[i] || !proxy_images[i]) return 0;
        for (unsigned j=0;j<n;j++) if (proxy_images[i]==images[j]) return 0;
    }
    VkPhysicalDevice phys = (VkPhysicalDevice)physv;
    VkDevice dev = (VkDevice)devv;
    LOADI(p_GetDeviceProcAddr, "vkGetDeviceProcAddr");
    LOADI(p_GetPhysMemProps, "vkGetPhysicalDeviceMemoryProperties");
    LOADI(p_CreateRenderPass, "vkCreateRenderPass");
    LOADI(p_CreateImageView, "vkCreateImageView");
    LOADI(p_CreateFramebuffer, "vkCreateFramebuffer");
    LOADI(p_CreateShaderModule, "vkCreateShaderModule");
    LOADI(p_CreateDSL, "vkCreateDescriptorSetLayout");
    LOADI(p_CreatePipelineLayout, "vkCreatePipelineLayout");
    LOADI(p_CreateGraphicsPipelines, "vkCreateGraphicsPipelines");
    LOADI(p_CreateImage, "vkCreateImage");
    LOADI(p_GetImageMemReq, "vkGetImageMemoryRequirements");
    LOADI(p_CreateBuffer, "vkCreateBuffer");
    LOADI(p_GetBufferMemReq, "vkGetBufferMemoryRequirements");
    LOADI(p_AllocateMemory, "vkAllocateMemory");
    LOADI(p_BindImageMemory, "vkBindImageMemory");
    LOADI(p_BindBufferMemory, "vkBindBufferMemory");
    LOADI(p_MapMemory, "vkMapMemory");
    LOADI(p_CreateSampler, "vkCreateSampler");
    LOADI(p_CreateDescriptorPool, "vkCreateDescriptorPool");
    LOADI(p_AllocateDescriptorSets, "vkAllocateDescriptorSets");
    LOADI(p_UpdateDescriptorSets, "vkUpdateDescriptorSets");
    LOADI(p_CmdPipelineBarrier, "vkCmdPipelineBarrier");
    LOADI(p_CmdCopyBufferToImage, "vkCmdCopyBufferToImage");
    LOADI(p_CmdBeginRenderPass, "vkCmdBeginRenderPass");
    LOADI(p_CmdEndRenderPass, "vkCmdEndRenderPass");
    LOADI(p_CmdBindPipeline, "vkCmdBindPipeline");
    LOADI(p_CmdBindDescriptorSets, "vkCmdBindDescriptorSets");
    LOADI(p_CmdPushConstants, "vkCmdPushConstants");
    LOADI(p_CmdDraw, "vkCmdDraw");
    VkResult r;

    // The fullscreen draw replaces every pixel before the touch overlay.
    VkAttachmentDescription att = {0};
    att.format = (VkFormat)fmt; att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sp = {0};
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; sp.colorAttachmentCount = 1; sp.pColorAttachments = &ref;
    VkSubpassDependency dep[2] = {{0}};
    dep[0].srcSubpass = VK_SUBPASS_EXTERNAL; dep[0].dstSubpass = 0;
    dep[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; dep[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dep[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dep[1].srcSubpass = 0; dep[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dep[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; dep[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dep[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; dep[1].dstAccessMask = 0;
    VkRenderPassCreateInfo rpci = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpci.attachmentCount = 1; rpci.pAttachments = &att; rpci.subpassCount = 1; rpci.pSubpasses = &sp;
    rpci.dependencyCount = 2; rpci.pDependencies = dep;
    if ((r = p_CreateRenderPass(dev, &rpci, 0, &s_rp)) != VK_SUCCESS) { OLOG("OVL: render pass %d", r); return 0; }

    for (unsigned i = 0; i < n; i++) {
        VkImageViewCreateInfo vci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image = (VkImage)images[i]; vci.viewType = VK_IMAGE_VIEW_TYPE_2D; vci.format = (VkFormat)fmt;
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; vci.subresourceRange.levelCount = 1; vci.subresourceRange.layerCount = 1;
        if ((r = p_CreateImageView(dev, &vci, 0, &s_views[i])) != VK_SUCCESS) { OLOG("OVL: view %u %d", i, r); return 0; }
        VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass = s_rp; fci.attachmentCount = 1; fci.pAttachments = &s_views[i]; fci.width = w; fci.height = h; fci.layers = 1;
        if ((r = p_CreateFramebuffer(dev, &fci, 0, &s_fbs[i])) != VK_SUCCESS) { OLOG("OVL: fb %u %d", i, r); return 0; }
        vci.image = (VkImage)proxy_images[i]; vci.format = VK_FORMAT_B8G8R8A8_SRGB;
        if ((r = p_CreateImageView(dev, &vci, 0, &s_scene_views[i])) != VK_SUCCESS) { OLOG("OVL: scene view %u %d", i, r); return 0; }
    }

    // atlas texture + staging buffer (RLE decoded straight into the mapped staging memory)
    VkImageCreateInfo ici = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType = VK_IMAGE_TYPE_2D; ici.format = VK_FORMAT_R8G8B8A8_SRGB;
    ici.extent.width = OVL_ATLAS_W; ici.extent.height = OVL_ATLAS_H; ici.extent.depth = 1;
    ici.mipLevels = 1; ici.arrayLayers = 1; ici.samples = VK_SAMPLE_COUNT_1_BIT; ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if ((r = p_CreateImage(dev, &ici, 0, &s_atlas)) != VK_SUCCESS) { OLOG("OVL: atlas image %d", r); return 0; }
    VkMemoryRequirements mr; p_GetImageMemReq(dev, s_atlas, &mr);
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size; mai.memoryTypeIndex = mem_type(phys, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkDeviceMemory amem;
    if ((r = p_AllocateMemory(dev, &mai, 0, &amem)) != VK_SUCCESS || (r = p_BindImageMemory(dev, s_atlas, amem, 0)) != VK_SUCCESS) {
        OLOG("OVL: atlas memory %d", r); return 0;
    }
    VkImageViewCreateInfo avci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    avci.image = s_atlas; avci.viewType = VK_IMAGE_VIEW_TYPE_2D; avci.format = VK_FORMAT_R8G8B8A8_SRGB;
    avci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; avci.subresourceRange.levelCount = 1; avci.subresourceRange.layerCount = 1;
    if ((r = p_CreateImageView(dev, &avci, 0, &s_atlas_view)) != VK_SUCCESS) { OLOG("OVL: atlas view %d", r); return 0; }
    VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = (VkDeviceSize)OVL_ATLAS_W * OVL_ATLAS_H * 4; bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if ((r = p_CreateBuffer(dev, &bci, 0, &s_stage)) != VK_SUCCESS) { OLOG("OVL: staging %d", r); return 0; }
    VkMemoryRequirements br; p_GetBufferMemReq(dev, s_stage, &br);
    VkMemoryAllocateInfo bmai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    bmai.allocationSize = br.size;
    bmai.memoryTypeIndex = mem_type(phys, br.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkDeviceMemory bmem; void* map = 0;
    if ((r = p_AllocateMemory(dev, &bmai, 0, &bmem)) != VK_SUCCESS || (r = p_BindBufferMemory(dev, s_stage, bmem, 0)) != VK_SUCCESS ||
        (r = p_MapMemory(dev, bmem, 0, VK_WHOLE_SIZE, 0, &map)) != VK_SUCCESS || !map) { OLOG("OVL: staging memory %d", r); return 0; }
    {
        unsigned char* dst = (unsigned char*)map;
        const unsigned char* src = ovl_atlas_rle;
        size_t pos = 0, total = (size_t)OVL_ATLAS_W * OVL_ATLAS_H, k = 0;
        memset(dst, 0, total * 4);
        while (k + 4 <= sizeof ovl_atlas_rle && pos < total) {
            unsigned run = src[k] | (src[k + 1] << 8), lit = src[k + 2] | (src[k + 3] << 8);
            k += 4; pos += run;
            if (pos + lit > total || k + lit * 4 > sizeof ovl_atlas_rle) break;
            memcpy(dst + pos * 4, src + k, lit * 4); pos += lit; k += lit * 4;
        }
        if (pos != total) OLOG("OVL: atlas RLE decoded %zu of %zu pixels", pos, total);
    }
    VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_LINEAR; sci.minFilter = VK_FILTER_LINEAR; sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; sci.maxLod = 0.0f;
    if ((r = p_CreateSampler(dev, &sci, 0, &s_sampler)) != VK_SUCCESS) { OLOG("OVL: sampler %d", r); return 0; }
    sci.magFilter = VK_FILTER_NEAREST; sci.minFilter = VK_FILTER_NEAREST;
    if ((r = p_CreateSampler(dev, &sci, 0, &s_scene_sampler)) != VK_SUCCESS) { OLOG("OVL: scene sampler %d", r); return 0; }

    // descriptors
    VkDescriptorSetLayoutBinding b = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 0};
    VkDescriptorSetLayoutCreateInfo dlci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dlci.bindingCount = 1; dlci.pBindings = &b;
    if ((r = p_CreateDSL(dev, &dlci, 0, &s_dsl)) != VK_SUCCESS) { OLOG("OVL: dsl %d", r); return 0; }
    VkDescriptorPoolSize ps = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, n+1};
    VkDescriptorPoolCreateInfo dpci = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = n+1; dpci.poolSizeCount = 1; dpci.pPoolSizes = &ps;
    if ((r = p_CreateDescriptorPool(dev, &dpci, 0, &s_dpool)) != VK_SUCCESS) { OLOG("OVL: dpool %d", r); return 0; }
    VkDescriptorSetAllocateInfo dsai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = s_dpool; dsai.descriptorSetCount = 1; dsai.pSetLayouts = &s_dsl;
    if ((r = p_AllocateDescriptorSets(dev, &dsai, &s_ds)) != VK_SUCCESS) { OLOG("OVL: dset %d", r); return 0; }
    VkDescriptorImageInfo dii = {s_sampler, s_atlas_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet wds = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    wds.dstSet = s_ds; wds.dstBinding = 0; wds.descriptorCount = 1; wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wds.pImageInfo = &dii;
    p_UpdateDescriptorSets(dev, 1, &wds, 0, 0);
    for (unsigned i=0;i<n;i++) {
        if ((r = p_AllocateDescriptorSets(dev, &dsai, &s_scene_ds[i])) != VK_SUCCESS) { OLOG("OVL: scene dset %u %d", i, r); return 0; }
        dii.sampler=s_scene_sampler; dii.imageView=s_scene_views[i];
        wds.dstSet=s_scene_ds[i];
        p_UpdateDescriptorSets(dev,1,&wds,0,0);
    }

    // pipeline
    VkPushConstantRange pcr = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(struct ovl_quad)};
    VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1; plci.pSetLayouts = &s_dsl; plci.pushConstantRangeCount = 1; plci.pPushConstantRanges = &pcr;
    if ((r = p_CreatePipelineLayout(dev, &plci, 0, &s_pl)) != VK_SUCCESS) { OLOG("OVL: pipeline layout %d", r); return 0; }
    VkShaderModule vs, fs;
    VkShaderModuleCreateInfo smci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = sizeof ovl_vert_spv; smci.pCode = ovl_vert_spv;
    if ((r = p_CreateShaderModule(dev, &smci, 0, &vs)) != VK_SUCCESS) { OLOG("OVL: vs %d", r); return 0; }
    smci.codeSize = sizeof ovl_frag_spv; smci.pCode = ovl_frag_spv;
    if ((r = p_CreateShaderModule(dev, &smci, 0, &fs)) != VK_SUCCESS) { OLOG("OVL: fs %d", r); return 0; }
    VkPipelineShaderStageCreateInfo st[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}, {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
    st[0].stage = VK_SHADER_STAGE_VERTEX_BIT; st[0].module = vs; st[0].pName = "main";
    st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; st[1].module = fs; st[1].pName = "main";
    VkPipelineVertexInputStateCreateInfo vi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport vp = {0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f};
    VkRect2D sc = {{0, 0}, {w, h}};
    VkPipelineViewportStateCreateInfo vps = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vps.viewportCount = 1; vps.pViewports = &vp; vps.scissorCount = 1; vps.pScissors = &sc;
    VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE; rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState cba = {0};
    cba.blendEnable = VK_TRUE;
    cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; cba.colorBlendOp = VK_BLEND_OP_ADD;
    cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; cba.alphaBlendOp = VK_BLEND_OP_ADD;
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1; cb.pAttachments = &cba;
    VkGraphicsPipelineCreateInfo gp = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gp.stageCount = 2; gp.pStages = st; gp.pVertexInputState = &vi; gp.pInputAssemblyState = &ia; gp.pViewportState = &vps;
    gp.pRasterizationState = &rs; gp.pMultisampleState = &ms; gp.pColorBlendState = &cb; gp.layout = s_pl; gp.renderPass = s_rp;
    if ((r = p_CreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gp, 0, &s_pipe)) != VK_SUCCESS) { OLOG("OVL: pipeline %d", r); return 0; }

    s_dev = dev; s_w = w; s_h = h; s_n = n; s_fmt = fmt;
    s_ready = 1;
    OLOG("OVL: ready %ux%u fmt=%u images=%u", w, h, fmt, n);
    return 1;
}

static void img_barrier(VkCommandBuffer cmd, VkImage img, VkImageLayout o, VkImageLayout nl, VkAccessFlags sa, VkAccessFlags da,
                        VkPipelineStageFlags ss, VkPipelineStageFlags ds) {
    VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask = sa; b.dstAccessMask = da; b.oldLayout = o; b.newLayout = nl;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; b.image = img;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; b.subresourceRange.levelCount = 1; b.subresourceRange.layerCount = 1;
    p_CmdPipelineBarrier(cmd, ss, ds, 0, 0, 0, 0, 0, 1, &b);
}

void ovl_record(void* cmdv, unsigned idx, const struct ovl_quad* q, int nq) {
    if (!s_ready || !cmdv || idx >= s_n) return;
    if (!q || nq<0) nq=0;
    if (nq>48) nq=48;
    VkCommandBuffer cmd = (VkCommandBuffer)cmdv;
    if (!s_uploaded) {
        img_barrier(cmd, s_atlas, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy c = {0};
        c.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; c.imageSubresource.layerCount = 1;
        c.imageExtent.width = OVL_ATLAS_W; c.imageExtent.height = OVL_ATLAS_H; c.imageExtent.depth = 1;
        p_CmdCopyBufferToImage(cmd, s_stage, s_atlas, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &c);
        img_barrier(cmd, s_atlas, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        s_uploaded = 1;
    }
    VkRenderPassBeginInfo bi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    bi.renderPass = s_rp; bi.framebuffer = s_fbs[idx];
    bi.renderArea.extent.width = s_w; bi.renderArea.extent.height = s_h;
    p_CmdBeginRenderPass(cmd, &bi, VK_SUBPASS_CONTENTS_INLINE);
    p_CmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,s_pipe);
    p_CmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,s_pl,0,1,&s_scene_ds[idx],0,0);
    const struct ovl_quad scene={-1,-1,1,1,0,0,1,1,1,1,1,2};
    p_CmdPushConstants(cmd,s_pl,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof scene,&scene);
    p_CmdDraw(cmd,6,1,0,0);
    if (nq > 0) {
        p_CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_pl, 0, 1, &s_ds, 0, 0);
        for (int i = 0; i < nq; i++) {
            p_CmdPushConstants(cmd, s_pl, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(struct ovl_quad), &q[i]);
            p_CmdDraw(cmd, 6, 1, 0, 0);
        }
    }
    p_CmdEndRenderPass(cmd);
}
