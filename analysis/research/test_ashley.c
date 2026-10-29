#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static volatile int g_touch_gt_tid,g_touch_context,g_touch_cancel,g_touch_pause,g_touch_inventory_request;
static volatile int g_touch_reload_request,g_reload_phase,g_touch_weapon_request,g_touch_interact_request;
static int qs_ready,fixture_tid=41,qs_busy,motion_busy;
#define gettid() fixture_tid
#define LOGF(...) ((void)0)
static void* (*tw_hand_class)(void),*(*phone_player)(const void*),*(*phone_active)(void),*(*rl_right)(const void*);
static unsigned (*phone_type)(void);
static int (*rl_paused)(void),(*rl_player_valid)(void),(*qs_allowed)(const void*);
/* SHARED_QS_GT */
static int touch_quickselect_busy(void){return qs_busy;}
static int touch_motion_pose_busy(void){return motion_busy;}
/* SHARED_QS_NAME */
/* SHARED_TYPED */
#include "ASHLEY_HEADER"
#ifdef ASHLEY_NATIVE_FIXTURE
#include ASHLEY_NATIVE_FIXTURE
#endif
static struct {
 unsigned char hand[2][0x570],pooled[0x570],pawn[0x1650],other[0x1650],cls[2][0x100];
 void* chain[2][1];unsigned char sub[2],player[2];
} f __attribute__((aligned(16)));
static void* current_pawn,*bio4_pawn,*native_sub,*native_player;
static unsigned native_type,native_active,native_next;
static int native_paused,native_valid,native_regular,native_control,native_event,native_qte;
static int native_allowed,native_exists,native_health,native_can,toggle_calls,can_calls,late_can_call,late_change;
static int toggle_no_change,pooled_same_pawn;
static void* hand_class(void){return f.cls[0];}
static void* pawn_class(void){return f.cls[1];}
static void* owner(const void* p){return p==f.pooled ? pooled_same_pawn ? f.pawn : f.other : p==f.hand[0] || p==f.hand[1] ? f.pawn : 0;}
static void* right_hand(const void* p){assert(p==f.pawn || p==f.other);return p==f.pawn ? f.hand[1] : f.pooled;}
static void* active_pawn(void){return current_pawn;}
static void* bio4(void){return bio4_pawn;}
static unsigned type(void){return native_type;}
static int paused(void){return native_paused;}
static int valid(void){return native_valid;}
static int allowed(const void* p){assert(p==f.pawn || p==f.other);return native_allowed;}
static int regular(const void* p){assert(p==current_pawn);return native_regular;}
static int exists(const void* p){assert(p==current_pawn);return native_exists;}
static int health(const void* p){assert(p==current_pawn);return native_health;}
static int control(const void* p){assert(p==native_player);return native_control;}
static int max_health(void){return 1200;}
static int event(void){return native_event;}
static int qte(void){return native_qte;}
static unsigned active_command(const void* p){assert(p==current_pawn);return native_active;}
static unsigned next_command(const void* p){assert(p==current_pawn);return native_next;}
static int can(const void* p){
 assert(p==current_pawn);can_calls++;
 if(late_can_call && can_calls==late_can_call){
  if(late_change==1)g_touch_cancel=1;if(late_change==2)g_touch_context=2;
  if(late_change==3)(*(uint64_t*)(f.pawn+0x18))++;
  if(late_change==4)native_sub=&f.sub[1];if(late_change==5)native_player=&f.player[1];
  if(late_change==6)current_pawn=f.other;
  if(late_change==7)native_active^=1;if(late_change==8)native_next^=1;
  if(late_change==9)g_touch_reload_request=1;
 }
 return native_can;
}
static void toggle(void* p){
 assert(p==f.pawn && p==current_pawn && p==bio4_pawn);
 assert(native_sub==&f.sub[0] && native_player==&f.player[0]);
 assert(ash_input_context() && native_can && native_exists && native_health>0);
 assert(native_regular && native_control && !native_paused && native_valid && !native_event && !native_qte && native_allowed);
 toggle_calls++;if(!toggle_no_change){native_active=native_next;native_next^=1;}
}
static void ready(void){
 memset(&f,0,sizeof f);ash_ui=ash_request=0;ash_generation=ash_logs=0;
 ash_pawn=ash_sub=ash_player=0;ash_name=0;ash_ready=qs_ready=1;
 for(int i=0;i<2;i++){
  *(int*)(f.cls[i]+0x38)=0;*(int*)(f.cls[i]+0x58)=i ? sizeof f.pawn : sizeof f.hand[0];
  f.chain[i][0]=f.cls[i]+0x30;*(void***)(f.cls[i]+0x30)=f.chain[i];
  *(void**)(f.hand[i]+0x10)=f.cls[0];f.hand[i][0x314]=i;*(uint64_t*)(f.hand[i]+0x18)=10+i;
 }
 *(void**)(f.pooled+0x10)=f.cls[0];f.pooled[0x314]=1;
 *(void**)(f.pawn+0x10)=*(void**)(f.other+0x10)=f.cls[1];
 *(uint64_t*)(f.pawn+0x18)=100;*(uint64_t*)(f.other+0x18)=101;
 current_pawn=bio4_pawn=f.pawn;native_sub=&f.sub[0];native_player=&f.player[0];
 native_type=1;native_active=0;native_next=1;native_health=600;
 native_valid=native_regular=native_control=native_allowed=native_exists=native_can=1;
 native_paused=native_event=native_qte=qs_busy=motion_busy=0;
 toggle_calls=can_calls=late_can_call=late_change=toggle_no_change=pooled_same_pawn=0;
 g_touch_gt_tid=fixture_tid=41;g_touch_context=1;
 g_touch_cancel=g_touch_pause=g_touch_inventory_request=g_touch_reload_request=g_reload_phase=g_touch_weapon_request=g_touch_interact_request=0;
 tw_hand_class=hand_class;phone_player=owner;phone_active=active_pawn;phone_type=type;rl_right=right_hand;
 rl_paused=paused;rl_player_valid=valid;qs_allowed=allowed;
 ash_class=pawn_class;ash_bio4=bio4;ash_exists=exists;ash_health=health;ash_can=can;
 ash_active=active_command;ash_next=next_command;ash_regular=regular;ash_control=control;ash_event=event;ash_qte=qte;ash_toggle=toggle;ash_max=max_health;ash_hp=0;
 ash_sub_global=&native_sub;ash_player_global=&native_player;
}
static void publish(long now){touch_ashley_tick(f.hand[1],now);}
static void request_at(long now){assert(touch_ashley_request(now));}
int main(void){
#ifdef ASHLEY_NATIVE_FIXTURE
 verify_ashley_install_guard();
#endif
 ready();unsigned char before[sizeof f];memcpy(before,&f,sizeof f);publish(1000);
 struct touch_ashley_state s=touch_ashley_state(1000);
 assert(!memcmp(before,&f,sizeof f) && s.available && s.active==0 && s.next==1 && !toggle_calls);
 request_at(1001);assert(!touch_ashley_request(1001));publish(1002);
 assert(toggle_calls==1 && !ash_request);s=touch_ashley_state(1002);assert(s.active==1 && s.next==0);
 publish(1003);assert(toggle_calls==1);request_at(1004);publish(1005);assert(toggle_calls==2 && native_active==0);
 ready();publish(1000);request_at(1001);uint64_t request=ash_request,ui=ash_ui;
 touch_ashley_tick(f.pooled,1002);touch_ashley_tick(f.hand[0],1002);
 pooled_same_pawn=1;touch_ashley_tick(f.pooled,1002);
 assert(ash_request==request && ash_ui==ui && !toggle_calls);publish(1003);assert(toggle_calls==1);
 ready();publish(1000);request_at(1001);fixture_tid=42;publish(1002);assert(ash_request && !toggle_calls);
 fixture_tid=41;publish(1003);assert(toggle_calls==1);
 for(int why=0;why<25;why++){
  ready();publish(1000);request_at(1001);
  if(why==0)native_paused=1;if(why==1)native_valid=0;if(why==2)native_regular=0;if(why==3)native_control=0;
  if(why==4)native_event=1;if(why==5)native_qte=1;if(why==6)native_allowed=0;if(why==7)native_exists=0;if(why==8)native_health=0;
  if(why==9)native_can=0;if(why==10)native_sub=0;if(why==11)native_player=0;
  if(why==12)g_touch_context=2;if(why==13)g_touch_cancel=3;if(why==14)g_touch_pause=1;if(why==15)g_touch_inventory_request=1;
  if(why==16)g_touch_reload_request=1;if(why==17)g_reload_phase=1;if(why==18)g_touch_weapon_request=1;if(why==19)g_touch_interact_request=1;
  if(why==20)qs_busy=1;if(why==21)motion_busy=1;if(why==22)native_type=3;if(why==23)native_active=2;if(why==24)native_next=2;
  publish(1002);assert(!toggle_calls && !ash_request && !touch_ashley_state(1002).available);
 }
 for(int why=0;why<6;why++){
  ready();publish(1000);request_at(1001);
  if(why==0)(*(uint64_t*)(f.pawn+0x18))++;if(why==1)native_sub=&f.sub[1];if(why==2)native_player=&f.player[1];
  if(why==3)bio4_pawn=f.other;if(why==4)native_active=1;if(why==5)native_next=0;
  publish(1002);assert(!toggle_calls && !ash_request);
 }
 ready();publish(1000);assert(!touch_ashley_request(999) && !touch_ashley_request(1501));
 request_at(1001);publish(1502);assert(!toggle_calls && !ash_request);
 ready();publish(1000);request_at(1001);publish(999);assert(!toggle_calls && !ash_request);
#if LONG_MAX > 0x7fffffffL   /* the 32-bit ms wrap needs a 64-bit long (Android LP64; Windows long is 32-bit) */
 ready();publish(0xfffffff0L);request_at(0xfffffff5L);publish(0x100000005L);assert(toggle_calls==1);
#endif
 ready();publish(1000);request_at(1001);toggle_no_change=1;publish(1002);publish(1003);assert(toggle_calls==1);
 ready();publish(1000);request_at(1001);touch_ashley_cancel();publish(1002);assert(!toggle_calls);
 // A last eligibility getter can coincide with an input reset or identity change.
 for(int why=1;why<=9;why++){
  ready();publish(1000);request_at(1001);can_calls=0;late_can_call=2;late_change=why;
  publish(1002);assert(!toggle_calls && !ash_request);
 }
 puts("PASS: actual Ashley snapshot/request/callback once, Follow/Stay, 25 authority gates, active typed1650 owner, pooled/left/thread/identity changes, fresh/future/wrap/no retry and late getter cancellation");
 return 0;
}
