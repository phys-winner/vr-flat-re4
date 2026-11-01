// Actual QA wrapper; only the native ARM64 call is a host seam.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RE4_QA_USER_TRANSITIONS
static uintptr_t g_ue4_base_cached;
static unsigned char *image,player[0x1000],pawn[0x40];
_Alignas(16) static unsigned char aev[16+40*0x100];
static int gt=1,valid=1,paused,alive=1,calls,logs,native_accept=1,getter_bad;
static unsigned type=1,last_id;
static int cinema_on_game_thread(void){return gt;}
static unsigned get_type(void){return type;}
static void* get_active(void){return pawn;}
static int get_valid(void){return valid;}
static int get_paused(void){return paused;}
static int pv_alive(const void* value){return value==pawn&&alive;}
static unsigned (*cinema_type)(void)=get_type;
static void* (*cinema_active)(void)=get_active;
static int (*rl_player_valid)(void)=get_valid,(*rl_paused)(void)=get_paused;
static void diagnostic_log(const char* format,...){(void)format;logs++;}
#define LOGF(...) diagnostic_log(__VA_ARGS__)
static void put(void* at,const void* value,size_t size){memcpy(at,value,size);}
static void ptr(void* at,const void* value){put(at,&value,sizeof value);}
static void word(void* at,unsigned value){put(at,&value,4);}
static void half(void* at,unsigned short value){put(at,&value,2);}
static void native_execute(uintptr_t base,unsigned id) {
    assert(base==(uintptr_t)image);calls++;last_id=id;
    if(!native_accept)return;
    unsigned char* game=image+0xa501bd8;unsigned char* door=aev+16+id*0x100;
    word(game+0x24,4);game[0x2c]=door[0x91];game[0x2d]=door[0x90];
    memcpy(game+0x30,door+0x80,16);memcpy(game+0x503c,game+0x5038,3);
}
#define RE4_QA_DOOR_EXECUTE(base,id) native_execute(base,id)
static const void* native_ptr(uintptr_t base,unsigned id){
    assert(base==(uintptr_t)image);
    return getter_bad==1?0:getter_bad==2?aev+16+0x100:aev+16+id*0x100;
}
#define RE4_QA_DOOR_PTR(base,id) native_ptr(base,id)
#include "qa_door_entry.h"

static void setup(unsigned destination) {
    unsigned source=destination==0x101?0x100:0x211,count=destination==0x101?40:12,id=destination==0x101?0:1;
    static const unsigned poses[2][4]={{0xc74bb067,0x42b52440,0x46aff88c,0x4007cf34},{0x457dc000,0,0x44f78000,0xbfc90fdc}};
    memset(image+0xa501bd8,0,0x9000);memset(image+0xa541b38,0,0x100);memset(player,0,sizeof player);memset(aev,0,sizeof aev);
    g_ue4_base_cached=(uintptr_t)image;gt=valid=alive=native_accept=1;paused=calls=logs=getter_bad=0;type=1;
    cinema_type=get_type;cinema_active=get_active;rl_player_valid=get_valid;rl_paused=get_paused;
    ptr(image+0xa3f5e48,image+0xa501bd8);ptr(image+0xa542058,image+0xa541b38);ptr(image+0xa4e9b40,player);player[0x3dd]=1;
    word(image+0xa501bd8+0x24,3);half(image+0xa501bd8+0x5038,(unsigned short)source);
    word(image+0x6155664,0xb0021f68);word(image+0x6155668,0xf9402d08);
    word(image+0x6153ec0,0xf0021f68);word(image+0x6153ec4,0xf9402d08);
    word(image+0x61532c4,0xf81d0ff5);word(image+0x61532c8,0xa9014ff4);ptr(image+0x9cee030,image+0x61532c4);
    memcpy(aev,"AEV",4);half(aev+4,0x104);half(aev+6,(unsigned short)count);
    ptr(image+0xa541b38,aev);ptr(image+0xa541b38+8,aev+16);ptr(image+0xa541b38+0xd8,aev+16);
    for(unsigned i=0;i<count;i++) {
        unsigned char* entry=aev+16+i*0x100;ptr(entry,i+1<count?(void*)(entry+0x100):(void*)(uintptr_t)UINT32_MAX);entry[0x3a]=(unsigned char)i;
    }
    unsigned char* entry=aev+16+id*0x100;entry[0x38]=3;entry[0x39]=1;
    entry[0x90]=(unsigned char)(destination>>8);entry[0x91]=(unsigned char)destination;memcpy(entry+0x80,poses[destination==0x212],16);
}
int main(void) {
    image=calloc(1,0xa600000);assert(image);
    for(unsigned destination=0x101;;destination=0x212) {
        setup(destination);unsigned char original_player[sizeof player],original_aev[sizeof aev],original_manager[0x100];
        memcpy(original_player,player,sizeof player);memcpy(original_aev,aev,sizeof aev);memcpy(original_manager,image+0xa541b38,sizeof original_manager);
        assert(qa_authored_door(destination)==1&&calls==1&&last_id==(destination==0x101?0u:1u)&&logs==1);
        assert(!memcmp(original_player,player,sizeof player)&&!memcmp(original_aev,aev,sizeof aev)&&!memcmp(original_manager,image+0xa541b38,sizeof original_manager));
        assert(!qa_authored_door(destination)&&calls==1&&logs==2); // Native mode4 prevents duplicate entry.
        if(destination==0x212)break;
    }
    for(unsigned failure=0;failure<30;failure++) {
        setup(0x101);unsigned char* manager=image+0xa541b38;unsigned char* game=image+0xa501bd8;unsigned char* entry=aev+16;
        unsigned request=0x101;
        if(failure==0)request=0x102;
        if(failure==1)gt=0;
        if(failure==2)type=3;
        if(failure==3)valid=0;
        if(failure==4)paused=1;
        if(failure==5)alive=0;
        if(failure==6)g_ue4_base_cached=0;
        if(failure==7)word(image+0x6155664,0);
        if(failure==8)word(image+0x61532c4,0);
        if(failure==9)ptr(image+0x9cee030,image+0x61532c8);
        if(failure==10)ptr(image+0xa3f5e48,game+4);
        if(failure==11)ptr(image+0xa542058,manager+8);
        if(failure==12)ptr(image+0xa4e9b40,0);
        if(failure==13)player[0x3dd]=0;
        if(failure==14)word(game+0x24,4);
        if(failure==15)half(game+0x5038,0x101);
        if(failure==16)ptr(manager,0);
        if(failure==17)ptr(manager+8,aev+20);
        if(failure==18)aev[0]='X';
        if(failure==19)half(aev+4,0x103);
        if(failure==20)half(aev+6,39);
        if(failure==21)entry[0x38]=2;
        if(failure==22)entry[0x39]=2;
        if(failure==23)entry[0x3a]=40;
        if(failure==24)entry[0x91]=2;
        if(failure==25)entry[0x80]^=1;
        if(failure==26)getter_bad=1;
        if(failure==27)getter_bad=2;
        if(failure==28)word(image+0x6153ec0,0);
        if(failure==29)ptr(manager,(void*)(UINTPTR_MAX-3));
        unsigned char old_game[0x9000],old_manager[0x100],old_aev[sizeof aev],old_player[sizeof player];
        memcpy(old_game,game,sizeof old_game);memcpy(old_manager,manager,sizeof old_manager);memcpy(old_aev,aev,sizeof old_aev);memcpy(old_player,player,sizeof old_player);
        assert(!qa_authored_door(request)&&!calls&&logs==1);
        assert(!memcmp(old_game,game,sizeof old_game)&&!memcmp(old_manager,manager,sizeof old_manager)&&!memcmp(old_aev,aev,sizeof old_aev)&&!memcmp(old_player,player,sizeof old_player));
    }
    setup(0x101);native_accept=0;unsigned char before[0x9000];memcpy(before,image+0xa501bd8,sizeof before);
    assert(!qa_authored_door(0x101)&&calls==1&&logs==1&&!memcmp(before,image+0xa501bd8,sizeof before));
    free(image);
    puts("PASS: actual authored-door QA wrapper: both exact routes, GT/current-pawn/native initialization/preimage/AEV/pose/original-getter guards,30 refusal cases, native lock refusal retained, no wrapper game-object writes");
    return 0;
}
