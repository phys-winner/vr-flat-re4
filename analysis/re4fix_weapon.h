// Include after reload/phone helpers; the caller owns forced hover and release/regrip.
static void* (*tw_knife)(const void*);
static void* (*tw_holster)(const void*);
static void* (*tw_consumable)(const void*);   // AVR4GamePlayerPawn::GetConsumable: the healing item in its holster
static void* (*tw_hand_class)(void), *(*tw_pawn_class)(void), *(*tw_grip_class)(void);
struct tw_binding { void* object[6]; unsigned long long name[6]; };
static struct { struct tw_binding binding; int weapon,reason,waiting_release; long until,next; } tw_retry;
static unsigned tw_retry_logs;
static int (*tw_paused_for)(int);   // ABio4::IsPausedForReason (static), EVR4PauseReason Title..Debug = 0..6
// The first-run weapon-management tutorial runs inside the first call: the Bio4 world stays paused for reason
// Communicator (2) (Game (1) for its menus) while the player practises with the hands, and the native grab stays
// live. Title/Cutscene/Death/OculusDash/Debug still block the holster grab; call and menu pawns never reach it
// because the binding requires the active gameplay pawn.
static int tw_grab_paused(void) {
    if (!rl_paused()) return 0;
    if (!tw_paused_for || !(tw_paused_for(1) || tw_paused_for(2))) return 1;
    for (int reason=0;reason<7;reason++) if (reason!=1 && reason!=2 && tw_paused_for(reason)) return 1;
    return 0;
}

static int tw_typed(const void* object,const void* expected,int minimum) {
    const char* cls=object ? *(const char* const*)((const char*)object+0x10) : 0;
    int size=cls ? *(const int*)(cls+0x58) : 0;
    if (!expected || size<minimum || size>0x10000) return 0;
    int depth=*(const int*)((const char*)expected+0x38),child=*(const int*)(cls+0x38);
    const void* const* chain=*(const void* const* const*)(cls+0x30);
    return depth>=0 && depth<=128 && child>=depth && child<=256 && chain
        && chain[depth]==(const char*)expected+0x30;
}

static int tw_binding(void* hand,int weapon,struct tw_binding* binding) {
    if (!tw_hand_class || !tw_pawn_class || !tw_grip_class || !ti_prop_class || !ti_holster_class
        || !ti_parent || !tw_knife || !tw_holster || (weapon==2 && !rl_gun1) || (weapon==3 && !tw_consumable)
        || !phone_player || !phone_active || !phone_type
        || (weapon!=1 && weapon!=2 && weapon!=3) || !tw_typed(hand,tw_hand_class(),0x510)
        || *(const unsigned char*)((const char*)hand+0x314)!=1 || phone_type()!=1) return 0;
    void* pawn=phone_player(hand);
    if (!pawn || pawn!=phone_active() || !tw_typed(pawn,tw_pawn_class(),0x10d0)) return 0;
    void* item=weapon==1 ? tw_knife(pawn) : weapon==2 ? rl_gun1(pawn) : tw_consumable(pawn);
    if (!tw_typed(item,ti_prop_class(),weapon==2 ? 0xd40 : 0x4e0)
        || *(void* const*)((const char*)item+0x3c0)!=pawn) return 0;
    void* holster=tw_holster(item);
    if (!tw_typed(holster,ti_holster_class(),0x360)
        || *(void* const*)((const char*)holster+0x348)!=pawn) return 0;
    const void* const* props=*(const void* const* const*)((const char*)holster+0x350);
    int num=*(const int*)((const char*)holster+0x358),max=*(const int*)((const char*)holster+0x35c);
    if (!props || num<1 || num>32 || max<num || max>128) return 0;
    int found=0;
    for (int i=0;i<num;i++) {
        if (props[i] && props[i]!=item) return 0;
        if (props[i]==item) found=1;
    }
    void* grip=*(void* const*)((const char*)holster+0x248);
    void* primary=*(void* const*)((const char*)item+0x270);
    if (!found || !reload_grip_registered(grip) || !reload_grip_registered(primary)
        || !tw_typed(grip,tw_grip_class(),0x460) || !tw_typed(primary,tw_grip_class(),0x460)
        || *(void* const*)((const char*)grip+0x20)!=holster
        || *(void* const*)((const char*)primary+0x20)!=item
        || !ti_parent(grip) || !ti_parent(primary)
        || *(void* const*)((const char*)grip+0x300)!=holster
        || *(void* const*)((const char*)primary+0x300)!=item) return 0;
    void* objects[6]={hand,pawn,item,holster,grip,primary};
    for (int i=0;i<6;i++) {
        binding->object[i]=objects[i];
        memcpy(&binding->name[i],(const char*)objects[i]+0x18,8);
    }
    return 1;
}

static void tw_retry_log(int reason,const void* best,const void* current,int down,int allowed) {
    if (reason==tw_retry.reason || tw_retry_logs>=8) return;
    tw_retry.reason=reason;++tw_retry_logs;
    LOGF("WEAPON RETRY reason=%d best=%p down=%d allowed=%d target=%p primary=%p current=%p",
         reason,best,down,allowed,tw_retry.binding.object[4],tw_retry.binding.object[5],current);
}

static int tw_retry_end(int reason,const void* current) {
    tw_retry_log(reason,0,current,-1,-1);
    int owned=g_mobile_holster==tw_retry.binding.object[3]
        && g_mobile_holster_hand==tw_retry.binding.object[0]
        && __atomic_load_n(&g_mobile_holster_until,__ATOMIC_ACQUIRE)==tw_retry.until;
    if (owned) __atomic_store_n(&g_mobile_holster_until,0,__ATOMIC_RELEASE);
    memset(&tw_retry,0,sizeof tw_retry);
    return owned;
}

static void touch_weapon_retry_cancel(void* hand) {
    int gt=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE);
    if (gt<=0 || gettid()!=gt) return;
    if (hand && hand==tw_retry.binding.object[0]) (void)tw_retry_end(6,0);
}

static void touch_weapon_request_started(void* hand,int weapon,long now) {
    int gt=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE);
    if (gt<=0 || gettid()!=gt) return;
    memset(&tw_retry,0,sizeof tw_retry);
    if (!ti_ready || !ti_best || !ti_down || !ti_press || !ti_release || !ti_free
        || !f_GetCurrentGrip || !rl_primary || !rl_paused || !rl_player_valid
        || now<=0 || __atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE)
        || !tw_binding(hand,weapon,&tw_retry.binding)) {memset(&tw_retry,0,sizeof tw_retry);return;}
    long until=__atomic_load_n(&g_mobile_holster_until,__ATOMIC_ACQUIRE);
    if (until<=now || until-now>1800 || g_mobile_holster!=tw_retry.binding.object[3]
        || g_mobile_holster_hand!=hand) {memset(&tw_retry,0,sizeof tw_retry);return;}
    tw_retry.weapon=weapon;tw_retry.until=until;tw_retry.next=now+250;
    tw_retry.waiting_release=f_GetCurrentGrip(hand)!=0;
}

// Return true only to retire this request's existing forced-hover window in the caller.
static int touch_weapon_retry_tick(void* hand,long now) {
    int gt=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE);
    if (gt<=0 || gettid()!=gt || !hand || hand!=tw_retry.binding.object[0]) return 0;
    if (now<=0 || tw_retry.until-now>1800 || __atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE)
        || __atomic_load_n(&g_touch_weapon_request,__ATOMIC_ACQUIRE)
        || __atomic_load_n(&g_touch_weapon,__ATOMIC_ACQUIRE)!=tw_retry.weapon
        || !phone_type || phone_type()!=1) return tw_retry_end(6,0);
    struct tw_binding binding;
    if (!tw_binding(hand,tw_retry.weapon,&binding)
        || memcmp(&binding,&tw_retry.binding,sizeof binding)) return tw_retry_end(7,0);
    void* current=f_GetCurrentGrip(hand);
    if (tw_retry.waiting_release) {
        // The existing selector first emits RG-up; retain its hover through that 250ms release window.
        if (current && now<tw_retry.until-1550) return 0;
        tw_retry.waiting_release=0;
    }
    if (current) return tw_retry_end(current==binding.object[5]
        && rl_primary(binding.object[2])==hand ? 4 : 5,current);
    if (tw_grab_paused() || !rl_player_valid() || g_reload_phase
        || __atomic_load_n(&g_touch_reload_request,__ATOMIC_ACQUIRE)) return tw_retry_end(6,0);
    if (now>=tw_retry.until) {
        // No later native held-input grab may survive an unsuccessful bounded request.
        if (ti_down(hand)) ti_release(hand);
        return tw_retry_end(8,0);
    }
    if (g_mobile_holster!=binding.object[3] || g_mobile_holster_hand!=hand
        || __atomic_load_n(&g_mobile_holster_until,__ATOMIC_ACQUIRE)!=tw_retry.until) return tw_retry_end(7,0);
    if (now<tw_retry.next) return 0;
    tw_retry.next=now+250;
    int down=ti_down(hand),allowed=0;
    void** vtable=*(void***)hand;
    if (vtable && vtable[0x740/sizeof(void*)])
        allowed=((int (*)(const void*))vtable[0x740/sizeof(void*)])(hand);
    void* best=down && allowed ? ti_best(hand) : 0;
    if (!down || !allowed || best!=binding.object[4] || !ti_free(binding.object[4],1)) {
        tw_retry_log(!down ? 1 : !allowed ? 2 : 3,best,0,down,allowed);return 0;
    }
    struct tw_binding final_binding;
    if (__atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE)
        || __atomic_load_n(&g_touch_weapon_request,__ATOMIC_ACQUIRE)
        || __atomic_load_n(&g_touch_weapon,__ATOMIC_ACQUIRE)!=tw_retry.weapon
        || !tw_binding(hand,tw_retry.weapon,&final_binding)
        || memcmp(&final_binding,&tw_retry.binding,sizeof final_binding)
        || g_mobile_holster!=binding.object[3] || g_mobile_holster_hand!=hand
        || __atomic_load_n(&g_mobile_holster_until,__ATOMIC_ACQUIRE)!=tw_retry.until
        || tw_grab_paused() || !rl_player_valid() || !ti_down(hand) || f_GetCurrentGrip(hand)) return tw_retry_end(6,0);
    long call_time=ptr_now_ms();
    if (call_time<now || call_time>=tw_retry.until) {
        if (ti_down(hand)) ti_release(hand);
        return tw_retry_end(8,0);
    }
    // Normal input preserves OnGripPressed/CanAcceptProps and the authored holster delegate.
    ti_press(hand);
    current=f_GetCurrentGrip(hand);
    if (current) return tw_retry_end(current==binding.object[5]
        && rl_primary(binding.object[2])==hand ? 4 : 5,current);
    tw_retry_log(9,best,0,down,allowed);
    return 0;
}

static int touch_holster_hover_allowed(void* holster, void* hand, long now) {
    // Screen selection replaces reaching this one holster; native grab eligibility still applies.
    return holster && hand && holster == g_mobile_holster && hand == g_mobile_holster_hand
        && now < __atomic_load_n(&g_mobile_holster_until, __ATOMIC_ACQUIRE) && phone_type && phone_player && phone_active
        && phone_type() == 1 && phone_player(hand) == phone_active()
        && f_GetCurrentGrip && !f_GetCurrentGrip(hand);
}

static void* touch_weapon_grip(void* hand, int weapon) {
    int gt=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE);
    if (gt<=0 || gettid()!=gt) return 0;
    struct tw_binding binding;
    return tw_binding(hand,weapon,&binding) ? binding.object[4] : 0;
}

static void install_touch_weapon(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[2]; } check[] = {
        {0x62ac22c,{0xf81e0ff3,0xa9017bfd}}, {0x62ac238,{0xf9476013,0xb40002f3}},
        {0x62bbd7c,{0xf941e808,0xb4000268}}, {0x62bbd24,{0xf941e008,0xb4000268}},
        {0x687d798,{0x294b2408,0x2a0203f6}}, {0x62c0418,{0xf941a400,0xb4ffff00}},
        {0x62c0510,{0xf9412669,0xf8357909}}, {0x633d038,{0x394c5000,0xd65f03c0}},
        {0x69b6500,{0xaa0003e8,0xf9401000}}, {0x6283f38,{0xf9405035,0x91372000}},
        {0x6283f80,{0xaa1503e0,0xaa1603e1}}, {0x62be074,{0x91092260,0x91094261}},
        {0x6424b5c,{0xd10183ff,0xf90023f3}}, {0x642c718,{0xd10183ff,0xf90023f3}},
        {0x64192f4,{0xd10183ff,0xf90023f3}}, {0x6424be4,{0x5280a204,0x52800205}},
        {0x641937c,{0x52808c04,0x52800205}}, {0x61d0f54,{0xb0020ca8,0xf9446508}},
        {0x62ac400,{0xf81e0ff3,0xa9017bfd}}, {0x62ac40c,{0xf9476c13,0xb40002f3}},
    };
    static const struct { unsigned long addr;unsigned offset; } fields[]={
        {0x9db0e60,0x270},{0x9db1dd8,0x350},{0x9db1e38,0x348},{0x9d9e298,0x300},
    };
    tw_knife = 0; tw_holster = 0;tw_consumable=0;tw_hand_class=tw_pawn_class=tw_grip_class=0;tw_paused_for=0;
    for (unsigned i = 0; i < sizeof check/sizeof check[0]; i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,sizeof check[i].first)) {
            LOGF("WEAPONGRIP disabled: opcode mismatch +%lx",check[i].addr); return;
        }
    for (unsigned i=0;i<sizeof fields/sizeof *fields;i++)
        if (*(const unsigned*)(base+fields[i].addr+0x24)!=fields[i].offset) return;
    tw_knife = (void* (*)(const void*))(base+0x62ac22c);
    tw_holster = (void* (*)(const void*))(base+0x62bbd7c);
    if (!ti_ready || ti_best!=(void* (*)(const void*))(base+0x629a930)
        || ti_press!=(void (*)(void*))(base+0x633c54c)
        || ti_release!=(void (*)(void*))(base+0x633c558)
        || ti_down!=(int (*)(const void*))(base+0x633d048)
        || ti_free!=(int (*)(const void*,int))(base+0x627feb8)
        || ti_parent!=(void* (*)(const void*))(base+0x6280170)
        || ti_prop_class!=(void* (*)(void))(base+0x6432600)
        || ti_holster_class!=(void* (*)(void))(base+0x6434184)
        || rl_primary!=(void* (*)(const void*))(base+0x62bbc30)
        || rl_paused!=(int (*)(void))(base+0x61d0934)
        || rl_player_valid!=(int (*)(void))(base+0x61d401c)
        || f_GetCurrentGrip!=(void* (*)(const void*))(base+0x629a774)) return;
    tw_hand_class=(void* (*)(void))(base+0x6424b5c);
    tw_pawn_class=(void* (*)(void))(base+0x642c718);
    tw_grip_class=(void* (*)(void))(base+0x64192f4);
    tw_paused_for=(int (*)(int))(base+0x61d0f54);
    tw_consumable=(void* (*)(const void*))(base+0x62ac400);
    LOGF("WEAPONGRIP ready (active pawn's assigned holster)");
}
