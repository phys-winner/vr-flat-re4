// Runs the actual input header without installing or driving the game.
#define RE4_INPUT_TEST
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define LOGF(...) ((void)0)
static long g_menu_tick_ms, g_tn, g_tq_calls, g_cmd_until[32], g_cmd_lstick_until;
static long g_cmd_rstick_until;
static unsigned g_comp_w, g_comp_h, g_grip_latched;
static float g_look_yaw, g_look_pitch, g_ptr_u, g_ptr_v, g_cmd_lstick[2];
static float g_cmd_rstick[2];
static int g_ptr_down, g_touch_weapon_request, g_touch_weapon, g_touch_pause, g_touch_cancel, g_anim_kind;
static int g_touch_reload_request, g_reload_phase;
static int g_touch_context, g_phone_request;
static int g_touch_inventory_request, g_touch_interact_request, g_touch_aim;
static int g_touch_back_request;
static int g_mobile_settings_request;
static volatile int g_touch_gt_tid;
static unsigned (*phone_type)(void);
static int input_test_thread=41;
static int input_test_gettid(void) { return input_test_thread; }
#define gettid input_test_gettid
static long g_ptr_until_ms, g_weapon_grip_at, g_anim_len, g_anim_t0;
static long g_mobile_holster_until;
static volatile long g_menu_tap_ms;
struct ui_target { float x, y; void* component; };
static struct ui_target g_ui_targets[16];
static int g_ui_target_count;
static long g_ui_targets_ms;
static pthread_mutex_t g_ui_target_mx = PTHREAD_MUTEX_INITIALIZER;
static void* volatile g_ui_direct_click;
// Keyframed knife pose lives in re4fix_weapon_pose.h (needs the OVR math); the input header only forwards these.
static long knife_slash_length(void) { return 640; }
static void knife_pose_drawn(long now) { (void)now; }
// Production: hold-to-aim stays the tested default; prompt/world taps belong to the GameThread world helpers.
static int input_aim_toggle;
static int mobile_prefs_aim_toggle(void) { return input_aim_toggle; }
static int input_aim_walk;
static int input_health_presentations;
static void touch_health_flat_presented(long now){assert(now>0);++input_health_presentations;}
static int mobile_prefs_aim_walk(void){return input_aim_walk;}
static int touch_phone_answer_visible(long now){(void)now;return 1;}
static int input_phone_skip;
static int touch_phone_skip_visible(long now){(void)now;return input_phone_skip;}
static int input_gyro;
static int mobile_prefs_gyro(void){return input_gyro;}
static int mobile_prefs_gyro_sensitivity(void){return 100;}
static float input_heal_progress;
static int touch_heal_progress_read(long now,float* value){(void)now;*value=input_heal_progress;return input_heal_progress>0;}
static unsigned input_toolbar_mask=3;
static unsigned touch_menu_toolbar_mask(long now){(void)now;return input_toolbar_mask;}
static volatile int g_touch_density_milli=2750;
static int touch_prompt_hit(float x, float y) { (void)x; (void)y; return 0; }
static int test_world_taps;
static void touch_world_tap(float x, float y) { (void)x; (void)y; test_world_taps++; }
static float g_anim_amp, g_anim_period;
#include "re4fix_mobile_view.h"
struct mobile_hud_state { int current_health,max_health; unsigned command,action; };
struct mobile_action_state { unsigned command,action,qte,motion,context; };
static int input_action_valid;
static int touch_action_state(long now,struct mobile_action_state* out) {
    (void)now; memset(out,0,sizeof *out);
    out->command=7;out->action=12;out->qte=1;out->context=3;
    return input_action_valid && g_touch_context==3;
}
static void touch_qte_reset(void) { input_action_valid=0; }
static int input_motion_busy;
static int input_gr_busy,input_gr_finger,input_gr_downs,input_gr_ups,input_gr_cancels,input_gr_accept=1;
static unsigned input_gr_owned;
static unsigned touch_grenade_owned_grip_mask(void){return input_gr_owned;}
static unsigned touch_grenade_input_reset(void){
    ++input_gr_cancels;
    if(input_gr_owned){int bit=input_gr_owned==0x08000000u?27:29;g_cmd_until[bit]=LONG_MAX;}
    return input_gr_owned;
}
static void touch_grenade_cancel(void){(void)touch_grenade_input_reset();}
static void touch_grenade_fire_up(long now){(void)now;++input_gr_ups;input_gr_finger=0;}
static int touch_grenade_fire_down(long now){(void)now;if(!input_gr_accept||input_gr_finger)return 0;input_gr_finger=1;++input_gr_downs;return 1;}
static int touch_motion_pose_busy(void){return input_motion_busy||input_gr_busy;}
static int touch_qte_pose_busy(long now){(void)now;return input_motion_busy;}
static int input_hud_valid;
static struct mobile_hud_state input_hud;
static int touch_hud_state_read(long now,struct mobile_hud_state* out) {
    (void)now; *out=input_hud; return input_hud_valid;
}
struct touch_mobile_settings {float sensitivity,size,x,y,fps;};
static struct touch_mobile_settings input_settings={1,1,0,0,60};
static struct touch_mobile_settings touch_mobile_settings_snapshot(void) {return input_settings;}
struct touch_quickselect_state {unsigned available;int selected,fire_side,mode;};
enum { QS_HELD_WORLD_GUN=4 };
static int input_qs_ready,input_qs_busy,input_qs_requested=-1,input_qs_cancelled,input_qs_stale,input_qs_requests;
static struct touch_quickselect_state input_qs={0,-1,-1,0};
static struct touch_quickselect_state touch_quickselect_state(long now){(void)now;return input_qs;}
static int touch_quickselect_ready(void){return input_qs_ready;}
static int touch_quickselect_busy(void){return input_qs_busy;}
static int touch_quickselect_live(long now){(void)now;return input_qs_ready&&!input_qs_stale;}
static volatile long g_heal_holster_ms;
static volatile long g_watch_since_ms, g_watch_until_ms;
static int touch_tutorial_gesture(int z, long now){(void)z;(void)now;return 0;}
static int input_reload_hint,input_reload_needed;
static int touch_tutorial_reload_hint(long now){(void)now;return input_reload_hint;}
static int touch_hud_reload_needed(long now){(void)now;return input_reload_needed;}
static int touch_quickselect_request(int slot,long now){(void)now;if(!(input_qs.available&(1u<<slot)))return 0;input_qs_requested=slot;input_qs_busy=1;++input_qs_requests;return 1;}
static void touch_quickselect_cancel(void){input_qs_busy=0;++input_qs_cancelled;}
// Helpers added with the p304 features (Ashley, ammo, throws, levers/cranks, map pinch, gamepad, focus).
static volatile int g_app_input_focus = 1;
static volatile long g_aim_painted_ms, g_cutscene_skippable_ms, g_cutscene_skip_request, g_qturn_request_ms;
static int input_assist;
static int mobile_prefs_assist(void) { return input_assist; }
static int mobile_prefs_qturn(void) { return 1; }
struct touch_ashley_state { int available, active, next; };
static struct touch_ashley_state touch_ashley_state(long now) { (void)now; struct touch_ashley_state s = {0, -1, -1}; return s; }
static int touch_ashley_request(long now) { (void)now; return 0; }
static int touch_ashley_health(long now, int* current, int* maximum) { (void)now; (void)current; (void)maximum; return 0; }
static int touch_throw_mode(long now) { (void)now; return 0; }
static int touch_throw_request(long now) { (void)now; return 0; }
static unsigned touch_throw_owned_grip_mask(void) { return 0; }
static unsigned input_pump_mask;
static unsigned touch_weapon_cycle_input_mask(long now,int menu){(void)now;return menu?0:input_pump_mask;}
static void touch_weapon_cycle_input_reset(void){input_pump_mask=0;}
static void touch_typewriter_cancel(void) {}
static int touch_typewriter_event(int handle,int type,float x,float y,float width,float height,long now) {
    (void)handle;(void)type;(void)x;(void)y;(void)width;(void)height;(void)now;return 0;
}
static int touch_sce_state(long now, int* kind, int* running, float* progress) { (void)now; (void)kind; (void)running; (void)progress; return 0; }
static int input_ammo_available,input_ammo_reads;
static int touch_hud_ammo_read(long now, int* loaded, int* spare) { (void)now;++input_ammo_reads;*loaded=17;*spare=83;return input_ammo_available; }
static int touch_map_active(long now) { (void)now; return 0; }
static void touch_map_zoom_add(float ratio) { (void)ratio; }
#include "re4fix_input.h"
static void touch_gamepad_frame(void) {}
static int touch_gamepad_recent(long now) { (void)now; return 0; }
static int touch_gamepad_hud_hidden(long now) { (void)now; return 0; }
void ovl_cell_uv(int cell, float* u0, float* v0, float* u1, float* v1) {
    *u0 = (float)cell; *v0 = 0; *u1 = (float)cell+1; *v1 = 1;
}
struct event { int dev, handle, type; float lx, ly, x, y; };
static void send_touch(int type, int handle, float x, float y) {
    struct event e = {0, handle, type, 0, 0, x, y};
    struct { void* data; int num, max; } arr = {&e, 1, 1};
    touch_process(&arr);
}
static void tap(int z) {
    struct ctl c=ctl_geometry(z);
    float x = c.cx * g_tw, y = c.cy * g_th;
    send_touch(0, 2, x, y); send_touch(2, 2, x, y);
}
static void test_mobile_repair_input(void) {
    touch_reset();g_touch_cancel=0;g_touch_context=1;g_touch_weapon=2;g_menu_tick_ms=0;
    g_comp_w=2400;g_comp_h=1080;g_tw=2400;g_th=1080;
    send_touch(0,190,5,700);
    assert(g_lstick_x==0 && g_lstick_y==0 && vt_find(190)->x0==5);
    float radius=58.0f*g_touch_density_milli/1000.0f;
    send_touch(1,190,5+radius*.1f,700);assert(g_lstick_x==0 && g_lstick_y==0);
    send_touch(1,190,5+radius*.56f,700);assert(fabsf(g_lstick_x-.5f)<.0001f);
    send_touch(1,190,5+radius*2,700);assert(g_lstick_x==1 && g_lstick_y==0);
    struct ovl_quad q[64];int n=touch_overlay_quads(q,64);
    for(int i=0;i<n;i++)assert(q[i].u0!=OVL_STICK_BASE && q[i].u0!=OVL_STICK_KNOB);
    input_heal_progress=.4f;n=touch_overlay_quads(q,64);int progress_rings=0;
    for(int i=0;i<n;i++)if(q[i].a==-2.0f){progress_rings++;assert(q[i].r==.4f && fabsf(q[i].x0+q[i].x1)<.00001f && fabsf(q[i].y0+q[i].y1)<.00001f);}
    assert(progress_rings==1);input_heal_progress=0;
    send_touch(2,190,5+radius*2,700);assert(!g_lstick_x && !g_lstick_y);
    struct vt normal={0},assisted={0};
    g_look_yaw=0;touch_drag_look(&normal,8,0,0);float ordinary=g_look_yaw;
    g_look_yaw=0;input_assist=1;g_touch_aim=1;g_aim_painted_ms=now_ms();
    touch_drag_look(&assisted,8,0,0);assert(fabsf(g_look_yaw-ordinary*.55f)<.00001f);
    input_assist=0;g_aim_painted_ms=0;
    input_gyro=1;g_touch_aim=1;g_look_yaw=g_look_pitch=0;
    assert(Java_com_epicgames_ue4_MobileSettings_nativeGyroActive(0,0));
    Java_com_epicgames_ue4_MobileSettings_nativeGyro(0,0,.1f,.2f);
    assert(fabsf(g_look_yaw+.1f)<.00001f && fabsf(g_look_pitch+.2f)<.00001f);
    Java_com_epicgames_ue4_MobileSettings_nativeGyro(0,0,NAN,0);
    Java_com_epicgames_ue4_MobileSettings_nativeGyro(0,0,1,0);
    assert(fabsf(g_look_yaw+.1f)<.00001f);
    g_touch_aim=0;Java_com_epicgames_ue4_MobileSettings_nativeGyro(0,0,.2f,.2f);
    assert(fabsf(g_look_yaw+.1f)<.00001f);
    g_touch_aim=1;g_app_input_focus=0;assert(!Java_com_epicgames_ue4_MobileSettings_nativeGyroActive(0,0));
    g_app_input_focus=1;g_touch_context=3;assert(!Java_com_epicgames_ue4_MobileSettings_nativeGyroActive(0,0));
    g_cutscene_skippable_ms=now_ms();g_cutscene_skip_request=0;
    send_touch(0,191,g_tw-g_th*.09f,g_th*.88f);
    assert(g_cutscene_skip_request==now_ms() && vt_find(191)->skip==1);
    send_touch(2,191,g_tw-g_th*.09f,g_th*.88f);
    g_touch_context=5;input_phone_skip=1;n=touch_overlay_quads(q,64);int skips=0;
    for(int i=0;i<n;i++)if(q[i].u0==OVL_SKIP)skips++;
    assert(skips==1);tap(C_PHONE);assert(g_phone_request==1);
    input_phone_skip=input_gyro=0;touch_reset();g_touch_context=1;g_touch_cancel=0;g_menu_tick_ms=0;
    puts("PASS: invisible The Void movement origin/deadzone/walk/run/release; tap on cinema Skip; phone Skip; gyro ADS/focus/context/nonfinite/bounds gates");
}
static void test_context_lifetime(void) {
    unsigned char state[96];
    touch_reset(); g_touch_context=1; g_menu_tick_ms=0; g_touch_weapon=2;
    g_grip_latched=OB_RHAND; g_weapon_grip_at=0;
    float fx=g_ctl[C_FIRE].cx*g_tw,fy=g_ctl[C_FIRE].cy*g_th;
    float ax=g_ctl[C_AIM].cx*g_tw,ay=g_ctl[C_AIM].cy*g_th;
    send_touch(0,7,fx,fy);
    send_touch(0,8,350,780); send_touch(1,8,350,500);
    send_touch(0,6,ax,ay);
    fill_controller_state(state,3);
    assert(((float*)state)[5]==1 && ((float*)state)[9]>.99f && g_touch_aim);
    // Enter and leave a native pause menu while all three fingers remain down.
    g_touch_context=2;g_menu_tick_ms=now_ms();fill_controller_state(state,3);
    assert(((float*)state)[5]==0 && ((float*)state)[9]==0 && !g_touch_aim);
    assert(vt_find(7)->role==RT_NONE && vt_find(8)->role==RT_NONE && vt_find(6)->role==RT_NONE);
    assert(g_touch_weapon==2 && g_grip_latched==OB_RHAND && ((float*)state)[7]==1);
    // In-world overlay menus run in context 1, so a fresh menu tick keeps menu mode for its 300 ms window.
    g_touch_context=1;fill_controller_state(state,3);
    assert(((float*)state)[5]==0 && ((float*)state)[9]==0 && !g_touch_aim);
    usleep(310000);fill_controller_state(state,3);
    assert(!menu_mode() && ((float*)state)[5]==0 && ((float*)state)[9]==0 && !g_touch_aim);
    // MOVE events from those old fingers cannot revive their previous actions.
    send_touch(1,7,fx,fy);send_touch(1,8,350,490);send_touch(1,6,ax,ay);
    fill_controller_state(state,3);
    assert(((float*)state)[5]==0 && ((float*)state)[9]==0 && !g_touch_aim && !g_ptr_down);
    send_touch(2,7,fx,fy);send_touch(2,8,350,490);send_touch(2,6,ax,ay);
    send_touch(0,7,fx,fy);fill_controller_state(state,3);
    assert(((float*)state)[5]==1); // A fresh DOWN still starts FIRE normally.
    send_touch(2,7,fx,fy);usleep(160000);fill_controller_state(state,3);
    assert(((float*)state)[5]==0);

    // A direct menu click has an RT pulse, which ends on the raw pawn-context
    // transition even while the 300ms menu-hand timestamp remains recent.
    touch_reset();g_touch_context=2;g_touch_weapon=2;g_menu_tick_ms=now_ms();fill_controller_state(state,3);
    send_touch(0,4,1400,600);send_touch(2,4,1400,600);
    assert(g_tap_until_ms>g_tap_press_ms);
    usleep(130000);fill_controller_state(state,3);assert(((float*)state)[5]==1);
    g_touch_context=1;g_menu_tick_ms=0;fill_controller_state(state,3);
    assert(!menu_mode() && ((float*)state)[5]==0 && !g_tap_until_ms && !g_tap_press_ms);
    // A NEW tap in gameplay without an in-world menu is incapable of issuing a pointer RT pulse.
    send_touch(0,4,1400,600);send_touch(2,4,1400,600);
    usleep(130000);fill_controller_state(state,3);
    assert(g_touch_context==1 && !menu_mode() && !g_tap_until_ms && !g_tap_press_ms);
    assert(((float*)state)[5]==0 && g_touch_weapon==2);
    usleep(310000);fill_controller_state(state,3);assert(((float*)state)[5]==0);

    // An in-world menu (range console, quit prompt) ticks its menu hand during gameplay: menu mode
    // takes over and the held FIRE ends; it does not come back when the menu closes.
    touch_reset();g_touch_context=1;g_menu_tick_ms=0;
    send_touch(0,7,fx,fy);fill_controller_state(state,3);assert(((float*)state)[5]==1);
    g_menu_tick_ms=now_ms();fill_controller_state(state,3);assert(menu_mode() && ((float*)state)[5]==0);
    g_menu_tick_ms=0;fill_controller_state(state,3);assert(!menu_mode() && ((float*)state)[5]==0);
    send_touch(2,7,fx,fy);

    // After a focus reset Android may send MOVE without a corresponding DOWN.
    // It must not recreate movement, look, FIRE or a menu pointer.
    touch_reset();g_touch_context=1;g_menu_tick_ms=0;
    float yaw=g_look_yaw,pitch=g_look_pitch;
    send_touch(1,91,1500,600);send_touch(1,91,1540,630);
    send_touch(1,92,350,780);send_touch(1,92,350,500);
    fill_controller_state(state,3);
    assert(!vt_find(91) && !vt_find(92) && !g_ptr_down && !g_touch_aim);
    assert(g_look_yaw==yaw && g_look_pitch==pitch && ((float*)state)[5]==0 && ((float*)state)[9]==0);
    send_touch(0,91,1500,600);send_touch(1,91,1540,630);
    send_touch(0,92,350,780);send_touch(1,92,350,500);
    fill_controller_state(state,3);
    assert(g_look_yaw<yaw && ((float*)state)[9]>.99f);
    send_touch(2,91,1540,630);send_touch(2,92,350,500);
    touch_reset();
}
static void test_pump_controller_input(void){
    unsigned char state[96];
    for(int side=0;side<2;side++){
        touch_reset();g_touch_context=1;g_touch_cancel=0;g_menu_tick_ms=0;fill_controller_state(state,3);
        unsigned bit=side?OB_RHAND:OB_LHAND;input_pump_mask=bit;
        fill_controller_state(state,3);assert(((unsigned*)state)[1]&bit);
        assert(((float*)state)[side?7:6]==1&&!g_cmd_until[side?27:29]);
        g_touch_context=2;g_menu_tick_ms=now_ms();fill_controller_state(state,3);
        assert(!input_pump_mask&&!(((unsigned*)state)[1]&bit)&&((float*)state)[side?7:6]==0);
        g_touch_context=1;g_menu_tick_ms=0;fill_controller_state(state,3);
        assert(!(((unsigned*)state)[1]&bit));
        input_pump_mask=bit;touch_reset();assert(!input_pump_mask);
    }
    puts("PASS: pump publication reaches actual controller button/analog for each hand; menu transition and full input reset retire it without command writes");
}
static void seed_game_pawn_input(void) {
    touch_reset(); g_touch_context=1; g_menu_tick_ms=0; g_touch_cancel=0;
    g_touch_weapon=2; g_touch_weapon_request=2; g_weapon_grip_at=-1;
    g_grip_latched=OB_RHAND|OB_LHAND;
    send_touch(0,71,g_ctl[C_FIRE].cx*g_tw,g_ctl[C_FIRE].cy*g_th);
    send_touch(0,72,g_ctl[C_AIM].cx*g_tw,g_ctl[C_AIM].cy*g_th);
    send_touch(0,73,350,780); send_touch(1,73,350,500);
    send_touch(0,74,1500,700);
    long until=now_ms()+1000;
    g_cmd_until[0]=g_cmd_until[28]=g_cmd_until[29]=until;
    g_cmd_lstick[0]=.25f; g_cmd_lstick[1]=.5f; g_cmd_lstick_until=until;
    g_cmd_rstick[0]=-.25f; g_cmd_rstick[1]=-.5f; g_cmd_rstick_until=until;
    g_btn_hold_until[C_FIRE]=until;
    g_tap_press_ms=now_ms()-10; g_tap_until_ms=until; g_ptr_until_ms=until;
    g_mobile_holster_until=until; g_anim_kind=2; g_reload_phase=3;
    g_touch_reload_request=g_touch_inventory_request=g_touch_interact_request=1;
    g_touch_pause=g_phone_request=g_touch_back_request=1;
}
static void assert_game_pawn_input_owned(void) {
    unsigned char state[96]; fill_controller_state(state,3);
    assert(g_touch_weapon==2 && g_touch_weapon_request==2 && g_weapon_grip_at==-1);
    assert(g_grip_latched==(OB_RHAND|OB_LHAND) && g_touch_aim==1 && g_ptr_down==1);
    assert(g_cmd_until[0]>now_ms() && g_cmd_until[28]>now_ms() && g_cmd_until[29]>now_ms());
    assert(g_cmd_lstick_until>now_ms() && g_cmd_rstick_until>now_ms() && g_mobile_holster_until>now_ms());
    assert(g_touch_reload_request && g_touch_inventory_request && g_touch_interact_request);
    assert(g_touch_pause && g_phone_request && g_touch_back_request && g_reload_phase==3 && g_anim_kind==2);
    assert(((float*)state)[4]==1 && ((float*)state)[5]==1 && ((float*)state)[6]==1 && ((float*)state)[7]==1);
    assert(((float*)state)[8]==.25f && ((float*)state)[9]==.5f && ((float*)state)[10]==-.25f && ((float*)state)[11]==-.5f);
}
static void assert_new_game_pawn_neutral(void) {
    unsigned char state[96]; fill_controller_state(state,3);
    assert(g_touch_weapon==0 && g_touch_weapon_request==0 && g_weapon_grip_at==0);
    assert(g_grip_latched==0 && !g_touch_aim && !g_ptr_down && !g_ptr_until_ms);
    assert(!g_touch_reload_request && !g_touch_inventory_request && !g_touch_interact_request);
    assert(!g_touch_pause && !g_phone_request && !g_touch_back_request && !g_reload_phase && !g_anim_kind);
    assert(!g_cmd_lstick_until && !g_cmd_rstick_until && !g_mobile_holster_until);
    assert(!g_tap_press_ms && !g_tap_until_ms && g_touch_cancel==3);
    for(int i=0;i<32;i++) assert(g_cmd_until[i]==0);
    for(int i=0;i<C_N;i++) assert(g_btn_hold_until[i]==0);
    for(int i=0;i<MAXT;i++) assert(!g_vt[i].on);
    for(int i=4;i<12;i++) assert(((float*)state)[i]==0); // both triggers/grips and both sticks
}
static void test_new_game_pawn_lifetime(void) {
    unsigned char first[0x40],second[0x40],before[0x40];
    const unsigned long long initial=0x0102030405060708ULL,renamed=0x9902030405060708ULL;
    _Static_assert(sizeof(initial)==8,"native UObject FName identity width");
    memset(first,0xa5,sizeof first); memset(second,0x5a,sizeof second);
    memcpy(first+0x18,&initial,sizeof initial); memcpy(second+0x18,&renamed,sizeof renamed);
    g_touch_last_game_pawn=0; g_touch_last_game_name=0;
    seed_game_pawn_input(); memcpy(before,first,sizeof first);
    touch_new_game_pawn(first); // first observation preserves the user's existing selection/input
    assert(!memcmp(first,before,sizeof first) && g_touch_last_game_pawn==first && g_touch_last_game_name==initial);
    assert_game_pawn_input_owned();
    touch_new_game_pawn(0); assert_game_pawn_input_owned();
    assert(g_touch_last_game_pawn==first && g_touch_last_game_name==initial);
    touch_new_game_pawn(first); assert_game_pawn_input_owned(); // same pointer + complete same FName

    // Caller omits this helper for menu/cinema/phone objects. Returning to the
    // exact game pawn retains weapon intent/grips; context code still clears old fingers.
    unsigned char state[96];
    g_touch_context=2; g_menu_tick_ms=now_ms(); fill_controller_state(state,3);
    g_touch_context=3; fill_controller_state(state,3);
    g_touch_context=5; fill_controller_state(state,3);
    g_touch_context=1; g_menu_tick_ms=0; touch_new_game_pawn(first); fill_controller_state(state,3);
    assert(g_touch_weapon==2 && g_touch_weapon_request==2 && g_grip_latched==(OB_RHAND|OB_LHAND));
    assert(g_touch_last_game_pawn==first && g_touch_last_game_name==initial && g_touch_cancel==0);
    assert(!g_touch_aim); // ordinary context-transition rule remains intact

    seed_game_pawn_input();
    memcpy(first+0x18,&renamed,sizeof renamed); memcpy(before,first,sizeof first);
    float yaw=g_look_yaw,pitch=g_look_pitch;
    touch_new_game_pawn(first); // same address reused with a changed high FName word
    assert_new_game_pawn_neutral();
    assert(!memcmp(first,before,sizeof first) && g_touch_last_game_name==renamed);
    assert(g_look_yaw==yaw && g_look_pitch==pitch); // no native/world/view-angle mutation
    send_touch(1,71,g_ctl[C_FIRE].cx*g_tw,g_ctl[C_FIRE].cy*g_th);
    send_touch(1,72,g_ctl[C_AIM].cx*g_tw,g_ctl[C_AIM].cy*g_th);
    send_touch(1,73,350,490); send_touch(1,74,1540,730);
    assert_new_game_pawn_neutral(); // stale physical MOVE cannot revive old controls
    seed_game_pawn_input(); memcpy(before,second,sizeof second);
    touch_new_game_pawn(second); // different address, same full FName must still reset
    assert_new_game_pawn_neutral();
    assert(!memcmp(second,before,sizeof second) && g_touch_last_game_pawn==second && g_touch_last_game_name==renamed);
    tap(C_WEAPON); assert(g_touch_weapon==2 && g_touch_weapon_request==2); // fresh selection works
    touch_new_game_pawn(second); assert(g_touch_weapon==2 && g_touch_weapon_request==2);
    seed_game_pawn_input(); g_touch_last_game_pawn=(void*)1;
    touch_new_game_pawn(second); assert_new_game_pawn_neutral(); // cached stale address is never dereferenced
    g_touch_last_game_pawn=0; g_touch_last_game_name=0;
}
static unsigned input_native_type;
static int input_native_reads;
static unsigned input_native_get_type(void) {
    assert(g_touch_gt_tid>0 && input_test_thread==g_touch_gt_tid);
    input_native_reads++; return input_native_type;
}
static void menu_pointer_setup(void) {
    phone_type=0; g_touch_gt_tid=0; input_test_thread=41;
    touch_reset(); g_touch_context=2; g_menu_tick_ms=now_ms();
    unsigned char state[96]; fill_controller_state(state,3);
}
static float menu_pointer_rt(unsigned mask) {
    unsigned char state[96]; g_menu_tick_ms=now_ms(); fill_controller_state(state,mask);
    return ((float*)state)[5];
}
static void test_menu_pointer_drag(void) {
    menu_pointer_setup(); float yaw=g_look_yaw,pitch=g_look_pitch;
    send_touch(0,101,1400,600);
    assert(g_menu_pointer.down && g_menu_pointer.h==101 && !g_menu_pointer.pressed);
    assert(g_menu_pointer.press_ms==vt_find(101)->t0ms+120 && g_ptr_down==1 && vt_find(101)->role==RT_MENU_POINTER);
#ifdef RE4_INPUT_DETERMINISTIC_CLOCK
    assert(menu_pointer_rt(3)==0); usleep(119000); assert(menu_pointer_rt(3)==0);
#else
    // A live usleep(119ms) may resume after120ms under game/device scheduling.
    // Keep exact boundary tests in the deterministic host clock; live fixtures
    // use explicit far-future/past test deadlines, after checking scheduled+120.
    g_menu_pointer.press_ms=now_ms()+60000; assert(menu_pointer_rt(3)==0);
#endif
    send_touch(1,101,1600,650);
    assert(fabsf(g_ptr_u-1600/g_tw)<.00001f && fabsf(g_ptr_v-650/g_th)<.00001f);
    assert(g_look_yaw==yaw && g_look_pitch==pitch);
#ifdef RE4_INPUT_DETERMINISTIC_CLOCK
    usleep(1000);
#else
    g_menu_pointer.press_ms=now_ms()-60000;
#endif
    assert(menu_pointer_rt(3)==1 && g_menu_pointer.pressed);
    for(int i=0;i<5;i++) { usleep(200000); send_touch(1,101,1600+i*20,650+i*10); assert(menu_pointer_rt(3)==1); }
    send_touch(2,101,1750,710); assert(menu_pointer_rt(3)==0);
    assert(!g_menu_pointer.down && !g_menu_pointer.pressed && !g_tap_press_ms && !g_tap_until_ms && !g_ptr_down);
    assert(fabsf(g_ptr_u-1750/g_tw)<.00001f && fabsf(g_ptr_v-710/g_th)<.00001f);
    usleep(200000); assert(menu_pointer_rt(3)==0); // held drag release never emits another tap

    menu_pointer_setup(); send_touch(0,102,1400,600);
#ifdef RE4_INPUT_DETERMINISTIC_CLOCK
    usleep(40000);
#endif
    send_touch(2,102,1405,605);
    assert(g_tap_until_ms-g_tap_press_ms==150);
#ifdef RE4_INPUT_DETERMINISTIC_CLOCK
    long press=now_ms()+120; assert(g_tap_press_ms==press && g_tap_until_ms==press+150);
    assert(!g_menu_pointer.down && menu_pointer_rt(3)==0);
    usleep(119000); assert(menu_pointer_rt(3)==0); usleep(1000); assert(menu_pointer_rt(3)==1);
    usleep(149000); assert(menu_pointer_rt(3)==1); usleep(1000); assert(menu_pointer_rt(3)==0);
#else
    g_tap_press_ms=now_ms()+60000; g_tap_until_ms=g_tap_press_ms+150;
    assert(!g_menu_pointer.down && menu_pointer_rt(3)==0);
    g_tap_press_ms=now_ms()-60000; g_tap_until_ms=now_ms()+60000;
    assert(menu_pointer_rt(3)==1);
    g_tap_until_ms=now_ms()-60000; assert(menu_pointer_rt(3)==0);
#endif

    // A left-only poll never reports a right-trigger held press. Without one,
    // releasing a short stationary touch still falls back to the existing tap.
    menu_pointer_setup(); send_touch(0,103,1400,600); usleep(130000);
    assert(menu_pointer_rt(1)==0 && !g_menu_pointer.pressed);
    send_touch(2,103,1400,600); assert(g_tap_until_ms-g_tap_press_ms==150);

    menu_pointer_setup(); send_touch(0,104,1400,600); send_touch(0,105,1800,700);
    assert(g_menu_pointer.h==104 && g_ptr_down==1 && vt_find(105)->role==RT_NONE);
    float owner_u=g_ptr_u,owner_v=g_ptr_v;
    send_touch(1,105,1850,730); assert(g_ptr_u==owner_u && g_ptr_v==owner_v);
    usleep(120000); assert(menu_pointer_rt(3)==1);
    send_touch(2,104,1450,630); assert(menu_pointer_rt(3)==0 && !g_tap_until_ms);
    send_touch(1,105,1900,760); assert(menu_pointer_rt(3)==0 && g_ptr_down==0);
    send_touch(2,105,1900,760); assert(!g_tap_until_ms);
    send_touch(0,105,1800,700); assert(g_menu_pointer.h==105 && g_ptr_down==1);
    usleep(120000); assert(menu_pointer_rt(3)==1);
    send_touch(0,105,1810,710); assert(g_ptr_down==1 && menu_pointer_rt(3)==0); // duplicate DOWN cannot leak count
    usleep(120000); assert(menu_pointer_rt(3)==1); send_touch(2,105,1810,710); assert(menu_pointer_rt(3)==0);

    menu_pointer_setup(); send_touch(0,106,1400,600); usleep(120000); assert(menu_pointer_rt(3)==1);
    touch_reset(); assert(menu_pointer_rt(3)==0 && !g_menu_pointer.down && !g_menu_pointer.pressed);
    send_touch(1,106,1500,650); assert(menu_pointer_rt(3)==0 && !vt_find(106));
    menu_pointer_setup(); send_touch(0,107,1400,600); usleep(120000); assert(menu_pointer_rt(3)==1);
    g_touch_context=3; assert(menu_pointer_rt(3)==0 && !g_menu_pointer.down && !g_tap_until_ms);
    g_touch_context=2; assert(menu_pointer_rt(3)==0); send_touch(1,107,1500,650); assert(menu_pointer_rt(3)==0);

    unsigned char first[0x40]={0},second[0x40]={0};
    g_touch_last_game_pawn=0; g_touch_last_game_name=0; g_touch_context=1; touch_new_game_pawn(first);
    menu_pointer_setup(); send_touch(0,108,1400,600); usleep(120000); assert(menu_pointer_rt(3)==1);
    g_touch_context=1; touch_new_game_pawn(second); assert(menu_pointer_rt(3)==0 && !g_menu_pointer.down && !g_tap_until_ms);
    g_touch_last_game_pawn=0; g_touch_last_game_name=0;

    // Context must be refreshed on the known GameThread before payload creation.
    menu_pointer_setup(); input_native_reads=0; input_native_type=2;
    phone_type=input_native_get_type; g_touch_gt_tid=41;
    send_touch(0,109,1400,600); usleep(120000);
    assert(menu_pointer_rt(3)==1 && input_native_reads==1 && g_menu_pointer.pressed);
    input_native_type=1; input_test_thread=42;
    assert(menu_pointer_rt(3)==1 && input_native_reads==1 && g_touch_context==2); // no GUObjectArray call offGT
    input_test_thread=41;
    assert(menu_pointer_rt(3)==0 && input_native_reads==2 && g_touch_context==1);
    assert(!g_menu_pointer.down && !g_tap_until_ms && vt_find(109)->role==RT_NONE);
    input_test_thread=42; assert(menu_pointer_rt(3)==0 && input_native_reads==2);
    input_test_thread=41; send_touch(1,109,1500,650); assert(menu_pointer_rt(3)==0);
    send_touch(2,109,1500,650); assert(!g_tap_until_ms);

    menu_pointer_setup(); input_native_reads=0; input_native_type=1; phone_type=input_native_get_type;
    g_touch_gt_tid=0; assert(menu_pointer_rt(3)==0 && !input_native_reads && g_touch_context==2);
    g_touch_gt_tid=-1; assert(menu_pointer_rt(3)==0 && !input_native_reads && g_touch_context==2);
    g_touch_gt_tid=41; phone_type=0; assert(menu_pointer_rt(3)==0 && !input_native_reads && g_touch_context==2);
    phone_type=input_native_get_type;
    unsigned char state[96]; fill_controller_state(state,0); assert(!input_native_reads && ((unsigned*)state)[0]==0);
    input_native_type=7; assert(menu_pointer_rt(3)==0 && input_native_reads==1 && g_touch_context==7);

    // Gameplay movement/look holds never acquire the native menu-pointer trigger.
    phone_type=0; g_touch_gt_tid=0; touch_reset(); g_touch_context=1; g_menu_tick_ms=0;
    send_touch(0,110,350,780); send_touch(1,110,350,500);
    send_touch(0,111,1400,600); send_touch(1,111,1460,620); usleep(200000);
    assert(menu_pointer_rt(3)==0 && !g_menu_pointer.down);
    send_touch(2,110,350,500); send_touch(2,111,1460,620); assert(!g_tap_until_ms);
    touch_reset(); input_test_thread=41;
}
static void test_mobile_hud(void) {
    struct ovl_quad quads[48];
    touch_reset(); g_touch_context=1; g_menu_tick_ms=0;
    input_hud_valid=1; input_hud=(struct mobile_hud_state){600,1200,1,7};
    int n=touch_overlay_quads(quads,48), health=0,kick=0;
    for (int i=0;i<n;i++) {
        if (quads[i].a<0) { health++; assert(quads[i].r==.5f && quads[i].g==1 && quads[i].u0==OVL_HEALTH); }
        if (quads[i].u0==OVL_KICK) kick++;
        assert(quads[i].x0>=-1 && quads[i].x1<=1 && quads[i].y0>=-1 && quads[i].y1<=1);
    }
    assert(health==1 && kick==1);
    assert(ctl_at(g_tw*.22f,g_th*.84f,0)==-1);
    memset(quads,0x5a,sizeof quads);
    assert(touch_overlay_quads(quads,0)==0 && ((unsigned char*)quads)[0]==0x5a);
    for (int context=2;context<=7;context++) {
        g_touch_context=context; g_menu_tick_ms=now_ms(); n=touch_overlay_quads(quads,48);
        for (int i=0;i<n;i++) assert(quads[i].a>=0 && quads[i].u0!=OVL_KICK);
    }
    g_touch_context=1;g_menu_tick_ms=0;input_hud_valid=0; n=touch_overlay_quads(quads,48);
    for (int i=0;i<n;i++) assert(quads[i].a>=0 && quads[i].u0!=OVL_KICK);
    touch_reset();
}

static void test_mobile_view(void) {
    for (int context=0;context<=7;context++) {
        float pitch=-1.2f;
        float framed=mobile_view_pitch(context,pitch);
        assert(pitch==-1.2f);
        assert(framed==((context==2 || context==4 || context==5 || context==7)?0.0f:pitch));
    }
    assert(mobile_phone_approach(999,1000)==0 && mobile_phone_approach(1000,1000)==0);
    assert(mobile_phone_approach(1450,1000)==0.5f && mobile_phone_approach(1900,1000)==1);
    float previous=0,off[3],wq[4];
    mobile_phone_pose(0,off,wq);
    assert(fabsf(wq[0]-0.70710678f)<.000001f && fabsf(wq[3]-0.70710678f)<.000001f && off[2]==-0.65f);
    mobile_phone_pose(1,off,wq);
    assert(fabsf(wq[0]-0.506273f)<.00001f && fabsf(wq[2]-0.014289f)<.00001f && fabsf(off[2]+0.22314f)<.00001f);
    for (int ms=0;ms<=1000;ms++) {
        float alpha=mobile_phone_approach(1000+ms,1000);
        assert(alpha>=previous && alpha<=1); previous=alpha;
        mobile_phone_pose(alpha,off,wq);
        assert(isfinite(off[0]) && off[2]>=-0.65001f && off[2]<=-0.22313f);
        assert(fabsf(wq[0]*wq[0]+wq[1]*wq[1]+wq[2]*wq[2]+wq[3]*wq[3]-1)<.0001f);
    }
    assert(mobile_phone_hit(.5f,.6f) && !mobile_phone_hit(.1f,.6f) && !mobile_phone_hit(NAN,.6f));
    touch_reset(); g_touch_context=5; g_menu_tick_ms=0;
    send_touch(0,77,g_tw*.5f,g_th*.6f); assert(g_phone_request==1);
    g_phone_request=0;
    send_touch(1,77,g_tw*.6f,g_th*.7f); send_touch(2,77,g_tw*.6f,g_th*.7f);
    assert(!g_phone_request && !g_buttons && !g_ptr_down);
    send_touch(0,78,g_tw*.1f,g_th*.2f); send_touch(2,78,g_tw*.1f,g_th*.2f);
    assert(!g_phone_request);
    touch_reset(); g_touch_context=1;
}

static void test_settings_and_favorites(void) {
    unsigned char state[96];
    touch_reset();g_touch_cancel=0;g_menu_tick_ms=0;
    input_qs_ready=1;input_qs.available=(1u<<0)|(1u<<1)|(1u<<2)|(1u<<3)|(1u<<6);
    input_qs.selected=0;input_qs.fire_side=1;
    for(int width=1920;width<=2400;width+=480)for(int context=1;context<=7;context++) {
        if(context!=1 && context!=2 && context!=7)continue;
        g_touch_context=context;g_menu_tick_ms=context==1 ? 0 : now_ms();g_touch_weapon=2;
        g_tw=(float)width;g_th=1080;
        for(int size=750;size<=1250;size+=250)for(int shift=-80;shift<=80;shift+=80) {
            input_settings.size=(float)size/1000;input_settings.x=(float)shift/1000;input_settings.y=(float)shift/1000;
            for(int z=0;z<C_N;z++)if(ctl_visible(z,menu_mode())) {
                struct ctl c=ctl_geometry(z);float x=c.cx*g_tw,y=c.cy*g_th,r=c.r*g_th*1.15f;
                assert(x-r>=-.01f && x+r<=g_tw+.01f && y-r>=-.01f && y+r<=g_th+.01f);
                assert(ctl_at(x,y,menu_mode())==z);
                for(int k=z+1;k<C_N;k++)if(ctl_visible(k,menu_mode())) {
                    struct ctl d=ctl_geometry(k);float dx=(d.cx-c.cx)*g_tw,dy=(d.cy-c.cy)*g_th,both=(d.r+c.r)*g_th*1.15f;
                    if(dx*dx+dy*dy<=both*both)printf("geometry overlap width=%d context=%d size=%d shift=%d controls=%d,%d distance=%.3f radii=%.3f\n",width,context,size,shift,z,k,sqrtf(dx*dx+dy*dy),both);
                    fflush(stdout);
                    assert(dx*dx+dy*dy>both*both);
                }
            }
        }
    }
    input_settings=(struct touch_mobile_settings){1,1,0,0,60};g_touch_context=1;g_menu_tick_ms=0;
    touch_reset();g_touch_cancel=0;input_qs.selected=1;input_qs.fire_side=0;
    struct ctl fire=ctl_geometry(C_FIRE);
    send_touch(0,81,fire.cx*g_tw,fire.cy*g_th);fill_controller_state(state,3);
    assert(((float*)state)[4]==1 && ((float*)state)[5]==0 && g_touch_weapon==2);
    tap(C_WEAPON);assert(input_qs_requested==0 && !g_touch_weapon_request && !g_grip_latched);
    fill_controller_state(state,3);assert(((float*)state)[4]==0 && ((float*)state)[5]==0);
    input_qs_busy=0;input_qs.selected=0;input_qs.fire_side=1;
    send_touch(1,81,fire.cx*g_tw,fire.cy*g_th);fill_controller_state(state,3);
    assert(((float*)state)[4]==0 && ((float*)state)[5]==0);
    send_touch(2,81,fire.cx*g_tw,fire.cy*g_th);send_touch(0,81,fire.cx*g_tw,fire.cy*g_th);
    fill_controller_state(state,3);assert(((float*)state)[5]==1);
    input_qs.selected=6;fill_controller_state(state,3);assert(((float*)state)[5]==0);
    send_touch(2,81,fire.cx*g_tw,fire.cy*g_th);
    input_qs.selected=0;send_touch(0,82,fire.cx*g_tw,fire.cy*g_th);fill_controller_state(state,3);
    assert(((float*)state)[5]==1);input_motion_busy=1;fill_controller_state(state,3);
    assert(((float*)state)[4]==0 && ((float*)state)[5]==0 && vt_find(82)->role==RT_NONE);
    touch_button_press(C_RELOAD,0);assert(!g_touch_reload_request);
    send_touch(0,83,fire.cx*g_tw,fire.cy*g_th);fill_controller_state(state,3);assert(((float*)state)[5]==0);
    input_motion_busy=0;fill_controller_state(state,3);assert(((float*)state)[5]==0);
    send_touch(2,82,fire.cx*g_tw,fire.cy*g_th);send_touch(2,83,fire.cx*g_tw,fire.cy*g_th);
    input_qs.available=0;input_qs_requested=-1;tap(C_WEAPON);assert(input_qs_requested==-1 && !g_touch_weapon_request);
    // The gear button is retired (mobile settings live in the native options): it is never shown or requested.
    touch_reset();g_touch_context=2;g_menu_tick_ms=now_ms();tap(C_SETTINGS);assert(!g_mobile_settings_request);
    assert(!ctl_visible(C_SETTINGS,1) && !ctl_visible(C_SETTINGS,0));
    input_qs_ready=0;input_qs_busy=0;input_qs=(struct touch_quickselect_state){0,-1,-1,0};
    g_tw=2167;g_th=1080;g_menu_tick_ms=0;
}
static void test_quickselect_resume_request(void) {
    unsigned char state[96];const int buttons[]={C_KNIFE,C_WEAPON,C_HEAL},slots[]={3,0,6};
    for(int i=0;i<3;i++) {
        touch_reset();g_touch_context=1;g_touch_cancel=0;g_menu_tick_ms=0;g_app_input_focus=1;
        input_qs_ready=input_qs_stale=1;input_qs_requests=0;input_qs_requested=-1;
        input_qs=(struct touch_quickselect_state){0,6,1,1};
        touch_button_press(buttons[i],0);fill_controller_state(state,3);
        assert(g_touch_weapon_request==i+1 && g_weapon_grip_at==-1 && input_qs_requested==-1);
        // A fresh publisher may still temporarily forbid this slot; preserve the original tap.
        input_qs_stale=0;fill_controller_state(state,3);
        assert(g_touch_weapon_request==i+1 && g_weapon_grip_at==-1 && input_qs_requested==-1);
        assert(g_touch_weapon==3); // The published held item differs from pending knife/gun intent.
        input_qs.available=1u<<slots[i];g_grip_latched=OB_RHAND|OB_LHAND;
        g_aim_latched=g_touch_aim=1;g_btn_hold_until[C_FIRE]=now_ms()+1000;
        g_vt[0].on=g_vt[1].on=1;g_vt[0].role=RT_BTN+C_FIRE;g_vt[1].role=RT_BTN+C_AIM;
        fill_controller_state(state,3);
        assert(input_qs_requested==slots[i] && input_qs_requests==1 && !g_touch_weapon_request);
        assert(!g_weapon_grip_at && !g_grip_latched && !g_touch_aim && !g_aim_latched && !g_btn_hold_until[C_FIRE]);
        assert(g_vt[0].role==RT_NONE && g_vt[1].role==RT_NONE);
        fill_controller_state(state,3);assert(input_qs_requests==1);
    }
    for(int blocked=0;blocked<6;blocked++) {
        touch_reset();g_touch_context=1;g_touch_cancel=0;g_menu_tick_ms=0;g_app_input_focus=1;
        input_qs_ready=input_qs_stale=1;input_qs_requests=0;input_qs_requested=-1;
        input_qs=(struct touch_quickselect_state){1,6,1,1};touch_button_press(C_WEAPON,0);
        input_qs_stale=0;
        if(blocked<3)g_touch_context=(int[]){2,3,5}[blocked];
        if(blocked==3)g_app_input_focus=0;
        if(blocked==4)g_touch_cancel=3;
        if(blocked==5)g_menu_tick_ms=now_ms();
        fill_controller_state(state,3);assert(input_qs_requested==-1 && !input_qs_requests);
    }
    g_app_input_focus=1;g_touch_context=1;g_menu_tick_ms=0;touch_reset();g_touch_cancel=0;
    fill_controller_state(state,3);assert(!g_touch_weapon_request && !input_qs_requests);
    input_qs_ready=input_qs_busy=input_qs_stale=0;input_qs=(struct touch_quickselect_state){0,-1,-1,0};
    puts("PASS: stale-to-fresh Quick Select forwards pending knife/gun/heal once, preserves unavailable request, retires legacy grip/fire/aim, and blocks menu/context/focus/cancel");
}
static void test_grenade_input(void) {
    unsigned char state[96];
    for(int side=0;side<2;side++) {
        input_gr_owned=0;input_gr_busy=input_motion_busy=0;input_gr_accept=1;
        touch_reset();g_touch_context=1;g_touch_cancel=0;g_menu_tick_ms=0;
        input_qs_ready=1;input_qs_busy=0;input_qs=(struct touch_quickselect_state){1u<<2,2,side,1};
        fill_controller_state(state,3);
        struct ctl fire=ctl_geometry(C_FIRE);float x=fire.cx*g_tw,y=fire.cy*g_th;
        int before=input_gr_downs;
        send_touch(0,160,x,y);fill_controller_state(state,3);
        assert(input_gr_downs==before+1 && input_gr_finger && vt_find(160)->grenade);
        assert(vt_find(160)->role==RT_NONE && !g_btn_hold_until[C_FIRE]);
        assert(!(((unsigned*)state)[1]&(OB_RINDEX|OB_LINDEX)) && ((float*)state)[4]==0 && ((float*)state)[5]==0);
        send_touch(1,160,x,y);fill_controller_state(state,3);assert(input_gr_downs==before+1);
        send_touch(0,164,x,y);assert(vt_find(164)->grenade==2);send_touch(2,164,x,y);assert(input_gr_finger);
        usleep(6000000);send_touch(0,160,x,y);assert(input_gr_downs==before+1 && input_gr_finger);
        input_gr_busy=input_motion_busy=1;fill_controller_state(state,3);assert(vt_find(160)->grenade);
        int ups=input_gr_ups;send_touch(2,160,x,y);assert(input_gr_ups==ups+1 && !input_gr_finger);
        input_gr_busy=input_motion_busy=0;fill_controller_state(state,3);
        input_gr_accept=0;send_touch(0,161,x,y);assert(!input_gr_finger && vt_find(161)->grenade==2 && vt_find(161)->role==RT_NONE);
        send_touch(2,161,x,y);input_gr_accept=1;
        send_touch(0,165,x,y);assert(input_gr_finger);touch_button_press(C_INTERACT,0);
        assert(!input_gr_finger && vt_find(165)->grenade==2);g_touch_interact_request=0;
        send_touch(0,166,x,y);assert(input_gr_finger);send_touch(2,165,x,y);assert(input_gr_finger);
        send_touch(2,166,x,y);assert(!input_gr_finger);
        send_touch(0,167,x,y);assert(input_gr_finger);touch_button_press(C_GRENADE,0);
        assert(!input_gr_finger && vt_find(167)->grenade==2);input_qs_busy=0;
        send_touch(0,168,x,y);assert(input_gr_finger);send_touch(2,167,x,y);assert(input_gr_finger);
        send_touch(2,168,x,y);assert(!input_gr_finger);
        send_touch(0,162,x,y);assert(input_gr_finger);g_touch_context=2;g_menu_tick_ms=now_ms();fill_controller_state(state,3);
        assert(!input_gr_finger && !vt_find(162)->grenade && vt_find(162)->role==RT_NONE);
        g_touch_context=1;g_menu_tick_ms=0;fill_controller_state(state,3);before=input_gr_downs;
        send_touch(1,162,x,y);send_touch(2,162,x,y);assert(input_gr_downs==before);
        send_touch(0,163,x,y);assert(input_gr_finger);touch_reset();assert(!input_gr_finger && !vt_find(163));
        unsigned own=side?OB_RHAND:OB_LHAND;int bit=side?27:29;
        input_gr_owned=own;for(int k=0;k<32;k++)g_cmd_until[k]=now_ms()+5000;
        g_grip_latched=~0u;touch_reset();assert(g_cmd_until[bit]==LONG_MAX && !g_grip_latched);
        for(int k=0;k<32;k++)if(k!=bit)assert(!g_cmd_until[k]);
        g_touch_context=1;g_touch_cancel=0;g_menu_tick_ms=0;fill_controller_state(state,3);
        assert(((unsigned*)state)[1]&own);assert(((float*)state)[side?7:6]==1);
        g_touch_context=2;g_menu_tick_ms=now_ms();g_grip_latched=OB_RHAND|OB_LHAND;fill_controller_state(state,3);
        assert(!(((unsigned*)state)[1]&own) && ((float*)state)[side?7:6]==0);
        assert(((unsigned*)state)[1]&(side?OB_LHAND:OB_RHAND));assert(((float*)state)[side?6:7]==1);
        assert(g_cmd_until[bit]==LONG_MAX);
        input_gr_owned=0;touch_reset();
    }
    input_qs_ready=input_qs_busy=input_gr_busy=input_motion_busy=0;
    input_qs=(struct touch_quickselect_state){0,-1,-1,0};g_touch_context=1;g_menu_tick_ms=0;
    puts("PASS: actual managed grenade Down/Up without RT/LT, both hands, busy/context/focus retirement, exact reset deadline preservation and Menu button/analog filtering");
}
static void test_knife_watch_holds(void) {
    unsigned char state[96];
    touch_reset();input_qs_ready=input_qs_busy=input_motion_busy=input_gr_busy=0;
    input_qs=(struct touch_quickselect_state){0,-1,-1,0};
    g_touch_context=1;g_touch_cancel=0;g_menu_tick_ms=0;g_touch_weapon=1;g_pad_held_ctl=0;
    fill_controller_state(state,3);
    struct ctl fire=ctl_geometry(C_FIRE);float x=fire.cx*g_tw,y=fire.cy*g_th;
    tap(C_FIRE);long first=g_anim_t0,end=g_knife_swing_until;
    assert(g_anim_kind==2 && end-first==knife_slash_length());
    for(int i=0;i<8;i++) {usleep(10000);tap(C_FIRE);assert(g_anim_t0==first && g_knife_swing_until==end);}
    // Releasing a tap leaves its complete swing intact; an expired minimum
    // press is not treated as a new attack after the animation's end.
    usleep((unsigned long)(knife_slash_length()+200)*1000);fill_controller_state(state,3);
    assert(g_anim_t0==first && !g_buttons && !g_vt[0].on);
    send_touch(0,170,x,y);first=g_anim_t0;end=g_knife_swing_until;
    assert(first>0 && end-first==knife_slash_length());
    for(int i=0;i<4;i++){usleep(10000);fill_controller_state(state,3);assert(g_anim_t0==first);}
    usleep((unsigned long)(knife_slash_length()+20)*1000);fill_controller_state(state,3);
    long second=g_anim_t0;assert(second>=end && g_knife_swing_until==second+knife_slash_length());
    send_touch(2,170,x,y);assert(g_anim_t0==second && g_knife_swing_until==second+knife_slash_length());
    usleep((unsigned long)(knife_slash_length()+200)*1000);fill_controller_state(state,3);assert(g_anim_t0==second);
    // Focus/CANCEL and context transitions retire the held role and deadline.
    send_touch(0,170,x,y);assert(g_anim_kind==2);touch_reset();
    assert(!g_anim_kind && !g_knife_swing_until && !vt_find(170));
    g_touch_context=1;g_touch_cancel=0;g_touch_weapon=1;send_touch(0,170,x,y);
    g_touch_context=0;fill_controller_state(state,3);assert(!g_anim_kind && !g_knife_swing_until && vt_find(170)->role==RT_NONE);
    g_touch_context=1;fill_controller_state(state,3);send_touch(1,170,x,y);assert(!g_anim_kind);
    send_touch(2,170,x,y);touch_reset();g_touch_context=1;g_touch_cancel=0;g_touch_weapon=2;g_menu_tick_ms=0;
    input_hud_valid=1;input_hud=(struct mobile_hud_state){600,1200,0,0};
    float wx=.055f*g_tw,wy=.25f*g_th;
    send_touch(0,171,wx,wy);assert(vt_find(171)->role==RT_WATCH && g_watch_held);
    long raised=g_watch_since_ms;
    for(int i=0;i<10;i++) {usleep(400000);fill_controller_state(state,3);assert(g_watch_held && g_watch_since_ms==raised && g_watch_until_ms==now_ms()+500);}
    assert(now_ms()-raised>=4000);send_touch(2,171,wx,wy);
    assert(!g_watch_held && g_watch_until_ms==now_ms());
    send_touch(0,172,wx,wy);send_touch(1,172,wx+35,wy);
    assert(g_watch_held && vt_find(172)->role==RT_WATCH && g_lstick_x>0 && g_watch_until_ms==now_ms()+500);
    send_touch(2,172,wx+35,wy);
    send_touch(0,173,wx,wy);assert(g_watch_held);touch_reset();assert(!g_watch_held && !g_watch_since_ms && !g_watch_until_ms);
    g_touch_context=1;g_touch_cancel=0;send_touch(0,174,wx,wy);assert(g_watch_held);
    g_touch_context=0;fill_controller_state(state,3);assert(!g_watch_held && !g_watch_since_ms && !g_watch_until_ms);
    g_touch_context=1;fill_controller_state(state,3);send_touch(1,174,wx,wy);assert(!g_watch_held);
    send_touch(2,174,wx,wy);input_hud_valid=0;touch_reset();
    // Native held range guns are input state, not a healing item or inventory slot.
    for(int side=0;side<2;side++){
        g_touch_context=1;g_touch_cancel=0;g_menu_tick_ms=0;input_qs_ready=1;input_qs_busy=0;
        input_qs=(struct touch_quickselect_state){0,QS_HELD_WORLD_GUN,side,1};
        fill_controller_state(state,3);assert(g_touch_weapon==2);
        fire=ctl_geometry(C_FIRE);send_touch(0,175,fire.cx*g_tw,fire.cy*g_th);fill_controller_state(state,3);
        assert(((float*)state)[side ? 5 : 4]==1 && ((float*)state)[side ? 4 : 5]==0);
        send_touch(2,175,fire.cx*g_tw,fire.cy*g_th);touch_reset();
    }
    input_qs_ready=0;input_qs=(struct touch_quickselect_state){0,-1,-1,0};
    g_touch_context=1;g_touch_cancel=0;g_touch_weapon=2;g_menu_tick_ms=0;input_ammo_available=1;input_ammo_reads=0;
    struct ovl_quad quads[64];int n=touch_overlay_quads(quads,64);assert(!input_ammo_reads);
    for(int i=0;i<n;i++)assert(quads[i].u0<OVL_DIGIT0 || quads[i].u0>OVL_SLASH);
    pointer_click(g_tw*.45f,g_th*.42f,50);
    n=touch_overlay_quads(quads,64);
    int aim_dots=0;
    for(int i=0;i<n;i++)if(quads[i].u0==OVL_DOT){
        aim_dots++;assert(fabsf((quads[i].x0+quads[i].x1)*.5f)<.0001f && fabsf((quads[i].y0+quads[i].y1)*.5f)<.0001f);
    }
    assert(aim_dots==1);   // gun reticle remains, touch feedback is absent
    input_reload_needed=1;float low=1,high=0;
    for(int frame=0;frame<24;frame++){
        n=touch_overlay_quads(quads,64);
        for(int i=0;i<n;i++)if(quads[i].u0==OVL_RELOAD){assert(quads[i].a>=.349f && quads[i].a<=.801f);low=fminf(low,quads[i].a);high=fmaxf(high,quads[i].a);}
        usleep(100000);
    }
    assert(low<.36f && high>.79f);
    g_reload_phase=1;n=touch_overlay_quads(quads,64);
    for(int i=0;i<n;i++)if(quads[i].u0==OVL_RELOAD)assert(quads[i].a==1);
    g_reload_phase=0;input_reload_needed=0;input_reload_hint=1;
    input_settings=(struct touch_mobile_settings){1,1.25f,.03f,-.04f,60};
    struct ctl reload=ctl_geometry(C_RELOAD);n=touch_overlay_quads(quads,48);int arrow=0;
    for(int i=0;i<n;i++)if(quads[i].u0==OVL_SOLID){
        arrow++;assert(fabsf((quads[i].x0+quads[i].x1)*.5f-(reload.cx*2-1))<.0001f);
        assert(quads[i].y1<(reload.cy-reload.r)*2-1);
    }
    assert(arrow==4);
#ifdef RE4_RELOAD_ARROW_PROOF
    FILE* proof=fopen(RE4_RELOAD_ARROW_PROOF,"w");assert(proof);
    fprintf(proof,"%g %g %d\n",g_tw,g_th,n);
    for(int i=0;i<n;i++)if(quads[i].u0==OVL_SOLID)
        fprintf(proof,"%g %g %g %g %g\n",quads[i].x0,quads[i].y0,quads[i].x1,quads[i].y1,quads[i].a);
    assert(!fclose(proof));
#endif
    input_reload_hint=0;input_settings=(struct touch_mobile_settings){1,1,0,0,60};
    input_ammo_available=0;touch_reset();
    puts("PASS: actual input knife/watch/range Fire, no ammo digits, no touch feedback dot, preserved reticle, slow nonzero Reload pulse/full active alpha and arrow at configured Reload position");
}
static void test_linear_look_layout(void) {
    float old_width=g_tw,old_height=g_th;
    unsigned old_comp_width=g_comp_w,old_comp_height=g_comp_h;
    float yaw[3],pitch[3];
    for(int mode=0;mode<3;mode++) {
        touch_reset();g_touch_cancel=0;g_touch_context=1;g_touch_weapon=2;g_menu_tick_ms=0;
        g_tw=g_comp_w=2400;g_th=g_comp_h=1080;input_assist=0;
        g_look_yaw=g_look_pitch=0;
        send_touch(0,184,1400,500);
        struct event events[10];
        for(int i=0;i<10;i++) {
            events[i]=(struct event){0,184,1,0,0,1400+20*(i+1),500+3*(i+1)};
            if(!mode){usleep(8000);send_touch(1,184,events[i].x,events[i].y);}
        }
        if(mode==1){struct {void* data;int num,max;} batch={events,10,10};touch_process(&batch);}
        if(mode==2)send_touch(1,184,1600,530);
        yaw[mode]=g_look_yaw;pitch[mode]=g_look_pitch;
        assert(fabsf(yaw[mode]+200*LOOK_SENS)<.00001f);
        assert(fabsf(pitch[mode]+30*LOOK_SENS)<.00001f);
        send_touch(2,184,1600,530);
    }
    for(int mode=1;mode<3;mode++)assert(fabsf(yaw[mode]-yaw[0])<.00001f&&fabsf(pitch[mode]-pitch[0])<.00001f);
    for(int size=75;size<=125;size+=25)for(int x=-80;x<=80;x+=80) {
        input_settings=(struct touch_mobile_settings){1,size/100.0f,x/1000.0f,0,60};
        g_touch_weapon=2;struct ctl firearm[C_N];
        for(int z=0;z<C_N;z++)firearm[z]=ctl_geometry(z);
        g_touch_weapon=1;
        for(int z=0;z<C_N;z++)if(ctl_visible(z,0)) {
            struct ctl knife=ctl_geometry(z);
            assert(knife.cx==firearm[z].cx&&knife.cy==firearm[z].cy&&knife.r==firearm[z].r);
        }
    }
    input_settings=(struct touch_mobile_settings){1,1,0,0,60};touch_reset();
    g_tw=old_width;g_th=old_height;g_comp_w=old_comp_width;g_comp_h=old_comp_height;
    puts("PASS: actual touch MOVE handling is distance-linear across timed/batched/coalesced events; knife/firearm layout is invariant at size/offset limits");
}
static void test_pause_aim_retirement(void) {
    unsigned char state[96];
    for(int toggle=0;toggle<2;toggle++) {
        touch_reset();g_touch_cancel=0;g_touch_context=1;g_touch_weapon=2;g_menu_tick_ms=0;
        input_aim_toggle=toggle;
        struct ctl aim=ctl_geometry(C_AIM);float x=aim.cx*g_tw,y=aim.cy*g_th;
        send_touch(0,185,x,y);fill_controller_state(state,3);assert(g_touch_aim);
        tap(C_PAUSE);assert(g_touch_pause&&!g_touch_aim&&!g_aim_latched);
        assert(vt_find(185)->role==RT_NONE);
        g_touch_context=2;g_menu_tick_ms=now_ms();fill_controller_state(state,3);
        send_touch(1,185,x+1,y);tap(C_BACK);
        assert(g_touch_back_request&&!g_touch_aim&&!g_aim_latched);
        g_touch_context=1;g_menu_tick_ms=0;fill_controller_state(state,3);
        send_touch(1,185,x+2,y);fill_controller_state(state,3);assert(!g_touch_aim);
        send_touch(2,185,x+2,y);send_touch(0,185,x,y);assert(g_touch_aim);
        send_touch(2,185,x,y);tap(C_PAUSE);assert(!g_touch_aim&&!g_aim_latched);
    }
    input_aim_toggle=0;touch_reset();g_menu_tick_ms=0;
    puts("PASS: pause/Back retires Hold and Tap ADS; stale MOVE cannot re-aim after resume; only a fresh ADS press reactivates aim");
}
static void test_requested_controls(void) {
    unsigned char state[96];
    touch_reset();g_touch_cancel=0;g_touch_context=1;g_touch_weapon=2;g_menu_tick_ms=0;
    float ax=g_ctl[C_AIM].cx*g_tw,ay=g_ctl[C_AIM].cy*g_th;
    input_aim_walk=1;
    send_touch(0,81,ax,ay);send_touch(0,82,300,700);send_touch(1,82,300,350);
    fill_controller_state(state,3);
    assert(g_touch_aim&&fabsf(hypotf(((float*)state)[8],((float*)state)[9])-.45f)<1e-5f);
    send_touch(2,81,ax,ay);fill_controller_state(state,3);
    assert(!g_touch_aim&&((float*)state)[9]>.99f);
    send_touch(0,81,ax,ay);tap(C_INVENTORY);assert(g_touch_inventory_request&&!g_touch_aim);
    send_touch(1,81,ax+1,ay);assert(!g_touch_aim);
    send_touch(2,81,ax,ay);send_touch(0,81,ax,ay);assert(g_touch_aim);
    tap(C_INTERACT);assert(g_touch_interact_request&&!g_touch_aim);
    input_aim_walk=0;touch_reset();g_touch_cancel=0;g_touch_context=1;g_touch_weapon=1;
    float fx=g_ctl[C_FIRE].cx*g_tw,fy=g_ctl[C_FIRE].cy*g_th,yaw=g_look_yaw;
    send_touch(0,83,fx,fy);send_touch(1,83,fx-70,fy-20);
    assert(g_look_yaw!=yaw&&g_anim_kind==2&&(g_buttons&OB_RINDEX));
    send_touch(2,83,fx-70,fy-20);assert(!(g_buttons&OB_RINDEX));
    // Toolbar A enters the title/menu immediately; only the exact native game Start component confirms.
    touch_reset();g_touch_cancel=0;g_touch_context=7;g_menu_tick_ms=now_ms();tap(C_CONFIRM);fill_controller_state(state,3);
    assert(((unsigned*)state)[1]&OB_A);
    touch_reset();g_touch_context=3;g_menu_tick_ms=0;
    assert(!ctl_visible(C_PAUSE,0));
    touch_reset();g_touch_cancel=0;g_touch_context=1;g_menu_tick_ms=0;
    input_hud_valid=1;input_hud=(struct mobile_hud_state){50,100,0,0};
    struct ovl_quad quads[48];int presentations=input_health_presentations;
    touch_overlay_quads(quads,0);assert(input_health_presentations==presentations);
    touch_overlay_quads(quads,48);assert(input_health_presentations==presentations+1);
    g_touch_context=3;touch_overlay_quads(quads,48);assert(input_health_presentations==presentations+1);
    g_touch_context=1;input_hud_valid=0;touch_overlay_quads(quads,48);assert(input_health_presentations==presentations+1);
    touch_reset();input_hud_valid=0;
    puts("PASS: requested controls: walking ADS cap/release, inventory/vault retires aim finger, held knife look, immediate toolbar A without game-Start confirmation, no cutscene menu");
}
int main(void) {
    test_linear_look_layout();
    test_pause_aim_retirement();
    const int native_health_colours[][3]={{0,1200,0},{400,1200,0},{401,1200,1},{800,1200,1},{801,1200,2},
        {33,100,0},{34,100,1},{66,100,1},{67,100,2},{66,101,1},{67,101,2},{0,2,0},{1,2,2}};
    for(unsigned i=0;i<sizeof native_health_colours/sizeof *native_health_colours;i++)
        assert(touch_health_colour(native_health_colours[i][0],native_health_colours[i][1])==native_health_colours[i][2]);
    test_mobile_view();
    test_mobile_hud();
    _Static_assert(sizeof(struct event) == 28, "UE touch event ABI");
    unsigned char state[96];
    g_touch_context = 1; g_touch_weapon = 2;
    for (int z = 0; z < C_N; z++) if (ctl_visible(z,0)) assert(ctl_at(g_ctl[z].cx*g_tw, g_ctl[z].cy*g_th, 0) == z);
    tap(C_KNIFE); assert(g_touch_weapon_request == 1 && !(g_grip_latched & OB_RHAND));
    usleep(270000); fill_controller_state(state, 1); assert(!(g_grip_latched & OB_RHAND));
    fill_controller_state(state, 3); assert(((float*)state)[7] == 0 && g_weapon_grip_at == -1);
    usleep(270000); fill_controller_state(state, 3); assert(((float*)state)[7] == 0 && !(g_grip_latched & OB_RHAND));
    g_touch_weapon_request = 0; // GameThread accepted the request for the active pawn.
    fill_controller_state(state, 3); assert(((float*)state)[7] == 0);
    usleep(270000); fill_controller_state(state, 3); assert(((float*)state)[7] == 1);
    tap(C_FIRE); assert(g_anim_kind == 2 && g_anim_len == 640);
    assert(knife_cut_offset(-1) == 0 && knife_cut_offset(0) == 0 && knife_cut_offset(640) == 0);
    assert(fabsf(knife_cut_offset(200) - 1.135f) < 0.00001f);
    assert(fabsf(knife_cut_offset(340) + 0.615f) < 0.00001f);
    assert(knife_cut_offset(400) == knife_cut_offset(440));
    float cut_speed = (knife_cut_offset(240) - knife_cut_offset(220)) / 0.020f;
    assert(cut_speed < -12.4f && cut_speed > -12.6f);
    tap(C_WEAPON); assert(g_touch_weapon_request == 2 && g_anim_kind == 0);
    tap(C_PAUSE); assert(g_touch_pause == 1);
    tap(C_INVENTORY); assert(g_touch_inventory_request == 1);
    tap(C_INTERACT); assert(g_touch_interact_request == 1);
    tap(C_RELOAD); assert(g_touch_reload_request == 1);
    float ax = g_ctl[C_AIM].cx*g_tw, ay = g_ctl[C_AIM].cy*g_th;
    send_touch(0, 6, ax, ay); fill_controller_state(state,3);
    assert(g_touch_aim == 1 && ((float*)state)[4] == 0 && ((float*)state)[5] == 0);
    send_touch(2, 6, ax, ay); assert(g_touch_aim == 0);
    g_touch_context = 5; tap(C_PHONE); assert(g_phone_request == 1 && !(g_buttons & OB_A));
    assert(ctl_at(g_ctl[C_KNIFE].cx*g_tw, g_ctl[C_KNIFE].cy*g_th, menu_mode()) == -1);
    int pending = g_touch_weapon_request;
    float phone_yaw = g_look_yaw;
    send_touch(0, 5, 350, 780); send_touch(1, 5, 350, 500); send_touch(2, 5, 350, 500);
    assert(g_lstick_y == 0 && g_look_yaw == phone_yaw && g_ptr_down == 0);
    assert(g_touch_weapon_request == pending);
    touch_reset(); g_touch_context=2; g_menu_tick_ms=now_ms();
    tap(C_BACK); fill_controller_state(state,3);
    assert(g_touch_back_request==1 && !(((unsigned*)state)[1]&OB_B));
    touch_reset(); assert(!g_touch_back_request);
    g_touch_context=7; g_menu_tick_ms=now_ms(); tap(C_BACK); fill_controller_state(state,3);
    assert(!g_touch_back_request && (((unsigned*)state)[1]&OB_B));
    touch_reset(); g_menu_tick_ms=0;
    g_touch_context = 1;
    send_touch(0, 0, 350, 780); send_touch(1, 0, 350, 500);
    send_touch(0, 1, 1500, 650); send_touch(1, 1, 1540, 660);
    fill_controller_state(state, 3); assert(((float*)state)[9] > 0.99f && g_look_yaw < 0);
    send_touch(2, 0, 350, 500); send_touch(2, 1, 1540, 660);
    assert(g_lstick_y == 0 && g_ptr_down == 0);
    // A camera tap or a stationary movement thumb must not discharge a held weapon.
    send_touch(0, 1, 1450, 600); send_touch(2, 1, 1450, 600);
    assert(g_tap_until_ms == 0);
    send_touch(0, 0, 350, 780); send_touch(2, 0, 350, 780);
    assert(g_tap_until_ms == 0);
    float yaw = g_look_yaw;
    g_touch_context = 2; g_menu_tick_ms = now_ms();
    send_touch(0, 3, 1400, 600); send_touch(1, 3, 1430, 620); send_touch(2, 3, 1430, 620);
    assert(g_look_yaw == yaw && g_ptr_down == 0);
    assert(!ctl_visible(C_CONFIRM,1));
    g_touch_context=7;
    assert(ctl_at(g_ctl[C_CONFIRM].cx*g_tw,g_ctl[C_CONFIRM].cy*g_th,1) == C_CONFIRM);
    tap(C_CONFIRM); fill_controller_state(state,3); assert(((unsigned*)state)[1] & OB_A);
    input_toolbar_mask=5;
    {
        struct ovl_quad q[64];int n=touch_overlay_quads(q,64),skips=0;
        for(int i=0;i<n;i++)if(q[i].u0==OVL_SKIP)skips++;
        assert(skips==1 && ctl_visible(C_CONFIRM,1) && !ctl_visible(C_BACK,1));
        tap(C_CONFIRM);fill_controller_state(state,3);assert(((unsigned*)state)[1]&OB_A);
        input_toolbar_mask=0;assert(!ctl_visible(C_CONFIRM,1));
        input_toolbar_mask=1;n=touch_overlay_quads(q,64);
        for(int i=0;i<n;i++)assert(q[i].u0!=OVL_SKIP);
    }
    input_toolbar_mask=3;
    send_touch(0, 4, NAN, 600); assert(vt_find(4) == 0);
    fill_controller_state(state, 0); assert(((unsigned*)state)[0] == 0);
    g_cmd_rstick[1] = 1; g_cmd_rstick_until = now_ms() + 500;
    fill_controller_state(state, 3); assert(((float*)state)[11] == 1);
    touch_reset(); fill_controller_state(state, 3);
    assert(g_buttons == 0 && g_grip_latched == 0 && g_lstick_y == 0 && g_ptr_down == 0 && g_anim_kind == 0);
    assert(g_cmd_rstick_until == 0 && ((float*)state)[11] == 0 && g_phone_request == 0 && g_touch_cancel == 3);
    assert(g_touch_inventory_request == 0 && g_touch_interact_request == 0 && g_touch_aim == 0);
    g_menu_tick_ms = 0; g_touch_context = 1; g_touch_weapon = 2;
    for (int width = 2167; width <= 2400; width += 233) {
        g_tw = (float)width;
        for (int z = 0; z < C_N; z++) if (ctl_visible(z,0)) {
            float x = g_ctl[z].cx*g_tw, y = g_ctl[z].cy*g_th, radius = g_ctl[z].r*g_th*1.15f;
            assert(ctl_at(x,y,0) == z);
            assert(x-radius>0 && x+radius<g_tw && y-radius>0 && y+radius<g_th);
            for (int k = z+1; k < C_N; k++) if (ctl_visible(k,0)) {
                float dx = (g_ctl[k].cx-g_ctl[z].cx)*g_tw, dy = (g_ctl[k].cy-g_ctl[z].cy)*g_th;
                float both = (g_ctl[k].r+g_ctl[z].r)*g_th*1.15f;
                assert(dx*dx+dy*dy > both*both);
            }
        }
    }
    // Opening a menu while a thumb is down must not keep movement/fire/aim active.
    float fx = g_ctl[C_FIRE].cx*g_tw, fy = g_ctl[C_FIRE].cy*g_th;
    send_touch(0, 7, fx, fy); send_touch(0, 8, 350, 780); send_touch(1, 8, 350, 500);
    fill_controller_state(state,3); assert(((float*)state)[5] == 1 && ((float*)state)[9] > .99f);
    g_touch_context = 5; fill_controller_state(state,3);
    assert(((float*)state)[5] == 0 && ((float*)state)[9] == 0 && !g_touch_aim);
    touch_reset(); g_touch_context = 3; yaw = g_look_yaw;
    send_touch(0, 9, 1500, 600); send_touch(1, 9, 1540, 630); send_touch(2, 9, 1540, 630);
    assert(g_look_yaw == yaw && g_lstick_y == 0);
    touch_reset(); g_touch_context=1; g_touch_weapon=2;
    long fire_now=now_ms();
    g_cmd_until[26]=fire_now+5000;
    fill_controller_state(state,3);
    assert(((float*)state)[5]==1 && !touch_reload_user_fire(fire_now));
    g_cmd_until[26]=0;
    send_touch(0,61,g_ctl[C_FIRE].cx*g_tw,g_ctl[C_FIRE].cy*g_th);
    assert(touch_reload_user_fire(now_ms()));
    send_touch(2,61,g_ctl[C_FIRE].cx*g_tw,g_ctl[C_FIRE].cy*g_th);
    assert(touch_reload_user_fire(now_ms()) && !touch_reload_user_fire(now_ms()+200));
    touch_reset();
    send_touch(0,62,g_ctl[C_AIM].cx*g_tw,g_ctl[C_AIM].cy*g_th);
    assert(!touch_reload_user_fire(now_ms()));
    send_touch(0,63,g_ctl[C_FIRE].cx*g_tw,g_ctl[C_FIRE].cy*g_th);
    assert(touch_reload_user_fire(now_ms()));
    touch_reset(); assert(!touch_reload_user_fire(now_ms()));
    test_context_lifetime();
    test_pump_controller_input();
    test_new_game_pawn_lifetime();
    test_menu_pointer_drag();
    test_settings_and_favorites();
    test_quickselect_resume_request();
    test_grenade_input();
    test_knife_watch_holds();
    test_requested_controls();
    test_mobile_repair_input();
    for(int context=1;context<=7;context++){
        touch_reset();g_touch_cancel=0;g_touch_context=context;g_menu_tick_ms=now_ms();g_input_context=-1;
        assert(touch_mobile_back());fill_controller_state(state,3);
        assert(g_touch_pause==(context==1 || context==3));
        assert(g_touch_back_request==(context==2));
        assert((((unsigned*)state)[1]&OB_B)!=0==(context==4 || context==7));
        assert(!g_phone_request && !g_touch_weapon_request && !g_touch_interact_request);
    }
    touch_reset();g_touch_context=0;assert(!touch_mobile_back());
    g_touch_context=8;assert(!touch_mobile_back());touch_reset();
    puts("touch self-check PASS: semantic hitboxes at 2167/2400, weapon release/regrip, knife, native action queues, hold aim, multitouch, contextual HUD, context return without FIRE/move/aim resurrection, menu RT cancellation, new history-window tap without gameplay RT, orphan MOVE rejection and focus reset; new game pawn pointer/full8-byteName reset, initial/same/null preservation, menu/cinema/phone return, neutral both grips/triggers/sticks/all queues and stale-MOVE refusal; menu120ms hold/drag/immediateUP, fasttap120+150, singleowner/duplicateDOWN, context/focus/newPawn cancellation, GT-only nativegetter and freshGame1 RT refusal");
    return 0;
}
