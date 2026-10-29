#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
_Alignas(16) static unsigned char hmd[0x278], settings[0x40], wrapper[0x210];
_Alignas(16) static unsigned char engine[0xb10], vtable[0x140], secondary[0x140], wrong_hmd[0x250];
_Alignas(16) static unsigned char pawn[0x2e0],camera[0x208],pawn_class[0x60],camera_class[0x60],frame[0x20];
static void* pawn_chain[1], *camera_chain[1], *active_pawn;
static int pose_calls,raw_calls;
static char last_log[768],last_pose_log[768];
static void* g_hmd_instance;
static volatile float g_look_yaw,g_look_pitch;
static void* engine_pointer;
static const void* wrapper_pointer;
static int enforce_calls,enforced_calls,allowed_calls,init_calls,logs;
static int native_enforced,native_initialized=1;
static void probe_log(const char* format,...) {
    va_list args;va_start(args,format);vsnprintf(last_log,sizeof last_log,format,args);va_end(args);++logs;
    if (!strncmp(format,"TRACKPOSE",9)) memcpy(last_pose_log,last_log,sizeof last_log);
}
#define LOGF(...) probe_log(__VA_ARGS__)
static int is_enforced(const void* p) { assert(p==hmd); ++enforced_calls; return native_enforced; }
static int is_allowed(const void* p) { assert(p==hmd); ++allowed_calls; return native_enforced; }
static int is_initialized(void) { ++init_calls; return native_initialized; }
static void enforce(void* p,int enable) {
    assert(p==hmd && enable==1 && native_initialized==1);
    assert(*(const void* const*)((char*)p+0x248)==settings);
    ++enforce_calls; native_enforced=1;
}
#include "../re4fix_tracking.h"
static void* get_active_pawn(void) { return active_pawn; }
static void* get_pawn_class(void) { return pawn_class; }
static void* get_camera_class(void) { return camera_class; }
static int get_pose(void* self,int device,struct tracking_quat* q,struct tracking_vec* p) {
    assert(self==hmd && device==0 && q && p);++pose_calls;
    if (!*(void* const*)(hmd+0x270)) { *q=(struct tracking_quat){0,0,0,1};*p=(struct tracking_vec){0,0,0};return 0; }
    *q=(struct tracking_quat){.1f,.2f,.3f,.9f};*p=(struct tracking_vec){4,5,6};return 1;
}
int h5_ovrp_GetNodePoseState3(long step,long frame_number,long node,long out) {
    (void)step;(void)frame_number;(void)node;(void)out;return -1000;
}
static int raw_pose_wide(intptr_t step,intptr_t frame_number,intptr_t node,intptr_t out) {
    assert(step==-1 && frame_number==123 && node==9 && out && !(out&7));++raw_calls;
    struct tracking_quat q={.1f,.2f,.3f,.9f};struct tracking_vec p={4,5,6};
    memcpy((void*)out,&q,sizeof q);memcpy((char*)out+16,&p,sizeof p);return 0;
}
static void store_pointer(unsigned char* target,const void* value) { memcpy(target,&value,sizeof value); }
static void setup(void) {
    memset(hmd,0,sizeof hmd);memset(settings,0,sizeof settings);memset(wrapper,0,sizeof wrapper);
    memset(engine,0,sizeof engine);memset(vtable,0,sizeof vtable);memset(secondary,0,sizeof secondary);
    memset(wrong_hmd,0,sizeof wrong_hmd);
    enforce_calls=enforced_calls=allowed_calls=init_calls=logs=native_enforced=0;
    native_initialized=1;g_hmd_instance=0;engine_pointer=0;wrapper_pointer=wrapper;
    tracking_logs=tracking_state_logs=tracking_capture_logs=0;
    tracking_enforce=0;tracking_allowed=is_allowed;tracking_enforced=is_enforced;
    tracking_pose=0;tracking_active_pawn=0;tracking_pawn_class=0;tracking_camera_class=0;
    tracking_diag_samples=0;g_tracking_diag_request=0;pose_calls=raw_calls=0;active_pawn=0;
    tracking_wrapper_slot=&wrapper_pointer;tracking_engine_slot=&engine_pointer;tracking_hmd_vtable=vtable;
    int (*initialized)(void)=is_initialized;
    int (*allowed)(const void*)=is_allowed, (*enforced)(const void*)=is_enforced;
    memcpy(wrapper+0x18,&initialized,sizeof initialized);
    memcpy(vtable+0x128,&allowed,sizeof allowed);memcpy(vtable+0x130,&enforced,sizeof enforced);
    store_pointer(hmd,vtable);store_pointer(wrong_hmd,secondary);
}
int main(void) {
    setup();touch_tracking_tick();assert(!logs && !enforce_calls);
    tracking_enforce=enforce;
    touch_tracking_tick();assert(!g_hmd_instance && !enforced_calls && logs==1);
    engine_pointer=engine;touch_tracking_tick();assert(!g_hmd_instance && !enforced_calls && logs==2);
    store_pointer(engine+0xb00,wrong_hmd);
    touch_tracking_tick();assert(!g_hmd_instance && !enforced_calls && logs==3);
    store_pointer(engine+0xb00,hmd);
    int (*allowed)(const void*)=is_allowed, (*enforced)(const void*)=is_enforced;
    memset(vtable+0x128,0,sizeof allowed);
    touch_tracking_tick();assert(!g_hmd_instance && !enforced_calls && logs==4);
    memcpy(vtable+0x128,&allowed,sizeof allowed);memset(vtable+0x130,0,sizeof enforced);
    touch_tracking_tick();assert(!g_hmd_instance && !enforced_calls && logs==5);
    memcpy(vtable+0x130,&enforced,sizeof enforced);
    unsigned char before_engine[sizeof engine],before_hmd[sizeof hmd],before_vtable[sizeof vtable];
    memcpy(before_engine,engine,sizeof engine);memcpy(before_hmd,hmd,sizeof hmd);memcpy(before_vtable,vtable,sizeof vtable);
    touch_tracking_tick();assert(g_hmd_instance==hmd && !enforced_calls && !enforce_calls);
    assert(tracking_capture_logs==6 && tracking_logs==1);
    assert(!memcmp(before_engine,engine,sizeof engine) && !memcmp(before_hmd,hmd,sizeof hmd)
           && !memcmp(before_vtable,vtable,sizeof vtable));
    store_pointer(hmd+0x248,settings);
    wrapper_pointer=0;touch_tracking_tick();assert(!init_calls && !enforce_calls);
    wrapper_pointer=wrapper;memset(wrapper+0x18,0,sizeof(void*));
    touch_tracking_tick();assert(!init_calls && !enforce_calls);
    int (*initialized)(void)=is_initialized;
    memcpy(wrapper+0x18,&initialized,sizeof initialized);
    native_initialized=0;touch_tracking_tick();assert(init_calls==1 && !enforce_calls && !allowed_calls);
    native_initialized=2;touch_tracking_tick();assert(init_calls==2 && !enforce_calls && !allowed_calls);
    native_initialized=1;
    unsigned char before_settings[sizeof settings],before_wrapper[sizeof wrapper];
    memcpy(before_hmd,hmd,sizeof hmd);memcpy(before_settings,settings,sizeof settings);memcpy(before_wrapper,wrapper,sizeof wrapper);
    touch_tracking_tick();assert(enforce_calls==1 && allowed_calls==2 && native_enforced==1 && tracking_state_logs==1);
    assert(!memcmp(before_hmd,hmd,sizeof hmd) && !memcmp(before_settings,settings,sizeof settings)
           && !memcmp(before_wrapper,wrapper,sizeof wrapper));
    int before_init=init_calls,before_logs=logs;
    for(int i=0;i<30;i++) touch_tracking_tick();
    assert(enforce_calls==1 && allowed_calls==2 && init_calls==before_init && logs==before_logs);
    native_enforced=0;native_initialized=0;
    for(int i=0;i<30;i++) touch_tracking_tick();
    assert(enforce_calls==1 && tracking_logs==8 && tracking_state_logs==1);
    setup();tracking_enforce=enforce;
    for(int i=0;i<30;i++) touch_tracking_tick();
    assert(tracking_capture_logs==8 && !g_hmd_instance && !enforced_calls && logs==8);
    engine_pointer=engine;store_pointer(engine+0xb00,hmd);store_pointer(hmd+0x248,settings);
    touch_tracking_tick();assert(g_hmd_instance==hmd && enforce_calls==1 && tracking_state_logs==1 && allowed_calls==2);
    assert(logs==9);
    setup();tracking_enforce=enforce;g_hmd_instance=wrong_hmd;
    engine_pointer=engine;store_pointer(engine+0xb00,hmd);store_pointer(hmd+0x248,settings);
    touch_tracking_tick();assert(g_hmd_instance==wrong_hmd && !enforced_calls && !enforce_calls && logs==1);
    setup();tracking_enforce=enforce;engine_pointer=engine;
    store_pointer(engine+0xb00,hmd);store_pointer(hmd+0x248,settings);native_enforced=1;
    touch_tracking_tick();assert(g_hmd_instance==hmd && !enforce_calls && !init_calls && allowed_calls==1 && tracking_state_logs==1);
    before_logs=logs;for(int i=0;i<30;i++) touch_tracking_tick();
    assert(logs==before_logs && allowed_calls==1 && !enforce_calls && !init_calls);
    // Actual native-output ABI and bounded read-only camera/pose diagnostics.
    setup();tracking_enforce=enforce;engine_pointer=engine;
    store_pointer(engine+0xb00,hmd);store_pointer(hmd+0x248,settings);
    memset(pawn,0,sizeof pawn);memset(camera,0,sizeof camera);
    memset(pawn_class,0,sizeof pawn_class);memset(camera_class,0,sizeof camera_class);
    pawn_chain[0]=pawn_class+0x30;camera_chain[0]=camera_class+0x30;
    store_pointer(pawn_class+0x30,pawn_chain);store_pointer(camera_class+0x30,camera_chain);
    *(int*)(pawn_class+0x58)=sizeof pawn;*(int*)(camera_class+0x58)=sizeof camera;
    store_pointer(pawn+0x10,pawn_class);store_pointer(camera+0x10,camera_class);
    store_pointer(pawn+0x2d8,camera);camera[0x204]=4;active_pawn=pawn;
    tracking_pose=get_pose;tracking_active_pawn=get_active_pawn;
    tracking_pawn_class=get_pawn_class;tracking_camera_class=get_camera_class;
    unsigned char before_pawn[sizeof pawn],before_camera[sizeof camera];
    memcpy(before_pawn,pawn,sizeof pawn);memcpy(before_camera,camera,sizeof camera);
    touch_tracking_tick();assert(pose_calls==1 && tracking_diag_samples==1);
    assert(strstr(last_pose_log,"result=0") && strstr(last_pose_log,"q=(0.00000,0.00000,0.00000,1.00000)")
           && strstr(last_pose_log,"valid=1 lockHMD=1") && !raw_calls);
    touch_tracking_tick();assert(pose_calls==2 && tracking_diag_samples==2);
    before_logs=logs;for(int i=0;i<30;i++)touch_tracking_tick();assert(pose_calls==2 && logs==before_logs);
    *(unsigned*)(frame+0x10)=123;*(float*)(frame+0x14)=100;
    *(unsigned*)(settings+0x18)=16;*(float*)(settings+0x3c)=1;*(float*)(settings+0x24)=1.6f;
    int (*raw)(intptr_t,intptr_t,intptr_t,intptr_t)=raw_pose_wide;
    memcpy(wrapper+0x208,&raw,sizeof raw);
    store_pointer(hmd+0x270,frame);g_tracking_diag_request=1;
    touch_tracking_tick();assert(pose_calls==3 && !g_tracking_diag_request && tracking_diag_samples==2);
    assert(strstr(last_pose_log,"result=1") && strstr(last_pose_log,"p=(4.000,5.000,6.000)"));
    assert(raw_calls==1 && strstr(last_log,"flags=00000010 frameNumber=123 scale=100.000")
           && strstr(last_log,"same=0 rawResult=0") && strstr(last_log,"rawP=(4.000,5.000,6.000)"));
    assert(!memcmp(before_pawn,pawn,sizeof pawn) && !memcmp(before_camera,camera,sizeof camera));
    active_pawn=0;g_tracking_diag_request=1;touch_tracking_tick();
    assert(pose_calls==4 && strstr(last_pose_log,"valid=0 lockHMD=-1"));
    active_pawn=pawn;*(int*)(pawn_class+0x58)=0x2d8;g_tracking_diag_request=1;touch_tracking_tick();
    assert(pose_calls==5 && strstr(last_pose_log,"valid=0 lockHMD=-1"));
    *(int*)(pawn_class+0x58)=sizeof pawn;camera_chain[0]=pawn_class+0x30;
    g_tracking_diag_request=1;touch_tracking_tick();assert(pose_calls==6 && strstr(last_pose_log,"valid=0 lockHMD=-1"));
    camera_chain[0]=camera_class+0x30;*(int*)(camera_class+0x58)=0x204;
    g_tracking_diag_request=1;touch_tracking_tick();assert(pose_calls==7 && strstr(last_pose_log,"valid=0 lockHMD=-1"));
    *(int*)(camera_class+0x58)=sizeof camera;camera[0x204]=0;native_initialized=0;
    int before_raw=raw_calls;
    g_tracking_diag_request=1;touch_tracking_tick();assert(pose_calls==7 && raw_calls==before_raw && strstr(last_pose_log,"result=-1")
        && strstr(last_pose_log,"valid=1 lockHMD=0"));
    puts("PASS: typed XR capture, primary vtable/getter guards, cached rejection, Settings/wrapper/init guards, native setter once, independent bounded logs and no raw object writes");
    puts("PASS: two bounded native pose snapshots plus explicit one-shot, Frame NULL/present, quaternion/vector output ABI, camera ancestry/size/Lock flag guards and immutable pawn/camera");
    puts("PASS: conversion Settings fields/frame scale, raw OVR208 pointer identity/stack outputs and no raw call without frame/initialized");
    return 0;
}
