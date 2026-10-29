// Original Ashley command callback; UI readers contain only atomic snapshots.
#ifndef RE4FIX_ASHLEY_H
#define RE4FIX_ASHLEY_H
#include <stdint.h>
struct touch_ashley_state {int available,active,next;}; // Follow=0, Stay=1.
static void* (*ash_class)(void),*(*ash_bio4)(void);
static int (*ash_exists)(const void*),(*ash_health)(const void*),(*ash_can)(const void*),(*ash_max)(void);
// HUD: Ashley's health while she exists in the room, (ms << 32) | max << 16 | current.
static unsigned long long ash_hp;
static int touch_ashley_health(long now,int* current,int* maximum) {
    unsigned long long value=__atomic_load_n(&ash_hp,__ATOMIC_ACQUIRE);
    if(!value || now<=0 || __atomic_load_n(&g_touch_context,__ATOMIC_ACQUIRE)!=1
        || (uint32_t)((uint32_t)now-(uint32_t)(value>>32))>500)return 0;
    *current=(int)(value&0xffffu);*maximum=(int)((value>>16)&0xffffu);
    return *maximum>0 && *current<=*maximum;
}
static unsigned (*ash_active)(const void*),(*ash_next)(const void*);
static int (*ash_regular)(const void*),(*ash_control)(const void*),(*ash_event)(void),(*ash_qte)(void);
static void (*ash_toggle)(void*);
static void* const* ash_sub_global;
static void* const* ash_player_global;
static unsigned long long ash_ui,ash_request;
static int ash_ready;
static unsigned ash_generation,ash_logs;
static void* ash_pawn,*ash_sub,*ash_player;
static unsigned long long ash_name;
static int ash_input_context(void) {
    return __atomic_load_n(&g_touch_context,__ATOMIC_ACQUIRE)==1
        && !__atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE)
        && !g_touch_pause && !g_touch_inventory_request && !g_touch_reload_request
        && !g_reload_phase && !g_touch_weapon_request && !g_touch_interact_request
        && !touch_quickselect_busy() && !touch_motion_pose_busy();
}
static struct touch_ashley_state touch_ashley_state(long now) {
    struct touch_ashley_state out={0,-1,-1};
    uint64_t value=__atomic_load_n(&ash_ui,__ATOMIC_ACQUIRE);
    if(!value || now<=0 || !ash_input_context()
        || (uint32_t)((uint32_t)now-(uint32_t)(value>>32))>500)return out;
    out.available=(unsigned)value&1;out.active=((unsigned)value>>1)&1;out.next=((unsigned)value>>2)&1;
    return out;
}
static int touch_ashley_request(long now) {
    uint64_t value=__atomic_load_n(&ash_ui,__ATOMIC_ACQUIRE);
    if(!value || !(value&1) || now<=0 || !ash_input_context()
        || (uint32_t)((uint32_t)now-(uint32_t)(value>>32))>500)return 0;
    unsigned long long request=((unsigned long long)(uint32_t)now<<32)|(unsigned)value;
    unsigned long long empty=0;
    return __atomic_compare_exchange_n(&ash_request,&empty,request,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
static void touch_ashley_cancel(void) {
    __atomic_store_n(&ash_request,0,__ATOMIC_RELEASE);__atomic_store_n(&ash_ui,0,__ATOMIC_RELEASE);
}
static int ash_live(void* pawn,void** sub,void** player,unsigned* active,unsigned* next) {
    if(!ash_ready || !qs_gt() || !ash_input_context() || phone_type()!=1
        || rl_paused() || !rl_player_valid() || !tw_typed(pawn,ash_class(),0x1650)
        || pawn!=phone_active() || pawn!=ash_bio4())return 0;
    // Native legacy actors are identity tokens only: never dereference cached actors.
    *sub=*ash_sub_global;*player=*ash_player_global;
    if(!*sub || !*player)return 0;
    unsigned long long name=qs_name(pawn);
    if(!ash_regular(pawn) || ash_event() || ash_qte() || !qs_allowed(pawn)
        || !ash_input_context() || pawn!=phone_active() || pawn!=ash_bio4() || qs_name(pawn)!=name
        || *sub!=*ash_sub_global || *player!=*ash_player_global)return 0;
    if(!ash_exists(pawn) || ash_health(pawn)<=0 || !ash_control(*player))return 0;
    *active=ash_active(pawn);*next=ash_next(pawn);
    return *active<=1 && *next<=1 && *sub==*ash_sub_global && *player==*ash_player_global
        && ash_input_context() && phone_type()==1 && pawn==phone_active() && pawn==ash_bio4();
}
static int ash_still(void* pawn,unsigned long long name,void* sub,void* player,unsigned active,unsigned next) {
    return ash_input_context() && phone_type()==1 && !rl_paused() && rl_player_valid()
        && tw_typed(pawn,ash_class(),0x1650) && pawn==phone_active() && pawn==ash_bio4()
        && qs_name(pawn)==name && sub==*ash_sub_global && player==*ash_player_global
        && !ash_event() && !ash_qte() && ash_exists(pawn) && ash_health(pawn)>0
        && ash_regular(pawn) && ash_control(player) && ash_active(pawn)==active && ash_next(pawn)==next
        && ash_input_context() && pawn==phone_active() && qs_name(pawn)==name
        && sub==*ash_sub_global && player==*ash_player_global;
}
static void touch_ashley_tick(void* hand,long now) {
    if(!ash_ready || !qs_gt() || !tw_typed(hand,tw_hand_class(),0x510)
        || *(const unsigned char*)((const char*)hand+0x314)!=1)return;
    void* pawn=phone_player(hand);
    // A pooled inactive hand must not consume or overwrite an active request.
    if(pawn!=phone_active() || !tw_typed(pawn,ash_class(),0x1650) || hand!=rl_right(pawn))return;
    void* sub=0,*player=0;unsigned active=0,next=0;
    if(ash_max && now>0 && phone_type()==1 && !rl_paused() && rl_player_valid() && pawn==ash_bio4() && ash_exists(pawn)) {
        int current=ash_health(pawn),maximum=ash_max();
        if(maximum>0 && maximum<0x8000 && current>=0 && current<=maximum)
            __atomic_store_n(&ash_hp,((unsigned long long)(uint32_t)now<<32)|((unsigned long long)maximum<<16)|(unsigned)current,__ATOMIC_RELEASE);
    } else __atomic_store_n(&ash_hp,0,__ATOMIC_RELEASE);
    if(now<=0 || !ash_live(pawn,&sub,&player,&active,&next)){
        touch_ashley_cancel();ash_pawn=ash_sub=ash_player=0;return;
    }
    unsigned long long name=qs_name(pawn);
    if(ash_pawn!=pawn || ash_name!=name || ash_sub!=sub || ash_player!=player){
        touch_ashley_cancel();ash_pawn=pawn;ash_name=name;ash_sub=sub;ash_player=player;
        ash_generation=(ash_generation+1)&0x1fffffffu;if(!ash_generation)ash_generation=1;
    }
    uint64_t request=__atomic_exchange_n(&ash_request,0,__ATOMIC_ACQ_REL);
    if(request && ((unsigned)request>>3)==ash_generation
        && (uint32_t)((uint32_t)now-(uint32_t)(request>>32))<=500
        && (((unsigned)request>>1)&1)==active && (((unsigned)request>>2)&1)==next
        && ash_can(pawn)) {
        // Re-evaluate authoritative eligibility at execution, including a new QTE/scene.
        void* checked_sub=0,*checked_player=0;unsigned checked_active=0,checked_next=0;
        if(ash_live(pawn,&checked_sub,&checked_player,&checked_active,&checked_next)
            && checked_sub==sub && checked_player==player && qs_name(pawn)==name
            && checked_active==active && checked_next==next && ash_can(pawn)
            && ash_still(pawn,name,sub,player,active,next)) {
            ash_toggle(pawn);
            if(ash_logs<12){++ash_logs;LOGF("ASHLEY native command requested active=%u next=%u pawn=%p",active,next,pawn);}
        }
    }
    // No retry/ack writes: the original callback owns voice, timers, hiding and movement.
    if(!ash_live(pawn,&sub,&player,&active,&next) || sub!=ash_sub || player!=ash_player
        || qs_name(pawn)!=ash_name){touch_ashley_cancel();return;}
    int available=ash_can(pawn);
    if(!ash_still(pawn,ash_name,sub,player,active,next)){touch_ashley_cancel();return;}
    unsigned low=(available?1u:0u)|(active<<1)|(next<<2)|(ash_generation<<3);
    __atomic_store_n(&ash_ui,((uint64_t)(uint32_t)now<<32)|low,__ATOMIC_RELEASE);
}
static void install_touch_ashley(unsigned long base) {
    static const struct{unsigned long addr;unsigned word[2];} check[]={
        {0x621a6d4,{0xa9bf7bfd,0x910003fd}},{0x621a968,{0xa9bf7bfd,0x910003fd}},
        {0x621ee50,{0xa9bf7bfd,0x910003fd}},{0x621eeec,{0xa9bf7bfd,0x910003fd}},
        {0x621ef18,{0xa9bf7bfd,0x910003fd}},{0x621ef50,{0xa9be4ff4,0xa9017bfd}},
        {0x63c6b34,{0xf9401028,0xf100011f}},{0x63c8a5c,{0xd10183ff,0xf90023f3}},
        {0x63c8ae4,{0x5282ca04,0x52800205}},{0x621984c,{0xf81e0ff3,0xa9017bfd}},
        {0x6216efc,{0xa9bf7bfd,0x910003fd}},{0x5fb1770,{0x90021d88,0xf9469908}},
        {0x6241018,{0x90020900,0xf9469400}},{0x62dd310,{0xb0020448,0xf9415508}},
        {0x62a8e50,{0xf81e0ff3,0xa9017bfd}},{0x621a9c8,{0xa9bf7bfd,0x910003fd}},
    };
    __atomic_store_n(&ash_ready,0,__ATOMIC_RELEASE);touch_ashley_cancel();ash_pawn=ash_sub=ash_player=0;
    if(!base || !qs_ready || !tw_hand_class || !phone_player || !phone_active || !phone_type
        || !rl_paused || !rl_player_valid || !rl_right || !qs_allowed)return;
    if(tw_hand_class!=(void* (*)(void))(base+0x6424b5c)
        || phone_player!=(void* (*)(const void*))(base+0x633d030)
        || phone_active!=(void* (*)(void))(base+0x63380dc)
        || phone_type!=(unsigned (*)(void))(base+0x6338aec)
        || rl_paused!=(int (*)(void))(base+0x61d0934)
        || rl_player_valid!=(int (*)(void))(base+0x61d401c)
        || rl_right!=(void* (*)(const void*))(base+0x62a8e50)
        || qs_allowed!=(int (*)(const void*))(base+0x63c767c))return;
    for(unsigned i=0;i<sizeof check/sizeof *check;i++)if(memcmp((const void*)(base+check[i].addr),check[i].word,8)){
        LOGF("ASHLEY disabled: opcode mismatch +%lx",check[i].addr);return;
    }
    if(*(void* const*)(base+0xa361e48)!=(void*)(base+0xa4e9b48)
        || *(void* const*)(base+0xa361d78)!=(void*)(base+0xa4e9b40)
        || *(const unsigned*)(base+0x9d703f0+40)!=2
        || *(void* const*)(base+0x9d703f0+32)!=(void*)(base+0x9d703d0))return;
    const char* const names[]={"EAshleyCommand::Follow","EAshleyCommand::Stay"};
    for(unsigned i=0;i<2;i++){
        const char* entry=(const char*)(base+0x9d703d0+16*i);const char* name=*(const char* const*)entry;
        if(!name || strcmp(name,names[i]) || *(const long long*)(entry+8)!=(long long)i)return;
    }
    ash_class=(void* (*)(void))(base+0x63c8a5c);ash_bio4=(void* (*)(void))(base+0x621984c);
    ash_can=(int (*)(const void*))(base+0x621a6d4);ash_health=(int (*)(const void*))(base+0x621a968);
    ash_max=(int (*)(void))(base+0x621a9c8);
    ash_exists=(int (*)(const void*))(base+0x621ee50);ash_active=(unsigned (*)(const void*))(base+0x621eeec);
    ash_next=(unsigned (*)(const void*))(base+0x621ef18);ash_toggle=(void (*)(void*))(base+0x621ef50);
    ash_regular=(int (*)(const void*))(base+0x6216efc);ash_control=(int (*)(const void*))(base+0x5fb1770);
    ash_event=(int (*)(void))(base+0x6241018);ash_qte=(int (*)(void))(base+0x62dd310);
    ash_sub_global=(void* const*)(base+0xa4e9b48);ash_player_global=(void* const*)(base+0xa4e9b40);
    __atomic_store_n(&ash_ready,1,__ATOMIC_RELEASE);LOGF("ASHLEY original Wait/Follow callback and native eligibility ready");
}
#endif
