#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static void tt_after_mutex_lock(void);
#define RE4_TEST_MUTEX_AFTER_LOCK() tt_after_mutex_lock()
#include <pthread.h>
#ifndef RE4_TEST_MUTEX_LOCKED
#define RE4_TEST_MUTEX_LOCKED(m) 0
#endif
typedef struct{float x,y,z,w;} qf_t;
static qf_t q_mul(qf_t a,qf_t b){return(qf_t){a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
 a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
 a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
struct __attribute__((aligned(16))) reload_tf{qf_t q;float p[3],s[3],pad[2];};
struct cinema_ui_weak{int index,serial;};
struct cinema_ui_vec2{float x,y;};
static unsigned char objects[24][0x1800],classes[20][0x100],functions[9][0x100],fields[35][0x90];
static void* table[160];
static int alive[24],serials[24],gt=1,context=6,g_touch_context=6,g_app_input_focus=1,phone_inspect_ready=1;
static int visual_calls;static float visual_amount;
static struct reload_tf key_surfaces[2];static int attach_calls,scroll_calls;static unsigned char scroll_forward;
static int accepted,releases,cancels,calls,input_locked,enabled=1,activation_enabled=1,destroy_key,destroy_theatre,swap_pawn;
static long clock_ms=1000;
static unsigned g_comp_w=2000,g_comp_h=1000;
static void* active;
static void* dial_player(const void* hand){assert(hand==objects[20]);return active;}
static void*(*rl_player)(const void*)=dial_player;
static const char* names[]={"","VR4PlayerPawn_Typewriter_C","TypewriterTheatreBox_C","TypewriterKeyBase_C","TypewriterKeyStandard_C",
 "ChildActorComponent","CapsuleComponent","BoxComponent","SceneComponent","VR4Camera","WidgetComponent","Typewriter_Bell_C","Function",
 "SetKeyActivated","IsKeyEnabled","GetFingerMagnetCollision","OnRing","TheatreBox","AllKeys","ParentComponent","ChildActor","CapsuleRadius",
 "CapsuleHalfHeight","BoxExtent","CancelWidget","BellActor","Type_CancelBell_01","PressCollision","ObjectProperty","ArrayProperty","FloatProperty","StructProperty","BoolProperty","WeakObjectProperty",
 "WidgetTitle","WidgetList","WidgetOverwrite","FingerMagnetCollision","VR4GamePlayerMeshSet","SkeletalMeshComponent","ByteProperty","OverwriteKey","TypewriterKeyOverwrite_C","OvewriteCollision","TypewriterRoot","KeyPressThreshold","TypeWriterKeyBase_Translating_C","TypeWriterKeyBase_Rotating_C","SetAnimBPKeyTranslation","SetAnimBPKeyAngle","KeyPressThresholdDegrees","GetAttachPoint","NameProperty","ParentSocket","TypewriterMesh","LocalKeySurfaceNormal","ScrollList","ScrollActive","HandState_LH","TypewriterHandState_C","Hand","VR4GamePlayerHand","HandRoot","DummyDial","IkRotatorDial",
 "BndEvt__DialPlayerGrip_K2Node_ComponentBoundEvent_5_GripHandEventMulticastDelegate__DelegateSignature","BndEvt__DialPlayerGrip_K2Node_ComponentBoundEvent_6_GripHandEventMulticastDelegate__DelegateSignature"};
static int object_index(const void* p){for(int i=0;i<24;i++)if(p==objects[i])return i;return -1;}
static int pv_alive(const void* p){int i=object_index(p);if(i>=0)return alive[i];for(int k=0;k<20;k++)if(p==classes[k])return 1;for(int k=0;k<9;k++)if(p==functions[k])return 1;return 0;}
static void name_read(const void* p,char* out,int cap){unsigned i;memcpy(&i,p,4);const char* s=i<sizeof names/sizeof *names?names[i]:"";assert((int)strlen(s)<cap);memcpy(out,s,strlen(s)+1);}
static void phone_inspect_fname(const void* p,char* out,int cap){name_read(p,out,cap);}
static void obj_name(const void* p,char* out,int cap){assert(pv_alive(p));name_read((const char*)p+0x18,out,cap);}
static int cinema_ui_named(const void* p,const char* wanted,int minimum){if(!pv_alive(p))return 0;const char* c=*(const char* const*)((const char*)p+0x10);char n[96];name_read(c+0x18,n,sizeof n);return !strcmp(n,wanted)&&*(const int*)(c+0x58)>=minimum;}
static int cinema_object(const void* p,const void* cls,int minimum){return pv_alive(p)&&*(const void* const*)((const char*)p+0x10)==cls&&*(const int*)((const char*)cls+0x58)>=minimum;}
static int menu_visual_layout(const void* p,const void* cls,int minimum){return pv_alive(p)&&*(const int*)(*(const char* const*)((const char*)p+0x10)+0x58)>=minimum&&(cls==classes[7]||*(const void* const*)((const char*)p+0x10)==cls);}
static void weak_assign(struct cinema_ui_weak* w,const void* p){w->index=object_index(p);w->serial=w->index<0?0:serials[w->index];}
static void* weak_get(const struct cinema_ui_weak* w){return w->index>=0&&w->index<24&&alive[w->index]&&serials[w->index]==w->serial?objects[w->index]:0;}
static void(*cu_weak_assign)(struct cinema_ui_weak*,const void*)=weak_assign;
static void*(*cu_weak_get)(const struct cinema_ui_weak*)=weak_get;
static void* get_active(void){assert(!input_locked);return active;}static unsigned get_type(void){assert(!input_locked);return context;}
static void* camera_class(void){return classes[8];}static void* scene_class(void){return classes[7];}static void* widget_class(void){return classes[9];}
static void*(*phone_active)(void)=get_active;static unsigned(*phone_type)(void)=get_type;
static void*(*cinema_camera_class)(void)=camera_class,*(*mv_scene_class)(void)=scene_class,*(*cu_component_class)(void)=widget_class;
static void* skeletal_class(void){return classes[15];}
static void*(*mv_skeletal_class)(void)=skeletal_class;
static int hidden_hands;
static void hide_mesh(void* mesh,int value,int propagate){assert((mesh==objects[21]||mesh==objects[22])&&!value&&!propagate);hidden_hands++;*(unsigned short*)((char*)mesh+0x14c)&=~0x20;}
static void(*mv_visible)(void*,int,int)=hide_mesh;
static int cinema_on_game_thread(void){return gt;}static long ptr_now_ms(void){return clock_ms;}
static void(*cinema_typewriter_view)(void*,void*);
static struct cinema_ui_vec2 draw_sizes[24];
static struct cinema_ui_vec2 draw_size(const void* p){int i=object_index(p);assert(i>=0);return draw_sizes[i];}
static struct cinema_ui_vec2(*fv_current_draw_size)(const void*)=draw_size;
static void q_rot(qf_t q,const float v[3],float o[3]){float d=q.x*v[0]+q.y*v[1]+q.z*v[2],n=q.x*q.x+q.y*q.y+q.z*q.z,k=q.w*q.w-n;o[0]=2*d*q.x+k*v[0]+2*q.w*(q.y*v[2]-q.z*v[1]);o[1]=2*d*q.y+k*v[1]+2*q.w*(q.z*v[0]-q.x*v[2]);o[2]=2*d*q.z+k*v[2]+2*q.w*(q.x*v[1]-q.y*v[0]);}
static int reload_marker_valid(const struct reload_tf* t){float n=t->q.x*t->q.x+t->q.y*t->q.y+t->q.z*t->q.z+t->q.w*t->q.w;if(!isfinite(n)||fabsf(n-1)>.01f)return 0;for(int k=0;k<3;k++)if(!isfinite(t->p[k])||!isfinite(t->s[k])||t->s[k]<=0)return 0;return 1;}
static const char* cinema_field_property(const void* object,const char* wanted,const char* kind){
 int i=object_index(object),f=-1;
 if(i==0&&!strcmp(wanted,"TheatreBox"))f=0;
 if(i==1&&!strcmp(wanted,"AllKeys"))f=1;
 if((i==2||i==3)&&!strcmp(wanted,"ParentComponent"))f=2;
 if((i==4||i==5)&&!strcmp(wanted,"ChildActor"))f=3;
 if((i==6||i==7||i==12)&&!strcmp(wanted,"CapsuleRadius"))f=4;
 if((i==6||i==7||i==12)&&!strcmp(wanted,"CapsuleHalfHeight"))f=5;
 if((i==6||i==7||i==23)&&!strcmp(wanted,"BoxExtent"))f=6;
 if(i==1&&!strcmp(wanted,"CancelWidget"))f=7;
 if(i==1&&!strcmp(wanted,"BellActor"))f=8;
 if(i==10&&!strcmp(wanted,"Type_CancelBell_01"))f=9;
 if(i==10&&!strcmp(wanted,"PressCollision"))f=10;
 if(i==1&&!strcmp(wanted,"WidgetTitle"))f=11;
 if(i==1&&!strcmp(wanted,"WidgetList"))f=12;
 if(i==1&&!strcmp(wanted,"WidgetOverwrite"))f=13;
 if((i==2||i==3)&&!strcmp(wanted,"FingerMagnetCollision"))f=14;
 if(i==0&&!strcmp(wanted,"MeshSet"))f=15;
 if(i==0&&!strcmp(wanted,"RootComponent"))f=16;
 if(i==18&&!strcmp(wanted,"MeshesRoot"))f=17;
 if(i==18&&!strcmp(wanted,"Mesh"))f=18;
 if(i==18&&!strcmp(wanted,"SecondaryMesh"))f=19;
 if(i==1&&!strcmp(wanted,"currentState"))f=20;
 if(i==1&&!strcmp(wanted,"OverwriteKey"))f=21;
 if(i==1&&!strcmp(wanted,"OvewriteCollision"))f=22;
 if((i==2||i==3)&&!strcmp(wanted,"KeyPressThreshold")&&*(unsigned*)(classes[3]+0x18)!=47)f=23;
 if((i==2||i==3)&&!strcmp(wanted,"KeyPressThresholdDegrees")&&*(unsigned*)(classes[3]+0x18)==47)f=27;
 if(i==1&&!strcmp(wanted,"TypewriterMesh"))f=25;
 if((i==2||i==3)&&!strcmp(wanted,"LocalKeySurfaceNormal"))f=26;
 if(i==1&&!strcmp(wanted,"ScrollActive"))f=28;
 if(i==1&&!strcmp(wanted,"HandState_LH"))f=30;
 if(i==19&&!strcmp(wanted,"Hand"))f=31;
 if(i==20&&!strcmp(wanted,"HandRoot"))f=32;
 if(i==1&&!strcmp(wanted,"DummyDial"))f=33;
 if(i==1&&!strcmp(wanted,"IkRotatorDial"))f=34;
 if(i==1&&!strcmp(wanted,"TypewriterRoot"))f=24;
 if(f<0)return 0;char actual[48];name_read(*(void**)(fields[f]+8),actual,sizeof actual);return !strcmp(actual,kind)?(const char*)fields[f]:0;
}
static void process_event(void*,void*,void*);
static struct reload_tf widget_relative[24];static int widget_world_writes,widget_restores;
static struct reload_tf relative_widget(const void* component){int i=object_index(component);assert(i>=0);return widget_relative[i];}
static void set_widget(void* component,const struct reload_tf* tf,int sweep,void* hit,int teleport){
 int i=object_index(component);assert(i>=0&&!sweep&&!hit&&!teleport&&reload_marker_valid(tf));
 memcpy(objects[i]+0x1c0,tf,sizeof *tf);widget_relative[i]=*tf;
}
static void world_widget(void* c,const struct reload_tf* tf,int s,void* h,int t){widget_world_writes++;set_widget(c,tf,s,h,t);}
static void restore_widget(void* c,const struct reload_tf* tf,int s,void* h,int t){widget_restores++;set_widget(c,tf,s,h,t);}
static struct reload_tf(*fv_relative_transform)(const void*)=relative_widget;
static void(*fv_set_relative)(void*,const struct reload_tf*,int,void*,int)=restore_widget;
static void(*rl_world_transform)(void*,const struct reload_tf*,int,void*,int)=world_widget;
#define LOGF(...) ((void)0)
#include "../re4fix_typewriter_touch.h"
static int publish_stamp_on_lock;
static void tt_after_mutex_lock(void){if(publish_stamp_on_lock){publish_stamp_on_lock=0;tt_input.stamp=++clock_ms;}}
static void process_event(void* object,void* fn,void* params){
 assert(!RE4_TEST_MUTEX_LOCKED(tt_mutex)&&!input_locked&&gt);calls++;
 if(fn==functions[0]){assert(object==objects[2]||object==objects[3]);if(*(unsigned char*)params){if(enabled&&activation_enabled)accepted++;if(destroy_key)alive[object_index(object)]=0;if(destroy_theatre)alive[1]=0;if(swap_pawn){context=4;active=objects[13];}}else releases++;}
 else if(fn==functions[1])*(unsigned char*)params=(unsigned char)enabled;
 else if(fn==functions[2])*(void**)params=object==objects[2]?objects[6]:objects[7];
 else if(fn==functions[3]){assert(object==objects[1]&&!params);cancels++;}
 else if(fn==functions[4]){assert(object==objects[2]||object==objects[3]);visual_calls++;visual_amount=*(float*)params;}
 else if(fn==functions[5]){assert(object==objects[2]||object==objects[3]);struct{void*component;unsigned long long socket;}*out=params;out->component=objects[23];out->socket=object==objects[2]?101:102;attach_calls++;}
 else if(fn==functions[6]){assert(object==objects[1]);scroll_forward=*(unsigned char*)params;scroll_calls++;objects[1][0x729]=1;}
 else if(fn==functions[7]){assert(object==objects[1]&&*(void**)params==objects[20]);objects[1][0x5d0]=5;}
 else if(fn==functions[8]){assert(object==objects[1]&&*(void**)params==objects[20]);if(objects[1][0x5d0]==5)objects[1][0x5d0]=0;}
 else assert(0);
}
static void field(int i,int kind,int size,int offset){memset(fields[i],0,sizeof fields[i]);*(void**)(fields[i]+8)=classes[13];*(unsigned*)(classes[13]+0x18)=kind;/* Each descriptor gets its own kind FName below. */*(int*)(fields[i]+0x34)=1;*(int*)(fields[i]+0x38)=size;*(int*)(fields[i]+0x4c)=offset;}
static unsigned kind_names[35][2];
static void property(int i,unsigned kind,int size,int offset){field(i,kind,size,offset);kind_names[i][0]=kind;kind_names[i][1]=0;*(void**)(fields[i]+8)=kind_names[i];}
static unsigned parameter_kind[9][2];static unsigned char parameters[9][0x90];
static void function(int i,int name,void* owner,int bytes,int output,int kind){
 memset(functions[i],0,sizeof functions[i]);*(void**)(functions[i]+0x10)=classes[12];*(unsigned*)(functions[i]+0x18)=name;*(void**)(functions[i]+0x20)=owner;functions[i][0xb4]=(unsigned char)!!bytes;*(unsigned short*)(functions[i]+0xb6)=(unsigned short)bytes;*(unsigned short*)(functions[i]+0xb8)=0xffff;
 if(bytes){memset(parameters[i],0,sizeof parameters[i]);parameter_kind[i][0]=(unsigned)kind;*(void**)(parameters[i]+8)=parameter_kind[i];*(int*)(parameters[i]+0x34)=1;*(int*)(parameters[i]+0x38)=bytes;*(uint64_t*)(parameters[i]+0x40)=0x80|(output?0x100:0);parameters[i][0x7b]=0xff;*(void**)(functions[i]+0x50)=parameters[i];}
}
static void transform(int i,float x,float y,float z){struct reload_tf tf={{0,0,0,1},{x,y,z},{1,1,1},{0,0}};memcpy(objects[i]+0x1c0,&tf,sizeof tf);}
static void* key_array[2];static unsigned char view[64];
static void ready(void){
 memset(tt_typing_widgets,0,sizeof tt_typing_widgets);widget_world_writes=widget_restores=0;
 publish_stamp_on_lock=0;tt_animation_started=0;visual_calls=0;visual_amount=0;tt_socket=0;attach_calls=scroll_calls=0;tt_scroll.remaining=0;
 memset(objects,0,sizeof objects);memset(classes,0,sizeof classes);memset(fields,0,sizeof fields);memset(table,0,sizeof table);memset(&tt_input,0,sizeof tt_input);memset(draw_sizes,0,sizeof draw_sizes);
 for(int i=0;i<24;i++){alive[i]=serials[i]=1;*(void***)objects[i]=table;*(void**)(objects[i]+0x10)=classes[7];transform(i,0,0,0);}
 unsigned cn[]={1,2,3,4,5,6,7,8,9,10,11,8,12,28,38,39,42};for(int i=0;i<17;i++){*(unsigned*)(classes[i]+0x18)=cn[i];*(int*)(classes[i]+0x58)=0x1800;}
 *(void**)(classes[3]+0x40)=classes[2];
 *(void**)(classes[16]+0x40)=classes[3];
 int ci[]={0,1,3,3,4,4,5,6,8,9,10,11,5,0,1,8,3,8};for(int i=0;i<18;i++)*(void**)(objects[i]+0x10)=classes[ci[i]];
 table[0x218/sizeof(void*)]=(void*)process_event;
 *(void**)(objects[0]+0x2d8)=objects[8];*(void**)(objects[8]+0xa0)=objects[0];*(void**)(objects[15]+0xa0)=objects[0];
 property(0,28,8,0x700);*(void**)(objects[0]+0x700)=objects[1];
 property(1,29,16,0x500);key_array[0]=objects[2];key_array[1]=objects[3];struct{void**data;int count,max;}list={key_array,2,2};memcpy(objects[1]+0x500,&list,16);
 property(2,33,8,0x160);property(3,28,8,0x1f8);for(int i=0;i<2;i++){struct cinema_ui_weak parent;weak_assign(&parent,objects[4+i]);memcpy(objects[2+i]+0x160,&parent,sizeof parent);*(void**)(objects[4+i]+0x20)=objects[1];*(void**)(objects[4+i]+0x1f8)=objects[2+i];}
 property(4,30,4,0x430);property(5,30,4,0x434);property(6,31,12,0x430);
 for(int i=6;i<=7;i++){*(void**)(objects[i]+0x20)=*(void**)(objects[i]+0xa0)=objects[i-4];transform(i,100,i==6?0:30,0);}
 *(float*)(objects[6]+0x430)=3;*(float*)(objects[6]+0x434)=4;float extent[]={2,6,2};memcpy(objects[7]+0x430,extent,12);
 property(7,28,8,0x520);property(8,28,8,0x528);property(9,28,8,0x700);property(10,28,8,0x708);
 *(void**)(objects[9]+0x20)=*(void**)(objects[9]+0xa0)=objects[1];transform(9,100,-40,0);float pivot[]={.5f,.5f};memcpy(objects[9]+0x45c,pivot,8);
 draw_sizes[9]=(struct cinema_ui_vec2){20,10};
 property(11,28,8,0x530);property(12,28,8,0x538);property(13,28,8,0x540);property(14,28,8,0x700);
 property(15,28,8,0xba8);property(16,28,8,0x130);property(17,28,8,0x220);property(18,28,8,0x228);property(19,28,8,0x230);property(20,40,1,0x5d0);hidden_hands=0;
 property(21,28,8,0x550);
 property(22,28,8,0x558);property(23,30,4,0x710);property(24,28,8,0x720);
 property(25,28,8,0x730);property(26,31,12,0x740);property(27,30,4,0x714);property(28,32,1,0x729);fields[28][0x7b]=1;
 property(30,28,8,0x750);property(31,28,8,0x30);property(32,28,8,0x130);property(33,28,8,0x758);property(34,28,8,0x760);
 *(void**)(objects[2]+0x700)=objects[6];*(void**)(objects[3]+0x700)=objects[7];
 *(void**)(objects[10]+0x700)=objects[11];*(void**)(objects[10]+0x708)=objects[12];*(void**)(objects[11]+0x20)=objects[10];
 *(void**)(objects[12]+0x20)=*(void**)(objects[12]+0xa0)=objects[10];transform(12,100,-60,0);*(float*)(objects[12]+0x430)=7;*(float*)(objects[12]+0x434)=8;
 function(0,13,classes[2],1,0,32);function(1,14,classes[2],1,1,32);function(2,15,classes[3],8,1,28);function(3,16,classes[1],0,0,0);
 *(void**)(classes[2]+0x48)=functions[0];*(void**)(functions[0]+0x28)=functions[1];*(void**)(classes[3]+0x48)=functions[2];*(void**)(classes[1]+0x48)=functions[3];
 memset(view,0,sizeof view);*(float*)(view+0x18)=90;*(float*)(view+0x2c)=2;
 clock_ms=1000;gt=1;active=objects[0];context=g_touch_context=6;g_app_input_focus=phone_inspect_ready=1;accepted=releases=cancels=calls=destroy_key=destroy_theatre=swap_pawn=0;enabled=activation_enabled=1;input_locked=0;
 phone_active=get_active;phone_type=get_type;tt_event_address=(void*)process_event;tt_ready=1;tt_bound=0;tt_logs=0;tt_pawn=tt_theatre=tt_camera=(struct cinema_ui_weak){-1,0};
 touch_typewriter_view(objects[8],view);assert(tt_bound&&tt_input.stamp==1000);
}
static void event(int h,int phase,float u,float v){input_locked=1;assert(touch_typewriter_event(h,phase,u*g_comp_w,v*g_comp_h,g_comp_w,g_comp_h,clock_ms));input_locked=0;}
static void tap(float u,float v){event(1,0,u,v);clock_ms+=40;event(1,2,u,v);}
static void frame(void){clock_ms+=1;touch_typewriter_view(objects[8],view);}
static void queue_tests(void){
 ready();tap(.5f,.5f);assert(!calls&&tt_input.pending);frame();assert(accepted==1&&releases==1);frame();assert(accepted==1);
 ready();tap(.65f,.5f);frame();assert(accepted==1&&releases==1);
 ready();event(1,0,.5f,.5f);event(2,0,.65f,.5f);event(2,2,.65f,.5f);clock_ms+=50;event(1,2,.5f,.5f);frame();assert(accepted==1);
 ready();event(1,0,.5f,.5f);event(1,1,.6f,.5f);event(1,2,.5f,.5f);frame();assert(!accepted&&!cancels);
 ready();event(1,0,.5f,.5f);clock_ms+=1501;touch_typewriter_view(objects[8],view);event(1,2,.5f,.5f);frame();assert(!accepted);
 ready();tap(.5f,.5f);touch_typewriter_cancel();frame();assert(!accepted&&!calls);
 ready();tap(.5f,.5f);clock_ms+=501;frame();assert(!accepted&&!calls);
 ready();tap(.5f,.5f);clock_ms=900;touch_typewriter_view(objects[8],view);assert(!accepted&&!calls);
 ready();tap(.5f,.5f);g_app_input_focus=0;frame();assert(!accepted&&!calls);
 ready();tap(.5f,.5f);context=g_touch_context=4;frame();assert(!accepted&&!tt_bound);assert(!touch_typewriter_event(1,0,100,100,2000,1000,clock_ms));
 ready();tap(.5f,.5f);gt=0;frame();assert(!accepted&&tt_input.pending);gt=1;frame();assert(accepted==1);
 ready();event(1,0,.5f,.5f);event(1,2,NAN,.5f);frame();assert(!accepted);
 ready();clock_ms+=301;event(1,0,.5f,.5f);event(1,2,.5f,.5f);frame();assert(!accepted);
 // Game thread publishes a newer stamp while this input's earlier now value waits for tt_mutex.
 ready();publish_stamp_on_lock=1;event(1,0,.5f,.5f);assert(tt_input.down&&clock_ms==1001);
 clock_ms+=40;event(1,2,.5f,.5f);frame();assert(accepted==1&&releases==1);
 puts("PASS: actual queued touch, one finger, drag/long/focus/reset/stale/context/GT guards; no native call under either input mutex");
}
static void ownership_tests(void){
 for(int mode=0;mode<9;mode++){
  ready();tap(.5f,.5f);
  if(mode==0)serials[0]++;if(mode==1)serials[1]++;if(mode==2)serials[8]++;
  if(mode==3)*(void**)(objects[0]+0x2d8)=objects[15];if(mode==4)*(void**)(objects[8]+0xa0)=objects[13];
  if(mode==5)*(void**)(objects[4]+0x20)=objects[14];if(mode==6)*(void**)(objects[4]+0x1f8)=objects[3];
  if(mode==7)*(int*)(objects[1]+0x508)=65;if(mode==8)*(void**)(objects[6]+0x20)=objects[3];
  frame();assert(!accepted&&!cancels);
 }
 ready();enabled=0;tap(.5f,.5f);frame();assert(!accepted&&!releases);
 ready();serials[4]++;tap(.5f,.5f);frame();assert(!accepted&&!releases);
 ready();*(int*)(objects[2]+0x164)=0;tap(.5f,.5f);frame();assert(!accepted&&!releases);
 ready();property(2,28,8,0x160);*(void**)(objects[2]+0x160)=objects[4];tap(.5f,.5f);frame();assert(!accepted&&!releases);
 for(int mode=0;mode<3;mode++){
  ready();if(mode==0)*(unsigned short*)(functions[1]+0xb8)=0;
  if(mode==1)*(uint64_t*)(parameters[1]+0x40)=0x480;
  if(mode==2)*(uint64_t*)(parameters[2]+0x40)=0x80;
  tap(.5f,.5f);frame();assert(!accepted&&!releases);
 }
 ready();activation_enabled=0;tap(.5f,.5f);frame();assert(!accepted&&releases==1);
 ready();destroy_key=1;tap(.5f,.5f);frame();assert(accepted==1&&!releases);
 ready();destroy_theatre=1;tap(.5f,.5f);frame();assert(accepted==1&&!releases);
 ready();swap_pawn=1;tap(.5f,.5f);frame();assert(accepted==1&&releases==1);frame();assert(!tt_bound);
 for(int mode=0;mode<6;mode++){
  ready();if(mode==0)functions[0][0xb4]=2;if(mode==1)*(unsigned short*)(functions[0]+0xb6)=2;
  if(mode==2)*(unsigned short*)(functions[0]+0xb8)=0;if(mode==3)*(void**)(functions[0]+0x20)=classes[1];
  if(mode==4)*(int*)(parameters[0]+0x4c)=1;if(mode==5)parameters[0][0x7b]=2;
  tap(.5f,.5f);frame();assert(!accepted&&!releases);
 }
 puts("PASS: weak pawn/theatre/camera generation, child/component ownership, bounded arrays, disabled native keys, destructive callbacks and typed function/parameter refusals");
}
static void geometry_tests(void){
 ready();float o[]={-10,0,0},dir[]={1,0,0},extent[]={2,3,4};assert(fabsf(tt_box(o,dir,extent)-8)<.001f);o[1]=4;assert(tt_box(o,dir,extent)<0);
 o[1]=0;assert(fabsf(tt_capsule(o,dir,3,4)-7)<.001f);o[1]=3.1f;assert(tt_capsule(o,dir,3,4)<0);assert(tt_capsule(o,dir,-1,4)<0);
 float eye[3],ray[3];assert(tt_view_ray(view,.5f,.5f,2000,1000,eye,ray)&&fabsf(ray[0]-1)<.001f&&fabsf(ray[1])<.001f);
 *(float*)(view+0x10)=90;assert(tt_view_ray(view,.5f,.5f,2000,1000,eye,ray)&&fabsf(ray[1]-1)<.001f);*(float*)(view+0x10)=0;
 *(float*)(view+0xc)=30;assert(tt_view_ray(view,.5f,.5f,2000,1000,eye,ray)&&fabsf(ray[2]-.5f)<.001f);*(float*)(view+0xc)=0;
 view[0x30]=1;*(float*)(view+0x2c)=1;assert(!tt_view_ray(view,.1f,.5f,2000,1000,eye,ray));assert(tt_view_ray(view,.5f,.5f,2000,1000,eye,ray));
 view[0x31]=1;assert(!tt_view_ray(view,.5f,.5f,2000,1000,eye,ray));
 ready();*(float*)(objects[6]+0x1c0)=NAN;tap(.5f,.5f);frame();assert(!accepted);
 ready();*(float*)(objects[7]+0x430)=-1;tap(.65f,.5f);frame();assert(!accepted);
 ready();*(void**)(objects[1]+0x520)=objects[9];*(unsigned short*)(objects[9]+0x14c)=0x20;tap(.3f,.5f);frame();assert(cancels==1&&!accepted);
 ready();*(void**)(objects[1]+0x520)=objects[9];*(unsigned short*)(objects[9]+0x14c)=0x420;tap(.3f,.5f);frame();assert(!cancels&&!accepted);
 ready();*(void**)(objects[1]+0x528)=objects[10];*(unsigned short*)(objects[11]+0x14c)=0x20;tap(.2f,.5f);frame();assert(cancels==1&&!accepted);
 puts("PASS: final-view yaw/pitch/aspect ray, exact Box/Capsule hits, invalid shapes, original visible Cancel widget/bell target; no SaveGame call");
}
static void overwrite_ready(void){
 ready();key_array[0]=objects[3];*(int*)(objects[1]+0x508)=1;
 *(void**)(objects[1]+0x550)=objects[2];*(void**)(objects[2]+0x10)=classes[16];
 *(void**)(objects[6]+0x10)=classes[6];float extent[]={2,6,2};memcpy(objects[6]+0x430,extent,12);
}
static void overwrite_tests(void){
 overwrite_ready();unsigned char native_before[sizeof objects];memcpy(native_before,objects,sizeof objects);
 assert(tt_overwrite_key(objects[1])==objects[2]);tap(.5f,.5f);frame();
 assert(accepted==1&&releases==1&&!cancels&&!memcmp(native_before,objects,sizeof objects));
 assert(*(int*)(objects[1]+0x508)==1&&key_array[0]==objects[3]);
 // The separate original key follows exactly the ordinary native enabled/weak-child gates.
 for(int mode=0;mode<8;mode++){
  overwrite_ready();if(mode==0)enabled=0;if(mode==1)alive[2]=0;if(mode==2)serials[4]++;
  if(mode==3)*(void**)(objects[4]+0x20)=objects[14];if(mode==4)*(void**)(objects[4]+0x1f8)=objects[3];
  if(mode==5)*(void**)(objects[2]+0x10)=classes[3];if(mode==6)*(void**)(objects[6]+0xa0)=objects[3];
  if(mode==7)*(unsigned short*)(functions[1]+0xb8)=0;
  tap(.5f,.5f);frame();assert(!accepted&&!releases&&!cancels);
 }
 overwrite_ready();activation_enabled=0;tap(.5f,.5f);frame();assert(!accepted&&releases==1);
 overwrite_ready();destroy_key=1;tap(.5f,.5f);frame();assert(accepted==1&&!releases);
 puts("PASS: separate original Overwrite key excluded from AllKeys receives one native activation/release, enabled/activation/serial/foreign-child/exact-class/component/ABI guards and destructive callback; native array/objects unchanged");
}
#include "typewriter_overwrite_capture.h"
static void overwrite_face_ready(void){
 overwrite_ready();g_comp_w=2400;g_comp_h=1080;memcpy(view,tt_overwrite_pov,64);
 memcpy(objects[6]+0x1c0,tt_overwrite_magnet_tf,48);memcpy(objects[6]+0x430,tt_overwrite_extent,12);
 *(void**)(objects[1]+0x558)=objects[23];*(void**)(objects[23]+0x10)=classes[6];
 *(void**)(objects[23]+0x20)=*(void**)(objects[23]+0xa0)=objects[1];
 memcpy(objects[23]+0x1c0,tt_overwrite_button_tf,48);memcpy(objects[23]+0x430,tt_overwrite_extent,12);
 *(void**)(objects[1]+0x540)=objects[9];memcpy(objects[9]+0x1c0,tt_overwrite_widget_tf,48);
 *(unsigned short*)(objects[9]+0x14c)=0x4063;objects[1][0x5d0]=6;
}
static void overwrite_face_tests(void){
 overwrite_face_ready();float eye[3],ray[3];
 assert(tt_view_ray(view,848.0f/2400,539.0f/1080,2400,1080,eye,ray));
 assert(tt_shape(objects[6],objects[2],eye,ray)<0);
 float face=tt_shape(objects[23],objects[1],eye,ray);assert(face>94.15f&&face<94.17f);
 unsigned char before[sizeof objects];memcpy(before,objects,sizeof objects);
 tap(848.0f/2400,539.0f/1080);frame();assert(accepted==1&&releases==1&&!cancels&&!memcmp(before,objects,sizeof objects));
 for(int mode=0;mode<11;mode++){
  overwrite_face_ready();if(mode==0)*(unsigned short*)(objects[9]+0x14c)|=0x400;
  if(mode==1)*(void**)(objects[9]+0x20)=objects[14];if(mode==2)*(void**)(objects[9]+0xa0)=objects[14];
  if(mode==3)*(void**)(objects[23]+0x20)=objects[14];if(mode==4)*(void**)(objects[23]+0xa0)=objects[14];
  if(mode==5)enabled=0;if(mode==6)*(void**)(objects[23]+0x10)=classes[7];
  if(mode==7)*(float*)(objects[23]+0x430)=NAN;if(mode==8)*(void**)(objects[2]+0x10)=classes[3];
  if(mode==9)serials[4]++;if(mode==10)*(void**)(objects[1]+0x558)=0;
  tap(848.0f/2400,539.0f/1080);frame();assert(!accepted&&!releases&&!cancels);
 }
 g_comp_w=2000;g_comp_h=1000;
 puts("PASS: exact frozen p371 tap848,539 misses elevated VR magnet but hits original authored button; one native press/release, visible owned widget/exact owned key/native enabled/owned Box guards, immutable native objects and no-collider counterfactual");
}
#include "typewriter_touch_capture.h"
static void captured_shape(unsigned index){
 assert(index<sizeof tt_capture_keys/sizeof *tt_capture_keys);
 const float* shape=tt_capture_keys[index].shape;
 memcpy(objects[6]+0x1c0,tt_capture_keys[index].tf,48);
 *(void**)(objects[6]+0x10)=classes[tt_capture_keys[index].box?6:5];
 property(4,30,4,0x424);property(5,30,4,0x420);property(6,31,12,0x420);
 if(tt_capture_keys[index].box)memcpy(objects[6]+0x420,shape,12);
 else{memcpy(objects[6]+0x424,shape,4);memcpy(objects[6]+0x420,shape+1,4);}
}
static void captured_tests(void){
 for(unsigned touch=0;touch<sizeof tt_capture_touches/sizeof *tt_capture_touches;touch++){
  ready();g_comp_w=2400;g_comp_h=1080;memcpy(view,tt_capture_view,sizeof view);
  float eye[3],ray[3];assert(tt_view_ray(view,tt_capture_touches[touch].x/2400,tt_capture_touches[touch].y/1080,2400,1080,eye,ray));
  float best=401;int found=-1;
  for(unsigned key=0;key<sizeof tt_capture_keys/sizeof *tt_capture_keys;key++){
   captured_shape(key);float hit=tt_shape(objects[6],objects[2],eye,ray);
   if(hit>=0&&hit<best){best=hit;found=(int)key;}
  }
  if(found!=tt_capture_touches[touch].key)fprintf(stderr,"capture touch %u expected %s found %s\n",touch,tt_capture_keys[tt_capture_touches[touch].key].name,found<0?"none":tt_capture_keys[found].name);
  assert(found==tt_capture_touches[touch].key);
  captured_shape((unsigned)found);*(int*)(objects[1]+0x508)=1;
  *(uint64_t*)(parameters[0]+0x40)=0x1040000294ULL;
  *(uint64_t*)(parameters[1]+0x40)=0x1040000380ULL;
  *(uint64_t*)(parameters[2]+0x40)=0x8001040080380ULL;
  tap(tt_capture_touches[touch].x/2400,tt_capture_touches[touch].y/1080);frame();assert(accepted==1&&releases==1);
  ready();g_comp_w=2000;g_comp_h=1000;
 }
 puts("PASS: exact frozen p347 final camera and all41 native shapes select Q/A/Space/Back at failed physical tap coordinates; real weak-parent and Blueprint OUT-parameter ABI accepts press/release");
}
#include "typewriter_frame_capture.h"
static void captured_widget(unsigned data,int object){
 memcpy(objects[object]+0x1c0,tt_capture_widgets[data].tf,48);
 memcpy(widget_relative+object,tt_capture_widgets[data].tf,48);*(void**)(objects[object]+0xc0)=objects[23];*(void**)(objects[object]+0x10)=classes[9];
 *(void**)(objects[object]+0x20)=*(void**)(objects[object]+0xa0)=objects[1];
 memcpy(objects[object]+0x45c,tt_capture_widgets[data].pivot,8);
 memcpy(draw_sizes+object,tt_capture_widgets[data].size,8);*(unsigned short*)(objects[object]+0x14c)=tt_capture_widgets[data].flags;
}
static int project_view(const unsigned char* camera,const float p[3],float* u,float* v){
 float eye[3],f[3],r[3],top[3],edge[3],d[3],depth=0,x=0,y=0;
 if(!tt_view_ray(camera,.5f,.5f,g_comp_w,g_comp_h,eye,f)||!tt_view_ray(camera,1,.5f,g_comp_w,g_comp_h,eye,edge))return 0;
 float tangent=tanf(*(const float*)(camera+0x18)*.00872664625997165f),n=0;
 for(unsigned k=0;k<3;k++){r[k]=edge[k]-f[k]*(edge[0]*f[0]+edge[1]*f[1]+edge[2]*f[2]);n+=r[k]*r[k];}
 n=sqrtf(n);for(unsigned k=0;k<3;k++)r[k]/=n;
 top[0]=f[1]*r[2]-f[2]*r[1];top[1]=f[2]*r[0]-f[0]*r[2];top[2]=f[0]*r[1]-f[1]*r[0];
 for(unsigned k=0;k<3;k++){d[k]=p[k]-eye[k];depth+=d[k]*f[k];x+=d[k]*r[k];y+=d[k]*top[k];}
 if(depth<=0)return 0;*u=.5f+.5f*x/(depth*tangent);*v=.5f-.5f*y*g_comp_w/(depth*tangent*g_comp_h);return 1;
}
static void captured_theatre(void){
 ready();g_comp_w=2400;g_comp_h=1080;memcpy(view,tt_capture_view,sizeof view);
 captured_widget(0,16);captured_widget(1,17);captured_widget(2,9);
 *(void**)(objects[1]+0x530)=objects[16];*(void**)(objects[1]+0x538)=objects[17];*(void**)(objects[1]+0x520)=objects[9];
 captured_shape(0);memcpy(objects[7]+0x1c0,tt_capture_keys[37].tf,48);memcpy(objects[7]+0x420,tt_capture_keys[37].shape,12);
}
static void captured_overwrite(void){
 captured_theatre();key_array[0]=objects[3];*(int*)(objects[1]+0x508)=1;
 *(void**)(objects[1]+0x550)=objects[2];*(void**)(objects[2]+0x10)=classes[16];
}
static void frame_tests(void){
 ready();g_comp_w=2400;g_comp_h=1080;memcpy(view,tt_capture_view,sizeof view);
 struct tt_frame_bounds bounds={0};
 for(unsigned i=0;i<sizeof tt_capture_keys/sizeof *tt_capture_keys;i++){
  captured_shape(i);assert(tt_frame_shape(&bounds,objects[6],objects[2]));
 }
 for(unsigned i=0;i<sizeof tt_capture_widgets/sizeof *tt_capture_widgets;i++){
  captured_widget(i,9);assert(tt_frame_widget(&bounds,objects[9],objects[1])==1);
 }
 assert(bounds.count==45*8);
 float old_u,old_v;assert(project_view(view,tt_capture_widgets[0].tf+4,&old_u,&old_v)&&old_v<0);
 unsigned char original[64],framed[64];memcpy(original,view,64);
 assert(tt_frame_view(view,&bounds,g_comp_w,g_comp_h,0));memcpy(framed,view,64);assert(!memcmp(view+0x18,original+0x18,64-0x18));
 for(unsigned c=0;c<bounds.count;c++){
  float u,v;assert(project_view(view,bounds.points[c],&u,&v)&&u>=.049f&&u<=.951f&&v>=.049f&&v<=.951f);
 }
 for(unsigned key=0;key<sizeof tt_capture_keys/sizeof *tt_capture_keys;key++){
  float u,v,eye[3],ray[3];assert(project_view(view,tt_capture_keys[key].tf+4,&u,&v));
  assert(u>0&&u<1&&v>0&&v<1&&tt_view_ray(view,u,v,g_comp_w,g_comp_h,eye,ray));
  int found=-1;float best=401;
  for(unsigned candidate=0;candidate<sizeof tt_capture_keys/sizeof *tt_capture_keys;candidate++){
   captured_shape(candidate);float hit=tt_shape(objects[6],objects[2],eye,ray);
   if(hit>=0&&hit<best){found=(int)candidate;best=hit;}
  }
  if(found!=(int)key)fprintf(stderr,"framed key %s selected %s\n",tt_capture_keys[key].name,found<0?"none":tt_capture_keys[found].name);
  assert(found==(int)key);
 }
 for(unsigned i=0;i<20;i++){assert(tt_frame_view(view,&bounds,g_comp_w,g_comp_h,0));for(unsigned k=0;k<6;k++)assert(fabsf(((float*)view)[k]-((float*)framed)[k])<.002f);}
 for(int mode=0;mode<8;mode++){
  memcpy(view,original,64);struct tt_frame_bounds bad=bounds;
  if(mode==0)bad.count=0;if(mode==1)bad.hi[0]=NAN;if(mode==2)bad.lo[0]=bad.hi[0]+1;
  if(mode==3)bad.hi[0]=bad.lo[0]+401;if(mode==4)view[0x31]=1;if(mode==5)*(float*)(view+0x18)=0;
  if(mode==6){view[0x30]=1;*(float*)(view+0x2c)=NAN;}if(mode==7)*(float*)view=NAN;
  unsigned char before[64];memcpy(before,view,64);assert(!tt_frame_view(view,&bad,g_comp_w,g_comp_h,0)&&!memcmp(before,view,64));
 }
 // Exercise the actual callback: framing precedes this exact final-view ray and original key delegate.
 captured_theatre();unsigned char untouched[sizeof objects];memcpy(untouched,objects,sizeof objects);frame();assert(!calls&&!memcmp(untouched,objects,sizeof objects));float u,v;assert(project_view(view,tt_capture_keys[0].tf+4,&u,&v));
 assert(u>0&&u<1&&v>0&&v<1);tap(u,v);frame();assert(accepted==1&&releases==1);
 captured_theatre();frame();assert(project_view(view,tt_capture_widgets[2].tf+4,&u,&v));tap(u,v);frame();assert(cancels==1&&!accepted);
 for(int mode=0;mode<10;mode++){
  captured_theatre();unsigned char before[64];memcpy(before,view,64);
  if(mode==0)context=4;if(mode==1)gt=0;if(mode==2)*(void**)(objects[8]+0xa0)=objects[13];
  if(mode==3)*(void**)(objects[16]+0xa0)=objects[14];if(mode==4)*(void**)(objects[17]+0x20)=objects[14];
  if(mode==5)objects[16][0x429]=1;if(mode==6)objects[17][0x518]=1;if(mode==7)draw_sizes[16].x=NAN;
  if(mode==8)*(float*)(objects[6]+0x1c0)=NAN;if(mode==9)*(void**)(objects[4]+0x1f8)=objects[3];
  frame();assert(!memcmp(before,view,64)&&!accepted&&!cancels);
 }
 captured_overwrite();frame();assert(project_view(view,tt_capture_keys[0].tf+4,&u,&v));
 assert(u>0&&u<1&&v>0&&v<1);tap(u,v);frame();assert(accepted==1&&releases==1);
 // The non-typing fit validates the separate owned collision; state9 excludes it from the close-up.
 captured_overwrite();unsigned char before_overwrite[64];memcpy(before_overwrite,view,64);
 *(float*)(objects[6]+0x1c0)=NAN;frame();assert(!memcmp(before_overwrite,view,64));
 objects[1][0x5d0]=9;frame();assert(fabsf(*(float*)(view+0xc)+50)<.01f);
 ready();g_comp_w=2000;g_comp_h=1000;
 puts("PASS: captured offscreen paper reproducer; all41 original colliders+4 rendered quads fit and remain individually hittable, FOV/native objects unchanged, no frame drift/native calls; actual callback/framed key/Cancel and unsafe geometry/context/ownership guards");
}
static void hand_visibility_tests(void){
 for(int mode=0;mode<8;mode++){
  ready();*(void**)(objects[0]+0xba8)=objects[18];*(void**)(objects[0]+0x130)=objects[19];
  *(void**)(objects[18]+0x10)=classes[14];*(void**)(objects[18]+0x220)=objects[20];
  *(void**)(objects[18]+0x228)=objects[21];*(void**)(objects[18]+0x230)=objects[22];
  *(void**)(objects[19]+0x20)=*(void**)(objects[19]+0xa0)=objects[0];
  *(void**)(objects[20]+0x20)=*(void**)(objects[20]+0xa0)=objects[18];*(void**)(objects[20]+0xc0)=objects[19];
  for(int i=21;i<=22;i++){*(void**)(objects[i]+0x10)=classes[15];*(void**)(objects[i]+0x20)=*(void**)(objects[i]+0xa0)=objects[18];*(void**)(objects[i]+0xc0)=objects[20];*(unsigned short*)(objects[i]+0x14c)=0x20;}
  if(mode==1)alive[18]=0;if(mode==2)*(void**)(objects[20]+0xc0)=objects[23];
  if(mode==3)*(void**)(objects[19]+0xa0)=objects[23];if(mode==4)*(void**)(objects[18]+0x10)=classes[7];
  if(mode==5)*(void**)(objects[21]+0x20)=objects[23];if(mode==6)*(void**)(objects[21]+0xc0)=objects[23];
  if(mode==7)*(void**)(objects[21]+0x10)=classes[7];
  tt_hide_hands(objects[0]);assert(hidden_hands==(mode==0?2:mode>=5?1:0));
  if(mode==0){tt_hide_hands(objects[0]);assert(hidden_hands==2);*(unsigned short*)(objects[21]+0x14c)|=0x20;tt_hide_hands(objects[0]);assert(hidden_hands==3);}
 }
 puts("PASS: typewriter exact owned mesh leaves hidden without propagation, stale/wrong ownership/class rejected, native visibility refresh handled");
}
static void keyboard_closeup_tests(void){
 ready();g_comp_w=2400;g_comp_h=1080;memcpy(view,tt_capture_view,64);
 struct tt_frame_bounds bounds={0};
 for(unsigned i=0;i<sizeof tt_capture_keys/sizeof *tt_capture_keys;i++){captured_shape(i);assert(tt_frame_shape(&bounds,objects[6],objects[2]));}
 unsigned char original[64],closeup[64];memcpy(original,view,64);assert(tt_frame_view(view,&bounds,g_comp_w,g_comp_h,1));memcpy(closeup,view,64);
 float left=1,right=0,top=1,bottom=0;
 for(unsigned i=0;i<sizeof tt_capture_keys/sizeof *tt_capture_keys;i++){
  float u,v;assert(project_view(view,tt_capture_keys[i].tf+4,&u,&v));
  left=fminf(left,u);right=fmaxf(right,u);top=fminf(top,v);bottom=fmaxf(bottom,v);
  float eye[3],ray[3],best=401;int found=-1;assert(tt_view_ray(view,u,v,g_comp_w,g_comp_h,eye,ray));
  for(unsigned candidate=0;candidate<sizeof tt_capture_keys/sizeof *tt_capture_keys;candidate++){
   captured_shape(candidate);float hit=tt_shape(objects[6],objects[2],eye,ray);if(hit>=0&&hit<best){found=(int)candidate;best=hit;}
  }
  assert(found==(int)i);
 }
 assert(right-left>.40f&&bottom-top>.30f&&fabsf(*(float*)(view+0xc)+50)<.01f);
 assert(!memcmp(view+0x18,original+0x18,64-0x18));
 for(unsigned i=0;i<20;i++){assert(tt_frame_view(view,&bounds,g_comp_w,g_comp_h,1));for(unsigned k=0;k<6;k++)assert(fabsf(((float*)view)[k]-((float*)closeup)[k])<.003f);}
 // Every original typing state uses the same close-up; existing-slot selection state6 retains the broad view.
 captured_theatre();objects[1][0x5d0]=9;frame();assert(fabsf(*(float*)(view+0xc)+50)<.01f);
 for(int state=7;state<=14;state++){if(state!=7&&state!=8&&state!=9&&state!=14)continue;memcpy(view,tt_capture_view,64);objects[1][0x5d0]=(unsigned char)state;frame();assert(fabsf(*(float*)(view+0xc)+50)<.01f);}
 memcpy(view,tt_capture_view,64);objects[1][0x5d0]=6;frame();assert(fabsf(*(float*)(view+0xc)+50)>1);
 g_comp_w=2000;g_comp_h=1000;
 puts("PASS: captured typing keyboard fills >40% width/>30% height, all41 keys remain separately hittable/Cancel fits, 20 stable frames, all original typing states7/8/9/14 close-up/state6 readable paper, FOV retained");
}
static void native_key_animation_tests(void){
 for(int rotating=0;rotating<=1;rotating++){
  ready();*(unsigned*)(classes[3]+0x18)=rotating?47:46;
  function(4,rotating?49:48,classes[3],4,0,30);*(void**)(functions[2]+0x28)=functions[4];
  *(float*)(objects[2]+(rotating?0x714:0x710))=rotating?8:.8f;
  tap(.5f,.5f);frame();assert(accepted==1&&releases==1&&visual_calls==1&&tt_animation_started);
  float first=visual_amount;assert(fabsf(first-*(float*)(objects[2]+(rotating?0x714:0x710)))<.00001f);
  clock_ms+=90;touch_typewriter_view(objects[8],view);assert(fabsf(visual_amount-first*.5f)<.00001f&&accepted==1&&releases==1);
  clock_ms+=90;touch_typewriter_view(objects[8],view);assert(visual_amount==0&&!tt_animation_started&&accepted==1);
  tap(.5f,.5f);frame();assert(accepted==2&&tt_animation_started);context=4;frame();assert(!tt_animation_started&&visual_amount==0);
  ready();*(unsigned*)(classes[3]+0x18)=rotating?47:46;
  function(4,rotating?49:48,classes[3],4,0,30);*(void**)(functions[2]+0x28)=functions[4];
  *(float*)(objects[2]+(rotating?0x714:0x710))=1;tap(.5f,.5f);frame();assert(tt_animation_started);
  int rendered=visual_calls;serials[2]++;frame();assert(!tt_animation_started&&visual_calls==rendered);
 }
 ready();*(float*)(objects[2]+0x710)=NAN;tap(.5f,.5f);frame();assert(accepted==1&&!visual_calls&&!tt_animation_started);
 puts("PASS: actual native translating/rotating AnimBP writers use original threshold, 180ms smooth return, no duplicate delegate, context reset and weak serial rejection");
}
static struct reload_tf socket_surface(const void* mesh,unsigned long long socket,int space){
 assert(mesh==objects[23]&&(socket==101||socket==102)&&space==0);return key_surfaces[socket-101];
}
static void setup_surface(void){
 ready();tt_socket=socket_surface;table[0x430/sizeof(void*)]=(void*)socket_surface;
 *(void**)(objects[1]+0x730)=objects[23];*(void**)(objects[23]+0x10)=classes[15];*(void**)(objects[23]+0x20)=*(void**)(objects[23]+0xa0)=objects[1];
 function(5,51,classes[2],16,1,28);functions[5][0xb4]=2;*(int*)(parameters[5]+0x38)=8;
 *(unsigned*)(parameters[5]+0x28)=19;
 property(29,52,8,8);*(uint64_t*)(fields[29]+0x40)=0x180;*(unsigned*)(fields[29]+0x28)=53;
 *(void**)(parameters[5]+0x20)=fields[29];*(void**)(functions[1]+0x28)=functions[5];
 for(int i=0;i<2;i++){
  *(void**)(objects[6+i]+0x10)=classes[5];*(float*)(objects[6+i]+0x430)=1;*(float*)(objects[6+i]+0x434)=2;
  float normal[3]={0,0,1};memcpy(objects[2+i]+0x740,normal,12);
  key_surfaces[i]=(struct reload_tf){{0,.70710678f,0,.70710678f},{100,3.5f*i,-12},{1,1,1},{0,0}};
 }
}
static void surface_key_tests(void){
 setup_surface();float eye[3]={0},ray[3]={100,0,-12},n=sqrtf(10144),score;
 for(int k=0;k<3;k++)ray[k]/=n;
 assert(tt_shape(objects[6],objects[2],eye,ray)<0);
 assert(tt_key_surface(objects[2],objects[1],objects[6],eye,ray,&score)>0&&score<.0001f);
 // Overlapping flat targets select their own nearest cap, independent of array order.
 for(int side=0;side<2;side++){
  ray[0]=100;ray[1]=side?1.8f:1.7f;ray[2]=-12;n=sqrtf(ray[0]*ray[0]+ray[1]*ray[1]+ray[2]*ray[2]);for(int k=0;k<3;k++)ray[k]/=n;
  float best=2;int found=-1;
  for(int i=1;i>=0;i--){float s=2;if(tt_key_surface(objects[2+i],objects[1],objects[6+i],eye,ray,&s)>=0&&s<best){best=s;found=i;}}
  assert(found==side);
 }
 for(int mode=0;mode<6;mode++){
  setup_surface();ray[0]=1;ray[1]=0;ray[2]=-.12f;n=sqrtf(1.0144f);for(int k=0;k<3;k++)ray[k]/=n;
  if(mode==0)*(void**)(objects[23]+0x20)=objects[0];if(mode==1)table[0x430/sizeof(void*)]=0;
  if(mode==2)*(int*)(fields[29]+0x4c)=0;if(mode==3)*(uint64_t*)(parameters[5]+0x40)=0x80;
  if(mode==4)*(float*)(objects[2]+0x740)=NAN;if(mode==5)*(void**)(objects[6]+0xa0)=objects[3];
  assert(tt_key_surface(objects[2],objects[1],objects[6],eye,ray,&score)<0);
 }
 puts("PASS: original rendered surface socket fixes cap ray missing elevated VR capsule; nearest cap wins overlaps; exact two-OUT getter ABI, owned mesh/collider, native getter and normal guards");
}
static void setup_list(void){
 ready();objects[1][0x5d0]=6;
 *(void**)(objects[1]+0x538)=objects[17];*(void**)(objects[17]+0x10)=classes[9];
 *(void**)(objects[17]+0x20)=*(void**)(objects[17]+0xa0)=objects[1];*(unsigned short*)(objects[17]+0x14c)=0x20;
 transform(17,100,0,0);draw_sizes[17]=(struct cinema_ui_vec2){550,160};*(float*)(objects[17]+0x45c)=*(float*)(objects[17]+0x460)=.5f;
 *(void**)(objects[1]+0x750)=objects[19];*(void**)(objects[19]+0x10)=classes[17];*(void**)(objects[19]+0x20)=objects[1];
 *(unsigned*)(classes[17]+0x18)=59;*(int*)(classes[17]+0x58)=0x60;*(void**)(objects[19]+0x30)=objects[20];
 *(void**)(objects[20]+0x10)=classes[18];*(unsigned*)(classes[18]+0x18)=61;*(int*)(classes[18]+0x58)=0x1800;
 *(void**)(objects[20]+0x130)=objects[21];*(void**)(objects[1]+0x758)=objects[22];*(void**)(objects[1]+0x760)=objects[23];
 for(int i=21;i<=23;i++)*(void**)(objects[i]+0x20)=*(void**)(objects[i]+0xa0)=i==21?objects[20]:objects[1];
 function(6,56,classes[1],1,0,32);function(7,65,classes[1],8,0,28);function(8,66,classes[1],8,0,28);
 *(void**)(functions[3]+0x28)=functions[6];*(void**)(functions[6]+0x28)=functions[7];*(void**)(functions[7]+0x28)=functions[8];
}
static void list_ray(int row,float ray[3]){ray[0]=100;ray[1]=0;ray[2]=80-(row*32+16);float n=sqrtf(ray[0]*ray[0]+ray[2]*ray[2]);for(int k=0;k<3;k++)ray[k]/=n;}
static void list_selection_tests(void){
 float eye[3]={0},ray[3];
 for(int row=0;row<5;row++){
  setup_list();list_ray(row,ray);assert(tt_list_target(objects[1],eye,ray,clock_ms));
  if(row==2){assert(!scroll_calls&&!tt_scroll.remaining);continue;}
  assert(scroll_calls==1&&scroll_forward==(row>2)&&objects[1][0x5d0]==0&&objects[1][0x729]);
  assert(tt_scroll.remaining==(row==0?-1:row==4?1:0));
  tt_scroll_tick(objects[1],clock_ms+100);assert(scroll_calls==1);
  objects[1][0x729]=0;tt_scroll_tick(objects[1],clock_ms+150);assert(scroll_calls==1); // native state reevaluation still pending
  objects[1][0x5d0]=7;tt_scroll_tick(objects[1],clock_ms+200);
  assert(scroll_calls==(row==0||row==4?2:1)&&!tt_scroll.remaining);
 }
 for(int mode=0;mode<9;mode++){
  setup_list();list_ray(3,ray);
  if(mode==0)objects[1][0x5d0]=9;if(mode==1)*(unsigned short*)(objects[17]+0x14c)=0;
  if(mode==2)*(void**)(objects[17]+0xa0)=objects[0];if(mode==3)draw_sizes[17].y=200;
  if(mode==4)*(void**)(objects[19]+0x20)=objects[0];if(mode==5)*(int*)(fields[31]+0x4c)=0;
  if(mode==6)*(void**)(objects[21]+0xa0)=objects[1];if(mode==7)*(unsigned short*)(functions[8]+0xb6)=4;
  if(mode==8)*(float*)(objects[22]+0x1c0)=NAN;
  tt_list_target(objects[1],eye,ray,clock_ms);assert(!scroll_calls);
 }
 setup_list();list_ray(4,ray);assert(tt_list_target(objects[1],eye,ray,clock_ms)&&tt_scroll.remaining==1);
 context=4;frame();assert(!tt_scroll.remaining&&scroll_calls==1);
 setup_list();list_ray(0,ray);assert(tt_list_target(objects[1],eye,ray,clock_ms)&&tt_scroll.remaining==-1);
 serials[1]++;tt_scroll_tick(objects[1],clock_ms+400);assert(!tt_scroll.remaining&&scroll_calls==1);
 puts("PASS: all five native list rows replay original dial grab/ScrollList/release, async native0/ScrollActive wait and two-step queue; typing, hidden/foreign paper, hand/helper/components, malformed release, context and serial guards; no slot/state/save writes");
}
static void aligned_keyboard_tests(void){
 captured_theatre();objects[1][0x5d0]=9;
 *(void**)(objects[1]+0x720)=objects[15];*(void**)(objects[15]+0x10)=classes[7];
 *(void**)(objects[15]+0x20)=*(void**)(objects[15]+0xa0)=objects[1];
 memcpy(objects[15]+0x1c0,tt_capture_keys[0].tf,48);
 frame();float expected=atan2f(2*(tt_capture_keys[0].tf[3]*tt_capture_keys[0].tf[2]),1-2*tt_capture_keys[0].tf[2]*tt_capture_keys[0].tf[2])*57.2957795130823f;
 expected+=180;if(expected>180)expected-=360;
 assert(fabsf(*(float*)(view+0x10)-expected)<.001f);
 // Original ProcessKey writes WidgetList: every rendered paper corner stays inside the typing frame.
 struct tt_frame_bounds paper={0};assert(tt_frame_widget(&paper,objects[17],objects[1])==1);
 for(unsigned c=0;c<paper.count;c++){float u,v;assert(project_view(view,paper.points[c],&u,&v)&&u>.049f&&u<.951f&&v>.049f&&v<.951f);}
 float oldyaw=*(float*)(view+0x10);*(float*)(view+4)+=30;*(float*)(view+0x10)=70;frame();assert(fabsf(*(float*)(view+0x10)-oldyaw)<.001f);
 g_comp_w=2000;g_comp_h=1000;
 puts("PASS: owned authored TypewriterRoot centers front-facing keyboard independent of entering camera angle; original typed-text WidgetList remains framed");
}
static void combined_typing_layout_tests(void){
 captured_theatre();objects[1][0x5d0]=9;
 struct tt_frame_bounds keys={0};
 for(unsigned i=0;i<sizeof tt_capture_keys/sizeof *tt_capture_keys;i++){captured_shape(i);assert(tt_frame_shape(&keys,objects[6],objects[2]));}
 const float* rotation=tt_capture_keys[0].tf;q_rot((qf_t){rotation[0],rotation[1],rotation[2],rotation[3]},(float[3]){-1,0,0},keys.facing);keys.aligned=1;
 assert(tt_frame_view(view,&keys,g_comp_w,g_comp_h,1));
 unsigned char composed[64];memcpy(composed,view,64);
 struct reload_tf originals[3];const int widget_ids[]={16,17,9};
 for(unsigned i=0;i<3;i++)memcpy(originals+i,objects[widget_ids[i]]+0x1c0,sizeof *originals);
 assert(tt_typing_widgets_place(objects[1],view)&&widget_world_writes==3);
 float left=1,right=0,top=1,bottom=0;
 for(unsigned i=0;i<keys.count;i++){
  float u,v;assert(project_view(view,keys.points[i],&u,&v));
  left=fminf(left,u);right=fmaxf(right,u);top=fminf(top,v);bottom=fmaxf(bottom,v);
  assert(u>=.2249f&&u<=.7751f&&v>=.3599f&&v<=.9401f);
 }
 assert(right-left>=.50f&&bottom-top>=.50f);
 for(unsigned i=0;i<sizeof tt_capture_keys/sizeof *tt_capture_keys;i++){
  float u,v,eye[3],ray[3],best=401;int found=-1;
  assert(project_view(view,tt_capture_keys[i].tf+4,&u,&v)&&tt_view_ray(view,u,v,g_comp_w,g_comp_h,eye,ray));
  for(unsigned candidate=0;candidate<sizeof tt_capture_keys/sizeof *tt_capture_keys;candidate++){
   captured_shape(candidate);float hit=tt_shape(objects[6],objects[2],eye,ray);if(hit>=0&&hit<best){best=hit;found=(int)candidate;}
  }
  assert(found==(int)i);
 }
 for(unsigned i=0;i<3;i++){
  struct tt_frame_bounds quad={0};assert(tt_frame_widget(&quad,objects[widget_ids[i]],objects[1])==1);
  float lo_u=1,hi_u=0,lo_v=1,hi_v=0;
  for(unsigned c=0;c<quad.count;c++){
   float u,v;assert(project_view(view,quad.points[c],&u,&v));lo_u=fminf(lo_u,u);hi_u=fmaxf(hi_u,u);lo_v=fminf(lo_v,v);hi_v=fmaxf(hi_v,v);
  }
  assert(lo_u>.02f&&hi_u<.98f&&lo_v>.02f&&hi_v<.98f);
  if(i<2)assert(hi_v<top-.04f);else assert(lo_u>right+.04f);
  if(i==1)assert(hi_u-lo_u>.35f&&hi_v-lo_v>.22f);
 }
 float eye[3],ray[3];assert(tt_view_ray(view,.90f,.87f,g_comp_w,g_comp_h,eye,ray));assert(tt_cancel_target(objects[1],eye,ray)>=0);
 // Longer native text changes only its own quad scale, never the keyboard camera or key targets.
 draw_sizes[17]=(struct cinema_ui_vec2){1100,500};
 for(unsigned i=0;i<20;i++){assert(tt_typing_widgets_place(objects[1],view));assert(!memcmp(view,composed,64));}
 int before=widget_restores;tt_typing_widgets_restore();assert(widget_restores==before+3);
 for(unsigned i=0;i<3;i++)assert(!memcmp(originals+i,objects[widget_ids[i]]+0x1c0,sizeof *originals));
 printf("PASS: actual captured41-key+Title+typedList+Cancel composition: keyboard width%.2f%% height%.2f%%; all41 exact hit targets, native text >35%% width/no overlap, original Cancel hit,20 growing-text frames keep camera fixed; exact relative restore\n",100*(right-left),100*(bottom-top));
 // Reparenting or weak reuse cannot restore an old local pose onto another native widget.
 captured_theatre();assert(tt_typing_widgets_place(objects[1],view));before=widget_restores;serials[17]++;tt_typing_widgets_restore();assert(widget_restores==before+2);
 captured_theatre();assert(tt_typing_widgets_place(objects[1],view));before=widget_restores;*(void**)(objects[17]+0xc0)=objects[20];tt_typing_widgets_restore();assert(widget_restores==before+2);
 captured_theatre();objects[1][0x5d0]=9;frame();assert(tt_typing_widgets[1].active);context=4;frame();assert(!tt_typing_widgets[1].active);
 g_comp_w=2000;g_comp_h=1000;
}
#ifdef TYPEWRITER_INSTALL_FIXTURE
#include TYPEWRITER_INSTALL_FIXTURE
#endif
int main(void){queue_tests();ownership_tests();geometry_tests();overwrite_tests();overwrite_face_tests();captured_tests();frame_tests();hand_visibility_tests();keyboard_closeup_tests();native_key_animation_tests();surface_key_tests();list_selection_tests();aligned_keyboard_tests();combined_typing_layout_tests();
#ifdef TYPEWRITER_INSTALL_FIXTURE
 verify_installer();
#endif
 return 0;}
