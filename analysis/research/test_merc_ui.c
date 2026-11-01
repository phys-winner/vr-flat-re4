// Execute the actual scoped ending repair; native/reflection providers supply controlled object data.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define LOGF(...) ((void)0)
struct cinema_ui_weak {int index,serial;};
enum {PAWN,UI,RING,COMPONENT,WIDGET,ANIMATION,FOREIGN,NODES};
static unsigned char image[0xb000000],nodes[NODES][0x800],classes[NODES][0x80],game[0x5100];
static int alive[NODES],serial[NODES],named[NODES],kind[NODES],sizes[NODES];
static int gt,type,paused,valid,merc,instance,timer,playing,bool_ok,vis_calls,provider_calls,input_locked;
static int swap_weak,swap_active;
static void *active_pawn,*widget_result,*world_ui;
static void unlocked(void){assert(!input_locked);++provider_calls;}
static int index_of(const void* p){for(int i=0;i<NODES;i++)if(p==nodes[i])return i;return -1;}
static int cinema_on_game_thread(void){return gt;}
static int pv_alive(const void* p){unlocked();int i=index_of(p);return i>=0&&alive[i];}
static int cinema_object(const void* p,const void* cls,int minimum){
    unlocked();int i=index_of(p);return i>=0&&kind[i]>=0&&cls==classes[kind[i]]&&sizes[i]>=minimum;
}
static int cinema_ui_named(const void* p,const char* name,int minimum){
    unlocked();int i=index_of(p);const char* names[NODES]={"","VR4MercUI_BP_C","MercRing_BP_C","","MercRing_Message_C","",""};
    return i>=0&&named[i]&&sizes[i]>=minimum&&!strcmp(name,names[i]);
}
static void weak_assign(struct cinema_ui_weak* w,const void* p){
    unlocked();int i=index_of(p);w->index=i;w->serial=i>=0?serial[i]:0;
    if(i==ANIMATION&&swap_weak)++serial[COMPONENT];
    if(i==ANIMATION&&swap_active)active_pawn=nodes[FOREIGN];
}
static void* weak_get(const struct cinema_ui_weak* w){unlocked();return w->index>=0&&w->index<NODES&&alive[w->index]&&serial[w->index]==w->serial?nodes[w->index]:0;}
static void (*cu_weak_assign)(struct cinema_ui_weak*,const void*)=weak_assign;
static void* (*cu_weak_get)(const struct cinema_ui_weak*)=weak_get;
static void* pawn_class(void){unlocked();return classes[PAWN];}
static void* ui_class(void){unlocked();return classes[UI];}
static void* component_class(void){unlocked();return classes[COMPONENT];}
static void* widget_class(void){unlocked();return classes[WIDGET];}
static void* animation_class(void){unlocked();return classes[ANIMATION];}
static void* (*comfort_player_class)(void)=pawn_class;
static void* (*cu_component_class)(void)=component_class,*(*cu_user_class)(void)=widget_class;
static void* active(void){unlocked();return active_pawn;}
static unsigned current_type(void){unlocked();return (unsigned)type;}
static void* (*phone_active)(void)=active;
static unsigned (*phone_type)(void)=current_type;
static void* world_actor(const void* context,void* cls){unlocked();assert(context==active_pawn&&cls==classes[UI]);return world_ui;}
static void* (*fv_title_actor)(const void*,void*)=world_actor;
static int phone_inspect_ready=1;
static int paused_get(void){unlocked();return paused;}
static int valid_get(void){unlocked();return valid;}
static int (*rl_paused)(void)=paused_get,(*rl_player_valid)(void)=valid_get;
static int is_merc(void){unlocked();return merc;}
static int has_instance(void){unlocked();return instance;}
static int get_frame(void* p){unlocked();assert(p==image+0xa4e2290+0xb0);return timer;}
static int is_playing(const void* widget,const void* animation){unlocked();assert(widget==nodes[WIDGET]&&animation==nodes[ANIMATION]);return playing;}
static void* get_widget(const void* c){unlocked();assert(c==nodes[COMPONENT]);return widget_result;}
static void* (*cu_get_widget)(const void*)=get_widget;
static void visible(void* c,int value,int propagate){
    unlocked();assert(c==nodes[COMPONENT]&&value==1&&propagate==0);++vis_calls;
    unsigned short flags;memcpy(&flags,nodes[COMPONENT]+0x14c,2);flags|=0x20;memcpy(nodes[COMPONENT]+0x14c,&flags,2);
}
static void (*mv_visible)(void*,int,int)=visible;
static void* fv_object_property(const void* object,const char* name){
    unlocked();const unsigned char* p=object;
    if(object==nodes[UI]&&!strcmp(name,"MercRing"))return *(void* const*)(p+0x308);
    if(object==nodes[RING]&&!strcmp(name,"WidgetMessage"))return *(void* const*)(p+0x368);
    if(object==nodes[RING]&&!strcmp(name,"MessageWidget"))return *(void* const*)(p+0x498);
    if(object==nodes[WIDGET]&&!strcmp(name,"EndMessage"))return *(void* const*)(p+0x240);
    return 0;
}
static int fv_action_bool_read(void* object,const char* name,int* value){
    unlocked();assert(object==nodes[UI]);
    if(!strcmp(name,"MissionStartCleared")){if(!(bool_ok&1))return 0;*value=nodes[UI][0x2f8];return 1;}
    if(!strcmp(name,"MissionEndCleared")){if(!(bool_ok&2))return 0;*value=nodes[UI][0x300];return 1;}
    return 0;
}
#include "../re4fix_merc_ui.h"
static void ready(void){
    memset(nodes,0,sizeof nodes);memset(game,0xa5,sizeof game);game[0x50aa]=0;
    for(int i=0;i<NODES;i++){alive[i]=serial[i]=named[i]=1;kind[i]=i;sizes[i]=sizeof nodes[i];}
    *(void**)(image+0xa3f5e48)=game;*(void**)(image+0xa579328)=nodes[UI];
    memset(image+0xa4e2290+0xb0,0,0x30);memset(image+0xa50b850,0,0x350);
    *(void**)(nodes[UI]+0x308)=nodes[RING];nodes[UI][0x2f8]=1;
    *(void**)(nodes[RING]+0xe0)=nodes[UI];
    *(void**)(nodes[RING]+0x368)=nodes[COMPONENT];*(void**)(nodes[RING]+0x498)=nodes[WIDGET];
    *(void**)(nodes[COMPONENT]+0x20)=*(void**)(nodes[COMPONENT]+0xa0)=nodes[RING];
    *(unsigned short*)(nodes[COMPONENT]+0x14c)=0x5043;nodes[COMPONENT][0x496]=1;
    *(void**)(nodes[COMPONENT]+0x498)=nodes[WIDGET];*(void**)(nodes[WIDGET]+0x240)=nodes[ANIMATION];
    active_pawn=nodes[PAWN];widget_result=nodes[WIDGET];world_ui=nodes[UI];fv_title_actor=world_actor;
    gt=type=valid=merc=instance=playing=1;bool_ok=3;paused=timer=vis_calls=provider_calls=input_locked=swap_weak=swap_active=0;
    phone_inspect_ready=1;comfort_player_class=pawn_class;cu_component_class=component_class;cu_user_class=widget_class;
    cu_get_widget=get_widget;mv_visible=visible;cu_weak_assign=weak_assign;cu_weak_get=weak_get;
    merc_ui_base=(uintptr_t)image;merc_ui_has_instance=has_instance;merc_ui_is_merc=is_merc;
    merc_ui_frame=get_frame;merc_ui_playing=is_playing;merc_ui_class=ui_class;merc_ui_animation_class=animation_class;
    touch_merc_ui_cleanup();
}
static unsigned char saved_nodes[sizeof nodes],saved_game[sizeof game],saved_countdown[0x30],saved_work[0x350];
static void remember(void){
    memcpy(saved_nodes,nodes,sizeof nodes);memcpy(saved_game,game,sizeof game);
    memcpy(saved_countdown,image+0xa4e2290+0xb0,sizeof saved_countdown);memcpy(saved_work,image+0xa50b850,sizeof saved_work);
}
static void preserved(int repaired){
    if(repaired)memcpy(nodes[COMPONENT]+0x14c,saved_nodes+COMPONENT*sizeof nodes[0]+0x14c,2);
    assert(!memcmp(saved_nodes,nodes,sizeof nodes)&&!memcmp(saved_game,game,sizeof game));
    assert(!memcmp(saved_countdown,image+0xa4e2290+0xb0,sizeof saved_countdown));
    assert(!memcmp(saved_work,image+0xa50b850,sizeof saved_work));
    if(repaired)*(unsigned short*)(nodes[COMPONENT]+0x14c)|=0x20;
}
#ifdef MERC_UI_NATIVE_FIXTURE
#include MERC_UI_NATIVE_FIXTURE
#endif
int main(void){
#ifdef MERC_UI_NATIVE_FIXTURE
    verify_merc_ui_installer();
#endif
    ready();remember();touch_merc_ui_tick();assert(vis_calls==1&&merc_ui_owned.used&&(*(unsigned short*)(nodes[COMPONENT]+0x14c)&0x20));preserved(1);
    for(int i=0;i<32;i++)touch_merc_ui_tick();assert(vis_calls==1);
    // Native Intro may hide it again; only the live EndMessage phase permits a second original call.
    *(unsigned short*)(nodes[COMPONENT]+0x14c)&=~0x20;touch_merc_ui_tick();assert(vis_calls==2);
    for(unsigned end=0;end<5;end++){
        ready();touch_merc_ui_tick();assert(vis_calls==1);
        if(end==0)nodes[UI][0x300]=1;if(end==1)playing=0;if(end==2)type=4;
        if(end==3)paused=1;if(end==4)image[0xa50b850+0xe9]=1;
        remember();touch_merc_ui_tick();assert(vis_calls==1&&!merc_ui_owned.used);preserved(0);
        // Cleanup never restores the late Intro's hidden value or touches a new native object.
        touch_merc_ui_cleanup();assert((*(unsigned short*)(nodes[COMPONENT]+0x14c)&0x20));
    }
    for(unsigned refusal=0;refusal<45;refusal++){
        ready();
        switch(refusal){
        case 0:merc_ui_base=0;break;case 1:gt=0;break;case 2:type=2;break;case 3:paused=1;break;
        case 4:valid=0;break;case 5:merc=0;break;case 6:*(void**)(image+0xa3f5e48)=0;break;
        case 7:game[0x50aa]=0x10;break;case 8:image[0xa50b850+0xe9]=1;break;case 9:image[0xa4e2290+0xb0]=1;break;
        case 10:timer=30;break;case 11:world_ui=nodes[FOREIGN];break;case 12:instance=0;break;case 13:active_pawn=0;break;
        case 14:alive[PAWN]=0;break;case 15:kind[PAWN]=FOREIGN;break;case 16:*(void**)(image+0xa579328)=0;break;
        case 17:alive[UI]=0;break;case 18:kind[UI]=FOREIGN;break;case 19:named[UI]=0;break;
        case 20:bool_ok=2;break;case 21:nodes[UI][0x2f8]=0;break;case 22:bool_ok=1;break;case 23:nodes[UI][0x300]=1;break;
        case 24:*(void**)(nodes[UI]+0x308)=0;break;case 25:alive[RING]=0;break;case 26:named[RING]=0;break;
        case 27:*(void**)(nodes[RING]+0xe0)=nodes[FOREIGN];break;case 28:nodes[RING][0x58]=0x20;break;
        case 29:*(void**)(nodes[RING]+0x368)=0;break;case 30:alive[COMPONENT]=0;break;case 31:kind[COMPONENT]=FOREIGN;break;
        case 32:*(void**)(nodes[COMPONENT]+0x20)=nodes[FOREIGN];break;case 33:*(void**)(nodes[COMPONENT]+0xa0)=nodes[FOREIGN];break;
        case 34:*(void**)(nodes[RING]+0x498)=0;break;case 35:alive[WIDGET]=0;break;case 36:kind[WIDGET]=FOREIGN;break;
        case 37:named[WIDGET]=0;break;case 38:widget_result=nodes[FOREIGN];break;case 39:nodes[COMPONENT][0x429]=1;break;
        case 40:*(void**)(nodes[WIDGET]+0x240)=0;break;case 41:alive[ANIMATION]=0;break;case 42:kind[ANIMATION]=FOREIGN;break;
        case 43:*(unsigned short*)(nodes[COMPONENT]+0x14c)|=0x400;break;case 44:playing=0;break;
        }
        remember();touch_merc_ui_tick();assert(!vis_calls&&!merc_ui_owned.used);preserved(0);
        if(refusal==1)assert(!provider_calls);
    }
    for(type=0;type<10;type++)if(type!=1){int next=type;ready();type=next;remember();touch_merc_ui_tick();assert(!vis_calls);preserved(0);type=next;}
    ready();swap_weak=1;remember();touch_merc_ui_tick();assert(!vis_calls&&!merc_ui_owned.used);preserved(0);
    ready();swap_active=1;remember();touch_merc_ui_tick();assert(!vis_calls&&!merc_ui_owned.used);preserved(0);
    ready();nodes[COMPONENT][0x496]=0;remember();touch_merc_ui_tick();assert(vis_calls==1&&!nodes[COMPONENT][0x496]);preserved(1);
    ready();timer=-1;remember();touch_merc_ui_tick();assert(vis_calls==1);preserved(1);
    puts("PASS: actual Merc ending C restores only hidden owned EndMessage component through original visibility API; repeat idempotence, late Intro re-hide, completion/context/start/pause cleanup,45 refusals plus native contexts/weak serial/current pawn races; byte-identical game/timer/work/other objects, offscreen untouched");
    return 0;
}
