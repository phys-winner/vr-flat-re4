#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int g_touch_context,g_touch_cancel,g_touch_gt_tid=41,g_touch_interact_request;
static long g_cmd_until[32];
static int fixture_tid=41;
#define SYS_gettid 0
#define syscall(x) ((long)fixture_tid)
#define LOGF(...) ((void)0)
#define LOGI(...) ((void)0)
static void* (*phone_player)(const void*),*(*phone_active)(void);
static unsigned (*phone_type)(void);
static void* (*hs_hand_class)(void),*(*ti_manager)(void),*(*ti_manager_class)(void);
static int (*ti_action_active)(void),(*ti_can_action)(void),(*rl_paused)(void),(*rl_player_valid)(void);
static int ti_ready=1;
static int interact_is_a(const void* object,const void* expected) {
    return object && *(const void* const*)((const char*)object+0x10)==expected;
}
#include QTE_KNIFE_FIXTURE
#include "../re4fix_qte.h"
#include QTE_NATIVE_FIXTURE
static unsigned char hand[0x598],pawn[0x1650],other[0x1650],manager[0x410];
static unsigned char hand_class[0x100],manager_class[0x100],settings[0xb8];
static void* active;
static int paused,valid=1,show=1,allowed=1,has_motion=1;
static unsigned motion=2;
static void* get_player(const void* p) { (void)p;return pawn; }
static void* get_active(void) { return active; }
static unsigned get_type(void) { return (unsigned)g_touch_context; }
static void* get_hand_class(void) { return hand_class; }
static void* get_manager(void) { return manager; }
static void* get_manager_class(void) { return manager_class; }
static int get_paused(void) { return paused; }
static int get_valid(void) { return valid; }
static int get_show(void) { return show; }
static int get_allowed(void) { return allowed; }
static unsigned get_motion(void) { return motion; }
static int get_has_motion(unsigned qte) { return has_motion && qte==1; }
static const void* get_settings(void) { return settings; }
static void seed(unsigned command,int context) {
    touch_qte_cancel();memset(g_cmd_until,0,sizeof g_cmd_until);
    memset(hand,0,sizeof hand);memset(pawn,0,sizeof pawn);memset(manager,0,sizeof manager);
    *(void**)(hand+0x10)=hand_class;*(void**)(manager+0x10)=manager_class;
    *(int*)(hand_class+0x58)=0x510;*(int*)(manager_class+0x58)=0x410;
    *(unsigned long long*)(pawn+0x18)=0x123456789abcdef0ULL;
    manager[0x3d9]=(unsigned char)command;manager[0x3da]=12;manager[0x3dc]=1;
    active=pawn;g_touch_context=context;g_touch_cancel=0;g_touch_interact_request=0;
    fixture_tid=41;paused=0;valid=1;show=1;allowed=1;has_motion=1;motion=2;
    qt_ready=1;qt_pawn=0;qt_name=0;qt_logs=0;qt_reset_pending=0;
    qt_motion_type=get_motion;qt_has_motion=get_has_motion;qt_settings=get_settings;
    phone_player=get_player;phone_active=get_active;phone_type=get_type;
    hs_hand_class=get_hand_class;ti_manager=get_manager;ti_manager_class=get_manager_class;
    ti_action_active=get_show;ti_can_action=get_allowed;rl_paused=get_paused;rl_player_valid=get_valid;
}
int main(void) {
    const unsigned commands[]={1,2,3,4,5,6,7,8,10,11,15};
    const unsigned masks[]={1,2,0x100,0x200,0x10000000,0x04000000,3,0x14000000,1,2,0x04000000};
    for (unsigned i=0;i<sizeof commands/sizeof commands[0];i++) {
        seed(commands[i],3);g_touch_interact_request=1;
        touch_qte_tick(hand,0,1000);assert(qt_mask==masks[i]);
        for(int bit=0;bit<32;bit++) assert(g_cmd_until[bit]==((masks[i]&(1u<<bit))?1090:0));
        touch_qte_tick(hand,0,1090);assert(!qt_mask);
    }
    seed(1,1);g_touch_interact_request=1;touch_qte_tick(hand,0,1000);
    assert(g_touch_interact_request==1 && !qt_mask); // Ordinary Interact retains its request.
    seed(8,3);g_cmd_until[28]=2000;g_touch_interact_request=1;touch_qte_tick(hand,0,1000);
    assert(!qt_mask && g_cmd_until[28]==2000 && !g_cmd_until[26]);
    seed(7,3);g_touch_interact_request=1;touch_qte_tick(hand,0,1000);
    g_cmd_until[0]=3000;touch_qte_cancel();assert(g_cmd_until[0]==3000 && g_cmd_until[1]==0);
    seed(10,1);g_touch_interact_request=1;touch_qte_tick(hand,0,1000);
    touch_qte_tick(hand,0,1090);g_touch_interact_request=1;touch_qte_tick(hand,0,1100);
    assert(qt_until==1190); // Repeated physical taps produce separate normal edges.
    seed(16,3);g_touch_interact_request=1;touch_qte_tick(hand,0,1000);
    assert(qt_pose && !qt_mask);float off[3]={0};touch_qte_pose(1,1100,off);assert(off[1]>0);
    memset(off,0,sizeof off);touch_qte_pose(1,1640,off);assert(off[0]==0 && off[1]==0 && off[2]==0);
    for (unsigned m=0;m<12;m++) {
        seed(16,3);motion=m;g_touch_interact_request=1;touch_qte_tick(hand,0,1000);
        for(int side=0;side<2;side++) for(long t=1000;t<1640;t++) {
            float v[3]={0};touch_qte_pose(side,t,v);
            for(int k=0;k<3;k++) assert(isfinite(v[k]) && fabsf(v[k])<=1.1351f);
        }
    }
    seed(8,3);g_touch_interact_request=1;touch_qte_tick(hand,0,1000);
    g_touch_cancel=3;touch_qte_tick(hand,0,1010);assert(!qt_snapshot && !qt_pose && !qt_mask);
    seed(8,3);g_touch_interact_request=1;touch_qte_tick(hand,0,1000);
    active=other;touch_qte_tick(hand,0,1010);
    assert(!qt_snapshot && !qt_pose && !qt_mask);
    // Inactive pooled hands never publish or invoke the active pawn.
    struct mobile_action_state s;assert(!touch_action_state(1600,&s));
    seed(8,3);touch_qte_tick(hand,0,1000);assert(touch_action_state(1500,&s));
    assert(!touch_action_state(1501,&s) && !touch_action_state(999,&s));
    touch_qte_reset();assert(!touch_action_state(1000,&s));touch_qte_tick(hand,0,1010);
    seed(16,3);fixture_tid=99;g_touch_interact_request=1;touch_qte_tick(hand,0,1000);
    assert(!qt_snapshot && g_touch_interact_request);
    seed(8,3);manager[0x3da]=48;touch_qte_tick(hand,0,1000);assert(!qt_snapshot);
    seed(8,3);motion=12;touch_qte_tick(hand,0,1000);assert(!qt_snapshot);
    seed(8,3);*(int*)(hand_class+0x58)=0x500;touch_qte_tick(hand,0,1000);assert(!qt_snapshot);
    fixture_installer_tests();
    puts("PASS: native QTE key edges, combined masks, repeated taps, gesture geometry, fresh owned contexts and installer refusals");
    return 0;
}
