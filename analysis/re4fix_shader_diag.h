// Temporary native fragment capture; the original Vulkan call and shader bytes stay exact.
#ifndef RE4FIX_SHADER_DIAG_H
#define RE4FIX_SHADER_DIAG_H
#include <vulkan/vulkan_core.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static PFN_vkCreateShaderModule shader_diag_create_module;
static pthread_mutex_t shader_diag_mutex=PTHREAD_MUTEX_INITIALIZER;
static uint64_t shader_diag_hashes[64];
static unsigned shader_diag_count;
static size_t shader_diag_bytes;

static int shader_diag_fragment(const VkShaderModuleCreateInfo* info) {
    if(!info||info->sType!=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO||!info->pCode
        ||info->codeSize<20||info->codeSize>512u*1024u||(info->codeSize&3u))return 0;
    const uint32_t* code=info->pCode;size_t words=info->codeSize/4;
    if(code[0]!=0x07230203u||!code[3]||code[4])return 0;
    int fragment=0;
    for(size_t at=5;at<words;) {
        unsigned count=code[at]>>16,operation=code[at]&0xffffu;
        if(!count||count>words-at)return 0;
        if(operation==15) { // OpEntryPoint: ExecutionModel Fragment is 4.
            if(count<4||!memchr(code+at+3,0,(count-3u)*4u))return 0;
            if(code[at+1]==4)fragment=1;
        }
        at+=count;
    }
    return fragment;
}
static void shader_diag_capture(const VkShaderModuleCreateInfo* info,VkResult result,VkShaderModule module,void* caller) {
    pthread_mutex_lock(&shader_diag_mutex);
    int full=shader_diag_count>=64||shader_diag_bytes>=8u*1024u*1024u;
    pthread_mutex_unlock(&shader_diag_mutex);
    if(full||!shader_diag_fragment(info))return;
    const unsigned char* bytes=(const unsigned char*)info->pCode;
    uint64_t hash=14695981039346656037ULL;
    for(size_t i=0;i<info->codeSize;i++){hash^=bytes[i];hash*=1099511628211ULL;}
    pthread_mutex_lock(&shader_diag_mutex);
    if(shader_diag_count>=64||info->codeSize>8u*1024u*1024u-shader_diag_bytes) {
        pthread_mutex_unlock(&shader_diag_mutex);return;
    }
    for(unsigned i=0;i<shader_diag_count;i++)if(shader_diag_hashes[i]==hash) {
        pthread_mutex_unlock(&shader_diag_mutex);return;
    }
    unsigned index=shader_diag_count++;
    shader_diag_hashes[index]=hash;shader_diag_bytes+=info->codeSize;
    char path[128];
    snprintf(path,sizeof path,"/data/data/com.Armature.VR4/re4_native_shader_%02u.spv",index);
    FILE* file=fopen(path,"wb");size_t written=0;int closed=-1;
    if(file){written=fwrite(info->pCode,1,info->codeSize,file);closed=fclose(file);}
    int saved=written==info->codeSize&&closed==0;
    if(file&&!saved)remove(path);
    pthread_mutex_unlock(&shader_diag_mutex);
    LOGF("SHADER native fragment n=%u hash=%016llx bytes=%zu module=%p result=%d caller=%p saved=%d path=%s",
        index,(unsigned long long)hash,info->codeSize,(void*)(uintptr_t)module,result,caller,saved,path);
}
static VKAPI_ATTR __attribute__((noinline)) VkResult VKAPI_CALL h_shader_diag_CreateShaderModule(VkDevice device,
    const VkShaderModuleCreateInfo* info,const VkAllocationCallbacks* allocator,VkShaderModule* out) {
    if(!shader_diag_create_module)return VK_ERROR_INITIALIZATION_FAILED;
    VkResult result=shader_diag_create_module(device,info,allocator,out);
    shader_diag_capture(info,result,result==VK_SUCCESS&&out?*out:VK_NULL_HANDLE,__builtin_return_address(0));
    return result;
}
static void* shader_diag_proc(const char* name,void* real) {
    if(!name||!real||strcmp(name,"vkCreateShaderModule"))return real;
    if(real!=(void*)h_shader_diag_CreateShaderModule)shader_diag_create_module=(PFN_vkCreateShaderModule)real;
    return (void*)h_shader_diag_CreateShaderModule;
}
#endif
