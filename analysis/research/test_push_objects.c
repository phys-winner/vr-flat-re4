// Host fixtures execute the actual capability publisher; no native game object is modified.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef RE4_QA_USER_TRANSITIONS
#include <stdarg.h>
static char diagnostic_lines[512][1024];
static int diagnostic_count;
static void diagnostic_log(const char* format,...) {
    assert(diagnostic_count<512);va_list args;va_start(args,format);
    vsnprintf(diagnostic_lines[diagnostic_count++],sizeof diagnostic_lines[0],format,args);va_end(args);
}
#define LOGF(...) diagnostic_log(__VA_ARGS__)
#else
#define LOGF(...) ((void)0)
#endif
static unsigned char pawn_a[0x1500],pawn_b[0x1500],right_hand[0x510],foreign_hand[0x510];
struct cinema_ui_weak {int index,serial;};
static unsigned serial=1;
static void weak_assign(struct cinema_ui_weak* value,const void* object){value->index=object==pawn_a?1:object==pawn_b?2:-1;value->serial=serial;}
static void* weak_get(const struct cinema_ui_weak* value){return value->serial!=(int)serial?0:value->index==1?pawn_a:value->index==2?pawn_b:0;}
static void (*cu_weak_assign)(struct cinema_ui_weak*,const void*)=weak_assign;
static void* (*cu_weak_get)(const struct cinema_ui_weak*)=weak_get;
static int g_app_input_focus=1,g_touch_context=1,g_touch_cancel,gt=1,paused,valid_player=1;
static int ti_ready=1;
static unsigned type=1,command=1,action=6;
static void* active=pawn_a;
static int cinema_on_game_thread(void){return gt;}
static void* active_pawn(void){return active;}
static unsigned active_type(void){return type;}
static void* hand_pawn(const void* hand){return hand==right_hand?active:pawn_b;}
static int is_paused(void){return paused;}
static int is_player_valid(void){return valid_player;}
static void* (*phone_active)(void)=active_pawn;
static unsigned (*phone_type)(void)=active_type;
static void* (*phone_player)(const void*)=hand_pawn;
static int (*rl_paused)(void)=is_paused,(*rl_player_valid)(void)=is_player_valid;
static int interact_bound_hand(void* hand,void* pawn,int side){return hand==right_hand&&pawn==active&&side==1;}
static unsigned interact_prompt(unsigned* out){*out=action;return command;}
static int interact_face_bit(unsigned value){return value==1?0:value==2?1:value==3?8:value==4?9:-1;}
#include "../re4fix_push_objects.h"

static unsigned char player[0x1000],push[0x18],targets[2][0x1200],manager[0x18];
static void* player_slot=player;
static void ptr(void* at,void* value){memcpy(at,&value,sizeof value);}
static void word(void* at,unsigned value){memcpy(at,&value,4);}
static void half(void* at,short value){memcpy(at,&value,2);}
static void setup(void) {
    memset(player,0,sizeof player);memset(push,0,sizeof push);memset(targets,0,sizeof targets);memset(manager,0,sizeof manager);
    g_app_input_focus=1;g_touch_context=type=1;g_touch_cancel=paused=0;valid_player=gt=ti_ready=1;
    active=pawn_a;command=1;action=6;player_slot=player;
    ptr(player+0x9b8,push);ptr(push,player);ptr(push+8,targets[0]);word(push+0x14,0);
    ptr(manager+8,targets);word(manager+0x10,2);word(manager+0x14,sizeof targets[0]);
    word(targets[0]+8,1);targets[0][0x118]=0x55;half(targets[0]+0x3e8,1);
    push_objects_player_slot=&player_slot;push_objects_em_manager=manager;
    touch_push_objects_clear();
}
static void publish(void){touch_push_objects_tick(right_hand,1000);assert(touch_push_objects_input_mask(1000)==1);}
#ifdef RE4_QA_USER_TRANSITIONS
static unsigned char diagnostic_pool[12][0x1200],diagnostic_keys[0x20],diagnostic_game[0x5040];
static void* diagnostic_game_slot=diagnostic_game;
static void diagnostic_setup(void) {
    setup();diagnostic_count=0;push_objects_diag_ready=1;push_objects_diag_pending=-1;
    push_objects_diag_until=push_objects_diag_next=0;
    memset(diagnostic_pool,0,sizeof diagnostic_pool);memset(diagnostic_keys,0,sizeof diagnostic_keys);memset(diagnostic_game,0,sizeof diagnostic_game);
    push_objects_diag_keys=diagnostic_keys;push_objects_diag_game=&diagnostic_game_slot;
    half(diagnostic_game+0x5038,0x212);word(diagnostic_keys+0x18,0x400);
    ptr(manager+8,diagnostic_pool);word(manager+0x10,12);word(manager+0x14,sizeof diagnostic_pool[0]);
    for(int i=0;i<12;i++) {
        word(diagnostic_pool[i]+8,1);diagnostic_pool[i][0x118]=0x55;diagnostic_pool[i][0x119]=3;
        diagnostic_pool[i][0x61e]=(unsigned char)(100+i);half(diagnostic_pool[i]+0x3e8,1);
    }
    ptr(push+8,diagnostic_pool[11]);word(push+0x14,3);
}
static void diagnostic_tests(void) {
    diagnostic_setup();assert(!touch_push_objects_diag_request(-1)&&!touch_push_objects_diag_request(15001));
    assert(touch_push_objects_diag_request(1000));gt=0;touch_push_objects_tick(right_hand,1000);assert(!diagnostic_count&&push_objects_diag_pending==1000);gt=1;
    touch_push_objects_tick(foreign_hand,1000);assert(!diagnostic_count&&push_objects_diag_pending==1000);
    touch_push_objects_tick(right_hand,0);assert(!diagnostic_count&&push_objects_diag_pending==1000);
    unsigned char old_player[sizeof player],old_push[sizeof push],old_manager[sizeof manager],old_pool[sizeof diagnostic_pool];
    unsigned char old_keys[sizeof diagnostic_keys],old_game[sizeof diagnostic_game];
    memcpy(old_player,player,sizeof player);memcpy(old_push,push,sizeof push);memcpy(old_manager,manager,sizeof manager);
    memcpy(old_pool,diagnostic_pool,sizeof old_pool);memcpy(old_keys,diagnostic_keys,sizeof old_keys);memcpy(old_game,diagnostic_game,sizeof old_game);
    touch_push_objects_tick(right_hand,1000);assert(diagnostic_count==9);
    assert(strstr(diagnostic_lines[0],"room=212")&&strstr(diagnostic_lines[0],"slot=11 side=3")&&strstr(diagnostic_lines[0],"held=0000000000000400"));
    assert(strstr(diagnostic_lines[1],"slot=11 target=1")&&strstr(diagnostic_lines[1],"ets=111"));
    for(long now=1001;now<1200;now++)touch_push_objects_tick(right_hand,now);assert(diagnostic_count==9);
    touch_push_objects_tick(right_hand,1200);assert(diagnostic_count==18);
    assert(touch_push_objects_diag_request(0));touch_push_objects_tick(right_hand,1400);assert(diagnostic_count==18);
    assert(!memcmp(old_player,player,sizeof player)&&!memcmp(old_push,push,sizeof push)&&!memcmp(old_manager,manager,sizeof manager));
    assert(!memcmp(old_pool,diagnostic_pool,sizeof old_pool)&&!memcmp(old_keys,diagnostic_keys,sizeof old_keys)&&!memcmp(old_game,diagnostic_game,sizeof old_game));
    diagnostic_setup();assert(touch_push_objects_diag_request(1000));g_app_input_focus=0;g_touch_cancel=3;word(push+0x14,4);
    touch_push_objects_tick(right_hand,1000);assert(!diagnostic_count&&push_objects_diag_pending==1000);
    g_app_input_focus=1;g_touch_cancel=0;touch_push_objects_tick(right_hand,1001);
    assert(diagnostic_count==9&&strstr(diagnostic_lines[0],"slot=-1 side=4"));
    for(int failure=0;failure<6;failure++) {
        diagnostic_setup();assert(touch_push_objects_diag_request(1000));
        if(failure==0)word(manager+0x10,4097);
        if(failure==1)word(manager+0x14,16);
        if(failure==2)ptr(manager+8,(void*)(uintptr_t)1);
        if(failure==3)ptr(manager+8,(void*)(UINTPTR_MAX-3));
        if(failure==4){word(manager+0x14,0x10001);word(manager+0x10,4096);}
        if(failure==5)word(manager+0x10,0);
        touch_push_objects_tick(right_hand,1000);assert(diagnostic_count==1&&strstr(diagnostic_lines[0],"pool_valid=0"));
    }
    diagnostic_setup();ptr(push+8,diagnostic_pool[11]+4);assert(touch_push_objects_diag_request(1));
    touch_push_objects_tick(right_hand,1000);assert(diagnostic_count==9&&strstr(diagnostic_lines[0],"slot=-1"));
    touch_push_objects_tick(right_hand,1200);assert(diagnostic_count==9);
    diagnostic_setup();word(manager+0x10,1);half(diagnostic_pool[0]+0x3e8,0);ptr(push+8,diagnostic_pool[0]);
    assert(touch_push_objects_diag_request(15000));
    for(long now=1000;now<17000;now++)touch_push_objects_tick(right_hand,now);
    assert(diagnostic_count==75); // Exactly5Hz for15s; no dead slot or out-of-range target reads/logs.
    puts("PASS: QA-only Push diagnostic requests, owning GT/focus/cancel gate, 5Hz/15s bounds, nine-line sample cap/target priority, invalid pool/alignment/overflow refusal, pre-target snapshots, byte-identical game objects");
}
#endif
int main(void) {
    setup();publish();assert(touch_push_objects_input_mask(1250)==1&&!touch_push_objects_input_mask(1251)&&!touch_push_objects_input_mask(999));
    for(unsigned face=1;face<=4;face++) {setup();command=face;touch_push_objects_tick(right_hand,1000);assert(touch_push_objects_input_mask(1000)==1u<<interact_face_bit(face));}
    for(unsigned subtype=0;subtype<4;subtype++)for(unsigned side=0;side<4;side++) {
        setup();targets[0][0x119]=(unsigned char)subtype;word(push+0x14,side);publish();
        for(unsigned state=0;state<=21;state++)if(state==0||state==1||state==20||state==21) {
            player[0x115]=9;player[0x116]=(unsigned char)state;command=0;action=71;
            touch_push_objects_tick(right_hand,1020);assert(touch_push_objects_input_mask(1020)==1);
        }
        touch_push_objects_tick(foreign_hand,1040);assert(touch_push_objects_input_mask(1040)==1);
        player[0x116]=22;touch_push_objects_tick(right_hand,1050);assert(!touch_push_objects_input_mask(1050));
        command=1;action=6;touch_push_objects_tick(right_hand,1051);assert(!touch_push_objects_input_mask(1051));
    }
    for(int failure=0;failure<16;failure++) {
        setup();publish();player[0x115]=9;player[0x116]=21;command=0;action=71;
        if(failure==0)g_app_input_focus=0;
        if(failure==1)g_touch_context=2;
        if(failure==2)g_touch_cancel=1;
        if(failure==3)paused=1;
        if(failure==4)valid_player=0;
        if(failure==5)type=3;
        if(failure==6)active=pawn_b;
        if(failure==7)++serial;
        if(failure==8)player[0x115]=8;
        if(failure==9)player[0x116]=50;
        if(failure==10)word(push+0x14,4);
        if(failure==11)ptr(push,pawn_b);
        if(failure==12)word(manager+0x10,0);
        if(failure==13)ptr(push+8,targets[0]+8);
        if(failure==14)targets[0][0x118]=0x54;
        if(failure==15)half(targets[0]+0x3e8,0);
        if(failure<3)assert(!touch_push_objects_input_mask(1001));
        touch_push_objects_tick(right_hand,1020);assert(!touch_push_objects_input_mask(1020));
        g_app_input_focus=1;g_touch_context=type=1;g_touch_cancel=paused=0;valid_player=1;
        assert(!touch_push_objects_input_mask(1021));
    }
    for(int failure=0;failure<5;failure++) {
        setup();
        if(failure==0)targets[0][0x119]=4;
        if(failure==1)word(targets[0]+8,0x201);
        if(failure==2)word(manager+0x14,16);
        if(failure==3)word(manager+0x10,4097);
        if(failure==4){action=16;command=1;}
        touch_push_objects_tick(right_hand,1000);assert(!touch_push_objects_input_mask(1000));
    }
    setup();gt=0;touch_push_objects_tick(right_hand,1000);assert(!push_objects_snapshot);gt=1;publish();
    unsigned char before_player[sizeof player],before_push[sizeof push],before_targets[sizeof targets];
    memcpy(before_player,player,sizeof player);memcpy(before_push,push,sizeof push);memcpy(before_targets,targets,sizeof targets);
    for(int i=0;i<100;i++)touch_push_objects_tick(right_hand,1000+i);
    assert(!memcmp(before_player,player,sizeof player)&&!memcmp(before_push,push,sizeof push)&&!memcmp(before_targets,targets,sizeof targets));
#ifdef RE4_QA_USER_TRANSITIONS
    diagnostic_tests();
#endif
    puts("PASS: actual Push6 capability publisher, original face mapping/250ms lease, native flow9 stages/subtypes/sides, fresh active pawn/weak identity/EmMgr membership, context/focus/cancel/exit refusal, no native object writes");
    return 0;
}
