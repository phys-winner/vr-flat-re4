#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan_core.h>
static FILE* test_open(const char*,const char*);
static size_t test_write(const void*,size_t,size_t,FILE*);
static int test_close(FILE*);
static int test_remove(const char*);
static void test_log(const char*,...);
#define fopen test_open
#define fwrite test_write
#define fclose test_close
#define remove test_remove
#define LOGF(...) test_log(__VA_ARGS__)
#include "../re4fix_shader_diag.h"
#undef fopen
#undef fwrite
#undef fclose
#undef remove
#ifndef RE4_TEST_MUTEX_LOCKED
#define RE4_TEST_MUTEX_LOCKED(m) 0
#endif
static unsigned calls,opens,writes,closes,removes,logs;
static int fail_open,short_write,fail_close;
static VkResult native_result=VK_SUCCESS;
static VkDevice device=(VkDevice)(uintptr_t)0x1234567812345678ULL;
static const VkAllocationCallbacks* allocator=(const VkAllocationCallbacks*)(uintptr_t)0x9876543212345678ULL;
static VkShaderModule output;
static VkShaderModuleCreateInfo info={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
static uint32_t fragment[]={0x07230203,0x00010000,0,2,0,0x0005000f,4,1,0x6e69616d,0};
static uint32_t before[sizeof fragment/sizeof *fragment];
static const void* expected_code;
static size_t expected_size;
static VkResult create(VkDevice d,const VkShaderModuleCreateInfo* ci,const VkAllocationCallbacks* a,VkShaderModule* out) {
    assert(!RE4_TEST_MUTEX_LOCKED(shader_diag_mutex));
    assert(d==device&&ci==&info&&a==allocator&&out==&output&&ci->pCode==expected_code&&ci->codeSize==expected_size);
    calls++;*out=(VkShaderModule)(uintptr_t)0xabcdef0198765432ULL;return native_result;
}
static FILE* test_open(const char* path,const char* mode) {
    char expected[128];snprintf(expected,sizeof expected,"/data/data/com.Armature.VR4/re4_native_shader_%02u.spv",shader_diag_count-1);
    assert(!strcmp(path,expected)&&!strcmp(mode,"wb"));opens++;
    return fail_open?0:(FILE*)(uintptr_t)0x123456789abcdef0ULL;
}
static size_t test_write(const void* p,size_t size,size_t count,FILE* file) {
    assert(p==expected_code&&size==1&&count==expected_size&&file==(FILE*)(uintptr_t)0x123456789abcdef0ULL);
    writes++;return short_write?count-1:count;
}
static int test_close(FILE* file){assert(file==(FILE*)(uintptr_t)0x123456789abcdef0ULL);closes++;return fail_close?-1:0;}
static int test_remove(const char* path){assert(strstr(path,"/re4_native_shader_"));removes++;return 0;}
static void test_log(const char* format,...){assert(!RE4_TEST_MUTEX_LOCKED(shader_diag_mutex)&&strstr(format,"SHADER native fragment"));logs++;}
static void reset_capture(void){memset(shader_diag_hashes,0,sizeof shader_diag_hashes);shader_diag_count=0;shader_diag_bytes=0;opens=writes=closes=removes=logs=0;fail_open=short_write=fail_close=0;}
static void run(const uint32_t* code,size_t size) {
    info.pCode=code;info.codeSize=size;expected_code=code;expected_size=size;
    VkShaderModuleCreateInfo ci=info;unsigned previous=calls;
    assert(h_shader_diag_CreateShaderModule(device,&info,allocator,&output)==native_result&&calls==previous+1);
    assert(!memcmp(&ci,&info,sizeof ci)&&output==(VkShaderModule)(uintptr_t)0xabcdef0198765432ULL);
}
int main(void) {
    assert(shader_diag_proc(0,(void*)create)==(void*)create&&!shader_diag_proc("vkCreateShaderModule",0));
    assert(shader_diag_proc("vkOther",(void*)create)==(void*)create);
    assert(shader_diag_proc("vkCreateShaderModule",(void*)create)==(void*)h_shader_diag_CreateShaderModule);
    assert(shader_diag_proc("vkCreateShaderModule",(void*)h_shader_diag_CreateShaderModule)==(void*)h_shader_diag_CreateShaderModule&&shader_diag_create_module==create);
    memcpy(before,fragment,sizeof before);run(fragment,sizeof fragment);
    assert(opens==1&&writes==1&&closes==1&&logs==1&&shader_diag_count==1&&shader_diag_bytes==sizeof fragment&&!memcmp(before,fragment,sizeof fragment));
    run(fragment,sizeof fragment);assert(opens==1&&shader_diag_count==1);
    fragment[6]=0;run(fragment,sizeof fragment);assert(opens==1);fragment[6]=4;
    fragment[0]=0;run(fragment,sizeof fragment);assert(opens==1);fragment[0]=0x07230203;
    fragment[5]=15;run(fragment,sizeof fragment);assert(opens==1);fragment[5]=0x0005000f;
    run(fragment,19);run(fragment,sizeof fragment-1);run(0,sizeof fragment);run(fragment,512u*1024u+4);assert(opens==1);
    uint32_t malformed[11];memcpy(malformed,fragment,sizeof fragment);malformed[10]=0x00020000;run(malformed,sizeof malformed);assert(opens==1);
    memcpy(malformed,fragment,sizeof fragment);malformed[9]=0x11111111;malformed[10]=0x00010000;run(malformed,sizeof malformed);assert(opens==1);
    reset_capture();native_result=VK_ERROR_OUT_OF_HOST_MEMORY;run(fragment,sizeof fragment);assert(opens==1&&logs==1);native_result=VK_SUCCESS;
    reset_capture();fail_open=1;run(fragment,sizeof fragment);assert(opens==1&&!writes&&!closes&&!removes&&logs==1);run(fragment,sizeof fragment);assert(opens==1);
    reset_capture();short_write=1;run(fragment,sizeof fragment);assert(opens==1&&writes==1&&closes==1&&removes==1&&logs==1);
    reset_capture();fail_close=1;run(fragment,sizeof fragment);assert(removes==1);
    reset_capture();for(unsigned i=0;i<70;i++){fragment[3]=2+i;run(fragment,sizeof fragment);}
    assert(shader_diag_count==64&&opens==64&&logs==64&&shader_diag_bytes==64*sizeof fragment);
    reset_capture();size_t size=512u*1024u;uint32_t* large=malloc(size);assert(large);memcpy(large,fragment,sizeof fragment);
    for(size_t i=sizeof fragment/4;i<size/4;i++)large[i]=0x00010000;
    for(unsigned i=0;i<20;i++){large[3]=2+i;run(large,size);}
    assert(shader_diag_count==16&&shader_diag_bytes==8u*1024u*1024u&&opens==16&&logs==16);free(large);
    puts("PASS: actual SDK shader hook, exact once-only forwarding and native result, unchanged borrowed bytes/CI, fragment instruction bounds, dedup,64-file/8MiB limits, I/O failures and idempotent mapper");return 0;
}
