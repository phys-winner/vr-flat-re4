#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile int g_touch_context,g_phone_request;
static volatile long g_phone_zoom_start;
static long now=1000;
static long ptr_now_ms(void) { return now; }
static void phone_view_inspect(void* pawn,void* device) { (void)pawn;(void)device; }
static void* cinema_subtitle_probe(void* pawn) { (void)pawn;return 0; }
static void* inspected_object;
static int inspected_count;
static void touch_interact_inspect_object(void* object) { inspected_object=object;inspected_count++; }
#define LOGF(...) do { printf(__VA_ARGS__); putchar('\n'); } while (0)
static void obj_name(const void* object,char* name,int n) {
    const char* text=object ? *(const char* const*)((const char*)object+0x18) : 0;
    snprintf(name,n,"%s",text ? text : "");
}
#include "../re4fix_phone.h"

static unsigned char hand[0x400],pawn[0x1300],actor[0x800],device[0x500];
static unsigned char pawn_class[0x140],device_class[0x140],function_class[0x140],actor_class[0x140];
static void* actor_chain[1];
static unsigned char object_property_class[0x40],device_prop[0x90],button_fn[0xe0];
static void* device_vtable[70];
static unsigned active_type;
static void* active_pawn;
static int events,lookups,skip_calls;
static void skip_line(void* receiver) {
    assert(receiver==actor && actor[0x734] && !actor[0x735]);skip_calls++;
    ++*(int*)(actor+0x718);
}
static void* get_player(const void* h) { assert(h==hand); return pawn; }
static void* get_active(void) { return active_pawn; }
static unsigned get_type(void) { return active_type; }
static void* get_class(void) { return actor_class; }
static void* get_actor(const void* p,void* cls) {
    assert(p==pawn && cls==actor_class); lookups++; return actor;
}
static void process_event(void* receiver,void* fn,void* params) {
    assert(receiver==device && fn==button_fn && params==0); events++;
}
static void fname(const void* name,void* out) {
    const char* text=*(const char* const*)name;
    struct { unsigned short* data; int num,max; }* fs=out;
    fs->num=fs->max=(int)strlen(text)+1;
    fs->data=malloc((size_t)fs->num*2);
    for (int i=0;i<fs->num;i++) fs->data[i]=(unsigned char)text[i];
}
static void named(void* object,const char* name) { *(const char**)((char*)object+0x18)=name; }
static void ready(void) {
    memset(actor_class,0,sizeof actor_class);*(int*)(actor_class+0x58)=sizeof actor;
    actor_chain[0]=actor_class+0x30;*(void**)(actor_class+0x30)=actor_chain;
    memset(actor,0,sizeof actor); named(actor,"MockCommunicator");
    *(void**)(actor+0x10)=actor_class;
    memset(pawn_class,0,sizeof pawn_class); named(pawn_class,"VR4CommunicatorPlayerPawn_BP_C");
    memset(device_class,0,sizeof device_class); named(device_class,"CommunicatorDevice_C");
    memset(function_class,0,sizeof function_class); named(function_class,"Function");
    memset(device_prop,0,sizeof device_prop); memset(button_fn,0,sizeof button_fn);
    named(device,"CommunicatorDevice_C_mock"); named(pawn,"CommunicatorPawn_mock");
    *(void**)(pawn+0x10)=pawn_class; *(void**)(device+0x10)=device_class;
    *(int*)(pawn_class+0x58)=0x11a8; *(void**)(pawn_class+0x70)=device_prop;
    *(const char**)(object_property_class+0x0)="ObjectProperty";
    *(void**)(device_prop+0x8)=object_property_class; *(const char**)(device_prop+0x28)="Device";
    *(int*)(device_prop+0x34)=1; *(int*)(device_prop+0x38)=8; *(int*)(device_prop+0x4c)=0x1188;
    *(void**)(pawn+0x1188)=device; *(void**)(device_class+0x48)=button_fn;
    named(button_fn,"ButtonShortcut"); *(void**)(button_fn+0x10)=function_class;
    *(void**)(button_fn+0x20)=device_class; *(unsigned*)(button_fn+0xb0)=0x0c000000;
    *(unsigned short*)(button_fn+0xb8)=0xffff;
    *(void***)device=device_vtable; device_vtable[0x218/sizeof(void*)]=(void*)process_event;
}
static void action(void) { g_phone_request=1; touch_phone_tick(hand); assert(!g_phone_request); }
int main(int argc,char** argv) {
    (void)argv;
    if (argc==999) install_touch_phone(0);
    phone_player=get_player; phone_active=get_active; phone_type=get_type;
    phone_class=get_class; phone_actor=get_actor; phone_actor_event_address=(void*)process_event;
    phone_inspect_name=fname; phone_inspect_free=free; phone_inspect_ready=1;
    active_type=5; active_pawn=pawn; ready();
    g_phone_request=1; touch_phone_tick(hand); assert(g_phone_request==1 && events==0 && lookups==0);
    hand[0x314]=1; touch_phone_tick(hand); assert(events==1 && g_touch_context==5);
    assert(actor[0x734]==0 && actor[0x735]==0 && *(void**)(actor+0x6f0)==0);
    g_phone_request=2; touch_phone_tick(hand); assert(events==1);
    action(); assert(events==2); // Audio/data NULL remain the Blueprint receiver's decision.
    active_type=3; action(); assert(events==2 && g_touch_context==3);
    active_type=5; active_pawn=0; g_phone_request=1; touch_phone_tick(hand);
    assert(g_phone_request==1 && events==2);
    active_pawn=pawn; touch_phone_tick(hand); assert(!g_phone_request && events==3);
    touch_phone_tick(hand); assert(events==3);
    ready(); *(int*)(device_prop+0x4c)=0x11a8; action(); assert(events==3);
    ready(); *(int*)(device_prop+0x38)=4; action(); assert(events==3);
    ready(); *(int*)(device_prop+0x34)=2; action(); assert(events==3);
    ready(); *(void**)(pawn+0x1188)=0; action(); assert(events==3);
    ready(); named(device_class,"OtherDevice_C"); action(); assert(events==3);
    ready(); named(button_fn,"ButtonPressed__DelegateSignature"); action(); assert(events==3);
    ready(); *(void**)(button_fn+0x20)=pawn_class; action(); assert(events==3);
    ready(); *(unsigned*)(button_fn+0xb0)=0x0c120000; action(); assert(events==3);
    ready(); button_fn[0xb4]=1; action(); assert(events==3);
    ready(); *(unsigned short*)(button_fn+0xb6)=8; action(); assert(events==3);
    ready(); *(unsigned short*)(button_fn+0xb8)=0; action(); assert(events==3);
    ready(); device_vtable[0x218/sizeof(void*)]=0; action(); assert(events==3);
    ready(); action(); assert(events==4);
    active_type=1; touch_phone_tick(hand); assert(g_touch_context==1 && events==4);
    assert(!touch_phone_answer_visible(now) && !phone_answer_snapshot && !g_phone_zoom_start);
    active_type=5;ready();g_phone_request=0;touch_phone_tick(hand);
    assert(touch_phone_answer_visible(now) && !touch_phone_answer_visible(now-1) && !touch_phone_answer_visible(now+401));
    actor[0x734]=1;now+=16;touch_phone_tick(hand);assert(!touch_phone_answer_visible(now));
    actor[0x734]=0;actor[0x735]=1;now+=16;touch_phone_tick(hand);assert(!touch_phone_answer_visible(now));
    actor[0x735]=0;now+=16;touch_phone_tick(hand);assert(touch_phone_answer_visible(now));
    *(int*)(actor_class+0x58)=0x730;touch_phone_tick(hand);assert(!phone_answer_snapshot);
    ready();action();assert(!touch_phone_answer_visible(now));
    now+=16;touch_phone_tick(hand);assert(!touch_phone_answer_visible(now));
    phone_skip_line=skip_line;actor[0x734]=1;now+=16;touch_phone_tick(hand);
    assert(touch_phone_skip_visible(now) && !touch_phone_answer_visible(now));
    assert(!touch_phone_skip_visible(now-1) && !touch_phone_skip_visible(now+401));
    int before_events=events;action();assert(skip_calls==1 && events==before_events && *(int*)(actor+0x718)==1);
    action();assert(skip_calls==2 && events==before_events && *(int*)(actor+0x718)==2);
    actor[0x735]=1;now+=16;touch_phone_tick(hand);assert(!touch_phone_skip_visible(now));
    actor[0x735]=0;phone_skip_line=0;touch_phone_tick(hand);assert(!touch_phone_skip_visible(now));
    phone_skip_line=skip_line;touch_phone_tick(hand);assert(touch_phone_skip_visible(now));
    active_type=1;touch_phone_tick(hand);assert(!touch_phone_skip_visible(now) && !phone_skip_snapshot);
    ready();phone_inspect_objects(pawn,actor,pawn);
    assert(inspected_count==1 && inspected_object==pawn); // Only the explicitly requested first object, including followed children.
    phone_inspect_objects(0,actor,hand);assert(inspected_count==1);
    puts("PASS: native ButtonShortcut/dispatch and metadata/inactive guards; fresh answer snapshot, hide on answer/running/finished, expiry and layout refusal");
    puts("PASS: running conversation exposes a fresh skip snapshot and one native line skip per tap; finished/missing-native/expiry/context guards");
    return 0;
}
