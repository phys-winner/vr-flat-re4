#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
static int on_gt=1,logs;
static int cinema_on_game_thread(void){return on_gt;}
#define LOGF(...) ((void)++logs)
#include "re4fix_mobile_settings.h"
static unsigned char variable[64];
static int find_calls,get_calls,set_calls,find_null;
static float current;
static void* fake_find(void* manager,const unsigned short* name,int flags){
    assert(manager==(void*)0x1234 && flags==0);find_calls++;
    const unsigned short wanted[]={'t','.','M','a','x','F','P','S',0};
    assert(!memcmp(name,wanted,sizeof wanted));return find_null?0:variable;
}
static float fake_get(const void* v){assert(v==variable);get_calls++;return current;}
static void fake_set(void* v,const unsigned short* text,unsigned flags){
    assert(v==variable && flags==0x08000000u);set_calls++;
    char ascii[16];unsigned i=0;for(;text[i] && i<15;i++)ascii[i]=(char)text[i];
    assert(i<15);ascii[i]=0;current=strtof(ascii,0);
}
/* The checker supplies the actual native code preimages to both installers. */
extern void fixture_settings_image(unsigned char*);
extern unsigned long fixture_settings_code[];
extern unsigned fixture_settings_count;
static void settings_snapshot_tests(void){
    struct touch_mobile_settings v=touch_mobile_settings_snapshot();
    assert(v.sensitivity==1 && v.size==1 && v.x==0 && v.y==0 && v.fps==60);
    for(int i=0;i<20000;i++){
        int s=500+i%1501,z=750+i%501,x=i%161-80,y=(i*7)%161-80,f=24000+(i*997)%216001;
        assert(touch_mobile_settings_apply(s,z,x,y,f));v=touch_mobile_settings_snapshot();
        assert(fabsf(v.sensitivity-s/1000.0f)<1e-6f && fabsf(v.size-z/1000.0f)<1e-6f);
        assert(fabsf(v.x-x/1000.0f)<1e-6f && fabsf(v.y-y/1000.0f)<1e-6f && fabsf(v.fps-f/1000.0f)<1e-5f);
    }
    const int invalid[][5]={{499,1000,0,0,60000},{2001,1000,0,0,60000},{1000,749,0,0,60000},
        {1000,1251,0,0,60000},{1000,1000,-81,0,60000},{1000,1000,81,0,60000},
        {1000,1000,0,-81,60000},{1000,1000,0,81,60000},{1000,1000,0,0,23999},{1000,1000,0,0,240001}};
    uint64_t saved=touch_settings_value;
    for(unsigned i=0;i<sizeof invalid/sizeof *invalid;i++){
        const int* a=invalid[i];assert(!touch_mobile_settings_apply(a[0],a[1],a[2],a[3],a[4]));assert(saved==touch_settings_value);
    }
    // The Java settings Activity (and its nativeApply bridge) is retired: values come from the native options rows.
    assert(touch_mobile_settings_apply(1000,1000,0,0,60000));
    assert(touch_settings_value==TOUCH_SETTINGS_PACK(1000,1000,0,0,60000));
}
#ifdef _WIN32
static void setup(uintptr_t base){
    memset(variable,0,sizeof variable);*(uintptr_t*)variable=base+0x9e20df8;
    *(void**)(base+0xa6b1ac0)=(void*)0x1234;
    mobile_fps_base=base;mobile_fps_find=fake_find;mobile_fps_get=fake_get;mobile_fps_set=fake_set;
    mobile_fps_applied=0;mobile_fps_logs=0;find_calls=get_calls=set_calls=logs=find_null=0;on_gt=1;current=72;
    assert(touch_mobile_settings_apply(1000,1000,0,0,60000));
}
#endif
int main(void){
    settings_snapshot_tests();
#ifdef _WIN32
    uintptr_t base=0x20000000;
    unsigned char* image=VirtualAlloc((void*)base,0xb000000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(image==(void*)base);fixture_settings_image(image);
    install_touch_mobile_settings(base);assert(mobile_fps_base==base);
    for(unsigned i=0;i<fixture_settings_count;i++)for(unsigned j=0;j<2;j++){
        *(unsigned*)(image+fixture_settings_code[i]+j*4)^=1;
        install_touch_mobile_settings(base);assert(!mobile_fps_base);
        *(unsigned*)(image+fixture_settings_code[i]+j*4)^=1;
    }
    install_touch_mobile_settings(0);assert(!mobile_fps_base);
    const int targets[]={30000,45000,60000,59940,90000,120000,144000,240000};
    for(unsigned i=0;i<sizeof targets/sizeof *targets;i++){
        setup(base);assert(touch_mobile_settings_apply(1000,1000,0,0,targets[i]));touch_mobile_settings_tick();
        assert(fabsf(current-targets[i]/1000.0f)<.0005f && set_calls==1 && mobile_fps_applied==targets[i]);
        touch_mobile_settings_tick();assert(set_calls==1 && find_calls==1);
    }
    setup(base);on_gt=0;touch_mobile_settings_tick();assert(!find_calls && !set_calls);
    setup(base);*(void**)(base+0xa6b1ac0)=0;touch_mobile_settings_tick();assert(!find_calls);
    setup(base);find_null=1;touch_mobile_settings_tick();assert(find_calls==1 && !get_calls && !set_calls);
    setup(base);*(uintptr_t*)variable^=8;touch_mobile_settings_tick();assert(!get_calls && !set_calls);
    const unsigned flags[]={4,0x09000000u,0xff000000u};
    for(unsigned i=0;i<sizeof flags/sizeof *flags;i++){
        setup(base);*(unsigned*)(variable+0x18)=flags[i];touch_mobile_settings_tick();assert(!set_calls && current==72);
    }
    const float bad[]={NAN,INFINITY,-1,1001};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;i++){
        setup(base);current=bad[i];touch_mobile_settings_tick();assert(!set_calls);
    }
    setup(base);current=60;touch_mobile_settings_tick();assert(!set_calls && mobile_fps_applied==60000);
    setup(base);*(unsigned*)(variable+0x18)=0x08000000u;touch_mobile_settings_tick();assert(current==60 && set_calls==1);
    assert(VirtualFree(image,0,MEM_RELEASE));
#endif
    puts("PASS: actual atomic settings bounds/defaults/20k round trips; FPS float cvar targets, GT/type/priority/value guards and installer corruption refusals");
    return 0;
}
