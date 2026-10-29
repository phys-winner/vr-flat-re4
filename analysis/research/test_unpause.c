#include <assert.h>
#include <stdio.h>
#include <string.h>

static volatile int g_touch_back_request;
static void* (*phone_player)(const void*);
static void* (*phone_active)(void);
static unsigned (*phone_type)(void);
#define LOGF(...) ((void)0)
static int cancels;
static void touch_menu_grip_cancel(void* hand) { assert(hand); cancels++; }
#include "../re4fix_unpause.h"

static struct {
    unsigned char hand[0x400], inactive[0x400], pawn[0x1300], other[0x1300], screen[0x400];
    unsigned char pawn_class[0x100], native_pawn_class[0x100], screen_class[0x100], native_screen_class[0x100];
    void* pawn_chain[2], *screen_chain[2];
} f __attribute__((aligned(16)));
static void* active;
static unsigned type;
static int calls, accepted, last_reason = -1;
static void* get_player(const void* hand) { return hand == f.hand ? f.pawn : hand == f.inactive ? f.other : 0; }
static void* get_active(void) { return active; }
static unsigned get_type(void) { return type; }
static void* get_pawn_class(void) { return f.native_pawn_class; }
static void* get_screen_class(void) { return f.native_screen_class; }
static int try_unpause(unsigned char reason) { assert(cancels==calls+1); calls++; last_reason = reason; return accepted; }
static void ready(void) {
    memset(&f,0,sizeof f);
    f.hand[0x314] = f.inactive[0x314] = 1;
    *(void**)(f.pawn+0x10) = f.pawn_class;
    *(void**)(f.pawn+0x11b0) = f.screen;
    *(void**)(f.screen+0x10) = f.screen_class;
    *(int*)(f.pawn_class+0x58) = sizeof f.pawn;
    *(int*)(f.native_pawn_class+0x58) = 0x11b8;
    *(int*)(f.screen_class+0x58) = sizeof f.screen;
    *(int*)(f.native_screen_class+0x58) = 0x220;
    *(int*)(f.pawn_class+0x38) = *(int*)(f.screen_class+0x38) = 1;
    f.pawn_chain[0] = f.native_pawn_class+0x30; f.pawn_chain[1] = f.pawn_class+0x30;
    f.screen_chain[0] = f.native_screen_class+0x30; f.screen_chain[1] = f.screen_class+0x30;
    *(void**)(f.pawn_class+0x30) = f.pawn_chain; *(void**)(f.screen_class+0x30) = f.screen_chain;
    active = f.pawn; type = 2; calls = cancels = 0; last_reason = -1; accepted = 1; g_touch_back_request = 1;
    phone_player = get_player; phone_active = get_active; phone_type = get_type;
    unpause_pawn_class = get_pawn_class; unpause_screen_class = get_screen_class; unpause_try = try_unpause;
}
static void tick(void* hand) {
    unsigned char before[sizeof f]; memcpy(before,&f,sizeof f);
    touch_unpause_tick(hand); assert(!memcmp(before,&f,sizeof f));
}
static void rejected(void) { tick(f.hand); assert(!calls && !cancels && !g_touch_back_request); }
int main(int argc,char** argv) {
    (void)argv; if (argc == 999) install_touch_unpause(0);
    ready(); tick(0); tick(f.inactive); assert(!calls && g_touch_back_request == 1);
    f.hand[0x314] = 0; tick(f.hand); assert(!calls && g_touch_back_request == 1); f.hand[0x314] = 1;
    active = 0; tick(f.hand); assert(!calls && g_touch_back_request == 1); active = f.pawn;
    unpause_try = 0; tick(f.hand); assert(!calls && g_touch_back_request == 1); unpause_try = try_unpause;
    phone_player = 0; tick(f.hand); assert(!calls && g_touch_back_request == 1); phone_player = get_player;
    phone_active = 0; tick(f.hand); assert(!calls && g_touch_back_request == 1); phone_active = get_active;
    phone_type = 0; tick(f.hand); assert(!calls && g_touch_back_request == 1); phone_type = get_type;
    tick(f.hand); assert(calls == 1 && last_reason == 0 && !g_touch_back_request);
    tick(f.hand); assert(calls == 1);
    ready(); accepted = 0; tick(f.hand); assert(calls == 1 && last_reason == 0 && !g_touch_back_request);
    tick(f.hand); assert(calls == 1); // native modal/animation refusal is never retried or bypassed
    ready(); type = 1; rejected(); ready(); type = 5; rejected();
    ready(); *(void**)(f.pawn+0x10) = 0; rejected();
    ready(); *(int*)(f.pawn_class+0x58) = 0x11b7; rejected();
    ready(); *(int*)(f.pawn_class+0x58) = 0x10001; rejected();
    ready(); *(int*)(f.native_pawn_class+0x58) = 0x11b7; rejected();
    ready(); *(int*)(f.native_pawn_class+0x58) = sizeof f.pawn+1; rejected();
    ready(); *(int*)(f.native_pawn_class+0x38) = -1; rejected();
    ready(); *(int*)(f.pawn_class+0x38) = 1025; rejected();
    ready(); f.pawn_chain[0] = f.native_screen_class+0x30; rejected();
    ready(); *(void**)(f.pawn_class+0x30) = 0; rejected();
    ready(); *(void**)(f.pawn+0x11b0) = 0; rejected();
    ready(); *(void**)(f.screen+0x10) = 0; rejected();
    ready(); *(int*)(f.screen_class+0x58) = 0x27; rejected();
    ready(); *(int*)(f.screen_class+0x58) = 0x10001; rejected();
    ready(); *(int*)(f.screen_class+0x58) = 0x21f; rejected();
    ready(); f.screen_chain[0] = f.native_pawn_class+0x30; rejected();
    ready(); *(void**)(f.screen_class+0x30) = 0; rejected();
    ready(); g_touch_back_request = 0; tick(f.hand); assert(!calls);
    puts("PASS: actual unpause C, active Pause right-hand ownership, queued inactive requests, Game reason=0, one-shot native acceptance/refusal, pawn/UI class-size-ancestry guards and immutable objects");
    return 0;
}
