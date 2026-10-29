#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
__declspec(dllimport) void* __stdcall VirtualAlloc(void*,size_t,unsigned long,unsigned long);
__declspec(dllimport) int __stdcall VirtualFree(void*,size_t,unsigned long);
#endif

typedef int test_mutex_t;
#define pthread_mutex_t test_mutex_t
static void pthread_mutex_lock(pthread_mutex_t* m) {(void)m;}
static void pthread_mutex_unlock(pthread_mutex_t* m) {(void)m;}
#define LOGF(...) ((void)0)
typedef struct {float x,y,z,w;} qf_t;
static qf_t q_mul(qf_t a,qf_t b) {
 return (qf_t){a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
              a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
static void q_rot(qf_t q,const float v[3],float o[3]) {
 float d=q.x*v[0]+q.y*v[1]+q.z*v[2],k=q.w*q.w-q.x*q.x-q.y*q.y-q.z*q.z;
 o[0]=2*d*q.x+k*v[0]+2*q.w*(q.y*v[2]-q.z*v[1]);
 o[1]=2*d*q.y+k*v[1]+2*q.w*(q.z*v[0]-q.x*v[2]);
 o[2]=2*d*q.z+k*v[2]+2*q.w*(q.x*v[1]-q.y*v[0]);
}
static pthread_mutex_t g_gripmx;
static struct {void* g;} g_grips[256];
static int g_ngrips;
static volatile long g_cmd_until[32];
static volatile int g_touch_reload_request,g_reload_phase,g_touch_gt_tid;
static volatile int g_touch_cancel,g_touch_weapon_request,g_touch_weapon,g_touch_context;
static volatile int g_touch_inventory_request,g_touch_pause,g_touch_interact_request;
static int touch_quickselect_busy(void){return 0;}
static void* g_mobile_holster,*g_mobile_holster_hand;
static volatile long g_mobile_holster_until;
static void* (*f_GetCurrentGrip)(const void*);
static void (*f_SetGripForcedHover)(void*,void*,int);
static int test_tid=17,test_user_fire,cancel_on_user_read;
static long test_now;
static long clock_delay;
static int gettid(void) {return test_tid;}
static long ptr_now_ms(void) {return test_now+clock_delay;}
static int touch_reload_user_fire(long now) {
 assert(now==test_now+clock_delay);
 if(cancel_on_user_read) {g_touch_cancel=3;cancel_on_user_read=0;}
 return test_user_fire;
}
static void* touch_equipped_gun(void* hand);
static int reload_family_info(void* gun,int* offset){(void)gun;(void)offset;return 0;}
static int reload_family_shells_pending(void* gun,int* loaded){(void)gun;*loaded=0;return 0;}
static int reload_family_post_begin(void* hand,void* gun,long now){(void)hand;(void)gun;(void)now;return 0;}
static int reload_family_post_tick(void* hand,int side,long now){(void)hand;(void)side;(void)now;return 0;}
static void reload_family_post_cancel(void){}
#include "../re4fix_reload.h"

static unsigned char pawn[0x1200],gun[0x1000],ammo[0x700],left_hand[0x600],right_hand[0x600];
static unsigned char gun_grip[0x500],slide_grip[0x500],ammo_grip[0x500],holster_grip[0x500];
static unsigned char holster[0x400],root[0x300],other_hand[0x600],other_grip[0x500];
static unsigned char hand_cls[0x70],pawn_cls[0x70],pistol_cls[0x70],ammo_cls[0x70];
static unsigned char grip_cls[0x70],holster_cls[0x70];
static const void* chains[6][1];
static void* pistol_vtable[0x900/sizeof(void*)];
static void* ammo_vtable[0x678/sizeof(void*)];
static void* props[1];
static void* current_right,*current_left,*active_pawn,*slide_owner,*ammo_owner;
static int reloading,paused,player_valid,type,base_ready,held_ammo_ready,native_trigger;
static int grab_blending,flourishing;
static int cancel_on_trigger,foreign_on_trigger,rename_on_trigger;
static int select_on_trigger,phase_cancel_on_trigger;
static int cock_on_trigger,expire_on_trigger;
static int begin_calls,world_calls,hover_calls,native_downs,native_ups,native_shots,native_cocks;
static int native_ammo,native_reserves;
static int prior_native_down;
static int dominant_side=1;
static void putp(void* object,int offset,const void* value) {memcpy((char*)object+offset,&value,sizeof value);}
static void putn(void* object,uint64_t value) {memcpy((char*)object+0x18,&value,sizeof value);}
static void klass(unsigned char* cls,const void** chain,int size) {
 memset(cls,0,0x70);chain[0]=cls+0x30;putp(cls,0x30,chain);memcpy(cls+0x58,&size,sizeof size);
}
static void object(unsigned char* obj,size_t size,unsigned char* cls,uint64_t name) {
 memset(obj,0,size);putp(obj,0x10,cls);putn(obj,name);
}
static void* player(const void* hand) {return hand==other_hand ? other_hand : pawn;}
static void* active(void) {return active_pawn;}
static void* gun1(const void* p) {assert(p==pawn);return gun;}
static void* touch_equipped_gun(void* hand){return dominant_side ? hand==right_hand && current_right==gun_grip ? gun : 0
    : hand==left_hand && current_left==gun_grip ? gun : 0;}
static void* left(const void* p) {assert(p==pawn);return left_hand;}
static void* right(const void* p) {assert(p==pawn);return right_hand;}
static void* primary(const void* prop) {return prop==gun ? dominant_side ? right_hand : left_hand : prop==ammo ? ammo_owner : 0;}
static void* pistol_class(void) {return pistol_cls;}
static void* ammo_class(void) {return ammo_cls;}
static void* holster_class(void) {return holster_cls;}
static void* hand_class(void) {return hand_cls;}
static void* pawn_class(void) {return pawn_cls;}
static void* grip_class(void) {return grip_cls;}
static void* get_pawn(const void* prop) {return *(void* const*)((const char*)prop+0x3c0);}
static void* get_holster(const void* prop) {assert(prop==ammo);return holster;}
static int is_paused(void) {return paused;}
static int valid(void) {return player_valid;}
static int interactivity(const void* prop) {assert(prop==gun);return 2;}
static int available(const void* prop) {assert(prop==gun);return 1;}
static int can_grab_ammo(const void* prop) {assert(prop==ammo);return 1;}
static int progress(void* prop) {assert(prop==gun);return reloading;}
static int load_ready(const void* prop,float dt) {assert(prop==gun && dt==0);return 1;}
static int overlap(const void* prop,float tolerance) {(void)tolerance;assert(prop==gun);return 1;}
static int pawn_type(void) {return type;}
static int ready_base(const void* prop) {assert(prop==gun);return base_ready;}
static int ready_pistol(const void* prop) {assert(prop==gun);return gun[0xdd5] && !slide_owner && base_ready;}
static void* slide_hand(const void* prop) {assert(prop==gun);return slide_owner;}
static int held_ready(const void* prop,float dt) {assert(prop==gun && dt==0);return held_ammo_ready;}
static int blending(const void* prop) {assert(prop==gun);return grab_blending;}
static int flourish(const void* prop) {assert(prop==gun);return flourishing;}
static int trigger(const void* prop) {
 assert(prop==gun);
 if(cancel_on_trigger && !--cancel_on_trigger) g_touch_cancel=3;
 if(foreign_on_trigger && !--foreign_on_trigger) current_right=other_grip;
 if(rename_on_trigger && !--rename_on_trigger) putn(gun,0x9988776600000003ULL);
 if(select_on_trigger && !--select_on_trigger) {g_touch_weapon=1;g_touch_weapon_request=1;g_reload_phase=0;}
 if(phase_cancel_on_trigger && !--phase_cancel_on_trigger) g_reload_phase=0;
 if(cock_on_trigger && !--cock_on_trigger) gun[0xdd5]=1;
 if(expire_on_trigger && !--expire_on_trigger) clock_delay=5000;
 return native_trigger;
}
static void* current_grip(const void* hand) {
 return hand==right_hand ? current_right : hand==left_hand ? current_left : 0;
}
static void force_hover(void* hand,void* grip,int on) {
 assert(hand==(dominant_side ? left_hand : right_hand) && grip==holster_grip);(void)on;hover_calls++;
}
static void begin(void* prop) {assert(prop==gun);begin_calls++;reloading=1;}
static struct reload_tf cached(const void* scene) {
 (void)scene;return (struct reload_tf){{0,0,0,1},{0,0,0},{1,1,1},{0,0}};
}
static void world(void* scene,const struct reload_tf* t,int sweep,void* hit,int teleport) {
 assert(scene==root && !sweep && !hit && !teleport);memcpy(root+0x1c0,t,48);world_calls++;
}
static void tick(void* hand,int side,long now) {test_now=now;touch_reload_tick(hand,side,now);}
static void fixture(void) {
 klass(hand_cls,chains[0],sizeof left_hand);klass(pawn_cls,chains[1],sizeof pawn);
 klass(pistol_cls,chains[2],sizeof gun);klass(ammo_cls,chains[3],sizeof ammo);
 klass(grip_cls,chains[4],sizeof gun_grip);klass(holster_cls,chains[5],sizeof holster);
 object(pawn,sizeof pawn,pawn_cls,0x1122334400000001ULL);
 object(gun,sizeof gun,pistol_cls,0x1122334400000002ULL);
 memset(pistol_vtable,0,sizeof pistol_vtable);pistol_vtable[0x778/sizeof(void*)]=(void*)ready_pistol;
 putp(gun,0,pistol_vtable);
 object(ammo,sizeof ammo,ammo_cls,0x1122334400000003ULL);
 ammo_vtable[0x670/sizeof(void*)]=(void*)can_grab_ammo;putp(ammo,0,ammo_vtable);
 object(left_hand,sizeof left_hand,hand_cls,0x1122334400000004ULL);
 object(right_hand,sizeof right_hand,hand_cls,0x1122334400000005ULL);
 object(gun_grip,sizeof gun_grip,grip_cls,0x1122334400000006ULL);
 object(slide_grip,sizeof slide_grip,grip_cls,0x1122334400000007ULL);
 object(ammo_grip,sizeof ammo_grip,grip_cls,0x1122334400000008ULL);
 object(holster_grip,sizeof holster_grip,grip_cls,0x1122334400000009ULL);
 object(holster,sizeof holster,holster_cls,0x112233440000000aULL);
 object(other_hand,sizeof other_hand,hand_cls,0x112233440000000bULL);
 object(other_grip,sizeof other_grip,grip_cls,0x112233440000000cULL);
 right_hand[0x314]=other_hand[0x314]=1;
 putp(gun,0x270,gun_grip);putp(gun,0xd38,slide_grip);putp(gun,0x8e0,ammo);putp(gun,0x3c0,pawn);
 putp(ammo,0x270,ammo_grip);putp(ammo,0x578,gun);putp(ammo,0x3c0,pawn);
 void* grips[]={gun_grip,slide_grip,ammo_grip,holster_grip};
 void* owners[]={gun,gun,ammo,holster};
 for(int i=0;i<4;i++) {putp(grips[i],0x20,owners[i]);putp(grips[i],0x300,owners[i]);g_grips[i].g=grips[i];}
 g_ngrips=4;putp(holster,0x348,pawn);putp(holster,0x248,holster_grip);
 props[0]=ammo;putp(holster,0x350,props);*(int*)(holster+0x358)=*(int*)(holster+0x35c)=1;
 struct reload_tf identity={{0,0,0,1},{0,0,0},{1,1,1},{0,0}};
 memset(root,0,sizeof root);memcpy(root+0x1c0,&identity,48);
 putp(left_hand,0x130,root);putp(right_hand,0x130,root);putp(ammo,0x5b0,root);putp(gun,0xc80,root);
 rl_player=player;rl_active=active;rl_gun1=gun1;rl_left=left;rl_right=right;rl_primary=primary;
 rl_pistol_class=pistol_class;rl_ammo_class=ammo_class;rl_holster_class=holster_class;
 rl_hand_class=hand_class;rl_pawn_class=pawn_class;rl_grip_class=grip_class;
 rl_get_pawn=get_pawn;rl_get_holster=get_holster;rl_paused=is_paused;rl_player_valid=valid;
 rl_can_grab_holstered_ammo=can_grab_ammo;
 rl_interactivity=interactivity;rl_ammo_available=available;rl_reloading=progress;
 rl_ready=load_ready;rl_overlap=overlap;rl_begin=begin;rl_cached_transform=cached;rl_world_transform=world;
 rl_type=pawn_type;rl_base_ready=ready_base;rl_pistol_ready=ready_pistol;
 rl_slide_hand=slide_hand;rl_held_ammo_ready=held_ready;rl_trigger=trigger;
 rl_cock_blending=blending;rl_cock_flourishing=flourish;
 f_GetCurrentGrip=current_grip;f_SetGripForcedHover=force_hover;
 rl_pawn=rl_weapon=rl_hover_hand=rl_hover_grip=rl_mobile_holster=rl_mobile_hand=0;
 rl_deadline=rl_eject_end=rl_grip_until=rl_prior_grip_until=rl_eject_until=rl_prior_eject_until=rl_mobile_until=0;
 rl_hold_pose_valid=0;memset(&rl_post,0,sizeof rl_post);
 memset(&rl_run,0,sizeof rl_run);rl_main_hand=rl_other_hand=0;rl_main_side=1;rl_grip_bit=29;rl_eject_bit=1;rl_angle_offset=0xd50;rl_family=0;
 memset((void*)g_cmd_until,0,sizeof g_cmd_until);g_reload_phase=g_touch_reload_request=0;
 g_mobile_holster=g_mobile_holster_hand=0;g_mobile_holster_until=0;
 g_touch_gt_tid=test_tid=17;g_touch_cancel=g_touch_weapon_request=0;g_touch_weapon=2;g_touch_context=1;
 current_right=gun_grip;current_left=slide_owner=ammo_owner=0;active_pawn=pawn;
 reloading=paused=native_trigger=held_ammo_ready=0;player_valid=type=base_ready=1;
 grab_blending=flourishing=0;
 cancel_on_trigger=foreign_on_trigger=rename_on_trigger=select_on_trigger=phase_cancel_on_trigger=0;
 cock_on_trigger=expire_on_trigger=0;clock_delay=0;
 test_user_fire=cancel_on_user_read=0;
 begin_calls=world_calls=hover_calls=native_downs=native_ups=native_shots=native_cocks=0;
 native_ammo=10;native_reserves=19;prior_native_down=0;test_now=1000;
 dominant_side=1;
}
static void begin_macro(void) {
 g_touch_reload_request=1;tick(right_hand,1,1000);assert(g_reload_phase==1 && rl_post.valid);
 tick(left_hand,0,1200);assert(g_reload_phase==2);
 current_left=ammo_grip;ammo_owner=left_hand;
 tick(left_hand,0,1210);assert(g_reload_phase==3 && !begin_calls);
 tick(left_hand,0,1210+g_reload_approach_ms);assert(g_reload_phase==4 && begin_calls==1);
 reloading=0;tick(left_hand,0,1810);assert(g_reload_phase==5 && !g_cmd_until[29]);
 current_left=ammo_owner=0;
}
static void native_tick(long now) {
 int down=test_user_fire || now<g_cmd_until[dominant_side ? 26 : 28];
 if(down && !prior_native_down) {
  native_downs++;
  if(ready_pistol(gun)) {native_ammo--;native_shots++;}
  else if(base_ready && !gun[0xdd5] && !slide_owner && !held_ammo_ready && !ammo_owner) {
   gun[0xdd5]=1;native_cocks++;
  }
 }
 if(!down && prior_native_down) native_ups++;
 prior_native_down=down;native_trigger=down;(dominant_side ? right_hand : left_hand)[0x33c]=(unsigned char)down;
}
static void wait_for_pulse(void) {
 tick(right_hand,1,1820);assert(g_reload_phase==5 && !g_cmd_until[26]);
 tick(right_hand,1,1969);assert(g_reload_phase==5 && !g_cmd_until[26]);
 tick(right_hand,1,1970);assert(g_reload_phase==6 && g_cmd_until[26]==2120);
}
static void installer_cases(const char* native_path) {
#ifdef _WIN32
 const unsigned addresses[]={
  0x633d030,0x62ac2c8,0x63380dc,0x62a8db4,0x62bbc30,0x642e548,0x61d0934,0x61d401c,
  0x628cf08,0x628d2e8,0x628c410,0x628fed0,0x6290128,0x628e1c4,0x61e3c88,0x83a926c,
  0x62bbd24,0x62bbd7c,0x640dea8,0x6434184,0x6272bd8,0x62bbea0,0x62c08c4,0x62c098c,
  0x6271e30,0x62c0908,0x61e3cb0,0x61e3cdc,0x61e4120,0x6424b5c,0x642c718,0x64192f4,
  0x6338aec,0x628d268,0x62b6034,0x62b5ed8,0x628e090,0x628d978,0x62b59ac,0x62b59bc,
  0x62b5a30,0x62b5e98,0x62b5ea4,0x62b5eb4,0x62b6138,0x642e5d0,0x62825b0,0x62bb880,0x62bb584,
  0x62a8e50,0x6272bbc
  ,0x62729d0,0x62c0978,0x62c0980
 };
 const unsigned fields[]={0x9daded8+0x24,0x9d9e298+0x24};
 _Static_assert(sizeof addresses/sizeof *addresses==54,"every checked native instruction pair");
 unsigned char* image=VirtualAlloc((void*)(uintptr_t)0x20000000,0x9dadf08,0x3000,4);
 assert(image && (uintptr_t)image<=UINT32_MAX);
 FILE* input=0;assert(!fopen_s(&input,native_path,"rb") && input);
 for(unsigned i=0;i<54;i++) {
  assert(!fseek(input,(long)addresses[i],SEEK_SET));assert(fread(image+addresses[i],1,8,input)==8);
 }
 for(unsigned i=0;i<2;i++) {
  assert(!fseek(input,(long)fields[i],SEEK_SET));assert(fread(image+fields[i],1,4,input)==4);
 }
 assert(!fclose(input));
 rl_player=0;rl_hand_class=0;rl_type=0;install_touch_reload((unsigned long)(uintptr_t)image);
 assert(rl_player && rl_hand_class && rl_type);
 for(unsigned i=0;i<54;i++) for(unsigned word=0;word<2;word++) {
  unsigned* location=(unsigned*)(image+addresses[i]+4*word);*location^=1;
  rl_player=0;rl_hand_class=0;rl_type=0;install_touch_reload((unsigned long)(uintptr_t)image);
  assert(!rl_player && !rl_hand_class && !rl_type);*location^=1;
 }
 for(unsigned i=0;i<2;i++) {
  unsigned* location=(unsigned*)(image+fields[i]);*location^=1;
  rl_player=0;rl_hand_class=0;rl_type=0;install_touch_reload((unsigned long)(uintptr_t)image);
  assert(!rl_player && !rl_hand_class && !rl_type);*location^=1;
 }
 assert(VirtualFree(image,0,0x8000));
#else
 (void)native_path;
#endif
}
int main(int argc,char** argv) {
 assert(argc==2);installer_cases(argv[1]);
 fixture();struct rl_post_binding binding;
 assert(rl_post_binding(right_hand,&binding));
 const void* expected[]={right_hand,pawn,gun,left_hand,gun_grip,slide_grip,ammo,ammo_grip};
 for(int i=0;i<8;i++) assert(binding.object[i]==expected[i]);
 current_right=other_grip;assert(!rl_post_binding(right_hand,&binding));

 fixture();current_right=0;g_touch_reload_request=1;tick(right_hand,1,1000);
 assert(!g_touch_reload_request && !g_reload_phase && !g_cmd_until[1]);
 current_right=gun_grip;tick(right_hand,1,1100);assert(!g_reload_phase);
 fixture();g_touch_reload_request=1;tick(left_hand,0,1000);assert(g_touch_reload_request && !g_reload_phase);
 tick(right_hand,1,1001);assert(g_reload_phase==1 && !g_touch_reload_request);
 putn(left_hand,0x7777666655554444ULL);tick(left_hand,0,1200);assert(!g_reload_phase && !hover_calls);
 for(int why=0;why<3;why++){
  fixture();g_touch_reload_request=1;tick(right_hand,1,1000);assert(g_reload_phase==1);
  if(why==0)g_touch_inventory_request=1;if(why==1)g_touch_pause=1;if(why==2)g_touch_interact_request=1;
  tick(left_hand,0,1200);assert(!g_reload_phase && !hover_calls);
  g_touch_inventory_request=g_touch_pause=g_touch_interact_request=0;
 }

 fixture();begin_macro();wait_for_pulse();native_tick(1970);
 assert(native_cocks==1 && native_downs==1 && !native_shots && native_ammo==10 && native_reserves==19);
 tick(right_hand,1,1980);assert(g_reload_phase==6 && !g_cmd_until[26]);native_tick(1980);
 assert(native_ups==1);tick(right_hand,1,1981);assert(!g_reload_phase);
 tick(right_hand,1,2200);native_tick(2200);
 assert(native_downs==1 && native_cocks==1 && !native_shots);

 fixture();dominant_side=0;current_right=0;current_left=gun_grip;rl_gun1=0;
 g_touch_reload_request=1;tick(left_hand,0,1000);assert(g_reload_phase==1 && g_cmd_until[9]==1180 && !g_cmd_until[1]);
 tick(right_hand,1,1200);assert(g_reload_phase==2 && g_cmd_until[27] && !g_cmd_until[29]);
 current_right=ammo_grip;ammo_owner=right_hand;tick(right_hand,1,1210);assert(g_reload_phase==3);
 tick(right_hand,1,1210+g_reload_approach_ms);assert(g_reload_phase==4);
 reloading=0;tick(right_hand,1,1810);assert(g_reload_phase==5 && !g_cmd_until[27]);current_right=ammo_owner=0;
 tick(left_hand,0,1820);tick(left_hand,0,1970);assert(g_reload_phase==6 && g_cmd_until[28]==2120 && !g_cmd_until[26]);
 native_tick(1970);assert(native_cocks==1 && !native_shots && native_ammo==10);
 tick(left_hand,0,1980);assert(g_reload_phase==6 && !g_cmd_until[28]);native_tick(1980);tick(left_hand,0,1981);
 assert(!g_reload_phase && native_ups==1 && !native_shots);

 fixture();begin_macro();gun[0xdd5]=1;tick(right_hand,1,1820);
 assert(!g_reload_phase && !g_cmd_until[26] && !native_shots);

 fixture();begin_macro();current_left=ammo_grip;ammo_owner=left_hand;
 tick(right_hand,1,1820);tick(right_hand,1,1970);assert(!g_cmd_until[26]);
 current_left=ammo_owner=0;tick(right_hand,1,1980);tick(right_hand,1,2129);assert(!g_cmd_until[26]);
 tick(right_hand,1,2130);assert(g_reload_phase==6 && g_cmd_until[26]==2280);

 fixture();begin_macro();wait_for_pulse();native_tick(1970);test_user_fire=1;
 tick(right_hand,1,1980);assert(g_reload_phase==6 && !g_cmd_until[26]);native_tick(1980);
 assert(native_downs==1 && !native_shots);
 test_user_fire=0;native_tick(1981);tick(right_hand,1,1982);assert(!g_reload_phase);
 test_user_fire=1;native_tick(1990);assert(native_shots==1 && native_ammo==9 && native_reserves==19);

 fixture();begin_macro();native_trigger=1;right_hand[0x33c]=1;
 tick(right_hand,1,1820);tick(right_hand,1,1970);assert(!g_cmd_until[26]);

 fixture();begin_macro();base_ready=0;tick(right_hand,1,1820);tick(right_hand,1,1970);assert(!g_cmd_until[26]);
 fixture();begin_macro();slide_owner=left_hand;tick(right_hand,1,1820);tick(right_hand,1,1970);assert(!g_cmd_until[26]);
 fixture();begin_macro();held_ammo_ready=1;tick(right_hand,1,1820);tick(right_hand,1,1970);assert(!g_cmd_until[26]);
 fixture();begin_macro();gun[0xdb8]=1;tick(right_hand,1,1820);tick(right_hand,1,1970);assert(!g_cmd_until[26]);
 fixture();begin_macro();grab_blending=1;tick(right_hand,1,1820);tick(right_hand,1,1970);assert(!g_cmd_until[26]);
 fixture();begin_macro();flourishing=1;tick(right_hand,1,1820);tick(right_hand,1,1970);assert(!g_cmd_until[26]);

 for(int failure=0;failure<9;failure++) {
  fixture();begin_macro();tick(right_hand,1,1820);
  if(failure==0) g_touch_cancel=3;
  if(failure==1) g_touch_weapon_request=1;
  if(failure==2) g_touch_weapon=1;
  if(failure==3) g_touch_context=2;
  if(failure==4) type=2;
  if(failure==5) active_pawn=other_hand;
  if(failure==6) player_valid=0;
  if(failure==7) paused=1;
  if(failure==8) test_tid=18;
  tick(right_hand,1,1970);assert(!g_cmd_until[26]);
 }
 for(int name=0;name<8;name++) {
  fixture();begin_macro();tick(right_hand,1,1820);
  uint64_t old;memcpy(&old,(const char*)expected[name]+0x18,sizeof old);
  putn((void*)expected[name],old^0x8000000000000000ULL);
  tick(right_hand,1,1970);assert(!g_cmd_until[26]);
 }
 fixture();begin_macro();tick(right_hand,1,1820);test_user_fire=1;
 tick(right_hand,1,1970);assert(!g_cmd_until[26]);
 fixture();begin_macro();tick(right_hand,1,1820);g_cmd_until[26]=5000;
 tick(right_hand,1,1970);assert(g_cmd_until[26]==5000);

 fixture();begin_macro();tick(right_hand,1,1820);cancel_on_trigger=1;
 tick(right_hand,1,1970);assert(!g_cmd_until[26]);
 fixture();begin_macro();tick(right_hand,1,1820);foreign_on_trigger=1;
 tick(right_hand,1,1970);assert(!g_cmd_until[26]);
 fixture();begin_macro();tick(right_hand,1,1820);rename_on_trigger=1;
 tick(right_hand,1,1970);assert(!g_cmd_until[26]);
 fixture();begin_macro();tick(right_hand,1,1820);cancel_on_user_read=1;
 tick(right_hand,1,1970);assert(!g_cmd_until[26]);

 int late_failures=0;
 for(int race=0;race<7;race++) {
  fixture();begin_macro();tick(right_hand,1,1820);
  if(race==0) cancel_on_trigger=2;
  if(race==1) foreign_on_trigger=2;
  if(race==2) rename_on_trigger=2;
  if(race==3) select_on_trigger=2;
  if(race==4) phase_cancel_on_trigger=2;
  if(race==5) cock_on_trigger=2;
  if(race==6) expire_on_trigger=2;
  tick(right_hand,1,1970);
  if(g_cmd_until[26]) {printf("FAIL: RT published after final getter race %d\n",race);late_failures++;}
 }
 fflush(stdout);assert(!late_failures);

 fixture();begin_macro();wait_for_pulse();g_cmd_until[26]=7000;
 g_touch_cancel=3;tick(right_hand,1,1980);assert(g_cmd_until[26]==7000);
 fixture();begin_macro();wait_for_pulse();tick(right_hand,1,2120);
 assert(!g_cmd_until[26]);gun[0xdd5]=1;tick(right_hand,1,2130);tick(right_hand,1,2400);
 assert(!g_cmd_until[26]);
 fixture();begin_macro();tick(other_hand,1,1820);tick(other_hand,1,1970);assert(!g_cmd_until[26]);
 puts("PASS: actual reload header cocking completion, native up acknowledgement, one edge without a shot, ownership/races, bounded pulse and 110 installer corruption cases");
 return 0;
}
