// Include after phone/reload helpers and patch_code. The native visibility writer owns recovery.
#ifndef RE4FIX_CUTSCENE_WEAPON_H
#define RE4FIX_CUTSCENE_WEAPON_H
static void* (*cw_pawn_class)(void), *(*cw_hand_class)(void), *(*cw_gun_class)(void);
static void* (*cw_poseable_class)(void), *(*cw_scene_class)(void);
static void* (*cw_meshset_class)(void), *(*cw_skeletal_class)(void);
static void* (*cw_get_mesh)(const void*, unsigned char);
static void (*cw_visible)(void*, int, int);
static void (*cw_avatar_visible)(void*, int, int);
static unsigned cw_log_count, cw_reject_mask;
static unsigned cw_avatar_logs;
unsigned long g_tgt_cutscene_weapon, g_tgt_cutscene_avatar;

static int cutscene_weapon_layout(const void* object, const void* expected, int minimum) {
    const char* cls = object ? *(const char* const*)((const char*)object+0x10) : 0;
    if (!cls || !expected) return 0;
    int size = *(const int*)(cls+0x58), expected_size = *(const int*)((const char*)expected+0x58);
    int depth = *(const int*)((const char*)expected+0x38), actual_depth = *(const int*)(cls+0x38);
    const void* const* chain = *(const void* const**)(cls+0x30);
    return size>=minimum && size<=0x10000 && expected_size>=minimum && expected_size<=size
        && depth>=0 && depth<=128 && actual_depth>=depth && actual_depth<=256
        && chain && chain[depth]==(const char*)expected+0x30;
}
static int cutscene_weapon_component(const void* component, const void* cls, const void* owner) {
    return cutscene_weapon_layout(component,cls,0x150)
        && *(void* const*)((const char*)component+0x20)==owner;
}
static void cutscene_weapon_rejected(unsigned stage, const char* reason, void* gun, void* pawn,
                                    void* hand, void* grip, void* mesh, void* pivot, void* parent) {
    // The input snapshot gates diagnostics only; functional eligibility uses native getters.
    // Eight distinct stages yield at most eight records, including pre-manager owner rejection.
    if (stage>7 || g_touch_context!=3
        || (__atomic_fetch_or(&cw_reject_mask,1u<<stage,__ATOMIC_RELAXED)&(1u<<stage))) return;
    LOGF("CUTSCENE WEAPON reject=%s gun=%p pawn=%p hand=%p primary=%p mesh=%p pivot=%p parent=%p",
         reason,gun,pawn,hand,grip,mesh,pivot,parent);
}
static void cutscene_weapon_post_visibility(void* gun) {
    void (*visible)(void*,int,int) = __atomic_load_n(&cw_visible,__ATOMIC_ACQUIRE);
    if (!visible || !phone_player || !phone_active || !phone_type || !rl_gun1 || !rl_primary
        || !f_GetCurrentGrip || !gun) return;
    // Visibility also runs for early/CDO guns. Validate their stored owner before
    // reaching any player-controller/manager getter.
    if (!cutscene_weapon_layout(gun,cw_gun_class(),0x548)) return;
    void* pawn = *(void* const*)((const char*)gun+0x3c0);
    if (!cutscene_weapon_layout(pawn,cw_pawn_class(),0xeb0)) {
        cutscene_weapon_rejected(0,"stored owner is not Cutscene",gun,pawn,0,0,0,0,0); return;
    }
    if (phone_type()!=3) return;
    if (pawn!=phone_active()) {
        cutscene_weapon_rejected(1,"inactive owner",gun,pawn,0,0,0,0,0); return;
    }
    if (rl_gun1(pawn)!=gun) {
        cutscene_weapon_rejected(2,"not Gun1",gun,pawn,0,0,0,0,0); return;
    }
    void* grip = *(void* const*)((const char*)gun+0x270);
    if (!reload_grip_registered(grip) || *(void* const*)((const char*)grip+0x20)!=gun) {
        cutscene_weapon_rejected(3,"primary not registered/owned",gun,pawn,0,grip,0,0,0); return;
    }
    void* hand = rl_primary(gun);
    if (!cutscene_weapon_layout(hand,cw_hand_class(),0x510)
        || *(const unsigned char*)((const char*)hand+0x314)>1
        || phone_player(hand)!=pawn || f_GetCurrentGrip(hand)!=grip) {
        cutscene_weapon_rejected(4,"primary hand/current grip",gun,pawn,hand,grip,0,0,0); return;
    }
    void* mesh = *(void* const*)((const char*)gun+0x540);
    void* root = *(void* const*)((const char*)gun+0x130);
    void* pivot = *(void* const*)((const char*)gun+0x288);
    if (!cutscene_weapon_component(mesh,cw_poseable_class(),gun)
        || !cutscene_weapon_component(root,cw_scene_class(),gun)) {
        cutscene_weapon_rejected(5,"mesh/root layout or owner",gun,pawn,hand,grip,mesh,pivot,0); return;
    }
    if (!cutscene_weapon_component(pivot,cw_scene_class(),gun)) {
        cutscene_weapon_rejected(6,"FlourishPivot layout or owner",gun,pawn,hand,grip,mesh,pivot,0); return;
    }
    void* parent = *(void* const*)((const char*)mesh+0xc0);
    if (parent!=pivot || *(void* const*)((const char*)pivot+0xc0)!=root) {
        cutscene_weapon_rejected(7,"mesh->FlourishPivot->root",gun,pawn,hand,grip,mesh,pivot,parent); return;
    }
    // Only the exact render leaf is suppressed, after its original visibility writer.
    // Its native desired/current comparison restores visibility on the next normal call.
    if (*(const unsigned short*)((const char*)mesh+0x14c)&0x20) visible(mesh,0,0);
    if (cw_log_count<8) {
        cw_log_count++;
        LOGF("CUTSCENE WEAPON pawn=%p hand=%p gun=%p primary=%p mesh=%p hidden=1",pawn,hand,gun,grip,mesh);
    }
}

#ifndef RE4FIX_CUTSCENE_NATIVE_GATE
__attribute__((naked)) static void gate_cutscene_weapon(void* gun) {
    asm volatile(
        ".inst 0xf81d0ff5\n" ".inst 0xa9014ff4\n" ".inst 0xa9027bfd\n" ".inst 0x910083fd\n"
        "adrp x16, :got:g_tgt_cutscene_weapon\n" "ldr x16, [x16, :got_lo12:g_tgt_cutscene_weapon]\n"
        "ldr x16, [x16]\n" "br x16\n");
}
#else
#define gate_cutscene_weapon RE4FIX_CUTSCENE_NATIVE_GATE
#endif
static void hook_cutscene_weapon(void* gun) {
    gate_cutscene_weapon(gun);
    cutscene_weapon_post_visibility(gun);
}
static int cutscene_avatar_component(const void* mesh, const void* owner) {
    return cutscene_weapon_layout(mesh,cw_skeletal_class(),0x438)
        && *(void* const*)((const char*)mesh+0x20)==owner;
}
static void cutscene_avatar_post_visibility(void* pawn) {
    void (*visible)(void*,int,int) = __atomic_load_n(&cw_avatar_visible,__ATOMIC_ACQUIRE);
    if (!visible || !phone_type || !phone_active || !pawn
        || !cutscene_weapon_layout(pawn,cw_pawn_class(),0xbb0)) return;
    void* meshset = *(void* const*)((const char*)pawn+0xba8);
    if (!cutscene_weapon_layout(meshset,cw_meshset_class(),0x238)) return;
    void* root = *(void* const*)((const char*)meshset+0x130);
    void* pawn_root = *(void* const*)((const char*)pawn+0x130);
    void* primary = *(void* const*)((const char*)meshset+0x228);
    void* secondary = *(void* const*)((const char*)meshset+0x230);
    // Validate the entire exact binding before GetMesh reads Secondary.SkeletalMesh+430.
    if (root!=*(void* const*)((const char*)meshset+0x220)
        || !cutscene_weapon_component(root,cw_scene_class(),meshset)
        || !cutscene_weapon_component(pawn_root,cw_scene_class(),pawn)
        || *(void* const*)((const char*)root+0xc0)!=pawn_root
        || !cutscene_avatar_component(primary,meshset)
        || (secondary && !cutscene_avatar_component(secondary,meshset))) return;
    if (phone_type()!=3 || phone_active()!=pawn) return;
    void* meshes[2] = {cw_get_mesh(pawn,0),cw_get_mesh(pawn,1)};
    for (unsigned i=0;i<2;i++)
        if ((meshes[i]!=primary && meshes[i]!=secondary)
            || !cutscene_avatar_component(meshes[i],meshset)
            || *(void* const*)((const char*)meshes[i]+0xc0)!=root) return;
    unsigned hidden = 0;
    for (unsigned i=0;i<2;i++) {
        if (i && meshes[i]==meshes[0]) continue;
        if (*(const unsigned short*)((const char*)meshes[i]+0x14c)&0x20) visible(meshes[i],0,0);
        hidden++;
    }
    if (cw_avatar_logs<8) {
        cw_avatar_logs++;
        LOGF("CUTSCENE AVATAR pawn=%p meshset=%p mesh0=%p mesh1=%p hidden=%u",
             pawn,meshset,meshes[0],meshes[1],hidden);
    }
}
#include "re4fix_phone_view.h"
#ifndef RE4FIX_CUTSCENE_AVATAR_NATIVE_GATE
__attribute__((naked)) static void gate_cutscene_avatar(void* pawn) {
    asm volatile(
        ".inst 0xf81c0ff7\n" ".inst 0xa90157f6\n" ".inst 0xa9024ff4\n" ".inst 0xa9037bfd\n"
        "adrp x16, :got:g_tgt_cutscene_avatar\n" "ldr x16, [x16, :got_lo12:g_tgt_cutscene_avatar]\n"
        "ldr x16, [x16]\n" "br x16\n");
}
#else
#define gate_cutscene_avatar RE4FIX_CUTSCENE_AVATAR_NATIVE_GATE
#endif
static void hook_cutscene_avatar(void* pawn) {
    gate_cutscene_avatar(pawn);
    cutscene_avatar_post_visibility(pawn);
    phone_view_avatar_post(pawn);
}
static void install_touch_cutscene_weapon(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[2]; } check[] = {
        {0x628d024,{0xf81d0ff5,0xa9014ff4}}, {0x628d02c,{0xa9027bfd,0x910083fd}},
        {0x628d064,{0xf942a260,0xf9400008}}, {0x62b9958,{0xf9436d08,0xd63f0100}},
        {0x628acb8,{0xf902a260,0x94847e8f}}, {0x62b977c,{0xf901e261,0xb00204d5}},
        {0x62ac2c8,{0xf81e0ff3,0xa9017bfd}}, {0x62bbc30,{0xf9413800,0x17ff1a5f}},
        {0x633d030,{0xf9417800,0xd65f03c0}}, {0x63380dc,{0xd0020168,0xf9431508}},
        {0x6338aec,{0xd0020168,0xf9431508}}, {0x629a774,{0xf9421408,0xb4000268}},
        {0x83ad964,{0xd104c3ff,0xf90073fc}}, {0x63e60c8,{0xd10183ff,0xf90023f3}},
        {0x6424b5c,{0xd10183ff,0xf90023f3}}, {0x6420bc0,{0xd10183ff,0xf90023f3}},
        {0x8f4e5bc,{0xd10183ff,0xf90023f3}}, {0x8f82500,{0xd10183ff,0xf90023f3}},
        {0x628b0f0,{0xf942a260,0xf9414661}}, {0x628b100,{0x94845f76,0xf942a260}},
        {0x62b90ec,{0xf9409a61,0xaa1f03e2}}, {0x62b90f4,{0xf9014660,0x9483c580}},
        {0x62a65e0,{0xf81c0ff7,0xa90157f6}}, {0x62a65e8,{0xa9024ff4,0xa9037bfd}},
        {0x6427ad4,{0xd10183ff,0xf90023f3}}, {0x8f94650,{0xd10183ff,0xf90023f3}},
        {0x62af678,{0xf945d408,0xb40002a8}}, {0x62a59d0,{0xaa1303e0,0x94000303}},
        {0x6424be4,{0x5280a204,0x52800205}},
    };
    static const struct { unsigned long params; unsigned offset; } fields[] = {
        {0x9da4618,0x540}, {0x9db0b18,0x3c0}, {0x9db0e60,0x270},
        {0xa0b7530,0x130}, {0xa235ba8,0xc0}, {0x9db0dd0,0x288},
        {0x9dac9e0,0xba8}, {0x9da8c10,0x220}, {0x9da8be0,0x228}, {0x9da8bb0,0x230},
    };
    if (!base || __atomic_load_n(&cw_visible,__ATOMIC_ACQUIRE)) return;
    for (unsigned i=0;i<sizeof check/sizeof check[0];i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,sizeof check[i].first)) {
            LOGF("CUTSCENE WEAPON disabled: opcode mismatch +%lx",check[i].addr); return;
        }
    for (unsigned i=0;i<sizeof fields/sizeof fields[0];i++)
        if (*(const unsigned*)(base+fields[i].params+0x24)!=fields[i].offset) {
            LOGF("CUTSCENE WEAPON disabled: property mismatch +%lx",fields[i].params); return;
        }
    if (phone_player!=(void* (*)(const void*))(base+0x633d030)
        || phone_active!=(void* (*)(void))(base+0x63380dc)
        || phone_type!=(unsigned (*)(void))(base+0x6338aec)
        || rl_gun1!=(void* (*)(const void*))(base+0x62ac2c8)
        || rl_primary!=(void* (*)(const void*))(base+0x62bbc30)
        || f_GetCurrentGrip!=(void* (*)(const void*))(base+0x629a774)
        || *(unsigned long*)(base+0x9d1b3e0)!=base+0x628c5bc
        || *(unsigned long*)(base+0x9d1b458)!=base+0x628d024) {
        LOGF("CUTSCENE WEAPON disabled: native dependency/vtable mismatch"); return;
    }
    cw_pawn_class=(void* (*)(void))(base+0x63e60c8);
    cw_hand_class=(void* (*)(void))(base+0x6424b5c);
    cw_gun_class=(void* (*)(void))(base+0x6420bc0);
    cw_poseable_class=(void* (*)(void))(base+0x8f4e5bc);
    cw_scene_class=(void* (*)(void))(base+0x8f82500);
    cw_meshset_class=(void* (*)(void))(base+0x6427ad4);
    cw_skeletal_class=(void* (*)(void))(base+0x8f94650);
    cw_get_mesh=(void* (*)(const void*,unsigned char))(base+0x62af678);
    g_tgt_cutscene_weapon=base+0x628d024+16;
    __atomic_store_n(&cw_visible,(void (*)(void*,int,int))(base+0x83ad964),__ATOMIC_RELEASE);
    if (patch_code((void*)(base+0x628d024),(void*)&hook_cutscene_weapon)!=0) {
        __atomic_store_n(&cw_visible,0,__ATOMIC_RELEASE);
        LOGF("CUTSCENE WEAPON disabled: native hook installation failed"); return;
    }
    LOGF("CUTSCENE WEAPON ready: native original first, active type3 owned held Gun1 exact PoseableMesh leaf");
    g_tgt_cutscene_avatar=base+0x62a65e0+16;
    __atomic_store_n(&cw_avatar_visible,(void (*)(void*,int,int))(base+0x83ad964),__ATOMIC_RELEASE);
    if (patch_code((void*)(base+0x62a65e0),(void*)&hook_cutscene_avatar)!=0) {
        __atomic_store_n(&cw_avatar_visible,0,__ATOMIC_RELEASE);
        LOGF("CUTSCENE AVATAR disabled: native hook installation failed"); return;
    }
    LOGF("CUTSCENE AVATAR ready: native original first, active type3 exact owned GetMesh leaves");
}
#endif
