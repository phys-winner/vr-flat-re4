// Optional Start confirmation retains the original native Click delegate and its exact weak target.
#ifndef RE4FIX_UI_START_H
#define RE4FIX_UI_START_H
unsigned long g_tgt_ui_start_click;
#ifndef RE4FIX_UI_START_GATE
__attribute__((naked)) static void gate_ui_start_click(void* self,const struct fv_ui_object* object,const struct fv_ui_click_state* state) {
    asm volatile(".inst 0xd101c3ff\n" ".inst 0xa90457f6\n" ".inst 0xa9054ff4\n" ".inst 0xa9067bfd\n"
        "adrp x16, :got:g_tgt_ui_start_click\n" "ldr x16, [x16, :got_lo12:g_tgt_ui_start_click]\n"
        "ldr x16, [x16]\n" "br x16\n");
}
#else
#define gate_ui_start_click RE4FIX_UI_START_GATE
#endif
static struct {
    struct cinema_ui_weak pawn,top,screen,component,actor,hand;
    struct fv_ui_click_state state;
    int used,has_hand;
} fv_start_pending;
static int fv_start_hook_ready;
static void* fv_current_screen(void) {
    void* manager=fv_ui_manager ? fv_ui_manager() : 0;
    return manager && fv_peek_screen ? fv_peek_screen(manager) : 0;
}
static int fv_start_target(void* component,void* screen,void* top) {
    if (!fv_button_class || !fv_ui_enabled || !pv_alive(component) || !pv_alive(screen) || !pv_alive(top)
        || !menu_visual_layout(component,fv_button_class(),0x320) || !fv_ui_enabled(component)
        || *(void* const*)((const char*)component+0x308)!=screen) return 0;
    char name[96];obj_name(component,name,sizeof name);
    if (cinema_ui_named(screen,"NewGameConfirmScreen_BP_C",0x250)) {
        void* owner=fv_object_property(top,"TrifoldBaseBP");
        return !strcmp(name,"VR4UIButton_Confirm") && *(void* const*)((const char*)component+0x20)==screen
            && owner && cinema_ui_named(owner,"UI_PauseMenu_Content_NewGame_BP_C",0x3c0)
            && fv_object_property(owner,"UIScreen")==top && fv_object_property(owner,"DoorRightScreen")==screen;
    }
    if (top==screen && cinema_ui_named(screen,"ShootingRange_Configuration_Screen_BP_C",0x248)) {
        void* actor=*(void* const*)((const char*)component+0x20);
        if (!pv_alive(actor) || !cinema_ui_named(actor,"ShootingRange_Panel_Button_BP_C",0x220)) return 0;
        void* child=fv_object_property(screen,"StartButton");
        if(!pv_child_class || !pv_alive(child) || !menu_visual_layout(child,pv_child_class(),0x220)
            || *(void* const*)((const char*)child+0x20)!=screen || *(void* const*)((const char*)child+0xa0)!=screen
            || *(void* const*)((const char*)child+0x1f8)!=actor)return 0;
        obj_name(actor,name,sizeof name);
        return !strncmp(name,"StartButton_",12);
    }
    return 0;
}
static void fv_start_cancel(void) {
    if (fv_start_pending.used) mobile_android_confirm_cancel_kind(7);
    memset(&fv_start_pending,0,sizeof fv_start_pending);
}
static int fv_start_state_valid(const struct fv_ui_click_state* state) {
    if (!state || state->action || state->count<1 || state->count>1024
        || !isfinite(state->press_time) || !isfinite(state->release_time)) return 0;
    for(unsigned i=0;i<3;i++) if (!isfinite(state->press[i]) || !isfinite(state->release[i])) return 0;
    return !state->hand || pv_alive(state->hand);
}
static void hook_ui_start_click(void* self,const struct fv_ui_object* object,const struct fv_ui_click_state* state) {
    if (!cinema_on_game_thread() || !mobile_prefs_start_confirm() || !object || object->component!=self
        || !fv_start_target(self,object->screen,fv_current_screen())) {
        gate_ui_start_click(self,object,state);return;
    }
    // A pending dialog or unavailable confirmation transport never turns an explicit Start into an unconfirmed click.
    if (fv_start_pending.used || !fv_start_state_valid(state) || !phone_active || !pv_alive(phone_active())
        || object->actor!=*(void* const*)((const char*)self+0x20) || !pv_alive(object->actor)) return;
    const char* language=mobile_android_language();
    if (!mobile_android_confirm_request(7,language && !strncmp(language,"ru",2),ptr_now_ms())) return;
    cu_weak_assign(&fv_start_pending.pawn,phone_active());cu_weak_assign(&fv_start_pending.top,fv_current_screen());
    cu_weak_assign(&fv_start_pending.screen,object->screen);cu_weak_assign(&fv_start_pending.component,self);
    cu_weak_assign(&fv_start_pending.actor,object->actor);
    fv_start_pending.has_hand=state->hand!=0;
    if (state->hand) cu_weak_assign(&fv_start_pending.hand,state->hand);
    fv_start_pending.state=*state;fv_start_pending.state.hand=0;fv_start_pending.used=1;
}
static void fv_start_confirm_tick(long now) {
    if (!cinema_on_game_thread() || !fv_start_pending.used) return;
    if (!mobile_android_confirm_active(7)) {fv_start_cancel();return;}
    void* pawn=cu_weak_get(&fv_start_pending.pawn),*top=cu_weak_get(&fv_start_pending.top);
    void* screen=cu_weak_get(&fv_start_pending.screen),*component=cu_weak_get(&fv_start_pending.component);
    void* actor=cu_weak_get(&fv_start_pending.actor),*hand=fv_start_pending.has_hand ? cu_weak_get(&fv_start_pending.hand) : 0;
    if (!pawn || !phone_active || pawn!=phone_active() || !top || top!=fv_current_screen()
        || !fv_start_target(component,screen,top) || !pv_alive(actor)
        || actor!=*(void* const*)((const char*)component+0x20) || (fv_start_pending.has_hand && !hand)) {
        fv_start_cancel();return;
    }
    int answer=mobile_android_confirm_take(7,now);
    if (!answer) return;
    struct fv_ui_click_state state=fv_start_pending.state;state.hand=hand;
    memset(&fv_start_pending,0,sizeof fv_start_pending);
    if (answer>0) {
        struct fv_ui_object object={actor,component,screen};
        gate_ui_start_click(component,&object,&state);
        LOGF("UISTART confirmed original native Start component=%p screen=%p",component,screen);
    }
}
static void install_ui_start(uintptr_t base) {
    static const unsigned preimage[]={0xd101c3ff,0xa90457f6,0xa9054ff4,0xa9067bfd};
    if (!base || fv_start_hook_ready || !fv_button_class || !fv_ui_enabled || !cu_weak_get || !cu_weak_assign
        || memcmp((const void*)(base+0x6371bcc),preimage,sizeof preimage)) return;
    g_tgt_ui_start_click=base+0x6371bcc+16;
    if (patch_code((void*)(base+0x6371bcc),(void*)&hook_ui_start_click)==0) fv_start_hook_ready=1;
    LOGF("UISTART native Click confirmation hook=%d",fv_start_hook_ready);
}
#endif
