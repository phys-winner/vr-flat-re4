#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../re4fix_overlay.c"

#define H(type,value) ((type)(uintptr_t)(value))
static void *window_images[8], *proxy_images[8];
static VkImageViewCreateInfo views[24];
static VkImageView view_handles[24];
static VkSamplerCreateInfo samplers[3];
static VkSampler sampler_handles[3];
static VkDescriptorSet sets[9];
static VkDescriptorImageInfo set_images[9];
static unsigned set_written[9];
static VkFramebuffer framebuffers[8];
static VkDescriptorSet bound_set;
static struct ovl_quad pushed, draws[64];
static VkDescriptorSet draw_sets[64];
static unsigned api_calls, lookups, view_count, sampler_count, set_count, writes, fb_count;
static unsigned begins, ends, pipelines, binds, pushes, draw_count, barriers, copies;
static unsigned expected_n, expected_index, expected_w, expected_h, rp_count, shader_count, pool_capacity;
static const char *failure, *missing;
static unsigned failure_at, failure_hits;
static int null_map;
static int proxy_mutable;
static uint32_t property_vendor, property_device, property_driver;
static unsigned property_queries, property_lookups;
static char last_log[256];
static unsigned char atlas_map[OVL_ATLAS_W * OVL_ATLAS_H * 4];

int __android_log_print(int priority,const char *tag,const char *format,...) {
    (void)priority;(void)tag;va_list args;va_start(args,format);
    int result=vsnprintf(last_log,sizeof last_log,format,args);va_end(args);
    assert(!strstr(format,"RB quirk"));
    return result;
}
static VkResult result_for(const char *name) {
    ++api_calls;
    return failure && !strcmp(failure,name) && ++failure_hits==failure_at
        ? VK_ERROR_FORMAT_NOT_SUPPORTED : VK_SUCCESS;
}
static void memory_properties(VkPhysicalDevice physical,VkPhysicalDeviceMemoryProperties *out) {
    assert(physical==H(VkPhysicalDevice,0x2000));memset(out,0,sizeof *out);
    out->memoryTypeCount=1;
    out->memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        |VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
}
static void physical_properties(VkPhysicalDevice physical,VkPhysicalDeviceProperties *out) {
    assert(physical==H(VkPhysicalDevice,0x2000) && !api_calls && !property_queries);
    ++property_queries;memset(out,0,sizeof *out);
    out->vendorID=property_vendor;out->deviceID=property_device;out->driverVersion=property_driver;
    out->sparseProperties.residencyNonResidentStrict=VK_TRUE;
}
static VkResult create_rp(VkDevice device,const VkRenderPassCreateInfo *ci,const VkAllocationCallbacks *a,VkRenderPass *out) {
    assert(device==H(VkDevice,0x1000) && !a && ci->sType==VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
    assert(ci->attachmentCount==1 && ci->subpassCount==1 && ci->dependencyCount==2);
    const VkAttachmentDescription *att=ci->pAttachments;
    assert(att->format==VK_FORMAT_R8G8B8A8_SRGB && att->samples==VK_SAMPLE_COUNT_1_BIT);
    assert(att->loadOp==VK_ATTACHMENT_LOAD_OP_DONT_CARE && att->storeOp==VK_ATTACHMENT_STORE_OP_STORE);
    assert(att->initialLayout==VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        && att->finalLayout==VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    assert(ci->pSubpasses[0].colorAttachmentCount==1 && ci->pSubpasses[0].pColorAttachments[0].attachment==0);
    assert(ci->pSubpasses[0].pColorAttachments[0].layout==VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const VkSubpassDependency *d=ci->pDependencies;
    assert(d[0].srcSubpass==VK_SUBPASS_EXTERNAL && !d[0].dstSubpass);
    assert(d[0].srcStageMask==VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        && d[0].dstStageMask==VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    assert(d[0].dstAccessMask&VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    assert(!d[1].srcSubpass && d[1].dstSubpass==VK_SUBPASS_EXTERNAL
        && d[1].srcAccessMask==VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    ++rp_count;VkResult r=result_for("vkCreateRenderPass");*out=H(VkRenderPass,0x3000);return r;
}
static VkResult create_view(VkDevice d,const VkImageViewCreateInfo *ci,const VkAllocationCallbacks *a,VkImageView *out) {
    assert(d==H(VkDevice,0x1000) && !a && ci->sType==VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO && !ci->pNext);
    assert(ci->viewType==VK_IMAGE_VIEW_TYPE_2D && ci->subresourceRange.aspectMask==VK_IMAGE_ASPECT_COLOR_BIT);
    assert(!ci->subresourceRange.baseMipLevel && !ci->subresourceRange.baseArrayLayer
        && ci->subresourceRange.levelCount==1 && ci->subresourceRange.layerCount==1);
    assert(ci->components.r==VK_COMPONENT_SWIZZLE_IDENTITY && ci->components.b==VK_COMPONENT_SWIZZLE_IDENTITY);
    assert(ci->components.g==VK_COMPONENT_SWIZZLE_IDENTITY && ci->components.a==VK_COMPONENT_SWIZZLE_IDENTITY);
    assert(view_count<24);views[view_count]=*ci;
    *out=view_handles[view_count]=H(VkImageView,0x11000+view_count*0x100);++view_count;
    return result_for("vkCreateImageView");
}
static VkResult create_fb(VkDevice d,const VkFramebufferCreateInfo *ci,const VkAllocationCallbacks *a,VkFramebuffer *out) {
    assert(d==H(VkDevice,0x1000) && !a && ci->sType==VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
    assert(ci->renderPass==H(VkRenderPass,0x3000) && ci->attachmentCount==1
        && ci->width==expected_w && ci->height==expected_h && ci->layers==1);
    unsigned found=24;for(unsigned i=0;i<view_count;i++) if(view_handles[i]==ci->pAttachments[0]) found=i;
    assert(found<24 && views[found].image==(VkImage)window_images[fb_count]
        && views[found].format==VK_FORMAT_R8G8B8A8_SRGB);
    *out=framebuffers[fb_count]=H(VkFramebuffer,0x15000+fb_count*0x100);++fb_count;
    return result_for("vkCreateFramebuffer");
}
static VkResult create_shader(VkDevice d,const VkShaderModuleCreateInfo *ci,const VkAllocationCallbacks *a,VkShaderModule *out) {
    assert(d==H(VkDevice,0x1000) && !a && ci->sType==VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
    assert(ci->pCode[0]==0x07230203 && ci->codeSize%4==0);
    assert(ci->codeSize==(shader_count ? sizeof ovl_frag_spv : sizeof ovl_vert_spv));
    *out=H(VkShaderModule,0x16000+shader_count++*0x100);return result_for("vkCreateShaderModule");
}
static VkResult create_dsl(VkDevice d,const VkDescriptorSetLayoutCreateInfo *ci,const VkAllocationCallbacks *a,VkDescriptorSetLayout *out) {
    (void)d;assert(!a && ci->bindingCount==1 && !ci->pBindings[0].binding);
    assert(ci->pBindings[0].descriptorType==VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        && ci->pBindings[0].descriptorCount==1 && ci->pBindings[0].stageFlags==VK_SHADER_STAGE_FRAGMENT_BIT);
    *out=H(VkDescriptorSetLayout,0x17000);return result_for("vkCreateDescriptorSetLayout");
}
static VkResult create_pl(VkDevice d,const VkPipelineLayoutCreateInfo *ci,const VkAllocationCallbacks *a,VkPipelineLayout *out) {
    (void)d;assert(!a && ci->setLayoutCount==1 && ci->pushConstantRangeCount==1);
    assert(ci->pPushConstantRanges[0].stageFlags==VK_SHADER_STAGE_VERTEX_BIT
        && !ci->pPushConstantRanges[0].offset && ci->pPushConstantRanges[0].size==sizeof(struct ovl_quad));
    *out=H(VkPipelineLayout,0x18000);return result_for("vkCreatePipelineLayout");
}
static VkResult create_pipeline(VkDevice d,VkPipelineCache cache,uint32_t n,const VkGraphicsPipelineCreateInfo *ci,const VkAllocationCallbacks *a,VkPipeline *out) {
    (void)d;assert(!cache && !a && n==1 && ci->stageCount==2 && ci->renderPass==H(VkRenderPass,0x3000));
    assert(ci->pViewportState->pViewports[0].width==expected_w && ci->pViewportState->pViewports[0].height==expected_h);
    assert(ci->pColorBlendState->attachmentCount==1 && ci->pColorBlendState->pAttachments[0].blendEnable);
    assert(ci->pColorBlendState->pAttachments[0].srcColorBlendFactor==VK_BLEND_FACTOR_SRC_ALPHA
        && ci->pColorBlendState->pAttachments[0].dstColorBlendFactor==VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    *out=H(VkPipeline,0x19000);return result_for("vkCreateGraphicsPipelines");
}
static VkResult create_image(VkDevice d,const VkImageCreateInfo *ci,const VkAllocationCallbacks *a,VkImage *out) {
    (void)d;assert(!a && ci->format==VK_FORMAT_R8G8B8A8_SRGB && ci->extent.width==OVL_ATLAS_W && ci->extent.height==OVL_ATLAS_H);
    assert(ci->usage==(VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT));
    *out=H(VkImage,0x1a000);return result_for("vkCreateImage");
}
static void image_requirements(VkDevice d,VkImage image,VkMemoryRequirements *out) {
    (void)d;assert(image==H(VkImage,0x1a000));*out=(VkMemoryRequirements){sizeof atlas_map,16,1};
}
static VkResult create_buffer(VkDevice d,const VkBufferCreateInfo *ci,const VkAllocationCallbacks *a,VkBuffer *out) {
    (void)d;assert(!a && ci->size==sizeof atlas_map && ci->usage==VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    *out=H(VkBuffer,0x1b000);return result_for("vkCreateBuffer");
}
static void buffer_requirements(VkDevice d,VkBuffer b,VkMemoryRequirements *out) {
    (void)d;assert(b==H(VkBuffer,0x1b000));*out=(VkMemoryRequirements){sizeof atlas_map,16,1};
}
static VkResult allocate_memory(VkDevice d,const VkMemoryAllocateInfo *ci,const VkAllocationCallbacks *a,VkDeviceMemory *out) {
    (void)d;assert(!a && !ci->memoryTypeIndex && ci->allocationSize==sizeof atlas_map);
    *out=H(VkDeviceMemory,0x1c000);return result_for("vkAllocateMemory");
}
static VkResult bind_image(VkDevice d,VkImage image,VkDeviceMemory memory,VkDeviceSize offset) {
    (void)d;(void)image;(void)memory;assert(!offset);return result_for("vkBindImageMemory");
}
static VkResult bind_buffer(VkDevice d,VkBuffer buffer,VkDeviceMemory memory,VkDeviceSize offset) {
    (void)d;(void)buffer;(void)memory;assert(!offset);return result_for("vkBindBufferMemory");
}
static VkResult map_memory(VkDevice d,VkDeviceMemory memory,VkDeviceSize offset,VkDeviceSize size,VkMemoryMapFlags flags,void **out) {
    (void)d;(void)memory;assert(!offset && size==VK_WHOLE_SIZE && !flags);
    *out=null_map ? 0 : atlas_map;return result_for("vkMapMemory");
}
static VkResult create_sampler(VkDevice d,const VkSamplerCreateInfo *ci,const VkAllocationCallbacks *a,VkSampler *out) {
    (void)d;assert(!a && sampler_count<3 && ci->minFilter==ci->magFilter && !ci->maxLod);
    assert(ci->addressModeU==VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE && ci->addressModeV==ci->addressModeU);
    samplers[sampler_count]=*ci;*out=sampler_handles[sampler_count]=H(VkSampler,0x1d000+sampler_count*0x100);
    ++sampler_count;return result_for("vkCreateSampler");
}
static VkResult create_pool(VkDevice d,const VkDescriptorPoolCreateInfo *ci,const VkAllocationCallbacks *a,VkDescriptorPool *out) {
    (void)d;assert(!a && ci->maxSets==expected_n+1 && ci->poolSizeCount==1);
    assert(ci->pPoolSizes[0].type==VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        && ci->pPoolSizes[0].descriptorCount==expected_n+1);
    pool_capacity=ci->maxSets;*out=H(VkDescriptorPool,0x1e000);return result_for("vkCreateDescriptorPool");
}
static VkResult allocate_sets(VkDevice d,const VkDescriptorSetAllocateInfo *ci,VkDescriptorSet *out) {
    (void)d;assert(ci->descriptorPool==H(VkDescriptorPool,0x1e000) && ci->descriptorSetCount && set_count+ci->descriptorSetCount<=9);
    for(unsigned i=0;i<ci->descriptorSetCount;i++) {
        assert(ci->pSetLayouts[i]==H(VkDescriptorSetLayout,0x17000));
        out[i]=sets[set_count]=H(VkDescriptorSet,0x1f000+set_count*0x100);++set_count;
    }
    return result_for("vkAllocateDescriptorSets");
}
static unsigned set_index(VkDescriptorSet set) {
    for(unsigned i=0;i<set_count;i++) if(sets[i]==set) return i;
    assert(0);return 0;
}
static void update_sets(VkDevice d,uint32_t n,const VkWriteDescriptorSet *w,uint32_t nc,const VkCopyDescriptorSet *c) {
    (void)d;assert(!nc && !c);
    for(unsigned i=0;i<n;i++) {
        assert(!w[i].dstBinding && !w[i].dstArrayElement && w[i].descriptorCount==1
            && w[i].descriptorType==VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        unsigned index=set_index(w[i].dstSet);assert(!set_written[index]++);
        set_images[index]=*w[i].pImageInfo;assert(set_images[index].imageLayout==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);++writes;
    }
}
static void barrier(VkCommandBuffer cmd,VkPipelineStageFlags src,VkPipelineStageFlags dst,VkDependencyFlags flags,
    uint32_t nmem,const VkMemoryBarrier *mem,uint32_t nbuf,const VkBufferMemoryBarrier *buf,uint32_t nimage,const VkImageMemoryBarrier *image) {
    (void)src;(void)dst;assert(cmd==H(VkCommandBuffer,0x21000) && !flags && !nmem && !mem && !nbuf && !buf && nimage==1);
    assert(image->image==H(VkImage,0x1a000));++barriers;
}
static void copy_atlas(VkCommandBuffer cmd,VkBuffer buffer,VkImage image,VkImageLayout layout,uint32_t n,const VkBufferImageCopy *regions) {
    assert(cmd==H(VkCommandBuffer,0x21000) && buffer==H(VkBuffer,0x1b000) && image==H(VkImage,0x1a000));
    assert(layout==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && n==1 && regions->imageExtent.width==OVL_ATLAS_W);++copies;
}
static void begin_pass(VkCommandBuffer cmd,const VkRenderPassBeginInfo *ci,VkSubpassContents contents) {
    assert(cmd==H(VkCommandBuffer,0x21000) && contents==VK_SUBPASS_CONTENTS_INLINE && !begins && !ends);
    assert(ci->renderPass==H(VkRenderPass,0x3000) && ci->framebuffer==framebuffers[expected_index]);
    assert(ci->renderArea.extent.width==expected_w && ci->renderArea.extent.height==expected_h);++begins;
}
static void end_pass(VkCommandBuffer cmd) {assert(cmd==H(VkCommandBuffer,0x21000) && begins==1 && !ends);++ends;}
static void bind_pipeline(VkCommandBuffer cmd,VkPipelineBindPoint point,VkPipeline pipeline) {
    assert(cmd==H(VkCommandBuffer,0x21000) && begins && !ends && point==VK_PIPELINE_BIND_POINT_GRAPHICS && pipeline==H(VkPipeline,0x19000));++pipelines;
}
static void bind_sets(VkCommandBuffer cmd,VkPipelineBindPoint point,VkPipelineLayout layout,uint32_t first,uint32_t n,const VkDescriptorSet *set,uint32_t offsets,const uint32_t *dynamic) {
    assert(cmd==H(VkCommandBuffer,0x21000) && begins && !ends && pipelines && point==VK_PIPELINE_BIND_POINT_GRAPHICS);
    assert(layout==H(VkPipelineLayout,0x18000) && !first && n==1 && !offsets && !dynamic);
    set_index(set[0]);bound_set=set[0];++binds;
}
static void push(VkCommandBuffer cmd,VkPipelineLayout layout,VkShaderStageFlags stages,uint32_t offset,uint32_t size,const void *data) {
    assert(cmd==H(VkCommandBuffer,0x21000) && begins && !ends && bound_set);
    assert(layout==H(VkPipelineLayout,0x18000) && stages==VK_SHADER_STAGE_VERTEX_BIT && !offset && size==sizeof pushed);
    memcpy(&pushed,data,size);++pushes;
}
static void draw(VkCommandBuffer cmd,uint32_t vertices,uint32_t instances,uint32_t first,uint32_t first_instance) {
    assert(cmd==H(VkCommandBuffer,0x21000) && vertices==6 && instances==1 && !first && !first_instance);
    assert(pushes==draw_count+1 && begins && !ends && draw_count<64);
    draws[draw_count]=pushed;draw_sets[draw_count++]=bound_set;
}
static void *lookup(void *instance,const char *name);
static PFN_vkVoidFunction get_device_proc(VkDevice device,const char *name) {return (PFN_vkVoidFunction)lookup(device,name);}
static void *lookup(void *instance,const char *name) {
    assert(instance);++lookups;if(missing && !strcmp(missing,name)) return 0;
    if(!strcmp(name,"vkGetPhysicalDeviceProperties")) ++property_lookups;
#define METHOD(name_,fn_) if(!strcmp(name,name_)) return (void*)(fn_)
    METHOD("vkGetDeviceProcAddr",get_device_proc);METHOD("vkGetPhysicalDeviceMemoryProperties",memory_properties);
    METHOD("vkGetPhysicalDeviceProperties",physical_properties);
    METHOD("vkCreateRenderPass",create_rp);METHOD("vkCreateImageView",create_view);METHOD("vkCreateFramebuffer",create_fb);
    METHOD("vkCreateShaderModule",create_shader);METHOD("vkCreateDescriptorSetLayout",create_dsl);METHOD("vkCreatePipelineLayout",create_pl);
    METHOD("vkCreateGraphicsPipelines",create_pipeline);METHOD("vkCreateImage",create_image);METHOD("vkGetImageMemoryRequirements",image_requirements);
    METHOD("vkCreateBuffer",create_buffer);METHOD("vkGetBufferMemoryRequirements",buffer_requirements);METHOD("vkAllocateMemory",allocate_memory);
    METHOD("vkBindImageMemory",bind_image);METHOD("vkBindBufferMemory",bind_buffer);METHOD("vkMapMemory",map_memory);
    METHOD("vkCreateSampler",create_sampler);METHOD("vkCreateDescriptorPool",create_pool);METHOD("vkAllocateDescriptorSets",allocate_sets);
    METHOD("vkUpdateDescriptorSets",update_sets);METHOD("vkCmdPipelineBarrier",barrier);METHOD("vkCmdCopyBufferToImage",copy_atlas);
    METHOD("vkCmdBeginRenderPass",begin_pass);METHOD("vkCmdEndRenderPass",end_pass);METHOD("vkCmdBindPipeline",bind_pipeline);
    METHOD("vkCmdBindDescriptorSets",bind_sets);METHOD("vkCmdPushConstants",push);METHOD("vkCmdDraw",draw);
#undef METHOD
    assert(0);return 0;
}
static void reset_record(void) {begins=ends=pipelines=binds=pushes=draw_count=barriers=copies=0;bound_set=VK_NULL_HANDLE;}
static void reset(unsigned n) {
    s_ready=s_uploaded=0;expected_n=n;expected_w=2400;expected_h=1080;
    failure=missing=0;failure_at=1;failure_hits=0;null_map=0;
    proxy_mutable=1;property_vendor=0x5143u;property_device=0x06050002u;property_driver=0x801f6000u;
    property_queries=property_lookups=0;
    api_calls=lookups=view_count=sampler_count=set_count=writes=fb_count=rp_count=shader_count=pool_capacity=0;
    memset(views,0,sizeof views);memset(set_written,0,sizeof set_written);memset(set_images,0,sizeof set_images);
    last_log[0]=0;reset_record();
    for(unsigned i=0;i<8;i++) {window_images[i]=(void*)(uintptr_t)(0x5000+i*0x100);proxy_images[i]=(void*)(uintptr_t)(0x7000+i*0x100);}
}
static int initialize(void) {
    void *windows_before[8],*proxies_before[8];
    memcpy(windows_before,window_images,sizeof windows_before);memcpy(proxies_before,proxy_images,sizeof proxies_before);
    int ready=ovl_init(lookup,(void*)0x300,(void*)0x2000,(void*)0x1000,0,expected_w,expected_h,VK_FORMAT_R8G8B8A8_SRGB,expected_n,window_images,proxy_images,proxy_mutable);
    assert(!memcmp(windows_before,window_images,sizeof windows_before) && !memcmp(proxies_before,proxy_images,sizeof proxies_before));
    return ready;
}
static VkDescriptorSet descriptor_for(VkImage image,VkFilter filter) {
    unsigned found=0;VkDescriptorSet answer=VK_NULL_HANDLE;
    for(unsigned i=0;i<set_count;i++) for(unsigned v=0;v<view_count;v++) if(set_images[i].imageView==view_handles[v] && views[v].image==image) {
        unsigned sampler=3;for(unsigned k=0;k<sampler_count;k++) if(sampler_handles[k]==set_images[i].sampler) sampler=k;
        assert(sampler<3 && samplers[sampler].magFilter==filter);++found;answer=sets[i];
    }
    assert(found==1);return answer;
}
static void check_creation(void) {
    assert(ovl_ready() && rp_count==1 && fb_count==expected_n && view_count==2*expected_n+1
        && sampler_count==2 && set_count==expected_n+1 && writes==set_count && pool_capacity==set_count && shader_count==2);
    assert(!property_queries && !property_lookups);
    for(unsigned i=0;i<expected_n;i++) {
        unsigned found=0;for(unsigned v=0;v<view_count;v++) if(views[v].image==(VkImage)proxy_images[i]) {
            assert(views[v].format==VK_FORMAT_B8G8R8A8_SRGB);++found;
        }
        assert(found==1);descriptor_for((VkImage)proxy_images[i],VK_FILTER_NEAREST);
    }
    descriptor_for(H(VkImage,0x1a000),VK_FILTER_LINEAR);
}
static void record(unsigned index,const struct ovl_quad *q,int nq,unsigned wanted) {
    reset_record();expected_index=index;
    unsigned old_views=view_count,old_writes=writes,old_calls=api_calls;
    VkDescriptorImageInfo before[9];memcpy(before,set_images,sizeof before);
    ovl_record((void*)0x21000,index,q,nq);
    assert(begins==1 && ends==1 && draw_count==wanted && pushes==wanted && pipelines==1);
    assert(view_count==old_views && writes==old_writes && api_calls==old_calls && !memcmp(before,set_images,sizeof before));
    struct ovl_quad full={-1,-1,1,1,0,0,1,1,1,1,1,2};
    assert(!memcmp(&draws[0],&full,sizeof full) && draw_sets[0]==descriptor_for((VkImage)proxy_images[index],VK_FILTER_NEAREST));
    for(unsigned i=1;i<wanted;i++) assert(!memcmp(&draws[i],&q[i-1],sizeof full)
        && draw_sets[i]==descriptor_for(H(VkImage,0x1a000),VK_FILTER_LINEAR));
}
int main(void) {
    reset(4);assert(initialize());check_creation();unsigned created=api_calls;
    unsigned initialized_lookups=lookups;
    assert(initialize() && api_calls==created && lookups==initialized_lookups && !property_queries && !property_lookups);
    record(0,0,0,1);assert(barriers==2 && copies==1);
    struct ovl_quad q[2]={{-.8f,-.7f,-.6f,-.5f,.1f,.2f,.3f,.4f,.5f,.6f,.7f,.8f},{.2f,.3f,.4f,.5f,.6f,.7f,.8f,.9f,1,.2f,.3f,-1}};
    for(unsigned i=0;i<4;i++) {record(i,q,2,3);assert(!barriers && !copies && binds==2);}
    record(2,0,2,1);record(1,q,-1,1);
    struct ovl_quad many[64];for(unsigned i=0;i<64;i++) many[i]=q[i&1];
    record(3,many,64,49);assert(binds==2);
    reset_record();ovl_record(0,0,q,2);ovl_record((void*)0x21000,4,q,2);assert(!begins && !draw_count);
    s_ready=0;ovl_record((void*)0x21000,0,q,2);assert(!begins && !draw_count);
    for(unsigned n=1;n<=8;n++) {reset(n);assert(initialize());check_creation();record(n-1,0,0,1);}
    const uint32_t hardware[][3]={{0x5143u,0x06050002u,0x801f6000u},{0x5143u,0x06010800u,0x801f6000u},
        {0x5143u,0x06050002u,0x801f6001u},{0x5143u,0x06010800u,0x801f6001u},{0x13b5u,0,0},{0,0,0}};
    for(unsigned which=0;which<sizeof hardware/sizeof *hardware;which++)for(int mutable=0;mutable<=1;mutable++) {
        reset(4);proxy_mutable=mutable;
        property_vendor=hardware[which][0];property_device=hardware[which][1];property_driver=hardware[which][2];
        assert(initialize());check_creation();record(3,q,2,3);
    }
    reset(1);expected_w=expected_h=16384;assert(initialize());check_creation();record(0,0,0,1);
    for(unsigned which=0;which<16;which++) {
        reset(4);void *instance=(void*)0x300,*physical=(void*)0x2000,*device=(void*)0x1000;unsigned w=2400,h=1080,n=4;
        void *const *images=window_images,*proxies=proxy_images;void *(*get)(void*,const char*)=lookup;
        switch(which) {case 0:get=0;break;case 1:instance=0;break;case 2:physical=0;break;case 3:device=0;break;
        case 4:w=0;break;case 5:h=0;break;case 6:n=0;break;case 7:n=9;break;case 8:images=0;break;case 9:proxies=0;break;
        case 10:window_images[2]=0;break;case 11:proxy_images[2]=0;break;case 12:proxy_images[2]=window_images[2];break;
        case 13:proxy_images[2]=window_images[0];break;case 14:w=16385;break;case 15:h=16385;break;}
        assert(!ovl_init(get,instance,physical,device,0,w,h,VK_FORMAT_R8G8B8A8_SRGB,n,images,proxies,proxy_mutable));
        assert(!ovl_ready() && !api_calls && !lookups && !property_queries && !property_lookups);
    }
    const char *failures[]={"vkCreateRenderPass","vkCreateImageView","vkCreateFramebuffer","vkCreateImage","vkAllocateMemory",
        "vkBindImageMemory","vkCreateBuffer","vkBindBufferMemory","vkMapMemory","vkCreateSampler","vkCreateDescriptorSetLayout",
        "vkCreateDescriptorPool","vkAllocateDescriptorSets","vkCreatePipelineLayout","vkCreateShaderModule","vkCreateGraphicsPipelines"};
    for(unsigned i=0;i<sizeof failures/sizeof *failures;i++) {
        reset(4);failure=failures[i];assert(!initialize() && !ovl_ready() && failure_hits==1);
        assert(strstr(last_log,"-11"));reset_record();ovl_record((void*)0x21000,0,q,2);assert(!begins);
    }
    for(unsigned i=0;i<4;i++) {
        reset(4);failure=i==0 ? "vkCreateSampler" : i==1 ? "vkCreateShaderModule" : "vkCreateImageView";
        failure_at=i==3 ? 8 : 2;assert(!initialize() && !ovl_ready());
    }
    reset(4);null_map=1;assert(!initialize() && !ovl_ready());
    reset(4);missing="vkCmdDraw";assert(!initialize() && !ovl_ready() && !api_calls);
    assert(!property_queries && !property_lookups);
    reset(4);missing="vkGetPhysicalDeviceProperties";assert(initialize());check_creation();record(0,q,2,3);
    puts("PASS: actual sampled overlay C, identity scene/window/atlas components on Adreno618/650 and other GPUs with either mutable flag, no GPU identity query; NEAREST scene/LINEAR atlas, immutable descriptors, fullscreen-first HUD, n=1..8, guards/native failures, no per-frame creation/update");
    return 0;
}
