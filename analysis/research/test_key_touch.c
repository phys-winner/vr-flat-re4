#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#ifndef RE4_KEY_TOUCH_ASM_CHECK
#define RE4_KEY_TOUCH_TEST
#endif
struct cinema_ui_weak {int index,serial;};
struct __attribute__((aligned(16))) pv_scene_tf {float q[4],p[3],s[3],padding[2];};
enum {PAWN,MANAGER,ACTOR,HAND,CONTROLLER,INPUT,WORLD,ROOT,HEAD,BOX,OTHER,OBJECTS};
static unsigned char storage[OBJECTS][0x1000],classes[OBJECTS][0x100];
static int serial[OBJECTS],sizes[OBJECTS],class_sizes[OBJECTS],paused,valid,game_thread,facing,body_valid;
static const void* dead;
static const void* wrong_world_actor;
static int g_touch_context=1,g_app_input_focus=1,g_touch_cancel;
static int ti_ready=1;
static long now_ms;
static void* object(int i){return storage[i];}
static int index_of(const void* p){for(int i=0;i<OBJECTS;i++)if(p==object(i))return i;return -1;}
static void weak_assign(struct cinema_ui_weak* w,const void* p){w->index=index_of(p);w->serial=w->index>=0?serial[w->index]:0;}
static void* weak_get(const struct cinema_ui_weak* w){return w->index>=0&&w->index<OBJECTS&&w->serial==serial[w->index]?object(w->index):0;}
static void (*cu_weak_assign)(struct cinema_ui_weak*,const void*)=weak_assign;
static void* (*cu_weak_get)(const struct cinema_ui_weak*)=weak_get;
static int pv_alive(const void* p){return p!=dead&&index_of(p)>=0;}
static int menu_visual_layout(const void* p,const void* cls,int minimum){
    int expected=-1;for(int k=0;k<OBJECTS;k++)if(cls==classes[k])expected=k;
    int i=index_of(p);return i>=0 && expected>=0 && class_sizes[expected]>=minimum
        && sizes[i]>=class_sizes[expected] && sizes[i]>=minimum
        && (cls==classes[i] || (i>=ROOT&&i<=BOX&&cls==classes[ROOT]));
}
static int interact_layout(const void* p,int minimum){int i=index_of(p);return i>=0&&sizes[i]>=minimum;}
static int cinema_on_game_thread(void){return game_thread;}
static long ptr_now_ms(void){return now_ms;}
static unsigned player_type(void){return 1;}
static unsigned (*phone_type)(void)=player_type;
static void* active(void){return object(PAWN);}
static void* (*phone_active)(void)=active;
static void* hand_player(const void* hand){return hand==object(HAND)?object(PAWN):object(OTHER);}
static void* (*phone_player)(const void*)=hand_player;
static int is_paused(void){return paused;}
static int is_valid(void){return valid;}
static int (*rl_paused)(void)=is_paused,(*rl_player_valid)(void)=is_valid;
static void* get_hand(const void* pawn,unsigned char side){return pawn==object(PAWN)&&side==1?object(HAND):0;}
static void* (*ti_get_hand)(const void*,unsigned char)=get_hand;
static int interact_bound_hand(void* hand,void* pawn,int side){return hand==object(HAND)&&pawn==object(PAWN)&&side==1&&storage[HAND][0x314]==1;}
static int interact_body_origin(void* pawn,float body[3]){memset(body,0,12);return pawn==object(PAWN)&&body_valid&&ti_ready;}
static void* manager(void){return object(MANAGER);}
static void* (*ti_manager)(void)=manager;
static void* manager_class(void){return classes[MANAGER];}
static void* scene_class(void){return classes[ROOT];}
static void* (*ti_manager_class)(void)=manager_class,*(*ti_scene_class)(void)=scene_class;
static void* receptor_class(void){return classes[ACTOR];}
static void* box_class(void){return classes[BOX];}
static void* controller_class(void){return classes[CONTROLLER];}
static void* world(const void* p){return object(p==wrong_world_actor?OTHER:WORLD);}
static void* weapon_hand(const void* p){return p==object(PAWN)?object(HAND):0;}
static struct pv_scene_tf head_tf(const void* p){assert(p==object(PAWN));struct pv_scene_tf tf;memcpy(&tf,storage[HEAD]+0x1c0,sizeof tf);return tf;}
static struct pv_scene_tf (*pv_head_transform)(const void*)=head_tf;
static int pv_tf_finite(const struct pv_scene_tf* tf){
    float norm=0;for(unsigned i=0;i<4;i++)norm+=tf->q[i]*tf->q[i];
    if(!isfinite(norm)||fabsf(norm-1)>.01f)return 0;
    for(unsigned i=0;i<3;i++)if(!isfinite(tf->p[i])||!isfinite(tf->s[i])||tf->s[i]<=0)return 0;
    return 1;
}
static float distance_squared(const void* actor,const void* box,const float* head){
    assert(actor==object(ACTOR));assert(!box||box==object(BOX));
    const float* point=(const float*)(storage[ROOT]+0x1d0);float total=0;
    for(unsigned i=0;i<3;i++){float d=head[i]-point[i];total+=d*d;}return total;
}
static int looking(const float* point,const struct pv_scene_tf* head,float angle,unsigned char previous,float hysteresis){
    assert(point[0]==100&&angle==75&&previous==1&&hysteresis==5);assert(head->p[2]==0);return facing;
}
static void log_message(const char* format,...){(void)format;}
#define LOGF(...) log_message(__VA_ARGS__)
static int (*interact_key_hook)(void*,long);
static void (*interact_key_tick)(void*,int,int);
static int patch_code(void* at,void* fn){(void)at;(void)fn;return 0;}
#include "../re4fix_key_touch.h"
#ifdef RE4_KEY_TOUCH_ASM_CHECK
__attribute__((used)) static void (*const force_install_assembly)(uintptr_t)=install_touch_key_touch;
#endif
static unsigned char local[32],clone[32],foreign[32],finish_local[48],finish_clone[48],finish_foreign[48];
static struct kt_delegate listeners[3],finished_listeners[2];
static void ptr(void* p,const void* x){memcpy(p,&x,8);}
static void u64(void* p,uint64_t x){memcpy(p,&x,8);}
static void scalar(void* p,float x){memcpy(p,&x,4);}
static void setup(void){
    memset(storage,0,sizeof storage);memset(local,0,sizeof local);memset(clone,0,sizeof clone);
    for(int i=0;i<OBJECTS;i++){sizes[i]=sizeof storage[i];class_sizes[i]=sizeof storage[i];serial[i]=1;}
    class_sizes[ROOT]=class_sizes[HEAD]=0x1f0;class_sizes[BOX]=0x430;
    sizes[ROOT]=sizes[HEAD]=0x1f0;sizes[BOX]=0x430;
    now_ms=1000;paused=0;valid=1;game_thread=1;facing=1;body_valid=1;dead=wrong_world_actor=0;
    g_touch_context=1;g_app_input_focus=1;g_touch_cancel=0;ti_ready=1;
    kt_base=0x100000000;kt_ready=1;touch_key_item_cancel();
    kt_receptor_class=receptor_class;kt_box_class=box_class;kt_controller_class=controller_class;
    kt_world=world;kt_weapon_hand=weapon_hand;kt_distance=distance_squared;kt_looking=looking;
    ptr(storage[PAWN]+0x258,object(CONTROLLER));ptr(storage[CONTROLLER]+0x348,object(INPUT));
    ptr(storage[PAWN]+0x2d8,object(HEAD));ptr(storage[HAND]+0x2f0,object(PAWN));storage[HAND][0x314]=1;
    for(unsigned i=ROOT;i<=BOX;i++){
        ptr(storage[i]+0x20,object(i==HEAD?PAWN:ACTOR));ptr(storage[i]+0xa0,object(i==HEAD?PAWN:ACTOR));
        struct pv_scene_tf tf={{0,0,0,1},{0,0,0},{1,1,1},{0,0}};
        memcpy(storage[i]+0x1c0,&tf,sizeof tf);
    }
    ptr(storage[ACTOR]+0x130,object(ROOT));scalar(storage[ROOT]+0x1d0,100);
    scalar(storage[ACTOR]+0x360,125);scalar(storage[ACTOR]+0x364,75);
    storage[ACTOR][0x3e0]=storage[ACTOR][0x414]=storage[ACTOR][0x416]=storage[ACTOR][0x418]=1;
    storage[MANAGER][0x2c1]=1;
    u64(local,kt_base+0x9d2d420);ptr(local+8,object(ACTOR));u64(local+0x18,0x53da);
    memcpy(clone,local,32);memcpy(foreign,local,32);ptr(foreign+8,object(OTHER));
    struct kt_delegate owned={local,2,0};memcpy(storage[ACTOR]+0x3e8,&owned,sizeof owned);u64(storage[ACTOR]+0x3f8,0x53da);
    listeners[0]=(struct kt_delegate){clone,2,0};listeners[1]=(struct kt_delegate){0,0,0};listeners[2]=listeners[1];
    struct kt_list list={listeners,1,3};memcpy(storage[MANAGER]+0x3f8,&list,sizeof list);
}
static void setup_message(void){
    setup();
    listeners[0]=(struct kt_delegate){0,0,0}; // Original Remove leaves Num1 tombstone.
    memset(finish_local,0,sizeof finish_local);
    u64(finish_local,kt_base+0x9d2d500);u64(finish_local+0x10,0x49e98);
    weak_assign((struct cinema_ui_weak*)(finish_local+0x18),object(ACTOR));u64(finish_local+0x28,0x5601);
    memcpy(finish_clone,finish_local,48);memcpy(finish_foreign,finish_local,48);
    weak_assign((struct cinema_ui_weak*)(finish_foreign+0x18),object(OTHER));
    struct kt_delegate local_finish={finish_local,3,0};memcpy(storage[ACTOR]+0x390,&local_finish,sizeof local_finish);
    finished_listeners[0]=(struct kt_delegate){finish_clone,3,0};finished_listeners[1]=(struct kt_delegate){0,0,0};
    struct kt_list list={finished_listeners,1,2};memcpy(storage[MANAGER]+0x3e0,&list,sizeof list);
    storage[MANAGER][0x2a0]=storage[MANAGER][0x2c8]=1;
    int index=1,count=2;memcpy(storage[MANAGER]+0x2a4,&index,4);memcpy(storage[MANAGER]+0x2ac,&count,4);
}
static int rejected;
#define REFUSE(...) do{setup();__VA_ARGS__;assert(!touch_key_item_interact(object(PAWN),1000));assert(!kt_request.start);rejected++;}while(0)
int main(void){
    setup();assert(sizeof(struct pv_scene_tf)==48&&offsetof(struct pv_scene_tf,s)==28);
    assert(touch_key_item_interact(object(PAWN),1000)); // Actual R104 has NULL MessageVolume.
    assert(kt_confirm_result(object(MANAGER),kt_base+0x62dc500,1010,0)==1);
    assert(!kt_request.start&&kt_confirm_result(object(MANAGER),kt_base+0x62dc500,1020,0)==0);
    setup();ptr(storage[ACTOR]+0x238,object(BOX));assert(class_sizes[ROOT]<0x42c&&sizes[BOX]>=0x42c);
    assert(touch_key_item_interact(object(PAWN),1000));
    assert(kt_confirm_result(object(MANAGER),kt_base+0x62dc500,1000,1)==1&&!kt_request.start);
    setup();ptr(storage[ACTOR]+0x238,object(BOX));scalar(storage[BOX]+0x1dc,-1);
    assert(touch_key_item_interact(object(PAWN),1000));
    setup();ptr(storage[ACTOR]+0x238,object(BOX));scalar(storage[BOX]+0x1dc,0);
    assert(touch_key_item_interact(object(PAWN),1000));
    REFUSE(paused=1);REFUSE(valid=0);REFUSE(game_thread=0);REFUSE(g_touch_context=2);
    REFUSE(g_app_input_focus=0);REFUSE(g_touch_cancel=1);REFUSE(body_valid=0);REFUSE(ti_ready=0);
    REFUSE(dead=object(PAWN));REFUSE(dead=object(MANAGER));REFUSE(dead=object(ACTOR));
    REFUSE(dead=object(HAND));REFUSE(dead=object(CONTROLLER));REFUSE(dead=object(INPUT));
    REFUSE(sizes[ACTOR]=0x41f);REFUSE(sizes[MANAGER]=0x40f);REFUSE(sizes[HAND]=0x50f);
    REFUSE(sizes[CONTROLLER]=0x34f);REFUSE(storage[HAND][0x314]=0);
    REFUSE(ptr(storage[HAND]+0x2f0,object(OTHER)));REFUSE(ptr(storage[CONTROLLER]+0x348,0));
    REFUSE(storage[MANAGER][0x2c1]=0);
    REFUSE(((struct kt_list*)(storage[MANAGER]+0x3f8))->count=-1);
    REFUSE(((struct kt_list*)(storage[MANAGER]+0x3f8))->count=33);
    REFUSE(((struct kt_list*)(storage[MANAGER]+0x3f8))->capacity=65);
    REFUSE(((struct kt_list*)(storage[MANAGER]+0x3f8))->capacity=0);
    REFUSE(((struct kt_list*)(storage[MANAGER]+0x3f8))->data=0);
    REFUSE(((struct kt_list*)(storage[MANAGER]+0x3f8))->data=(void*)UINTPTR_MAX);
    REFUSE(listeners[0].units=3);REFUSE(listeners[0].instance=0);
    REFUSE(u64(clone,kt_base+0x9d2d500));REFUSE(ptr(clone+8,object(OTHER)));
    REFUSE(u64(clone+0x18,0x53db));REFUSE(u64(storage[ACTOR]+0x3f8,0));
    REFUSE(u64(local,kt_base+0x9d2d500));REFUSE(ptr(local+8,object(OTHER)));
    REFUSE(u64(local+0x18,0x53db));REFUSE(((struct kt_delegate*)(storage[ACTOR]+0x3e8))->units=0);
    REFUSE(((struct kt_list*)(storage[MANAGER]+0x3f8))->count=2;listeners[1]=(struct kt_delegate){foreign,2,0});
    REFUSE(storage[ACTOR][0x3e0]=2);REFUSE(storage[ACTOR][0x414]=0);REFUSE(storage[ACTOR][0x416]=0);
    REFUSE(storage[ACTOR][0x418]=0);REFUSE(storage[ACTOR][0x419]=1);REFUSE(storage[ACTOR][0x41a]=1);
    REFUSE(ptr(storage[ACTOR]+0x3d0,object(OTHER)));REFUSE(wrong_world_actor=object(ACTOR));
    REFUSE(wrong_world_actor=object(MANAGER));REFUSE(dead=object(WORLD));
    REFUSE(dead=object(ROOT));REFUSE(dead=object(HEAD));
    REFUSE(ptr(storage[ROOT]+0xa0,object(OTHER)));REFUSE(ptr(storage[HEAD]+0x20,object(OTHER)));
    REFUSE(scalar(storage[HEAD]+0x1cc,0));REFUSE(scalar(storage[HEAD]+0x1dc,0));
    REFUSE(scalar(storage[HEAD]+0x1d0,NAN));REFUSE(scalar(storage[ROOT]+0x1d0,INFINITY));
    REFUSE(scalar(storage[ACTOR]+0x360,100)); // Native comparison is strictly less.
    REFUSE(scalar(storage[ACTOR]+0x360,0));REFUSE(scalar(storage[ACTOR]+0x364,NAN));REFUSE(facing=0);
    REFUSE(ptr(storage[ACTOR]+0x238,object(BOX));sizes[BOX]=0x42b);
    REFUSE(ptr(storage[ACTOR]+0x238,object(BOX));scalar(storage[BOX]+0x420,-1));
    REFUSE(ptr(storage[ACTOR]+0x238,object(BOX));scalar(storage[BOX]+0x1dc,NAN));
    REFUSE(ptr(storage[ACTOR]+0x238,object(BOX));dead=object(BOX));
    setup();assert(touch_key_item_interact(object(PAWN),1000));
    assert(kt_confirm_result(object(MANAGER),kt_base+0x62dc504,1010,0)==0&&kt_request.start==1000);
    assert(kt_confirm_result(object(OTHER),kt_base+0x62dc500,1010,0)==0&&kt_request.start==1000);
    assert(kt_confirm_result(object(MANAGER),kt_base+0x62dc504,1010,7)==7&&kt_request.start==1000);
    assert(touch_key_item_interact(object(PAWN),1200)&&kt_request.start==1000); // No repeat renewal.
    assert(kt_confirm_result(object(MANAGER),kt_base+0x62dc500,1301,0)==0&&!kt_request.start);
    setup();assert(touch_key_item_interact(object(PAWN),1000));serial[ACTOR]++;
    assert(kt_confirm_result(object(MANAGER),kt_base+0x62dc500,1010,0)==0&&!kt_request.start);
    setup();assert(touch_key_item_interact(object(PAWN),1000));u64(clone+0x18,0x53db);
    assert(kt_confirm_result(object(MANAGER),kt_base+0x62dc500,1010,0)==0&&!kt_request.start);
    setup();assert(touch_key_item_interact(object(PAWN),1000));facing=0;
    assert(kt_confirm_result(object(MANAGER),kt_base+0x62dc500,1010,0)==0&&!kt_request.start);
    setup();assert(touch_key_item_interact(object(PAWN),1000));
    ((struct kt_list*)(storage[MANAGER]+0x3f8))->count=0; // ResetUI leaves stale actor handle.
    assert(kt_confirm_result(object(MANAGER),kt_base+0x62dc500,1010,0)==0&&!kt_request.start);
    setup();assert(touch_key_item_interact(object(PAWN),1000));paused=1;touch_key_item_idle_tick();assert(!kt_request.start);
    setup();assert(touch_key_item_interact(object(PAWN),1000));touch_key_item_tick(object(OTHER),0,0);assert(kt_request.start);
    touch_key_item_tick(object(HAND),1,1);assert(!kt_request.start);
    setup();assert(touch_key_item_interact(object(PAWN),1000));now_ms=1301;touch_key_item_idle_tick();assert(!kt_request.start);
    setup();assert(touch_key_item_interact(object(PAWN),1000));g_app_input_focus=0;touch_key_item_idle_tick();assert(!kt_request.start);
    setup();assert(touch_key_item_interact(object(PAWN),1000));touch_key_item_cancel();assert(!kt_request.start);
    printf("PASS: actual key Check C guards: %d refusal fixtures; NULL/owned box, fresh native geometry, exact unique delegate owner, one-shot/expiry/cancel/pool/original-true\n",rejected);
    setup_message();assert(touch_key_item_interact(object(PAWN),1000)&&kt_request.phase==2&&kt_request.handle==0x5601);
    assert(kt_confirm_result(object(MANAGER),kt_base+0x62dc500,1010,0)==1&&!kt_request.start);
    setup_message();int last=2;memcpy(storage[MANAGER]+0x2a4,&last,4);
    assert(touch_key_item_interact(object(PAWN),1000)); // Native next confirm owns end-of-array finish.
    setup_message();unsigned literal=UINT_MAX;int zero=0,one=1;
    memcpy(storage[ACTOR]+0x340,&literal,4);memcpy(storage[MANAGER]+0x2c4,&literal,4);
    memcpy(storage[MANAGER]+0x2a4,&zero,4);memcpy(storage[MANAGER]+0x2ac,&one,4);
    assert(touch_key_item_interact(object(PAWN),1000)); // Native literal-text branch completes on confirm.
    int first_rejected=rejected;
#define MESSAGE_REFUSE(...) do{setup_message();__VA_ARGS__;assert(!touch_key_item_interact(object(PAWN),1000));assert(!kt_request.start);rejected++;}while(0)
    MESSAGE_REFUSE(storage[MANAGER][0x2a0]=0);MESSAGE_REFUSE(storage[MANAGER][0x2c8]=2);
    MESSAGE_REFUSE(storage[MANAGER][0x2c1]=0);MESSAGE_REFUSE(storage[MANAGER][0x2a4]=0);
    MESSAGE_REFUSE(storage[MANAGER][0x2a4]=3);MESSAGE_REFUSE(storage[MANAGER][0x2ac]=0);
    MESSAGE_REFUSE(storage[MANAGER][0x2c4]=1);MESSAGE_REFUSE(storage[ACTOR][0x340]=1);
    MESSAGE_REFUSE(((struct kt_list*)(storage[MANAGER]+0x3e0))->count=0);
    MESSAGE_REFUSE(((struct kt_list*)(storage[MANAGER]+0x3e0))->count=33);
    MESSAGE_REFUSE(((struct kt_list*)(storage[MANAGER]+0x3e0))->capacity=0);
    MESSAGE_REFUSE(((struct kt_list*)(storage[MANAGER]+0x3e0))->data=0);
    MESSAGE_REFUSE(finished_listeners[0].units=2);MESSAGE_REFUSE(finished_listeners[0].instance=0);
    MESSAGE_REFUSE(u64(finish_clone,kt_base+0x9d2d420));
    MESSAGE_REFUSE(weak_assign((struct cinema_ui_weak*)(finish_clone+0x18),object(OTHER)));
    MESSAGE_REFUSE(weak_assign((struct cinema_ui_weak*)(finish_local+0x18),object(OTHER)));
    MESSAGE_REFUSE(u64(finish_local,kt_base+0x9d2d420));
    MESSAGE_REFUSE(((struct kt_delegate*)(storage[ACTOR]+0x390))->units=2);
    MESSAGE_REFUSE(u64(finish_clone+0x10,0x49e99));MESSAGE_REFUSE(u64(finish_local+0x10,0));
    MESSAGE_REFUSE(u64(finish_clone+0x28,0x5602));MESSAGE_REFUSE(u64(finish_local+0x28,0));
    MESSAGE_REFUSE(u64(finish_local+8,0x1234);u64(finish_clone+8,0x1235));
    MESSAGE_REFUSE(((struct kt_list*)(storage[MANAGER]+0x3e0))->count=2;finished_listeners[1]=(struct kt_delegate){finish_foreign,3,0});
    MESSAGE_REFUSE(listeners[0]=(struct kt_delegate){foreign,2,0});
    MESSAGE_REFUSE(dead=object(ACTOR));MESSAGE_REFUSE(wrong_world_actor=object(ACTOR));
    MESSAGE_REFUSE(facing=0);MESSAGE_REFUSE(paused=1);MESSAGE_REFUSE(storage[ACTOR][0x418]=0);
    setup_message();assert(touch_key_item_interact(object(PAWN),1000));serial[ACTOR]++;
    assert(kt_confirm_result(object(MANAGER),kt_base+0x62dc500,1010,0)==0&&!kt_request.start);
    setup_message();assert(touch_key_item_interact(object(PAWN),1000));u64(finish_clone+0x28,0x5602);
    assert(kt_confirm_result(object(MANAGER),kt_base+0x62dc500,1010,0)==0&&!kt_request.start);
    setup_message();assert(touch_key_item_interact(object(PAWN),1000));
    ((struct kt_list*)(storage[MANAGER]+0x3e0))->count=0;
    assert(kt_confirm_result(object(MANAGER),kt_base+0x62dc500,1010,0)==0&&!kt_request.start);
    setup_message();assert(touch_key_item_interact(object(PAWN),1000));
    assert(kt_confirm_result(object(MANAGER),kt_base+0x62dc500,1010,1)==1&&!kt_request.start);
    setup_message();assert(touch_key_item_interact(object(PAWN),1000));memcpy(storage[MANAGER]+0x2a4,&last,4);
    assert(kt_confirm_result(object(MANAGER),kt_base+0x62dc500,1010,0)==0&&!kt_request.start);
    setup();assert(touch_key_item_interact(object(PAWN),1000));__typeof__(kt_request) first_phase=kt_request;
    setup_message();kt_request=first_phase;u64(finish_local+0x28,0x53da);u64(finish_clone+0x28,0x53da);
    assert(kt_confirm_result(object(MANAGER),kt_base+0x62dc500,1010,0)==0&&!kt_request.start);
    printf("PASS: actual native-message C guards: %d refusal fixtures; Num1 tombstone, exact finished weak/name/handle, text index, phase identity and original-true\n",rejected-first_rejected);
    return 0;
}
