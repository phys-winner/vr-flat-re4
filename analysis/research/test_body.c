#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static volatile float g_look_yaw;
static float native_head_yaw = -28.0f;
static unsigned diagnostic_logs, head_reads, diagnostic_reads;
static void diagnostic_log(const char* format, ...) {
    char message[640], expected[64];
    va_list args; va_start(args,format);
    vsnprintf(message,sizeof message,format,args); va_end(args);
    if (strncmp(message,"BODY follow pawn=",17)) return;
    diagnostic_logs++;
    snprintf(expected,sizeof expected,"head_world_yaw_UE=%.1f",native_head_yaw);
    assert(strstr(message,expected));
    assert(strstr(message,"body_world_yaw_UE=-170.0"));
    assert(strstr(message,"target_body_world_yaw_UE=-169.0"));
}
#define LOGF(...) diagnostic_log(__VA_ARGS__)
#include "../re4fix_body.h"

static unsigned char pawn_a[0x558], pawn_b[0x558];
static int snaps;
static unsigned active_type = 1;
static void* owner = &pawn_a, *active = &pawn_a;
static void* player(const void* h) { (void)h; return owner; }
static void* current(void) { return active; }
static unsigned type(void) { return active_type; }
static struct body_rot head_rot(const void* p) { assert(p == active); return (struct body_rot){0,native_head_yaw+28,0}; }
static struct body_rot head_world_rot(const void* p) { assert(p == active); head_reads++; return (struct body_rot){1,native_head_yaw,2}; }
static struct body_rot body_world_rot(const void* p) { assert(p == active); diagnostic_reads++; return (struct body_rot){3,-170,4}; }
static struct body_rot target_world_rot(const void* p) { assert(p == active); diagnostic_reads++; return (struct body_rot){5,-169,6}; }
static void snap(void* p) { assert(p == active); snaps++; *(float*)((char*)p+0x4b0) = head_rot(p).yaw; }

int main(int argc,char** argv) {
    (void)argv; if (argc == 999) install_touch_body(0);
    unsigned char hand[0x318] = {0};
    body_player = player; body_active = current; body_type = type; body_snap = snap;
    body_head_local = head_rot; body_head_world = head_world_rot; body_world = body_world_rot;
    body_target_world = target_world_rot;
    touch_body_tick(0); touch_body_tick(hand); assert(snaps == 0 && !head_reads);
    hand[0x314] = 1;
    owner = 0; touch_body_tick(hand); assert(!head_reads);
    owner = &pawn_b; touch_body_tick(hand); assert(!head_reads); owner = &pawn_a;
    for (unsigned i = 0; i < 10; i++) if (i != 1) { active_type = i; touch_body_tick(hand); }
    assert(snaps == 0 && !head_reads && !diagnostic_reads && !diagnostic_logs); active_type = 1;
    touch_body_tick(hand); touch_body_tick(hand); assert(snaps == 1 && head_reads == 2);
    // Frame 0: touch changed but the camera's native head pose has not arrived yet.
    g_look_yaw = 0.78539816f; touch_body_tick(hand); assert(snaps == 1 && head_reads == 3);
    assert(*(float*)(pawn_a+0x4b0) == 0);
    // Frame 1: native head catches up, while the raw touch remains unchanged.
    native_head_yaw = -73; touch_body_tick(hand); assert(snaps == 2 && head_reads == 4);
    assert(*(float*)(pawn_a+0x4b0) == -45);
    touch_body_tick(hand); assert(snaps == 2 && head_reads == 5);
    native_head_yaw = NAN; touch_body_tick(hand); assert(snaps == 2);
    native_head_yaw = INFINITY; touch_body_tick(hand); assert(snaps == 2);
    native_head_yaw = -INFINITY; touch_body_tick(hand); assert(snaps == 2);
    native_head_yaw = -73; owner = active = &pawn_b; touch_body_tick(hand); assert(snaps == 3);
    active_type = 5; touch_body_tick(hand); active_type = 1; touch_body_tick(hand); assert(snaps == 4);
    native_head_yaw = 179.999f; touch_body_tick(hand); assert(snaps == 5);
    native_head_yaw = -179.999f; touch_body_tick(hand); assert(snaps == 5); // ~0.002 degrees across wrap
    native_head_yaw = -179.99f; touch_body_tick(hand); assert(snaps == 6);
    g_look_yaw = NAN; native_head_yaw = -160; touch_body_tick(hand); assert(snaps == 7);
    g_look_yaw = 0.2f;
    for (unsigned i = 0; i < 4; i++) { native_head_yaw = -150+(float)i*10; touch_body_tick(hand); }
    assert(snaps == 11 && diagnostic_logs == 8 && diagnostic_reads == 16);
    assert(head_reads == 18); // One functional head-world read per eligible Tick, reused by diagnostics.
    puts("PASS: actual body C, delayed native camera Frame0 no-Snap/Frame1 Snap with unchanged touch, active/right guards, native finite/wrap tolerance, diagnostic reuse and eight-log bound");
    return 0;
}
