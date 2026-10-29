#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static volatile long g_knife_diag_until,g_anim_t0;
static volatile int g_anim_kind,g_touch_weapon;
static void* (*phone_player)(const void*);
static void* (*phone_active)(void);
static unsigned (*phone_type)(void);
static void* (*f_GetCurrentGrip)(const void*);
static long current_ms;
static long ptr_now_ms(void) { return current_ms; }
static int samples,configurations,unavailable;
static void diag_log(const char* fmt,...) {
    if (!strncmp(fmt,"KNIFECFG ",9)) configurations++;
    else if (!strncmp(fmt,"KNIFEDIAG unavailable",21)) unavailable++;
    else if (!strncmp(fmt,"KNIFEDIAG knife=",16)) samples++;
}
#define LOGF(...) diag_log(__VA_ARGS__)
static void obj_name(const void* p,char* out,int n) {
    snprintf(out,n,"%s",*(const char* const*)((const char*)p+0x18));
}
#include "../re4fix_knife_diag.h"

static unsigned char hand[0x500],inactive_hand[0x500],pawn[0x1300],knife[0x700];
static unsigned char grip[0x400],knife_class[0x100];
static unsigned active_type=1;
static void* get_player(const void* h) { return h==inactive_hand ? 0 : pawn; }
static void* get_active(void) { return pawn; }
static unsigned get_type(void) { return active_type; }
static void* get_grip(const void* h) { assert(h==hand); return *(void* const*)(hand+0x428); }
static void* get_primary(const void* k) { assert(k==knife); return hand; }
static unsigned get_interactivity(const void* k) { assert(k==knife); return 2; }
static int get_false(const void* k) { assert(k==knife); return 0; }
static int get_valid(void) { return 1; }
static void reset_window(long duration) {
    current_ms+=10000; g_knife_diag_until=current_ms+duration;
}
int main(int argc,char** argv) {
    (void)argv;
    if (argc==999) install_touch_knife_diag(0);
    phone_player=get_player; phone_active=get_active; phone_type=get_type;
    f_GetCurrentGrip=get_grip; kd_primary=get_primary; kd_interactivity=get_interactivity;
    kd_blending=get_false; kd_flourishing=get_false; kd_player_valid=get_valid;
    *(void**)(hand+0x428)=grip; *(void**)(grip+0x20)=knife;
    *(void**)(knife+0x10)=knife_class; *(void**)(knife+0x3c0)=pawn;
    *(void**)(knife+0x270)=grip;
    *(const char**)(knife_class+0x18)="VR4GamePlayerKnife_Leon_BP_C";
    *(int*)(knife_class+0x58)=sizeof knife;
    float* v=(float*)(knife+0x680); v[0]=3; v[1]=4; v[2]=12;
    assert(knife_diag_speed(knife,0x680)==13);
    unsigned char before_hand[sizeof hand],before_knife[sizeof knife],before_grip[sizeof grip];
    memcpy(before_hand,hand,sizeof hand); memcpy(before_knife,knife,sizeof knife);
    memcpy(before_grip,grip,sizeof grip);
    touch_knife_diag(hand); assert(!samples && !configurations);
    reset_window(500); touch_knife_diag(hand); assert(!samples);
    hand[0x314]=1; before_hand[0x314]=1;
    inactive_hand[0x314]=1; touch_knife_diag(inactive_hand); assert(!samples);
    touch_knife_diag(hand); assert(samples==1 && configurations==1);
    for (int i=0;i<4;i++) touch_knife_diag(hand);
    assert(samples==1);
    current_ms+=99; touch_knife_diag(hand); assert(samples==1);
    current_ms++; touch_knife_diag(hand); assert(samples==2 && configurations==1);
    current_ms+=400; touch_knife_diag(hand); assert(samples==2);
    assert(!memcmp(before_hand,hand,sizeof hand) && !memcmp(before_knife,knife,sizeof knife)
        && !memcmp(before_grip,grip,sizeof grip));
    reset_window(100000); touch_knife_diag(hand); assert(samples==3 && configurations==2);
    for (int i=0;i<49;i++) { current_ms+=100; touch_knife_diag(hand); }
    assert(samples==52); current_ms+=100; touch_knife_diag(hand); assert(samples==52);
    reset_window(500); *(void**)(hand+0x428)=0; touch_knife_diag(hand);
    assert(samples==52 && unavailable==1);
    reset_window(500); *(void**)(hand+0x428)=grip;
    *(const char**)(knife_class+0x18)="VR4GamePlayerGun_Pistol_BP_C";
    touch_knife_diag(hand); assert(samples==52 && unavailable==2);
    reset_window(500); *(const char**)(knife_class+0x18)="VR4GamePlayerKnife_Leon_BP_C";
    *(int*)(knife_class+0x58)=0x6cf; touch_knife_diag(hand); assert(unavailable==3);
    reset_window(500); *(int*)(knife_class+0x58)=0x10001; touch_knife_diag(hand); assert(unavailable==4);
    reset_window(500); *(int*)(knife_class+0x58)=sizeof knife;
    *(void**)(knife+0x3c0)=0; touch_knife_diag(hand); assert(unavailable==5);
    reset_window(500); *(void**)(knife+0x3c0)=pawn; active_type=5;
    touch_knife_diag(hand); assert(unavailable==6 && samples==52);
    reset_window(500); active_type=1; touch_knife_diag(hand); assert(samples==53);
    assert(g_knife_diag_until==current_ms+500 && !g_anim_kind && !g_touch_weapon);
    puts("PASS: idle/active/known-knife guards, 100ms rate, 5s cap, no engine or input writes");
    return 0;
}
