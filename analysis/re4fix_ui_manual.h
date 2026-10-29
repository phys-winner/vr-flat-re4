// The native Manual keeps category, article and text on sibling actors; fit their original roots together.
#ifndef RE4FIX_UI_MANUAL_H
#define RE4FIX_UI_MANUAL_H
static struct cinema_ui_weak fv_manual_owner={-1,0};
static struct {
    struct cinema_ui_weak actor,root,parent;
    struct reload_tf original;
    int changed;
} fv_manual[3]={{{-1,0},{-1,0},{-1,0}},{{-1,0},{-1,0},{-1,0}},{{-1,0},{-1,0},{-1,0}}};
static const struct fv_region fv_manual_regions[3]={{.01f,.07f,.27f,.86f},{.30f,.07f,.49f,.86f},{.80f,.07f,.19f,.86f}};
static void fv_manual_restore(void) {
    if (!cu_weak_get || !fv_set_relative) return;
    void* owner=cu_weak_get(&fv_manual_owner);
    for (unsigned i=0;i<3;i++) {
        void* actor=cu_weak_get(&fv_manual[i].actor),*root=cu_weak_get(&fv_manual[i].root);
        if (fv_manual[i].changed && owner && pv_alive(owner) && fv_frame_owned(root,actor)
            && *(void* const*)((const char*)actor+0xe0)==owner
            && *(void* const*)((const char*)root+0xc0)==cu_weak_get(&fv_manual[i].parent)) {
            fv_set_relative(root,&fv_manual[i].original,0,0,0);
            if (fv_screen_moved) fv_screen_moved(actor,root);
        }
        fv_manual[i].actor=fv_manual[i].root=fv_manual[i].parent=(struct cinema_ui_weak){-1,0};
        fv_manual[i].changed=0;
    }
    fv_manual_owner=(struct cinema_ui_weak){-1,0};
}
static int fv_manual_tick(void* screen,const struct reload_tf* camera) {
    static const char* classes[3]={"PlayingManualEntrySelection_C","PlayingManualMain_C","PlayingManualInfoText_C"};
    static const int sizes[3]={0x2e7,0x3b0,0x299};
    if (!screen || !pv_alive(screen) || (!cinema_ui_named(screen,classes[0],sizes[0])
        && !cinema_ui_named(screen,classes[1],sizes[1]) && !cinema_ui_named(screen,classes[2],sizes[2]))) {
        fv_manual_restore();return 0;
    }
    if (!camera || !reload_tf_valid(camera) || !fv_button_class || !fv_ui_enabled || !fv_widget_planes
        || !cu_weak_get || !cu_weak_assign || !cu_component_class || !cu_get_widget
        || !rl_world_transform || !fv_relative_transform || !fv_set_relative) {fv_manual_restore();return 0;}
    void* owner=*(void* const*)((const char*)screen+0xe0);
    if (!owner || !pv_alive(owner) || !cinema_ui_named(owner,"UI_PauseMenu_Content_PlayingManual_BP_C",0x428)
        || (*(const unsigned*)((const char*)owner+0x58)&0x20)) {fv_manual_restore();return 0;}
    void* actors[3]={cinema_ui_property(owner,"UI_PauseMenu_Content_PlayingManual_BP_C","DoorLeftScreen",0x3a0),
                     cinema_ui_property(owner,"UI_PauseMenu_Content_PlayingManual_BP_C","UIScreen",0x370),
                     cinema_ui_property(owner,"UI_PauseMenu_Content_PlayingManual_BP_C","DoorRightScreen",0x398)};
    void* roots[3];int current=0,changed=cu_weak_get(&fv_manual_owner)!=owner;
    for (unsigned i=0;i<3;i++) {
        void* actor=actors[i];
        if (!actor || !pv_alive(actor) || !cinema_ui_named(actor,classes[i],sizes[i])
            || *(void* const*)((const char*)actor+0xe0)!=owner) {fv_manual_restore();return 0;}
        roots[i]=*(void* const*)((const char*)actor+0x130);
        if (!fv_frame_owned(roots[i],actor)) {fv_manual_restore();return 0;}
        if (actor==screen) current=1;
        if (cu_weak_get(&fv_manual[i].actor)!=actor || cu_weak_get(&fv_manual[i].root)!=roots[i]) changed=1;
    }
    if (!current || cinema_ui_property(actors[0],classes[0],"ContentParent",0x290)!=owner
        || cinema_ui_property(actors[0],classes[0],"main",0x2c0)!=actors[1]
        || cinema_ui_property(actors[0],classes[0],"InfoText",0x2b8)!=actors[2]) {fv_manual_restore();return 0;}
    for (unsigned i=1;i<3;i++) {
        void* component=cinema_ui_property(actors[i],classes[i],i==1 ? "Widget_Main" : "Widget_InfoText",0x258);
        void* widget=cinema_ui_property(actors[i],classes[i],i==1 ? "Widget" : "Info",i==1 ? 0x328 : 0x278);
        if (!component || !pv_alive(component) || !menu_visual_layout(component,cu_component_class(),0x519)
            || *(void* const*)((const char*)component+0x20)!=actors[i]
            || *(void* const*)((const char*)component+0xa0)!=actors[i] || !fv_descends_from(component,roots[i])
            || !widget || !pv_alive(widget) || cu_get_widget(component)!=widget
            || !cinema_ui_named(widget,i==1 ? "PlayingManualMainWidget_C" : "PlayingManualInfoTextWidget_C",0x230)) {
            fv_manual_restore();return 0;
        }
    }
    if (changed) {
        fv_manual_restore();fv_frame_restore();
        cu_weak_assign(&fv_manual_owner,owner);
        for (unsigned i=0;i<3;i++) {cu_weak_assign(&fv_manual[i].actor,actors[i]);cu_weak_assign(&fv_manual[i].root,roots[i]);}
    }
    struct reload_tf framed[3],original[3];void* parents[3]={0};int move[3]={0};
    float aspect=g_comp_h ? (float)g_comp_w/g_comp_h : 0;
    for (unsigned i=0;i<3;i++) {
        if (*(const unsigned*)((const char*)actors[i]+0x58)&0x20) continue;
        struct reload_tf root;memcpy(&root,(const char*)roots[i]+0x1c0,sizeof root);
        if (!reload_marker_valid(&root)) {fv_manual_restore();return 0;}
        struct fv_plane planes[64];unsigned count=fv_screen_planes(actors[i],roots[i],&root,planes,64,0);
        if (count>64) {fv_manual_restore();return 0;}
        if (!count) continue; // Native fades and an empty article remain authoritative.
        if (!fv_frame_region(camera,&root,planes,count,aspect,g_fov_h_deg,fv_manual_regions+i,framed+i)) {
            fv_manual_restore();return 0;
        }
        parents[i]=*(void* const*)((const char*)roots[i]+0xc0);
        if (fv_manual[i].changed) {
            if (parents[i]!=cu_weak_get(&fv_manual[i].parent)) {fv_manual_restore();return 0;}
        } else {
            if (parents[i] && (!pv_alive(parents[i]) || !menu_visual_layout(parents[i],mv_scene_class(),0x1f0))) {
                fv_manual_restore();return 0;
            }
            original[i]=fv_relative_transform(roots[i]);
            if (!reload_marker_valid(original+i)) {fv_manual_restore();return 0;}
        }
        move[i]=1;
    }
    for (unsigned i=0;i<3;i++) if (move[i]) {
        if (!fv_manual[i].changed) {
            fv_manual[i].original=original[i];
            if (parents[i]) cu_weak_assign(&fv_manual[i].parent,parents[i]);
            fv_manual[i].changed=1;
        }
        rl_world_transform(roots[i],framed+i,0,0,0);
        if (fv_screen_moved) fv_screen_moved(actors[i],roots[i]);
    }
    static unsigned logs;
    if (changed && logs<4) {logs++;LOGF("UIMANUAL native siblings category=%p main=%p info=%p in bounded viewport regions",actors[0],actors[1],actors[2]);}
    return 1;
}
#endif
