#include <math.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
typedef int test_reload_mutex_t;
#define pthread_mutex_t test_reload_mutex_t
static void pthread_mutex_lock(pthread_mutex_t* m) {(void)m;}
static void pthread_mutex_unlock(pthread_mutex_t* m) {(void)m;}
static void test_log(const char* format,...) __attribute__((format(printf,1,2)));
#define LOGF(...) test_log(__VA_ARGS__)
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
static volatile int g_touch_reload_request,g_reload_phase;
static volatile int g_touch_gt_tid=77,g_touch_cancel,g_touch_weapon_request,g_touch_weapon=2,g_touch_context=1;
static volatile int g_touch_inventory_request,g_touch_pause,g_touch_interact_request;
static int touch_quickselect_busy(void){return 0;}
static int gettid(void) {return 77;}
static long ptr_now_ms(void) {return 1000;}
static int touch_reload_user_fire(long now) {(void)now;return 0;}
static void* g_mobile_holster,*g_mobile_holster_hand;
static volatile long g_mobile_holster_until;
static void* (*f_GetCurrentGrip)(const void*);
static void (*f_SetGripForcedHover)(void*,void*,int);
static void* touch_equipped_gun(void* hand);
static int reload_family_info(void* gun,int* offset){(void)gun;(void)offset;return 0;}
static int test_shell_loaded,test_shell_pending,test_family_complete;
static int reload_family_shells_pending(void* gun,int* loaded){(void)gun;*loaded=test_shell_loaded;return test_shell_pending;}
static int reload_family_post_begin(void* hand,void* gun,long now){(void)hand;(void)gun;(void)now;return test_family_complete;}
static int reload_family_post_tick(void* hand,int side,long now){(void)hand;(void)side;(void)now;return test_family_complete?2:0;}
static void reload_family_post_cancel(void){}
#include "../re4fix_reload.h"
static unsigned char test_pawn[4096],test_gun[4096],test_ammo[2048],test_hand[0x500],test_grip[64];
static unsigned char other_pawn[16],other_hand[0x500],other_left[0x500],same_pawn_left[0x500],right_hand[0x500];
static unsigned char pistol_class[0x70],ammo_class[0x70],holster_class[0x70],hand_class[0x70];
static unsigned char test_holster[1024],holster_grip[64],other_grip[64],scene_root[0x300],primary_grip[64];
static unsigned char second_ammo[2048],second_gun[4096];
static void* ammo_vtable[0x678/sizeof(void*)];
static void* active_pawn=test_pawn;
static const void* pistol_chain[1];
static const void* ammo_chain[1],*holster_chain[1];
static void* holster_props[2];
static int test_reloading=1;
static int force_on,force_off,begin_calls,world_writes,test_ready=1,test_overlap=1,holster_missing;
static int ammo_eligible,second_eligible,eligibility_calls,eligibility_cancel,eligibility_rename;
static int mock_current_ammo,mock_reserves;
static int tf_diagnostic_logs;
static char tf_diagnostics[3][700];
static void test_log(const char* format,...) {
 if(strncmp(format,"RELOADTF ",9)) return;
 assert(tf_diagnostic_logs<3);
 va_list args;va_start(args,format);
 int bytes=vsnprintf(tf_diagnostics[tf_diagnostic_logs],sizeof tf_diagnostics[0],format,args);
 va_end(args);assert(bytes>0 && bytes<650);tf_diagnostic_logs++;
}
static void* hovered,*held_left;
static void* touch_equipped_gun(void* hand){return hand==right_hand ? test_gun : 0;}
static struct reload_tf mock_capsule,mock_target;
static void* test_player(const void* hand) {return hand==other_hand || hand==other_left ? other_pawn : test_pawn;}
static void* test_active(void) {return active_pawn;}
static void* test_left(const void* pawn) {(void)pawn;return test_hand;}
static void* test_primary(const void* gun) {return gun==test_ammo ? (held_left?test_hand:0):right_hand;}
static void* test_pistol_class(void) {return pistol_class;}
static int test_ammo_available(const void* gun) {assert(gun==test_gun);return mock_reserves>0;}
static void* test_current_grip(const void* hand) {return hand==test_hand ? held_left : hand==right_hand ? primary_grip : 0;}
static void* test_gun1(const void* pawn) {(void)pawn;return test_gun;}
static int test_paused(void) {return 0;}
static int test_valid(void) {return 1;}
static int test_interactivity(const void* gun) {(void)gun;return 2;}
static int test_progress(void* gun) {(void)gun;return test_reloading;}
static void* test_ammo_class(void) {return ammo_class;}
static void* test_holster_class(void) {return holster_class;}
static void* test_get_pawn(const void* prop) {return *(void* const*)((const char*)prop+0x3c0);}
static void* test_get_holster(const void* prop) {assert(prop==test_ammo||prop==second_ammo);return holster_missing ? 0 : test_holster;}
static int test_can_grab_ammo(const void* prop) {
 assert(prop==test_ammo||prop==second_ammo);eligibility_calls++;
 if(eligibility_cancel)g_touch_cancel=1;
 if(eligibility_rename)((unsigned char*)prop)[0x1c]^=1;
 return prop==test_ammo ? ammo_eligible : second_eligible;
}
static void test_force(void* hand,void* grip,int on) {
 assert(hand==test_hand && grip==holster_grip); // Direct Ammo.Primary is never forced.
 if(on){force_on++;hovered=grip;}else{force_off++;hovered=0;}
}
static int test_load_ready(const void* gun,float dt) {assert(gun==test_gun && dt==0);return test_ready;}
static int test_load_overlap(const void* gun,float tolerance) {(void)tolerance;assert(gun==test_gun);return test_overlap;}
static void test_begin(void* gun) {assert(gun==test_gun);begin_calls++;test_reloading=1;}
static struct reload_tf test_cache(const void* cache) {
 return cache==test_ammo+0x5b0 ? mock_capsule : mock_target;
}
static void test_world(void* root,const struct reload_tf* tf,int sweep,void* hit,int teleport) {
 assert(root==scene_root && !sweep && !hit && !teleport);world_writes++;memcpy(scene_root+0x1c0,tf,48);
}
static int test_grip_down(const void* hand){assert(hand==test_hand);return 0;}
static void fixture(void) {
 memset(test_gun,0,sizeof test_gun);memset(test_ammo,0,sizeof test_ammo);memset(test_holster,0,sizeof test_holster);
 memset(test_hand,0,sizeof test_hand);memset(right_hand,0,sizeof right_hand);memset(other_hand,0,sizeof other_hand);
 memset(other_left,0,sizeof other_left);memset(same_pawn_left,0,sizeof same_pawn_left);memset(scene_root,0,sizeof scene_root);
 memset(pistol_class,0,sizeof pistol_class);memset(ammo_class,0,sizeof ammo_class);memset(holster_class,0,sizeof holster_class);
 pistol_chain[0]=pistol_class+0x30;*(const void***)(pistol_class+0x30)=pistol_chain;
 ammo_chain[0]=ammo_class+0x30;*(const void***)(ammo_class+0x30)=ammo_chain;
 holster_chain[0]=holster_class+0x30;*(const void***)(holster_class+0x30)=holster_chain;
 *(int*)(pistol_class+0x58)=sizeof test_gun;*(int*)(ammo_class+0x58)=sizeof test_ammo;
 *(int*)(holster_class+0x58)=sizeof test_holster;*(int*)(hand_class+0x58)=sizeof test_hand;
 *(void**)(test_gun+0x10)=pistol_class;*(void**)(test_ammo+0x10)=ammo_class;
 *(void**)(test_holster+0x10)=holster_class;
 void* hands[]={test_hand,right_hand,other_hand,other_left,same_pawn_left};
 for(unsigned i=0;i<sizeof hands/sizeof *hands;i++) *(void**)((char*)hands[i]+0x10)=hand_class;
 right_hand[0x314]=other_hand[0x314]=1;
 *(void**)(test_gun+0x8e0)=test_ammo;*(void**)(test_ammo+0x270)=test_grip;
 *(void**)(test_gun+0x270)=primary_grip;*(void**)(primary_grip+0x20)=test_gun;
 *(void**)(test_gun+0x3c0)=test_pawn;*(void**)(test_ammo+0x3c0)=test_pawn;
 *(void**)(test_ammo+0x578)=test_gun;*(void**)(test_grip+0x20)=test_ammo;
 ammo_vtable[0x670/sizeof(void*)]=(void*)test_can_grab_ammo;*(void**)test_ammo=ammo_vtable;
 memset(second_ammo,0,sizeof second_ammo);*(void**)second_ammo=ammo_vtable;
 *(void**)(second_ammo+0x10)=ammo_class;*(void**)(second_ammo+0x3c0)=test_pawn;*(void**)(second_ammo+0x578)=second_gun;
 *(void**)(test_holster+0x348)=test_pawn;*(void**)(test_holster+0x248)=holster_grip;
 *(void**)(holster_grip+0x20)=test_holster;holster_props[0]=test_ammo;
 *(void***)(test_holster+0x350)=holster_props;*(int*)(test_holster+0x358)=*(int*)(test_holster+0x35c)=1;
 g_grips[0].g=test_grip;g_grips[1].g=holster_grip;g_grips[2].g=primary_grip;g_ngrips=3;
 rl_player=test_player;rl_active=test_active;rl_gun1=test_gun1;rl_paused=test_paused;rl_player_valid=test_valid;
 rl_interactivity=test_interactivity;rl_reloading=test_progress;rl_left=test_left;rl_primary=test_primary;
 rl_pistol_class=test_pistol_class;rl_ammo_class=test_ammo_class;rl_holster_class=test_holster_class;
 rl_get_pawn=test_get_pawn;rl_get_holster=test_get_holster;rl_ammo_available=test_ammo_available;
 rl_can_grab_holstered_ammo=test_can_grab_ammo;
 f_GetCurrentGrip=test_current_grip;f_SetGripForcedHover=test_force;ti_down=test_grip_down;
 rl_ready=test_load_ready;rl_overlap=test_load_overlap;rl_begin=test_begin;
 rl_cached_transform=test_cache;rl_world_transform=test_world;
 mock_capsule=mock_target=(struct reload_tf){{0,0,0,1},{0,0,0},{1,1,1},{0,0}};
 *(void**)(test_hand+0x130)=scene_root;memcpy(scene_root+0x1c0,&mock_capsule,48);
 *(void**)(test_ammo+0x5b0)=scene_root;*(void**)(test_gun+0xc80)=scene_root;
 active_pawn=test_pawn;test_reloading=0;test_ready=test_overlap=1;holster_missing=0;held_left=hovered=0;
 force_on=force_off=begin_calls=world_writes=0;
 ammo_eligible=1;second_eligible=eligibility_calls=eligibility_cancel=eligibility_rename=0;
 mock_current_ammo=0;mock_reserves=20;g_touch_cancel=g_touch_weapon_request=g_touch_inventory_request=g_touch_pause=g_touch_interact_request=0;
 g_touch_context=1;g_touch_weapon=2;
 memset((void*)g_cmd_until,0,sizeof g_cmd_until);g_reload_phase=g_touch_reload_request=0;
 rl_pawn=rl_weapon=rl_hover_hand=rl_hover_grip=rl_mobile_holster=rl_mobile_hand=0;
 rl_deadline=rl_eject_end=rl_grip_until=rl_prior_grip_until=rl_eject_until=rl_prior_eject_until=rl_mobile_until=0;
 rl_hold_pose_valid=0;g_mobile_holster=g_mobile_holster_hand=0;g_mobile_holster_until=0;
 memset(&rl_run,0,sizeof rl_run);rl_main_hand=rl_other_hand=0;rl_main_side=1;rl_grip_bit=29;rl_eject_bit=1;rl_angle_offset=0xd50;rl_family=0;
}
int main(void) {
 assert(reload_pose_selfcheck());
 // The magazine begins without a jump, follows a moved/rotated well, dips
 // below the grip and reaches the exact native target with unit rotation.
 struct reload_tf feed_from={{0,0,0,1},{20,-25,110},{1,1,1},{0,0}};
 struct reload_tf feed_anchor={{0,0,0,1},{40,8,140},{1,1,1},{0,0}};
 struct reload_tf feed_finish=feed_from;feed_finish.p[0]=40;feed_finish.p[1]=8;feed_finish.p[2]=140;
 for(int i=0;i<=100;i++) {
  float t=(float)i/100.0f;
  struct reload_tf pose=reload_approach_pose(feed_from,feed_anchor,feed_anchor,feed_finish,t);
  assert(reload_tf_valid(&pose));
  float e=t*t*(3-2*t),arc=t*t*(1-t)*(1-t);
  for(int k=0;k<3;k++)assert(fabsf(pose.p[k]-(feed_from.p[k]+(feed_finish.p[k]-feed_from.p[k])*e-(k==2 ? 128*arc : 0)))<.0001f);
  if(i==0)assert(!memcmp(&pose,&feed_from,sizeof pose));
  if(i==100)assert(!memcmp(&pose,&feed_finish,sizeof pose));
 }
 struct reload_tf moved_anchor=feed_anchor;moved_anchor.q=(qf_t){0,0,.6f,.8f};
 moved_anchor.p[0]+=50;moved_anchor.p[1]-=30;
 struct reload_tf moved_finish=reload_hand_pose(feed_finish,feed_anchor,moved_anchor);
 struct reload_tf moved_start=reload_hand_pose(feed_from,feed_anchor,moved_anchor);
 struct reload_tf actual_start=reload_approach_pose(feed_from,feed_anchor,moved_anchor,moved_finish,0);
 assert(reload_tf_valid(&actual_start));
 for(int k=0;k<3;k++)assert(fabsf(actual_start.p[k]-moved_start.p[k])<.0001f);
 struct reload_tf moved_end=reload_approach_pose(feed_from,feed_anchor,moved_anchor,moved_finish,1);
 for(int k=0;k<3;k++)assert(fabsf(moved_end.p[k]-moved_finish.p[k])<.0001f);
 unsigned char native_bytes[48]={0};
 const float native_q[4]={0,0,0,1}, native_p[3]={12,34,56}, native_s[3]={1,1,1};
 memcpy(native_bytes,native_q,16);memcpy(native_bytes+16,native_p,12);memcpy(native_bytes+28,native_s,12);
 struct reload_tf decoded;memcpy(&decoded,native_bytes,48);
 assert(decoded.p[2]==56 && decoded.s[0]==1 && decoded.s[2]==1 && reload_tf_valid(&decoded));
 decoded.s[2]=0;assert(!reload_tf_valid(&decoded));
 for (int i=0;i<100;i++) {
  float a=i*0.031f,b=i*0.043f;
  struct reload_tf h={{0,sinf(a),0,cosf(a)},{2,3,4},{1,1,1},{0,0}};
  struct reload_tf c=h,t={{sinf(b),0,0,cosf(b)},{9,8,7},{1,1,1},{0,0}};
  qf_t marker_q={0,0,0.6f,0.8f};
  float local[3]={1,2,3},world[3]; q_rot(h.q,local,world);
  for(int j=0;j<3;j++) c.p[j]=h.p[j]+world[j];
  c.q=q_mul(h.q,marker_q);
  c.s[0]=c.s[1]=c.s[2]=.606456f;
  t.s[0]=.25f+i*.01f;t.s[1]=.5f;t.s[2]=1.5f;
  assert(reload_marker_valid(&c) && reload_marker_valid(&t));
  struct reload_tf out=reload_hand_pose(h,c,t);
  assert(reload_tf_valid(&out) && out.s[0]==1 && out.s[1]==1 && out.s[2]==1 && c.s[0]==.606456f);
  q_rot(out.q,local,world);
  for(int j=0;j<3;j++) assert(fabsf(out.p[j]+world[j]-t.p[j])<0.0001f);
  qf_t marker_out=q_mul(out.q,marker_q);
  assert(fabsf(marker_out.x-t.q.x)<0.0001f && fabsf(marker_out.y-t.q.y)<0.0001f);
  assert(fabsf(marker_out.z-t.q.z)<0.0001f && fabsf(marker_out.w-t.q.w)<0.0001f);
 }
 struct reload_tf invalid={{0,0,0,1},{0,0,0},{2,1,1},{0,0}};
 assert(!reload_tf_valid(&invalid));
 invalid.s[0]=1; invalid.q.w=NAN; assert(!reload_tf_valid(&invalid));
 // World collision markers can carry authored scale independently of the
 // rigid hand pose. The parent retains unit scale and the marker keeps sizing.
 struct reload_tf authored_hand={{0,.6f,0,.8f},{10,20,30},{1,1,1},{0,0}};
 qf_t authored_local_q={.6f,0,0,.8f};float authored_local_p[3]={2,3,4},authored_world_p[3];
 q_rot(authored_hand.q,authored_local_p,authored_world_p);
 struct reload_tf authored_capsule={{0,0,0,1},{0,0,0},{.606456f,.606456f,.606456f},{0,0}};
 authored_capsule.q=q_mul(authored_hand.q,authored_local_q);
 for(int i=0;i<3;i++) authored_capsule.p[i]=authored_hand.p[i]+authored_world_p[i];
 struct reload_tf authored_target={{0,0,.6f,.8f},{40,50,60},{1,1,1},{0,0}};
 struct reload_tf authored_before=authored_capsule,authored_output=reload_hand_pose(authored_hand,authored_capsule,authored_target);
 assert(reload_marker_valid(&authored_capsule) && !reload_tf_valid(&authored_capsule));
 assert(reload_marker_valid(&authored_target) && reload_tf_valid(&authored_output));
 assert(!memcmp(&authored_before,&authored_capsule,sizeof authored_before));
 q_rot(authored_output.q,authored_local_p,authored_world_p);
 for(int i=0;i<3;i++) {
  assert(fabsf(authored_output.p[i]+authored_world_p[i]-authored_target.p[i])<.0001f);
  assert(authored_output.s[i]==1 && authored_capsule.s[i]==.606456f);
 }
 qf_t authored_world_q=q_mul(authored_output.q,authored_local_q);
 assert(fabsf(authored_world_q.x-authored_target.q.x)<.0001f && fabsf(authored_world_q.y-authored_target.q.y)<.0001f);
 assert(fabsf(authored_world_q.z-authored_target.q.z)<.0001f && fabsf(authored_world_q.w-authored_target.q.w)<.0001f);
 const float bad_scales[]={0,-.606456f,NAN,INFINITY};
 for(unsigned i=0;i<sizeof bad_scales/sizeof *bad_scales;i++) {
  struct reload_tf bad=authored_capsule;bad.s[1]=bad_scales[i];assert(!reload_marker_valid(&bad));
 }
 struct reload_tf bad_marker=authored_capsule;bad_marker.q.w=2;assert(!reload_marker_valid(&bad_marker));
 bad_marker=authored_capsule;bad_marker.p[0]=NAN;assert(!reload_marker_valid(&bad_marker));
 bad_marker=authored_capsule;bad_marker.q.x=INFINITY;assert(!reload_marker_valid(&bad_marker));
 g_cmd_until[29]=777; rl_grip_until=777; rl_prior_grip_until=1000; rl_deadline=2000; g_reload_phase=4;
 reload_stop("test completion"); assert(g_cmd_until[29]==1000 && !g_reload_phase && !rl_deadline);
 g_cmd_until[29]=999; rl_grip_until=888; rl_prior_grip_until=1000; rl_deadline=2000;
 reload_stop("external command"); assert(g_cmd_until[29]==999);
 g_cmd_until[29]=0; rl_grip_until=888; rl_deadline=2000;
 reload_stop("focus reset"); assert(g_cmd_until[29]==0);
 fixture();test_reloading=1;
 rl_pawn=test_pawn;rl_weapon=test_gun;rl_deadline=5000;rl_prior_grip_until=0;g_reload_phase=4;
 touch_reload_tick(test_hand,0,1000);assert(g_cmd_until[29]==1600 && g_reload_phase==4);
 touch_reload_tick(test_hand,0,2000);assert(g_cmd_until[29]==2600);
 touch_reload_tick(test_hand,0,4950);assert(g_cmd_until[29]==5000);
 test_reloading=0;touch_reload_tick(test_hand,0,4970);assert(!g_cmd_until[29] && !g_reload_phase);
 // Real helper request/lifecycle path: inactive pooled hands cannot consume/cancel it.
 g_cmd_until[1]=300;g_touch_reload_request=1;
 touch_reload_tick(other_hand,1,2000);assert(g_touch_reload_request==1 && !g_reload_phase);
 touch_reload_tick(right_hand,1,2000);
 assert(!g_touch_reload_request && g_reload_phase==1 && g_cmd_until[1]==2180);
 touch_reload_tick(other_left,0,2050);assert(g_reload_phase==1 && g_cmd_until[1]==2180);
 active_pawn=other_pawn;touch_reload_tick(other_left,0,2070);
 assert(!g_reload_phase && !rl_deadline && g_cmd_until[1]==300);
 active_pawn=test_pawn;g_touch_reload_request=1;touch_reload_tick(right_hand,1,2200);
 assert(g_reload_phase==1 && g_cmd_until[1]==2380);
 g_cmd_until[1]=9000;reload_stop("foreign B");assert(g_cmd_until[1]==9000);
 g_cmd_until[1]=3000;g_touch_reload_request=1;touch_reload_tick(right_hand,1,2400);
 assert(g_reload_phase==1 && !rl_eject_until && g_cmd_until[1]==3000);
 g_reload_phase=0;touch_reload_tick(other_left,0,2410);
 assert(!rl_deadline && g_cmd_until[1]==3000);
 g_cmd_until[29]=0;rl_grip_until=0;rl_prior_grip_until=9000;rl_deadline=4000;
 reload_stop("cancel before left grip ownership");assert(!g_cmd_until[29]);

 // Exact assigned ammo/holster ownership and layout: malformed targets do not
 // become a forced hover, nor do they eject a magazine on a fresh request.
 fixture();void* holster=0;
 assert(reload_ammo_primary(test_gun,test_pawn)==test_grip);
 assert(reload_ammo_holster_grip(test_ammo,test_pawn,&holster)==holster_grip && holster==test_holster);
 *(void**)(test_ammo+0x3c0)=other_pawn;assert(!reload_ammo_primary(test_gun,test_pawn));
 *(void**)(test_ammo+0x3c0)=test_pawn;*(void**)(test_ammo+0x578)=other_pawn;
 assert(!reload_ammo_primary(test_gun,test_pawn));*(void**)(test_ammo+0x578)=test_gun;
 *(void**)(test_grip+0x20)=test_holster;assert(!reload_ammo_primary(test_gun,test_pawn));
 *(void**)(test_grip+0x20)=test_ammo;*(int*)(ammo_class+0x58)=0x60f;
 assert(!reload_ammo_primary(test_gun,test_pawn));*(int*)(ammo_class+0x58)=sizeof test_ammo;
 *(void**)(test_ammo+0x10)=hand_class;assert(!reload_ammo_primary(test_gun,test_pawn));
 *(void**)(test_ammo+0x10)=ammo_class;g_ngrips=0;assert(!reload_ammo_primary(test_gun,test_pawn));g_ngrips=2;
 *(void**)(test_holster+0x348)=other_pawn;assert(!reload_ammo_holster_grip(test_ammo,test_pawn,&holster));
 *(void**)(test_holster+0x348)=test_pawn;*(int*)(holster_class+0x58)=0x35f;
 assert(!reload_ammo_holster_grip(test_ammo,test_pawn,&holster));*(int*)(holster_class+0x58)=sizeof test_holster;
 *(void**)(test_holster+0x10)=hand_class;assert(!reload_ammo_holster_grip(test_ammo,test_pawn,&holster));
 *(void**)(test_holster+0x10)=holster_class;g_ngrips=1;assert(!reload_ammo_holster_grip(test_ammo,test_pawn,&holster));g_ngrips=2;
 *(void**)(holster_grip+0x20)=test_ammo;assert(!reload_ammo_holster_grip(test_ammo,test_pawn,&holster));
 *(void**)(holster_grip+0x20)=test_holster;holster_props[0]=other_pawn;
 assert(!reload_ammo_holster_grip(test_ammo,test_pawn,&holster));holster_props[0]=test_ammo;
 holster_props[1]=other_pawn;*(int*)(test_holster+0x358)=*(int*)(test_holster+0x35c)=2;
 assert(!reload_ammo_holster_grip(test_ammo,test_pawn,&holster));holster_props[1]=0;
 assert(reload_ammo_holster_grip(test_ammo,test_pawn,&holster)==holster_grip);
 *(int*)(test_holster+0x358)=0;assert(!reload_ammo_holster_grip(test_ammo,test_pawn,&holster));
 *(int*)(test_holster+0x358)=33;assert(!reload_ammo_holster_grip(test_ammo,test_pawn,&holster));
 *(int*)(test_holster+0x358)=1;*(int*)(test_holster+0x35c)=0;
 assert(!reload_ammo_holster_grip(test_ammo,test_pawn,&holster));*(int*)(test_holster+0x35c)=129;
 assert(!reload_ammo_holster_grip(test_ammo,test_pawn,&holster));*(int*)(test_holster+0x35c)=1;
 *(void***)(test_holster+0x350)=0;assert(!reload_ammo_holster_grip(test_ammo,test_pawn,&holster));
 g_touch_reload_request=1;touch_reload_tick(right_hand,1,1000);
 assert(!g_touch_reload_request && !g_reload_phase && !g_cmd_until[1] && !force_on);

 // Actual campaign topology: pistol and shotgun pouches share AmmoHolster.
 // The native handler chooses the first ELIGIBLE item, not the sole item.
 // Start with an empty or partially used magazine, send the ordinary eject
 // first, then wait for native eligibility before any off-hand grip input.
 for(int loaded=0;loaded<=6;loaded+=6)for(int order=0;order<2;order++) {
  fixture();mock_current_ammo=loaded;ammo_eligible=0;
  holster_props[order]=test_ammo;holster_props[1-order]=second_ammo;
  *(int*)(test_holster+0x358)=*(int*)(test_holster+0x35c)=2;
  unsigned char original_gun[sizeof test_gun],original_ammo[sizeof test_ammo],original_second[sizeof second_ammo];
  memcpy(original_gun,test_gun,sizeof test_gun);memcpy(original_ammo,test_ammo,sizeof test_ammo);
  memcpy(original_second,second_ammo,sizeof second_ammo);
  g_touch_reload_request=1;touch_reload_tick(right_hand,1,1000);
  assert(g_reload_phase==1 && g_cmd_until[1]==1180 && !eligibility_calls);
  touch_reload_tick(test_hand,0,1200);
  assert(g_reload_phase==1 && !force_on && !g_cmd_until[29] && !world_writes);
  ammo_eligible=1;touch_reload_tick(test_hand,0,1210);
  assert(g_reload_phase==2 && force_on==1 && hovered==holster_grip && g_cmd_until[29]==1810);
  assert(!memcmp(holster_props,(void*[2]){order?second_ammo:test_ammo,order?test_ammo:second_ammo},sizeof holster_props));
  held_left=test_grip;touch_reload_tick(test_hand,0,1220);
  touch_reload_tick(test_hand,0,1220+g_reload_approach_ms);
  assert(g_reload_phase==4 && begin_calls==1 && mock_current_ammo==loaded && mock_reserves==20);
  assert(!memcmp(original_gun,test_gun,sizeof test_gun) && !memcmp(original_ammo,test_ammo,sizeof test_ammo)
      && !memcmp(original_second,second_ammo,sizeof second_ammo));
 }
 fixture();holster_props[0]=second_ammo;holster_props[1]=test_ammo;
 *(int*)(test_holster+0x358)=*(int*)(test_holster+0x35c)=2;second_eligible=1;
 g_touch_reload_request=1;touch_reload_tick(right_hand,1,1000);touch_reload_tick(test_hand,0,1200);
 assert(g_reload_phase==1 && !force_on && !g_cmd_until[29]); // native would choose the other pouch
 second_eligible=0;touch_reload_tick(test_hand,0,1210);assert(g_reload_phase==2 && force_on==1);
 second_eligible=1;touch_reload_tick(test_hand,0,1220);
 assert(g_reload_phase==1 && force_off==1 && !g_cmd_until[29] && !g_mobile_holster_until);
 second_eligible=0;touch_reload_tick(test_hand,0,1230);assert(g_reload_phase==2 && force_on==2 && g_cmd_until[29]==1830);
 for(int invalid=0;invalid<4;invalid++) {
  fixture();holster_props[1]=second_ammo;*(int*)(test_holster+0x358)=*(int*)(test_holster+0x35c)=2;
  if(invalid==0)*(void**)(second_ammo+0x3c0)=other_pawn;
  if(invalid==1)*(void**)(second_ammo+0x10)=hand_class;
  if(invalid==2)*(void**)second_ammo=0;
  if(invalid==3)rl_can_grab_holstered_ammo=0;
  g_touch_reload_request=1;touch_reload_tick(right_hand,1,1000);
  assert(!g_reload_phase && !g_cmd_until[1] && !force_on);
 }
 for(int race=0;race<2;race++) {
  fixture();g_touch_reload_request=1;touch_reload_tick(right_hand,1,1000);
  eligibility_cancel=race==0;eligibility_rename=race==1;touch_reload_tick(test_hand,0,1200);
  assert(!g_reload_phase && !force_on && !g_cmd_until[29] && !g_mobile_holster_until);
 }

 // Normal handoff: Holster.Primary is hovered, the authored callback then puts
 // Ammo.Primary in the left hand; only native ready+overlap can begin loading.
 fixture();g_touch_reload_request=1;touch_reload_tick(right_hand,1,1000);
 assert(g_reload_phase==1 && g_cmd_until[1]==1180);
 touch_reload_tick(same_pawn_left,0,1200);assert(!force_on && !g_cmd_until[29]);
 unsigned char gun_before[sizeof test_gun],ammo_before[sizeof test_ammo],holster_before[sizeof test_holster];
 memcpy(gun_before,test_gun,sizeof test_gun);memcpy(ammo_before,test_ammo,sizeof test_ammo);
 memcpy(holster_before,test_holster,sizeof test_holster);
 touch_reload_tick(test_hand,0,1200);
 assert(g_reload_phase==2 && force_on==1 && hovered==holster_grip);
 assert(g_mobile_holster==test_holster && g_mobile_holster_hand==test_hand && g_mobile_holster_until==3000);
 assert(g_cmd_until[29]==1800 && !begin_calls && !world_writes);
 assert(!memcmp(gun_before,test_gun,sizeof test_gun) && !memcmp(ammo_before,test_ammo,sizeof test_ammo));
 assert(!memcmp(holster_before,test_holster,sizeof test_holster));
 held_left=test_grip;holster_missing=1;*(int*)(test_holster+0x358)=0;test_ready=0;
 touch_reload_tick(test_hand,0,1210);
 assert(g_reload_phase==3 && force_off==1 && !hovered && !g_mobile_holster_until && !begin_calls && world_writes==1);
 test_ready=1;test_overlap=0;touch_reload_tick(test_hand,0,1220);assert(g_reload_phase==3 && !begin_calls);
 test_overlap=1;touch_reload_tick(test_hand,0,1230);assert(g_reload_phase==3 && !begin_calls);
 touch_reload_tick(test_hand,0,1210+g_reload_approach_ms);assert(g_reload_phase==4 && begin_calls==1);
 touch_reload_tick(test_hand,0,1800);assert(g_cmd_until[29]==2400 && begin_calls==1);
 test_reloading=0;touch_reload_tick(test_hand,0,1810);assert(!g_reload_phase && !g_cmd_until[29]);

 // A still-live right-hand weapon target owns the shared spatial publication.
 fixture();g_touch_reload_request=1;touch_reload_tick(right_hand,1,1000);
 g_mobile_holster=other_pawn;g_mobile_holster_hand=right_hand;g_mobile_holster_until=2500;
 touch_reload_tick(test_hand,0,1200);
 assert(g_reload_phase==1 && !force_on && !g_cmd_until[29] && g_mobile_holster_until==2500);
 touch_reload_tick(test_hand,0,2501);assert(g_reload_phase==2 && force_on==1 && g_mobile_holster_until==4301);
 // A replacement publication must survive cancellation of this reload macro.
 g_mobile_holster=other_pawn;g_mobile_holster_hand=right_hand;g_mobile_holster_until=7000;
 reload_stop("foreign target");assert(g_mobile_holster_until==7000);
 touch_reload_tick(test_hand,0,2510);assert(force_off==1 && !rl_hover_hand && g_mobile_holster_until==7000);
 fixture();g_touch_reload_request=1;touch_reload_tick(right_hand,1,1000);touch_reload_tick(test_hand,0,1200);
 reload_stop("own target");assert(!g_mobile_holster_until);
 touch_reload_tick(test_hand,0,1210);assert(force_off==1 && !rl_hover_hand);
 fixture();g_touch_reload_request=1;touch_reload_tick(right_hand,1,1000);touch_reload_tick(test_hand,0,1200);
 g_reload_phase=0;touch_reload_tick(test_hand,0,1210);
 assert(!g_mobile_holster_until && !rl_deadline && force_off==1 && !g_cmd_until[29]);

 // Typed cached getter ABI returns precisely the scalar FTransform, including
 // authored non-unit scale. Canary fields surround the whole 48-byte result.
 fixture();mock_capsule=(struct reload_tf){{0,0,.6f,.8f},{12,34,56},{.25f,.5f,.75f},{0,0}};
 struct { uint64_t before;struct reload_tf transform;uint64_t after; } guarded;
 guarded.before=UINT64_C(0x1a2b3c4d5e6f7081);guarded.after=UINT64_C(0x81706f5e4d3c2b1a);
 guarded.transform=rl_cached_transform(test_ammo+0x5b0);
 assert(guarded.before==UINT64_C(0x1a2b3c4d5e6f7081) && guarded.after==UINT64_C(0x81706f5e4d3c2b1a));
 assert(!memcmp(&guarded.transform,&mock_capsule,48) && !reload_tf_valid(&guarded.transform) && reload_marker_valid(&guarded.transform));

 // The measured authored capsule scale passes the insertion path without being
 // copied into Hand.Scale. Geometry and both native Begin gates still decide.
 fixture();mock_capsule.s[0]=mock_capsule.s[1]=mock_capsule.s[2]=.606456f;
 mock_target.p[0]=25;mock_target.q=(qf_t){0,0,.6f,.8f};
 g_touch_reload_request=1;touch_reload_tick(right_hand,1,1000);touch_reload_tick(test_hand,0,1200);
 held_left=test_grip;test_ready=0;touch_reload_tick(test_hand,0,1210);
 assert(g_reload_phase==3 && !begin_calls && world_writes==1);
 struct reload_tf actual_pose;memcpy(&actual_pose,scene_root+0x1c0,48);
 assert(reload_tf_valid(&actual_pose) && actual_pose.s[0]==1 && actual_pose.s[1]==1 && actual_pose.s[2]==1);
 assert(mock_capsule.s[0]==.606456f && mock_capsule.s[1]==.606456f && mock_capsule.s[2]==.606456f);
 test_ready=1;test_overlap=0;touch_reload_tick(test_hand,0,1220);assert(!begin_calls);
 test_overlap=1;touch_reload_tick(test_hand,0,1230);assert(!begin_calls && g_reload_phase==3);
 touch_reload_tick(test_hand,0,1210+g_reload_approach_ms);assert(begin_calls==1 && g_reload_phase==4);
 mock_target.s[0]=.5f;mock_target.s[1]=.75f;mock_target.s[2]=1.25f;
 touch_reload_tick(test_hand,0,1210+g_reload_approach_ms+10);assert(g_reload_phase==4 && begin_calls==1);
 memcpy(&actual_pose,scene_root+0x1c0,48);assert(reload_tf_valid(&actual_pose) && actual_pose.s[0]==1);
 fixture();mock_capsule.q.w=mock_target.q.w=1.004f;
 assert(reload_marker_valid(&mock_capsule) && reload_marker_valid(&mock_target));
 g_touch_reload_request=1;touch_reload_tick(right_hand,1,1000);touch_reload_tick(test_hand,0,1200);held_left=test_grip;
 touch_reload_tick(test_hand,0,1210);
 assert(!g_reload_phase && !world_writes && !begin_calls); // Result must remain normalized/unit too.

 // Three raw failure records maximum, each before any pose write or reload
 // Begin. Nonpositive marker scale, bad hand and bad target are still rejected.
 rl_transform_failure_logs=0;tf_diagnostic_logs=0;
 for(int failure=0;failure<4;failure++) {
  fixture();g_touch_reload_request=1;touch_reload_tick(right_hand,1,1000);touch_reload_tick(test_hand,0,1200);
  held_left=test_grip;
  if(failure==0) mock_capsule.s[0]=0;
  else if(failure==1) ((struct reload_tf*)(scene_root+0x1c0))->s[2]=0;
  else if(failure==2) {mock_target.q.w=NAN;test_hand[0x36c]=1;*(void**)(test_gun+0xcd0)=scene_root;}
  else mock_target.s[0]=-1;
  unsigned char scene_before[sizeof scene_root];memcpy(scene_before,scene_root,sizeof scene_root);
  memcpy(gun_before,test_gun,sizeof test_gun);memcpy(ammo_before,test_ammo,sizeof test_ammo);
  touch_reload_tick(test_hand,0,1210);
  assert(!g_reload_phase && !begin_calls && !world_writes);
  assert(!memcmp(scene_before,scene_root,sizeof scene_root) && !memcmp(gun_before,test_gun,sizeof test_gun));
  assert(!memcmp(ammo_before,test_ammo,sizeof test_ammo));
  assert(tf_diagnostic_logs==(failure<3 ? failure+1 : 3));
 }
 assert(strstr(tf_diagnostics[0],"ok=H1C0T1") && strstr(tf_diagnostics[0],"Cs=(0,1,1)"));
 assert(strstr(tf_diagnostics[1],"ok=H0C1T1") && strstr(tf_diagnostics[1],"Hs=(1,1,0)"));
 assert(strstr(tf_diagnostics[2],"cache=+cd0") && strstr(tf_diagnostics[2],"ok=H1C1T0"));
 assert(strstr(tf_diagnostics[0],"Hq=(0,0,0,1)") && strstr(tf_diagnostics[0],"Cp=(0,0,0)"));
 // The real shared reload state machine repeats only after native ammo count progress and UP.
 fixture();rl_family=5;rl_pawn=test_pawn;rl_weapon=test_gun;rl_main_hand=right_hand;rl_deadline=5000;
 rl_shell_count=3;test_shell_loaded=4;test_shell_pending=1;g_reload_phase=4;held_left=test_grip;
 touch_reload_tick(test_hand,0,1000);assert(g_reload_phase==8&&rl_shell_count==4&&rl_deadline==13000&&!g_cmd_until[29]);
 touch_reload_tick(test_hand,0,1100);assert(g_reload_phase==8&&!force_on);
 held_left=0;touch_reload_tick(test_hand,0,1200);assert(g_reload_phase==2&&force_on==1&&!begin_calls);
 held_left=test_grip;touch_reload_tick(test_hand,0,1210);touch_reload_tick(test_hand,0,1380);
 assert(g_reload_phase==4&&begin_calls==1);
 test_reloading=0;test_shell_loaded=5;test_shell_pending=0;test_family_complete=1;
 touch_reload_tick(test_hand,0,1400);assert(g_reload_phase==7&&rl_shell_count==5);
 touch_reload_tick(right_hand,1,1410);assert(!g_reload_phase&&mock_current_ammo==0&&mock_reserves==20);
 fixture();rl_family=5;rl_pawn=test_pawn;rl_weapon=test_gun;rl_deadline=5000;rl_shell_count=4;
 test_shell_loaded=4;test_shell_pending=1;g_reload_phase=4;
 touch_reload_tick(test_hand,0,1000);assert(!g_reload_phase); // No endless repeat when insertion made no progress.
 fixture();rl_family=5;rl_pawn=test_pawn;rl_weapon=test_gun;rl_deadline=5000;g_reload_phase=8;g_touch_inventory_request=1;
 touch_reload_tick(test_hand,0,1000);assert(!g_reload_phase&&!force_on);
 puts("PASS: 100 rigid alignments, measured .606456 marker sizing preserved with unit hand, animated positive markers/native gates, scalar/cached ABI48 canaries, owned holster handoff, immutable bounded failures and cancellation ownership");
 puts("PASS: campaign shared pistol/shotgun AmmoHolster in both item orders, empty/partial magazine requests before native eject acknowledgement, exact first-eligible native grab, wait/no-wrong-pouch, full-name/context races and immutable ammo/Props");
 return 0;
}
