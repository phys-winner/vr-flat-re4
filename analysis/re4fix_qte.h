// Native prompt state and ordinary input; the game owns QTE progress and completion.
#ifndef RE4FIX_QTE_H
#define RE4FIX_QTE_H
#include <stdint.h>

struct mobile_action_state { unsigned command,action,qte,motion,context; };
static uint64_t qt_snapshot,qt_pose;
static int qt_ready;
static unsigned (*qt_motion_type)(void);
static int (*qt_has_motion)(unsigned);
static const void* (*qt_settings)(void);
static unsigned qt_mask;
static long qt_until,qt_prior[32];
static void* qt_pawn;
static unsigned long long qt_name;
static unsigned qt_logs;
static int qt_reset_pending;

static unsigned touch_qte_mask(unsigned command) {
    switch (command) {
        case 1: case 10: return 1u;
        case 2: case 11: return 2u;
        case 3: return 0x100u;
        case 4: return 0x200u;
        case 5: return 0x10000000u;
        case 6: case 15: return 0x04000000u;
        case 7: return 3u;
        case 8: return 0x14000000u;
        default: return 0;
    }
}
static int touch_action_state(long now,struct mobile_action_state* out) {
    if (!out) return 0;
    memset(out,0,sizeof *out);
    uint64_t s=__atomic_load_n(&qt_snapshot,__ATOMIC_ACQUIRE);
    unsigned context=(unsigned)((s>>24)&3u);
    if (!s || now<=0 || (uint32_t)((uint32_t)now-(uint32_t)(s>>32))>500
        || context!=(unsigned)__atomic_load_n(&g_touch_context,__ATOMIC_ACQUIRE)
        || __atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE)) return 0;
    out->command=(unsigned)(s&31u); out->action=(unsigned)((s>>5)&127u);
    out->qte=(unsigned)((s>>12)&255u); out->motion=(unsigned)((s>>20)&15u);
    out->context=context;
    return out->command>0 && out->command<=16 && (context==1 || context==3);
}
static void touch_qte_clear_keys(void) {
    for (int bit=0;bit<32;bit++) if (qt_mask&(1u<<bit)) {
        long expected=qt_until;
        __atomic_compare_exchange_n(&g_cmd_until[bit],&expected,qt_prior[bit],0,
            __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
    }
    qt_mask=0; qt_until=0;
}
static void touch_qte_cancel(void) {
    __atomic_store_n(&qt_snapshot,0,__ATOMIC_RELEASE);
    __atomic_store_n(&qt_pose,0,__ATOMIC_RELEASE);
    touch_qte_clear_keys();
}
static void touch_qte_reset(void) {
    __atomic_store_n(&qt_snapshot,0,__ATOMIC_RELEASE);
    __atomic_store_n(&qt_pose,0,__ATOMIC_RELEASE);
    __atomic_store_n(&qt_reset_pending,1,__ATOMIC_RELEASE);
}
static int touch_qte_keys(unsigned mask,long now) {
    if (!mask || qt_mask || now<=0) return 0;
    for (int bit=0;bit<32;bit++) if (mask&(1u<<bit)) {
        qt_prior[bit]=__atomic_load_n(&g_cmd_until[bit],__ATOMIC_ACQUIRE);
        if (qt_prior[bit]>now) return 0;
    }
    qt_until=now+90;
    for (int bit=0;bit<32;bit++) if (mask&(1u<<bit)) {
        long expected=qt_prior[bit];
        if (!__atomic_compare_exchange_n(&g_cmd_until[bit],&expected,qt_until,0,
            __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) { touch_qte_clear_keys(); return 0; }
        qt_mask|=1u<<bit;
    }
    return 1;
}
static int touch_qte_motion_active(long now) {
    struct mobile_action_state s;
    return touch_action_state(now,&s) && s.qte && (s.command==9 || s.command>=12);
}
static int touch_qte_pose_busy(long now) {
    uint64_t pose=__atomic_load_n(&qt_pose,__ATOMIC_ACQUIRE);
    return pose && (uint32_t)((uint32_t)now-(uint32_t)pose)<640 && touch_qte_motion_active(now);
}
static void touch_qte_pose(int side,long now,float off[3]) {
    struct mobile_action_state state;
    uint64_t p=__atomic_load_n(&qt_pose,__ATOMIC_ACQUIRE);
    if (!p || side<0 || side>1 || !off || !touch_action_state(now,&state)) return;
    uint32_t elapsed=(uint32_t)now-(uint32_t)p;
    if (elapsed>=640 || !state.qte || (state.command!=9 && state.command<12)) return;
    unsigned motion=(unsigned)((p>>32)&15u),dominant=(unsigned)((p>>36)&1u);
    if (motion!=state.motion) return;
    float d=knife_cut_offset((long)elapsed);
    switch (motion) {
        case 0: off[0]+=(side ? d : -d); break;
        case 1: off[1]-=d; break;
        case 2: off[1]+=d; break;
        case 3: if ((unsigned)side==dominant) off[1]+=d; break;
        case 4: if ((unsigned)side==dominant) off[1]-=d; break;
        case 5: if ((unsigned)side==dominant) off[0]+=d; break;
        case 6: if ((unsigned)side==dominant) off[0]-=d; break;
        case 7: off[0]+=d; break;
        case 8: off[0]-=d; break;
        case 9: off[0]+=(side ? d : -d); off[1]+=d; break;
        case 10: off[0]+=(side ? d : -d); off[1]-=d; break;
        case 11: off[1]+=(side ? d : -d); break;
        default: break;
    }
}
static void touch_qte_tick(void* hand,int side,long now) {
    if (!qt_ready || side!=0 || now<=0
        || (int)syscall(SYS_gettid)!=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE)) return;
    if (__atomic_exchange_n(&qt_reset_pending,0,__ATOMIC_ACQ_REL)) touch_qte_clear_keys();
    if (!hand || !phone_player || !phone_active || !phone_type || !hs_hand_class
        || !ti_manager || !ti_manager_class || !ti_action_active || !ti_can_action
        || !rl_paused || !rl_player_valid) { touch_qte_cancel(); return; }
    const char* cls=*(const char* const*)((const char*)hand+0x10);
    void* expected=hs_hand_class();
    if (!cls || !expected || *(const int*)((const char*)expected+0x58)!=0x510
        || *(const int*)(cls+0x58)<0x510 || *(const int*)(cls+0x58)>0x10000
        || !interact_is_a(hand,expected) || *((const unsigned char*)hand+0x314)!=0) return;
    void* pawn=phone_player(hand),*active=phone_active();
    unsigned context=phone_type();
    if (qt_pawn && active!=qt_pawn) touch_qte_cancel();
    if (!pawn || pawn!=active) return;
    unsigned long long identity; memcpy(&identity,(const char*)pawn+0x18,8);
    if ((qt_pawn && (qt_pawn!=pawn || qt_name!=identity))
        || __atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE)
        || (context!=1 && context!=3) || rl_paused() || !rl_player_valid()) {
        touch_qte_cancel(); qt_pawn=pawn; qt_name=identity;
        if ((context!=1 && context!=3) || rl_paused()
            || __atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE)) return;
    }
    qt_pawn=pawn; qt_name=identity;
    if (qt_mask && now>=qt_until) touch_qte_clear_keys();
    if (!ti_action_active() || !ti_can_action()) { touch_qte_cancel(); return; }
    void* manager=ti_manager(),*manager_class=ti_manager_class();
    cls=manager ? *(const char* const*)((const char*)manager+0x10) : 0;
    if (!cls || !manager_class || *(const int*)((const char*)manager_class+0x58)!=0x410
        || *(const int*)(cls+0x58)<0x410 || *(const int*)(cls+0x58)>0x10000
        || !interact_is_a(manager,manager_class)) { touch_qte_cancel(); return; }
    unsigned command=*((const unsigned char*)manager+0x3d9);
    unsigned action=*((const unsigned char*)manager+0x3da),qte=*((const unsigned char*)manager+0x3dc);
    unsigned motion=11;
    if (!command || command>16 || action>=72 || action==48) { touch_qte_cancel(); return; }
    if (qte && qt_has_motion(qte)) {
        motion=qt_motion_type();
        if (motion>11) { touch_qte_cancel(); return; }
    }
    uint64_t snapshot=(uint64_t)command|((uint64_t)action<<5)|((uint64_t)qte<<12)
        |((uint64_t)motion<<20)|((uint64_t)context<<24)|((uint64_t)(uint32_t)now<<32);
    uint64_t prior=__atomic_load_n(&qt_snapshot,__ATOMIC_ACQUIRE);
    if ((prior&0x3ffffffu)!=(snapshot&0x3ffffffu)) {
        __atomic_store_n(&qt_pose,0,__ATOMIC_RELEASE); touch_qte_clear_keys();
        if (qt_logs++<32) LOGF("QTE native action=%u command=%u id=%u motion=%u context=%u",action,command,qte,motion,context);
    }
    __atomic_store_n(&qt_snapshot,snapshot,__ATOMIC_RELEASE);
    if (context==1 && command<=4) return; // Existing ordinary interaction owns these edges.
    if (!__atomic_exchange_n(&g_touch_interact_request,0,__ATOMIC_ACQ_REL)) return;
    unsigned mask=touch_qte_mask(command);
    if (mask) { touch_qte_keys(mask,now); return; }
    if (!qte || (command!=9 && command<12)) return;
    uint64_t pose=__atomic_load_n(&qt_pose,__ATOMIC_ACQUIRE);
    if (pose && (uint32_t)((uint32_t)now-(uint32_t)pose)<640) return;
    const unsigned char* settings=qt_settings();
    if (!settings || settings[0x32]>1) return;
    unsigned dominant=settings[0x32] ? 0u : 1u;
    __atomic_store_n(&qt_pose,(uint64_t)(uint32_t)now|((uint64_t)motion<<32)
        |((uint64_t)dominant<<36),__ATOMIC_RELEASE);
}
static void install_touch_qte(unsigned long base) {
    qt_ready=0;
    if (!ti_ready || !hs_hand_class) return;
    static const struct { unsigned long addr; unsigned first[2]; } checks[]={
        {0x61fe024,{0xf0020b28,0xf9450908}}, {0x61fe074,{0x39441100,0xd65f03c0}},
        {0x61fe0e4,{0xf0020b28,0xf9450908}}, {0x61fe134,{0xf9401509,0xb4000249}},
        {0x62c7e04,{0xa9be4ff4,0xa9017bfd}}, {0x61f632c,{0x3dc00100,0xf9400908}},
        {0x61f636c,{0x97fffbc3,0xf85503b4}}, {0x61f654c,{0x3dc00100,0xf9400908}},
        {0x61f65ac,{0x97fffb33,0xf940c3f4}}, {0x61f6664,{0x3dc00100,0xf9400908}},
        {0x61f5d88,{0x940020a7,0x12001c08}}, {0x61f5da4,{0x9400260a,0x2a0003f4}},
    };
    for (unsigned i=0;i<sizeof checks/sizeof checks[0];i++)
        if (memcmp((const void*)(base+checks[i].addr),checks[i].first,sizeof checks[i].first)) {
            LOGI("QTE disabled: native instruction mismatch"); return;
        }
    static const char* names[17]={"Invalid","A","B","X","Y","LB","RB","A_B","LB_RB",
        "Rotate","A_Rapid","B_Rapid","Stick_A","Stick_Up","Waggle","LB_or_RB","HandMotion"};
    for (unsigned i=0;i<17;i++) {
        const unsigned char* pair=(const unsigned char*)(base+0x9d92268+16*i);
        const char* name; long long value; memcpy(&name,pair,8); memcpy(&value,pair+8,8);
        if (!name || value!=i || strncmp(name,"EVR4CommandButton::",19)
            || strcmp(name+19,names[i])) { LOGI("QTE disabled: native enum mismatch"); return; }
    }
    static const char* motions[13]={"SidesOut","BothDown","BothUp","DominantUp","DominantDown",
        "DominantRight","DominantLeft","AcrossRight","AcrossLeft","UpperV","LowerV","Button","Count"};
    for(unsigned i=0;i<13;i++) {
        const unsigned char* pair=(const unsigned char*)(base+0x9ded060+16*i);
        const char* name;long long value;memcpy(&name,pair,8);memcpy(&value,pair+8,8);
        const char* prefix="EMotionActionType::";
        if(!name || value!=i || strncmp(name,prefix,strlen(prefix))
            || strcmp(name+strlen(prefix),motions[i])) {LOGI("QTE disabled: native motion enum mismatch");return;}
    }
    const unsigned char* params=(const unsigned char*)(base+0x9d92378);
    unsigned count,offset; memcpy(&count,params+40,4);
    memcpy(&offset,(const void*)(base+0x9dbf730+36),4);
    if (count!=17 || offset!=0x3dc || *(const unsigned*)(base+0x9ded130+40)!=13) { LOGI("QTE disabled: native metadata mismatch"); return; }
    qt_motion_type=(unsigned (*)(void))(base+0x61fe024);
    qt_has_motion=(int (*)(unsigned))(base+0x61fe0e4);
    qt_settings=(const void* (*)(void))(base+0x62c7e04);
    qt_ready=1; LOGI("QTE native prompt/input adapter installed");
}
#endif
