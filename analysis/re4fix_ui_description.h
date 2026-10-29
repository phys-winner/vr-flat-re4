// Reuse the live localized options description or reset question in the left viewport panel.
#ifndef RE4FIX_UI_DESCRIPTION_H
#define RE4FIX_UI_DESCRIPTION_H
static struct {
    struct cinema_ui_weak screen,panel,component,widget;
    struct cinema_ui_vec2 scale,pivot,position,size,alignment;
    struct cinema_ui_anchors anchors;
    unsigned char visibility;
    int saved,added,hidden,width,height,confirmation;
    float dpi;
} fv_description = {{-1,0},{-1,0},{-1,0},{-1,0}};
static int fv_description_ready;
static unsigned fv_description_logs;
static int fv_description_prepare(void) {
    if (fv_description_ready) return 1;
    if (!g_ue4_base_cached || !__atomic_load_n(&cu_ready,__ATOMIC_ACQUIRE) || !phone_inspect_ready
        || !mv_visible || !wt_set_visibility || !cu_weak_get || !cu_weak_assign) return 0;
    static const unsigned preimage[]={0xd10183ff,0xf90013f7,0xa90357f6,0xa9044ff4};
    if (wt_set_visibility!=(void (*)(void*,unsigned char))(g_ue4_base_cached+0x7da4280)
        || mv_visible!=(void (*)(void*,int,int))(g_ue4_base_cached+0x83ad964)
        || memcmp((const void*)(g_ue4_base_cached+0x7da4280),preimage,sizeof preimage)
        || *(const unsigned*)(g_ue4_base_cached+0x9f6ff58+0x24)!=0xc3) return 0;
    fv_description_ready=1;return 1;
}
static int fv_description_widget(void* widget,int confirmation) {
    return widget && pv_alive(widget) && cinema_object(widget,cu_user_class(),0x230)
        && cinema_ui_named(widget,confirmation ? "UI_MainMenu_Options_Confirm_Header_Widget_C" : "UI_MainMenu_Info_Widget_C",0x230);
}
static void fv_description_restore(void) {
    if (!cu_weak_get || !cu_weak_assign) return;
    void* widget=cu_weak_get(&fv_description.widget);
    if (fv_description.saved && fv_description_widget(widget,fv_description.confirmation)) {
        if (fv_description.added && cu_in_viewport(widget)) cu_remove(widget);
        cu_set_scale(widget,fv_description.scale);cu_set_pivot(widget,fv_description.pivot);
        cu_set_position(widget,fv_description.position,0);cu_set_size(widget,fv_description.size);
        cu_set_alignment(widget,fv_description.alignment);cu_set_anchors(widget,fv_description.anchors);
        if (*(const unsigned char*)((const char*)widget+0xc3)==3)
            wt_set_visibility(widget,fv_description.visibility);
    }
    void* component=cu_weak_get(&fv_description.component),*panel=cu_weak_get(&fv_description.panel);
    if (fv_description.hidden && component && panel && pv_alive(component) && pv_alive(panel)
        && cinema_ui_named(panel,"DoorDescriptionPanel_BP_C",0x270)
        && cinema_object(component,cu_component_class(),0x4a0)
        && *(void* const*)((const char*)component+0x20)==panel
        && *(void* const*)((const char*)component+0xa0)==panel
        && cinema_ui_property(panel,"DoorDescriptionPanel_BP_C","Description_Widget",0x258)==component
        && !(*(const unsigned short*)((const char*)component+0x14c)&0x20)) mv_visible(component,1,0);
    fv_description.screen=fv_description.panel=fv_description.component=fv_description.widget=(struct cinema_ui_weak){-1,0};
    fv_description.saved=fv_description.added=fv_description.hidden=fv_description.width=fv_description.height=fv_description.confirmation=0;
}
static int fv_description_layout(void* widget,void* component) {
    int width=__atomic_load_n(&g_comp_w,__ATOMIC_ACQUIRE),height=__atomic_load_n(&g_comp_h,__ATOMIC_ACQUIRE);
    struct cinema_ui_vec2 draw=cu_draw_size(component);
    float dpi=cu_viewport_scale(widget);
    if (height<64 || width<height || width>16384 || height>16384 || (float)width/height>4
        || !isfinite(dpi) || dpi<0.25f || dpi>4 || !isfinite(draw.x) || !isfinite(draw.y)
        || draw.x<16 || draw.x>16384 || draw.y<16 || draw.y>16384) return 0;
    float fit=fminf(width*0.20f/(dpi*draw.x),height*0.70f/(dpi*draw.y));
    if (!isfinite(fit) || fit<0.01f || fit>32) return 0;
    if (!fv_description.saved) {
        memcpy(&fv_description.scale,(const char*)widget+0x98,8);
        memcpy(&fv_description.pivot,(const char*)widget+0xac,8);
        memcpy(&fv_description.anchors,(const char*)widget+0x1e0,16);
        memcpy(&fv_description.position,(const char*)widget+0x1f0,8);
        memcpy(&fv_description.size,(const char*)widget+0x1f8,8);
        memcpy(&fv_description.alignment,(const char*)widget+0x200,8);
        fv_description.visibility=*(const unsigned char*)((const char*)widget+0xc3);
        if (fv_description.visibility>4) return 0;
        fv_description.saved=1;
    }
    // Preserve the authored canvas and wrapping; the left door stays clear of the list and top tabs.
    cu_set_scale(widget,(struct cinema_ui_vec2){fit,fit});
    cu_set_pivot(widget,(struct cinema_ui_vec2){0,0});
    cu_set_position(widget,(struct cinema_ui_vec2){width*0.01f,height*0.23f},1);
    cu_set_size(widget,draw);cu_set_alignment(widget,(struct cinema_ui_vec2){0,0});
    cu_set_anchors(widget,(struct cinema_ui_anchors){0,0,0,0});
    wt_set_visibility(widget,3); // ESlateVisibility::HitTestInvisible includes the whole description tree.
    fv_description.width=width;fv_description.height=height;fv_description.dpi=dpi;
    return 1;
}
static void fv_description_tick(void* screen) {
    if (!cinema_on_game_thread() || !fv_description_prepare()) return;
    if (!screen || !pv_alive(screen) || !cinema_ui_named(screen,"OptionsUIScreen_BP_C",0x440)
        || (*(const unsigned*)((const char*)screen+0x58)&0x20)) {fv_description_restore();return;}
    void* owner=fv_object_property(screen,"TrifoldBaseBP");
    if (!owner || !pv_alive(owner) || (!cinema_ui_named(owner,"UI_PauseMenu_Content_Options_BP_C",0x3c0)
                  && !cinema_ui_named(owner,"UI_PauseMenu_Content_GameOptions_BP_C",0x3c0))
        || fv_object_property(owner,"UIScreen")!=screen) {fv_description_restore();return;}
    void* panel=0,*component=0,*widget=0;int confirmation=0;
    void* footer=fv_object_property(owner,"DoorRightScreen");
    if (footer && pv_alive(footer) && cinema_ui_named(footer,"OptionsConfirmScreen_BP_C",0x260)
        && !(*(const unsigned*)((const char*)footer+0x58)&0x20)) {
        void* header=cinema_ui_property(footer,"OptionsConfirmScreen_BP_C","Header Widget",0x258);
        if (header && pv_alive(header) && cinema_object(header,cu_component_class(),0x4a0)
            && *(void* const*)((const char*)header+0x20)==footer
            && *(void* const*)((const char*)header+0xa0)==footer
            && (*(const unsigned short*)((const char*)header+0x14c)&0x420)==0x20) {
            void* question=cu_get_widget(header);
            if (fv_description_widget(question,1)) {panel=footer;component=header;widget=question;confirmation=1;}
        }
    }
    if (!confirmation) {
        panel=cinema_ui_property(screen,"OptionsUIScreen_BP_C","DescriptionPanel",0x438);
        if (!panel || !pv_alive(panel) || !cinema_ui_named(panel,"DoorDescriptionPanel_BP_C",0x270)
            || fv_object_property(owner,"DoorLeftScreen")!=panel
            || (*(const unsigned*)((const char*)panel+0x58)&0x20)) {fv_description_restore();return;}
        component=cinema_ui_property(panel,"DoorDescriptionPanel_BP_C","Description_Widget",0x258);
        widget=cinema_ui_property(panel,"DoorDescriptionPanel_BP_C","DescriptionWidget",0x268);
    }
    if (!component || !pv_alive(component) || !cinema_object(component,cu_component_class(),0x4a0)
        || *(void* const*)((const char*)component+0x20)!=panel
        || *(void* const*)((const char*)component+0xa0)!=panel || cu_get_widget(component)!=widget
        || !fv_description_widget(widget,confirmation)) {fv_description_restore();return;}
    unsigned flags=*(const unsigned short*)((const char*)component+0x14c);
    unsigned char visibility=*(const unsigned char*)((const char*)widget+0xc3);
    if ((flags&0x400) || visibility==1 || visibility==2 || visibility>4
        || (!(flags&0x20) && !(fv_description.hidden && cu_weak_get(&fv_description.component)==component))) {
        fv_description_restore();return;
    }
    if (cu_weak_get(&fv_description.screen)!=screen || cu_weak_get(&fv_description.widget)!=widget
        || cu_weak_get(&fv_description.component)!=component) {
        fv_description_restore();
        if (cu_in_viewport(widget)) return;
        cu_weak_assign(&fv_description.screen,screen);cu_weak_assign(&fv_description.panel,panel);
        cu_weak_assign(&fv_description.component,component);cu_weak_assign(&fv_description.widget,widget);
        fv_description.confirmation=confirmation;
    }
    if (fv_description.added && !cu_in_viewport(widget)) {fv_description_restore();return;}
    if (!fv_description.added || fv_description.width!=g_comp_w || fv_description.height!=g_comp_h
        || fv_description.dpi!=cu_viewport_scale(widget)) {
        if (!fv_description_layout(widget,component)) {fv_description_restore();return;}
    }
    if (!fv_description.added) {
        cu_add(widget,9);fv_description.added=cu_in_viewport(widget)!=0;
        if (!fv_description.added) {fv_description_restore();return;}
        if (fv_description_logs<8) {fv_description_logs++;LOGF("UIDESCRIPTION live localized widget=%p side panel; touch passes through",widget);}
    }
    // Native Header visibility ends the question on Yes/No; leave that component's bit authoritative.
    if (!confirmation && (*(const unsigned short*)((const char*)component+0x14c)&0x20)) {mv_visible(component,0,0);fv_description.hidden=1;}
}
#endif
