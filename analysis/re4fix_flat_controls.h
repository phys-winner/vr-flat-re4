// Native Settings deep copy removes VR locomotion/turn modes without touching scripted teleports/fades.
#ifndef RE4FIX_FLAT_CONTROLS_H
#define RE4FIX_FLAT_CONTROLS_H
#include <stdint.h>
static int flat_controls_ready;
static unsigned flat_controls_logs;
// One-time phone defaults; afterwards the player's own native option choices are kept.
static int flat_defaults_state;   // 0 unknown, 1 pending, 2 done
static const char flat_defaults_path[]="/data/data/com.Armature.VR4/re4_mobile_defaults_v1";
static void install_touch_flat_controls(uintptr_t base) {
    static const struct {unsigned long addr;unsigned first[2];} code[]={
        {0x62c7dd4,{0x39412008,0x7100051f}},
        {0x643925c,{0x52800028,0x39002008}},
        {0x6439178,{0x52800028,0x39024008}},
        {0x6347298,{0x39400288,0xbd4006a0}},
        {0x6216694,{0x9402c5dc,0x3940cc08}},
        {0x62aa4fc,{0xd10483ff,0xfd0063ea}},
        {0x62aa824,{0x3940ce88,0xb00205a9}},
        {0x62aa8e0,{0x3940ce88,0xb00205a9}},
        {0x621baac,{0xd10243ff,0xf90033f5}},
        {0x621bad4,{0x39413c08,0x7100051f}},
        {0x621bb70,{0xaa1303e0,0x9404974a}},
        {0x62faf3c,{0xa9bf7bfd,0x910003fd}},
        {0x62fad44,{0x97ff3430,0x39412c08}},
        {0x62fada0,{0xaa1403e0,0x97febdd6}},
        {0x62c7d94,{0x3940d008,0x340000a8}},
        {0x6439220,{0x52800028,0x3900d008}},
        {0x621a120,{0xf81e0ff3,0xa9017bfd}},
        {0x621a174,{0xd101c3ff,0xa90457f6}},
        {0x621a378,{0xd10283ff,0xf9003bf5}},
        {0x643919c,{0x52800028,0x39014c08}},
    };
    flat_controls_ready=0;
    if(!base || !qs_ready || !qs_settings || !qs_copy || !qs_set || !qs_destroy
        || !comfort_player_class || !cinema_active || !cinema_type)return;
    for(unsigned i=0;i<sizeof code/sizeof *code;i++)
        if(memcmp((const void*)(base+code[i].addr),code[i].first,8))return;
    if(*(const unsigned*)(base+0x9db6000+36)!=0
        || *(const unsigned*)(base+0x9db5808+36)!=0x48
        || *(const unsigned*)(base+0x9db5748+36)!=0x4a
        || *(const unsigned*)(base+0x9db5c58+36)!=0x33
        || *(const unsigned*)(base+0x9db55e0+36)!=0x4f
        || *(const unsigned*)(base+0x9db56e8+36)!=0x4b
        || *(const unsigned*)(base+0x9db5950+36)!=0x3e
        || *(const unsigned*)(base+0x9db57a8+36)!=0x49)return;
    for(unsigned i=0;i<3;i++)if(*(const int64_t*)(base+0x9d5e8e8+i*16+8)!=i)return;
    for(unsigned i=0;i<4;i++)if(*(const int64_t*)(base+0x9d65b30+i*16+8)!=i)return;
    for(unsigned i=0;i<4;i++)if(*(const int64_t*)(base+0x9d58690+i*16+8)!=i)return;
    for(unsigned i=0;i<3;i++)if(*(const int64_t*)(base+0x9d5a2b0+i*16+8)!=i)return;
    for(unsigned i=0;i<3;i++)if(*(const int64_t*)(base+0x9d5dbf8+i*16+8)!=i)return;
    flat_controls_ready=1;
    LOGF("FLAT controls ready: native continuous walk/smooth turn/first-person action modes");
}
static void touch_flat_controls_tick(void) {
    if(!flat_controls_ready || !cinema_on_game_thread())return;
    unsigned type=cinema_type();void* pawn=cinema_active();
    if(type<1 || type>7 || !cinema_object(pawn,comfort_player_class(),0x570))return;
    const unsigned char* source=qs_settings();
    if(!source || (source[0]!=0 && source[0]!=15 && source[0]!=30 && source[0]!=45 && source[0]!=90)
        || source[8]>1 || source[0x33]>1 || source[0x34]>1 || source[0x3e]>1
        || source[0x48]>2 || source[0x49]>1 || source[0x4a]>1
        || source[0x4b]>2 || source[0x4f]>2 || source[0x90]>1)return;
    if(!flat_defaults_state)flat_defaults_state=access(flat_defaults_path,F_OK)==0 ? 2 : 1;
    int defaults=flat_defaults_state==1 && source[0x31]<=3 && source[0x3a]<=1;
    // ActionTransitionMode (0x4a) is the player's choice in the options menu; it is validated above, not forced.
    if(!defaults && !source[0] && !source[8] && !source[0x33] && source[0x34]==1 && !source[0x3e]
        && !source[0x48] && !source[0x49]
        && source[0x4b]==2 && !source[0x4f] && !source[0x90] && !source[0x53])return;
    // FString/arrays remain owned by the original copy constructor/destructor.
    struct __attribute__((aligned(16))) {unsigned char bytes[0xb8];} copy;
    qs_copy(&copy,source);
    copy.bytes[0]=0;       // ETurnMode::SmoothTurn, not 15/30/45/90-degree VR snap turn.
    copy.bytes[8]=0;       // Original EnableQuickTurn; avoids its VR fade callbacks.
    copy.bytes[0x33]=0;    // EHandedness::Left: touch publishes its movement axes here.
    copy.bytes[0x34]=1;    // EnableStrafing: the left stick keeps its horizontal axis.
    copy.bytes[0x3e]=0;    // ECombatStyle::RunAndGun retains ordinary FPS sideways movement.
    copy.bytes[0x48]=0;    // EPlayerMovementMode::Continuous; no teleport/motion blinding mode.
    copy.bytes[0x49]=0;    // MaintainCurrent: first-person transitions preserve the player's view direction.
    copy.bytes[0x4b]=2;    // Original boat mode reads movement axes instead of grabbed VR handles.
    copy.bytes[0x4f]=0;    // ESteeringMethod::Head: movement follows the flat camera, not a VR hand.
    copy.bytes[0x90]=0;    // EnableComfortCutscenes: full-frame authored cinema already provided separately.
    copy.bytes[0x53]=0;    // EnableTutorialUI: the VR body-gesture tutorial cannot complete on a flat phone.
    if(defaults) {
        copy.bytes[0x31]=0;  // EVignetteStrength::None: no VR tunneling unless the player enables it.
        copy.bytes[0x3a]=1;  // EnableCameraBobbing: native step bob/anchor curve gives phone view bobbing.
    }
    qs_set(&copy);qs_destroy(&copy);
    if(defaults) {
        int fd=open(flat_defaults_path,O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600);
        if(fd>=0){(void)!write(fd,"1",1);close(fd);}
        flat_defaults_state=2;
        LOGF("FLAT phone defaults applied: tunneling=None camera_bobbing=On (marker fd=%d)",fd);
    }
    if(flat_controls_logs<8){++flat_controls_logs;LOGF("FLAT controls applied native VR settings pawn=%p type=%u",pawn,type);}
}
#endif
