// Include after flat_view. Both entry points are GameThread-only, outside input mutexes.
#ifndef RE4FIX_MERC_UI_H
#define RE4FIX_MERC_UI_H
#include <stdint.h>

static uintptr_t merc_ui_base;
static int (*merc_ui_has_instance)(void),(*merc_ui_is_merc)(void);
static int (*merc_ui_frame)(void*),(*merc_ui_playing)(const void*,const void*);
static void* (*merc_ui_class)(void),*(*merc_ui_animation_class)(void);
static struct {
    struct cinema_ui_weak pawn,ui,ring,component,widget,animation;
    int used;
} merc_ui_owned;

static void touch_merc_ui_cleanup(void) {
    // PlayEndMission itself requested visible=true. Do not restore a late Intro's false value:
    // native EndMessage/FadeMessage, pawn visibility and destruction retain their original cleanup.
    memset(&merc_ui_owned,0,sizeof merc_ui_owned);
}
static void touch_merc_ui_tick(void) {
    if(!cinema_on_game_thread())return;
    if(!merc_ui_base || !merc_ui_has_instance || !merc_ui_is_merc || !merc_ui_frame || !merc_ui_playing
        || !merc_ui_class || !merc_ui_animation_class || !phone_type || !phone_active || !phone_inspect_ready
        || !comfort_player_class || !cu_component_class || !cu_user_class || !cu_get_widget || !mv_visible
        || !cu_weak_assign || !cu_weak_get || !fv_title_actor || !rl_paused || !rl_player_valid || phone_type()!=1
        || rl_paused() || !rl_player_valid() || !merc_ui_is_merc()) {
        touch_merc_ui_cleanup();return;
    }
    const unsigned char* game=*(const unsigned char* const*)(merc_ui_base+0xa3f5e48);
    void* countdown=(void*)(merc_ui_base+0xa4e2290+0xb0);
    if(!game || (game[0x50aa]&0x10) || *(const unsigned char*)(merc_ui_base+0xa50b850+0xe9)
        || (*(const unsigned char*)countdown&1) || merc_ui_frame(countdown)>0 || !merc_ui_has_instance()) {
        touch_merc_ui_cleanup();return;
    }
    void* pawn=phone_active(),*ui=*(void* const*)(merc_ui_base+0xa579328);
    int started=0,ended=0;
    if(!pv_alive(pawn) || !cinema_object(pawn,comfort_player_class(),0x570)
        || !pv_alive(ui) || !cinema_object(ui,merc_ui_class(),0x2e8)
        || !cinema_ui_named(ui,"VR4MercUI_BP_C",0x310)
        || fv_title_actor(pawn,merc_ui_class())!=ui
        || !fv_action_bool_read(ui,"MissionStartCleared",&started) || !started
        || !fv_action_bool_read(ui,"MissionEndCleared",&ended) || ended) {
        touch_merc_ui_cleanup();return;
    }
    void* ring=fv_object_property(ui,"MercRing");
    if(!pv_alive(ring) || !cinema_ui_named(ring,"MercRing_BP_C",0x220)
        || *(void* const*)((const char*)ring+0xe0)!=ui || (*(const unsigned char*)((const char*)ring+0x58)&0x20)) {
        touch_merc_ui_cleanup();return;
    }
    void* component=fv_object_property(ring,"WidgetMessage"),*widget=fv_object_property(ring,"MessageWidget");
    if(!pv_alive(component) || !cinema_object(component,cu_component_class(),0x519)
        || *(void* const*)((const char*)component+0x20)!=ring || *(void* const*)((const char*)component+0xa0)!=ring
        || !pv_alive(widget) || !cinema_object(widget,cu_user_class(),0x230)
        || !cinema_ui_named(widget,"MercRing_Message_C",0x230) || cu_get_widget(component)!=widget
        || *(const unsigned char*)((const char*)component+0x429)!=0) {
        touch_merc_ui_cleanup();return;
    }
    void* animation=fv_object_property(widget,"EndMessage");
    unsigned flags=*(const unsigned short*)((const char*)component+0x14c);
    if(!pv_alive(animation) || !cinema_object(animation,merc_ui_animation_class(),0x350)
        || (flags&0x400) || !merc_ui_playing(widget,animation)) {
        touch_merc_ui_cleanup();return;
    }
    cu_weak_assign(&merc_ui_owned.pawn,pawn);cu_weak_assign(&merc_ui_owned.ui,ui);
    cu_weak_assign(&merc_ui_owned.ring,ring);cu_weak_assign(&merc_ui_owned.component,component);
    cu_weak_assign(&merc_ui_owned.widget,widget);cu_weak_assign(&merc_ui_owned.animation,animation);
    merc_ui_owned.used=1;
    if(flags&0x20)return;
    // Recheck current weak identities and exact references before the single original visibility call.
    if(cu_weak_get(&merc_ui_owned.pawn)!=phone_active() || phone_type()!=1 || rl_paused()
        || cu_weak_get(&merc_ui_owned.ui)!=*(void* const*)(merc_ui_base+0xa579328)
        || cu_weak_get(&merc_ui_owned.ring)!=fv_object_property(ui,"MercRing")
        || cu_weak_get(&merc_ui_owned.component)!=fv_object_property(ring,"WidgetMessage")
        || cu_weak_get(&merc_ui_owned.widget)!=cu_get_widget(component)
        || cu_weak_get(&merc_ui_owned.animation)!=fv_object_property(widget,"EndMessage")) {
        touch_merc_ui_cleanup();return;
    }
    mv_visible(component,1,0);
#ifdef RE4_QA_USER_TRANSITIONS
    static unsigned logs;
    if(logs<12) {
        logs++;LOGF("MERC UI original ending visibility ui=%p ring=%p component=%p widget=%p animation=%p before=%04x after=%04x",
            ui,ring,component,widget,animation,flags,*(const unsigned short*)((const char*)component+0x14c));
    }
#endif
}
static void install_touch_merc_ui(uintptr_t base) {
    static const struct {uintptr_t address;unsigned words[4];} checks[]={
        {0x631c5b4,{0xd0020248,0xf9427108,0xf9400108,0xb4000268}},
        {0x648a5ac,{0xd10183ff,0xf90023f3,0xa9057bfd,0x910143fd}},
        {0x7e3ffe8,{0xd10183ff,0xf90023f3,0xa9057bfd,0x910143fd}},
        {0x61d177c,{0x90020ca8,0xf9446508,0xf9400108,0xb40003a8}},
        {0x5d62490,{0x91002000,0x1406c564,0,0}},
        {0x7dd4fbc,{0xa9bf7bfd,0x910003fd,0xb4000181,0xb9818008}},
        {0x83ad964,{0xd104c3ff,0xf90073fc,0xa90f5ff8,0xa91057f6}},
        {0x7db1a28,{0xf9424c00,0xd65f03c0,0,0}},
        {0x88945d8,{0xd10283ff,0xa90757f6,0xa9084ff4,0xa9097bfd}},
    };
    touch_merc_ui_cleanup();merc_ui_base=0;
    if(!base || !cu_component_class || !cu_user_class || !comfort_player_class || !cu_get_widget || !mv_visible
        || !cu_weak_assign || !cu_weak_get || !phone_active || !phone_type || !phone_inspect_ready || !fv_title_actor
        || !rl_paused || !rl_player_valid)return;
    for(unsigned i=0;i<sizeof checks/sizeof *checks;i++) {
        unsigned count=checks[i].words[2]?4:2;
        if(memcmp((const void*)(base+checks[i].address),checks[i].words,count*4))return;
    }
    if(cu_get_widget!=(void*(*)(const void*))(base+0x7db1a28)
        || mv_visible!=(void(*)(void*,int,int))(base+0x83ad964)
        || fv_title_actor!=(void*(*)(const void*,void*))(base+0x88945d8)
        || *(const unsigned*)(base+0x9f77968+0x24)!=0x498)return;
    merc_ui_has_instance=(int(*)(void))(base+0x631c5b4);
    merc_ui_class=(void*(*)(void))(base+0x648a5ac);
    merc_ui_animation_class=(void*(*)(void))(base+0x7e3ffe8);
    merc_ui_is_merc=(int(*)(void))(base+0x61d177c);
    merc_ui_frame=(int(*)(void*))(base+0x5d62490);
    merc_ui_playing=(int(*)(const void*,const void*))(base+0x7dd4fbc);
    merc_ui_base=base;
}
#endif
