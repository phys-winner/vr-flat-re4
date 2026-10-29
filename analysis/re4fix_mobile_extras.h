// Explicit front-end requests use original unlock functions; no patched gates or raw save/item writes.
#ifndef RE4FIX_MOBILE_EXTRAS_H
#define RE4FIX_MOBILE_EXTRAS_H
#include <stdint.h>
static void touch_extras_save_begin(long now,unsigned action,int metadata_accepted);
static int touch_extras_save_ready(void);
struct touch_mobile_extras {unsigned available,unlocked;int menu_ready,last_action,status;unsigned pending,failed;};
// Action1 Mercenaries,2 Professional,3 Mafia,4 bonus weapons,5 authored special modes.
static int (*extras_is_extra)(unsigned),(*extras_is_item)(unsigned char),(*extras_try_save)(void);
static void (*extras_unlock_merc)(void),(*extras_unlock_extra)(unsigned),(*extras_unlock_item)(unsigned char);
static void* (*extras_save_game)(void),*(*extras_save_class)(void),*(*extras_menu_class)(void);
struct extras_level_string {const unsigned short* data;int count,capacity;};
unsigned long g_tgt_extras_level_name;
static void (*extras_string_free)(void*);
#ifndef EXTRAS_MAP_TEST
__attribute__((naked)) static void extras_current_level(const void* object,unsigned char strip,struct extras_level_string* out) {
    asm volatile("mov x8,x2\n" "adrp x16, :got:g_tgt_extras_level_name\n"
        "ldr x16,[x16,:got_lo12:g_tgt_extras_level_name]\n" "ldr x16,[x16]\n" "br x16\n");
}
#else
static void extras_current_level(const void*,unsigned char,struct extras_level_string*);
#endif
static void** extras_system;
static void** extras_front_card_work;
static int extras_ready;
static uint64_t extras_ui,extras_request;
static void* extras_menu,*extras_save;
static uint64_t extras_menu_name,extras_save_name;
static unsigned extras_generation,extras_last_action,extras_status,extras_logs;
static long extras_last_tick;
static unsigned extras_front_logs;
// Unsaved actions belong to the native SaveGame identity, not the last selected menu row.
static unsigned extras_dirty,extras_failed,extras_persistence_ui;
static void *extras_dirty_save,*extras_dirty_system;
static uint64_t extras_dirty_save_name;
static void extras_dirty_publish(void) {
    __atomic_store_n(&extras_persistence_ui,(extras_dirty&~extras_failed)|(extras_failed<<5),__ATOMIC_RELEASE);
}
static void extras_dirty_generation(void* save,uint64_t name,void* system) {
    if(save!=extras_dirty_save || name!=extras_dirty_save_name || system!=extras_dirty_system) {
        extras_dirty_save=save;extras_dirty_save_name=name;extras_dirty_system=system;
        extras_dirty=extras_failed=0;extras_dirty_publish();
    }
}

static int extras_frontend(void* pawn) {
    if(!extras_string_free)return 0;
    struct extras_level_string level={0,0,0};
    extras_current_level(pawn,1,&level);
    static const unsigned short lobby[]={'L','o','b','b','y',0};
    int valid=level.data && level.count>=1 && level.count<=64 && level.capacity>=level.count
        && level.data[level.count-1]==0;
    int frontend=valid && level.count==6 && !memcmp(level.data,lobby,sizeof lobby);
    if(extras_front_logs<8) {
        char name[64]={0};
        if(valid)for(int i=0;i<level.count-1;i++)name[i]=level.data[i]<128 ? (char)level.data[i]:'?';
        ++extras_front_logs;LOGF("EXTRAS frontend native type=%u map='%s' eligible=%d",cinema_type(),name,frontend);
    }
    if(level.data)extras_string_free((void*)level.data);
    return frontend;
}

static struct touch_mobile_extras touch_mobile_extras_snapshot(void) {
    uint64_t p=__atomic_load_n(&extras_ui,__ATOMIC_ACQUIRE);
    unsigned persistence=__atomic_load_n(&extras_persistence_ui,__ATOMIC_ACQUIRE);
    struct touch_mobile_extras out={(unsigned)p&31,((unsigned)p>>5)&31,
        ((unsigned)p>>10)&1,((unsigned)p>>25)&7,((unsigned)p>>28)&7,persistence&31,(persistence>>5)&31};
    return out;
}
static int touch_mobile_extras_request(int action,long now) {
    uint64_t ui=__atomic_load_n(&extras_ui,__ATOMIC_ACQUIRE),empty=0;
    if(action<1 || action>5 || now<=0 || !((unsigned)ui&(1u<<10))
        || !((unsigned)ui&(1u<<(action-1))))return 0;
    uint64_t pending=((uint64_t)(unsigned)now<<32)
        |((((unsigned)ui>>11)&0x3fff)<<3)|(unsigned)action;
    return __atomic_compare_exchange_n(&extras_request,&empty,pending,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
static void touch_mobile_extras_cancel(void) {
    __atomic_store_n(&extras_request,0,__ATOMIC_RELEASE);
}
static uint64_t extras_name(const void* object) {
    uint64_t name;memcpy(&name,(const char*)object+0x18,8);return name;
}
static unsigned extras_unlocked(void) {
    unsigned mask=0;
    if(extras_is_extra(9))mask|=1;
    if(extras_is_extra(1))mask|=2;
    if(extras_is_item(1))mask|=4;
    if(extras_is_item(2) && extras_is_item(3))mask|=8;
    if(extras_is_item(4) && extras_is_item(5) && extras_is_item(6) && extras_is_item(7))mask|=16;
    return mask;
}
static void extras_publish(long now,unsigned unlocked) {
    // A confirmed unlock may queue behind an authored load/save job. Only the persistence
    // adapter may publish a verified Lobby snapshot or start the original system-only CardSave.
    unsigned available=touch_extras_save_ready() && extras_front_card_work ? 31u:0u;
    unsigned low=available|(unlocked<<5)|(1u<<10)|(extras_generation<<11)
        |(extras_last_action<<25)|(extras_status<<28);
    __atomic_store_n(&extras_ui,((uint64_t)(unsigned)now<<32)|low,__ATOMIC_RELEASE);
}
static void touch_mobile_extras_tick(long now) {
    if(!extras_ready || !cinema_on_game_thread() || now<=0)return;
    void* pawn=cinema_active();
    unsigned native_type=cinema_type();
    if((native_type!=2 && native_type!=4 && native_type!=7)
        || !cinema_object(pawn,extras_menu_class(),0x570)
        || !extras_system || !*extras_system) {
        touch_mobile_extras_cancel();__atomic_store_n(&extras_ui,0,__ATOMIC_RELEASE);
        extras_menu=extras_save=0;return;
    }
    void* save=extras_save_game();
    if(!cinema_object(save,extras_save_class(),0x598)) {
        touch_mobile_extras_cancel();__atomic_store_n(&extras_ui,0,__ATOMIC_RELEASE);
        extras_menu=extras_save=0;return;
    }
    uint64_t pawn_name=extras_name(pawn),save_name=extras_name(save);
    extras_dirty_generation(save,save_name,*extras_system);
    if(pawn!=extras_menu || pawn_name!=extras_menu_name || save!=extras_save || save_name!=extras_save_name) {
        touch_mobile_extras_cancel();extras_menu=pawn;extras_menu_name=pawn_name;
        extras_save=save;extras_save_name=save_name;
        extras_generation=(extras_generation+1)&0x3fff;if(!extras_generation)extras_generation=1;
        extras_last_tick=0;extras_last_action=extras_status=0;
    }
    uint64_t request=__atomic_load_n(&extras_request,__ATOMIC_ACQUIRE);
    if(!request && extras_last_tick && now>=extras_last_tick && now-extras_last_tick<250)return;
    // Query the actual world again for every explicit request; inventory/pause in campaign maps is excluded.
    if(!extras_frontend(pawn)) {
        touch_mobile_extras_cancel();__atomic_store_n(&extras_ui,0,__ATOMIC_RELEASE);
        extras_menu=extras_save=0;return;
    }
    __atomic_store_n(&g_lobby_menu_ms,now,__ATOMIC_RELEASE);
    unsigned unlocked=extras_unlocked();extras_last_tick=now;
    if(request) {
        request=__atomic_exchange_n(&extras_request,0,__ATOMIC_ACQ_REL);
        unsigned action=(unsigned)request&7;
        int retry=action>=1 && action<=5 && (extras_failed&(1u<<(action-1)));
        extras_last_action=action;
        if(((unsigned)request>>3)!=extras_generation
            || (unsigned)((unsigned)now-(unsigned)(request>>32))>5000
            || action<1 || action>5 || !touch_extras_save_ready()
            || !extras_front_card_work)extras_status=3;
        else if(unlocked&(1u<<(action-1))) {
            // A new explicit confirmation may retry a failed publication, never the unlock/reset.
            if(retry)touch_extras_save_begin(now,action,extras_try_save());
            else extras_status=2;
        }
        else {
            // Mercenaries' original first-unlock reset belongs to this one explicit transaction.
            if(action==1)extras_unlock_merc();
            else if(action==2)extras_unlock_extra(1);
            else {
                unsigned first=action==3?1:action==4?2:4,last=action==3?1:action==4?3:7;
                for(unsigned item=first;item<=last;item++)
                    if(!extras_is_item((unsigned char)item))extras_unlock_item((unsigned char)item);
            }
            unlocked=extras_unlocked();
            if(unlocked&(1u<<(action-1))) {
                int metadata=extras_try_save();
                extras_status=metadata?1u:5u;
                touch_extras_save_begin(now,action,metadata);
            } else extras_status=4u;
        }
        if(extras_logs<16){++extras_logs;LOGF("EXTRAS native action=%u status=%u unlocked=%02x menu=%p save=%p card_busy=%d",action,extras_status,unlocked,pawn,save,extras_front_card_work && *extras_front_card_work!=0);}
    }
    extras_publish(now,unlocked);
}
static void install_touch_mobile_extras(uintptr_t base) {
    static const struct {unsigned long addr;unsigned first[2];} check[]={
        {0x61d196c,{0xd102c3ff,0xf9004bf3}},
        {0x61d1994,{0xb940052a,0x37b001ea}},
        {0x61d19ac,{0xa9017d1f,0xf900111f}},
        {0x61e1b28,{0x90020c08,0xf946d508}},
        {0x61e1b94,{0x90020c08,0xf946d508}},
        {0x61d1b34,{0xd10403ff,0xa90c5ff8}},
        {0x61d1fac,{0xa9bf7bfd,0x910003fd}},
        {0x626dd40,{0xa9be4ff4,0xa9017bfd}},
        {0x626dc00,{0xa9be4ff4,0xa9017bfd}},
        {0x626dcc8,{0xa9417bfd,0xaa1303e0}},
        {0x64c2538,{0xd10183ff,0xf90023f3}},
        {0x64c25bc,{0x5280b304,0x52800105}},
        {0x64b7218,{0xd10183ff,0xf90023f3}},
        {0x64b72a0,{0x5280ae04,0x52800205}},
        {0x8894238,{0xf81d0ff5,0xa9014ff4}},
        {0x8894260,{0xaa0803f3,0x940deb06}},
        {0x6615fd0,{0xa9be4ff4,0xa9017bfd}},
        {0x5f215bc,{0xd0022ee8,0xf941c900}},
    };
    extras_ready=0;touch_mobile_extras_cancel();__atomic_store_n(&extras_ui,0,__ATOMIC_RELEASE);
    if(!base || !cinema_active || !cinema_type)return;
    for(unsigned i=0;i<sizeof check/sizeof *check;i++)
        if(memcmp((const void*)(base+check[i].addr),check[i].first,8)) {
            LOGF("EXTRAS disabled: opcode mismatch +%lx",check[i].addr);return;
        }
    if(*(const int64_t*)(base+0x9d55728+1*16+8)!=1
        || *(const int64_t*)(base+0x9d55728+9*16+8)!=9)return;
    for(unsigned i=1;i<=7;i++)
        if(*(const int64_t*)(base+0x9de3c10+i*16+8)!=(int64_t)i)return;
    if(*(uintptr_t*)(base+0xa361da8)!=base+0xa3f5e50)return;
    extras_system=(void**)(base+0xa3f5e50);
    extras_front_card_work=(void**)(base+0xa4ff390);
    extras_is_extra=(int (*)(unsigned))(base+0x61e1b94);
    extras_is_item=(int (*)(unsigned char))(base+0x61d1fac);
    extras_unlock_merc=(void (*)(void))(base+0x61d196c);
    extras_unlock_extra=(void (*)(unsigned))(base+0x61e1b28);
    extras_unlock_item=(void (*)(unsigned char))(base+0x61d1b34);
    extras_save_game=(void* (*)(void))(base+0x626dd40);
    extras_try_save=(int (*)(void))(base+0x626dc00);
    extras_save_class=(void* (*)(void))(base+0x64c2538);
    extras_menu_class=(void* (*)(void))(base+0x64b7218);
    extras_string_free=(void(*)(void*))(base+0x6615fd0);
    g_tgt_extras_level_name=base+0x8894238;
    extras_ready=1;
    LOGF("EXTRAS ready: explicit active-menu original unlocks; original menu selects content");
}
#endif
