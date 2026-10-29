// UI thread publishes one bounded atomic snapshot; original engine FPS cvar is applied only on GT.
#ifndef RE4FIX_MOBILE_SETTINGS_H
#define RE4FIX_MOBILE_SETTINGS_H
#include <stdint.h>
struct touch_mobile_settings {float sensitivity,size,x,y,fps;};
#define TOUCH_SETTINGS_PACK(s,z,x,y,f) ((uint64_t)(s)|((uint64_t)(z)<<11)|((uint64_t)((x)+80)<<22)|((uint64_t)((y)+80)<<30)|((uint64_t)(f)<<38))
static uint64_t touch_settings_value=TOUCH_SETTINGS_PACK(1000,1000,0,0,60000);
static volatile int g_touch_density_milli=2750;
static void* (*mobile_fps_find)(void*,const unsigned short*,int);
static float (*mobile_fps_get)(const void*);
static void (*mobile_fps_set)(void*,const unsigned short*,unsigned);
static uintptr_t mobile_fps_base;
static int mobile_fps_applied;
static unsigned mobile_fps_logs;
static int touch_mobile_settings_apply(int sensitivity,int size,int x,int y,int fps) {
    if(sensitivity<500 || sensitivity>2000 || size<750 || size>1250
        || x<-80 || x>80 || y<-80 || y>80 || fps<24000 || fps>240000)return 0;
    __atomic_store_n(&touch_settings_value,TOUCH_SETTINGS_PACK(sensitivity,size,x,y,fps),__ATOMIC_RELEASE);
    return 1;
}
static struct touch_mobile_settings touch_mobile_settings_snapshot(void) {
    uint64_t p=__atomic_load_n(&touch_settings_value,__ATOMIC_ACQUIRE);
    struct touch_mobile_settings value={
        (float)(p&2047)/1000.0f,(float)((p>>11)&2047)/1000.0f,
        (float)((int)((p>>22)&255)-80)/1000.0f,(float)((int)((p>>30)&255)-80)/1000.0f,
        (float)((p>>38)&262143)/1000.0f};
    return value;
}
static void install_touch_mobile_settings(uintptr_t base) {
    mobile_fps_base=0;mobile_fps_applied=0;
    if(!base)return;
    static const struct {unsigned long addr;unsigned first[2];} code[]={
        {0x65e9ba0,{0xf81e0ff3,0xa9017bfd}},
        {0x65e7b0c,{0xf81e0ff3,0xa9017bfd}},
        {0x65eec38,{0xf81d0ff5,0xa9014ff4}},
        {0x8c0d4c0,{0xb000f348,0xf9429913}},
    };
    // The actual cvar type/callsite is checked again at application time.
    for(unsigned i=0;i<sizeof code/sizeof *code;i++)
        if(memcmp((const void*)(base+code[i].addr),code[i].first,8))return;
    mobile_fps_find=(void* (*)(void*,const unsigned short*,int))(base+0x65e9ba0);
    mobile_fps_get=(float (*)(const void*))(base+0x65e7b0c);
    mobile_fps_set=(void (*)(void*,const unsigned short*,unsigned))(base+0x65eec38);
    mobile_fps_base=base;
}
static void touch_mobile_settings_tick(void) {
    if(!mobile_fps_base || !cinema_on_game_thread())return;
    uint64_t packed=__atomic_load_n(&touch_settings_value,__ATOMIC_ACQUIRE);
    int target=(int)((packed>>38)&262143);
    if(target==mobile_fps_applied)return;
    void* manager=*(void**)(mobile_fps_base+0xa6b1ac0);
    if(!manager)return;
    static const unsigned short name[]={'t','.','M','a','x','F','P','S',0};
    void* var=mobile_fps_find(manager,name,0);
    if(!var || *(uintptr_t*)var!=mobile_fps_base+0x9e20df8)return;
    unsigned flags=*(const unsigned*)((const char*)var+0x18);
    float before=mobile_fps_get(var),wanted=(float)target/1000.0f;
    if(!isfinite(before) || before<0 || before>1000 || (flags&4u)
        || (flags&0xff000000u)>0x08000000u) {
        mobile_fps_applied=target;
        if(mobile_fps_logs<16){++mobile_fps_logs;LOGF("MOBILE FPS retained native=%.3f requested=%.3f flags=%08x",before,wanted,flags);}return;
    }
    if(fabsf(before-wanted)>0.0005f) {
        char ascii[16];unsigned short value[16];
        int n=snprintf(ascii,sizeof ascii,"%d.%03d",target/1000,target%1000);
        if(n<=0 || n>=(int)sizeof ascii)return;
        for(int i=0;i<=n;i++)value[i]=(unsigned char)ascii[i];
        mobile_fps_set(var,value,0x08000000u);
    }
    float after=mobile_fps_get(var);
    if(isfinite(after) && fabsf(after-wanted)<=0.0005f)mobile_fps_applied=target;
    if(mobile_fps_logs<16){++mobile_fps_logs;LOGF("MOBILE FPS native t.MaxFPS %.3f -> %.3f requested=%.3f flags=%08x",before,after,wanted,flags);}
}
#endif
