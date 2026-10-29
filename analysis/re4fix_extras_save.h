// Explicit changed unlocks persist through the original complete system serializer and writer.
#ifndef RE4FIX_EXTRAS_SAVE_H
#define RE4FIX_EXTRAS_SAVE_H
#include <stdint.h>
unsigned long g_tgt_extras_card_update, g_tgt_extras_native_write;
void** g_extras_card_work;
static void** extras_native_global;
static const unsigned char *extras_card_disabled,*extras_card_auto;
static void (*extras_card_save)(unsigned,unsigned);
static int (*extras_native_write_ok)(void);
static void* (*extras_card_menu)(void),*(*extras_bio4_instance)(void);
static int extras_persist_ready,extras_persist_pending,extras_persist_started;
static int extras_persist_metadata,extras_persist_write_seen,extras_persist_write_ok;
static void *extras_persist_system,*extras_persist_work;
static void* extras_persist_save;
static uint64_t extras_persist_save_name;
static long extras_persist_requested;
static unsigned extras_persist_action,extras_persist_logs,extras_persist_status;
static unsigned extras_persist_revision,extras_persist_written_revision;
static unsigned extras_persist_tick_contexts;
static unsigned extras_card_update_calls;
static long extras_persist_diagnostic_ms;
static void (*extras_card_ctor)(void*),(*extras_card_dtor)(void*),(*extras_file_free)(void*);
static unsigned (*extras_card_size)(void*,unsigned);
static int (*extras_create_system)(void*,void*),(*extras_snapshot_size)(void);
static void* (*extras_snapshot_data)(void),*(*extras_file_alloc)(uint64_t,unsigned);
static unsigned char (*extras_menu_access)(const void*),(*extras_menu_state)(const void*);
static const unsigned char* extras_snapshot_ready;
static void** extras_publication_buffer;
static const unsigned char* extras_private_source;
static void* extras_private_work;
static unsigned extras_private_flags;

static int touch_extras_save_ready(void) { return extras_persist_ready; }

#ifndef EXTRAS_SAVE_TEST
__attribute__((naked)) static void gate_extras_card_update(void) {
    asm volatile(".inst 0xa9bf7bfd\n" ".inst 0x910003fd\n"
        "adrp x8, :got:g_extras_card_work\n" "ldr x8, [x8, :got_lo12:g_extras_card_work]\n"
        "ldr x8, [x8]\n" "ldr x0, [x8]\n"
        "adrp x16, :got:g_tgt_extras_card_update\n" "ldr x16, [x16, :got_lo12:g_tgt_extras_card_update]\n"
        "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static int gate_extras_native_write(int slot,unsigned char* data,int count) {
    asm volatile(".inst 0xf81b0ffc\n" ".inst 0xa9015ff8\n" ".inst 0xa90257f6\n" ".inst 0xa9034ff4\n"
        "adrp x16, :got:g_tgt_extras_native_write\n" "ldr x16, [x16, :got_lo12:g_tgt_extras_native_write]\n"
        "ldr x16, [x16]\n" "br x16\n");
}
#else
static void gate_extras_card_update(void);
static int gate_extras_native_write(int,unsigned char*,int);
#endif

static void extras_persist_publish_status(void) {
    uint64_t ui=__atomic_load_n(&extras_ui,__ATOMIC_ACQUIRE);
    if((ui&(1u<<10)) && extras_persist_action)
        __atomic_store_n(&extras_ui,(ui&~((uint64_t)63u<<25))
            |((uint64_t)extras_persist_action<<25)|((uint64_t)extras_status<<28),__ATOMIC_RELEASE);
}
static void extras_persist_result(unsigned status) {
    extras_status=extras_persist_status=status;
    extras_last_action=extras_persist_action;
    extras_persist_pending=0;
    if(extras_persist_system==extras_dirty_system && extras_persist_save==extras_dirty_save
        && extras_persist_save_name==extras_dirty_save_name) {
        if(status==1 && extras_persist_write_seen && extras_persist_write_ok && extras_persist_metadata)
            extras_dirty=extras_failed=0;
        else extras_failed|=extras_dirty;
        extras_dirty_publish();
    }
    extras_persist_publish_status();
    if(extras_persist_logs<16){++extras_persist_logs;
        LOGF("EXTRAS save action=%u status=%u started=%d native_write_seen=%d native_write_ok=%d metadata=%d",
            extras_persist_action,status,extras_persist_started,extras_persist_write_seen,
            extras_persist_write_ok,extras_persist_metadata);}
}
// Called after an explicitly confirmed unlock, or an explicit retry of a failed publication.
static void touch_extras_save_begin(long now,unsigned action,int metadata_accepted) {
    if(!cinema_on_game_thread() || now<=0 || action<1 || action>5)return;
    void* system=extras_system ? *extras_system:0;
    if(extras_persist_pending && (extras_persist_system!=system || extras_persist_save!=extras_dirty_save
        || extras_persist_save_name!=extras_dirty_save_name))extras_persist_result(7);
    extras_dirty|=1u<<(action-1);extras_failed=0;extras_dirty_publish();
    if(!extras_persist_ready || !extras_system || !*extras_system) {
        extras_persist_system=system;extras_persist_save=extras_dirty_save;extras_persist_save_name=extras_dirty_save_name;
        extras_persist_action=action;extras_persist_metadata=metadata_accepted;
        extras_persist_started=extras_persist_write_seen=extras_persist_write_ok=0;
        extras_persist_result(7);return;
    }
    // Additional explicit changes before one pending writer are included by the original serializer.
    if(extras_persist_pending) {
        extras_persist_action=action;
        extras_persist_metadata=metadata_accepted;
        ++extras_persist_revision;
        extras_status=6;return;
    }
    extras_persist_pending=1;extras_persist_started=extras_persist_write_seen=extras_persist_write_ok=0;
    extras_persist_action=action;extras_persist_metadata=metadata_accepted;
    extras_persist_revision=1;extras_persist_written_revision=0;
    extras_persist_requested=now;extras_persist_system=*extras_system;extras_persist_work=0;
    extras_persist_save=extras_dirty_save;extras_persist_save_name=extras_dirty_save_name;
    extras_persist_diagnostic_ms=0;
    extras_status=6;
}
// A ready Lobby load menu keeps its selection cCard until New Game/Load/Back is chosen.
// Publish a private, complete snapshot with the original serializer/writer; leave that job waiting.
static int extras_publish_frontend(long now) {
    if(!extras_persist_pending || extras_persist_started || !extras_card_ctor || !g_extras_card_work
        || !*g_extras_card_work || !extras_native_global || !*extras_native_global
        || !extras_snapshot_ready || *extras_snapshot_ready!=1 || !extras_publication_buffer
        || *extras_publication_buffer || !extras_card_disabled || *extras_card_disabled
        || !extras_card_auto || *extras_card_auto)return 0;
    unsigned context=cinema_type();
    void* pawn=cinema_active();
    if((context!=2 && context!=4 && context!=7) || !pawn || pawn!=extras_menu
        || extras_name(pawn)!=extras_menu_name || !extras_frontend(pawn))return 0;
    void* menu=extras_card_menu();
    if(!menu || extras_menu_access(menu)!=0)return 0;
    const unsigned char* menu_bytes=(const unsigned char*)menu;
    // Exact original HasConfirmed/HasCancelled/IsShowingMessage byte fields; read only.
    if(menu_bytes[0x243] || menu_bytes[0x244])return 0;
    unsigned menu_state=extras_menu_state(menu);
    const unsigned char* work=(const unsigned char*)*g_extras_card_work;
    unsigned mode,size,flags,clear,old,message;int error;
    memcpy(&mode,work+0x14,4);memcpy(&size,work+0x5e8,4);
    memcpy(&error,work+0x20,4);memcpy(&message,work+0x6d4,4);
    memcpy(&flags,(const char*)*extras_native_global+4,4);
    memcpy(&clear,(const char*)*extras_native_global+0x1c,4);
    memcpy(&old,(const char*)*extras_system+0x28,4);
    const unsigned char* source=(const unsigned char*)extras_snapshot_data();
    const void* loaded;memcpy(&loaded,work+0x5e0,8);
    // dataSelect waits for selection; the exact no-saves(-537/message73) frontend waits for
    // its authored message instead. No other error/save/load/confirmation state is eligible.
    int waiting=(work[4]==2 && menu_state==0 && !menu_bytes[0x245])
        || (work[4]==5 && !work[0x1e] && error==-537 && message==73 && menu_state==2
            && menu_bytes[0x245]==1);
    if(mode || !waiting || work[5]!=1 || work[6] || work[7] || work[0x661]
        || (flags&~0x80000307u) || (clear&old) || size!=2079352 || !source || loaded!=source
        || extras_snapshot_size()!=2079352
        || memcmp(source+0x1e6c,(const char*)*extras_system+0x2c,0xa0))return 0;
    unsigned char* copy=(unsigned char*)extras_file_alloc(2079352,0);
    if(!copy){extras_persist_result(7);return 1;}
    memcpy(copy,source,2079352);
    unsigned char card[0x6f0] __attribute__((aligned(16)))={0};
    extras_card_ctor(card);extras_card_size(card,1);
    memcpy(&size,card+0x5e8,4);
    if(size==2079352) {
        // The ctor/getUseMemSize pair owns no buffers. Bind only this private buffer;
        // A zero-initialized +0x5c8 keeps the original campaign-slot copy unreachable without flag writes.
        memcpy(card+0x5e0,&copy,8);
        extras_private_source=source;extras_private_work=(void*)work;extras_private_flags=flags;
        extras_persist_write_seen=extras_persist_write_ok=0;
        int created=extras_create_system(card,0);
        extras_private_source=0;extras_private_work=0;
        extras_persist_started=1;
        unsigned after;memcpy(&after,(const char*)*extras_native_global+4,4);
        int ok=created==1 && extras_persist_write_seen && extras_persist_write_ok
            && after==flags && *g_extras_card_work==work;
        extras_card_dtor(card);extras_file_free(copy);
        if(extras_persist_logs<16){++extras_persist_logs;
            LOGF("EXTRAS Lobby original system publication action=%u ok=%d bytes=2079352 selection_preserved=1 at=%ld",
                extras_persist_action,ok,now);}
        extras_persist_result(ok && extras_persist_metadata ? 1u:7u);
        return 1;
    }
    extras_card_dtor(card);extras_file_free(copy);extras_persist_result(7);return 1;
}
static void touch_extras_save_tick(long now) {
    if(!cinema_on_game_thread())return;
    if(!extras_persist_pending) {
        if(extras_persist_action && extras_persist_status && !extras_status && !extras_last_action) {
            extras_status=extras_persist_status;extras_last_action=extras_persist_action;
            extras_persist_publish_status();
        }
        return;
    }
    if(!extras_system || *extras_system!=extras_persist_system || extras_persist_save!=extras_dirty_save
        || extras_persist_save_name!=extras_dirty_save_name || now<=0
        || now<extras_persist_requested || now-extras_persist_requested>300000) {
        extras_persist_result(7);return;
    }
    if(extras_publish_frontend(now))return;
    if(g_extras_card_work && *g_extras_card_work && extras_persist_logs<16
        && (!extras_persist_diagnostic_ms || now-extras_persist_diagnostic_ms>=5000)) {
        const unsigned char* work=(const unsigned char*)*g_extras_card_work;
        unsigned mode,flags=0,message;int error;
        memcpy(&mode,work+0x14,4);memcpy(&error,work+0x20,4);memcpy(&message,work+0x6d4,4);
        if(extras_native_global && *extras_native_global)memcpy(&flags,(const char*)*extras_native_global+4,4);
        void* menu=extras_card_menu();
        unsigned state=menu && extras_menu_state ? extras_menu_state(menu):255u;
        ++extras_persist_logs;extras_persist_diagnostic_ms=now;
        LOGF("EXTRAS queued card context=%u work=%p mode=%u stage=%u,%u,%u done=%u flags=%08x native_ticks=%u own=%d error=%d message=%u menu_state=%u ready=%u",
            cinema_type(),work,mode,work[4],work[5],work[6],work[0x661],flags,extras_card_update_calls,extras_persist_started,
            error,message,state,extras_snapshot_ready ? *extras_snapshot_ready:0);
    }
    if(extras_persist_started && g_extras_card_work && !*g_extras_card_work) {
        if(extras_persist_write_seen && extras_persist_write_ok
            && extras_persist_written_revision!=extras_persist_revision) {
            // A distinct later confirmed change happened after the earlier native write.
            extras_persist_started=extras_persist_write_seen=extras_persist_write_ok=0;
            extras_persist_work=0;extras_status=6;
        } else extras_persist_result(extras_persist_write_seen && extras_persist_write_ok && extras_persist_metadata ? 1u:7u);
    } else extras_status=6;
    extras_persist_publish_status();
}
static void hook_extras_card_update(void) {
    long now=ptr_now_ms();
    if(cinema_on_game_thread()) {
        ++extras_card_update_calls;
        unsigned context=cinema_type();
        if(context<8 && !(extras_persist_tick_contexts&(1u<<context))) {
            extras_persist_tick_contexts|=1u<<context;
            LOGF("EXTRAS original card tick context=%u bio4=%p work=%p",context,
                extras_bio4_instance(),g_extras_card_work ? *g_extras_card_work:0);
        }
    }
    if(extras_persist_pending && cinema_on_game_thread()) {
        touch_extras_save_tick(now);
        if(extras_persist_pending && !extras_persist_started && g_extras_card_work && !*g_extras_card_work
            && extras_native_global && *extras_native_global && extras_card_disabled && !*extras_card_disabled
            && extras_card_auto && !*extras_card_auto && extras_bio4_instance() && !extras_card_menu()) {
            const unsigned char* native=(const unsigned char*)*extras_native_global;
            unsigned flags;memcpy(&flags,native+4,4);
            // Preserve any authored existing load/save/reset transaction and its flags.
            if(!(flags&~0x80000007u)) {
                extras_card_save(native[2]<40 ? (unsigned)native[2]:0u,0x20u);
                extras_persist_work=*g_extras_card_work;
                extras_persist_started=1;
                if(!extras_persist_work)extras_persist_result(7);
            }
        }
    }
    // The original engine called this function. Exactly one original call is forwarded.
    gate_extras_card_update();
    touch_extras_save_tick(now);
}
static int hook_extras_native_write(int slot,unsigned char* data,int count) {
    int own=0;
    if(extras_private_source && cinema_on_game_thread()) {
        unsigned in_file=0,current=0,flags=0;
        if(data && count==2079352){memcpy(&in_file,data+0x1e44,4);memcpy(&current,(const char*)*extras_system+4,4);}
        if(extras_native_global && *extras_native_global)memcpy(&flags,(const char*)*extras_native_global+4,4);
        // Refuse an owned publication before disk I/O if any source/header/slot payload was changed.
        if(!extras_persist_pending || slot || !data || count!=2079352 || in_file!=current
            || flags!=extras_private_flags || !g_extras_card_work || *g_extras_card_work!=extras_private_work
            || memcmp(data,extras_private_source,0x1e40)
            || memcmp(data+0x1e6c,extras_private_source+0x1e6c,0xa0)
            || memcmp(data+0x1f30,extras_private_source+0x1f30,2079352-0x1f30)) {
            LOGF("EXTRAS refused private system publication: original campaign container changed");return -1;
        }
        own=1;
    }
    if(extras_persist_pending && extras_persist_started && cinema_on_game_thread()
        && g_extras_card_work && *g_extras_card_work==extras_persist_work && extras_system
        && *extras_system==extras_persist_system && extras_native_global && *extras_native_global
        && slot==0 && data && count==2079352) {
        unsigned in_file,current,flags;
        memcpy(&in_file,data+0x1e44,4);memcpy(&current,(const char*)*extras_system+4,4);
        memcpy(&flags,(const char*)*extras_native_global+4,4);
        own=own || ((flags&0x80u) && in_file==current);
    }
    int result=gate_extras_native_write(slot,data,count);
    if(own) {
        extras_persist_write_seen=1;extras_persist_write_ok=!!extras_native_write_ok();
        extras_persist_written_revision=extras_persist_revision;
        if(extras_persist_logs<16){++extras_persist_logs;
            LOGF("EXTRAS original native writer slot=%d bytes=%d system_only=1 result=%d publication_ok=%d",
                slot,count,result,extras_persist_write_ok);}
    }
    return result;
}
static void install_touch_extras_save(uintptr_t base) {
    static const struct {unsigned long address;unsigned words[4];} code[]={
        {0x5f215b4,{0xa9bf7bfd,0x910003fd,0xd0022ee8,0xf941c900}},
        {0x5f210e4,{0xf81b0ffc,0xa9015ff8,0xa90257f6,0xa9034ff4}},
        {0x5f1fe34,{0xd0022209,0xf9469929,0x2a0003e8,0x370801e1}},
        {0x61d47d8,{0xb0020c68,0xf9469908,0x5290cc09,0xf9400108}},
        {0x63580b0,{0xd0020068,0xf943f908,0xf9400108,0xb40002e8}},
        {0x61d09dc,{0xb0020ca8,0xf9446508,0xf9400108,0xb40002e8}},
        {0x5f1b0dc,{0xf81e0ff3,0xa9017bfd,0x910043fd,0xaa0003f3}},
        {0x5f1b10c,{0x911aa000,0x17f901da,0xb900101f,0xf0022228}},
        {0x5f1d2a0,{0x90ff1ee8,0xfd429900,0x90022228,0xfd02f400}},
        {0x5f202e8,{0xf0022ee8,0xf941cd00,0xd65f03c0,0xf0022ee8}},
        {0x5f202f4,{0xf0022ee8,0xb9438d00,0xd65f03c0,0xf0022ee8}},
        {0x5f1e630,{0xa9bd57f6,0xa9014ff4,0xa9027bfd,0x910083fd}},
        {0x6358cc0,{0x39490000,0xd65f03c0,0x39491800,0xd65f03c0}},
        {0x6358180,{0x39490d08,0x7100011f,0x1a9f07e0,0xd65f03c0}},
        {0x63581f8,{0x39491108,0x7100011f,0x1a9f07e0,0xd65f03c0}},
        {0x6358270,{0x39491508,0x7100011f,0x1a9f07e0,0xd65f03c0}},
        {0x6615ef0,{0xf81d0ff5,0xa9014ff4,0xa9027bfd,0x910083fd}},
        {0x6615fd0,{0xa9be4ff4,0xa9017bfd,0x910043fd,0xb40001c0}},
    };
    extras_persist_ready=extras_persist_pending=0;
    if(!base || !extras_ready)return;
    for(unsigned i=0;i<sizeof code/sizeof *code;i++)
        if(memcmp((const void*)(base+code[i].address),code[i].words,16)) {
            LOGF("EXTRAS save disabled: native opcode mismatch +%lx",code[i].address);return;
        }
    if(*(uintptr_t*)(base+0xa361d30)!=base+0xa3f5e48
        || *(uintptr_t*)(base+0xa362e30)!=base+0xa50ae25
        || *(uintptr_t*)(base+0xa362e48)!=base+0xa4ff378)return;
    g_extras_card_work=(void**)(base+0xa4ff390);
    extras_native_global=(void**)(base+0xa3f5e48);
    extras_card_disabled=(const unsigned char*)(base+0xa50ae25);
    extras_card_auto=(const unsigned char*)(base+0xa4ff378);
    extras_card_save=(void(*)(unsigned,unsigned))(base+0x5f1fe34);
    extras_native_write_ok=(int(*)(void))(base+0x61d47d8);
    extras_card_menu=(void*(*)(void))(base+0x63580b0);
    extras_bio4_instance=(void*(*)(void))(base+0x61d09dc);
    extras_card_ctor=(void(*)(void*))(base+0x5f1b0dc);
    extras_card_dtor=(void(*)(void*))(base+0x5f1b10c);
    extras_card_size=(unsigned(*)(void*,unsigned))(base+0x5f1d2a0);
    extras_snapshot_data=(void*(*)(void))(base+0x5f202e8);
    extras_snapshot_size=(int(*)(void))(base+0x5f202f4);
    extras_create_system=(int(*)(void*,void*))(base+0x5f1e630);
    extras_menu_access=(unsigned char(*)(const void*))(base+0x6358cc0);
    extras_menu_state=(unsigned char(*)(const void*))(base+0x6358cc8);
    extras_file_alloc=(void*(*)(uint64_t,unsigned))(base+0x6615ef0);
    extras_file_free=(void(*)(void*))(base+0x6615fd0);
    extras_snapshot_ready=(const unsigned char*)(base+0xa4ff380);
    extras_publication_buffer=(void**)(base+0xa4ff3a0);
    g_tgt_extras_card_update=base+0x5f215c4;g_tgt_extras_native_write=base+0x5f210f4;
    if(patch_code((void*)(base+0x5f215b4),(void*)&hook_extras_card_update)
        || patch_code((void*)(base+0x5f210e4),(void*)&hook_extras_native_write))return;
    extras_persist_ready=1;
    LOGF("EXTRAS save ready: verified Lobby snapshot or queued original system-only serializer/writer");
}
#endif
