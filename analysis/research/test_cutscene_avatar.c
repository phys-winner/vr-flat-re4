#include <assert.h>
#include <stdio.h>
#include <string.h>
static void* (*phone_player)(const void*), *(*phone_active)(void), *(*rl_gun1)(const void*);
static unsigned (*phone_type)(void);
static void* (*rl_primary)(const void*), *(*f_GetCurrentGrip)(const void*);
static volatile int g_touch_context=3;
static int reload_grip_registered(void* p) { (void)p;return 0; }
static volatile int test_patch_result=-1;
static volatile unsigned long test_install_base;
static void* volatile test_patch_target;
static void* volatile test_patch_hook;
static int patch_code(void* target,void* hook) { test_patch_target=target;test_patch_hook=hook;return test_patch_result; }
#if !defined(__aarch64__)
static void fake_weapon_native(void* p) { (void)p; }
static void fake_avatar_native(void*);
#define RE4FIX_CUTSCENE_NATIVE_GATE fake_weapon_native
#define RE4FIX_CUTSCENE_AVATAR_NATIVE_GATE fake_avatar_native
#endif
#define LOGF(...) ((void)0)
#include "../re4fix_cutscene_weapon.h"
enum { PAWN,MESHSET,SCENE,SKELETAL,WRONG,CLASS_COUNT };
static struct {
    unsigned char pawn[0x1200],other[0x1200],meshset[0x300],root[0x600],pawn_root[0x600];
    unsigned char primary[0x600],secondary[0x600],cinema[0x600],gun[0xe00],foreign[0x600];
    unsigned char classes[CLASS_COUNT][0x100];void* chains[CLASS_COUNT][1];
} f __attribute__((aligned(16)));
static unsigned type;
static void* active,*mesh_override;
static int native_calls,visible_calls,mesh_calls,type_calls,active_calls,desired,order;
static void* pawn_class(void) { return f.classes[PAWN]; }
static void* meshset_class(void) { return f.classes[MESHSET]; }
static void* scene_class(void) { return f.classes[SCENE]; }
static void* skeletal_class(void) { return f.classes[SKELETAL]; }
static unsigned get_type(void) { type_calls++;return type; }
static void* get_active(void) { active_calls++;return active; }
static void* get_mesh(const void* p,unsigned char side) {
    assert(p==f.pawn && side<2);mesh_calls++;
    if(mesh_override)return mesh_override;
    void* secondary=*(void**)(f.meshset+0x230);
    return !side && secondary && *(void**)((char*)secondary+0x430) ? secondary : f.primary;
}
static void visible(void* p,int value,int propagation) {
    assert((p==f.primary || p==f.secondary) && value==0 && propagation==0 && order==1);
    visible_calls++;*(unsigned short*)((char*)p+0x14c)&=(unsigned short)~0x20;
}
#if !defined(__aarch64__)
static void fake_avatar_native(void* p) {
    assert(p==f.pawn);native_calls++;order=1;
    unsigned char* meshes[2]={f.primary,f.secondary};
    for(int i=0;i<2;i++) {
        unsigned short* flags=(unsigned short*)(meshes[i]+0x14c);
        *flags=(*flags&(unsigned short)~0x20)|(desired ? 0x20 : 0);
    }
}
#endif
static void object(unsigned char* p,int cls,void* owner) {
    *(void**)(p+0x10)=f.classes[cls];*(void**)(p+0x20)=owner;
}
static void ready(void) {
    memset(&f,0,sizeof f);
    for(int i=0;i<CLASS_COUNT;i++) {
        f.chains[i][0]=f.classes[i]+0x30;*(void**)(f.classes[i]+0x30)=f.chains[i];
        *(int*)(f.classes[i]+0x58)=i==PAWN ? sizeof f.pawn : i==MESHSET ? sizeof f.meshset : 0x600;
    }
    object(f.pawn,PAWN,0);object(f.meshset,MESHSET,0);
    object(f.root,SCENE,f.meshset);object(f.pawn_root,SCENE,f.pawn);
    object(f.primary,SKELETAL,f.meshset);object(f.secondary,SKELETAL,f.meshset);
    object(f.foreign,SKELETAL,f.other);
    *(void**)(f.pawn+0xba8)=f.meshset;*(void**)(f.pawn+0x130)=f.pawn_root;
    *(void**)(f.meshset+0x130)=f.root;*(void**)(f.meshset+0x220)=f.root;
    *(void**)(f.meshset+0x228)=f.primary;*(void**)(f.meshset+0x230)=f.secondary;
    *(void**)(f.root+0xc0)=f.pawn_root;*(void**)(f.primary+0xc0)=f.root;*(void**)(f.secondary+0xc0)=f.root;
    *(void**)(f.primary+0x430)=f.cinema;*(void**)(f.secondary+0x430)=f.cinema;
    *(unsigned short*)(f.primary+0x14c)=*(unsigned short*)(f.secondary+0x14c)=0x220;
    memset(f.cinema,0xa5,sizeof f.cinema);memset(f.gun,0x5a,sizeof f.gun);
    type=3;active=f.pawn;mesh_override=0;desired=1;
    native_calls=visible_calls=mesh_calls=type_calls=active_calls=order=0;cw_avatar_logs=0;
    phone_type=get_type;phone_active=get_active;cw_pawn_class=pawn_class;
    cw_meshset_class=meshset_class;cw_scene_class=scene_class;cw_skeletal_class=skeletal_class;
    cw_get_mesh=get_mesh;cw_avatar_visible=visible;
}
static void rejected(void) {
    unsigned char before[sizeof f];memcpy(before,&f,sizeof f);
    hook_cutscene_avatar(f.pawn);
    assert(native_calls==1 && !visible_calls && !memcmp(before,&f,sizeof f));
}
static void accepted(int leaves) {
    unsigned char before[sizeof f];memcpy(before,&f,sizeof f);
    hook_cutscene_avatar(f.pawn);assert(native_calls==1 && visible_calls==leaves && mesh_calls==2);
    *(unsigned short*)(f.primary+0x14c)|=0x20;*(unsigned short*)(f.secondary+0x14c)|=0x20;
    assert(!memcmp(before,&f,sizeof f));
}
int main(int argc,char** argv) {
    (void)argv;if(argc==999)install_touch_cutscene_weapon(test_install_base);
    ready();accepted(2);
    ready();*(void**)(f.secondary+0x430)=0;accepted(1); // Native left fallback deduplicates primary.
    ready();*(void**)(f.meshset+0x230)=0;accepted(1);
    ready();hook_cutscene_avatar(f.pawn);hook_cutscene_avatar(f.pawn);
    assert(native_calls==2 && visible_calls==4);
    type=1;hook_cutscene_avatar(f.pawn);
    assert(native_calls==3 && visible_calls==4 && *(unsigned short*)(f.primary+0x14c)==0x220);
    desired=0;hook_cutscene_avatar(f.pawn);
    assert(native_calls==4 && visible_calls==4 && *(unsigned short*)(f.primary+0x14c)==0x200);
    for(unsigned i=0;i<10;i++)if(i!=3){ready();type=i;rejected();}
    ready();cw_avatar_visible=0;rejected();ready();phone_type=0;rejected();ready();phone_active=0;rejected();
    ready();active=f.other;rejected();ready();active=0;rejected();
    ready();*(void**)(f.pawn+0x10)=f.classes[WRONG];rejected();assert(!type_calls && !active_calls);
    ready();*(void**)(f.pawn+0xba8)=0;rejected();assert(!type_calls && !active_calls);
    ready();*(void**)(f.meshset+0x10)=f.classes[WRONG];rejected();assert(!type_calls && !active_calls);
    ready();*(int*)(f.classes[MESHSET]+0x58)=0x237;rejected();
    ready();*(void**)(f.meshset+0x220)=f.pawn_root;rejected();
    ready();*(void**)(f.root+0x20)=f.other;rejected();ready();*(void**)(f.root+0xc0)=f.other;rejected();
    ready();*(void**)(f.pawn_root+0x20)=f.other;rejected();
    ready();*(void**)(f.primary+0x20)=f.other;rejected();assert(!mesh_calls);
    ready();*(void**)(f.secondary+0x10)=f.classes[WRONG];rejected();assert(!mesh_calls);
    ready();*(int*)(f.classes[SKELETAL]+0x58)=0x437;rejected();assert(!mesh_calls);
    ready();*(void**)(f.primary+0xc0)=f.other;rejected();ready();*(void**)(f.secondary+0xc0)=f.other;rejected();
    ready();mesh_override=f.foreign;rejected();assert(mesh_calls==2);
    ready();for(int i=0;i<20;i++)hook_cutscene_avatar(f.pawn);assert(cw_avatar_logs==8);
    puts("PASS: actual cutscene avatar C, original-first/reapply/native recovery, native GetMesh exact owned unique leaves/fallback; early no-manager guards; gun/cinema/other flags immutable; eight-log bound");
    return 0;
}
