#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void* (*phone_player)(const void*), *(*phone_active)(void), *(*rl_gun1)(const void*);
static unsigned (*phone_type)(void);
static void* (*rl_primary)(const void*), *(*f_GetCurrentGrip)(const void*);
static volatile int g_touch_context=5;
static int reload_grip_registered(void* p) { (void)p;return 0; }
static int patch_code(void* target,void* hook) { (void)target;(void)hook;assert(0);return -1; }
static void fake_unused_native(void* p) { (void)p; }
#define RE4FIX_CUTSCENE_NATIVE_GATE fake_unused_native
#define RE4FIX_CUTSCENE_AVATAR_NATIVE_GATE fake_unused_native
#define LOGF(...) ((void)0)
#include "../re4fix_cutscene_weapon.h"
#include "../re4fix_phone_view.h"
enum { PAWN,PHONE,MESHSET,SCENE,SKELETAL,WRONG,CLASS_COUNT };
static struct {
    unsigned char pawn[0x1200],other[0x1200],meshset[0x300],root[0x600],pawn_root[0x600];
    unsigned char primary[0x600],secondary[0x600],device[0x600],widget[0x600],hand[0x600],foreign[0x600];
    unsigned char classes[CLASS_COUNT][0x100];void* chains[CLASS_COUNT][2];
} f __attribute__((aligned(16)));
static unsigned type;
static void* active,*mesh_override;
static int original_calls,visible_calls,mesh_calls,type_calls,active_calls,desired,order;
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
static void original(void* pawn) {
    assert(pawn==f.pawn);original_calls++;order=1;
    unsigned char* meshes[2]={f.primary,f.secondary};
    for(int i=0;i<2;i++) {
        unsigned short* flags=(unsigned short*)(meshes[i]+0x14c);
        *flags=(*flags&(unsigned short)~0x20)|(desired ? 0x20 : 0);
    }
}
static void after_original(void) { original(f.pawn);phone_view_avatar_post(f.pawn); }
static void object(unsigned char* p,int cls,void* owner) {
    *(void**)(p+0x10)=f.classes[cls];*(void**)(p+0x20)=owner;
}
static void ready(void) {
    memset(&f,0,sizeof f);
    for(int i=0;i<CLASS_COUNT;i++) {
        f.chains[i][0]=f.classes[i]+0x30;*(void**)(f.classes[i]+0x30)=f.chains[i];
        *(int*)(f.classes[i]+0x58)=i==PAWN ? 0x10d0 : i==PHONE ? sizeof f.pawn : i==MESHSET ? sizeof f.meshset : 0x600;
    }
    f.chains[PHONE][0]=f.classes[PAWN]+0x30;f.chains[PHONE][1]=f.classes[PHONE]+0x30;
    *(int*)(f.classes[PHONE]+0x38)=1;
    object(f.pawn,PHONE,0);object(f.meshset,MESHSET,0);
    object(f.root,SCENE,f.meshset);object(f.pawn_root,SCENE,f.pawn);
    object(f.primary,SKELETAL,f.meshset);object(f.secondary,SKELETAL,f.meshset);
    object(f.foreign,SKELETAL,f.other);
    *(void**)(f.pawn+0xba8)=f.meshset;*(void**)(f.pawn+0x130)=f.pawn_root;
    *(void**)(f.meshset+0x130)=f.root;*(void**)(f.meshset+0x220)=f.root;
    *(void**)(f.meshset+0x228)=f.primary;*(void**)(f.meshset+0x230)=f.secondary;
    *(void**)(f.root+0xc0)=f.pawn_root;*(void**)(f.primary+0xc0)=f.root;*(void**)(f.secondary+0xc0)=f.root;
    *(void**)(f.primary+0x430)=f.device;*(void**)(f.secondary+0x430)=f.device;
    *(unsigned short*)(f.primary+0x14c)=*(unsigned short*)(f.secondary+0x14c)=0x220;
    memset(f.device,0xa5,sizeof f.device);memset(f.widget,0x5a,sizeof f.widget);memset(f.hand,0x35,sizeof f.hand);
    type=5;active=f.pawn;mesh_override=0;desired=1;
    original_calls=visible_calls=mesh_calls=type_calls=active_calls=order=0;pv_avatar_logs=0;
    phone_type=get_type;phone_active=get_active;pv_pawn_class=pawn_class;
    cw_meshset_class=meshset_class;cw_scene_class=scene_class;cw_skeletal_class=skeletal_class;
    cw_get_mesh=get_mesh;cw_avatar_visible=visible;phone_view_ready=1;
}
static void rejected(void) {
    unsigned char before[sizeof f];memcpy(before,&f,sizeof f);after_original();
    assert(original_calls==1 && !visible_calls && !memcmp(before,&f,sizeof f));
}
static void accepted(int leaves) {
    unsigned char before[sizeof f];memcpy(before,&f,sizeof f);after_original();
    assert(original_calls==1 && visible_calls==leaves && mesh_calls==2);
    *(unsigned short*)(f.primary+0x14c)|=0x20;*(unsigned short*)(f.secondary+0x14c)|=0x20;
    assert(!memcmp(before,&f,sizeof f));
}
static struct {
    unsigned char device[0x430],root[0x600],head[0x600],screens[2][0x600],assets[2][0x138];
    unsigned char device_class[0x100],static_class[0x100],asset_class[0x100],fields[3][0x80],field_class[0x40];
    void* static_chain[2];void* asset_chain[1];
} probe __attribute__((aligned(16)));
static int probe_head_calls,probe_bounds_calls,probe_names,probe_frees;
static void* probe_static_class(void) { return probe.static_class; }
static void* probe_asset_class(void) { return probe.asset_class; }
static void probe_name(const void* name,void* result) {
    const char* strings[]={"","CommunicatorDevice_C","ObjectProperty","Root","ScreenIncoming","ScreenOutgoing","Component","ScreenAsset"};
    unsigned index=*(const unsigned*)name;assert(index<sizeof strings/sizeof strings[0]);
    size_t length=strlen(strings[index])+1;unsigned short* text=malloc(length*2);assert(text);
    for(size_t i=0;i<length;i++)text[i]=(unsigned char)strings[index][i];
    struct { unsigned short* data;int count,capacity; } packet={text,(int)length,(int)length};
    memcpy(result,&packet,sizeof packet);probe_names++;
}
static void probe_free(void* memory) { assert(memory);probe_frees++;free(memory); }
static struct pv_scene_tf probe_head(const void* pawn) {
    assert(pawn==f.pawn);probe_head_calls++;
    struct pv_scene_tf tf;memcpy(&tf,probe.head+0x1c0,sizeof tf);return tf;
}
static struct pv_mesh_bounds probe_bounds(const void* asset) {
    assert(asset==probe.assets[0] || asset==probe.assets[1]);probe_bounds_calls++;
    struct pv_mesh_bounds bounds;memcpy(&bounds,(const char*)asset+0xf0,sizeof bounds);return bounds;
}
static void probe_ready(void) {
    ready();memset(&probe,0,sizeof probe);probe_head_calls=probe_bounds_calls=probe_names=probe_frees=0;
    *(int*)(f.classes[SCENE]+0x58)=0x230;
    pv_head_transform=probe_head;pv_mesh_bounds=probe_bounds;pv_static_class=probe_static_class;pv_asset_class=probe_asset_class;
    pv_name=probe_name;pv_free=probe_free;
    *(void**)(probe.device+0x10)=probe.device_class;*(int*)(probe.device_class+0x58)=0x42c;
    *(unsigned*)(probe.device_class+0x18)=1;*(void**)(probe.device_class+0x70)=probe.fields[0];
    *(void**)(probe.device+0x130)=probe.root;*(void**)(probe.device+0x360)=probe.root;
    *(void**)(probe.device+0x2f0)=probe.screens[0];*(void**)(probe.device+0x2d8)=probe.screens[1];
    *(unsigned*)probe.field_class=2;
    const int offsets[]={0x360,0x2f0,0x2d8};
    for(int i=0;i<3;i++) {
        *(void**)(probe.fields[i]+0x8)=probe.field_class;*(unsigned*)(probe.fields[i]+0x28)=3+i;
        *(int*)(probe.fields[i]+0x34)=1;*(int*)(probe.fields[i]+0x38)=8;*(int*)(probe.fields[i]+0x4c)=offsets[i];
        *(void**)(probe.fields[i]+0x58)=i<2 ? probe.fields[i+1] : 0;
    }
    probe.static_chain[0]=f.classes[SCENE]+0x30;probe.static_chain[1]=probe.static_class+0x30;
    *(void**)(probe.static_class+0x30)=probe.static_chain;*(int*)(probe.static_class+0x38)=1;*(int*)(probe.static_class+0x58)=0x4a0;
    probe.asset_chain[0]=probe.asset_class+0x30;*(void**)(probe.asset_class+0x30)=probe.asset_chain;
    *(int*)(probe.asset_class+0x58)=0x138;
    object(probe.root,SCENE,probe.device);object(probe.head,SCENE,f.pawn);*(void**)(f.pawn+0x2d8)=probe.head;
    struct pv_scene_tf tf={{0,0,0,1},{10,20,30},{1,1,1},{0,0}};
    memcpy(probe.root+0x1c0,&tf,sizeof tf);memcpy(probe.head+0x1c0,&tf,sizeof tf);
    for(int i=0;i<2;i++) {
        *(void**)(probe.screens[i]+0x10)=probe.static_class;*(void**)(probe.screens[i]+0x20)=probe.device;
        *(void**)(probe.screens[i]+0xc0)=probe.root;*(void**)(probe.screens[i]+0x440)=probe.assets[i];
        *(unsigned*)(probe.screens[i]+0x18)=6;memcpy(probe.screens[i]+0x1c0,&tf,sizeof tf);
        *(void**)(probe.assets[i]+0x10)=probe.asset_class;*(unsigned*)(probe.assets[i]+0x18)=7;
        struct pv_mesh_bounds bounds={{0,0,0},{4,5,.001f},6.5f};memcpy(probe.assets[i]+0xf0,&bounds,sizeof bounds);
    }
}
static void inspect_selfcheck(void) {
    probe_ready();unsigned char before_f[sizeof f],before_probe[sizeof probe];
    memcpy(before_f,&f,sizeof f);memcpy(before_probe,&probe,sizeof probe);
    phone_view_inspect(f.pawn,probe.device);assert(probe_head_calls==1 && probe_bounds_calls==2 && probe_names==probe_frees);
    assert(!memcmp(before_f,&f,sizeof f) && !memcmp(before_probe,&probe,sizeof probe));
    probe_ready();type=1;phone_view_inspect(f.pawn,probe.device);assert(!probe_head_calls && !probe_bounds_calls && !probe_names);
    probe_ready();active=f.other;phone_view_inspect(f.pawn,probe.device);assert(!probe_head_calls && !probe_bounds_calls && !probe_names);
    probe_ready();*(int*)(probe.fields[1]+0x34)=2;phone_view_inspect(f.pawn,probe.device);assert(probe_head_calls==1 && probe_bounds_calls==1 && probe_names==probe_frees);
    probe_ready();*(void**)(probe.screens[0]+0x20)=f.other;phone_view_inspect(f.pawn,probe.device);assert(probe_bounds_calls==1);
    probe_ready();*(void**)(probe.assets[0]+0x10)=f.classes[WRONG];phone_view_inspect(f.pawn,probe.device);assert(probe_bounds_calls==1);
    probe_ready();*(void**)(probe.head+0x20)=f.other;phone_view_inspect(f.pawn,probe.device);assert(!probe_head_calls && probe_bounds_calls==2);
    struct pv_scene_tf tf={{0,0,0,1},{1,2,3},{1,1,1},{0,0}};assert(pv_tf_finite(&tf));
    tf.s[0]=0;assert(!pv_tf_finite(&tf));tf.s[0]=1;tf.q[0]=NAN;assert(!pv_tf_finite(&tf));
    puts("PASS: actual explicit read-only reflected phone probe, scalar head48/bounds28, typed/owned component and asset getters, malformed-property/type/owner refusals, balanced native string lifetime and immutable objects");
}
static void installer(void) {
    const size_t extent=0x83ad970;
#if defined(_WIN32)
    __declspec(dllimport) void* __stdcall VirtualAlloc(void*,size_t,unsigned long,unsigned long);
    __declspec(dllimport) int __stdcall VirtualFree(void*,size_t,unsigned long);
    unsigned char* memory=VirtualAlloc((void*)0x20000000,extent,0x3000,4);assert(memory==(void*)0x20000000);
#else
    unsigned char* memory=calloc(1,extent);assert(memory);
#endif
    unsigned long base=(unsigned long)(size_t)memory;
    const unsigned long addresses[]={0x642c718,0x642c7a0,0x62af678,0x83ad964};
    const unsigned words[][2]={{0xd10183ff,0xf90023f3},{0x52821a04,0x52800205},
        {0xf945d408,0xb40002a8},{0xd104c3ff,0xf90073fc}};
    for(int i=0;i<4;i++)memcpy(memory+addresses[i],words[i],8);
    // The shared writer has already been patched when this installer runs.
    memset(memory+0x62a65e0,0xcc,16);
    phone_type=(unsigned (*)(void))(base+0x6338aec);phone_active=(void* (*)(void))(base+0x63380dc);
    cw_meshset_class=(void* (*)(void))(base+0x6427ad4);cw_scene_class=(void* (*)(void))(base+0x8f82500);
    cw_skeletal_class=(void* (*)(void))(base+0x8f94650);cw_get_mesh=(void* (*)(const void*,unsigned char))(base+0x62af678);
    cw_avatar_visible=(void (*)(void*,int,int))(base+0x83ad964);g_tgt_cutscene_avatar=base+0x62a65e0+16;
    phone_view_ready=0;pv_pawn_class=0;install_phone_view(0);assert(!phone_view_ready && !pv_pawn_class);
    for(int i=0;i<4;i++) {
        memory[addresses[i]]^=1;install_phone_view(base);assert(!phone_view_ready && !pv_pawn_class);memory[addresses[i]]^=1;
    }
    cw_avatar_visible=0;install_phone_view(base);assert(!phone_view_ready);
    cw_avatar_visible=(void (*)(void*,int,int))(base+0x83ad964);
    g_tgt_cutscene_avatar++;install_phone_view(base);assert(!phone_view_ready);g_tgt_cutscene_avatar--;
    install_phone_view(base);assert(phone_view_ready && pv_pawn_class==(void* (*)(void))(base+0x642c718));
    install_phone_view(base);assert(phone_view_ready);
#if defined(_WIN32)
    assert(VirtualFree(memory,0,0x8000));
#else
    free(memory);
#endif
    phone_view_ready=0;pv_pawn_class=0;
}
int main(void) {
    ready();accepted(2);
    ready();*(void**)(f.secondary+0x430)=0;accepted(1);
    ready();*(void**)(f.meshset+0x230)=0;accepted(1);
    ready();after_original();after_original();assert(original_calls==2 && visible_calls==4);
    type=1;after_original();assert(original_calls==3 && visible_calls==4 && *(unsigned short*)(f.primary+0x14c)==0x220);
    desired=0;after_original();assert(original_calls==4 && visible_calls==4 && *(unsigned short*)(f.primary+0x14c)==0x200);
    for(unsigned i=0;i<10;i++)if(i!=5){ready();type=i;rejected();}
    ready();phone_view_ready=0;rejected();ready();pv_pawn_class=0;rejected();ready();cw_avatar_visible=0;rejected();
    ready();phone_type=0;rejected();ready();phone_active=0;rejected();ready();cw_get_mesh=0;rejected();
    ready();active=f.other;rejected();ready();active=0;rejected();
    ready();*(void**)(f.pawn+0x10)=f.classes[WRONG];rejected();assert(!type_calls && !active_calls);
    ready();*(int*)(f.classes[PHONE]+0x58)=0x10cf;rejected();
    ready();*(int*)(f.classes[PAWN]+0x58)=0x10cf;rejected();
    ready();f.chains[PHONE][0]=f.classes[WRONG]+0x30;rejected();
    ready();*(void**)(f.pawn+0xba8)=0;rejected();assert(!type_calls && !active_calls);
    ready();*(void**)(f.meshset+0x10)=f.classes[WRONG];rejected();
    ready();*(int*)(f.classes[MESHSET]+0x58)=0x237;rejected();
    ready();*(void**)(f.meshset+0x220)=f.pawn_root;rejected();
    ready();*(void**)(f.root+0x20)=f.other;rejected();ready();*(void**)(f.root+0xc0)=f.other;rejected();
    ready();*(void**)(f.pawn_root+0x20)=f.other;rejected();
    ready();*(void**)(f.primary+0x20)=f.other;rejected();assert(!mesh_calls);
    ready();*(void**)(f.secondary+0x10)=f.classes[WRONG];rejected();assert(!mesh_calls);
    ready();*(int*)(f.classes[SKELETAL]+0x58)=0x437;rejected();assert(!mesh_calls);
    ready();*(void**)(f.primary+0xc0)=f.other;rejected();ready();*(void**)(f.secondary+0xc0)=f.other;rejected();
    ready();mesh_override=f.foreign;rejected();assert(mesh_calls==2);
    ready();for(int i=0;i<20;i++)after_original();assert(pv_avatar_logs==8);
    inspect_selfcheck();
    installer();
    puts("PASS: actual phone avatar helper, derived native GamePawn/type5/active binding, exact unique leaves/fallback, original recovery, immutable device/widgets/hand, propagation0, early malformed guards; installer corruption/dependency checks");
    return 0;
}
