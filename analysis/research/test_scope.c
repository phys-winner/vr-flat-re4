#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stddef.h>
typedef struct{float x,y,z,w;}qf_t;
struct __attribute__((aligned(16))) reload_tf{qf_t q;float p[3],s[3],pad[2];};
/* SHARED_MATH */
static int g_touch_context=1,g_touch_cancel,g_reload_phase,game_thread=1;
static int qs_gt(void){return game_thread;}
static unsigned touch_quickselect_pose_generation(void){return 0;}
static void* (*body_active)(void),*(*rl_primary)(const void*),*(*f_GetCurrentGrip)(const void*);
static int (*aim_blending)(const void*),(*aim_flourishing)(const void*),(*aim_recoiling)(const void*);
#define LOGF(...) ((void)0)
#include "SCOPE_HEADER"
static struct{
 unsigned char pawn[0x1300],hand[0x570],gun[0x1100],scope[0x300],root[0x500],gunroot[0x500],box[2][0x500],curve[0x200],primary[0x460];
 unsigned char cls[5][0x100];void* chains[5][2];
}f __attribute__((aligned(16)));
static void* active,*primary_hand,*current_grip;static int animating,scope_calls;static float curve_value;
static void* cls_two(void){return f.cls[0];}static void* cls_scope(void){return f.cls[1];}
static void* cls_scene(void){return f.cls[2];}static void* cls_box(void){return f.cls[3];}static void* cls_curve(void){return f.cls[4];}
static void* get_scope(const void* gun){assert(gun==f.gun);scope_calls++;return f.scope;}
static void* get_active(void){return active;}static void* get_primary(const void* gun){assert(gun==f.gun);return primary_hand;}
static void* get_current(const void* hand){assert(hand==f.hand);return current_grip;}
static int animation(const void* gun){assert(gun==f.gun);return animating;}
static float pitch(const float* v){return atan2f(v[2],sqrtf(v[0]*v[0]+v[1]*v[1]))*57.29577951308232f;}
static float curve(const void* object,float value){assert(object==f.curve && isfinite(value));return curve_value;}
static struct reload_tf lerp(const struct reload_tf* a,const struct reload_tf* b,float alpha,int flag){
 assert(!flag);struct reload_tf r=*a;
 qf_t end=b->q;float dot=a->q.x*end.x+a->q.y*end.y+a->q.z*end.z+a->q.w*end.w;
 if(dot<0){end.x=-end.x;end.y=-end.y;end.z=-end.z;end.w=-end.w;dot=-dot;}
 float aa=1-alpha,bb=alpha;if(dot<.9995f){float angle=acosf(dot);aa=sinf((1-alpha)*angle)/sinf(angle);bb=sinf(alpha*angle)/sinf(angle);}
 r.q=(qf_t){aa*a->q.x+bb*end.x,aa*a->q.y+bb*end.y,aa*a->q.z+bb*end.z,aa*a->q.w+bb*end.w};
 float n=sqrtf(r.q.x*r.q.x+r.q.y*r.q.y+r.q.z*r.q.z+r.q.w*r.q.w);r.q.x/=n;r.q.y/=n;r.q.z/=n;r.q.w/=n;
 for(int i=0;i<3;i++){r.p[i]=a->p[i]+(b->p[i]-a->p[i])*alpha;r.s[i]=a->s[i]+(b->s[i]-a->s[i])*alpha;}return r;
}
static struct reload_tf identity={{0,0,0,1},{0,0,0},{1,1,1},{0,0}};
static void ready(void){
 memset(&f,0,sizeof f);memset(&as_cache,0,sizeof as_cache);as_logs=0;
 for(int i=0;i<5;i++){
  *(int*)(f.cls[i]+0x58)=i==0 ? sizeof f.gun : i==1 ? sizeof f.scope : i==4 ? sizeof f.curve : sizeof f.root;
  *(int*)(f.cls[i]+0x38)=i==3 ? 1 : 0;f.chains[i][0]=f.cls[i==3 ? 2 : i]+0x30;f.chains[i][1]=f.cls[3]+0x30;
  *(void***)(f.cls[i]+0x30)=f.chains[i];
 }
 *(void**)(f.gun+0x10)=f.cls[0];*(void**)(f.scope+0x10)=f.cls[1];*(void**)(f.root+0x10)=*(void**)(f.gunroot+0x10)=f.cls[2];
 *(void**)(f.curve+0x10)=f.cls[4];*(void**)(f.gun+0x3c0)=f.pawn;*(void**)(f.gun+0x270)=f.primary;
 *(void**)(f.gun+0x130)=f.gunroot;*(void**)(f.scope+0x220)=*(void**)(f.scope+0x130)=f.root;
 *(void**)(f.root+0x20)=f.scope;*(void**)(f.root+0xc0)=f.gunroot;*(void**)(f.gunroot+0x20)=f.gun;
 memcpy(f.root+0x1c0,&identity,48);memcpy(f.gunroot+0x1c0,&identity,48);
 for(int i=0;i<2;i++){
  *(void**)(f.scope+(i ? 0x238 : 0x228))=f.box[i];*(void**)(f.box[i]+0x10)=f.cls[3];
  *(void**)(f.box[i]+0x20)=f.scope;*(void**)(f.box[i]+0xc0)=f.root;
  struct reload_tf tf=identity;tf.p[0]=-5;tf.p[2]=i*4;tf.s[0]=.7f;tf.s[1]=1.2f;
  memcpy(f.box[i]+0x1c0,&tf,48);*(unsigned long long*)(f.box[i]+0x18)=700+i;
 }
 *(float*)(f.scope+0x250)=5;for(int i=1;i<4;i++)*(float*)(f.scope+0x250+i*4)=10;
 active=f.pawn;primary_hand=f.hand;current_grip=f.primary;animating=0;scope_calls=0;curve_value=.75f;
 g_touch_context=1;g_touch_cancel=g_reload_phase=0;game_thread=as_ready=1;
 body_active=get_active;rl_primary=get_primary;f_GetCurrentGrip=get_current;
 aim_blending=aim_flourishing=aim_recoiling=animation;
 as_two_class=cls_two;as_scope_class=cls_scope;as_scene_class=cls_scene;as_box_class=cls_box;as_curve_class=cls_curve;
 as_scope=get_scope;as_lerp=lerp;as_pitch=pitch;as_curve=curve;
}
static int adjust(struct reload_tf h,struct reload_tf head,long key,float alpha,struct reload_tf* pose){
 return aim_scope_adjust(f.gun,f.pawn,f.hand,h,head,key,alpha,pose);
}
int main(void){
 for(int i=0;i<300;i++){
  ready();float a=(float)i*.013f;struct reload_tf h=identity,head=identity,pose=identity;
  head.p[0]=10;head.p[1]=20;head.p[2]=40;head.q=pose.q=(qf_t){0,0,sinf(a),cosf(a)};
  pose.p[0]=35;pose.p[1]=14;pose.p[2]=-15;
  unsigned char before[sizeof f];memcpy(before,&f,sizeof f);qf_t original=pose.q;
  assert(adjust(h,head,1000,1,&pose)==1);assert(!memcmp(&original,&pose.q,sizeof original));assert(!memcmp(before,&f,sizeof f));
  struct reload_tf box=aim_world_marker(pose,as_cache.box[0]);for(int j=0;j<3;j++)assert(fabsf(box.p[j]-head.p[j])<.001);
  float shot[3],forward[3];q_rot(q_mul(pose.q,(qf_t){0,0,-.70710678118f,.70710678118f}),(float[3]){0,1,0},shot);q_rot(head.q,(float[3]){1,0,0},forward);
  for(int j=0;j<3;j++)assert(fabsf(shot[j]-forward[j])<.001);
 }
 ready();struct reload_tf h=identity,head=identity,pose=identity;head.p[2]=50;
 *(void**)(f.scope+0x248)=f.curve;assert(adjust(h,head,1000,1,&pose));
 struct reload_tf box=as_lerp(&as_cache.box[0],&as_cache.box[1],.75f,0);box=aim_world_marker(pose,box);
 for(int j=0;j<3;j++)assert(fabsf(box.p[j]-head.p[j])<.001);
 struct reload_tf stable=pose;animating=1;assert(adjust(h,head,1000,1,&pose));assert(!memcmp(&stable,&pose,sizeof pose));
 (*(unsigned long long*)(f.box[0]+0x18))++;assert(!adjust(h,head,1000,1,&pose));
 ready();pose=identity;head=identity;head.p[2]=50;assert(adjust(h,head,1000,.5f,&pose));
 assert(fabsf(pose.p[0]-2.5f)<.001 && fabsf(pose.p[2]-25)<.001);
 for(int why=0;why<13;why++){
  ready();pose=identity;head=identity;head.p[2]=50;
  if(why==0)game_thread=0;if(why==1)g_touch_cancel=1;if(why==2)g_reload_phase=1;if(why==3)active=f.gun;
  if(why==4)*(void**)(f.gun+0x3c0)=f.scope;if(why==5)primary_hand=0;if(why==6)current_grip=0;
  if(why==7)*(void**)(f.root+0xc0)=f.root;if(why==8)*(void**)(f.box[1]+0x20)=f.gun;
  if(why==9)*(void**)(f.scope+0x130)=f.gunroot;if(why==10)*(float*)(f.scope+0x254)=0;
  if(why==11){struct reload_tf tf=identity;tf.q=(qf_t){0,0,.70710678f,.70710678f};memcpy(f.box[0]+0x1c0,&tf,48);}
  if(why==12){head.p[2]=1000;}
  struct reload_tf before=pose;assert(!adjust(h,head,1000,1,&pose));assert(!memcmp(&pose,&before,sizeof pose));
 }
 puts("PASS:300 rotated native-eye-box ADS poses preserve FireBone shot axis/quaternion and all native bytes; authored curve/downward interpolation/positive-scale boxes");
 puts("PASS: scope owner/parent/root/identity/recoil/acquisition/context/GameThread/angular/native-reach refusal; original scope lifecycle receives geometry only");
 return 0;
}
