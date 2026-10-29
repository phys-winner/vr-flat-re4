#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <vulkan/vulkan_core.h>

_Static_assert(sizeof(VkImageCreateInfo)==88,"native image CI ABI");
_Static_assert(offsetof(VkImageCreateInfo,pNext)==8,"native image pNext ABI");
_Static_assert(offsetof(VkImageCreateInfo,flags)==16,"native image flags ABI");
_Static_assert(sizeof(VkImageFormatListCreateInfo)==32,"native list ABI");
_Static_assert(offsetof(VkImageFormatListCreateInfo,viewFormatCount)==16,"native list count ABI");
_Static_assert(offsetof(VkImageFormatListCreateInfo,pViewFormats)==24,"native list array ABI");
#define PTR(n) ((void*)(uintptr_t)(n))
#define LOGF(...) ((void)0)
static volatile int g_comp_format_list;
static void* g_vk_instance,*g_phys_dev,*g_window_sc;
static void* g_own_imgs[8];
static unsigned g_sc_fmt_real;
static volatile long g_csc_calls;
static void* (*r_gipa)(void*,const char*);
static int (*r_vkQueueSubmit2)(void*,unsigned,const void*,void*);
static int swidx_of_image(void* image) {(void)image;return -1;}
#include "../re4fix_comp.h"

static unsigned image_calls,handle_calls,overlay_calls,semaphore_calls,rebind_calls;
static unsigned semaphore_error_at,semaphore_null_at;
static int rebind_error;
static int expected_mutable,image_error,overlay_error;
static int create_image(void* dev,const void* info,const void* allocator,void** out) {
    assert(dev==PTR(0x111) && !allocator && out);image_calls++;
    const VkImageCreateInfo* ci=info;
    assert(ci->sType==VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO && ci->format==(expected_mutable ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_B8G8R8A8_SRGB));
    assert(ci->imageType==VK_IMAGE_TYPE_2D && ci->extent.width==2400 && ci->extent.height==1080 && ci->extent.depth==1);
    assert(ci->mipLevels==1 && ci->arrayLayers==1 && ci->samples==VK_SAMPLE_COUNT_1_BIT && ci->tiling==VK_IMAGE_TILING_OPTIMAL);
    assert(ci->usage==(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|
        VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT));
    assert(ci->sharingMode==VK_SHARING_MODE_EXCLUSIVE && !ci->queueFamilyIndexCount && !ci->pQueueFamilyIndices && !ci->initialLayout);
    if(expected_mutable) {
        assert(ci->flags==VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT && ci->pNext);
        assert(((uintptr_t)ci->pNext&7)==0);
        const VkImageFormatListCreateInfo* list=ci->pNext;
        assert(list->sType==VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO && !list->pNext && list->viewFormatCount==2);
        assert(list->pViewFormats && list->pViewFormats[0]==VK_FORMAT_B8G8R8A8_UNORM && list->pViewFormats[1]==VK_FORMAT_B8G8R8A8_SRGB);
    } else assert(!ci->flags && !ci->pNext);
    if(image_error)return image_error;
    *out=PTR(0x200+image_calls);return VK_SUCCESS;
}
static int create_handle(void* dev,const void* ci,const void* allocator,void** out) {
    assert(dev==PTR(0x111) && ci && !allocator && out);*out=PTR(0x400+handle_calls++);return 0;
}
static int create_semaphore(void* dev,const void* info,const void* allocator,void** out) {
    const VkSemaphoreCreateInfo* ci=info;
    assert(ci && ci->sType==VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO && !ci->pNext && !ci->flags);
    semaphore_calls++;
    if(semaphore_calls==semaphore_error_at)return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    if(semaphore_calls==semaphore_null_at){*out=0;return 0;}
    return create_handle(dev,info,allocator,out);
}
static void mem_properties(void* physical,void* out) {
    assert(physical==g_phys_dev);memset(out,0,528);
    *(unsigned*)out=1;*(unsigned*)((char*)out+4)=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
}
static void mem_requirements(void* dev,void* image,void* out) {
    assert(dev==PTR(0x111) && image);memset(out,0,24);
    *(uint64_t*)out=4096;*(uint64_t*)((char*)out+8)=4096;*(unsigned*)((char*)out+16)=1;
}
static int bind_memory(void* dev,void* image,void* memory,unsigned long offset) {
    assert(dev==PTR(0x111) && image && memory && !offset);return 0;
}
static int command_buffers(void* dev,const void* ci,void** out) {
    assert(dev==PTR(0x111) && ci && out);unsigned n=*(const unsigned*)((const char*)ci+28);
    for(unsigned i=0;i<n;i++)out[i]=PTR(0x600+i);return 0;
}
int ovl_init(void* (*gipa)(void*,const char*),void* instance,void* physical,void* dev,unsigned family,
    unsigned width,unsigned height,unsigned fmt,unsigned n,void* const* images,void* const* proxies,int mutable_proxy) {
    (void)gipa;assert(instance==g_vk_instance && physical==g_phys_dev && dev==PTR(0x111) && !family);
    assert(width==2400 && height==1080 && fmt==43 && n==2 && images==g_own_imgs && proxies==g_comp_img);
    assert(mutable_proxy==expected_mutable);
    assert(proxies[0]==PTR(0x201) && proxies[1]==PTR(0x202));overlay_calls++;return !overlay_error;
}
int ovl_rebind(void* const* images,unsigned n) {
    assert(images==g_own_imgs && n==2 && g_comp_on && !g_comp_disable);rebind_calls++;
    return !rebind_error;
}
static void factory_case(int capability,int error) {
    g_comp_on=g_comp_disable=g_comp_mutable=0;g_comp_format_list=capability;
    g_comp_n=0;g_comp_dev=0;g_comp_w=2400;g_comp_h=1080;g_comp_usage_extra=VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    g_vk_instance=PTR(0x777);g_phys_dev=PTR(0x888);g_sc_fmt_real=43;
    g_csc_calls=1;
    g_own_imgs[0]=PTR(0xa10);g_own_imgs[1]=PTR(0xa11);
    image_calls=handle_calls=overlay_calls=0;expected_mutable=capability;image_error=error;
    semaphore_calls=rebind_calls=semaphore_error_at=semaphore_null_at=0;rebind_error=0;
    g_comp_fail=0;g_comp_last_err=0;
    c_CreateImage=create_image;c_CreateCommandPool=create_handle;
    c_GetPhysMemProps=mem_properties;c_GetImageMemReq=mem_requirements;c_AllocateMemory=create_handle;
    c_BindImageMemory=bind_memory;c_CreateSemaphore=create_semaphore;c_CreateFence=create_handle;c_AllocCmdBufs=command_buffers;
    comp_create(PTR(0x111),2);
    assert(g_comp_mutable==capability);
    if(error)assert(g_comp_disable && !g_comp_on && image_calls==1 && !overlay_calls);
    else {
        assert(g_comp_on==!overlay_error && g_comp_disable==!!overlay_error);
        assert(g_comp_n==2 && g_comp_dev==PTR(0x111) && image_calls==2 && overlay_calls==1);
        assert(g_comp_created_w==2400 && g_comp_created_h==1080);
        assert(!memcmp(g_comp_window_img,g_own_imgs,2*sizeof(void*)));
    }
}
static void replacement_cases(void) {
    factory_case(1,0);
    void *old_sem[2],*proxies[2],*commands[2],*fences[2];
    memcpy(old_sem,g_comp_sem,sizeof old_sem);memcpy(proxies,g_comp_img,sizeof proxies);
    memcpy(commands,g_comp_cmd,sizeof commands);memcpy(fences,g_comp_fence,sizeof fences);
    comp_create(PTR(0x111),2);assert(semaphore_calls==2 && !rebind_calls && image_calls==2);
    g_comp_init[0]=g_comp_drawn[0]=1;g_own_imgs[0]=PTR(0xa20);
    comp_create(PTR(0x111),2);
    assert(g_comp_on && !g_comp_disable && semaphore_calls==4 && rebind_calls==1 && image_calls==2);
    assert(g_comp_sem[0]!=old_sem[0] && g_comp_sem[1]!=old_sem[1]);
    assert(!memcmp(g_comp_img,proxies,sizeof proxies) && !memcmp(g_comp_cmd,commands,sizeof commands)
        && !memcmp(g_comp_fence,fences,sizeof fences) && g_comp_init[0] && g_comp_drawn[0]);
    assert(!memcmp(g_comp_window_img,g_own_imgs,2*sizeof(void*)));
    comp_create(PTR(0x111),2);assert(semaphore_calls==4 && rebind_calls==1);
    memcpy(old_sem,g_comp_sem,sizeof old_sem);
    g_own_imgs[1]=PTR(0xa20);g_own_imgs[0]=PTR(0xa11);comp_create(PTR(0x111),2);
    assert(semaphore_calls==6 && rebind_calls==2 && g_comp_sem[0]!=old_sem[0] && g_comp_sem[1]!=old_sem[1]);
    factory_case(1,0);memcpy(old_sem,g_comp_sem,sizeof old_sem);g_csc_calls++;
    comp_create(PTR(0x111),2);
    assert(semaphore_calls==4 && rebind_calls==1 && g_comp_sem[0]!=old_sem[0] && g_comp_sem[1]!=old_sem[1]);
    comp_create(PTR(0x111),2);assert(semaphore_calls==4 && rebind_calls==1);
    for(unsigned failure=0;failure<4;failure++) {
        factory_case(1,0);
        if(failure==0)g_comp_w++;if(failure==1)g_comp_h++;
        comp_create(failure==2 ? PTR(0x112) : PTR(0x111),failure==3 ? 3 : 2);
        assert(!g_comp_on && g_comp_disable && semaphore_calls==2 && !rebind_calls && image_calls==2);
    }
    for(unsigned failure=1;failure<=3;failure++) {
        factory_case(1,0);memcpy(old_sem,g_comp_sem,sizeof old_sem);
        if(failure<3)semaphore_error_at=semaphore_calls+failure;
        else semaphore_null_at=semaphore_calls+1;
        g_own_imgs[0]=PTR(0xa20);comp_create(PTR(0x111),2);
        assert(!g_comp_on && g_comp_disable && g_comp_fail==1 && !rebind_calls);
        assert(g_comp_last_err==(failure<3 ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_ERROR_INITIALIZATION_FAILED));
        assert(!memcmp(g_comp_sem,old_sem,sizeof old_sem) && g_comp_window_img[0]==PTR(0xa10));
        unsigned calls=semaphore_calls;comp_create(PTR(0x111),2);assert(semaphore_calls==calls);
    }
    factory_case(1,0);memcpy(old_sem,g_comp_sem,sizeof old_sem);rebind_error=1;
    g_own_imgs[0]=PTR(0xa20);comp_create(PTR(0x111),2);
    assert(!g_comp_on && g_comp_disable && g_comp_fail==1 && g_comp_last_err==VK_ERROR_INITIALIZATION_FAILED);
    assert(semaphore_calls==4 && rebind_calls==1 && !memcmp(g_comp_sem,old_sem,sizeof old_sem));
    assert(g_comp_window_img[0]==PTR(0xa10));
    puts("PASS: unchanged WSI query no-op; replaced/reordered/reused-handle generations use fresh present semaphores without proxy/command/fence recreation; created extent/device/count guards and atomic publication; semaphore/overlay failures disable compositor");
}
static unsigned sequence,fail_at,quad_count,wait_count,selected_index;
static int wait_fences(void* dev,unsigned n,void* const* fences,unsigned all,unsigned long timeout) {
    assert(sequence++==0 && dev==g_comp_dev && n==1 && fences[0]==g_comp_fence[selected_index] && all==1 && timeout==2000000000ul);
    return fail_at==1 ? VK_TIMEOUT : 0;
}
static int reset_cmd(void* cmd,unsigned flags) {
    assert(sequence++==1 && cmd==g_comp_cmd[selected_index] && !flags);
    return fail_at==2 ? VK_ERROR_DEVICE_LOST : 0;
}
static int begin_cmd(void* cmd,const void* info) {
    const VkCommandBufferBeginInfo* ci=info;
    assert(sequence++==2 && cmd==g_comp_cmd[selected_index] && ci->sType==VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
    assert(ci->flags==VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT && !ci->pNext && !ci->pInheritanceInfo);
    return fail_at==3 ? VK_ERROR_DEVICE_LOST : 0;
}
static void barrier(void* cmd,unsigned src,unsigned dst,unsigned flags,unsigned nm,const void* memory,unsigned nb,const void* buffers,unsigned ni,const void* info) {
    assert(cmd==g_comp_cmd[selected_index] && !flags && !nm && !memory && !nb && !buffers && ni==1);
    const VkImageMemoryBarrier* b=info;
    assert(b->sType==VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER && !b->pNext);
    assert(b->srcQueueFamilyIndex==VK_QUEUE_FAMILY_IGNORED && b->dstQueueFamilyIndex==VK_QUEUE_FAMILY_IGNORED);
    assert(b->subresourceRange.aspectMask==VK_IMAGE_ASPECT_COLOR_BIT && !b->subresourceRange.baseMipLevel
        && b->subresourceRange.levelCount==1 && !b->subresourceRange.baseArrayLayer && b->subresourceRange.layerCount==1);
    if(sequence==3) {
        assert((void*)b->image==g_comp_img[selected_index] && b->oldLayout==VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && b->newLayout==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        assert(b->srcAccessMask==VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT && b->dstAccessMask==VK_ACCESS_SHADER_READ_BIT);
        assert(src==VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT && dst==VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    } else if(sequence==4) {
        assert((void*)b->image==g_own_imgs[selected_index] && b->oldLayout==VK_IMAGE_LAYOUT_UNDEFINED && b->newLayout==VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        assert(!b->srcAccessMask && b->dstAccessMask==VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        assert(src==VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT && dst==VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    } else {
        assert(sequence==6 && (void*)b->image==g_comp_img[selected_index] && b->oldLayout==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && b->newLayout==VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        assert(b->srcAccessMask==VK_ACCESS_SHADER_READ_BIT && b->dstAccessMask==(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT));
        assert(src==VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT && dst==VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }
    sequence++;
}
int touch_overlay_quads(struct ovl_quad* q,int maximum) {assert(sequence==5 && q && maximum==48);return (int)quad_count;}
void ovl_record(void* cmd,unsigned index,const struct ovl_quad* q,int n) {
    assert(sequence++==5 && cmd==g_comp_cmd[selected_index] && index==selected_index && q && n==(int)quad_count);
}
static int end_cmd(void* cmd) {assert(sequence++==7 && cmd==g_comp_cmd[selected_index]);return fail_at==4 ? VK_ERROR_DEVICE_LOST : 0;}
static int reset_fences(void* dev,unsigned n,void* const* fences) {
    assert(sequence++==8 && dev==g_comp_dev && n==1 && fences[0]==g_comp_fence[selected_index]);
    return fail_at==5 ? VK_ERROR_DEVICE_LOST : 0;
}
static int submit(void* queue,unsigned n,const void* info,void* fence) {
    assert(sequence++==9 && queue==PTR(0xa01) && n==1 && fence==g_comp_fence[selected_index]);
    const VkSubmitInfo* ci=info;
    assert(ci->sType==VK_STRUCTURE_TYPE_SUBMIT_INFO && !ci->pNext && ci->waitSemaphoreCount==wait_count);
    for(unsigned i=0;i<wait_count;i++)assert((void*)ci->pWaitSemaphores[i]==PTR(0xb00+i) && ci->pWaitDstStageMask[i]==VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    assert(ci->commandBufferCount==1 && (void*)ci->pCommandBuffers[0]==g_comp_cmd[selected_index]);
    assert(ci->signalSemaphoreCount==1 && (void*)ci->pSignalSemaphores[0]==g_comp_sem[selected_index]);
    return fail_at==6 ? VK_ERROR_DEVICE_LOST : 0;
}
static void presentation_case(unsigned index,unsigned waits,unsigned quads,unsigned error) {
    factory_case(1,0);selected_index=index;wait_count=waits;quad_count=quads;fail_at=error;sequence=0;
    g_comp_blits=g_comp_fail=g_comp_skip=0;g_comp_init[index]=g_comp_drawn[index]=1;
    g_window_sc=PTR(0xa00);g_own_imgs[0]=PTR(0xa10);g_own_imgs[1]=PTR(0xa11);
    c_WaitFences=wait_fences;c_ResetCmd=reset_cmd;c_BeginCmd=begin_cmd;c_CmdBarrier=barrier;
    c_EndCmd=end_cmd;c_ResetFences=reset_fences;r_vkQueueSubmit2=submit;
    VkSemaphore semaphores[17];for(unsigned i=0;i<17;i++)semaphores[i]=(VkSemaphore)PTR(0xb00+i);
    VkSwapchainKHR swapchain=(VkSwapchainKHR)g_window_sc;int result=77;
    VkPresentInfoKHR ci={VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    ci.waitSemaphoreCount=waits;ci.pWaitSemaphores=semaphores;ci.swapchainCount=1;
    if(error==7)ci.pWaitSemaphores=0;
    ci.pSwapchains=&swapchain;ci.pImageIndices=&index;ci.pResults=(VkResult*)&result;
    VkPresentInfoKHR before=ci;_Alignas(8) unsigned char copied[64];memset(copied,0xa5,sizeof copied);void* selected=0;
    const void* output=comp_present(PTR(0xa01),&ci,copied,&selected);
    assert(!memcmp(&ci,&before,sizeof ci) && result==77);
    if(!error && waits<=16) {
        assert(output==copied && sequence==10 && g_comp_blits==1 && !g_comp_fail && g_comp_on);
        const VkPresentInfoKHR* published=output;
        assert(published->waitSemaphoreCount==1 && published->pWaitSemaphores==(VkSemaphore*)&selected && selected==g_comp_sem[index]);
        before.waitSemaphoreCount=1;before.pWaitSemaphores=(VkSemaphore*)&selected;
        assert(!memcmp(published,&before,sizeof before));
    } else {
        assert(output==&ci && !selected && !g_comp_blits);
        for(unsigned i=0;i<sizeof copied;i++)assert(copied[i]==0xa5);
        if(waits>16 || error==7)assert(!sequence && !g_comp_fail);
        else {
            unsigned expected[]={0,1,2,3,8,9,10};assert(sequence==expected[error] && g_comp_fail==1);
            assert(g_comp_on==(error!=6) && g_comp_disable==(error==6));
        }
    }
}
int main(void) {
    factory_case(0,0);factory_case(1,0);factory_case(1,VK_ERROR_FORMAT_NOT_SUPPORTED);
    overlay_error=1;factory_case(1,0);overlay_error=0;
    replacement_cases();
    for(unsigned waits=0;waits<=16;waits++)presentation_case(waits%2,waits,waits,0);
    presentation_case(1,16,48,0);presentation_case(0,17,2,0);presentation_case(0,2,2,7);
    for(unsigned error=1;error<=6;error++)presentation_case(0,2,2,error);
    puts("PASS: actual compositor factory; native SDK image/list ABI; extension0 immutable50/no pNext; extension1 native UE4 linear44 storage/mutable8 + exact aligned32 list44/50; usage/extent preserved and original image/overlay failure propagated");
    puts("PASS: actual sampled presentation; three exact SDK image/layout/access/stage barriers, overlay before restoration, all0..16 semaphore dependencies, zero-HUD frame, borrowed present immutable; wait/reset/begin/end/fence/submit failures never publish output and fence resets only after complete recording");
}
