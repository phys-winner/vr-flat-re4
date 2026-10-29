// Ammo counter and the laser-painted enemy for aim assist. Include after quickselect (touch_equipped_gun) and
// before re4fix_input.h (the HUD reads the snapshot).
#ifndef RE4FIX_HUD_EXTRA_H
#define RE4FIX_HUD_EXTRA_H
#include <stdint.h>
// Ammo counter: the held gun's own virtual getters (Gun, Bow, HarpoonGun and RocketLauncher override them), read only
// on the GameThread for the hand that actually holds the gun. Snapshot: (ms << 32) | reserves << 16 | current.
static volatile uint64_t hs_ammo;
static volatile long hs_reload_ready_ms;
static uintptr_t hs_ammo_base;
// Aim assist: AVR4GamePlayerGun::SetPaintTarget keeps the enemy under the laser sight in a cEmWrap at gun+0x930
// (original GetWepTargetPos acquisition; active while the native Laser Sight option or Classic Leon is on).
static int (*hs_em_alive)(const void*);
static int mobile_prefs_assist(void);
static int (*hs_aim_query)(float*,float*,unsigned,unsigned,void**,unsigned*);
static void touch_aim_assist_tick(void* gun,void* pawn,long now) {
    if(!gun || !pawn || !hs_aim_query || !aim_head || !mobile_prefs_assist() || g_touch_context!=1
        || g_touch_cancel || !rl_paused || rl_paused() || !rl_player_valid || !rl_player_valid())return;
    struct reload_tf head=aim_head(pawn);
    if(!reload_tf_valid(&head))return;
    head.q=q_mul(head.q,pose_camera_delta_ue());
    float direction[3];q_rot(head.q,(float[3]){1,0,0},direction);
    float range=*(const float*)((const char*)gun+0x60c);
    if(!isfinite(range) || range<=0 || range>1000000)return;
    // Same centimetres->Bio4 units/axis order and occluded target query as UpdateLaserSight, independent of its visual option.
    float start[3]={head.p[0]*10,head.p[2]*10,head.p[1]*10};
    float end[3]={start[0]+direction[0]*range*10,start[1]+direction[2]*range*10,start[2]+direction[1]*range*10};
    void* enemy=0;unsigned hit=0;
    int result=hs_aim_query(start,end,0,*(const unsigned char*)((const char*)gun+0xd20),&enemy,&hit);
    if(result==2 && enemy && (*(const unsigned*)((const char*)enemy+8)&0x201u)==1)
        __atomic_store_n(&g_aim_painted_ms,now,__ATOMIC_RELEASE);
}
static int hs_ammo_known(void* fn, const unsigned long* rvas, unsigned n) {
    for (unsigned i = 0; i < n; i++) if ((uintptr_t)fn == hs_ammo_base + rvas[i]) return 1;
    return 0;
}
static int hs_reload_possible(void* hand,void* gun) {
    if (g_touch_context!=1 || g_touch_cancel || g_reload_phase || g_touch_weapon_request
        || g_touch_pause || g_touch_inventory_request || g_touch_interact_request || touch_quickselect_busy()
        || !rl_player || !rl_active || !rl_left || !rl_primary || !rl_pistol_class || !rl_interactivity
        || !rl_paused || !rl_player_valid || !rl_ammo_available || !rl_reloading || !f_GetCurrentGrip
        || !reload_layout(hand,0x430) || !reload_layout(gun,0xd54)) return 0;
    unsigned side=*(const unsigned char*)((const char*)hand+0x314);
    void* pawn=rl_player(hand);
    if (side>1 || !pawn || pawn!=rl_active() || rl_paused() || !rl_player_valid()
        || rl_primary(gun)!=hand || rl_interactivity(gun)!=2 || !rl_ammo_available(gun) || rl_reloading(gun)) return 0;
    void* other=side ? rl_left(pawn) : rl_right ? rl_right(pawn) : 0;
    int angle=0xd50;
    if ((!reload_is_pistol(gun) && !reload_family_info(gun,&angle)) || !reload_layout(other,0x430)
        || *(const unsigned char*)((const char*)other+0x314)!=1-side || rl_player(other)!=pawn
        || angle<0xd38 || angle>0x1300 || !reload_layout(gun,angle+4)
        || !isfinite(*(const float*)((const char*)gun+angle))) return 0;
    void* grip=reload_ammo_primary(gun,pawn),*held=f_GetCurrentGrip(other),*holster=0;
    return grip && (!held || held==grip) && (held || reload_ammo_holster_grip(*(void* const*)((const char*)gun+0x8e0),pawn,&holster));
}
static int touch_hud_reload_needed(long now) {
    long ready=__atomic_load_n(&hs_reload_ready_ms,__ATOMIC_ACQUIRE);
    return ready>0 && now>=ready && now-ready<=400;
}
static void touch_hud_ammo_tick(void* hand, long now) {
    if (!hs_ammo_base || !hand || now <= 0 || (int)syscall(SYS_gettid) != __atomic_load_n(&g_touch_gt_tid, __ATOMIC_ACQUIRE)) return;
    void* pawn=rl_player ? rl_player(hand):0;
    if(!pawn || !rl_active || pawn!=rl_active())return;
    void* gun = touch_equipped_gun(hand);
    if (!gun) return;
    void* const* vtable = *(void* const* const*)gun;
    if (!vtable) return;
    static const unsigned long current_fn[] = {0x6291744, 0x627780c, 0x629d7b8, 0x62c557c};
    static const unsigned long reserve_fn[] = {0x62918d4, 0x629d7cc, 0x62c5594};
    void* current = vtable[0x7b8 / sizeof(void*)], *reserve = vtable[0x7d8 / sizeof(void*)];
    // The slots are proven by the vtables of Gun and its overriding subclasses; anything else is not read.
    if (!hs_ammo_known(current, current_fn, 4) || !hs_ammo_known(reserve, reserve_fn, 3)) return;
    if (hs_em_alive && hs_em_alive((const char*)gun + 0x930)) __atomic_store_n(&g_aim_painted_ms, now, __ATOMIC_RELEASE);
    touch_aim_assist_tick(gun,pawn,now);
    int loaded = ((int (*)(const void*, int))current)(gun, 0), spare = ((int (*)(const void*))reserve)(gun);
    if (loaded < 0 || loaded > 9999 || spare < 0 || spare > 9999) return;
    // Native AmmoAvailable also permits reinserting a loaded ejected magazine with zero reserves.
    __atomic_store_n(&hs_reload_ready_ms,loaded==0 && hs_reload_possible(hand,gun) ? now : 0,__ATOMIC_RELEASE);
    __atomic_store_n(&hs_ammo, ((uint64_t)(uint32_t)now << 32) | ((uint64_t)(unsigned)spare << 16) | (unsigned)loaded,
                     __ATOMIC_RELEASE);
}
static int touch_hud_ammo_read(long now, int* loaded, int* spare) {
    uint64_t s = __atomic_load_n(&hs_ammo, __ATOMIC_ACQUIRE);
    if (!s || (uint32_t)((uint32_t)now - (uint32_t)(s >> 32)) > 400) return 0;
    *loaded = (int)(s & 0xffffu); *spare = (int)((s >> 16) & 0xffffu);
    return 1;
}
static void install_hud_extra(unsigned long base) {
    // Gun vtable slots +0x7b8/+0x7d8 hold GetCurrentAmmo(bool)/GetAmmoReserves() (vtable for AVR4GamePlayerGun @0x9d1ad70).
    if (*(const uintptr_t*)(base+0x9d1ad70+0x10+0x7b8)==base+0x6291744
        && *(const uintptr_t*)(base+0x9d1ad70+0x10+0x7d8)==base+0x62918d4) hs_ammo_base=base;
    static const unsigned alive[4]={0x39402c08,0x7100051f,0x54000121,0xf9400008};
    static const unsigned paint[2]={0x9124c295,0xaa1503e0};   // SetPaintTarget: add x21,x20,#0x930; mov x0,x21
    if (!memcmp((const void*)(base+0x5d5c90c),alive,16) && !memcmp((const void*)(base+0x628d8b0),paint,8))
        hs_em_alive=(int (*)(const void*))(base+0x5d5c90c);
    static const unsigned query[4]={0xd10403ff,0x6d0733ed,0x6d082beb,0x6d0923e9};
    static const unsigned convert[2]={0x2d400800,0xbd400801};
    static const unsigned call[2]={0x2a1f03e2,0x97f0ea32};
    if(!memcmp((const void*)(base+0x5ec7190),query,sizeof query)
        && !memcmp((const void*)(base+0x61cba30),convert,sizeof convert)
        && !memcmp((const void*)(base+0x628c8c4),call,sizeof call))
        hs_aim_query=(int (*)(float*,float*,unsigned,unsigned,void**,unsigned*))(base+0x5ec7190);
    LOGF("HUD extra: ammo counter %s, laser paint %s", hs_ammo_base ? "ready" : "disabled (vtable mismatch)",
         hs_em_alive ? "ready" : "disabled");
}
#endif
