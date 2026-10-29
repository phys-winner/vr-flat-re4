// Read the original consumable timer on GameThread; the overlay reads one fresh atomic snapshot.
#ifndef RE4FIX_HEAL_PROGRESS_H
#define RE4FIX_HEAL_PROGRESS_H
#include <stdint.h>
static void* (*heal_class)(void);
static float (*heal_fraction)(const void*);
static int (*heal_interactivity)(const void*),(*heal_trigger)(const void*);
static uint64_t heal_snapshot;

static int touch_heal_progress_read(long now,float* progress) {
    if(!progress)return 0;
    *progress=0;
    int context=__atomic_load_n(&g_touch_context,__ATOMIC_ACQUIRE);
    if(now<=0 || (context!=1 && context!=2) || __atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE))return 0;
    uint64_t snapshot=__atomic_load_n(&heal_snapshot,__ATOMIC_ACQUIRE);
    if(!snapshot || (unsigned)((unsigned)now-(unsigned)(snapshot>>32))>200)return 0;
    unsigned bits=(unsigned)snapshot;float value;memcpy(&value,&bits,4);
    if(!isfinite(value) || value<=0 || value>=1)return 0;
    *progress=value;return 1;
}
static void touch_heal_progress_tick(void* hand,long now) {
    if(!qs_gt())return;
    if(!heal_fraction || !heal_class || !heal_interactivity || !heal_trigger || now<=0
        || g_touch_cancel || !phone_type || !phone_active || !phone_player
        || !tw_hand_class || !tw_pawn_class || !tw_grip_class || !tw_consumable
        || !qs_native_hand || !rl_primary || !f_GetCurrentGrip || !rl_paused || !rl_player_valid) {
        __atomic_store_n(&heal_snapshot,0,__ATOMIC_RELEASE);return;
    }
    unsigned type=phone_type();
    if((type!=1 && type!=2) || g_touch_context!=(int)type || !rl_player_valid() || (type==1 && rl_paused())) {
        __atomic_store_n(&heal_snapshot,0,__ATOMIC_RELEASE);return;
    }
    if(!tw_typed(hand,tw_hand_class(),0x510) || *(const unsigned char*)((const char*)hand+0x314)!=1)return;
    void* pawn=phone_player(hand);
    if(!pawn || pawn!=phone_active() || !tw_typed(pawn,tw_pawn_class(),0x10d0) || qs_native_hand(pawn,1)!=hand)return;
    uint64_t snapshot=0;
    void* item=tw_consumable(pawn);
    if(tw_typed(item,heal_class(),0x5d0) && *(void* const*)((const char*)item+0x3c0)==pawn
        && !*(const unsigned char*)((const char*)item+0x5c4) && heal_interactivity(item)==2) {
        void* inventory_hand=*(void* const*)((const char*)item+0x5a0);
        void* primary=inventory_hand ? inventory_hand : rl_primary(item);
        int native_hand=primary && (primary==hand || primary==qs_native_hand(pawn,0));
        int using_item=inventory_hand && native_hand;
        if(!inventory_hand && native_hand && tw_typed(primary,tw_hand_class(),0x510) && phone_player(primary)==pawn
            && *(const unsigned char*)((const char*)item+0x4f0)==1 && heal_trigger(primary)) {
            void* grip=*(void* const*)((const char*)item+0x270);
            using_item=reload_grip_registered(grip) && tw_typed(grip,tw_grip_class(),0x460)
                && *(void* const*)((const char*)grip+0x20)==item && f_GetCurrentGrip(primary)==grip;
        }
        if(using_item) {
            unsigned long long name=qs_name(item),pawn_name=qs_name(pawn);
            float fraction=heal_fraction(item);
            if(isfinite(fraction) && fraction>0 && fraction<1 && qs_name(item)==name && qs_name(pawn)==pawn_name
                && phone_active()==pawn && phone_type()==type && g_touch_context==(int)type && !g_touch_cancel) {
                unsigned bits;memcpy(&bits,&fraction,4);snapshot=((uint64_t)(unsigned)now<<32)|bits;
            }
        }
    }
    __atomic_store_n(&heal_snapshot,snapshot,__ATOMIC_RELEASE);
}
static void install_touch_heal_progress(unsigned long base) {
    static const struct {unsigned long addr;unsigned first[2];} code[]={
        {0x6278e78,{0x9113e000,0x52800021}}, {0x62e9178,{0xbd401000,0xbd400001}},
        {0x641231c,{0xd10183ff,0xf90023f3}}, {0x64123a4,{0x5280ba04,0x52800205}},
        {0x627890c,{0xf81e0ff3,0xa9017bfd}}, {0x633d040,{0x394cf000,0xd65f03c0}},
        {0x6278034,{0x39571268,0x35001288}}, {0x627811c,{0x3953c268,0x34000b48}},
        {0x62791fc,{0xf902d001,0xd65f03c0}},
        {0x62a8fc0,{0xf81e0ff3,0xa9017bfd}},
    };
    if(!base || heal_fraction)return;
    for(unsigned i=0;i<sizeof code/sizeof *code;i++)
        if(memcmp((const void*)(base+code[i].addr),code[i].first,8))return;
    if(*(const unsigned*)(base+0x9d99168+36)!=0x4f8
        || *(const unsigned*)(base+0x9d99110+36)!=0x5a0
        || tw_consumable!=(void* (*)(const void*))(base+0x62ac400)
        || phone_player!=(void* (*)(const void*))(base+0x633d030)
        || phone_active!=(void* (*)(void))(base+0x63380dc)
        || phone_type!=(unsigned (*)(void))(base+0x6338aec)
        || qs_native_hand!=(void* (*)(const void*,unsigned char))(base+0x62a8fc0))return;
    heal_class=(void* (*)(void))(base+0x641231c);
    heal_interactivity=(int (*)(const void*))(base+0x627890c);
    heal_trigger=(int (*)(const void*))(base+0x633d040);
    heal_fraction=(float (*)(const void*))(base+0x6278e78);
    LOGF("HEAL progress ready: original held/inventory consumable timer, read-only");
}
#endif
