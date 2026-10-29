#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
struct cinema_ui_weak {int index,serial;};
struct fv_ui_object {void* actor,*component,*screen;};
struct fv_ui_click_state {unsigned char action,pad[3];int count;float press[3],release[3],press_time,release_time;void* hand;};
static unsigned char objects[16][0x1000],classes[11][0x100],title_fields[4][0x80];
static int alive[16],serials[16],on_gt=1,confirm_option=1,originals,queued,cancelled,answer,transport=1,context=7,cover_mode,range_mode;
static long now=1000;
static int intro_skip,logo_skips;static void* title_table[128];
static void* phone_actor_event_address;static int mobile_prefs_intro_skip(void){return intro_skip;}
static long g_lobby_menu_ms,g_menu_tick_ms;
static volatile float g_look_yaw,g_look_pitch;
static void* active,*top;
static const char* names[]={"","NewGameConfirmScreen_BP_C","UI_PauseMenu_Content_NewGame_BP_C","Function",
    "SubAction_A","SubAction_B","Receiver_C","OptionsUIScreen_BP_C","Other_C","VR4UIButton_Confirm","FrontendManager_C","BlueprintGeneratedClass",
    "ShootingRange_Configuration_Screen_BP_C","ShootingRange_Panel_Button_BP_C","StartButton_GEN_VARIABLE_ShootingRange_Panel_Button_BP_C_CAT_655","VRUIButton","InpActEvt_Button A_K2Node_InputActionEvent_9","Key","StructProperty"};
static int index_of(const void* p){for(int i=0;i<16;i++)if(p==objects[i])return i;for(int i=0;i<11;i++)if(p==classes[i])return 16+i;return -1;}
static int pv_alive(const void* p){int i=index_of(p);return i>=16 || (i>=0 && alive[i]);}
static void weak_assign(struct cinema_ui_weak* ref,const void* object){ref->index=index_of(object);ref->serial=ref->index<0?0:ref->index>=16?1:serials[ref->index];}
static void* weak_get(const struct cinema_ui_weak* ref){return ref->index>=16 && ref->index<27 ? classes[ref->index-16]
    :ref->index>=0 && ref->index<16 && alive[ref->index] && serials[ref->index]==ref->serial ? objects[ref->index]:0;}
static void (*cu_weak_assign)(struct cinema_ui_weak*,const void*)=weak_assign;
static void* (*cu_weak_get)(const struct cinema_ui_weak*)=weak_get;
static void phone_inspect_fname(const void* value,char* out,int capacity){
    unsigned id;memcpy(&id,value,4);const char* name=id<sizeof names/sizeof *names ? names[id]:"";
    assert(capacity>(int)strlen(name));memcpy(out,name,strlen(name)+1);
}
static int cinema_ui_named(const void* object,const char* name,int size){
    if(!pv_alive(object))return 0;const char* cls=*(const char* const*)((const char*)object+0x10);
    char kind[96];phone_inspect_fname(cls+0x18,kind,sizeof kind);return !strcmp(kind,name) && *(const int*)(cls+0x58)>=size;
}
static void obj_name(const void* object,char* out,int capacity){phone_inspect_fname((const char*)object+0x18,out,capacity);}
static void* fv_object_property(const void* object,const char* name){
    if(range_mode && object==objects[0] && !strcmp(name,"StartButton"))return objects[12];
    if(object==objects[0] && !strcmp(name,"TrifoldBaseBP"))return objects[4];
    if(object==objects[4] && !strcmp(name,"UIScreen"))return objects[0];
    if(object==objects[4] && !strcmp(name,"DoorRightScreen"))return objects[1];
    if(cover_mode && object==objects[4] && !strcmp(name,"CentralCoverScreen"))return objects[7];
    return 0;
}
static void* get_active(void){return active;}static unsigned get_type(void){return (unsigned)context;}
static void* get_manager(void){return objects[6];}static void* peek(void* manager){assert(manager==objects[6]);return top;}
static void* button_class(void){return classes[5];}
static void* screen_class(void){return classes[0];}static void* menu_pawn_class(void){return classes[6];}static void* child_class(void){return classes[9];}
static void* (*phone_active)(void)=get_active,*(*fv_ui_manager)(void)=get_manager,*(*fv_button_class)(void)=button_class;
static void* (*fv_screen_class)(void)=screen_class,*(*mv_pawn_class)(void)=menu_pawn_class;
static void* (*pv_child_class)(void)=child_class;
static void* (*fv_peek_screen)(void*)=peek;
static unsigned (*phone_type)(void)=get_type;
static int enabled(const void* object){return pv_alive(object) && ((const unsigned char*)object)[0x264];}
static int (*fv_ui_enabled)(const void*)=enabled;
static int menu_visual_layout(const void* object,const void* cls,int size){return pv_alive(object) && (*(const void* const*)((const char*)object+0x10)==cls
    || (cls==classes[0] && object==objects[7])) && *(const int*)((const char*)cls+0x58)>=size;}
static int cinema_on_game_thread(void){return on_gt;}
static int mobile_prefs_start_confirm(void){return confirm_option;}
static const char* mobile_android_language(void){return "en";}
static int mobile_android_confirm_request(int kind,int russian,long ms){assert(kind==7 && !russian && ms==now);if(!transport)return 0;queued++;return 1;}
static int mobile_android_confirm_take(int kind,long ms){assert(kind==7 && ms==now);int result=answer;answer=0;return result;}
static int mobile_android_confirm_active(int kind){assert(kind==7);return queued!=0;}
static void mobile_android_confirm_cancel_kind(int kind){assert(kind==7);cancelled++;}
static long ptr_now_ms(void){return now;}
static int patch_code(void* target,void* hook){(void)target;(void)hook;return -1;}
static void native_click(void* component,const struct fv_ui_object* object,const struct fv_ui_click_state* state){
    assert(component==objects[2] && object->component==component
        && object->actor==objects[range_mode ? 13:1] && object->screen==objects[range_mode ? 0:1]);
    assert(!state->hand || state->hand==objects[5]);originals++;
}
#define RE4FIX_UI_START_GATE native_click
#define LOGF(...) ((void)0)
#include "../re4fix_ui_start.h"
static int phone_inspect_ready=1,g_touch_context=7;
static const char* cinema_field_property(const void* object,const char* name,const char* kind){
    if(object!=objects[11] || strcmp(kind,"BoolProperty"))return 0;
    return !strcmp(name,"allowButtonInput?") ? (const char*)title_fields[0]
        :!strcmp(name,"ReadyForStart") ? (const char*)title_fields[1]
        :!strcmp(name,"LogoIntroComplete") ? (const char*)title_fields[2]
        :!strcmp(name,"SkipLogo") ? (const char*)title_fields[3]:0;
}
static void* title_find(void* cls,void* outer,const unsigned short* path,int exact){
    assert(!cls&&!outer&&!exact&&path[0]=='/'&&path[1]=='G');return classes[7];
}
static void* title_actor(const void* pawn,void* cls){assert(pawn==active&&cls==classes[7]);return objects[11];}
#include "../re4fix_ui_actions.h"
static void ready(void){
    fv_gameplay_entered=intro_skip=logo_skips=0;phone_actor_event_address=0;
    memset(objects,0,sizeof objects);memset(classes,0,sizeof classes);memset(&fv_start_pending,0,sizeof fv_start_pending);
    for(int i=0;i<16;i++){alive[i]=serials[i]=1;*(void**)(objects[i]+0x10)=classes[7];}
    const unsigned class_names[]={7,1,2,3,6,8,8,8};
    for(int i=0;i<11;i++){if(i<8)*(unsigned*)(classes[i]+0x18)=class_names[i];*(int*)(classes[i]+0x58)=0x1000;}
    *(unsigned*)(classes[7]+0x18)=10;*(void**)(classes[7]+0x10)=classes[8];
    *(unsigned*)(classes[8]+0x18)=11;*(int*)(classes[8]+0x58)=0x1000;*(void**)(objects[11]+0x10)=classes[7];
    memset(title_fields,0,sizeof title_fields);
    for(int i=0;i<4;i++){*(int*)(title_fields[i]+0x34)=*(int*)(title_fields[i]+0x38)=1;*(int*)(title_fields[i]+0x4c)=0x600+i;title_fields[i][0x7b]=(unsigned char)(1u<<(i+2));objects[11][0x600+i]=title_fields[i][0x7b];}
    fv_title_find=title_find;fv_title_actor=title_actor;
    *(void**)(objects[0]+0x10)=classes[0];*(void**)(objects[1]+0x10)=classes[1];*(void**)(objects[4]+0x10)=classes[2];
    *(void**)(objects[2]+0x10)=classes[5];*(void**)(objects[2]+0x20)=objects[1];*(void**)(objects[2]+0x308)=objects[1];
    *(unsigned*)(objects[2]+0x18)=9;objects[2][0x264]=1;
    active=objects[3];top=objects[0];on_gt=confirm_option=transport=phone_inspect_ready=1;
    originals=queued=cancelled=answer=cover_mode=range_mode=0;now=1000;context=g_touch_context=7;g_lobby_menu_ms=g_menu_tick_ms=0;
}
static void click(void){
    struct fv_ui_object object={objects[range_mode ? 13:1],objects[2],objects[range_mode ? 0:1]};
    struct fv_ui_click_state state={0};state.count=1;state.hand=objects[5];
    hook_ui_start_click(objects[2],&object,&state);
}
static void range_ready(void){
    ready();range_mode=1;context=g_touch_context=1;
    *(unsigned*)(classes[0]+0x18)=12;*(unsigned*)(classes[10]+0x18)=13;
    *(void**)(objects[12]+0x10)=classes[9];*(void**)(objects[12]+0x20)=objects[0];*(void**)(objects[12]+0xa0)=objects[0];
    *(void**)(objects[12]+0x1f8)=objects[13];*(void**)(objects[13]+0x10)=classes[10];*(unsigned*)(objects[13]+0x18)=14;
    *(void**)(objects[2]+0x20)=objects[13];*(void**)(objects[2]+0x308)=objects[0];*(unsigned*)(objects[2]+0x18)=15;
}
static void start_tests(void){
    ready();confirm_option=0;click();assert(originals==1 && !queued);
    ready();click();click();assert(queued==1 && !originals && fv_start_pending.used);
    fv_start_confirm_tick(now);assert(!originals && fv_start_pending.used);
    answer=1;fv_start_confirm_tick(now);assert(originals==1 && !fv_start_pending.used);
    fv_start_confirm_tick(now);assert(originals==1);
    ready();click();answer=-1;fv_start_confirm_tick(now);assert(!originals && !fv_start_pending.used);
    for(int i=0;i<6;i++){
        ready();click();answer=1;
        if(i==0)active=objects[7];if(i==1)top=objects[7];if(i==2)serials[2]++;
        if(i==3)objects[2][0x264]=0;if(i==4)serials[5]++;if(i==5)*(void**)(objects[2]+0x20)=objects[7];
        fv_start_confirm_tick(now);assert(!originals && !fv_start_pending.used && cancelled==1);
    }
    ready();transport=0;click();assert(!originals && !queued && !fv_start_pending.used);
    ready();click();queued=0;fv_start_confirm_tick(now);assert(!originals && !fv_start_pending.used);
    ready();*(unsigned*)(objects[2]+0x18)=8;click();assert(originals==1 && !queued);
    ready();*(unsigned*)(classes[1]+0x18)=8;click();assert(originals==1 && !queued);
    ready();on_gt=0;click();assert(originals==1 && !queued);
    ready();click();on_gt=0;answer=1;fv_start_confirm_tick(now);assert(!originals && fv_start_pending.used);
    on_gt=1;fv_start_confirm_tick(now);assert(originals==1);
    range_ready();click();assert(queued==1&&!originals);answer=1;fv_start_confirm_tick(now);assert(originals==1&&!fv_start_pending.used);
    range_ready();click();answer=-1;fv_start_confirm_tick(now);assert(!originals&&!fv_start_pending.used);
    range_ready();confirm_option=0;click();assert(originals==1&&!queued);
    range_ready();*(void**)(objects[12]+0x1f8)=objects[14];click();assert(originals==1&&!queued);
    range_ready();click();*(void**)(objects[12]+0xa0)=objects[14];answer=1;fv_start_confirm_tick(now);assert(!originals&&cancelled==1);
    range_ready();click();serials[13]++;answer=1;fv_start_confirm_tick(now);assert(!originals&&cancelled==1);
    puts("PASS: optional exact Story/Range native Start click/once-only replay, No/duplicate/busy transport; exact original Range StartButton child ownership, pawn/screen/component/actor/hand weak replacement and enabled/GT guards");
}
static unsigned char a_code[32],b_code[32],graph[64];
static void function(unsigned index,unsigned name,unsigned char* code,int count){
    void* fn=objects[index];*(void**)((char*)fn+0x10)=classes[3];*(unsigned*)((char*)fn+0x18)=name;
    *(void**)((char*)fn+0x20)=classes[2];*(unsigned short*)((char*)fn+0xb8)=0xffff;
    struct {unsigned char* data;int count,max;} script={code,count,count};memcpy((char*)fn+0x60,&script,sizeof script);
}
static void actions_ready(void){
    ready();memset(a_code,0,sizeof a_code);memset(b_code,0,sizeof b_code);memset(graph,0,sizeof graph);
    a_code[0]=0x1c;a_code[1]=1;b_code[0]=0x1c;b_code[1]=2;
    function(8,4,a_code,3);function(9,5,b_code,3);*(void**)(classes[2]+0x48)=objects[8];*(void**)(objects[8]+0x28)=objects[9];
    fv_actions_ready=1;
}
static void wrapper(unsigned char* code,int target){
    code[0]=0x46;void* graph_fn=objects[10];memcpy(code+1,&graph_fn,8);code[9]=0x1d;memcpy(code+10,&target,4);
    code[14]=0x16;code[15]=0x04;code[16]=0x0b;code[17]=0x53;
}
static void action_tests(void){
    actions_ready();assert(fv_action_mask(objects[4])==3);
    b_code[1]=1;assert(fv_action_mask(objects[4])==2);
    a_code[0]=0x04;a_code[1]=0x0b;a_code[2]=0x53;assert(fv_action_mask(objects[4])==2);
    b_code[0]=0x04;b_code[1]=0x0b;b_code[2]=0x53;assert(!fv_action_mask(objects[4]));
    actions_ready();function(8,4,a_code,18);function(9,5,b_code,18);function(10,8,graph,64);
    wrapper(a_code,0);wrapper(b_code,8);graph[0]=graph[8]=0x06;int dest=16;memcpy(graph+1,&dest,4);memcpy(graph+9,&dest,4);
    graph[16]=0x1c;assert(fv_action_mask(objects[4])==2);
    graph[16]=0x04;graph[17]=0x0b;graph[18]=0x53;assert(!fv_action_mask(objects[4]));
    graph[16]=0x06;memcpy(graph+17,&dest,4);assert(!fv_action_mask(objects[4]));
    actions_ready();*(int*)(objects[8]+0x68)=-1;assert(fv_action_mask(objects[4])==2);
    actions_ready();*(void**)(objects[8]+0x20)=classes[1];assert(fv_action_mask(objects[4])==2);
    actions_ready();objects[8][0xb4]=1;assert(fv_action_mask(objects[4])==2);
    actions_ready();fv_actions_publish(objects[0],now);assert(touch_menu_toolbar_mask(now)==3);
    assert(!touch_menu_toolbar_mask(now-1) && !touch_menu_toolbar_mask(now+401));
    g_touch_context=2;assert(!touch_menu_toolbar_mask(now));g_touch_context=7;
    fv_actions_publish(0,now);assert(!touch_menu_toolbar_mask(now));
    *(void**)(objects[3]+0x10)=classes[6];g_lobby_menu_ms=g_menu_tick_ms=now;
    fv_actions_publish(0,now);assert(touch_menu_toolbar_mask(now)==1);
    objects[11][0x601]=0;fv_actions_publish(0,now);assert(!touch_menu_toolbar_mask(now));objects[11][0x601]=8;
    objects[11][0x600]=0;fv_actions_publish(0,now);assert(!touch_menu_toolbar_mask(now));objects[11][0x600]=4;
    title_fields[0][0x79]=1;fv_actions_publish(0,now);assert(!touch_menu_toolbar_mask(now));title_fields[0][0x79]=0;
    *(int*)(title_fields[1]+0x4c)=0x1000;fv_actions_publish(0,now);assert(!touch_menu_toolbar_mask(now));*(int*)(title_fields[1]+0x4c)=0x601;
    alive[11]=0;fv_actions_publish(0,now);assert(!touch_menu_toolbar_mask(now));alive[11]=1;
    fv_actions_publish(0,now);assert(touch_menu_toolbar_mask(now)==1);
    assert(!touch_menu_toolbar_mask(now+401));
    fv_actions_publish(0,now+500);assert(!touch_menu_toolbar_mask(now+500));
    g_lobby_menu_ms=now+1;fv_actions_publish(0,now);assert(!touch_menu_toolbar_mask(now));
    g_lobby_menu_ms=now;g_menu_tick_ms=now-300;fv_actions_publish(0,now);assert(!touch_menu_toolbar_mask(now));
    g_menu_tick_ms=now;context=g_touch_context=4;fv_actions_publish(0,now);assert(!touch_menu_toolbar_mask(now));
    context=g_touch_context=7;*(void**)(objects[3]+0x10)=classes[7];fv_actions_publish(0,now);assert(!touch_menu_toolbar_mask(now));
    *(void**)(objects[3]+0x10)=classes[6];on_gt=0;fv_actions_publish(0,now);assert(!touch_menu_toolbar_mask(now));
    on_gt=1;fv_actions_publish(objects[8],now);assert(!touch_menu_toolbar_mask(now));
    actions_ready();cover_mode=1;fv_actions_publish(objects[0],now);assert(touch_menu_toolbar_mask(now)==2);
    *(void**)(objects[7]+0xe0)=objects[4];fv_actions_publish(objects[7],now);assert(touch_menu_toolbar_mask(now)==3);
    puts("PASS: actual authored A/B handler/empty/identical/ubergraph aliases, bounded malformed script/owner/arity/cycle refusals; fresh context-bound toolbar mask and exact Lobby menu-pawn title capability");
}
static unsigned char title_parameter[0x80],title_parameter_kind[8],title_script[36];
static void title_event(void* object,const void* fn,void* params){
 assert(on_gt&&object==objects[11]&&fn==objects[8]);
 unsigned char zero[24]={0};assert(!memcmp(params,zero,24));
 assert(objects[11][0x600]&&!objects[11][0x601]&&!objects[11][0x602]&&!objects[11][0x603]);
 if(fv_gameplay_entered)assert(g_look_yaw==0&&g_look_pitch==0);
 logo_skips++;objects[11][0x603]=title_fields[3][0x7b];
}
static void title_intro_ready(void){
 ready();*(void**)(objects[3]+0x10)=classes[6];g_lobby_menu_ms=g_menu_tick_ms=now;
 g_look_yaw=-.9f;g_look_pitch=.4f;
 *(void**)(objects[11]+0x10)=classes[7];*(void**)(classes[7]+0x48)=objects[8];
 *(void**)(objects[8]+0x10)=classes[3];*(unsigned*)(objects[8]+0x18)=16;
 *(void**)(objects[8]+0x20)=classes[7];objects[8][0xb4]=1;
 *(unsigned short*)(objects[8]+0xb6)=24;*(unsigned short*)(objects[8]+0xb8)=0xffff;
 memset(title_parameter,0,sizeof title_parameter);*(unsigned*)title_parameter_kind=18;
 *(void**)(title_parameter+8)=title_parameter_kind;*(unsigned*)(title_parameter+0x28)=17;
 *(int*)(title_parameter+0x34)=1;*(int*)(title_parameter+0x38)=24;*(uint64_t*)(title_parameter+0x40)=0x80;
 *(void**)(objects[8]+0x50)=title_parameter;
 memset(title_script,0,sizeof title_script);title_script[0]=0x64;title_script[18]=0x46;title_script[27]=0x1d;
 int entry=11555;memcpy(title_script+28,&entry,4);title_script[32]=0x16;title_script[33]=4;title_script[34]=0x0b;title_script[35]=0x53;
 struct{unsigned char* data;int count,max;} script={title_script,36,36};memcpy(objects[8]+0x60,&script,sizeof script);
 memset(title_table,0,sizeof title_table);title_table[0x218/sizeof(void*)]=phone_actor_event_address=(void*)title_event;
 *(void***)objects[11]=title_table;
 objects[11][0x601]=objects[11][0x602]=objects[11][0x603]=0;
}
static void title_intro_tests(void){
 title_intro_ready();fv_actions_ready=1;fv_actions_publish(0,now);assert(touch_menu_toolbar_mask(now)==5);
 assert(!touch_menu_toolbar_mask(now-1)&&!touch_menu_toolbar_mask(now+401));
 for(int mode=0;mode<5;mode++){
  title_intro_ready();fv_actions_ready=1;
  if(mode==0)objects[11][0x600]=0;
  if(mode==1)objects[11][0x602]=title_fields[2][0x7b];
  if(mode==2)objects[11][0x603]=title_fields[3][0x7b];
  if(mode==3)*(int*)(title_fields[2]+0x4c)=0x1000;
  if(mode==4)title_fields[3][0x7b]=0;
  fv_actions_publish(0,now);assert(!touch_menu_toolbar_mask(now));
 }
 title_intro_ready();fv_actions_ready=1;objects[11][0x601]=title_fields[1][0x7b];
 fv_actions_publish(0,now);assert(touch_menu_toolbar_mask(now)==1);
 title_intro_ready();fv_title_intro_tick(now,7);assert(!logo_skips&&!fv_gameplay_entered&&g_look_yaw==-.9f&&g_look_pitch==.4f);
 intro_skip=1;fv_title_intro_tick(now,7);assert(logo_skips==1&&g_look_yaw==-.9f&&g_look_pitch==.4f);fv_title_intro_tick(now,7);assert(logo_skips==1);
 title_intro_ready();fv_title_intro_tick(now,1);assert(fv_gameplay_entered&&!logo_skips&&g_look_yaw==-.9f&&g_look_pitch==.4f);
 fv_title_intro_tick(now,7);assert(logo_skips==1&&g_look_yaw==0&&g_look_pitch==0);
 // Later lobby look and the original Start stage must remain under user control.
 g_look_yaw=.34f;g_look_pitch=-.21f;fv_title_intro_tick(now,7);assert(logo_skips==1&&g_look_yaw==.34f&&g_look_pitch==-.21f);
 objects[11][0x601]=title_fields[1][0x7b];objects[11][0x603]=0;
 fv_title_intro_tick(now,7);assert(logo_skips==1&&g_look_yaw==.34f&&g_look_pitch==-.21f);
 // A fresh FrontendManager on a later return performs its own single reset.
 serials[11]++;objects[11][0x601]=objects[11][0x602]=objects[11][0x603]=0;
 fv_title_intro_tick(now,7);assert(logo_skips==2&&g_look_yaw==0&&g_look_pitch==0);
 for(int mode=0;mode<12;mode++){
  title_intro_ready();intro_skip=1;fv_title_intro_tick(now,1);
  if(mode==0)objects[11][0x600]=0;if(mode==1)objects[11][0x601]=title_fields[1][0x7b];if(mode==2)objects[11][0x602]=title_fields[2][0x7b];if(mode==3)objects[11][0x603]=title_fields[3][0x7b];
  if(mode==4)on_gt=0;if(mode==5)objects[8][0xb4]=2;if(mode==6)*(unsigned short*)(objects[8]+0xb6)=16;
  if(mode==7)*(int*)(title_parameter+0x4c)=8;if(mode==8)title_script[28]^=1;
  if(mode==9)*(int*)(title_fields[2]+0x4c)=0x1000;if(mode==10)g_menu_tick_ms=now-300;
  if(mode==11)title_table[0x218/sizeof(void*)]=0;
  fv_title_intro_tick(now,7);assert(!logo_skips&&g_look_yaw==-.9f&&g_look_pitch==.4f);
 }
 puts("PASS: first Lobby intro respects option/look; returning gameplay resets yaw/pitch before exact original skip once per FrontendManager; later look/Start and all malformed/stale stage/event/FKey/thread/world guards preserve look");
}
int main(void){start_tests();action_tests();title_intro_tests();return 0;}
