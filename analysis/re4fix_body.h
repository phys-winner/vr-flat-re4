// Follow horizontal touch look through the author's body-target API on GameThread.
#ifndef RE4FIX_BODY_H
#define RE4FIX_BODY_H
#include <math.h>
#include <string.h>

static void* (*body_player)(const void*);
static void* (*body_active)(void);
static unsigned (*body_type)(void);
static void (*body_snap)(void*);
struct body_rot { float pitch, yaw, roll; };
_Static_assert(sizeof(struct body_rot) == 12, "UE4 FRotator HFA ABI");
static struct body_rot (*body_head_local)(const void*);
static struct body_rot (*body_head_world)(const void*);
static struct body_rot (*body_world)(const void*);
static struct body_rot (*body_target_world)(const void*);
static void* body_last_pawn;
static float body_last_yaw;

static void touch_body_tick(void* hand) {
    if (!body_snap || !hand || *(const unsigned char*)((const char*)hand + 0x314) != 1) return;
    if (body_type() != 1) { body_last_pawn = 0; return; } // EPlayerPawn::Bio4, verified native enum table.
    void* pawn = body_player(hand);
    if (!pawn || pawn != body_active()) return;
    struct body_rot head_world = body_head_world(pawn);
    float yaw = head_world.yaw;
    if (!isfinite(yaw)) return;
    // Native camera pose can arrive one frame after touch; compare world degrees at the same angular tolerance.
    if (pawn == body_last_pawn && fabsf(remainderf(yaw - body_last_yaw, 360.0f)) < 0.005729578f) return;
    body_snap(pawn);
    body_last_pawn = pawn;
    body_last_yaw = yaw;
    static unsigned logs;
    if (logs++ < 8) {
        struct body_rot head = body_head_local(pawn);
        struct body_rot body = body_world(pawn);
        struct body_rot target_world = body_target_world(pawn);
        LOGF("BODY follow pawn=%p touch_yaw_OVR=%.1f head_local_yaw_UE=%.1f target_body_local_yaw_UE=%.1f head_world_yaw_UE=%.1f body_world_yaw_UE=%.1f target_body_world_yaw_UE=%.1f",
             pawn, g_look_yaw * 57.29578f, head.yaw, *(const float*)((const char*)pawn + 0x4b0),
             head_world.yaw, body.yaw, target_world.yaw);
    }
}

static void install_touch_body(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[2]; } check[] = {
        {0x633d030,{0xf9417800,0xd65f03c0}}, {0x63380dc,{0xd0020168,0xf9431508}},
        {0x6338aec,{0xd0020168,0xf9431508}}, {0x6340474,{0xd10103ff,0xa9024ff4}},
        {0x6341b2c,{0xf9416c08,0xbd412900}}, {0x63404b0,{0xbd04b260,0xf9401688}},
        {0x61c4030,{0xd65f03c0,0xbd400000}},
        {0x6341b40,{0xd10103ff,0xa9024ff4}}, {0x6342e9c,{0xd10103ff,0xf90013f3}},
        {0x6342ff4,{0xd10143ff,0xf9001bf3}},
    };
    if (!base || body_snap) return;
    for (unsigned i = 0; i < sizeof check / sizeof check[0]; i++)
        if (memcmp((const void*)(base + check[i].addr), check[i].first, sizeof check[i].first)) {
            LOGF("BODY follow disabled: opcode mismatch +%lx", check[i].addr); return;
        }
    body_player = (void* (*)(const void*))(base + 0x633d030);
    body_active = (void* (*)(void))(base + 0x63380dc);
    body_type = (unsigned (*)(void))(base + 0x6338aec);
    body_head_local = (struct body_rot (*)(const void*))(base + 0x6341b2c);
    body_head_world = (struct body_rot (*)(const void*))(base + 0x6341b40);
    body_world = (struct body_rot (*)(const void*))(base + 0x6342e9c);
    body_target_world = (struct body_rot (*)(const void*))(base + 0x6342ff4);
    body_snap = (void (*)(void*))(base + 0x6340474);
    LOGF("BODY follow installed: active Bio4 pawn, existing hand Tick");
}
#endif
