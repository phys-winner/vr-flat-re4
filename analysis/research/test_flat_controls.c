#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
/* Actual shared guard inserted by checker. */
static int cinema_object(const void*,const void*,int);
static unsigned char pawn[0x600],cls[0x60],settings[0xb8],original[0xb8];
static void* chain[1];static int on_gt=1,qs_ready=1,copies,sets,destroys,logs;
static unsigned context=1;
static int cinema_on_game_thread(void){return on_gt;}
static unsigned get_type(void){return context;}
static void* get_active(void){return pawn;}
static void* get_class(void){return cls;}
static const void* get_settings(void){return settings;}
static void copy_settings(void* to,const void* from){assert(from==settings);copies++;memcpy(to,from,0xb8);}
static void set_settings(const void* from){assert(from!=settings);sets++;memcpy(settings,from,0xb8);}
static void destroy_settings(void* copy){assert(copy!=settings);destroys++;memset(copy,0,0xb8);}
static void* (*cinema_active)(void)=get_active,*(*comfort_player_class)(void)=get_class;
static unsigned (*cinema_type)(void)=get_type;
static const void* (*qs_settings)(void)=get_settings;
static void (*qs_copy)(void*,const void*)=copy_settings,(*qs_set)(const void*)=set_settings,(*qs_destroy)(void*)=destroy_settings;
#define LOGF(...) ((void)++logs)
// The one-time phone-defaults marker file: in-memory stand-in (production uses the app data directory).
#ifndef O_WRONLY
#define O_WRONLY 1
#define O_CREAT 0x40
#define O_TRUNC 0x200
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0x80000
#endif
static int marker_writes;
static int fixture_open(const char* path,int flags,int mode){(void)path;(void)flags;(void)mode;return 7;}
static long fixture_write(int fd,const void* data,unsigned long size){(void)data;assert(fd==7);marker_writes++;return (long)size;}
static int fixture_close(int fd){assert(fd==7);return 0;}
static int marker_exists;
#ifndef F_OK
#define F_OK 0
#endif
static int fixture_access(const char* path,int mode){(void)path;(void)mode;return marker_exists ? 0 : -1;}
#define access fixture_access
#define open fixture_open
#define write fixture_write
#define close fixture_close
#include "re4fix_flat_controls.h"
#undef access
#undef open
#undef write
#undef close
extern void fixture_flat_image(unsigned char*);
extern unsigned long fixture_flat_code[];
extern unsigned fixture_flat_count;
static void setup(void){
    memset(cls,0,sizeof cls);memset(pawn,0,sizeof pawn);chain[0]=cls+0x30;
    *(void**)(cls+0x30)=chain;*(int*)(cls+0x38)=0;*(int*)(cls+0x58)=0x570;*(void**)(pawn+0x10)=cls;
    memset(settings,0x91,sizeof settings);settings[0]=45;settings[8]=1;
    settings[0x33]=1;settings[0x34]=0;settings[0x3e]=1;settings[0x48]=1;settings[0x49]=1;settings[0x4a]=1;settings[0x4b]=0;settings[0x4f]=2;settings[0x90]=1;memcpy(original,settings,sizeof original);
    copies=sets=destroys=logs=0;on_gt=1;context=1;flat_controls_ready=1;flat_controls_logs=0;
}
int main(void){
    setup();touch_flat_controls_tick();assert(copies==1 && sets==1 && destroys==1);
    for(unsigned i=0;i<sizeof settings;i++)
        assert(settings[i]==(i==0x4b?2:i==0x34?1:(i==0 || i==8 || i==0x33 || i==0x3e || i==0x48 || i==0x49 || i==0x4f || i==0x53 || i==0x90)?0:original[i]));
    touch_flat_controls_tick();assert(copies==1); /* No repeated native persistence when already flat. */
    /* ActionTransitionMode is the player's option: either value is kept and never rewritten. */
    for(unsigned mode=0;mode<2;mode++){setup();settings[0x4a]=mode;touch_flat_controls_tick();assert(sets==1 && settings[0x4a]==mode);
        touch_flat_controls_tick();assert(sets==1);}
    for(unsigned type=1;type<=7;type++){setup();context=type;touch_flat_controls_tick();assert(sets==1);}
    const unsigned turns[]={0,15,30,45,90};
    for(unsigned i=0;i<sizeof turns/sizeof *turns;i++)for(unsigned mode=0;mode<3;mode++){
        setup();settings[0]=turns[i];settings[0x48]=mode;touch_flat_controls_tick();assert(sets==1);
    }
    for(unsigned stick=0;stick<2;stick++)for(unsigned steering=0;steering<3;steering++){
        setup();settings[0x33]=stick;settings[0x4f]=steering;touch_flat_controls_tick();
        assert(sets==1 && settings[0x33]==0 && settings[0x4f]==0);
    }
    for(unsigned boat=0;boat<3;boat++){setup();settings[0x4b]=boat;touch_flat_controls_tick();assert(sets==1 && settings[0x4b]==2);}
    for(unsigned strafe=0;strafe<2;strafe++)for(unsigned combat=0;combat<2;combat++)for(unsigned facing=0;facing<2;facing++){
        setup();settings[0x34]=strafe;settings[0x3e]=combat;settings[0x49]=facing;touch_flat_controls_tick();
        assert(sets==1 && settings[0x34]==1 && settings[0x3e]==0 && settings[0x49]==0);
    }
    const unsigned offsets[]={0,8,0x33,0x34,0x3e,0x48,0x49,0x4a,0x4b,0x4f,0x90};
    for(unsigned i=0;i<sizeof offsets/sizeof *offsets;i++){
        setup();settings[offsets[i]]=255;touch_flat_controls_tick();assert(!sets && !copies && !destroys);
    }
    setup();on_gt=0;touch_flat_controls_tick();assert(!sets);
    setup();context=0;touch_flat_controls_tick();assert(!sets);context=8;touch_flat_controls_tick();assert(!sets);
    setup();*(int*)(cls+0x58)=0x56f;touch_flat_controls_tick();assert(!sets);
    setup();chain[0]=0;touch_flat_controls_tick();assert(!sets);
    setup();for(int i=0;i<20;i++){settings[0]=45;touch_flat_controls_tick();}assert(flat_controls_logs==8);
#ifdef _WIN32
    uintptr_t base=0x20000000;unsigned char* image=VirtualAlloc((void*)base,0xb000000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(image==(void*)base);fixture_flat_image(image);setup();install_touch_flat_controls(base);assert(flat_controls_ready);
    for(unsigned i=0;i<fixture_flat_count;i++)for(unsigned j=0;j<2;j++){
        *(unsigned*)(image+fixture_flat_code[i]+j*4)^=1;install_touch_flat_controls(base);assert(!flat_controls_ready);
        *(unsigned*)(image+fixture_flat_code[i]+j*4)^=1;
    }
    const unsigned fields[]={0x9db6000,0x9db5808,0x9db5748,0x9db5c58,0x9db55e0,0x9db56e8,0x9db5950,0x9db57a8};
    for(unsigned i=0;i<8;i++){*(unsigned*)(image+fields[i]+36)^=1;install_touch_flat_controls(base);assert(!flat_controls_ready);*(unsigned*)(image+fields[i]+36)^=1;}
    const unsigned tables[]={0x9d5e8e8,0x9d65b30,0x9d58690,0x9d5a2b0,0x9d5dbf8},counts[]={3,4,4,3,3};
    for(unsigned table=0;table<5;table++)for(unsigned i=0;i<counts[table];i++){
        int64_t* value=(int64_t*)(image+tables[table]+i*16+8);
        *value^=1;install_touch_flat_controls(base);assert(!flat_controls_ready);*value^=1;
    }
    qs_ready=0;install_touch_flat_controls(base);assert(!flat_controls_ready);qs_ready=1;
    install_touch_flat_controls(0);assert(!flat_controls_ready);assert(VirtualFree(image,0,MEM_RELEASE));
#endif
    puts("PASS: actual original deep-copy/set/destroy order; only eleven proved VR fields change, movement/steering/boat/strafe/combat/facing profiles, type/GT/enums guards and installer refusals");return 0;
}
