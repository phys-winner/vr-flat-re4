#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
__declspec(dllimport) void* __stdcall VirtualAlloc(void*,size_t,unsigned long,unsigned long);
__declspec(dllimport) int __stdcall VirtualFree(void*,size_t,unsigned long);
#endif
static unsigned patches,logs;
static int patch_error;
static void* expected_target;
static void gate_stub(void){}
static int patch_code(void* target,void* hook){
 assert(target==expected_target&&hook==(void*)gate_stub);patches++;
 if(patch_error)return patch_error;
 const uint32_t jump[2]={0x58000051,0xd61f0220};memcpy(target,jump,8);memcpy((char*)target+8,&hook,8);return 0;
}
#define LOGF(...) (++logs)
#define RE4FIX_COMP_FB_SCAN_GATE gate_stub
#include "../re4fix_comp_framebuffer_lifecycle.h"
int main(void){
#ifdef _WIN32
 unsigned char* memory=VirtualAlloc((void*)0x20000000,0x6600000,0x3000,4);assert(memory==(void*)0x20000000);
 unsigned long base=(unsigned long)(uintptr_t)memory;expected_target=memory+0x6525900;
 for(unsigned i=0;i<sizeof comp_fb_lifecycle_checks/sizeof *comp_fb_lifecycle_checks;i++)
  memcpy(memory+comp_fb_lifecycle_checks[i].address,comp_fb_lifecycle_checks[i].words,comp_fb_lifecycle_checks[i].count*4);
 assert(!install_comp_framebuffer_lifecycle(0)&&!patches&&!g_tgt_comp_fb_match);
 for(unsigned i=0;i<sizeof comp_fb_lifecycle_checks/sizeof *comp_fb_lifecycle_checks;i++)for(unsigned j=0;j<comp_fb_lifecycle_checks[i].count;j++){
  uint32_t* word=(uint32_t*)(memory+comp_fb_lifecycle_checks[i].address)+j;*word^=1;
  assert(!install_comp_framebuffer_lifecycle(base)&&!patches&&!g_tgt_comp_fb_match&&!comp_fb_lifecycle_base);*word^=1;
 }
 patch_error=-1;assert(!install_comp_framebuffer_lifecycle(base)&&patches==1&&!g_tgt_comp_fb_match&&!g_tgt_comp_fb_depth&&!comp_fb_lifecycle_base);
 patch_error=0;assert(install_comp_framebuffer_lifecycle(base)&&patches==2&&comp_fb_lifecycle_base==base);
 assert(g_tgt_comp_fb_match==base+0x6525930&&g_tgt_comp_fb_depth==base+0x6525924);
 assert(*(uint32_t*)expected_target==0x58000051&&*((uint32_t*)expected_target+1)==0xd61f0220);
 assert(*(void**)((char*)expected_target+8)==(void*)gate_stub);
 assert(install_comp_framebuffer_lifecycle(base)&&patches==2&&!install_comp_framebuffer_lifecycle(base+4096));
 VirtualFree(memory,0,0x8000);
#endif
 puts("PASS: actual lifecycle installer, every guarded native word rejects before mutation; failed patch clears targets, successful16-byte branch publishes exact match/depth resumes, same-base idempotency and different-base refusal");return 0;
}
