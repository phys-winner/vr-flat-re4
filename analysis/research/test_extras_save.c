#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#endif
static long now_ms=1000;
static long ptr_now_ms(void){return now_ms;}
static int gt=1;
static int cinema_on_game_thread(void){return gt;}
static unsigned context=4;
static unsigned get_context(void){return context;}
static unsigned (*cinema_type)(void)=get_context;
static unsigned char menu_owner[0x40];
static void* extras_menu=menu_owner;
static uint64_t extras_menu_name=0x1234;
static void* active=menu_owner;
static int frontend=1;
static void* get_active(void){return active;}
static void* (*cinema_active)(void)=get_active;
static uint64_t extras_name(const void* value){uint64_t n;memcpy(&n,(const char*)value+0x18,8);return n;}
static int extras_frontend(const void* value){assert(value==active);return frontend;}
static unsigned extras_status,extras_last_action;
static uint64_t extras_ui=31u|(1u<<10)|(5u<<11);
static int extras_ready=1,log_count,patch_count,patch_fail;
static unsigned char native[0x8700],system_data[0x200],file_data[2079352],work_data[0x6f0];
static void *system_pointer=system_data,*global_pointer=native,*work_pointer;
static void** extras_system=&system_pointer;
static unsigned char disabled,automatic;
static unsigned char snapshot_ready,menu_access,menu_state;
static unsigned char native_menu_data[0x250];
static void *publication_buffer,*snapshot_data;
static int snapshot_size,alloc_calls,free_calls,alloc_fail,ctor_calls,dtor_calls,create_calls,corrupt_payload;
static void *save_menu,*bio4=(void*)0xabc;
static int save_calls,write_calls,advance_calls,write_success=1,write_result=77,finish_job;
static void* get_menu(void){return save_menu;}
static void* get_bio4(void){return bio4;}
static int get_ok(void){return write_success;}
static void card_save(unsigned slot,unsigned flags){
    assert(slot<40 && flags==0x20);save_calls++;
    work_pointer=work_data;*(unsigned*)(native+4)|=0x98u;
}
static int patch_code(void* address,void* hook){(void)address;(void)hook;++patch_count;return patch_fail && patch_count==patch_fail ? -1:0;}
#define LOGF(...) ((void)++log_count)
#include "re4fix_extras_save.h"
#ifdef EXTRAS_SAVE_TEST
static int gate_extras_native_write(int slot,unsigned char* data,int count){
    assert(slot==0 && count==2079352 && (data==file_data || (extras_private_source && data==publication_buffer)));
    ++write_calls;return write_result;
}
static void gate_extras_card_update(void){
    ++advance_calls;
    if(finish_job && work_pointer){
        memcpy(file_data+0x1e44,system_data+4,4);
        assert(hook_extras_native_write(0,file_data,sizeof file_data)==write_result);
        work_pointer=0;*(unsigned*)(native+4)&=0x80000007u;
    }
}
#endif
static void* file_alloc(uint64_t size,unsigned alignment){assert(size==2079352&&!alignment);return ++alloc_calls==alloc_fail ? 0:malloc((size_t)size);}
static void file_free(void* value){assert(value);++free_calls;free(value);}
static void card_ctor(void* value){assert(value);++ctor_calls;assert(!*(void**)((char*)value+0x5c8));}
static void card_dtor(void* value){assert(value);++dtor_calls;}
static unsigned card_size(void* value,unsigned mode){assert(mode==1);*(unsigned*)((char*)value+0x5e8)=2079352;return 0x24b000;}
static void* get_snapshot(void){return snapshot_data;}
static int get_snapshot_size(void){return snapshot_size;}
static unsigned char get_access(const void* value){assert(value==save_menu);return menu_access;}
static unsigned char get_state(const void* value){assert(value==save_menu);return menu_state;}
static int create_system(void* value,void* info){
    (void)info;unsigned char* card=value;assert(!*(void**)(card+0x5c8)&&*(unsigned*)(card+0x5e8)==2079352);
    unsigned char* data=*(void**)(card+0x5e0);assert(data&&data!=file_data);++create_calls;
    memcpy(data+0x1e40,system_data,0xf0);
    unsigned char* published=file_alloc(2079352,0);
    if(!published)return -1;
    memcpy(published,data,2079352);if(corrupt_payload)published[0x1f30]^=1;
    publication_buffer=published;(void)hook_extras_native_write(0,published,2079352);
    file_free(published);publication_buffer=0;return 1;
}
static void setup(void){
    memset(native,0,sizeof native);memset(system_data,0,sizeof system_data);memset(file_data,0,sizeof file_data);
    now_ms=1000;gt=1;context=4;system_pointer=system_data;global_pointer=native;work_pointer=0;
    save_menu=0;bio4=(void*)0xabc;disabled=automatic=0;native[2]=3;
    extras_ready=extras_persist_ready=1;extras_persist_pending=extras_persist_started=0;
    extras_dirty_generation((void*)0x5678,0x9876,system_pointer);
    extras_dirty=extras_failed=0;extras_dirty_publish();
    extras_persist_write_seen=extras_persist_write_ok=extras_persist_status=extras_persist_action=0;
    extras_persist_tick_contexts=extras_persist_logs=0;extras_status=extras_last_action=0;
    extras_ui=31u|(1u<<10)|(5u<<11);save_calls=write_calls=advance_calls=finish_job=0;write_success=1;write_result=77;
    g_extras_card_work=&work_pointer;extras_native_global=&global_pointer;
    extras_card_disabled=&disabled;extras_card_auto=&automatic;
    extras_card_save=card_save;extras_card_menu=get_menu;extras_bio4_instance=get_bio4;extras_native_write_ok=get_ok;
    *(uint64_t*)(menu_owner+0x18)=extras_menu_name;active=extras_menu=menu_owner;frontend=1;
    extras_card_ctor=0;extras_private_source=0;extras_private_work=0;publication_buffer=0;
    memset(native_menu_data,0,sizeof native_menu_data);
    snapshot_ready=1;menu_access=menu_state=0;snapshot_data=file_data;snapshot_size=2079352;
    alloc_calls=free_calls=alloc_fail=ctor_calls=dtor_calls=create_calls=corrupt_payload=0;
}
static void setup_private(void){
    setup();memset(file_data,0x5a,sizeof file_data);memcpy(system_data,file_data+0x1e40,0xf0);
    *(unsigned*)(system_data+4)=0x40000000u;
    memset(work_data,0,sizeof work_data);work_data[4]=2;work_data[5]=1;
    *(unsigned*)(work_data+0x5e8)=2079352;*(void**)(work_data+0x5e0)=file_data;work_pointer=work_data;
    save_menu=native_menu_data;*(unsigned*)(native+4)=0x100;
    extras_card_ctor=card_ctor;extras_card_dtor=card_dtor;extras_card_size=card_size;
    extras_snapshot_data=get_snapshot;extras_snapshot_size=get_snapshot_size;
    extras_create_system=create_system;extras_menu_access=get_access;extras_menu_state=get_state;
    extras_file_alloc=file_alloc;extras_file_free=file_free;extras_snapshot_ready=&snapshot_ready;extras_publication_buffer=&publication_buffer;
}
extern void fixture_extras_save_image(unsigned char*);
extern unsigned long fixture_extras_save_code[];
extern unsigned fixture_extras_save_count;
int main(void){
    setup();for(int i=0;i<10;i++){touch_extras_save_tick(now_ms);hook_extras_card_update();}assert(!save_calls);
    setup();touch_extras_save_begin(1000,2,1);assert(extras_status==6 && !save_calls);
    for(int i=0;i<10;i++)touch_extras_save_tick(++now_ms);assert(!save_calls && extras_persist_pending);
    context=1;hook_extras_card_update();assert(save_calls==1 && advance_calls==1 && extras_status==6);
    for(int i=0;i<10;i++)hook_extras_card_update();assert(save_calls==1);
    finish_job=1;hook_extras_card_update();assert(write_calls==1 && extras_status==1 && !extras_persist_pending);
    assert(((extras_ui>>28)&7)==1 && ((extras_ui>>25)&7)==2 && (extras_ui&((1u<<25)-1))==(31u|(1u<<10)|(5u<<11)));
    setup();work_pointer=work_data;*(unsigned*)(native+4)=0x100u;
    touch_extras_save_begin(1000,2,1);hook_extras_card_update();
    assert(!save_calls && !write_calls && advance_calls==1 && work_pointer==work_data);
    assert(extras_persist_pending && !extras_persist_started && extras_status==6 && *(unsigned*)(native+4)==0x100u);
    finish_job=1;hook_extras_card_update();
    assert(!save_calls && write_calls==1 && !work_pointer && !extras_persist_write_seen && extras_status==6);
    hook_extras_card_update();
    assert(save_calls==1 && write_calls==2 && extras_persist_write_seen && extras_status==1 && !extras_persist_pending);
    setup();touch_extras_save_begin(1000,1,1);touch_extras_save_begin(1001,2,1);
    finish_job=1;hook_extras_card_update();assert(save_calls==1 && write_calls==1 && extras_status==1);
    setup();touch_extras_save_begin(1000,1,1);hook_extras_card_update();
    memcpy(file_data+0x1e44,system_data+4,4);assert(hook_extras_native_write(0,file_data,sizeof file_data)==77);
    touch_extras_save_begin(1001,3,1);work_pointer=0;*(unsigned*)(native+4)=0;
    touch_extras_save_tick(1002);assert(extras_persist_pending && !extras_persist_started && extras_status==6);
    finish_job=1;hook_extras_card_update();assert(save_calls==2 && write_calls==2 && extras_status==1);
    setup();touch_extras_save_begin(1000,4,1);write_success=0;finish_job=1;hook_extras_card_update();assert(extras_status==7);
    assert(extras_dirty==8&&extras_failed==8&&extras_persistence_ui==(8u<<5));
    setup();touch_extras_save_begin(1000,5,0);finish_job=1;hook_extras_card_update();assert(extras_status==7 && write_calls==1);
    assert(extras_dirty==16&&extras_failed==16);
    extras_status=2;extras_last_action=3;assert(extras_failed==16);
    now_ms=1001;touch_extras_save_begin(1001,5,1);hook_extras_card_update();assert(extras_status==1&&!extras_dirty&&!extras_failed);
    setup();touch_extras_save_begin(1000,2,1);gt=0;hook_extras_card_update();assert(!save_calls && advance_calls==1);gt=1;
    hook_extras_card_update();assert(save_calls==1);
    setup();touch_extras_save_begin(1000,2,1);disabled=1;hook_extras_card_update();assert(!save_calls);disabled=0;
    automatic=1;hook_extras_card_update();assert(!save_calls);automatic=0;
    save_menu=(void*)1;hook_extras_card_update();assert(!save_calls);save_menu=0;
    bio4=0;hook_extras_card_update();assert(!save_calls);bio4=(void*)1;
    work_pointer=work_data;hook_extras_card_update();assert(!save_calls);work_pointer=0;
    global_pointer=0;hook_extras_card_update();assert(!save_calls);global_pointer=native;
    *(unsigned*)(native+4)=0x100;hook_extras_card_update();assert(!save_calls);*(unsigned*)(native+4)=0;
    hook_extras_card_update();assert(save_calls==1);
    setup();touch_extras_save_begin(1000,2,1);system_pointer=(void*)3;touch_extras_save_tick(1001);assert(extras_status==7 && !save_calls);
    setup();touch_extras_save_begin(1000,2,1);touch_extras_save_tick(301001);assert(extras_status==7 && !save_calls);
    setup();touch_extras_save_begin(1000,2,1);touch_extras_save_tick(300999);assert(extras_persist_pending);
    setup();extras_persist_ready=0;touch_extras_save_begin(1000,2,1);assert(extras_status==7);
    assert(!touch_extras_save_ready());extras_persist_ready=1;assert(touch_extras_save_ready());
    setup();gt=0;touch_extras_save_begin(1000,2,1);assert(!extras_persist_pending);gt=1;
    setup();touch_extras_save_begin(1000,2,1);assert(extras_dirty==2&&!extras_failed);
    extras_dirty_generation((void*)0x6678,0x9877,system_pointer);touch_extras_save_tick(1001);
    assert(!extras_dirty&&!extras_failed&&!extras_persist_pending&&!extras_persistence_ui);
    setup();touch_extras_save_begin(1000,2,1);extras_persist_result(1);
    assert(extras_dirty==2&&extras_failed==2); /* A success label alone cannot clear unsaved actions. */
    setup();hook_extras_native_write(0,file_data,sizeof file_data);assert(!extras_persist_write_seen);
    setup();touch_extras_save_begin(1000,2,1);hook_extras_card_update();*(unsigned*)(system_data+4)=0x400000;
    hook_extras_native_write(0,file_data,sizeof file_data);assert(!extras_persist_write_seen);
    setup();touch_extras_save_begin(1000,2,1);hook_extras_card_update();work_pointer=0;
    touch_extras_save_tick(1001);assert(extras_status==7 && !write_calls);
    for(unsigned kind=2;kind<=7;kind++)if(kind==2||kind==4||kind==7){
        setup_private();context=kind;unsigned char before_work[sizeof work_data],before_native[sizeof native];
        memcpy(before_work,work_data,sizeof work_data);memcpy(before_native,native,sizeof native);
        touch_extras_save_begin(1000,2,1);touch_extras_save_tick(1001);
        assert(extras_status==1&&!extras_persist_pending&&!save_calls&&write_calls==1&&create_calls==1);
        assert(alloc_calls==2&&free_calls==2&&ctor_calls==1&&dtor_calls==1&&!publication_buffer);
        assert(work_pointer==work_data&&!memcmp(before_work,work_data,sizeof work_data)&&!memcmp(before_native,native,sizeof native));
        touch_extras_save_tick(1002);assert(write_calls==1);
    }
    setup_private();work_data[4]=5;*(int*)(work_data+0x20)=-537;*(unsigned*)(work_data+0x6d4)=73;menu_state=2;
    native_menu_data[0x245]=1;
    touch_extras_save_begin(1000,2,1);touch_extras_save_tick(1001);
    assert(extras_status==1&&write_calls==1&&work_data[4]==5&&work_data[5]==1&&work_pointer==work_data&&menu_state==2);
    setup_private();work_data[4]=5;*(int*)(work_data+0x20)=-538;*(unsigned*)(work_data+0x6d4)=73;menu_state=2;
    native_menu_data[0x245]=1;
    touch_extras_save_begin(1000,2,1);touch_extras_save_tick(1001);assert(extras_status==6&&!write_calls&&!alloc_calls);
    for(int fail=1;fail<=2;fail++){
        setup_private();alloc_fail=fail;touch_extras_save_begin(1000,2,1);touch_extras_save_tick(1001);
        assert(extras_status==7&&!extras_persist_pending&&!write_calls&&alloc_calls==fail&&free_calls==fail-1);
        assert(ctor_calls==fail-1&&dtor_calls==fail-1&&work_pointer==work_data&&!publication_buffer);
    }
    setup_private();write_success=0;touch_extras_save_begin(1000,2,1);touch_extras_save_tick(1001);
    assert(extras_status==7&&write_calls==1&&free_calls==2);
    setup_private();touch_extras_save_begin(1000,2,0);touch_extras_save_tick(1001);
    assert(extras_status==7&&write_calls==1&&free_calls==2);
    touch_extras_save_begin(1002,2,1);touch_extras_save_tick(1003);assert(extras_status==1&&write_calls==2&&free_calls==4);
    setup_private();corrupt_payload=1;touch_extras_save_begin(1000,2,1);touch_extras_save_tick(1001);
    assert(extras_status==7&&!write_calls&&free_calls==2&&file_data[0x1f30]==0x5a);
    for(int invalid=0;invalid<25;invalid++){
        setup_private();
        if(invalid==0)snapshot_ready=0;else if(invalid==1)snapshot_size--;
        else if(invalid==2)snapshot_data=file_data+16;else if(invalid==3)system_data[0x2c]^=1;
        else if(invalid==4)publication_buffer=(void*)1;else if(invalid==5)work_data[4]=5;
        else if(invalid==6)menu_state=1;else if(invalid==7)*(unsigned*)(work_data+0x14)=1;
        else if(invalid==8)frontend=0;else if(invalid==9)*(uint64_t*)(menu_owner+0x18)^=1;
        else if(invalid==10)native_menu_data[0x243]=1;
        else if(invalid==11)native_menu_data[0x244]=1;
        else if(invalid==12)native_menu_data[0x245]=1;
        else if(invalid>=13&&invalid<=16){
            work_data[4]=5;*(int*)(work_data+0x20)=-537;*(unsigned*)(work_data+0x6d4)=73;
            menu_state=2;native_menu_data[0x245]=1;
            if(invalid==13)native_menu_data[0x245]=0;
            else if(invalid==14)*(unsigned*)(work_data+0x6d4)=74;
            else if(invalid==15)menu_state=1;else work_data[0x1e]=1;
        } else if(invalid==17)work_data[6]=1;
        else if(invalid==18)work_data[7]=1;
        else if(invalid==19)work_data[0x661]=1;
        else if(invalid==20)*(unsigned*)(native+4)|=8;
        else if(invalid==21){*(unsigned*)(native+0x1c)=1;*(unsigned*)(system_data+0x28)=1;}
        else if(invalid==22)--*(unsigned*)(work_data+0x5e8);
        else if(invalid==23)context=1;else active=0;
        touch_extras_save_begin(1000,2,1);touch_extras_save_tick(1001);
        assert(extras_status==6&&extras_persist_pending&&!alloc_calls&&!create_calls&&!write_calls);
    }
#ifdef _WIN32
    unsigned char* image=VirtualAlloc(0,0xa600000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(image);
    fixture_extras_save_image(image);patch_count=patch_fail=0;extras_ready=1;install_touch_extras_save((uintptr_t)image);
    assert(extras_persist_ready && patch_count==2 && !extras_persist_pending);
    for(unsigned i=0;i<fixture_extras_save_count;i++){
        *(unsigned*)(image+fixture_extras_save_code[i])^=1;patch_count=0;
        install_touch_extras_save((uintptr_t)image);assert(!extras_persist_ready && !patch_count);
        *(unsigned*)(image+fixture_extras_save_code[i])^=1;
    }
    patch_count=0;patch_fail=2;install_touch_extras_save((uintptr_t)image);assert(!extras_persist_ready);
    VirtualFree(image,0,MEM_RELEASE);
#endif
    puts("PASS: actual one-shot C adapter; exact Lobby selection/no-slots tuple, private allocation/publication failures, unchanged campaign/job/flags; original queued fallback/lifetime/revision; 25 invalid readiness states defer");
    return 0;
}
