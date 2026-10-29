#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int on_gt=1,kind=1,menu,paused,valid=1,typed=1,scope,option=90,zoom=1;
static long now=1000,g_cutscene_skippable_ms,g_cutscene_skip_request;
static float g_fov_h_deg=90;
static int g_touch_context=1,g_touch_aim,g_touch_weapon=2;
static union {void* alignment;unsigned char bytes[0x1660];} pawn;
static unsigned char pawn_type[0x60],native_settings[0xb8];
static const void* fake_settings(void){return native_settings;}
static const void* (*qs_settings)(void)=fake_settings;
static unsigned char camera[16],other_camera[16];
static unsigned fake_type(void){return (unsigned)kind;}
static void* fake_active(void){return pawn.bytes;}
static unsigned (*cinema_type)(void)=fake_type;
static void* (*cinema_active)(void)=fake_active;
static void (*cinema_view_extra)(void*,void*);
static int cinema_on_game_thread(void){return on_gt;}
static int menu_mode(void){return menu;}
static int fake_paused(void){return paused;}
static int fake_valid(void){return valid;}
static int (*rl_paused)(void)=fake_paused,(*rl_player_valid)(void)=fake_valid;
static int interact_is_a(void* object,void* cls){assert(object==pawn.bytes&&cls==(void*)1);return typed;}
static void* fake_class(void){return (void*)1;}
static int fake_scope(const void* object){assert(object==pawn.bytes);return scope;}
static int mobile_prefs_fov(void){return option;}
static int mobile_prefs_ads_zoom(void){return zoom;}
static long ptr_now_ms(void){return now;}
static int skip_allowed,skip_calls,movie,movie_opening,movie_completed,movie_finish_ok=1,intro_skip,tap_during_can;
static int fake_can_skip(void){
    if(tap_during_can){now++;g_cutscene_skip_request=now;tap_during_can=0;}
    return skip_allowed;
}
static void fake_skip(void){skip_calls++;}
static void* cinema_movie_probe(void* active,int opening_only){
    assert(active==pawn.bytes);return movie && (!opening_only || movie_opening) ? camera : 0;
}
static int cinema_movie_complete(void* theatre){assert(theatre==camera);movie_completed++;return movie_finish_ok;}
static int mobile_prefs_intro_skip(void){return intro_skip;}
#define LOGF(...) ((void)0)
typedef struct {float x,y,z,w;} qf_t;
static qf_t q_mul(qf_t a,qf_t b){return (qf_t){a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
    a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
    a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
#include "../re4fix_weapon_pose.h"
#include "../re4fix_view_extra.h"
static struct vx_velocity native_velocity;
static struct vx_velocity get_velocity(const void* p){assert(p==pawn.bytes);return native_velocity;}

static void fill_view(unsigned char* view,float fov){memset(view,0xa5,64);memcpy(view+0x18,&fov,4);}
static float read_fov(const unsigned char* view){float value;memcpy(&value,view+0x18,4);return value;}
static void untouched(unsigned char* view){
    unsigned char expected[64];memcpy(expected,view,64);
    touch_gameplay_fov_view(camera,view);assert(!memcmp(view,expected,64));
}
static void stable_fov(float authored,float expected){
    unsigned char view[64],unchanged[64];fill_view(unchanged,authored);
    for(int i=0;i<90;i++){
        fill_view(view,authored);now+=16;touch_gameplay_fov_view(camera,view);
        memcpy(view+0x18,&authored,4);assert(!memcmp(view,unchanged,64));
    }
    fill_view(view,authored);now+=16;touch_gameplay_fov_view(camera,view);
    assert(fabsf(read_fov(view)-expected)<.0001f&&fabsf(g_fov_h_deg-expected)<.0001f);
}
int main(void){
    *(void**)(pawn.bytes+0x10)=pawn_type;*(int*)(pawn_type+0x58)=0x1650;
    *(void**)(pawn.bytes+0x2d8)=camera;vx_bio4_class=fake_class;vx_scope=fake_scope;
    unsigned char view[64];fill_view(view,90);
    stable_fov(90,90);option=110;stable_fov(90,110);option=70;stable_fov(90,70);
    option=90;g_touch_aim=1;stable_fov(90,75.6f);scope=1;stable_fov(45,45);
    scope=0;zoom=0;stable_fov(90,90);g_touch_aim=0;zoom=1;option=110;
    for(int context=0;context<=8;context++)if(context!=1){g_touch_context=context;untouched(view);assert(vx_fov_scale==1&&vx_fov_ms==0);}
    g_touch_context=1;
    menu=1;fill_view(view,82);untouched(view);assert(g_fov_h_deg==82);menu=0;
    paused=1;untouched(view);paused=0;valid=0;untouched(view);valid=1;
    rl_paused=0;untouched(view);rl_paused=fake_paused;rl_player_valid=0;untouched(view);rl_player_valid=fake_valid;
    for(kind=0;kind<=8;kind++)if(kind!=1)untouched(view);kind=1;
    on_gt=0;untouched(view);on_gt=1;typed=0;untouched(view);typed=1;
    *(void**)(pawn.bytes+0x2d8)=other_camera;untouched(view);*(void**)(pawn.bytes+0x2d8)=camera;
    const float invalid[]={NAN,INFINITY,19,151};
    for(unsigned i=0;i<sizeof invalid/sizeof*invalid;i++){fill_view(view,invalid[i]);untouched(view);}
    fill_view(view,90);unsigned char expected[64];memcpy(expected,view,64);
    touch_gameplay_fov_view(0,view);touch_gameplay_fov_view(camera,0);assert(!memcmp(view,expected,64));
    cinema_type=0;untouched(view);cinema_type=fake_type;cinema_active=0;untouched(view);cinema_active=fake_active;
    stable_fov(90,110);menu=1;fill_view(view,90);untouched(view);menu=0;
    now+=10000;touch_gameplay_fov_view(camera,view);
    assert(read_fov(view)>90&&read_fov(view)<95);
    option=90;vx_fov_scale=1;native_settings[0x3a]=1;vx_velocity=get_velocity;
    native_velocity=(struct vx_velocity){180,0,0};float biggest=0;
    for(int i=0;i<150;i++){
        memset(view,0,64);float authored=90,position[3]={10,20,160},yaw=40;
        memcpy(view,position,12);memcpy(view+0x10,&yaw,4);memcpy(view+0x18,&authored,4);
        now+=16;touch_gameplay_fov_view(camera,view);float actual[3];memcpy(actual,view,12);
        float vertical=fabsf(actual[2]-position[2]);biggest=fmaxf(biggest,vertical);
        assert(vertical<=.4501f && hypotf(actual[0]-10,actual[1]-20)<=.3501f);
    }
    assert(biggest>.4f);
    // Descending and climbing preserve the same cadence as flat travel at the same horizontal speed.
    for(int direction=-1;direction<=1;direction+=2){
        memset(&vx_bob,0,sizeof vx_bob);native_velocity=(struct vx_velocity){180,0,200.0f*direction};biggest=0;
        for(int i=0;i<150;i++){
            memset(view,0,64);float fov=90;memcpy(view+0x18,&fov,4);
            now+=16;touch_gameplay_fov_view(camera,view);
            float z;memcpy(&z,view+8,4);biggest=fmaxf(biggest,fabsf(z));
        }
        assert(biggest>.4f&&biggest<=.4501f);
    }
    native_settings[0x3a]=0;memset(view,0,64);float authored=90;memcpy(view+0x18,&authored,4);
    now+=16;touch_gameplay_fov_view(camera,view);assert(!vx_bob.time&&!memcmp(view,(float[3]){0,0,0},12));
    native_settings[0x3a]=1;native_velocity=(struct vx_velocity){0,0,0};
    for(int i=0;i<100;i++){now+=16;touch_gameplay_fov_view(camera,view);assert(!memcmp(view,(float[3]){0,0,0},12));}
    native_velocity=(struct vx_velocity){NAN,0,0};now+=16;touch_gameplay_fov_view(camera,view);assert(!vx_bob.time);
    native_velocity=(struct vx_velocity){180,0,NAN};now+=16;touch_gameplay_fov_view(camera,view);assert(!vx_bob.time);
    native_settings[0x3a]=0;
    puts("PASS: actual gameplay camera bob from native movement speed, 150 frames at <=0.45cm, zero speed/off/nonfinite reset; other contexts retain authored camera");
    vx_can_skip=fake_can_skip;vx_skip=fake_skip;kind=g_touch_context=3;now=20000;
    skip_allowed=1;touch_cutscene_skip_tick();assert(g_cutscene_skippable_ms==now && !skip_calls);
    skip_allowed=0;now+=16;touch_cutscene_skip_tick();assert(!g_cutscene_skippable_ms);
    g_cutscene_skip_request=now;touch_cutscene_skip_tick();assert(!g_cutscene_skip_request && !skip_calls);
    skip_allowed=1;g_cutscene_skip_request=now-600;touch_cutscene_skip_tick();assert(!skip_calls);
    g_cutscene_skip_request=now+1;touch_cutscene_skip_tick();assert(!skip_calls);
    g_cutscene_skip_request=now-599;touch_cutscene_skip_tick();assert(skip_calls==1 && !g_cutscene_skip_request);
    touch_cutscene_skip_tick();assert(skip_calls==1);
    kind=1;g_cutscene_skip_request=now;touch_cutscene_skip_tick();assert(!g_cutscene_skippable_ms && !g_cutscene_skip_request);
    kind=3;on_gt=0;g_cutscene_skip_request=now;touch_cutscene_skip_tick();assert(g_cutscene_skip_request==now && skip_calls==1);on_gt=1;
    movie=1;skip_allowed=0;touch_cutscene_skip_tick();assert(movie_completed==1 && skip_calls==1);
    movie_opening=1;touch_cutscene_skip_tick();assert(movie_completed==1);
    intro_skip=1;touch_cutscene_skip_tick();assert(movie_completed==2 && vx_intro_done);
    touch_cutscene_skip_tick();assert(movie_completed==2);
    movie_opening=0;touch_cutscene_skip_tick();assert(vx_intro_done && !vx_intro_active && movie_completed==2);
    movie_opening=1;intro_skip=0;touch_cutscene_skip_tick();assert(movie_completed==3&&vx_intro_repeat);
    kind=1;touch_cutscene_skip_tick();assert(vx_intro_done&&!vx_intro_active);
    kind=3;intro_skip=0;g_cutscene_skip_request=now;touch_cutscene_skip_tick();assert(movie_completed==4);
    movie_finish_ok=0;intro_skip=1;vx_intro_done=0;touch_cutscene_skip_tick();assert(movie_completed==5 && !vx_intro_done);
    movie=0;touch_cutscene_skip_tick();assert(!g_cutscene_skippable_ms && skip_calls==1);
    skip_allowed=1;intro_skip=0;long published=now;tap_during_can=1;
    touch_cutscene_skip_tick();assert(now==published+1 && g_cutscene_skippable_ms==published && skip_calls==2 && !g_cutscene_skip_request);
    g_cutscene_skip_request=now-600;touch_cutscene_skip_tick();assert(skip_calls==2 && !g_cutscene_skip_request);
    g_cutscene_skip_request=now+1;touch_cutscene_skip_tick();assert(skip_calls==2 && !g_cutscene_skip_request);
    g_cutscene_skip_request=now-599;touch_cutscene_skip_tick();assert(skip_calls==3 && !g_cutscene_skip_request);
    puts("PASS: tap published1ms later during native-can probe uses post-consume clock; skippable publication time unchanged, age600/future decline and age599 acceptance retained");
    puts("PASS: real scene skip ABI(void), invalid/stale/consumed request and scene-exit cleanup, false skippable freshness; FMV authored completion; first opening respects skip option; later openings automatically complete through original authored graph");
    puts("PASS: production gameplay FOV 70/90/110, ADS and scopes; exact native view preservation for menus, pauses, cutscenes, context/owner/thread/validity failures; easing reset on menu exit");
    return 0;
}
