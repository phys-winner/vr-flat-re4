#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int game_thread=1,context=1,paused,valid=1,player,hand,foreign;
static int* active=&player;static unsigned long long player_name=7;
static uintptr_t g_ue4_base_cached;
static int cinema_on_game_thread(void){return game_thread;}
static int gr_context(void){return context;}
static int rl_paused(void){return paused;}
static int rl_player_valid(void){return valid;}
static void* qs_player(void* value){return value==&hand?active:0;}
static unsigned long long qs_name(const void* value){assert(value==&player);return player_name;}
#define LOGF(...) ((void)0)
#define RE4_QA_SHOTGUN_FIXTURE
#include "qa_shotgun_fixture.h"
static struct {unsigned item,qty,size;} calls[2];
static int count,refuse,interrupt;
static int put_in_case(unsigned item,unsigned short qty,unsigned size){
 assert(count<2);calls[count].item=item;calls[count].qty=qty;calls[count].size=size;count++;
 if(interrupt==1)context=0;if(interrupt==2)active=0;if(interrupt==3)player_name++;
 return !refuse;
}
static void ready(void){
 qa_shotgun_requested=qa_shotgun_used=0;game_thread=context=valid=1;paused=count=refuse=interrupt=0;
 active=&player;player_name=7;memset(calls,0,sizeof calls);
}
int main(void){
 const char* bad[]={0,"","qashotgunfixture extra","qashotgunfixture1","qashotgunfixtur","qagrenadefixture"};
 for(unsigned i=0;i<sizeof bad/sizeof *bad;i++){ready();assert(!qa_shotgun_fixture_command(bad[i])&&!qa_shotgun_requested);}
 for(unsigned size=0;size<4;size++){
  ready();assert(qa_shotgun_fixture_command("qashotgunfixture\n")&&qa_shotgun_requested);
  qa_shotgun_fixture_apply(&hand,size,put_in_case);assert(qa_shotgun_used&&count==2);
  assert(calls[0].item==44&&calls[0].qty==1&&calls[0].size==size);
  assert(calls[1].item==24&&calls[1].qty==20&&calls[1].size==size);
  qa_shotgun_fixture_apply(&hand,size,put_in_case);assert(count==2);
 }
 for(int guard=0;guard<8;guard++){
  ready();if(guard==0)game_thread=0;if(guard==1)context=0;if(guard==2)paused=1;if(guard==3)valid=0;if(guard==4)active=0;
  qa_shotgun_fixture_apply(guard==5?&foreign:&hand,guard==6?4:0,guard==7?0:put_in_case);
  assert(!count&&!qa_shotgun_used);
 }
 for(int mode=0;mode<4;mode++){
  ready();if(mode==0)refuse=1;else interrupt=mode;
  qa_shotgun_fixture_apply(&hand,0,put_in_case);assert(count==1&&qa_shotgun_used);
  qa_shotgun_fixture_apply(&hand,0,put_in_case);assert(count==1);
 }
 assert(!qa_shotgun_fixture_original_api(0));
 unsigned char* image=calloc(1,0xa600000);assert(image);uintptr_t base=(uintptr_t)image;
 const unsigned values[][3]={
  {0x5ffd1e4,0xd10283ff,0xa9046ffc},{0x62e3198,0xa9bf7bfd,0x910003fd},
  {0x5ffd6c0,0x97fcac6b,0xf94016c8},{0x5ffd6cc,0x3940cbe9,0x39002909},
  {0x5ffd6e0,0x39003109,0x3940d7e9},{0x61539ac,0x94063dfb,0x79415261},
  {0x61539b4,0x2a0003e2,0x2a1503e0},{0x61539bc,0x97faa60a,0x2a0003f4}
 };
 for(unsigned i=0;i<8;i++)memcpy(image+values[i][0],values[i]+1,8);
 *(void**)(image+0xa361da0)=image+0xa500780;*(uint64_t*)(image+0x9dc5b48)=44;*(uint64_t*)(image+0x9dc5a08)=24;
 assert(qa_shotgun_fixture_original_api(base));
 for(unsigned i=0;i<8;i++)for(unsigned word=0;word<2;word++){
  unsigned* opcode=(unsigned*)(image+values[i][0]+4*word);*opcode^=1;assert(!qa_shotgun_fixture_original_api(base));*opcode^=1;
 }
 *(uint64_t*)(image+0x9dc5b48)=45;assert(!qa_shotgun_fixture_original_api(base));*(uint64_t*)(image+0x9dc5b48)=44;
 *(uint64_t*)(image+0x9dc5a08)=25;assert(!qa_shotgun_fixture_original_api(base));*(uint64_t*)(image+0x9dc5a08)=24;
 *(void**)(image+0xa361da0)=0;assert(!qa_shotgun_fixture_original_api(base));free(image);
 ready();assert(qa_shotgun_fixture_command("qashotgunfixture"));game_thread=0;qa_shotgun_fixture_tick(&hand);assert(qa_shotgun_requested);
 game_thread=1;qa_shotgun_fixture_tick(&hand);assert(!qa_shotgun_requested&&!qa_shotgun_used&&!count);
 puts("PASS: QA-only command parsing, one original autopack Shotgun44/1 + Bullet_12gg24/20 for sizes0..3; once-only even on failure; active-player/GT/context/pause/identity guards and16 opcode/two enum/global corruptions rejected");
}
