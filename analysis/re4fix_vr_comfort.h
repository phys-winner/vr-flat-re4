// Remove only the native movement/turn comfort wipe; retain damage, scope and authored fades.
#ifndef RE4FIX_VR_COMFORT_H
#define RE4FIX_VR_COMFORT_H
#include <stdint.h>

static void* (*comfort_player_class)(void);
static unsigned comfort_motion_logs;
uintptr_t g_tgt_comfort_vignette;

#ifndef RE4FIX_COMFORT_VIGNETTE_GATE
__attribute__((naked)) static void gate_comfort_vignette(void* pawn,float* alpha,float* scale,unsigned char* scope) {
    asm volatile(".inst 0xa9bd57f6\n" ".inst 0xa9014ff4\n" ".inst 0xa9027bfd\n" ".inst 0x910083fd\n"
        "adrp x16, :got:g_tgt_comfort_vignette\n" "ldr x16, [x16, :got_lo12:g_tgt_comfort_vignette]\n"
        "ldr x16, [x16]\n" "br x16\n");
}
#else
#define gate_comfort_vignette RE4FIX_COMFORT_VIGNETTE_GATE
#endif

static void hook_comfort_vignette(void* pawn,float* alpha,float* scale,unsigned char* scope) {
    gate_comfort_vignette(pawn,alpha,scale,scope);
    if (!alpha || !scale || alpha==scale || !cinema_on_game_thread()
        || !comfort_player_class || !cinema_active || !cinema_type) return;
    unsigned type=cinema_type();
    if (type<1 || type>7 || pawn!=cinema_active()
        || !cinema_object(pawn,comfort_player_class(),0x570)) return;
    float previous=*alpha;
    *alpha=0.0f;
    *scale=1.0f;
    // Bio4's original override subsequently supplies its authored scope state.
    if (previous>0.0f && comfort_motion_logs<8) {
        ++comfort_motion_logs;
        LOGF("FLAT comfort movement/turn wipe removed pawn=%p type=%u nativeAlpha=%.3f",pawn,type,previous);
    }
}

static void install_touch_vr_comfort(uintptr_t base) {
    static const struct { unsigned long addr;unsigned first[2]; } check[] = {
        {0x6345d1c,{0xa9bd57f6,0xa9014ff4}}, {0x6345d24,{0xa9027bfd,0x910083fd}},
        {0x6345dd4,{0xbd455ea0,0xbd4562a1}}, {0x6345df0,{0x52a7f008,0xb90002df}},
        {0x6219ac0,{0x9404b097,0xaa1403e0}}, {0x6219ac8,{0x97ffff08,0x12000008}},
        {0x6219ad0,{0x39000268,0xa9417bfd}}, {0x622ecec,{0xf9440508,0xd63f0100}},
        {0x622ed20,{0xbd425261,0x5296e2e9}}, {0x64b7218,{0xd10183ff,0xf90023f3}},
        {0x64b72a0,{0x5280ae04,0x52800205}},
    };
    static const struct { unsigned long params;unsigned offset; } fields[] = {
        {0x9d77650,0x250}, {0x9d77678,0x24c}, {0x9d776a0,0x248},
        {0x9d77790,0x228}, {0x9db5d18,0x31},
    };
    if (!base || comfort_player_class || !cinema_active || !cinema_type) return;
    for (unsigned i=0;i<sizeof check/sizeof check[0];i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,sizeof check[i].first)) {
            LOGF("FLAT comfort disabled: opcode mismatch +%lx",check[i].addr);return;
        }
    for (unsigned i=0;i<sizeof fields/sizeof fields[0];i++)
        if (*(const unsigned*)(base+fields[i].params+0x24)!=fields[i].offset) {
            LOGF("FLAT comfort disabled: property mismatch +%lx",fields[i].params);return;
        }
    comfort_player_class=(void* (*)(void))(base+0x64b7218);
#ifndef RE4FIX_COMFORT_FORCE_REMOVAL
    // Phone build: MovementVignetteStrength (default None via one-time defaults) is honoured natively.
    LOGF("FLAT comfort follows native Tunneling option (no forced removal)");
    return;
#endif
    g_tgt_comfort_vignette=base+0x6345d1c+16;
    if (patch_code((void*)(base+0x6345d1c),(void*)&hook_comfort_vignette)!=0) {
        comfort_player_class=0;LOGF("FLAT comfort disabled: native hook installation failed");return;
    }
    LOGF("FLAT comfort ready: native movement/turn output only; damage/scope/authored fades retained");
}
#endif
