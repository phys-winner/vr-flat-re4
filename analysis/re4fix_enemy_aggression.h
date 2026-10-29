// Use the original highest-rank Ganado decisions without changing adaptive rank, damage, or drops.
static const int g_mobile_enemy_disabled;
const int* g_mobile_enemy_pref = &g_mobile_enemy_disabled;
unsigned long g_mobile_enemy_continue[6];

// x16 is unused throughout each verified native function; every other live register is preserved.
#define ENEMY_RANK(register_name) \
    "adrp x16, :got:g_mobile_enemy_pref\n" \
    "ldr x16, [x16, :got_lo12:g_mobile_enemy_pref]\n" \
    "ldr x16, [x16]\n" "ldr w16, [x16]\n" \
    "cbz w16, 1f\n" "mov " register_name ", #10\n" "1:\n"
#define ENEMY_CONTINUE(offset) \
    "adrp x16, :got:g_mobile_enemy_continue\n" \
    "ldr x16, [x16, :got_lo12:g_mobile_enemy_continue]\n" \
    "ldr x16, [x16, #" offset "]\n" "br x16\n"

__attribute__((naked)) static void gate_enemy_timer7(void) {
    asm volatile("ldr x8, [x25]\n" "mov w9, #0x5024\n" "ldrb w8, [x8, x9]\n"
        ENEMY_RANK("w8") "cmp w8, #7\n" ENEMY_CONTINUE("0"));
}
__attribute__((naked)) static void gate_enemy_timer10(void) {
    asm volatile("ldr x8, [x25]\n" "mov w9, #0x5024\n" "ldrb w8, [x8, x9]\n"
        ENEMY_RANK("w8") "cmp w8, #10\n" ENEMY_CONTINUE("8"));
}
__attribute__((naked)) static void gate_enemy_attack_floor(void) {
    asm volatile("mov w10, #0x5024\n" "ldr x8, [x21]\n" "ldrb w8, [x8, x10]\n"
        ENEMY_RANK("w8") "cmp w8, #2\n" ENEMY_CONTINUE("16"));
}
__attribute__((naked)) static void gate_enemy_attack_angle(void) {
    asm volatile("mov w9, #0x5024\n" "fabs s0, s0\n" "ldrb w8, [x8, x9]\n"
        ENEMY_RANK("w8") "mov w9, #0xfdb\n" ENEMY_CONTINUE("24"));
}
__attribute__((naked)) static void gate_enemy_attack_group(void) {
    asm volatile("mov w9, #0x5024\n" "ldr x8, [x8]\n" "ldrb w8, [x8, x9]\n"
        ENEMY_RANK("w8") ENEMY_CONTINUE("32"));
}
__attribute__((naked)) static void gate_enemy_attack_cooldown(void) {
    asm volatile("ldr x8, [x23]\n" "mov w9, #0x5024\n" "ldrb w9, [x8, x9]\n"
        ENEMY_RANK("w9") "cmp w9, #3\n" ENEMY_CONTINUE("40"));
}
#undef ENEMY_RANK
#undef ENEMY_CONTINUE

static void install_enemy_aggression(unsigned long base) {
    static const unsigned long sites[6] = {0x5d701fc,0x5d70228,0x5d86798,0x5d86858,0x5d86928,0x5d86bbc};
    static const unsigned long continuations[6] = {0x5d7020c,0x5d70238,0x5d867a8,0x5d86868,0x5d8693c,0x5d86bcc};
    static const unsigned int original[6][4] = {
        {0xf9400328,0x528a0489,0x38696908,0x71001d1f},
        {0xf9400328,0x528a0489,0x38696908,0x7100291f},
        {0x528a048a,0xf94002a8,0x386a6908,0x7100091f},
        {0x528a0489,0x1e20c000,0x38696908,0x5281fb69},
        {0x528a0489,0xf9400108,0x38696908,0x14000002},
        {0xf94002e8,0x528a0489,0x38696909,0x71000d3f}
    };
    void* gates[6] = {(void*)gate_enemy_timer7,(void*)gate_enemy_timer10,(void*)gate_enemy_attack_floor,
        (void*)gate_enemy_attack_angle,(void*)gate_enemy_attack_group,(void*)gate_enemy_attack_cooldown};
    for (int index=0; index<6; ++index) {
        if (memcmp((void*)(base+sites[index]),original[index],sizeof original[index])) {
            LOGI("mobile enemy aggression disabled: native AI preimage mismatch"); return;
        }
        g_mobile_enemy_continue[index] = base+continuations[index];
    }
    for (int index=0; index<6; ++index) if (patch_code((void*)(base+sites[index]),gates[index])) {
        LOGI("mobile enemy aggression disabled: native AI gate failed"); return;
    }
    // Same relaxed integer load as mobile_prefs_aggression(), without an ABI call inside live native code.
    g_mobile_enemy_pref = &g_mprefs.aggression;
    LOGI("mobile enemy aggression ready: six scoped native Ganado AI decisions; adaptive rank and simulation speed unchanged");
}
