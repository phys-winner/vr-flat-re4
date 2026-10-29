// Include after the phone/reload/interact helpers; sample native state only on GameThread.
#ifndef RE4FIX_HUD_STATE_H
#define RE4FIX_HUD_STATE_H
#include <stdint.h>
#include <string.h>

struct mobile_hud_state { int current_health, max_health; unsigned command, action; };
static int (*hs_current)(void), (*hs_max)(void);
static void* (*hs_bio4)(void), *(*hs_hand_class)(void);
static volatile uint64_t hs_snapshot;
static unsigned hs_logs, hs_last_command, hs_last_action;
static int hs_last_current, hs_last_max;
static int (*hs_paused_for)(int);   // ABio4::IsPausedForReason (static): EVR4PauseReason Title..Debug = 0..6
// The first-run weapon tutorial pauses the world for Communicator/Game while the player pawn stays active;
// the health dial (and its wrist-watch tap) stays available there. Every other pause hides it.
static int hs_world_paused(void) {
    if (!rl_paused()) return 0;
    if (!hs_paused_for || !(hs_paused_for(1) || hs_paused_for(2))) return 1;
    for (int reason=0;reason<7;reason++) if (reason!=1 && reason!=2 && hs_paused_for(reason)) return 1;
    return 0;
}

// One atomic value carries both readings and their 24-bit millisecond timestamp.
static int touch_hud_state_read(long now, struct mobile_hud_state* out) {
    if (!out) return 0;
    memset(out,0,sizeof *out);
    if (now<=0 || __atomic_load_n(&g_touch_context,__ATOMIC_ACQUIRE)!=1
        || __atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE)) return 0;
    uint64_t s=__atomic_load_n(&hs_snapshot,__ATOMIC_ACQUIRE);
    unsigned age=((unsigned)now-(unsigned)(s>>40))&0xffffffu;
    if (!s || age>500) return 0;
    out->current_health=(int)(s&0x7fffu); out->max_health=(int)((s>>15)&0x7fffu);
    out->command=(unsigned)((s>>30)&7u); out->action=(unsigned)((s>>33)&0x7fu);
    return out->max_health>0 && out->current_health<=out->max_health;
}
static void touch_hud_state_tick(void* hand, long now) {
    if ((int)syscall(SYS_gettid)!=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE)) return;
    if (!hs_current || !hs_max || !hs_bio4 || !hs_hand_class || !phone_player || !phone_active
        || !phone_type || !rl_paused || !rl_player_valid || now<=0) {
        __atomic_store_n(&hs_snapshot,0,__ATOMIC_RELEASE); return;
    }
    if (phone_type()!=1 || hs_world_paused() || !rl_player_valid()
        || __atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE)) {
        __atomic_store_n(&hs_snapshot,0,__ATOMIC_RELEASE); return;
    }
    void* expected=hs_hand_class();
    const char* cls=hand ? *(const char* const*)((const char*)hand+0x10) : 0;
    int size=cls ? *(const int*)(cls+0x58) : 0;
    if (!expected || *(const int*)((const char*)expected+0x58)!=0x510
        || size<0x510 || size>0x10000 || !interact_is_a(hand,expected)
        || *(const unsigned char*)((const char*)hand+0x314)!=1) return;
    void* pawn=phone_player(hand);
    // Pooled hands must not replace the active player's snapshot with their old state.
    if (!pawn || pawn!=phone_active() || pawn!=hs_bio4()) return;
    int current=hs_current(), maximum=hs_max();
    if (current<0 || maximum<=0 || maximum>32767 || current>maximum) {
        __atomic_store_n(&hs_snapshot,0,__ATOMIC_RELEASE); return;
    }
    unsigned action=71, command=interact_prompt(&action);
    if (!command && interact_pickup_available(now)) { command=1;action=40; }
    uint64_t s=(uint64_t)(unsigned)current | ((uint64_t)(unsigned)maximum<<15)
        | ((uint64_t)command<<30) | ((uint64_t)action<<33)
        | ((uint64_t)((unsigned)now&0xffffffu)<<40);
    __atomic_store_n(&hs_snapshot,s,__ATOMIC_RELEASE);
    if (hs_logs<32 && (!hs_logs || command!=hs_last_command || action!=hs_last_action
        || current!=hs_last_current || maximum!=hs_last_max)) {
        ++hs_logs; hs_last_command=command; hs_last_action=action;
        hs_last_current=current; hs_last_max=maximum;
        LOGF("HUD native HP=%d/%d action=%u command=%u pawn=%p",current,maximum,action,command,pawn);
    }
}
static void install_touch_hud_state(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[2]; } check[] = {
        {0x621a8f8,{0xa9bf7bfd,0x910003fd}}, {0x621a930,{0xa9bf7bfd,0x910003fd}},
        {0x621a900,{0x97fee5c7,0x36000100}}, {0x621a938,{0x97fee5b9,0x36000100}},
        {0x621a910,{0x528a0809,0xf9400108}}, {0x621a918,{0x78e96900,0xa8c17bfd}},
        {0x621a948,{0x528a0849,0xf9400108}}, {0x621a950,{0x78e96900,0xa8c17bfd}},
        {0x63c6eac,{0x97f94e93,0xb9000260}}, {0x63c6e78,{0x97f94eae,0xb9000260}},
        {0x621984c,{0xf81e0ff3,0xa9017bfd}}, {0x6424b5c,{0xd10183ff,0xf90023f3}},
        {0x6424be4,{0x5280a204,0x52800205}},
    };
    if (!base || hs_current) return;
    for (unsigned i=0;i<sizeof check/sizeof *check;i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,8)) {
            LOGF("HUD disabled: native opcode mismatch +%lx",check[i].addr); return;
        }
    if (!ti_ready || phone_player!=(void* (*)(const void*))(base+0x633d030)
        || phone_active!=(void* (*)(void))(base+0x63380dc)
        || phone_type!=(unsigned (*)(void))(base+0x6338aec)
        || rl_paused!=(int (*)(void))(base+0x61d0934)
        || rl_player_valid!=(int (*)(void))(base+0x61d401c)) return;
    hs_bio4=(void* (*)(void))(base+0x621984c);
    hs_hand_class=(void* (*)(void))(base+0x6424b5c);
    hs_max=(int (*)(void))(base+0x621a930);
    hs_current=(int (*)(void))(base+0x621a8f8);
    static const unsigned paused_for[2]={0xb0020ca8,0xf9446508};
    hs_paused_for=memcmp((const void*)(base+0x61d0f54),paused_for,sizeof paused_for) ? 0 : (int (*)(int))(base+0x61d0f54);
    LOGF("HUD ready: native read-only health and authored semantic face action");
}
#endif
