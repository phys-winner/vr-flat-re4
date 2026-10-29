#define _CRT_SECURE_NO_WARNINGS
#define main pump_fixture_unused_main
#define touch_equipped_gun pump_equipped_gun
#define test_refusals pump_test_refusals
#include "test_weapon_cycle.c"
#undef main
#undef touch_equipped_gun
#undef test_refusals
static void* (*ti_prop_class)(void),*(*rl_right)(const void*),*(*rl_get_pawn)(const void*);
static int tw_typed(const void* o,const void* c,int n){return c&&reload_layout(o,n)&&reload_is_a(o,c);}
static int tw_grab_paused(void){return rl_paused();}   // tutorial pause reasons: test_weapon.c
static void grenade_context_interleave(const volatile void* pointer);
// gr_context reads quickselect busy; qs_context is only an installer dependency here.
#define __atomic_load_n(P,O) ({ __auto_type test_p=(P); __auto_type test_value=__atomic_load_n(test_p,(O)); grenade_context_interleave(test_p); test_value; })
#include "../re4fix_quickselect.h"
#undef __atomic_load_n
static unsigned g_comp_w=2400,g_comp_h=1080;
static float g_fov_h_deg=90;
#include "../re4fix_weapon_pose.h"
static void grenade_interleave(const volatile void* pointer);
#define __atomic_store_n(P,V,O) ({ __auto_type test_p=(P); __atomic_store_n(test_p,(V),(O)); grenade_interleave(test_p); })
#define __atomic_compare_exchange_n(P,E,V,W,S,F) ({ __auto_type test_p=(P); int test_ok=__atomic_compare_exchange_n(test_p,(E),(V),(W),(S),(F)); grenade_interleave(test_p); test_ok; })
#include "../re4fix_grenade.h"
#undef __atomic_store_n
#undef __atomic_compare_exchange_n
#ifdef GRENADE_NATIVE_FIXTURE
#include GRENADE_NATIVE_FIXTURE
#endif
static void* grenade_table[0x800/8];
static int grenade_down,grenade_releases,throws,tracked_ok=1,body,auto_ok,mutate_ammo;
static float damp_speed,gesture_signal,previous_pose[3];static long native_last;
static const volatile void* reset_at;static int reset_repeat,reset_injected;
static int style_copies,style_sets,style_destroys,style_accept,style_cancel,style_rename;
static int reset_context,reset_track;
static void grenade_context_interleave(const volatile void* pointer){
 if(reset_context&&pointer==&qs_busy){reset_context=0;touch_grenade_input_reset();}
}
static void grenade_interleave(const volatile void* pointer){
 if(pointer!=reset_at||!reset_at)return;reset_at=0;reset_injected++;
 unsigned preserve=touch_grenade_input_reset();if(reset_repeat)preserve|=touch_grenade_input_reset();
 for(int bit=0;bit<32;bit++)if(!(preserve&(1u<<bit)))g_cmd_until[bit]=0;
}
static const void* grenade_settings(void){return f.settings;}
static void grenade_copy(void* copy,const void* source){assert(source==f.settings&&!((uintptr_t)copy&15));memcpy(copy,source,0xb8);style_copies++;}
static void grenade_destroy(void* copy){assert(copy!=f.settings);style_destroys++;}
static void grenade_set(const void* copy){assert(copy!=f.settings);unsigned char expected[0xb8];memcpy(expected,f.settings,sizeof expected);expected[0x40]=((const unsigned char*)copy)[0x40];assert(!memcmp(expected,copy,sizeof expected));
 style_sets++;if(style_accept)memcpy(f.settings,copy,sizeof expected);if(style_cancel)g_touch_cancel=1;if(style_rename&&style_sets==1)*(uint64_t*)(f.gun+0x18)^=1ULL<<40;}
static void* grenade_prop(const void* pawn,unsigned char slot){assert(pawn==f.pawn&&slot==2);return equipped;}
static void* grenade_hand(const void* pawn,unsigned char slot){assert(pawn==f.pawn&&slot==2);return f.right;}
static int grenade_context(const void* grenade){assert(grenade==f.gun);return 1;}
static void* owner_pawn(const void* grenade){assert(grenade==f.gun);return active_pawn;}
static int grenade_ammo(const void* grenade){assert(grenade==f.gun);if(mutate_ammo)g_touch_cancel=1;return *(int*)(f.gun+0xd90);}
static int grenade_interactivity(const void* grenade){assert(grenade==f.gun);return interactivity;}
static int grenade_body(const void* grenade){assert(grenade==f.gun);return body;}
static int grenade_auto(const void* grenade){assert(grenade==f.gun);return auto_ok;}
static void* grenade_pin(const void* grenade){assert(grenade==f.gun);return forest_hand;}
static int grenade_track(const void* grenade,const void* hand,float rollback,float time){assert(grenade==f.gun&&hand==f.right&&rollback==0&&time>=0);if(reset_track){reset_track=0;touch_grenade_input_reset();}return tracked_ok;}
static int grenade_is_down(const void* hand){assert(hand==f.right);return grenade_down;}
static void grenade_release(void* hand){assert(hand==f.right&&grenade_down);grenade_down=0;grenade_releases++;}
static void grenade_ready(int side){
 g_comp_w=2400;g_comp_h=1080;g_fov_h_deg=90;
 ready();memset(&gr_run,0,sizeof gr_run);memset(&gr_plan,0,sizeof gr_plan);memset(grenade_table,0,sizeof grenade_table);
 gr_ui=gr_request=0;gr_finger=gr_cancel_request=gr_busy_until=gr_logs=gr_owned_mask=gr_owned_seq=gr_owned_deadline=gr_freeze=gr_last_pose=gr_freeze_pose=0;gr_generation=1;qs_busy=0;qs_request=0;qs_ready=1;
 f.right[0x314]=side;f.left[0x314]=1-side;f.gun[0x2dc]=2;*(void**)f.gun=grenade_table;
 grenade_table[0x668/8]=grenade_interactivity;grenade_table[0x680/8]=grenade_body;
 *(float*)(f.gun+0x548)=.15f;*(float*)(f.gun+0x54c)=.04f;*(float*)(f.gun+0x550)=.025f;
 *(float*)(f.gun+0x580)=*(float*)(f.gun+0x584)=.1f;*(float*)(f.gun+0x588)=.9f;*(float*)(f.gun+0x58c)=.1f;*(float*)(f.gun+0x590)=200;
 *(unsigned short*)(f.gun+0x5ea)=1;f.gun[0x5ee]=2;f.settings[0x3d]=1;f.settings[0x40]=0;
 f.gun[0x5ef]=f.gun[0x5f1]=0;*(float*)(f.gun+0x600)=0;*(int*)(f.gun+0xd90)=2;
 qs_settings=grenade_settings;qs_prop=grenade_prop;qs_hand=grenade_hand;qs_context=grenade_context;ti_prop_class=gun_class;
 qs_copy=grenade_copy;qs_set=grenade_set;qs_destroy=grenade_destroy;style_copies=style_sets=style_destroys=style_cancel=style_rename=0;style_accept=1;
 rl_right=right;rl_get_pawn=owner_pawn;gr_class=gun_class;gr_auto_pin=grenade_auto;gr_body=grenade_body;gr_ammo=grenade_ammo;
 gr_interactivity=grenade_interactivity;gr_tracked=grenade_track;gr_pin_hand=grenade_pin;gr_installed=1;ti_down=grenade_is_down;ti_release=grenade_release;
 grenade_down=grenade_releases=throws=body=mutate_ammo=0;tracked_ok=auto_ok=1;forest_hand=0;damp_speed=gesture_signal=0;native_last=1000;
 gr_held_pose(side,previous_pose);
 reset_at=0;reset_repeat=reset_injected=reset_context=reset_track=0;
}
static void grenade_tick(long now,int offhand){fixture_clock=now;int ammo=*(int*)(f.gun+0xd90);void* hand=offhand?f.left:f.right;
 touch_grenade_tick(hand,*(unsigned char*)((char*)hand+0x314),now);assert(*(int*)(f.gun+0xd90)==ammo);}
// Independently transcribed original PreBio4Tick hold/damped-swing/default UP detector.
static void grenade_native(long now){float pose[3];gr_held_pose(f.right[0x314],pose);touch_grenade_pose(f.right[0x314],now,pose);
 float dt=(float)(now-native_last)*.001f;assert(dt>0&&dt<=.1f);native_last=now;float v[3],norm=0;
 for(int i=0;i<3;i++){v[i]=(pose[i]-previous_pose[i])*100/dt;previous_pose[i]=pose[i];norm+=v[i]*v[i];}
 int bit=f.right[0x314]?27:29;if(g_cmd_until[bit]>now)grenade_down=1;else grenade_down=0;
 float speed=sqrtf(norm),damping=*(float*)(f.gun+0x550);damp_speed+=(speed-damp_speed)*(damping>0?1-powf(.01f,dt/damping):1);
 float target=damp_speed>*(float*)(f.gun+0x590)?1:0,tau=*(float*)(f.gun+(target?0x580:0x584));gesture_signal+=(target-gesture_signal)*(tau>0?1-powf(.01f,dt/tau):1);
 int flick=f.gun[0x5ef]?gesture_signal>*(float*)(f.gun+0x58c):gesture_signal>=*(float*)(f.gun+0x588);f.gun[0x5ef]=flick;
 float hold=grenade_down?*(float*)(f.gun+0x600)+dt:0;int satisfied=hold>*(float*)(f.gun+0x548);
 if(!body&&!satisfied&&f.gun[0x5f1]&&flick&&f.gun[0x5ee]==2&&f.settings[0x40]==0&&tracked_ok){throws++;(*(int*)(f.gun+0xd90))--;f.gun[0x5ee]=*(int*)(f.gun+0xd90)?2:0;gesture_signal=damp_speed=0;f.gun[0x5ef]=0;}
 *(float*)(f.gun+0x600)=hold;f.gun[0x5f1]=satisfied;
}
static void grenade_begin(int side){grenade_ready(side);grenade_tick(1000,0);assert(touch_grenade_fire_side(1000)==side);assert(touch_grenade_fire_down(1001));
 grenade_tick(1002,1);assert(gr_request);grenade_tick(1002,0);assert(gr_run.phase==1&&g_cmd_until[side?27:29]==2802);assert(!g_cmd_until[26]&&!g_cmd_until[28]);}
static void test_gesture(void){for(int side=0;side<2;side++){
 grenade_begin(side);assert(!touch_grenade_fire_down(1003));touch_grenade_fire_up(1004);assert(!touch_grenade_fire_down(1005));
 for(long now=1016;now<4000;now+=16){grenade_native(now);grenade_tick(now,0);assert(throws<=1);}
 assert(throws==1&&grenade_releases==1&&!gr_run.phase&&!gr_busy_until&&!grenade_down&&*(int*)(f.gun+0xd90)==1);
 grenade_tick(4010,0);assert(touch_grenade_fire_side(4010)==side);assert(touch_grenade_fire_down(4011));
 grenade_ready(side);grenade_tick(1000,0);assert(touch_grenade_fire_down(1001));grenade_tick(1002,0);
 for(long now=1016;now<4000;now+=16){grenade_native(now);grenade_tick(now,0);}grenade_tick(4010,0);assert(!touch_grenade_fire_down(4011));touch_grenade_fire_up(4012);assert(touch_grenade_fire_down(4013));
 }}
static void test_refusals(void){
 for(int side=0;side<2;side++){
  grenade_ready(side);f.settings[0x40]=2;grenade_tick(1000,0);assert(touch_grenade_fire_side(1000)<0&&!touch_grenade_fire_down(1001));
  grenade_ready(side);f.gun[0x5ee]=3;grenade_tick(1000,0);assert(!touch_grenade_fire_down(1001));
  grenade_ready(side);grenade_tick(1000,0);assert(!touch_grenade_fire_down(1251));
  grenade_ready(side);grenade_tick(1000,0);assert(touch_grenade_fire_down(1001));g_cmd_until[side?27:29]=9999;grenade_tick(1002,0);assert(!gr_run.phase&&g_cmd_until[side?27:29]==9999);
  grenade_begin(side);g_cmd_until[side?27:29]=9999;grenade_tick(1020,0);assert(!gr_run.phase&&!grenade_releases&&g_cmd_until[side?27:29]==9999);
  grenade_begin(side);grenade_tick(999,0);assert(gr_run.phase==6&&!throws);grenade_tick(1015,0);assert(!gr_run.phase&&!throws);
  grenade_begin(side);tracked_ok=0;for(long now=1016;now<3000;now+=16){grenade_native(now);grenade_tick(now,0);}assert(!throws&&!gr_run.phase);
 }
 for(int i=0;i<6;i++){grenade_begin(1);*(uint64_t*)((char*)gr_run.object[i]+0x18)^=1ULL<<40;grenade_tick(1020,0);assert(!gr_run.phase&&!throws);}
 grenade_ready(1);grenade_tick(1000,0);assert(touch_grenade_fire_down(1001));mutate_ammo=1;grenade_tick(1002,0);assert(!gr_run.phase&&!g_cmd_until[27]);
 for(int i=0;i<6;i++){
  grenade_ready(1);grenade_tick(1000,0);assert(touch_grenade_fire_down(1001));void* b[6];assert(gr_binding(f.pawn,b));*(uint64_t*)((char*)b[i]+0x18)^=1ULL<<40;
  grenade_tick(1002,0);assert(!gr_run.phase&&!g_cmd_until[27]);
 }
 grenade_ready(1);grenade_tick(1000,0);assert(touch_grenade_fire_down(1001));f.right[0x314]=0;f.left[0x314]=1;grenade_tick(1002,0);assert(!gr_run.phase&&!g_cmd_until[29]);
 const int bad_offsets[]={0x548,0x54c,0x550,0x580,0x584,0x588,0x58c,0x590};
 for(int i=0;i<8;i++){grenade_ready(1);*(float*)(f.gun+bad_offsets[i])=NAN;grenade_tick(1000,0);assert(!touch_grenade_fire_down(1001));grenade_tick(1002,0);assert(!gr_run.phase&&!g_cmd_until[27]);}
 grenade_begin(1);fixture_tid=42;grenade_tick(1020,0);assert(gr_run.phase);touch_grenade_cancel();assert(gr_run.phase);fixture_tid=41;grenade_tick(1021,0);assert(gr_run.phase==6);grenade_tick(1037,0);assert(!gr_run.phase);
}
static void test_pose_and_cancel(void){
 grenade_begin(1);float p[3]={9,8,7};assert(!touch_grenade_pose(0,1002,p)&&p[0]==9);assert(!touch_grenade_pose(1,1002,p));
 for(long now=1016;now<1250;now+=16){grenade_native(now);grenade_tick(now,0);}assert(gr_run.phase==2);
 assert(touch_grenade_pose(1,1250,p)&&p[0]>.0f&&p[2]<-.30f);assert(!touch_grenade_pose(1,1700,p));
 grenade_begin(1);grenade_native(1016);touch_grenade_cancel();assert(gr_run.phase&&g_cmd_until[27]==LONG_MAX);touch_grenade_cancel_tick(f.right,1,1017);
 grenade_native(1032);touch_grenade_cancel_tick(f.right,1,1033);assert(!gr_run.phase&&!g_cmd_until[27]&&grenade_releases==1);grenade_native(1048);assert(!throws);
 grenade_begin(0);grenade_native(1016);g_grip_latched=1u<<29;touch_grenade_cancel();touch_grenade_cancel_tick(f.right,0,1017);touch_grenade_cancel_tick(f.right,0,1033);assert(!grenade_releases&&grenade_down);
 for(int side=0;side<2;side++){
  grenade_begin(side);long now;for(now=1016;now<1500;now+=16){grenade_native(now);grenade_tick(now,0);if(f.gun[0x5ef]&&gr_run.phase==2)break;}
  assert(f.gun[0x5ef]&&gr_run.phase==2&&!throws);g_cmd_until[26]=9999;unsigned mask=touch_grenade_input_reset();assert(mask==(1u<<(side?27:29))&&touch_grenade_owned_grip_mask()==mask);
  for(int bit=0;bit<32;bit++)if(!(mask&(1u<<bit)))g_cmd_until[bit]=0;
  assert(!g_cmd_until[26]&&g_cmd_until[side?27:29]==LONG_MAX);
  for(now+=16;now<2200&&gr_run.phase;now+=16){touch_grenade_cancel_tick(f.right,side,now);grenade_native(now);grenade_tick(now,0);assert(!throws);}
  assert(!gr_run.phase&&!g_cmd_until[side?27:29]&&grenade_releases==1&&!grenade_down&&!touch_grenade_owned_grip_mask());
  grenade_begin(side);g_cmd_until[side?27:29]=9999;assert(!touch_grenade_input_reset());touch_grenade_cancel_tick(f.right,side,1020);touch_grenade_cancel_tick(f.right,side,1036);assert(g_cmd_until[side?27:29]==9999&&!grenade_releases);
  grenade_begin(side);assert(touch_grenade_input_reset());active_pawn=f.inactive;touch_grenade_cancel_tick(f.right,side,1020);assert(!gr_run.phase&&!g_cmd_until[side?27:29]&&!grenade_releases);
  grenade_begin(side);assert(touch_grenade_input_reset());paused=1;touch_grenade_cancel_tick(f.right,side,1020);touch_grenade_cancel_tick(f.right,side,8000);assert(gr_run.phase==6&&g_cmd_until[side?27:29]==LONG_MAX);
  paused=0;touch_grenade_cancel_tick(f.right,side,8016);assert(gr_run.phase==6&&g_cmd_until[side?27:29]==LONG_MAX);touch_grenade_cancel_tick(f.right,side,8032);assert(!gr_run.phase&&!g_cmd_until[side?27:29]);
  grenade_begin(side);for(now=1016;now<1500;now+=16){grenade_native(now);grenade_tick(now,0);if(f.gun[0x5ef]&&gr_run.phase==2)break;}
  float actual[3],resumed[3];memcpy(actual,previous_pose,12);assert(touch_grenade_input_reset());paused=1;
  assert(touch_grenade_pose(side,8000,resumed)&&!memcmp(actual,resumed,12));
  touch_grenade_cancel_tick(f.right,side,8001);paused=0;touch_grenade_cancel_tick(f.right,side,8016);
  f.gun[0x5ef]=1;touch_grenade_cancel_tick(f.right,side,8616);assert(gr_run.phase==6&&g_cmd_until[side?27:29]==LONG_MAX&&!grenade_releases&&touch_grenade_busy(8616));
  f.gun[0x5ef]=0;touch_grenade_cancel_tick(f.right,side,8632);assert(!gr_run.phase&&!g_cmd_until[side?27:29]&&grenade_releases==1);
 }
}
static void test_atomic_interleavings(void){for(int side=0;side<2;side++)for(int step=0;step<4;step++)for(int repeat=0;repeat<2;repeat++){
 grenade_ready(side);grenade_tick(1000,0);assert(touch_grenade_fire_down(1001));
 const volatile void* target[]={&gr_owned_seq,&gr_owned_deadline,&gr_owned_mask,&g_cmd_until[side?27:29]};reset_at=target[step];reset_repeat=repeat;
 grenade_tick(1002,0);assert(reset_injected==1&&!throws);
 for(long now=1016;now<1300;now+=16){touch_grenade_cancel_tick(f.right,side,now);grenade_native(now);grenade_tick(now,0);assert(!throws);}
 assert(!gr_run.phase&&!g_cmd_until[side?27:29]&&!touch_grenade_owned_grip_mask());
 }
 for(int side=0;side<2;side++)for(int step=0;step<3;step++){
  grenade_begin(side);grenade_native(1016);touch_grenade_cancel();touch_grenade_cancel_tick(f.right,side,1017);
  const volatile void* target[]={&gr_owned_seq,&gr_owned_mask,&gr_owned_deadline};reset_at=target[step];reset_repeat=1;
  touch_grenade_cancel_tick(f.right,side,1033);assert(reset_injected==1&&!gr_run.phase&&!g_cmd_until[side?27:29]&&!throws);
 }
 for(int side=0;side<2;side++){
  grenade_begin(side);long now;for(now=1016;now<1700;now+=16){grenade_native(now);if(gr_run.phase==2&&now>=gr_run.move_start+gr_run.move_ms)break;grenade_tick(now,0);}
  assert(gr_run.phase==2&&f.gun[0x5ef]);reset_at=&g_cmd_until[side?27:29];reset_repeat=1;grenade_tick(now,0);assert(reset_injected==1&&!grenade_releases);
  for(now+=16;now<2300&&gr_run.phase;now+=16){touch_grenade_cancel_tick(f.right,side,now);grenade_native(now);grenade_tick(now,0);assert(!throws);}
  assert(!gr_run.phase&&!g_cmd_until[side?27:29]&&grenade_releases==1);
 }
 for(int side=0;side<2;side++)for(int branch=0;branch<2;branch++){
  grenade_begin(side);long now;for(now=1016;now<1700;now+=16){grenade_native(now);if(gr_run.phase==2&&now>=gr_run.move_start+gr_run.move_ms)break;grenade_tick(now,0);}
  assert(gr_run.phase==2&&f.gun[0x5ef]);if(branch==0)reset_context=1;else reset_track=1;
  grenade_tick(now,0);assert(!reset_context&&!reset_track);assert(gr_run.phase==6&&g_cmd_until[side?27:29]==LONG_MAX&&!grenade_releases);
  f.gun[0x5ef]=1;touch_grenade_cancel_tick(f.right,side,now+600);assert(gr_run.phase==6&&g_cmd_until[side?27:29]==LONG_MAX&&!grenade_releases&&!throws);
  f.gun[0x5ef]=0;touch_grenade_cancel_tick(f.right,side,now+616);assert(!gr_run.phase&&!g_cmd_until[side?27:29]&&grenade_releases==1&&!throws);
 }
}
static void test_style_transaction(void){for(int side=0;side<2;side++){
 grenade_ready(side);f.settings[0x40]=1;unsigned char before[0xb8];memcpy(before,f.settings,sizeof before);grenade_tick(1000,0);
 assert(!memcmp(before,f.settings,sizeof before)&&!style_copies&&!style_sets&&!style_destroys&&touch_grenade_fire_side(1000)==side);
 assert(touch_grenade_fire_down(1001));grenade_tick(1002,0);before[0x40]=0;
 assert(gr_run.phase==1&&!memcmp(before,f.settings,sizeof before)&&style_copies==1&&style_sets==1&&style_destroys==1);
 for(long now=1016;now<4000;now+=16){grenade_native(now);grenade_tick(now,0);}assert(throws==1&&!gr_run.phase);
 for(int failure=0;failure<3;failure++){
  grenade_ready(side);f.settings[0x40]=1;memcpy(before,f.settings,sizeof before);grenade_tick(1000,0);assert(touch_grenade_fire_down(1001));
  if(failure==0)style_accept=0;if(failure==1)style_cancel=1;if(failure==2)style_rename=1;
  grenade_tick(1002,0);assert(!gr_run.phase&&!g_cmd_until[side?27:29]&&!memcmp(before,f.settings,sizeof before));assert(style_copies==1&&style_sets==2&&style_destroys==1);
 }
 grenade_ready(side);f.settings[0x40]=1;grenade_tick(1000,0);assert(touch_grenade_fire_down(1001));qs_copy=0;grenade_tick(1002,0);assert(!gr_run.phase&&!style_sets&&!style_destroys&&f.settings[0x40]==1);
 }
}
static void test_held_framing(void) {
 const float model_delta[3]={-.012100123f,-.035f,-.005001193f};
 float old_model[3]={.18f+model_delta[0],-.20f+model_delta[1],-.30f+model_delta[2]};
 assert(-old_model[1]>(-old_model[2])/(2400.0f/1080)); // Exact p377 model origin falls below horizontal-FOV90 phone view.
 for(int side=0;side<2;side++)for(int fov=60;fov<=120;fov+=5)for(int shape=0;shape<=8;shape++) {
  float aspect=1+.25f*shape,off[3];grenade_held_pose(side,(float)fov,aspect,off);
  float tan_h=tanf(fov*.00872664626f),model[3];for(int i=0;i<3;i++)model[i]=off[i]+model_delta[i];
  // An8cm cube around the observed unchanged native origin fits in the view, even at its near corners.
  float near=-model[2]-.04f;
  assert(near>0&&fabsf(model[0])+.04f<near*tan_h&&fabsf(model[1])+.04f<near*tan_h/aspect);
  assert(fabsf(off[0])<=.18f&&off[2]==-.45f);
 }
 for(int side=0;side<2;side++) {
  grenade_ready(side);float base[3],actual[3];gr_held_pose(side,base);
  gr_publish_plan(1000,1000,160,1260,3260,.5f,side,1);
  assert(touch_grenade_pose(side,1000,actual)&&!memcmp(base,actual,sizeof base));
  assert(touch_grenade_pose(side,1080,actual)&&actual[0]==base[0]&&actual[1]==base[1]&&fabsf(actual[2]-(base[2]-.25f))<.000001f);
  gr_freeze=1;gr_freeze_pose=1080;float frozen[3];memcpy(frozen,actual,sizeof frozen);
  g_fov_h_deg=60;g_comp_w=3000;g_comp_h=1000;
  assert(touch_grenade_pose(side,1400,actual)&&!memcmp(actual,frozen,sizeof frozen));
 }
 float invalid[3]={9,8,7},before[3];memcpy(before,invalid,sizeof before);grenade_held_pose(2,90,2,invalid);assert(!memcmp(invalid,before,sizeof before));
 grenade_held_pose(1,NAN,NAN,invalid);for(int i=0;i<3;i++)assert(isfinite(invalid[i]));
 puts("PASS: p377 invisible native grenade origin reproduced; unchanged native model plus8cm framing envelope fits both hands FOV60..120/aspect1..3; throw starts at identical held pose, original travel velocity, frozen FOV changes preserve pose");
}
int main(void){
#ifdef GRENADE_NATIVE_FIXTURE
 verify_grenade_install_guard();
#endif
 test_held_framing();test_gesture();test_refusals();test_pose_and_cancel();test_atomic_interleavings();test_style_transaction();puts("PASS: actual grenade helper both hands one native hold/flick/UP, explicit Manual-to-Default settings transaction/rollback, original-only ammo consumption, frozen resume/mid-flick cancel/parked signal/reset publication interleavings and foreign/names/clock guards");return 0;
}
