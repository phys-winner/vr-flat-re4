#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int closed_fd,closed_asset,opened,constructed,original_exists,original_read;
static int available=1,descriptor=11,allocation=1;
static int zip_available,zip_index,fd_constructed;
static long long zip_offset=5000000000LL;
static long long asset_length=4278831240LL,asset_start=4096;
static unsigned char handle_bytes[48];
static int library_available=1,library_opens,library_closes,missing_symbol,symbol_lookups;
static void* asset_open(void*,const char*,int);
static long long asset_size(void*);
static int asset_fd(void*,long long*,long long*);
static void asset_close(void*);
static int fixture_close(int fd){assert(fd==11);++closed_fd;return 0;}
static void* fixture_dlopen(const char* name,int flags){assert(!strcmp(name,"libandroid.so")&&flags==2);++library_opens;return library_available?(void*)0x9000:0;}
static int fixture_dlclose(void* handle){assert(handle==(void*)0x9000);++library_closes;return 0;}
static void* fixture_dlsym(void* handle,const char* name){assert(handle==(void*)0x9000);++symbol_lookups;
 if(!strcmp(name,"AAssetManager_open"))return missing_symbol==1?0:(void*)asset_open;
 if(!strcmp(name,"AAsset_getLength64"))return missing_symbol==2?0:(void*)asset_size;
 if(!strcmp(name,"AAsset_openFileDescriptor64"))return missing_symbol==3?0:(void*)asset_fd;
 if(!strcmp(name,"AAsset_close"))return missing_symbol==4?0:(void*)asset_close;
 assert(!"Unexpected system API");return 0;}
static int patch_code(void* target,void* replacement){(void)target;(void)replacement;return 0;}
#define close fixture_close
#define dlsym fixture_dlsym
#define dlopen fixture_dlopen
#define dlclose fixture_dlclose
#define RTLD_NOW 2
#define RTLD_LOCAL 0
#define LOGF(...) ((void)0)
#define LOGI(...) ((void)0)
#define MOBILE_OBB_TEST
#include "../re4fix_packaged_obb.h"
static int mobile_zip_descriptor(int index,long long* offset){if(!zip_available)return -1;zip_index=index;*offset=zip_offset;return 11;}
static int gate_mobile_obb_exists(void* self,const unsigned short* path,int local){(void)self;(void)path;(void)local;++original_exists;return 0;}
static void* gate_mobile_obb_read(void* self,const unsigned short* path,int allow_local,int allow_write){(void)self;(void)path;(void)allow_local;(void)allow_write;++original_read;return (void*)0x1234;}
static void* manager(void){return available?(void*)1:0;}
static void* asset_open(void* context,const char* name,int mode){assert(context==(void*)1&&mode==1);assert(!strcmp(name,"main.201.com.Armature.VR4.obb")||!strcmp(name,"patch.201.com.Armature.VR4.obb"));++opened;return(void*)2;}
static long long asset_size(void* asset){assert(asset==(void*)2);return asset_length;}
static int asset_fd(void* asset,long long* start,long long* length){assert(asset==(void*)2);*start=asset_start;*length=asset_length;return descriptor;}
static void asset_close(void* asset){assert(asset==(void*)2);++closed_asset;}
static void* allocate(unsigned long bytes){assert(bytes==48);return allocation?handle_bytes:0;}
static void construct(void* handle,const MobileObbString* name,void* asset){assert(handle==handle_bytes&&asset==(void*)2);assert(name->count==name->capacity&&name->data[name->count-1]==0);memcpy(handle_bytes+0x18,&asset_start,8);memcpy(handle_bytes+0x20,&asset_length,8);++constructed;}
static void construct_fd(void* handle,const MobileObbString* name,int fd){assert(handle==handle_bytes&&fd==11);assert(name->data[name->count-1]==0);memset(handle_bytes,0,sizeof handle_bytes);++fd_constructed;}
static unsigned short* path(const char* input){static unsigned short output[1100];unsigned i=0;for(;input[i];++i)output[i]=(unsigned char)input[i];output[i]=0;return output;}
int main(void){
 library_available=0;assert(!mobile_obb_resolve_asset_api()&&library_opens==1&&library_closes==0&&!g_mobile_obb_android_library);
 library_available=1;
 for(int missing=1;missing<=4;missing++){missing_symbol=missing;assert(!mobile_obb_resolve_asset_api()&&!g_mobile_obb_android_library&&library_closes==missing);}
 missing_symbol=0;assert(mobile_obb_resolve_asset_api()&&g_mobile_obb_android_library==(void*)0x9000&&library_opens==6&&library_closes==4);
 int resolved_lookups=symbol_lookups;assert(mobile_obb_resolve_asset_api()&&library_opens==6&&symbol_lookups==resolved_lookups&&library_closes==4);
 f_mobile_obb_manager=manager;f_mobile_obb_open=asset_open;f_mobile_obb_length=asset_size;f_mobile_obb_descriptor=asset_fd;f_mobile_obb_close=asset_close;f_mobile_obb_allocate=allocate;f_mobile_obb_construct=construct;g_mobile_obb_ready=1;
 unsigned short main_path[100];memcpy(main_path,path("/storage/emulated/0/Android/obb/com.Armature.VR4/main.201.com.Armature.VR4.obb"),sizeof main_path);
 assert(hook_mobile_obb_exists(0,main_path,1)==1&&closed_fd==1&&closed_asset==1&&original_exists==0);
 assert(hook_mobile_obb_read(0,main_path,0,0)==handle_bytes&&constructed==1&&closed_fd==2&&closed_asset==1);
 assert(hook_mobile_obb_read(0,main_path,1,0)==handle_bytes&&constructed==2&&original_read==0&&opened==3&&closed_fd==3);
 assert(hook_mobile_obb_read(0,main_path,0,1)==handle_bytes&&constructed==3&&original_read==0&&opened==4&&closed_fd==4);
 ++asset_length;assert(hook_mobile_obb_exists(0,main_path,1)==0&&closed_asset==2&&closed_fd==5&&original_exists==1);--asset_length;
 descriptor=-1;assert(hook_mobile_obb_read(0,main_path,0,0)==(void*)0x1234&&closed_asset==3&&closed_fd==5);descriptor=11;
 asset_start=-1;assert(hook_mobile_obb_exists(0,main_path,1)==0&&closed_asset==4&&closed_fd==6);asset_start=4096;
 allocation=0;assert(!hook_mobile_obb_read(0,main_path,0,0)&&closed_asset==5);allocation=1;
 available=0;int before=opened;assert(!hook_mobile_obb_exists(0,main_path,1)&&opened==before);available=1;
 assert(!hook_mobile_obb_exists(0,path("other.obb"),1)&&opened==before);
 assert(!hook_mobile_obb_exists(0,path("main.201.com.Armature.VR4.obb/"),1)&&opened==before);
 asset_length=4237413580LL;assert(hook_mobile_obb_exists(0,path("patch.201.com.Armature.VR4.obb"),1));
 unsigned short invalid[1024];for(unsigned i=0;i<1024;i++)invalid[i]='a';const char* name;long long bytes;int count;assert(!mobile_obb_name(invalid,&name,&bytes,&count));
 assert(!mobile_obb_name(0,&name,&bytes,&count));
 g_mobile_obb_ready=0;before=opened;assert(!hook_mobile_obb_exists(0,main_path,1)&&opened==before);
 g_mobile_obb_ready=1;available=0;zip_available=1;f_mobile_obb_construct_fd=construct_fd;
 int closes=closed_fd;
 assert(hook_mobile_obb_exists(0,main_path,1)&&zip_index==0&&closed_fd==closes+1);
 assert(hook_mobile_obb_read(0,main_path,1,0)==handle_bytes&&fd_constructed==1&&closed_fd==closes+1);
 assert(*(long long*)(handle_bytes+0x18)==zip_offset&&*(long long*)(handle_bytes+0x20)==4278831240LL&&*(long long*)(handle_bytes+0x28)==zip_offset);
 zip_offset=9500000000LL;
 assert(hook_mobile_obb_read(0,path("patch.201.com.Armature.VR4.obb"),1,0)==handle_bytes&&zip_index==1&&fd_constructed==2);
 assert(*(long long*)(handle_bytes+0x18)==9500000000LL&&*(long long*)(handle_bytes+0x20)==4237413580LL&&*(long long*)(handle_bytes+0x28)==9500000000LL);
 allocation=0;closes=closed_fd;assert(!hook_mobile_obb_read(0,main_path,1,0)&&closed_fd==closes+1);allocation=1;
 zip_available=0;assert(hook_mobile_obb_read(0,main_path,1,0)==(void*)0x1234);
 puts("PASS: actual whole-OBB header, explicit retained libandroid owner/resolution and failure cleanup, native48-byte constructor ownership, both AllowLocal reads and genuine MountOBB arguments; unavailable/wrong-size/bad-FD/alloc/name/nonterminated/disabled fallbacks");return 0;
}
