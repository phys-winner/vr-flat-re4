#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
enum {N_NONE,N_PAWN_CLASS,N_WATCH_CLASS,N_MESHSET_CLASS,N_MESH_CLASS,N_SCENE_CLASS,N_FUNCTION_CLASS,
      N_UPDATE,N_ATTACH,N_LEFT_SOCKET,N_RIGHT_SOCKET,N_OBJECT,N_ENUM,N_NAME,N_BOOL,
      N_WATCH_ACTOR,N_MESHSET,N_PAWN,N_MESH,N_ACTIVE,N_LEFT,N_RIGHT,N_NEW,N_CALLBACK,N_COUNT};
static const char* names[N_COUNT]={"None","VR4Bio4PlayerPawn_BP_C","UI_WristWatch_BP_C","VR4GamePlayerMeshSet",
    "SkeletalMeshComponent","SceneComponent","Function","UpdateArm","AttachToArm","LeftHandWristSocket","RightHandWristSocket",
    "ObjectProperty","EnumProperty","NameProperty","BoolProperty","watchActor","MeshSet","Pawn","Mesh","ActiveArm",
    "LeftWristSocket","RightWristSocket","NewHand","Callback"};
enum {F_WATCH,F_MESHSET,F_PAWN,F_WATCH_MESH,F_ARM,F_LEFT,F_RIGHT,F_MESH,F_NEW,F_CALLBACK,F_COUNT};
struct fixture {
    _Alignas(8) unsigned char pawn[0x1200],watch[0x400],meshset[0x300],mesh[0x600],root[0x220];
    _Alignas(8) unsigned char cls[7][0x100],update[0xc0],attach[0xc0],field[F_COUNT][0x90],script[37],settings[0xb8];
    void* table[0x220/8];
} static x;
static void* field_owner[F_COUNT];static uint64_t field_types[4]={N_OBJECT,N_ENUM,N_NAME,N_BOOL};
static int gt=1,current_type=1,sets,events,logs,set_failure,event_failure;
static int phone_inspect_ready=1;
static void* phone_actor_event_address;
static void* (*phone_active)(void);static unsigned (*phone_type)(void);
static void* (*mv_meshset_class)(void);static void* (*mv_skeletal_class)(void);static void* (*mv_scene_class)(void);
static const void* (*qs_settings)(void);
static void putptr(void* p,uintptr_t value){memcpy(p,&value,8);}
static void put32(void* p,unsigned value){memcpy(p,&value,4);}
static void put16(void* p,unsigned short value){memcpy(p,&value,2);}
static void putname(void* p,unsigned value){putptr(p,value);}
static int pv_alive(const void* o){
    if(o==x.pawn||o==x.watch||o==x.meshset||o==x.mesh||o==x.root||o==x.update||o==x.attach)return 1;
    for(unsigned i=0;i<7;i++)if(o==x.cls[i])return 1;return 0;
}
static void phone_inspect_fname(const void* name,char* out,int size){
    unsigned id=0;if(name)memcpy(&id,name,4);snprintf(out,(size_t)size,"%s",id<N_COUNT?names[id]:"");
}
static int cinema_ui_named(const void* o,const char* wanted,int minimum){
    if(!pv_alive(o))return 0;const unsigned char* cls=*(const unsigned char* const*)((const char*)o+0x10);
    if(!pv_alive(cls)||*(const int*)(cls+0x58)<minimum||*(const int*)(cls+0x58)>0x10000)return 0;
    char name[64];phone_inspect_fname(cls+0x18,name,sizeof name);return !strcmp(name,wanted);
}
static int menu_visual_layout(const void* o,const void* cls,int minimum){
    return pv_alive(o)&&*(void* const*)((const char*)o+0x10)==cls&&*(const int*)((const char*)cls+0x58)>=minimum;
}
static const char* cinema_field_property(void* object,const char* wanted,const char* kind){
    const void* cls=*(void* const*)((char*)object+0x10);
    for(unsigned i=0;i<F_COUNT;i++)if(field_owner[i]==cls){
        char name[64],type[32];phone_inspect_fname(x.field[i]+0x28,name,sizeof name);
        phone_inspect_fname(*(void* const*)(x.field[i]+8),type,sizeof type);
        if(!strcmp(name,wanted)&&!strcmp(type,kind))return (const char*)x.field[i];
    }return 0;
}
static int cinema_on_game_thread(void){return gt;}
static void* active(void){return x.pawn;}static unsigned type(void){return (unsigned)current_type;}
static void* meshset_class(void){return x.cls[2];}static void* mesh_class(void){return x.cls[3];}static void* scene_class(void){return x.cls[4];}
static const void* settings(void){return x.settings;}
static int touch_flat_controls_qa_watch(int side){assert(gt&&side>=0&&side<=1);sets++;if(set_failure)return 0;x.settings[0x3c]=(unsigned char)side;return 1;}
static void event(void* actor,const void* fn,void* payload){
    unsigned char* p=payload;assert(gt&&actor==x.watch&&fn==x.update&&p[0]<=1&&x.settings[0x3c]==p[0]);events++;
    if(event_failure==2)return;
    x.watch[0x230]=p[0];memcpy(x.root+0xc8,x.watch+(p[0]?0x240:0x238),8);
    if(!event_failure)p[1]=1;
}
#define LOGF(...) ((void)logs++)
#define RE4_QA_USER_TRANSITIONS 1
#include "qa_watch_binding.h"
static void field(unsigned i,void* owner,unsigned name,unsigned kind,int offset,int bytes,uint64_t flags){
    field_owner[i]=owner;putptr(x.field[i]+8,(uintptr_t)&field_types[kind]);putname(x.field[i]+0x28,name);
    put32(x.field[i]+0x34,1);put32(x.field[i]+0x38,(unsigned)bytes);putptr(x.field[i]+0x40,flags);put32(x.field[i]+0x4c,(unsigned)offset);
}
static void ready(void){
    memset(&x,0,sizeof x);memset(&qa_watch_binding_last,0,sizeof qa_watch_binding_last);
    const unsigned cls_names[]={N_PAWN_CLASS,N_WATCH_CLASS,N_MESHSET_CLASS,N_MESH_CLASS,N_SCENE_CLASS,N_FUNCTION_CLASS};
    const unsigned cls_sizes[]={sizeof x.pawn,sizeof x.watch,0x238,sizeof x.mesh,sizeof x.root,sizeof x.update};
    for(unsigned i=0;i<6;i++){putname(x.cls[i]+0x18,cls_names[i]);put32(x.cls[i]+0x58,cls_sizes[i]);}
    putptr(x.pawn+0x10,(uintptr_t)x.cls[0]);putptr(x.watch+0x10,(uintptr_t)x.cls[1]);putptr(x.meshset+0x10,(uintptr_t)x.cls[2]);
    putptr(x.mesh+0x10,(uintptr_t)x.cls[3]);putptr(x.root+0x10,(uintptr_t)x.cls[4]);
    putptr(x.update+0x10,(uintptr_t)x.cls[5]);putptr(x.attach+0x10,(uintptr_t)x.cls[5]);
    field(F_WATCH,x.cls[0],N_WATCH_ACTOR,0,0x1100,8,0);field(F_MESHSET,x.cls[0],N_MESHSET,0,0x1108,8,0);
    field(F_PAWN,x.cls[1],N_PAWN,0,0x220,8,0);field(F_WATCH_MESH,x.cls[1],N_MESH,0,0x228,8,0);
    field(F_ARM,x.cls[1],N_ACTIVE,1,0x230,1,0);field(F_LEFT,x.cls[1],N_LEFT,2,0x238,8,0);field(F_RIGHT,x.cls[1],N_RIGHT,2,0x240,8,0);
    field(F_MESH,x.cls[2],N_MESH,0,0x228,8,0);
    field(F_NEW,x.update,N_NEW,1,0,1,0x94);field(F_CALLBACK,x.update,N_CALLBACK,3,1,1,0x180);
    putptr(x.field[F_NEW]+0x20,(uintptr_t)x.field[F_CALLBACK]);x.field[F_CALLBACK][0x7a]=1;x.field[F_CALLBACK][0x7b]=0xff;
    putptr(x.pawn+0x1100,(uintptr_t)x.watch);putptr(x.pawn+0x1108,(uintptr_t)x.meshset);
    putptr(x.watch+0x220,(uintptr_t)x.pawn);putptr(x.watch+0x228,(uintptr_t)x.mesh);putptr(x.watch+0x130,(uintptr_t)x.root);
    putptr(x.meshset+0x228,(uintptr_t)x.mesh);putptr(x.mesh+0x20,(uintptr_t)x.meshset);putptr(x.mesh+0xa0,(uintptr_t)x.meshset);
    putptr(x.root+0x20,(uintptr_t)x.watch);putptr(x.root+0xa0,(uintptr_t)x.watch);putptr(x.root+0xc0,(uintptr_t)x.mesh);
    putname(x.watch+0x238,N_LEFT_SOCKET);putname(x.watch+0x240,N_RIGHT_SOCKET);putname(x.root+0xc8,N_LEFT_SOCKET);
    putptr(x.cls[1]+0x48,(uintptr_t)x.update);putptr(x.update+0x28,(uintptr_t)x.attach);
    putname(x.update+0x18,N_UPDATE);putname(x.attach+0x18,N_ATTACH);putptr(x.update+0x20,(uintptr_t)x.cls[1]);putptr(x.attach+0x20,(uintptr_t)x.cls[1]);
    put32(x.update+0xb0,0x0c420000);x.update[0xb4]=2;put16(x.update+0xb6,2);put16(x.update+0xb8,0xffff);
    putptr(x.update+0x50,(uintptr_t)x.field[F_NEW]);
    x.script[0]=0x45;put32(x.script+1,N_ATTACH);put32(x.script+5,N_ATTACH);x.script[13]=0;putptr(x.script+14,(uintptr_t)x.field[F_NEW]);
    x.script[22]=0x16;x.script[23]=0x14;x.script[24]=0x48;putptr(x.script+25,(uintptr_t)x.field[F_CALLBACK]);
    x.script[33]=0x27;x.script[34]=4;x.script[35]=0x0b;x.script[36]=0x53;
    putptr(x.update+0x60,(uintptr_t)x.script);put32(x.update+0x68,37);put32(x.update+0x6c,37);
    x.table[0x218/8]=(void*)event;putptr(x.watch,(uintptr_t)x.table);phone_actor_event_address=(void*)event;
    phone_active=active;phone_type=type;mv_meshset_class=meshset_class;mv_skeletal_class=mesh_class;mv_scene_class=scene_class;qs_settings=settings;
    gt=phone_inspect_ready=1;current_type=1;sets=events=logs=set_failure=event_failure=0;qa_watch_binding_logs=0;
}
int main(void){
    ready();struct fixture before=x;assert(qa_watch_set_and_attach(1)&&sets==1&&events==1);
    assert(qa_watch_binding_last.prevalidated&&qa_watch_binding_last.settings_before==0&&qa_watch_binding_last.settings_after==1
        &&qa_watch_binding_last.arm_before==0&&qa_watch_binding_last.arm_after==1&&qa_watch_binding_last.callback==1&&qa_watch_binding_last.attached);
    before.settings[0x3c]=1;before.watch[0x230]=1;putname(before.root+0xc8,N_RIGHT_SOCKET);assert(!memcmp(&before,&x,sizeof x));
    assert(qa_watch_set_and_attach(0)&&sets==2&&events==2&&qa_watch_binding_last.arm_before==1&&qa_watch_binding_last.arm_after==0);
    // Reproduce the incomplete QA setter: settings already right while actor/actual socket remain left.
    ready();x.settings[0x3c]=1;assert(qa_watch_set_and_attach(1)&&qa_watch_binding_last.settings_before==1
        &&qa_watch_binding_last.arm_before==0&&qa_watch_binding_last.socket_before==N_LEFT_SOCKET&&qa_watch_binding_last.socket_after==N_RIGHT_SOCKET);
    for(unsigned refusal=0;refusal<43;refusal++){
        ready();int side=1;
        if(refusal==0)side=-1;if(refusal==1)side=2;if(refusal==2)gt=0;if(refusal==3)current_type=3;if(refusal==4)phone_inspect_ready=0;
        if(refusal==5)phone_actor_event_address=0;if(refusal==6)phone_active=0;if(refusal==7)mv_scene_class=0;
        if(refusal==8)putname(x.cls[0]+0x18,N_WATCH_CLASS);if(refusal==9)putptr(x.pawn+0x1100,0);
        if(refusal==10)putptr(x.watch+0x220,(uintptr_t)x.meshset);if(refusal==11)putptr(x.watch+0x228,(uintptr_t)x.root);
        if(refusal==12)putptr(x.mesh+0xa0,(uintptr_t)x.watch);if(refusal==13)putptr(x.root+0x20,(uintptr_t)x.meshset);
        if(refusal==14)putptr(x.root+0xc0,(uintptr_t)x.meshset);if(refusal==15)x.watch[0x230]=2;
        if(refusal==16)putname(x.root+0xc8,N_RIGHT_SOCKET);if(refusal==17)putname(x.watch+0x240,N_NONE);
        if(refusal==18)put32(x.field[F_WATCH]+0x34,2);if(refusal==19)put32(x.field[F_MESH]+0x38,4);
        if(refusal==20)put32(x.field[F_ARM]+0x4c,0x10000);if(refusal==21)putptr(x.update+0x20,(uintptr_t)x.cls[0]);
        if(refusal==22)put32(x.update+0xb0,0x0c000000);if(refusal==23)x.update[0xb4]=1;
        if(refusal==24)put16(x.update+0xb6,3);if(refusal==25)put16(x.update+0xb8,1);
        if(refusal==26)putname(x.field[F_NEW]+0x28,N_ACTIVE);if(refusal==27)putptr(x.field[F_NEW]+8,(uintptr_t)&field_types[0]);
        if(refusal==28)put32(x.field[F_CALLBACK]+0x4c,0);if(refusal==29)put32(x.field[F_CALLBACK]+0x34,2);
        if(refusal==30)put32(x.field[F_NEW]+0x38,2);if(refusal==31)putptr(x.field[F_NEW]+0x40,0x180);
        if(refusal==32)putptr(x.field[F_CALLBACK]+0x40,0x80);if(refusal==33)putptr(x.field[F_CALLBACK]+0x40,0x580);
        if(refusal==34)x.field[F_CALLBACK][0x7a]=2;if(refusal==35)putptr(x.field[F_CALLBACK]+0x20,(uintptr_t)x.field[F_CALLBACK]);
        if(refusal==36)put32(x.update+0x68,14);if(refusal==37)x.script[33]=0x28;
        if(refusal==38)putptr(x.script+14,(uintptr_t)x.field[F_CALLBACK]);if(refusal==39)putptr(x.script+25,(uintptr_t)x.field[F_NEW]);
        if(refusal==40)put32(x.script+1,N_UPDATE);if(refusal==41)put32(x.script+9,1);if(refusal==42)x.table[0x218/8]=0;
        before=x;assert(!qa_watch_set_and_attach(side)&&!sets&&!events&&!memcmp(&before,&x,sizeof x));
    }
    ready();set_failure=1;before=x;assert(!qa_watch_set_and_attach(1)&&sets==1&&!events&&!memcmp(&before,&x,sizeof x));
    ready();event_failure=1;assert(!qa_watch_set_and_attach(1)&&sets==1&&events==1&&!qa_watch_binding_last.callback);
    ready();event_failure=2;assert(!qa_watch_set_and_attach(1)&&sets==1&&events==1&&qa_watch_binding_last.arm_after==0);
    ready();for(unsigned i=0;i<12;i++)assert(qa_watch_set_and_attach((int)(i&1)));assert(logs==8);
    puts("PASS: actual watch QA C header: original two-byte in/out event, both-side roundtrip, settings/attachment mismatch repair,43 pre-set GT/reflection/owner/function/script refusals, setter/event failure checks, only expected settings/ActiveArm/socket writes, bounded diagnostics");
    return 0;
}
