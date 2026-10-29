#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static volatile int g_touch_inventory_request;
static unsigned active_type=1;
static const void *owner,*active;
static int paused,over,qte,pending,calls,accepted=1;
static unsigned short received_info;
static int aim_phase,aim_prepares,aim_results,aim_accepted;
static void mock_aim_prepare(void* h){assert(h&&aim_phase==0);aim_phase=1;aim_prepares++;}
static void mock_aim_result(int value){assert(aim_phase==2);aim_phase=0;aim_results++;aim_accepted=value;}
static void* player(const void* hand) { (void)hand; return (void*)owner; }
static void* current(void) { return (void*)active; }
static unsigned type(void) { return active_type; }
static int is_paused(void) { return paused; }
static int game_over(void) { return over; }
static int has_qte(void) { return qte; }
static int pending_load(void) { return pending; }
static int try_pause(unsigned short info) { if(aim_phase){assert(aim_phase==1);aim_phase=2;}calls++; received_info=info; return accepted; }
static void* (*phone_player)(const void*)=player;
static void* (*phone_active)(void)=current;
static unsigned (*phone_type)(void)=type;
static int (*rl_paused)(void)=is_paused;
typedef struct{float x,y,z,w;}qf_t;
struct __attribute__((aligned(16))) reload_tf{qf_t q;float p[3],s[3],padding[2];};
static int reload_marker_valid(const struct reload_tf* t){float n=t->q.x*t->q.x+t->q.y*t->q.y+t->q.z*t->q.z+t->q.w*t->q.w;if(!isfinite(n)||fabsf(n-1)>.01f)return 0;for(unsigned k=0;k<3;k++)if(!isfinite(t->p[k])||!isfinite(t->s[k])||t->s[k]<=0)return 0;return 1;}
static unsigned char drag_objects[7][0x1000],drag_classes[5][0x100];
static int g_ptr_down,drag_alive[7],on_game_thread=1;
static int cinema_on_game_thread(void){return on_game_thread;}
static int pv_alive(const void* object){for(unsigned i=0;i<7;i++)if(object==drag_objects[i])return drag_alive[i];return 0;}
static int menu_visual_layout(const void* object,const void* cls,int minimum){return object&&*(const void* const*)((const char*)object+0x10)==cls&&*(const int*)((const char*)cls+0x58)>=minimum;}
static void* drag_hand_class(void){return drag_classes[2];}static void* drag_scene_class(void){return drag_classes[4];}
static void*(*mv_hand_class)(void)=drag_hand_class,*(*mv_scene_class)(void)=drag_scene_class;
#define LOGF(...) ((void)0)
static void drag_target(void*,const struct reload_tf*,void*,float);
#define RE4FIX_INVENTORY_TARGET_GATE drag_target
#include "../re4fix_inventory.h"

struct mock_class { unsigned char padding[0x58]; int size; };
struct mock_pawn {
    unsigned char padding[0x10];
    const struct mock_class* cls;
    unsigned char between[0xc10-0x18];
    float cooldown;
};
struct short_pawn { unsigned char padding[0x10]; const struct mock_class* cls; };
_Static_assert(offsetof(struct mock_pawn,cls)==0x10 && offsetof(struct mock_pawn,cooldown)==0xc10,"native UObject/cooldown offsets");
_Static_assert(offsetof(struct mock_class,size)==0x58,"native class PropertiesSize");
static const unsigned char right[0x318]={[0x314]=1},left[0x318]={0};
static const struct mock_class good_class={.size=0xc14},huge_class={.size=0x10000};
static const struct mock_class short_class={.size=0xc13},invalid_class={.size=0x10001};
static const struct mock_pawn good={.cls=&good_class},other={.cls=&good_class};
static const struct mock_pawn upper_bound={.cls=&huge_class};
static const struct mock_pawn invalid_size={.cls=&invalid_class},no_class={0};
static const struct mock_pawn cooling={.cls=&good_class,.cooldown=0.01f};
static const struct mock_pawn negative={.cls=&good_class,.cooldown=-1.0f};
static const struct mock_pawn nan_cooldown={.cls=&good_class,.cooldown=NAN};
static const struct mock_pawn inf_cooldown={.cls=&good_class,.cooldown=INFINITY};
static const struct short_pawn too_short={.cls=&short_class};

static void request_on(const void* pawn) {
    owner=active=pawn; g_touch_inventory_request=1;
    touch_inventory_tick((void*)right);
    assert(!g_touch_inventory_request);
}
static void* drag_screen_class(void){return drag_classes[0];}static void* drag_item_class(void){return drag_classes[1];}static void* drag_case_class(void){return drag_classes[3];}
static struct inventory_drag_ray drag_ray_value;
static struct inventory_drag_plane drag_plane_value;
static struct reload_tf drag_received;
static int drag_calls;
static void* drag_received_item,*drag_received_parent;
static const struct reload_tf* drag_received_argument;
static float drag_received_scale;
static void* drag_manager_value,*drag_screen_value;
static void* drag_manager(void){return drag_manager_value;}
static void* drag_screen(void* manager){assert(manager==drag_objects[5]);return drag_screen_value;}
static struct inventory_drag_ray drag_ray(const void* hand){assert(hand==drag_objects[2]);return drag_ray_value;}
static struct inventory_drag_plane drag_plane(const void* attache){assert(attache==drag_objects[3]);return drag_plane_value;}
static void drag_target(void* item,const struct reload_tf* tf,void* parent,float scale){
 drag_calls++;drag_received_item=item;drag_received_argument=tf;drag_received_parent=parent;drag_received_scale=scale;
 if(tf){drag_received=*tf;if(item==drag_objects[1])memcpy(drag_objects[1]+0x2a0,tf,sizeof *tf);}
}
static void drag_native_tick(void){memcpy(drag_objects[6]+0x1c0,drag_objects[1]+0x2a0,sizeof(struct reload_tf));}
static void drag_produce(const struct reload_tf* tf,void* parent,float scale,const void* caller){inventory_drag_dispatch(drag_objects[1],tf,parent,scale,caller);}
static void drag_ready(void){
 memset(drag_objects,0,sizeof drag_objects);memset(drag_classes,0,sizeof drag_classes);
 for(unsigned i=0;i<7;i++){drag_alive[i]=1;*(void**)(drag_objects[i]+0x10)=drag_classes[i<5?i:0];}
 for(unsigned i=0;i<5;i++)*(int*)(drag_classes[i]+0x58)=0x1000;
 *(void**)(drag_objects[0]+0x2c0)=drag_objects[1];*(void**)(drag_objects[0]+0x330)=drag_objects[2];
 *(void**)(drag_objects[0]+0x2f0)=drag_objects[3];drag_objects[0][0x270]=1;
 *(void**)(drag_objects[3]+0x130)=drag_objects[4];*(void**)(drag_objects[4]+0x20)=*(void**)(drag_objects[4]+0xa0)=drag_objects[3];
 *(void**)(drag_objects[1]+0x130)=drag_objects[6];
 drag_objects[2][0x314]=1;drag_objects[2][0x33d]=1;
 struct reload_tf tf={{0,0,0,1},{10,20,85},{1,1,1},{0,0}};memcpy(drag_objects[1]+0x2a0,&tf,sizeof tf);*(float*)(drag_objects[1]+0x2d0)=1;
 drag_ray_value=(struct inventory_drag_ray){{0,0,100},{0,0,-1}};drag_plane_value=(struct inventory_drag_plane){{0,0,1},50};
 owner=active=&good;active_type=2;g_ptr_down=1;drag_calls=0;on_game_thread=1;
 drag_manager_value=drag_objects[5];drag_screen_value=drag_objects[0];fv_ui_manager=drag_manager;fv_peek_screen=drag_screen;
 inventory_drag_caller=(void*)0x62e5d5c;
 inventory_drag_screen_class=drag_screen_class;inventory_drag_item_class=drag_item_class;inventory_drag_case_class=drag_case_class;
 inventory_drag_ray=drag_ray;inventory_drag_plane=drag_plane;
}
static void drag_tests(void){
 drag_ready();struct reload_tf input;memcpy(&input,drag_objects[1]+0x2a0,sizeof input);struct reload_tf before=input;
 drag_produce(&input,0,1,inventory_drag_caller);assert(drag_calls==1&&drag_received.p[0]==10&&drag_received.p[1]==20&&drag_received.p[2]==54);
 assert(!memcmp(&input,&before,sizeof input));assert(!drag_received_parent&&drag_received_scale==1);
 struct reload_tf first=drag_received;for(unsigned k=0;k<20;k++)assert(inventory_drag_pose(&drag_received,drag_ray_value,drag_plane_value));assert(!memcmp(&first,&drag_received,sizeof first));
 for(int mode=0;mode<25;mode++){
  drag_ready();
  if(mode==0)active_type=1;if(mode==1)g_ptr_down=0;if(mode==2)drag_objects[0][0x270]=0;
  if(mode==3)drag_alive[1]=0;if(mode==4)drag_objects[2][0x314]=0;if(mode==5)drag_objects[2][0x33d]=0;
  if(mode==7)*(void**)(drag_objects[4]+0xa0)=drag_objects[5];
  if(mode==8)drag_plane_value.normal[0]=NAN;if(mode==9)drag_plane_value.normal[2]=0;
  if(mode==10)drag_ray_value.direction[2]=0;if(mode==11)drag_plane_value.distance=1000;
  if(mode==13)input.q.x=NAN;
  if(mode==14)*(void**)(drag_objects[3]+0x10)=drag_classes[0];
  if(mode==15)drag_screen_value=0;if(mode==16)drag_manager_value=0;if(mode==17)owner=&other;
  if(mode==18)active=0;if(mode==19)on_game_thread=0;if(mode==20)*(void**)(drag_objects[0]+0x2c0)=drag_objects[5];
  if(mode==21)inventory_drag_caller=0;if(mode==22)fv_ui_manager=0;if(mode==23)fv_peek_screen=0;
  if(mode==24)drag_alive[0]=0;
  const void* caller=mode==21?(void*)0x62e5d5c:inventory_drag_caller;
  void* parent=mode==6?drag_objects[5]:0;float scale=mode==12?NAN:1;
  drag_produce(&input,parent,scale,caller);assert(drag_calls==1&&drag_received_argument==&input);
  assert(drag_received_item==drag_objects[1]&&drag_received_parent==parent);
  assert(mode==12?isnan(drag_received_scale):drag_received_scale==scale);input=before;
 }
 drag_ready();drag_plane_value=(struct inventory_drag_plane){{1,0,0},40};drag_ray_value.direction[0]=1;drag_ray_value.direction[2]=0;
 drag_produce(&input,0,1,inventory_drag_caller);assert(drag_calls==1&&drag_received.p[0]==44&&drag_received.p[1]==20&&drag_received.p[2]==85);
 drag_ready();drag_produce(&input,0,1,(void*)0x62e4fa4);assert(drag_calls==1&&drag_received_argument==&input); // GunVice must remain native.
 drag_ready();drag_produce(0,drag_objects[5],.75f,inventory_drag_caller);assert(drag_calls==1&&!drag_received_argument&&drag_received_parent==drag_objects[5]&&drag_received_scale==.75f);
 drag_ready();drag_produce(0,0,1,inventory_drag_caller);assert(drag_calls==1&&!drag_received_argument&&!drag_received_parent&&drag_received_scale==1);
 // Hardware p367: a post-hand target191 was replaced by the native screen producer, leaving actual root-12.77.
 drag_ready();input=(struct reload_tf){{0,0,.99999988f,-.000000119f},{-12.772f,-13.23f,15182.7f},{1,1,1},{0,0}};
 drag_ray_value=(struct inventory_drag_ray){{-26.76f,7.93f,15170.14f},{.977f,-.212f,.037f}};
 drag_plane_value=(struct inventory_drag_plane){{-1,0,0},-195};
 struct reload_tf lifted=input;assert(inventory_drag_pose(&lifted,drag_ray_value,drag_plane_value));
 drag_target(drag_objects[1],&lifted,0,1);drag_target(drag_objects[1],&input,0,1);drag_native_tick();
 struct reload_tf actual;memcpy(&actual,drag_objects[6]+0x1c0,sizeof actual);assert(actual.p[0]==input.p[0]);
 drag_calls=0;drag_produce(&input,0,1,inventory_drag_caller);drag_native_tick();memcpy(&actual,drag_objects[6]+0x1c0,sizeof actual);
 assert(drag_calls==1&&fabsf(actual.p[0]-191)<.001f&&!memcmp(&actual,&lifted,sizeof actual));
 assert(!memcmp(&actual.q,&input.q,sizeof input.q)&&!memcmp(actual.s,input.s,sizeof input.s));
 puts("PASS: exact native producer replaces wrong-phase target; actual mock Tick root191 vs original-12.77; one original call on lift and 28 forwarding routes; original input/rotation/scale preserved");
}
int main(void) {
    owner=active=&good;
    install_touch_inventory(0);
    g_touch_inventory_request=1;touch_inventory_tick((void*)right);
    assert(g_touch_inventory_request==1 && !calls);
    inventory_game_over=game_over;inventory_qte=has_qte;inventory_pending_load=pending_load;
    inventory_try_pause=try_pause;
    rl_paused=0;touch_inventory_tick((void*)right);assert(g_touch_inventory_request==1 && !calls);rl_paused=is_paused;
    touch_inventory_tick(0);touch_inventory_tick((void*)left);
    assert(g_touch_inventory_request==1 && !calls);
    owner=&other;touch_inventory_tick((void*)right);assert(g_touch_inventory_request==1 && !calls);
    owner=0;touch_inventory_tick((void*)right);assert(g_touch_inventory_request==1 && !calls);owner=&good;
    active=0;touch_inventory_tick((void*)right);assert(g_touch_inventory_request==1 && !calls);active=&good;
    for(unsigned i=0;i<10;i++) if(i!=1) {
        active_type=i;g_touch_inventory_request=1;touch_inventory_tick((void*)right);
        assert(!g_touch_inventory_request && !calls);
    }
    active_type=1;
    request_on(&too_short);request_on(&invalid_size);request_on(&no_class);assert(!calls);
    request_on(&cooling);request_on(&nan_cooldown);request_on(&inf_cooldown);assert(!calls);
    owner=active=&good;
    paused=1;request_on(&good);paused=0;assert(!calls);
    over=1;request_on(&good);over=0;assert(!calls);
    qte=1;request_on(&good);qte=0;assert(!calls);
    pending=1;request_on(&good);pending=0;assert(!calls);
    unsigned char pawn_before[sizeof good],hand_before[sizeof right],class_before[sizeof good_class];
    memcpy(pawn_before,&good,sizeof good);memcpy(hand_before,right,sizeof right);memcpy(class_before,&good_class,sizeof good_class);
    accepted=0;request_on(&good);assert(calls==1 && received_info==0xff02);
    touch_inventory_tick((void*)right);assert(calls==1);
    accepted=1;request_on(&good);assert(calls==2 && received_info==0xff02);
    touch_inventory_tick((void*)right);assert(calls==2);
    assert(!memcmp(pawn_before,&good,sizeof good) && !memcmp(hand_before,right,sizeof right)
        && !memcmp(class_before,&good_class,sizeof good_class));
    request_on(&upper_bound);request_on(&negative);assert(calls==4 && received_info==0xff02);
    puts("PASS: inventory right/active/type/size/native gates, pending/one-shot requests, exact ff02 and immutable objects");
    inventory_aim_prepare=mock_aim_prepare;inventory_aim_result=mock_aim_result;
    accepted=1;request_on(&good);assert(aim_prepares==1&&aim_results==1&&aim_accepted==1&&!aim_phase);
    accepted=0;request_on(&good);assert(aim_prepares==2&&aim_results==2&&!aim_accepted&&!aim_phase);
    puts("PASS: hip preparation precedes original TryPause; native accept/refuse result reaches calibration hold only after original returns");
    drag_tests();
    return 0;
}
