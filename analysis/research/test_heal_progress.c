#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
static int g_touch_context=1,g_touch_cancel,gt=1,paused,valid=1,interactivity=2,trigger=1;
static unsigned type=1;static float fraction=.25f;static unsigned fraction_calls,rename_item,cancel_getter;
static unsigned char pawn[0x1100],other_pawn[0x1100],hand[0x510],left[0x510],item[0x5d0],grip[0x460];
static unsigned char classes[4][0x60];static void* chains[4][1];
static void* active;static void* current;static int registered=1;
static int qs_gt(void){return gt;}
static unsigned long long qs_name(const void* p){unsigned long long n=0;if(p)memcpy(&n,(const char*)p+0x18,8);return n;}
static void* cls_hand(void){return classes[0];}static void* cls_pawn(void){return classes[1];}
static void* cls_item(void){return classes[2];}static void* cls_grip(void){return classes[3];}
static void* active_pawn(void){return active;}static unsigned pawn_type(void){return type;}
static void* hand_pawn(const void* h){assert(h==hand||h==left);return pawn;}
static void* consumable(const void* p){assert(p==pawn);return item;}
static void* primary(const void* p){assert(p==item);return left;}
static void* native_hand(const void* p,unsigned char side){assert(p==pawn);return side ? hand:left;}
static void* (*qs_native_hand)(const void*,unsigned char)=native_hand;
static void* current_grip(const void* h){assert(h==left);return current;}
static int is_paused(void){return paused;}static int player_valid(void){return valid;}
static int native_interactivity(const void* p){assert(p==item);return interactivity;}
static int native_trigger(const void* h){assert(h==left);return trigger;}
static float native_fraction(const void* p){assert(p==item);fraction_calls++;if(rename_item)item[0x18]^=1;if(cancel_getter)g_touch_cancel=1;return fraction;}
static int reload_grip_registered(void* p){return registered&&p==grip;}
static void* (*tw_hand_class)(void)=cls_hand,*(*tw_pawn_class)(void)=cls_pawn,*(*tw_grip_class)(void)=cls_grip;
static void* (*tw_consumable)(const void*)=consumable,*(*phone_player)(const void*)=hand_pawn;
static void* (*phone_active)(void)=active_pawn;static unsigned (*phone_type)(void)=pawn_type;
static void* (*rl_primary)(const void*)=primary,*(*f_GetCurrentGrip)(const void*)=current_grip;
static int (*rl_paused)(void)=is_paused,(*rl_player_valid)(void)=player_valid;
/* TYPED_GUARD */
#define LOGF(...) ((void)0)
#include "HEAL_HEADER"
static void ready(void){
    memset(pawn,0,sizeof pawn);memset(hand,0,sizeof hand);memset(left,0,sizeof left);
    memset(item,0,sizeof item);memset(grip,0,sizeof grip);memset(classes,0,sizeof classes);
    const unsigned sizes[]={sizeof hand,sizeof pawn,sizeof item,sizeof grip};
    for(unsigned i=0;i<4;i++){*(unsigned*)(classes[i]+0x58)=sizes[i];chains[i][0]=classes[i]+0x30;*(void**)(classes[i]+0x30)=chains[i];}
    *(void**)(hand+0x10)=*(void**)(left+0x10)=classes[0];*(void**)(pawn+0x10)=classes[1];
    *(void**)(item+0x10)=classes[2];*(void**)(grip+0x10)=classes[3];
    *(void**)(item+0x3c0)=pawn;*(void**)(item+0x270)=grip;*(void**)(grip+0x20)=item;
    hand[0x314]=1;item[0x4f0]=1;current=grip;active=pawn;
    heal_class=cls_item;heal_fraction=native_fraction;heal_interactivity=native_interactivity;heal_trigger=native_trigger;
    gt=valid=registered=trigger=1;paused=g_touch_cancel=rename_item=cancel_getter=0;type=g_touch_context=1;interactivity=2;
    fraction=.25f;fraction_calls=0;heal_snapshot=0;
}
static void tick(long now){
    unsigned char before[sizeof pawn+sizeof hand+sizeof left+sizeof item+sizeof grip];unsigned pos=0;
    const void* objects[]={pawn,hand,left,item,grip};const unsigned sizes[]={sizeof pawn,sizeof hand,sizeof left,sizeof item,sizeof grip};
    for(unsigned i=0;i<5;i++){memcpy(before+pos,objects[i],sizes[i]);pos+=sizes[i];}
    touch_heal_progress_tick(hand,now);pos=0;
    if(!rename_item)for(unsigned i=0;i<5;i++){assert(!memcmp(before+pos,objects[i],sizes[i]));pos+=sizes[i];}
}
static void reject(void){float out=9;tick(1000);assert(!touch_heal_progress_read(1000,&out)&&!out);}
int main(void){
    ready();tick(1000);float out=0;assert(touch_heal_progress_read(1000,&out)&&out==.25f&&fraction_calls==1);
    assert(touch_heal_progress_read(1200,&out));assert(!touch_heal_progress_read(1201,&out)&&!out);
    assert(!touch_heal_progress_read(999,&out));assert(!touch_heal_progress_read(0,&out));assert(!touch_heal_progress_read(1000,0));
    for(unsigned i=1;i<20;i++){ready();fraction=(float)i/20;tick(1000);assert(touch_heal_progress_read(1000,&out)&&fabsf(out-fraction)<1e-6f);}
    const float bad[]={0,1,-.1f,1.1f,NAN,INFINITY};for(unsigned i=0;i<6;i++){ready();fraction=bad[i];reject();}
    ready();g_touch_cancel=1;reject();assert(!fraction_calls);
    ready();paused=1;reject();ready();valid=0;reject();ready();g_touch_context=3;reject();ready();type=3;reject();
    ready();active=other_pawn;reject();ready();item[0x5c4]=1;reject();ready();item[0x4f0]=0;reject();
    ready();interactivity=1;reject();ready();trigger=0;reject();ready();registered=0;reject();ready();current=0;reject();
    ready();*(void**)(item+0x3c0)=other_pawn;reject();ready();*(void**)(grip+0x20)=pawn;reject();
    ready();*(unsigned*)(classes[2]+0x58)=0x5cf;reject();ready();chains[2][0]=classes[1]+0x30;reject();
    ready();*(unsigned*)(classes[0]+0x58)=0x50f;reject();ready();*(unsigned*)(classes[1]+0x58)=0x10cf;reject();
    ready();rename_item=1;reject();ready();cancel_getter=1;reject();
    ready();tick(1000);uint64_t good=heal_snapshot;gt=0;touch_heal_progress_tick(hand,1010);assert(heal_snapshot==good&&fraction_calls==1);
    gt=1;touch_heal_progress_tick(left,1010);touch_heal_progress_tick(0,1010);assert(heal_snapshot==good&&fraction_calls==1);
    ready();type=g_touch_context=2;paused=1;trigger=0;item[0x4f0]=0;*(void**)(item+0x5a0)=left;tick(1000);
    assert(touch_heal_progress_read(1000,&out)&&out==.25f);*(void**)(item+0x5a0)=0;reject();
    ready();*(void**)(item+0x5a0)=(void*)(uintptr_t)0x1234;reject();assert(!fraction_calls);
    ready();tick(1000);heal_snapshot=(heal_snapshot&0xffffffffu)|((uint64_t)0xfffffff0u<<32);
    assert(touch_heal_progress_read(0x20,&out)&&out==.25f);assert(!touch_heal_progress_read(0x100,&out));
#ifdef _WIN32
    unsigned long base=0x10000000UL;unsigned char* image=VirtualAlloc((void*)(uintptr_t)base,0xa000000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(image);
    /* NATIVE_FIXTURE */
    assert(VirtualFree(image,0,MEM_RELEASE));
#endif
    puts("PASS: actual read-only consumable progress, held trigger/native inventory use, class/owner/grip/active/thread/cancel guards, exact float snapshot freshness/wrap and native installer refusals");
    return 0;
}
