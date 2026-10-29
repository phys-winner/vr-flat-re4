#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan_core.h>

_Static_assert(sizeof(VkImageMemoryBarrier)==72,"native barrier ABI");
_Static_assert(offsetof(VkImageMemoryBarrier,oldLayout)==24,"native layout ABI");
_Static_assert(offsetof(VkImageMemoryBarrier,image)==40,"native image ABI");
_Static_assert(sizeof(VkImageCopy)==68,"native copy region ABI");
_Static_assert(sizeof(VkImageBlit)==80,"native blit region ABI");
_Static_assert(sizeof(VkImageResolve)==68,"native resolve region ABI");
#define COMP_MAX 8
#define HANDLE(type,n) ((type)(uintptr_t)(n))
static int g_comp_on;
static unsigned g_comp_n;
static void* g_comp_img[COMP_MAX];
static volatile int g_comp_init[COMP_MAX],g_comp_drawn[COMP_MAX];
static unsigned long g_comp_pass_serial[COMP_MAX];
static long g_comp_inits;
#include "../re4fix_comp_images.h"

static VkCommandBuffer command=HANDLE(VkCommandBuffer,0xfedcba9876543210ULL);
static unsigned barrier_calls,copy_calls,blit_calls,resolve_calls;
static VkMemoryBarrier memories[2];
static VkBufferMemoryBarrier buffers[3];
static VkImageMemoryBarrier images[10],expected_images[10];
static VkPipelineStageFlags expected_source_stage,expected_destination_stage;
static VkDependencyFlags expected_flags;
static uint32_t expected_image_count;
static const VkImageMemoryBarrier* expected_image_pointer;
static int expect_copied;
static VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer c,VkPipelineStageFlags s,VkPipelineStageFlags d,VkDependencyFlags flags,
    uint32_t mc,const VkMemoryBarrier* m,uint32_t bc,const VkBufferMemoryBarrier* b,uint32_t ic,const VkImageMemoryBarrier* im) {
    assert(c==command && s==expected_source_stage && d==expected_destination_stage && flags==expected_flags);
    assert(mc==2 && m==memories && bc==3 && b==buffers && ic==expected_image_count);
    if (expect_copied) assert(im!=expected_image_pointer);
    else assert(im==expected_image_pointer);
    if (ic) assert(im && !memcmp(im,expected_images,ic*sizeof *im));
    barrier_calls++;
}

static VkImage source,destination;
static VkImageLayout source_layout,destination_layout;
static uint32_t region_count;
static const void* regions_pointer;
static VkImageCopy copies[2];
static VkImageBlit blits[2];
static VkImageResolve resolves[2];
static void arguments(VkCommandBuffer c,VkImage s,VkImageLayout sl,VkImage d,VkImageLayout dl,uint32_t count,const void* regions) {
    assert(c==command && s==source && sl==source_layout && d==destination && dl==destination_layout);
    assert(count==region_count && regions==regions_pointer);
}
static VKAPI_ATTR void VKAPI_CALL copy(VkCommandBuffer c,VkImage s,VkImageLayout sl,VkImage d,VkImageLayout dl,uint32_t count,const VkImageCopy* regions) {
    arguments(c,s,sl,d,dl,count,regions);copy_calls++;
}
static VKAPI_ATTR void VKAPI_CALL blit(VkCommandBuffer c,VkImage s,VkImageLayout sl,VkImage d,VkImageLayout dl,uint32_t count,const VkImageBlit* regions,VkFilter filter) {
    arguments(c,s,sl,d,dl,count,regions);assert(filter==VK_FILTER_LINEAR);blit_calls++;
}
static VKAPI_ATTR void VKAPI_CALL resolve(VkCommandBuffer c,VkImage s,VkImageLayout sl,VkImage d,VkImageLayout dl,uint32_t count,const VkImageResolve* regions) {
    arguments(c,s,sl,d,dl,count,regions);resolve_calls++;
}
static void reset_state(void) {
    g_comp_on=1;g_comp_n=COMP_MAX;g_comp_inits=0;
    memset((void*)g_comp_init,0,sizeof g_comp_init);memset((void*)g_comp_drawn,0,sizeof g_comp_drawn);
    memset(g_comp_pass_serial,0,sizeof g_comp_pass_serial);
    for(unsigned i=0;i<COMP_MAX;i++)g_comp_img[i]=HANDLE(void*,0xabcdef0100000000ULL+i);
}
static void barrier_input(unsigned count) {
    memset(images,0x39,sizeof images);
    for(unsigned i=0;i<10;i++) {
        images[i].sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        images[i].image=HANDLE(VkImage,0xabcdef0200000000ULL+i);
        images[i].oldLayout=VK_IMAGE_LAYOUT_GENERAL;images[i].newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }
    memcpy(expected_images,images,sizeof images);
    expected_source_stage=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT;
    expected_destination_stage=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    expected_flags=VK_DEPENDENCY_BY_REGION_BIT;
    expected_image_count=count;expected_image_pointer=images;expect_copied=0;
}
static void do_barriers(const VkImageMemoryBarrier* borrowed) {
    comp_native_barriers(command,expected_source_stage,expected_destination_stage,expected_flags,
        2,memories,3,buffers,expected_image_count,borrowed);
}
static void barrier_cases(void) {
    reset_state();barrier_input(10);
    for(unsigned i=0;i<COMP_MAX;i++)images[i].image=(VkImage)g_comp_img[i];
    images[0].oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    images[1].oldLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    images[2].newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    images[3].oldLayout=images[3].newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    images[7].image=VK_NULL_HANDLE;
    images[8].oldLayout=images[8].newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    memcpy(expected_images,images,sizeof images);
    expected_images[1].oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    expected_images[2].newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    expected_images[3].oldLayout=expected_images[3].newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkImageMemoryBarrier borrowed[10];memcpy(borrowed,images,sizeof images);
    VkMemoryBarrier memories_before[2];memcpy(memories_before,memories,sizeof memories);
    VkBufferMemoryBarrier buffers_before[3];memcpy(buffers_before,buffers,sizeof buffers);
    expect_copied=1;do_barriers(images);
    assert(barrier_calls==1 && g_comp_inits==7 && !memcmp(images,borrowed,sizeof images));
    assert(!memcmp(memories,memories_before,sizeof memories) && !memcmp(buffers,buffers_before,sizeof buffers));
    for(unsigned i=0;i<COMP_MAX;i++)assert(g_comp_init[i]==(i<7) && !g_comp_drawn[i] && !g_comp_pass_serial[i]);
    do_barriers(images);assert(barrier_calls==2 && g_comp_inits==7);
    barrier_input(1);images[0].image=(VkImage)g_comp_img[7];
    images[0].oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;images[0].newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    expected_images[0]=images[0];do_barriers(images);assert(g_comp_inits==8 && g_comp_init[7]);
    barrier_input(2);do_barriers(images);assert(barrier_calls==4 && g_comp_inits==8);
    barrier_input(0);do_barriers(images);assert(barrier_calls==5);
    expected_image_pointer=0;do_barriers(0);assert(barrier_calls==6);
    reset_state();barrier_input(1);images[0].image=(VkImage)g_comp_img[0];images[0].newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    expected_images[0]=images[0];g_comp_on=0;do_barriers(images);
    assert(barrier_calls==7 && !g_comp_inits && !g_comp_init[0]);
    puts("PASS: borrowed image/memory/buffer barriers immutable; proxy PRESENT maps both directions only; all stage/access/flags/queue/pNext/range fields preserved; native transition initializes once without claiming a write; offscreen, disabled and zero-count calls forward exactly once");
}
static void transfer_cases(void) {
    reset_state();source=HANDLE(VkImage,0x1234567800000001ULL);
    source_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;destination_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    memset(copies,0x41,sizeof copies);memset(blits,0x52,sizeof blits);memset(resolves,0x63,sizeof resolves);
    VkImageCopy copies_before[2];memcpy(copies_before,copies,sizeof copies);
    VkImageBlit blits_before[2];memcpy(blits_before,blits,sizeof blits);
    VkImageResolve resolves_before[2];memcpy(resolves_before,resolves,sizeof resolves);
    region_count=2;
    for(unsigned i=0;i<COMP_MAX;i++) {
        destination=(VkImage)g_comp_img[i];regions_pointer=copies;
        comp_native_copy(command,source,source_layout,destination,destination_layout,region_count,copies);
        assert(g_comp_drawn[i] && g_comp_pass_serial[i]==1 && !g_comp_init[i]);
        regions_pointer=blits;comp_native_blit(command,source,source_layout,destination,destination_layout,region_count,blits,VK_FILTER_LINEAR);
        assert(g_comp_pass_serial[i]==2);
        regions_pointer=resolves;comp_native_resolve(command,source,source_layout,destination,destination_layout,region_count,resolves);
        assert(g_comp_pass_serial[i]==3);
        comp_native_write(destination);assert(g_comp_pass_serial[i]==4);
    }
    assert(!memcmp(copies,copies_before,sizeof copies) && !memcmp(blits,blits_before,sizeof blits)
        && !memcmp(resolves,resolves_before,sizeof resolves) && !g_comp_inits);
    source=(VkImage)g_comp_img[0];destination=HANDLE(VkImage,0x1234567800000002ULL);regions_pointer=copies;
    comp_native_copy(command,source,source_layout,destination,destination_layout,region_count,copies);
    comp_native_write(destination);comp_native_write(VK_NULL_HANDLE);
    assert(g_comp_pass_serial[0]==4);
    destination=(VkImage)g_comp_img[0];region_count=0;regions_pointer=0;
    comp_native_copy(command,source,source_layout,destination,destination_layout,0,0);
    comp_native_blit(command,source,source_layout,destination,destination_layout,0,0,VK_FILTER_LINEAR);
    comp_native_resolve(command,source,source_layout,destination,destination_layout,0,0);
    assert(g_comp_pass_serial[0]==4);
    g_comp_on=0;region_count=2;regions_pointer=copies;
    comp_native_copy(command,source,source_layout,destination,destination_layout,region_count,copies);comp_native_write(destination);
    assert(g_comp_pass_serial[0]==4 && copy_calls==11 && blit_calls==9 && resolve_calls==9);
    puts("PASS: actual copy/blit/resolve preserve 64-bit handles, layouts, region pointers/counts/filter and borrowed bytes; each destination proxy write/clear counts once without inventing initialization; offscreen reads and disabled/zero writes leave provenance unchanged");
}
int main(void) {
    PFN_vkCmdPipelineBarrier barrier_abi=comp_native_barriers;
    PFN_vkCmdCopyImage copy_abi=comp_native_copy;
    PFN_vkCmdBlitImage blit_abi=comp_native_blit;
    PFN_vkCmdResolveImage resolve_abi=comp_native_resolve;
    assert(barrier_abi && copy_abi && blit_abi && resolve_abi);
    reset_state();
    assert(comp_image_proc(0,(void*)copy)==(void*)copy && !comp_image_proc("vkCmdCopyImage",0));
    assert(comp_image_proc("vkUnknown",(void*)copy)==(void*)copy);
    assert(comp_image_proc("vkCmdPipelineBarrier",(void*)barrier)==(void*)barrier_abi);
    assert(comp_image_proc("vkCmdCopyImage",(void*)copy)==(void*)copy_abi);
    assert(comp_image_proc("vkCmdBlitImage",(void*)blit)==(void*)blit_abi);
    assert(comp_image_proc("vkCmdResolveImage",(void*)resolve)==(void*)resolve_abi);
    assert(comp_image_proc("vkCmdPipelineBarrier",(void*)barrier_abi)==(void*)barrier_abi && comp_image_barrier==barrier);
    assert(comp_image_proc("vkCmdCopyImage",(void*)copy_abi)==(void*)copy_abi && comp_image_copy==copy);
    assert(comp_image_proc("vkCmdBlitImage",(void*)blit_abi)==(void*)blit_abi && comp_image_blit==blit);
    assert(comp_image_proc("vkCmdResolveImage",(void*)resolve_abi)==(void*)resolve_abi && comp_image_resolve==resolve);
    for(unsigned n=0;n<=COMP_MAX;n++) {
        g_comp_n=n;
        for(unsigned i=0;i<COMP_MAX;i++)assert(comp_proxy_index((VkImage)g_comp_img[i])==(i<n ? (int)i : -1));
        assert(comp_proxy_index(VK_NULL_HANDLE)==-1 && comp_proxy_index(HANDLE(VkImage,0xabcdef0200000000ULL))==-1);
    }
    g_comp_n=COMP_MAX+1;assert(comp_proxy_index(HANDLE(VkImage,0xabcdef0100000008ULL))==-1);
    g_comp_on=0;assert(comp_proxy_index((VkImage)g_comp_img[0])==-1);
    assert(!barrier_calls && !copy_calls && !blit_calls && !resolve_calls && !g_comp_inits);
    puts("PASS: exact Vulkan SDK function ABIs, all 0..8 proxy identities, null/foreign/disabled guards, idempotent native procedure mapper; queries perform no GPU calls");
    barrier_cases();transfer_cases();return 0;
}
