#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#endif
static int cinema_object(const void*,const void*,int);
static unsigned char menu[0x1200],save[0x600],other_menu[0x1200],other_save[0x600],menu_cls[0x60],save_cls[0x60];
static void* menu_chain[1],*save_chain[1];
static void* active=menu,*current_save=save,*sys=(void*)0x1234;
static void* front_work;
static int on_gt=1,save_result=1,merc_calls,extra_calls,item_calls,save_calls,queries,logs;
static unsigned type=4,extra_bits,item_bits,merc_progress;
static long g_lobby_menu_ms;
static int cinema_on_game_thread(void){return on_gt;}
static long ui_now=1100;
static long ptr_now_ms(void){return ui_now;}
static void* get_active(void){return active;}
static unsigned get_type(void){return type;}
static void* get_save(void){return current_save;}
static void* get_menu_class(void){return menu_cls;}
static void* get_save_class(void){return save_cls;}
static void* (*cinema_active)(void)=get_active;
static unsigned (*cinema_type)(void)=get_type;
static int is_extra(unsigned id){assert(id==1 || id==9);queries++;return !!(extra_bits&(1u<<id));}
static int is_item(unsigned char id){assert(id>=1 && id<=7);queries++;return !!(item_bits&(1u<<id));}
static void unlock_merc(void){merc_calls++;if(!(extra_bits&(1u<<9))){extra_bits|=1u<<9;merc_progress=0;}}
static void unlock_extra(unsigned id){assert(id==1);extra_calls++;extra_bits|=1u<<id;}
static void unlock_item(unsigned char id){assert(id>=1 && id<=7);item_calls++;item_bits|=1u<<id;}
static int try_save(void){save_calls++;return save_result;}
static unsigned persist_calls,persist_action;
static int persist_metadata,persist_ready;
static const char* map_name="Lobby";
static int map_calls,map_frees,map_bad_size,map_bad_capacity,map_bad_terminator,map_no_world;
#define LOGF(...) ((void)++logs)
#include "re4fix_mobile_extras.h"
#ifdef EXTRAS_MAP_TEST
static void extras_current_level(const void* object,unsigned char strip,struct extras_level_string* out){
    assert(object==active && strip==1);++map_calls;
    if(map_no_world){memset(out,0,sizeof *out);return;}
    size_t count=strlen(map_name)+1;
    unsigned short* text=(unsigned short*)malloc(128*sizeof *text);assert(text);
    for(size_t i=0;i<count;i++)text[i]=(unsigned char)map_name[i];
    out->data=text;out->count=map_bad_size ? 65:(int)count;
    out->capacity=map_bad_capacity ? 1:128;
    if(map_bad_terminator)text[count-1]='x';
}
#endif
static void map_free(void* object){assert(object);map_frees++;free(object);}
static void touch_extras_save_begin(long now,unsigned action,int metadata_accepted){
    assert(now>0 && action>=1 && action<=5 && cinema_on_game_thread());
    extras_dirty|=1u<<(action-1);extras_failed=0;extras_dirty_publish();
    ++persist_calls;persist_action=action;persist_metadata=metadata_accepted;extras_status=6;
}
static int touch_extras_save_ready(void){return persist_ready;}
extern void fixture_extras_image(unsigned char*);
extern unsigned long fixture_extras_code[];
extern unsigned fixture_extras_count;
static void typed(unsigned char* object,unsigned char* cls,void** chain,int size){
    memset(cls,0,0x60);chain[0]=cls+0x30;*(void**)(cls+0x30)=chain;
    *(int*)(cls+0x38)=0;*(int*)(cls+0x58)=size;*(void**)(object+0x10)=cls;
}
static void setup(void){
    memset(menu,0,sizeof menu);memset(save,0,sizeof save);typed(menu,menu_cls,menu_chain,0x570);typed(save,save_cls,save_chain,0x598);
    *(uint64_t*)(menu+0x18)=0xfeed112200000003ull;*(uint64_t*)(save+0x18)=0xbeef223300000004ull;
    active=menu;current_save=save;type=4;on_gt=1;sys=(void*)0x1234;
    extra_bits=item_bits=0;merc_progress=987;merc_calls=extra_calls=item_calls=save_calls=queries=logs=0;save_result=1;
    persist_calls=persist_action=0;persist_metadata=-1;persist_ready=1;
    map_name="Lobby";map_calls=map_frees=map_bad_size=map_bad_capacity=map_bad_terminator=map_no_world=0;
    extras_ready=1;extras_system=&sys;extras_menu=extras_save=0;extras_generation=0;extras_last_tick=0;
    front_work=0;extras_front_card_work=&front_work;
    extras_last_action=extras_status=extras_logs=0;extras_ui=extras_request=0;
    extras_dirty=extras_failed=extras_persistence_ui=0;extras_dirty_save=extras_dirty_system=0;extras_dirty_save_name=0;
    extras_is_extra=is_extra;extras_is_item=is_item;extras_unlock_merc=unlock_merc;
    extras_unlock_extra=unlock_extra;extras_unlock_item=unlock_item;extras_save_game=get_save;
    extras_try_save=try_save;extras_save_class=get_save_class;extras_menu_class=get_menu_class;
    extras_string_free=map_free;
    touch_mobile_extras_tick(1000);
    struct touch_mobile_extras snap=touch_mobile_extras_snapshot();assert(snap.available==31 && snap.unlocked==0 && snap.menu_ready==1);
}
static void transact(int action,long request_time,long tick){assert(touch_mobile_extras_request(action,request_time));touch_mobile_extras_tick(tick);}
int main(void){
    setup();uint64_t native_state=extras_ui;
    assert((native_state&31)==31 && (native_state&(1<<10)));
    assert(touch_mobile_extras_request(1,ui_now));
    assert(!touch_mobile_extras_request(2,ui_now));
    touch_mobile_extras_tick(1101);assert(merc_calls==1);
    for(int action=1;action<=5;action++){
        setup();unsigned char before_menu[sizeof menu],before_save[sizeof save];
        memcpy(before_menu,menu,sizeof menu);memcpy(before_save,save,sizeof save);
        transact(action,1050,1051);struct touch_mobile_extras s=touch_mobile_extras_snapshot();
        assert(s.last_action==action && s.status==6 && s.unlocked==(1u<<(action-1)) && save_calls==1);
        assert(persist_calls==1 && persist_action==(unsigned)action && persist_metadata==1);
        assert(!memcmp(before_menu,menu,sizeof menu) && !memcmp(before_save,save,sizeof save));
        assert(merc_calls==(action==1) && extra_calls==(action==2));
        assert(item_calls==(action==3?1:action==4?2:action==5?4:0));
        int calls=merc_calls+extra_calls+item_calls;merc_progress=3456;
        transact(action,1100,1101);s=touch_mobile_extras_snapshot();assert(s.status==2 && save_calls==1 && persist_calls==1);
        assert(calls==merc_calls+extra_calls+item_calls && merc_progress==3456); /* Already-open Mercenaries never reset. */
    }
    setup();transact(1,1100,1101);assert(merc_progress==0);merc_progress=555;
    for(int i=0;i<20;i++){transact(1,1200+i*2,1201+i*2);}assert(merc_calls==1 && merc_progress==555 && extras_logs==16);
    setup();item_bits=1u<<2;transact(4,1100,1101);assert(item_calls==1 && item_bits==((1u<<2)|(1u<<3)));
    setup();save_result=0;transact(5,1100,1101);assert(touch_mobile_extras_snapshot().status==6 && item_calls==4 && save_calls==1 && persist_calls==1 && persist_metadata==0);
    extras_status=7;extras_failed=16;extras_dirty_publish();save_result=1;transact(5,1200,1201);
    assert(item_calls==4 && save_calls==2 && persist_calls==2 && persist_metadata==1 && extras_status==6);
    setup();transact(1,1100,1101);merc_progress=567;extras_status=7;extras_failed=1;extras_dirty_publish();
    transact(1,1200,1201);assert(merc_calls==1 && merc_progress==567 && save_calls==2 && persist_calls==2);
    extras_status=7;extras_failed=1;extras_dirty_publish();item_bits=1u<<1;transact(3,1300,1301);
    assert(merc_calls==1 && merc_progress==567 && save_calls==2 && persist_calls==2 && extras_status==2);
    transact(1,1400,1401);assert(merc_calls==1&&merc_progress==567&&save_calls==3&&persist_calls==3);
    setup();item_bits=1u<<1;transact(2,1100,1101);extras_status=7;extras_failed=2;extras_dirty_publish();
    transact(3,1200,1201);assert(save_calls==1&&persist_calls==1&&touch_mobile_extras_snapshot().failed==2);
    transact(2,1300,1301);assert(extra_calls==1&&save_calls==2&&persist_calls==2&&touch_mobile_extras_snapshot().pending==2);
    extras_failed=2;extras_dirty_publish();memcpy(other_save,save,sizeof save);current_save=other_save;
    touch_mobile_extras_tick(1600);assert(!touch_mobile_extras_snapshot().pending&&!touch_mobile_extras_snapshot().failed);
    transact(2,1700,1701);assert(extra_calls==1&&save_calls==2&&persist_calls==2);
    setup();assert(touch_mobile_extras_request(1,1100));assert(!touch_mobile_extras_request(2,1100));touch_mobile_extras_cancel();touch_mobile_extras_tick(1101);assert(!merc_calls);
    setup();assert(!touch_mobile_extras_request(0,1100) && !touch_mobile_extras_request(6,1100) && !touch_mobile_extras_request(1,0));
    setup();transact(1,1100,6101);assert(!merc_calls && touch_mobile_extras_snapshot().status==3);
    setup();transact(1,1100,6100);assert(merc_calls==1); /* Exact max age. */
    setup();assert(touch_mobile_extras_request(1,1100));extras_request^=8;touch_mobile_extras_tick(1101);assert(!merc_calls);
    setup();assert(touch_mobile_extras_request(1,1100));on_gt=0;touch_mobile_extras_tick(1101);assert(!merc_calls && extras_request);on_gt=1;touch_mobile_extras_tick(1102);assert(merc_calls==1);
    const unsigned bad_types[]={0,1,3,5,6,8};
    for(unsigned i=0;i<sizeof bad_types/sizeof *bad_types;i++){
        setup();assert(touch_mobile_extras_request(1,1100));type=bad_types[i];touch_mobile_extras_tick(1101);
        assert(!merc_calls && !extras_request && !touch_mobile_extras_snapshot().menu_ready);
    }
    setup();assert(touch_mobile_extras_request(1,1100));sys=0;touch_mobile_extras_tick(1101);assert(!merc_calls && !extras_request);
    setup();assert(touch_mobile_extras_request(1,1100));current_save=0;touch_mobile_extras_tick(1101);assert(!merc_calls);
    setup();assert(touch_mobile_extras_request(1,1100));*(int*)(menu_cls+0x58)=0x56f;touch_mobile_extras_tick(1101);assert(!merc_calls);
    setup();assert(touch_mobile_extras_request(1,1100));*(int*)(save_cls+0x58)=0x597;touch_mobile_extras_tick(1101);assert(!merc_calls);
    setup();assert(touch_mobile_extras_request(1,1100));*(uint64_t*)(menu+0x18)^=1ull<<40;touch_mobile_extras_tick(1101);assert(!merc_calls && !extras_request);
    setup();assert(touch_mobile_extras_request(1,1100));*(uint64_t*)(save+0x18)^=1ull<<40;touch_mobile_extras_tick(1101);assert(!merc_calls);
    setup();assert(touch_mobile_extras_request(1,1100));memcpy(other_menu,menu,sizeof menu);active=other_menu;touch_mobile_extras_tick(1101);assert(!merc_calls);
    setup();assert(touch_mobile_extras_request(1,1100));memcpy(other_save,save,sizeof save);current_save=other_save;touch_mobile_extras_tick(1101);assert(!merc_calls);
    setup();int prior=queries;touch_mobile_extras_tick(1249);assert(queries==prior);touch_mobile_extras_tick(1250);assert(queries>prior);
    for(unsigned kind=2;kind<=7;kind++) {
        if(kind!=2 && kind!=4 && kind!=7)continue;
        setup();type=kind;transact(2,1100,1101);assert(extra_calls==1 && map_calls==map_frees && map_calls==2);
        setup();type=kind;map_name="r100";touch_mobile_extras_tick(1250);
        assert(!touch_mobile_extras_snapshot().menu_ready && !extra_calls && map_calls==map_frees);
        setup();type=kind;assert(touch_mobile_extras_request(2,1100));map_name="r100";
        touch_mobile_extras_tick(1101);assert(!extra_calls && !extras_request && !touch_mobile_extras_snapshot().menu_ready);
    }
    setup();assert(touch_mobile_extras_request(2,1100));map_no_world=1;touch_mobile_extras_tick(1101);assert(!extra_calls && map_frees==1);
    setup();assert(touch_mobile_extras_request(2,1100));map_bad_size=1;touch_mobile_extras_tick(1101);assert(!extra_calls && map_calls==map_frees);
    setup();assert(touch_mobile_extras_request(2,1100));map_bad_capacity=1;touch_mobile_extras_tick(1101);assert(!extra_calls && map_calls==map_frees);
    setup();assert(touch_mobile_extras_request(2,1100));map_bad_terminator=1;touch_mobile_extras_tick(1101);assert(!extra_calls && map_calls==map_frees);
    setup();assert(touch_mobile_extras_request(2,1100));extras_string_free=0;touch_mobile_extras_tick(1101);assert(!extra_calls && map_calls==1 && map_frees==1);
    setup();front_work=(void*)1;touch_mobile_extras_tick(1250);
    assert(touch_mobile_extras_snapshot().menu_ready && touch_mobile_extras_snapshot().available==31);
    assert(touch_mobile_extras_request(2,1251) && !extra_calls);
    touch_mobile_extras_tick(1252);
    assert(extra_calls==1 && persist_calls==1 && front_work==(void*)1 && touch_mobile_extras_snapshot().status==6);
    front_work=0;touch_mobile_extras_tick(1500);assert(touch_mobile_extras_snapshot().available==31);
    setup();assert(touch_mobile_extras_request(2,1100));front_work=(void*)1;touch_mobile_extras_tick(1101);
    assert(extra_calls==1 && persist_calls==1 && front_work==(void*)1 && touch_mobile_extras_snapshot().status==6);
    assert(touch_mobile_extras_snapshot().available==31);
    setup();persist_ready=0;touch_mobile_extras_tick(1250);
    assert(touch_mobile_extras_snapshot().menu_ready && !touch_mobile_extras_snapshot().available);
    assert(!touch_mobile_extras_request(2,1251) && !extra_calls);
    setup();assert(touch_mobile_extras_request(2,1100));persist_ready=0;touch_mobile_extras_tick(1101);
    assert(!extra_calls && !persist_calls && touch_mobile_extras_snapshot().status==3);
#ifdef _WIN32
    uintptr_t base=0x20000000;unsigned char* image=VirtualAlloc((void*)base,0xb000000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(image==(void*)base);
    fixture_extras_image(image);install_touch_mobile_extras(base);assert(extras_ready);
    for(unsigned i=0;i<fixture_extras_count;i++)for(unsigned j=0;j<2;j++){
        *(unsigned*)(image+fixture_extras_code[i]+j*4)^=1;install_touch_mobile_extras(base);assert(!extras_ready);
        *(unsigned*)(image+fixture_extras_code[i]+j*4)^=1;
    }
    const unsigned legacy[]={0x9d55728+1*16+8,0x9d55728+9*16+8};
    for(unsigned i=0;i<2;i++){*(int64_t*)(image+legacy[i])^=1;install_touch_mobile_extras(base);assert(!extras_ready);*(int64_t*)(image+legacy[i])^=1;}
    for(unsigned i=1;i<=7;i++){*(int64_t*)(image+0x9de3c10+i*16+8)^=1;install_touch_mobile_extras(base);assert(!extras_ready);*(int64_t*)(image+0x9de3c10+i*16+8)^=1;}
    *(uintptr_t*)(image+0xa361da8)^=8;install_touch_mobile_extras(base);assert(!extras_ready);*(uintptr_t*)(image+0xa361da8)^=8;
    install_touch_mobile_extras(0);assert(!extras_ready);install_touch_mobile_extras(base);assert(extras_ready);
    assert(VirtualFree(image,0,MEM_RELEASE));
#endif
    puts("PASS: actual exact-Lobby extras for native2/4/7; campaign/malformed maps rejected, every owned string freed; confirmed unlock queues behind unchanged original card job; explicit original transactions/no repeated Merc reset; installer36 code/9 enum/GOT refusals");return 0;
}
