#define main pump_fixture_unused_main
#define reload_grip_registered pump_fixture_grip_registered
#include "test_weapon_cycle.c"
#undef reload_grip_registered
#undef main

#define rl_hand_class tw_hand_class
#define rl_pawn_class tw_pawn_class
#define rl_grip_class tw_grip_class
#define rl_player phone_player
static void* (*rl_ammo_class)(void),*(*rl_get_pawn)(const void*),*(*rl_active)(void);
static int (*rl_type)(void),(*rl_base_ready)(const void*),(*rl_trigger)(const void*),(*rl_held_ammo_ready)(const void*,float);
static int user_fire,native_trigger,base_ready=1;static void* ammo_primary;
static int touch_reload_user_fire(long now){(void)now;return user_fire;}
static int reload_grip_registered(void* grip);
static void* reload_ammo_primary(void* gun,void* pawn);
static int reload_marker_valid(const struct reload_tf* tf){return reload_tf_valid(tf);}
#include "../re4fix_reload_family.h"
#ifdef FAMILY_NATIVE_FIXTURE
#include FAMILY_NATIVE_FIXTURE
#endif
static struct {
 unsigned char cls[7][0x100],ammo[0x620],ammo_grip[0x460],mesh[0x800];void* chain[7][1];void* gun_table[0x800/8];
} x __attribute__((aligned(16)));
static int fixture_capacity,fixture_reserve;
static int family_capacity(const void* gun){assert(gun==f.gun);return fixture_capacity;}
static int family_reserve(const void* gun){assert(gun==f.gun);return fixture_reserve;}
static int family_kind,ready_override,slide_model,close_model,pulse_edges,physical_shots,count_calls,cancel_count,rename_count;
static int ready_calls,mutate_ready_at,mutate_ready_offset;static long native_ready_at;
static int release_timer,release_no_up;
static int initial_type(void){return g_touch_context;}
static void* family_class0(void){return x.cls[0];}static void* family_class1(void){return x.cls[1];}
static void* family_class2(void){return x.cls[2];}static void* family_class3(void){return x.cls[3];}
static void* family_class4(void){return x.cls[4];}static void* family_class5(void){return x.cls[5];}
static void* ammo_class(void){return x.cls[6];}static void* mesh_class(void){return f.classes[4];}
static void* native_owner(const void* item){return item==f.gun||item==x.ammo?f.pawn:0;}
static void* native_primary(const void* item){return item==f.gun?f.right:item==x.ammo?ammo_primary:0;}
static void* reload_ammo_primary(void* gun,void* pawn){return gun==f.gun&&pawn==f.pawn?x.ammo_grip:0;}
static int reload_grip_registered(void* grip){return grip==x.ammo_grip||pump_fixture_grip_registered(grip);}
static int actual_ready(const void* gun){assert(gun==f.gun);ready_calls++;
 if(ready_calls==mutate_ready_at){if(mutate_ready_offset==0x11dd)f.gun[mutate_ready_offset]=0;else f.gun[mutate_ready_offset]=1;}
 if(ready_override>=0)return ready_override;if(!base_ready)return 0;
 if(family_kind==1)return f.gun[0x10f0]!=0;if(family_kind==2)return f.gun[0x11c8]==0;
 if(family_kind==3)return f.gun[0x111c]!=0;if(family_kind==4)return !f.gun[0xe40]&&*(float*)(f.gun+0xe3c)==0;
 return !f.gun[0x1160]&&!f.gun[0x1161];}
static int count_ammo(const void* gun,int included){assert(gun==f.gun&&!included);count_calls++;if(cancel_count&&count_calls>=cancel_count)g_touch_cancel=1;
 if(rename_count&&count_calls>=rename_count)*(uint64_t*)(f.gun+0x18)^=1ULL<<40;return *(int*)(f.gun+0xd90);}
static int is_base_ready(const void* gun){assert(gun==f.gun);return base_ready;}static int is_trigger(const void* gun){assert(gun==f.gun);return native_trigger;}
static void* slide(const void* gun){assert(gun==f.gun);return forest_hand;}static void* grip_hand(const void* grip){assert(grip==f.forest);return forest_hand;}
static const char* get_settings(void){return (const char*)f.settings;}
static struct rf_vec get_grab(const void* h){struct wc_vec v=grab_location(h);return (struct rf_vec){v.x,v.y,v.z};}
static struct reload_tf socket_tf(const void* mesh,unsigned long long name,int space){assert(mesh==x.mesh&&space==3&&name>=1&&name<=3);
 struct reload_tf t={{0,0,0,1},{name==1?30:name==2?25:20,0,0},{1,1,1},{0,0}};return t;}
static void family_world(void* root,const struct reload_tf* pose,int sweep,void* hit,int teleport){assert((root==f.roots[0]||root==f.roots[2])&&!sweep&&!hit&&!teleport&&reload_tf_valid(pose));
 world_calls++;if(root==f.roots[2]){qf_t old=tf(root)->q,inv={-old.x,-old.y,-old.z,old.w},delta=q_mul(pose->q,inv);tf(f.roots[1])->q=q_mul(delta,tf(f.roots[1])->q);tf(x.mesh)->q=q_mul(delta,tf(x.mesh)->q);}*tf(root)=*pose;}
static void family_press(void* hand){assert(hand==f.left&&!left_grip&&!down);press_calls++;down=1;if(!accept)return;left_grip=f.forest;forest_hand=f.left;
 struct reload_tf h=*tf(f.roots[0]),m=*tf(x.mesh);float d[3],local[3];for(int i=0;i<3;i++)d[i]=h.p[i]-m.p[i];qf_t inv={-m.q.x,-m.q.y,-m.q.z,m.q.w};q_rot(inv,d,local);memcpy(f.gun+0x112c,local,12);}
static void family_release(void* hand){if(release_no_up){release_calls++;return;}release(hand);if(release_timer)base_ready=0;}
static void family_ready(int kind,int primary_side){
 ready();memset(&x,0,sizeof x);memset(&rf_post,0,sizeof rf_post);family_kind=kind;
 f.right[0x314]=(unsigned char)primary_side;f.left[0x314]=(unsigned char)(1-primary_side);g_reload_phase=7;
 for(int i=0;i<7;i++){*(int*)(x.cls[i]+0x58)=i==6?0x620:0x1300;x.chain[i][0]=x.cls[i]+0x30;*(void**)(x.cls[i]+0x30)=x.chain[i];}
 *(void**)(f.gun+0x10)=x.cls[kind-1];*(void**)f.gun=x.gun_table;x.gun_table[0x778/8]=actual_ready;
 *(void**)(f.gun+0x8e0)=x.ammo;*(void**)(x.ammo+0x10)=x.cls[6];*(void**)(x.ammo+0x578)=f.gun;*(void**)(x.ammo+0x270)=x.ammo_grip;*(uint64_t*)(x.ammo+0x18)=99;
 *(void**)(x.ammo_grip+0x10)=f.classes[2];*(void**)(x.ammo_grip+0x20)=*(void**)(x.ammo_grip+0x300)=x.ammo;*(uint64_t*)(x.ammo_grip+0x18)=100;
 *(void**)(f.gun+0x540)=x.mesh;*(void**)(x.mesh+0x10)=f.classes[4];*(int*)(f.classes[4]+0x58)=0x800;
 *(void**)(x.mesh+0x20)=f.gun;*(uint64_t*)(x.mesh+0x18)=101;*tf(x.mesh)=(struct reload_tf){{0,0,0,1},{0,0,0},{1,1,1},{0,0}};
 if(kind<5)*(float*)(f.gun+0xd38)=45.0f;*(float*)(f.gun+0xd60)=30.0f;f.gun[0xd2c]=1;f.gun[0x10f0]=0;f.gun[0x11c8]=2;f.gun[0x11dd]=1;f.gun[0x10e8]=1;f.gun[0x111c]=0;f.gun[0xe40]=1;
 *(float*)(f.gun+0xda0)=1;*(void**)(f.gun+0x10c8)=f.forest;
 if(kind==3){*(uint64_t*)(f.gun+0x10d8)=1;*(uint64_t*)(f.gun+0x10e0)=2;*(uint64_t*)(f.gun+0x10e8)=3;}
 rf_classes[0]=family_class0;rf_classes[1]=family_class1;rf_classes[2]=family_class2;rf_classes[3]=family_class3;rf_classes[4]=family_class4;rf_classes[5]=family_class5;
 for(int i=0;i<6;i++)rf_ready[i]=actual_ready;rf_scene_class=scene_class;rf_mesh_class=mesh_class;rf_right=right;
 rf_count=count_ammo;rf_in_valid=in_valid;rf_recoil=is_recoil;rf_blend=is_blend;rf_flourish=is_flourish;rf_ammo_held=held_ammo;
 for(int i=0;i<3;i++)rf_slide_hand[i]=slide;rf_grip_hand=grip_hand;rf_socket=socket_tf;rf_grip_target=target;rf_settings=get_settings;rf_grab_location=get_grab;
 rl_ammo_class=ammo_class;rl_get_pawn=native_owner;rl_active=active;rl_primary=native_primary;rl_type=initial_type;rl_base_ready=is_base_ready;rl_trigger=is_trigger;rl_held_ammo_ready=held_ammo;
 rl_world_transform=family_world;ti_press=family_press;ti_release=family_release;user_fire=native_trigger=0;base_ready=1;ready_override=-1;
 slide_model=close_model=1;pulse_edges=physical_shots=count_calls=cancel_count=rename_count=0;rf_logs=0;
 ammo_primary=0;ready_calls=mutate_ready_at=mutate_ready_offset=release_timer=release_no_up=0;native_ready_at=0;
}
static void test_shell_repeat(void){
 for(int kind=5;kind<=6;kind++)for(int side=0;side<2;side++) {
  family_ready(kind,side);rf_capacity=family_capacity;rf_reserve=family_reserve;
  x.gun_table[0x7b8/8]=rf_count;x.gun_table[0x7c8/8]=rf_capacity;x.gun_table[0x7d8/8]=rf_reserve;
  int loaded=-1;fixture_capacity=10;fixture_reserve=3;
  assert(reload_family_shells_pending(f.gun,&loaded)==1&&loaded==7);
  for(int count=8;count<=10;count++){*(int*)(f.gun+0xd90)=count;fixture_reserve=10-count;
   assert(reload_family_shells_pending(f.gun,&loaded)==(count<10)&&loaded==count);}
  *(int*)(f.gun+0xd90)=7;fixture_reserve=0;assert(!reload_family_shells_pending(f.gun,&loaded));
  fixture_reserve=-1;assert(reload_family_shells_pending(f.gun,&loaded)==-1);
  fixture_reserve=3;fixture_capacity=0;assert(reload_family_shells_pending(f.gun,&loaded)==-1);
  fixture_capacity=10;x.gun_table[0x7c8/8]=0;assert(reload_family_shells_pending(f.gun,&loaded)==-1);
 }
}
static int shell_callback_calls,shell_callback_interrupt,shell_callback_override;
static void native_shell_commit(void* gun){
 assert(gun==f.gun);shell_callback_calls++;
 int enabled_offset=family_kind==5?0x1130:0x10c5,amount_offset=family_kind==5?0x1134:0x10c8;
 if(shell_callback_override)assert(f.gun[enabled_offset]==1&&*(int*)(f.gun+amount_offset)==1);
 int count=*(int*)(f.gun+0xd90),amount=f.gun[enabled_offset]?*(int*)(f.gun+amount_offset):fixture_capacity-count;
 amount=amount<fixture_reserve?amount:fixture_reserve;amount=amount<fixture_capacity-count?amount:fixture_capacity-count;
 *(int*)(f.gun+0xd90)=count+amount;fixture_reserve-=amount;
 if(shell_callback_interrupt==1)g_touch_cancel=1;
 if(shell_callback_interrupt==2)g_touch_context=2;
 if(shell_callback_interrupt==3)g_touch_weapon_request=1;
}
static void test_single_shell_commit(void){
 for(int kind=5;kind<=6;kind++)for(int side=0;side<2;side++)for(int interrupt=0;interrupt<4;interrupt++){
  family_ready(kind,side);rf_single_shell_ready=1;rf_shell_logs=0;
  int enabled_offset=kind==5?0x1130:0x10c5,amount_offset=kind==5?0x1134:0x10c8;
  f.gun[enabled_offset]=0;*(int*)(f.gun+amount_offset)=3;fixture_capacity=10;fixture_reserve=3;
  shell_callback_calls=0;shell_callback_interrupt=interrupt;shell_callback_override=1;
  for(int loaded=8;loaded<=10;loaded++){
   rf_single_shell_reload(f.gun,kind,native_shell_commit);
   assert(*(int*)(f.gun+0xd90)==loaded&&fixture_reserve==10-loaded&&shell_callback_calls==loaded-7);
   assert(f.gun[enabled_offset]==0&&*(int*)(f.gun+amount_offset)==3);
  }
  f.gun[enabled_offset]=1;*(int*)(f.gun+amount_offset)=5;*(int*)(f.gun+0xd90)=7;fixture_reserve=3;
  rf_single_shell_reload(f.gun,kind,native_shell_commit);
  assert(*(int*)(f.gun+0xd90)==8&&fixture_reserve==2&&f.gun[enabled_offset]==1&&*(int*)(f.gun+amount_offset)==5);
 }
 for(int guard=0;guard<4;guard++){
  family_ready(5,1);rf_single_shell_ready=1;f.gun[0x1130]=0;*(int*)(f.gun+0x1134)=3;
  fixture_capacity=10;fixture_reserve=3;shell_callback_calls=shell_callback_interrupt=shell_callback_override=0;
  if(guard==0)rf_single_shell_ready=0;
  if(guard==1)fixture_tid=7;
  if(guard==2)*(void**)(f.gun+0x3c0)=0;
  rf_single_shell_reload(f.gun,guard==3?6:5,native_shell_commit);
  assert(shell_callback_calls==1&&*(int*)(f.gun+0xd90)==10&&!fixture_reserve);
  assert(!f.gun[0x1130]&&*(int*)(f.gun+0x1134)==3);
 }
 puts("PASS: each native Shotgun/Striker animation commit transfers exactly one reserved shell for both hands; authoring fields restored after cancellation, pause, weapon switch and enabled partial mode; rejected guards still call original once");
}
static int family_tick(void* hand,long now){fixture_clock=now;int result=reload_family_post_tick(hand,*(unsigned char*)((char*)hand+0x314),now);assert(*(int*)(f.gun+0xd90)==7);return result;}
static void family_native_frame(long now){fixture_clock=now;int bit=f.right[0x314]?26:28,held=g_cmd_until[bit]>now;
 if(held&&!native_trigger){pulse_edges++;if(family_kind==1)f.gun[0x10f0]=1;if(family_kind==2){f.gun[0x11dd]=0;if(!native_ready_at)f.gun[0x11c8]=0;}}
 if(native_ready_at&&now>=native_ready_at){base_ready=1;if(family_kind==2)f.gun[0x11c8]=0;}
 native_trigger=held;f.right[0x33c]=(unsigned char)held;
 if(family_kind==3&&left_grip==f.forest&&slide_model){struct reload_tf h=*tf(f.roots[0]),m=*tf(x.mesh);float d[3],local[3];for(int i=0;i<3;i++)d[i]=h.p[i]-m.p[i];qf_t inv={-m.q.x,-m.q.y,-m.q.z,m.q.w};q_rot(inv,d,local);
  if(local[0]-*(float*)(f.gun+0x112c)+25<=20)f.gun[0x111c]=1;}
 if(family_kind==4&&f.gun[0xe40]&&close_model){float p[3],world[3];memcpy(p,f.gun+0xda0,12);q_rot(tf(f.roots[1])->q,p,world);if(world[2]<0){f.gun[0xe40]=0;*(float*)(f.gun+0xe3c)=.05f;}}
 if(family_kind==4&&*(float*)(f.gun+0xe3c)>0){*(float*)(f.gun+0xe3c)=fmaxf(0,*(float*)(f.gun+0xe3c)-.01f);}
}
static void test_metadata(void){for(int k=1;k<=6;k++)for(int side=0;side<2;side++){family_ready(k,side);int angle=-1;assert(reload_family_info(f.gun,&angle)==(k==6?5:k)&&angle==(k==4?0xd38:0xd60));
 unsigned char snapshot[sizeof f.gun];memcpy(snapshot,f.gun,sizeof snapshot);assert(reload_family_info(f.gun,&angle));assert(!memcmp(snapshot,f.gun,sizeof snapshot));
 *(int*)(x.cls[k-1]+0x58)=0xd00;assert(!reload_family_info(f.gun,&angle)&&!angle);}
 family_ready(4,1);*(float*)(f.gun+0xda0)=NAN;int angle;assert(!reload_family_info(f.gun,&angle));
 family_ready(1,1);x.gun_table[0x778/8]=0;assert(!reload_family_info(f.gun,&angle));
 for(int k=1;k<=6;k++){family_ready(k,1);int offset=k==4?0xd38:0xd60;
  *(float*)(f.gun+offset)=180;assert(reload_family_info(f.gun,&angle));
  *(float*)(f.gun+offset)=181;assert(!reload_family_info(f.gun,&angle));
  *(float*)(f.gun+offset)=NAN;assert(!reload_family_info(f.gun,&angle));}
}
static void test_edges(void){for(int kind=1;kind<=2;kind++)for(int side=0;side<2;side++){
 family_ready(kind,side);assert(reload_family_post_begin(f.right,f.gun,1000));assert(family_tick(f.right,1000)==1);
 assert(family_tick(f.left,1100)==1);assert(family_tick(f.right,1150)==1);assert(rf_post.pulse&&g_cmd_until[side?26:28]==1300);
 family_native_frame(1160);assert(actual_ready(f.gun)&&pulse_edges==1);assert(family_tick(f.right,1160)==1&&!rf_post.pulse&&!g_cmd_until[side?26:28]);
 family_native_frame(1170);assert(family_tick(f.right,1170)==2&&!rf_post.kind&&pulse_edges==1&&!physical_shots);
 family_ready(kind,side);ready_override=1;assert(reload_family_post_begin(f.right,f.gun,1000));assert(family_tick(f.right,1000)==2&&!g_cmd_until[side?26:28]);
 }
 family_ready(1,1);assert(!reload_family_post_begin(f.left,f.gun,1000));assert(!rf_post.kind);
 family_ready(1,1);g_reload_phase=0;assert(!reload_family_post_begin(f.right,f.gun,1000));
 family_ready(1,1);assert(reload_family_post_begin(f.right,f.gun,1000));family_tick(f.right,1000);family_tick(f.right,1150);g_cmd_until[26]=9999;g_touch_cancel=1;assert(family_tick(f.right,1160)==3&&g_cmd_until[26]==9999);
 family_ready(1,1);assert(reload_family_post_begin(f.right,f.gun,1000));family_tick(f.right,1000);user_fire=1;assert(family_tick(f.right,1150)==3&&!g_cmd_until[26]);
 family_ready(1,1);assert(reload_family_post_begin(f.right,f.gun,1000));family_tick(f.right,1000);family_tick(f.right,1150);assert(family_tick(f.right,1300)==3&&!g_cmd_until[26]);
 for(int kind=1;kind<=2;kind++)for(int side=0;side<2;side++){
  family_ready(kind,side);assert(reload_family_post_begin(f.right,f.gun,1000));family_tick(f.right,1000);family_tick(f.right,1150);
  native_ready_at=1560;family_native_frame(1160);if(kind==1)base_ready=0;
  assert(!actual_ready(f.gun));assert(family_tick(f.right,1160)==1&&rf_post.stage==3&&!rf_post.pulse&&!g_cmd_until[side?26:28]);
  for(long now=1170;now<1560;now+=10){family_native_frame(now);assert(family_tick(f.right,now)==1);}
  family_native_frame(1560);assert(family_tick(f.right,1560)==2&&pulse_edges==1&&!physical_shots);
 }
}
static void test_mechanical(void){for(int side=0;side<2;side++){
 family_ready(3,side);assert(reload_family_post_begin(f.right,f.gun,1000));int result=1;
 for(long now=1000;now<1800&&result==1;now+=10){family_native_frame(now);result=family_tick(f.left,now);}assert(result==2&&press_calls==1&&release_calls==1&&!pulse_edges&&!native_trigger);
 family_ready(4,side);assert(reload_family_post_begin(f.right,f.gun,1000));result=1;
 for(long now=1000;now<1800&&result==1;now+=10){family_native_frame(now);result=family_tick(f.right,now);}assert(result==2&&!press_calls&&!pulse_edges&&!native_trigger&&world_calls);
 family_ready(3,side);assert(reload_family_post_begin(f.right,f.gun,1000));family_tick(f.left,1000);left_grip=f.foreign;g_touch_cancel=1;assert(family_tick(f.left,1020)==3&&!release_calls&&left_grip==f.foreign&&hover_removes==1);
 }
 family_ready(3,1);assert(reload_family_post_begin(f.right,f.gun,1000));family_tick(f.left,1000);g_touch_inventory_request=1;assert(family_tick(f.left,1020)==3&&release_calls==1);
 for(int side=0;side<2;side++){
  family_ready(3,side);assert(reload_family_post_begin(f.right,f.gun,1000));family_tick(f.left,1000);f.gun[0x111c]=1;
  g_cmd_until[side?29:27]=9999;assert(family_tick(f.left,1020)==3&&!release_calls&&left_grip==f.forest&&g_cmd_until[side?29:27]==9999);
  family_ready(3,side);assert(reload_family_post_begin(f.right,f.gun,1000));family_tick(f.left,1000);g_reload_phase=0;reload_family_post_cancel();assert(release_calls==1&&!left_grip&&!forest_hand&&!down);
  family_ready(3,side);assert(reload_family_post_begin(f.right,f.gun,1000));family_tick(f.left,1000);f.gun[0x111c]=1;release_timer=1;
  assert(family_tick(f.left,1020)==1&&rf_post.stage==4&&release_calls==1&&!left_grip&&!forest_hand&&!down);
  assert(family_tick(f.left,1100)==1&&press_calls==1);base_ready=1;assert(family_tick(f.left,1120)==2&&release_calls==1&&press_calls==1);
  family_ready(3,side);assert(reload_family_post_begin(f.right,f.gun,1000));family_tick(f.left,1000);f.gun[0x111c]=1;g_grip_latched=1u<<(side?29:27);
  assert(family_tick(f.left,1020)==3&&!release_calls&&left_grip==f.forest);
  family_ready(3,side);assert(reload_family_post_begin(f.right,f.gun,1000));family_tick(f.left,1000);f.gun[0x111c]=1;release_no_up=1;
  assert(family_tick(f.left,1020)==3&&left_grip==f.forest&&forest_hand==f.left&&down);
 }
 family_ready(5,1);assert(reload_family_post_begin(f.right,f.gun,1000));assert(family_tick(f.right,1200)==1&&!world_calls&&!press_calls);ready_override=1;assert(family_tick(f.right,1300)==2);
}
static void test_guards(void){
 int* blocks[]={&g_touch_cancel,&g_touch_weapon_request,&g_touch_reload_request,&g_touch_inventory_request,&g_touch_pause,&g_touch_interact_request,&user_fire,&paused,&reloading};
 for(unsigned i=0;i<sizeof blocks/sizeof *blocks;i++){family_ready(1,1);*blocks[i]=1;assert(!reload_family_post_begin(f.right,f.gun,1000));}
 for(int i=0;i<13;i++){family_ready(3,1);assert(reload_family_post_begin(f.right,f.gun,1000));*(uint64_t*)((char*)rf_post.binding.object[i]+0x18)^=1ULL<<40;assert(family_tick(f.left,1020)==3&&!press_calls);}
 family_ready(1,1);assert(reload_family_post_begin(f.right,f.gun,1000));assert(!family_tick(f.inactive,1020)&&rf_post.kind);fixture_tid=42;assert(!family_tick(f.right,1020)&&rf_post.kind);reload_family_post_cancel();assert(rf_post.kind);fixture_tid=41;reload_family_post_cancel();assert(!rf_post.kind);
 family_ready(1,1);assert(reload_family_post_begin(f.right,f.gun,1000));assert(family_tick(f.right,999)==3);
 family_ready(1,1);assert(reload_family_post_begin(f.right,f.gun,1000));assert(family_tick(f.right,6000)==3);
 family_ready(1,1);assert(reload_family_post_begin(f.right,f.gun,1000));family_tick(f.right,1000);cancel_count=count_calls+1;assert(family_tick(f.right,1150)==3&&!g_cmd_until[26]);
 family_ready(1,1);assert(reload_family_post_begin(f.right,f.gun,1000));family_tick(f.right,1000);rename_count=count_calls+1;assert(family_tick(f.right,1150)==3&&!g_cmd_until[26]);
 for(int side=0;side<2;side++){
  family_ready(1,side);left_grip=x.ammo_grip;down=1;ammo_primary=f.left;ammo_held=1;
  assert(reload_family_post_begin(f.right,f.gun,1000));assert(family_tick(f.right,1150)==1&&rf_post.stage==0&&!g_cmd_until[side?26:28]&&!release_calls);
  left_grip=0;down=0;ammo_primary=0;ammo_held=0;assert(family_tick(f.right,1160)==1&&rf_post.stage==1);assert(family_tick(f.right,1310)==1&&rf_post.pulse);
  family_ready(3,side);left_grip=f.foreign;assert(!reload_family_post_begin(f.right,f.gun,1000)&&!release_calls);
  family_ready(1,side);ammo_primary=f.inactive;assert(!reload_family_post_begin(f.right,f.gun,1000));
  family_ready(3,side);left_grip=x.ammo_grip;down=1;assert(reload_family_post_begin(f.right,f.gun,1000));left_grip=f.foreign;assert(family_tick(f.left,1020)==3&&!release_calls&&left_grip==f.foreign);
 }
 for(int kind=1;kind<=2;kind++)for(int offset=0;offset<3;offset++){
  family_ready(kind,1);assert(reload_family_post_begin(f.right,f.gun,1000));family_tick(f.right,1000);
  mutate_ready_at=ready_calls+2;mutate_ready_offset=offset==0?0xdb8:offset==1?0xd22:kind==1?0x10f0:0x11dd;
  assert(family_tick(f.right,1150)==3&&!g_cmd_until[26]&&!rf_post.kind);
 }
}
static void test_family_geometry(void){
 for(int side=0;side<2;side++)for(int i=0;i<150;i++){
  family_ready(3,side);float a=(float)i*.043f;tf(x.mesh)->q=(qf_t){0,0,sinf(a/2),cosf(a/2)};
  tf(x.mesh)->p[0]=tf(f.roots[2])->p[0]=(float)i;tf(x.mesh)->p[1]=tf(f.roots[2])->p[1]=-(float)i*.3f;
  float v[3]={30,0,0},world[3];q_rot(tf(x.mesh)->q,v,world);for(int axis=0;axis<3;axis++)tf(f.roots[3])->p[axis]=tf(x.mesh)->p[axis]+world[axis];
  assert(reload_family_post_begin(f.right,f.gun,1000));int result=1;
  for(long now=1000;now<1800&&result==1;now+=10){family_native_frame(now);result=family_tick(f.left,now);}
  assert(result==2&&press_calls==1&&release_calls==1&&!pulse_edges);
  family_ready(4,side);tf(f.roots[1])->q=(qf_t){0,sinf(a/2),0,cosf(a/2)};assert(reload_family_post_begin(f.right,f.gun,1000));result=1;
  for(long now=1000;now<1800&&result==1;now+=10){family_native_frame(now);result=family_tick(f.right,now);}
  assert(result==2&&!pulse_edges&&!press_calls);
 }
 family_ready(4,1);tf(f.roots[1])->q=(qf_t){0,-.70710678f,0,.70710678f};assert(reload_family_post_begin(f.right,f.gun,1000));int result=1;
 for(long now=1000;now<1800&&result==1;now+=10){family_native_frame(now);result=family_tick(f.right,now);}assert(result==2);
 family_ready(3,1);assert(reload_family_post_begin(f.right,f.gun,1000));family_tick(f.left,1000);*(float*)(f.gun+0x112c)=INFINITY;assert(family_tick(f.left,1020)==3&&release_calls==1);
 family_ready(3,1);tf(f.roots[3])->p[0]=10000;assert(reload_family_post_begin(f.right,f.gun,1000));assert(family_tick(f.left,1000)==3&&!press_calls);
 family_ready(4,1);close_model=0;assert(reload_family_post_begin(f.right,f.gun,1000));assert(family_tick(f.right,6000)==3&&!pulse_edges);
}
static void test_shotgun_reload_pump(void){
 for(int side=0;side<2;side++){
  family_ready(5,side);wc_shotgun_class=family_class4;wc_reload_pending=rf_shotgun_reload_pending;
  ti_press=press;rl_world_transform=set_world;f.gun[0x1160]=1;f.gun[0x1161]=2;
  assert(reload_family_post_begin(f.right,f.gun,1000));assert(!wc_inputs_clear());
  assert(family_tick(f.left,1000)==1&&rf_shotgun_reload_pending(f.gun));
  touch_weapon_cycle_tick(f.left,1-side,1000);assert(touch_weapon_cycle_busy()&&press_calls==1);
  int result=1;for(long time=1010;time<1800&&result==1;time+=10){
   fixture_clock=time;native_frame();touch_weapon_cycle_tick(f.left,1-side,time);result=family_tick(f.left,time);
  }
  assert(result==2&&!touch_weapon_cycle_busy()&&pushes==1&&release_calls==1&&press_calls==1&&!down);
  // Some authored shotgun assets push forward inside ForestockGripGrabbed.
  // Ready must not let phase7 release the shared pump's still-owned hand.
  family_ready(5,side);wc_shotgun_class=family_class4;wc_reload_pending=rf_shotgun_reload_pending;
  ti_press=press;rl_world_transform=set_world;f.gun[0x1160]=1;f.gun[0x1161]=2;
  assert(reload_family_post_begin(f.right,f.gun,1000));assert(family_tick(f.left,1000)==1);
  touch_weapon_cycle_tick(f.left,1-side,1000);assert(touch_weapon_cycle_busy());
  f.gun[0x1160]=0;f.gun[0x1161]=0;
  assert(family_tick(f.left,1000)==1&&rf_post.kind==5&&down&&!release_calls);
  fixture_clock=1016;native_frame();touch_weapon_cycle_tick(f.left,1-side,1016);
  assert(!touch_weapon_cycle_busy()&&!down&&release_calls==1);
  assert(family_tick(f.right,1016)==2&&!rf_post.kind&&press_calls==1);
  family_ready(5,side);f.gun[0x1160]=0;f.gun[0x1161]=0;
  assert(reload_family_post_begin(f.right,f.gun,1000));
  assert(family_tick(f.right,1000)==2&&!rf_post.kind&&!press_calls&&!release_calls&&!touch_weapon_cycle_busy());
 }
 family_ready(5,1);wc_reload_pending=rf_shotgun_reload_pending;assert(reload_family_post_begin(f.right,f.gun,1000));
 assert(!rf_shotgun_reload_pending(0));family_tick(f.left,1000);assert(rf_shotgun_reload_pending(f.gun)&&!rf_shotgun_reload_pending(f.foreign));
}
int main(void){
#ifdef FAMILY_NATIVE_FIXTURE
 verify_reload_family_install_guard();
#endif
 test_single_shell_commit();test_shell_repeat();test_metadata();test_edges();test_mechanical();test_guards();test_family_geometry();test_shotgun_reload_pump();puts("PASS: actual family adapter both handednesses degree metadata/ammo-UP/one edge/delayed native ack-UP, mechanical release/foreign hold/late getters/13 names/600 geometry cases; no ammo changes");
 puts("PASS: exact shared post-shell reload -> native shotgun pump -> ready handoff for both primary hands; no phase7 deadlock");return 0;}
