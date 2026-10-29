// Append the original Accept/Reset or Start/Cancel actor through the game's own scroll attachment API.
#ifndef RE4FIX_UI_FOOTER_H
#define RE4FIX_UI_FOOTER_H
static void* (*fv_scroll_class)(void);
static void (*fv_scroll_attach)(void*,void*,const struct reload_tf*), (*fv_scroll_unattach)(void*,void*);
static int (*fv_scroll_find)(void*,int*,void*);
static void (*fv_scroll_update)(void*);
static struct reload_tf (*fv_scroll_world)(const void*), (*fv_relative_transform)(const void*);
static void (*fv_set_relative)(void*,const struct reload_tf*,int,void*,int);
// Native Scroll.Intersect uses the same bounded local-X plane as a button. Its logical children are clipped
// to this visible window; including every offscreen row would make the whole menu shrink or tilt.
static int fv_scroll_plane(void* part,struct fv_plane* plane) {
    if (!part || !pv_alive(part) || !fv_scroll_class || !fv_ui_enabled(part)) return 0;
    if (!menu_visual_layout(part,fv_scroll_class(),0x430)) {
        if (!menu_visual_layout(part,fv_button_class(),0x320)) return 0;
        void* parent=*(void* const*)((const char*)part+0x310);
        if (parent && pv_alive(parent) && menu_visual_layout(parent,fv_scroll_class(),0x430)) return 0;
    }
    memcpy(&plane->transform,(const char*)part+0x1c0,sizeof plane->transform);
    memcpy(&plane->width,(const char*)part+0x318,8);
    return 1;
}
static void fv_update_scrollers(void* screen,void* root) {
    if (!fv_scroll_update || !fv_scroll_class || !fv_frame_owned(root,screen)) return;
    struct {void** data;int count,max;} list;
    memcpy(&list,(const char*)screen+0x238,sizeof list);
    if (list.count<0 || list.count>512 || list.max<list.count || (list.count&&!list.data)) return;
    for (int i=0;i<list.count;i++) {
        void* part=list.data[i];
        if (part && pv_alive(part) && menu_visual_layout(part,fv_scroll_class(),0x430)
            && *(void* const*)((const char*)part+0x308)==screen && fv_descends_from(part,root)) fv_scroll_update(part);
    }
}
static struct {
    struct cinema_ui_weak screen, footer, scroll, root, parent, row;
    struct reload_tf original;
    int rows, list_length;
} fv_footer = {{-1,0},{-1,0},{-1,0},{-1,0},{-1,0},{-1,0}};
static int fv_footer_control_visible(void* part,void* scroll) {
    if (!part || !scroll || !pv_alive(part) || !pv_alive(scroll) || !fv_ui_enabled(part)
        || !menu_visual_layout(part,fv_button_class(),0x320)
        || !menu_visual_layout(scroll,fv_scroll_class(),0x430)
        || *(void* const*)((const char*)part+0x310)!=scroll || !fv_ui_enabled(scroll)) return 0;
    struct reload_tf control,window;
    memcpy(&control,(const char*)part+0x1c0,sizeof control);
    memcpy(&window,(const char*)scroll+0x1c0,sizeof window);
    float width,height;memcpy(&width,(const char*)scroll+0x318,4);memcpy(&height,(const char*)scroll+0x31c,4);
    if (!reload_marker_valid(&control) || !reload_marker_valid(&window)
        || !isfinite(width) || !isfinite(height) || width<=0 || height<=0 || width>16384 || height>16384) return 0;
    qf_t inverse={-window.q.x,-window.q.y,-window.q.z,window.q.w};
    float delta[3],local[3];for(unsigned k=0;k<3;k++)delta[k]=control.p[k]-window.p[k];
    q_rot(inverse,delta,local);
    return fabsf(local[1])<width*window.s[1]*.5f && fabsf(local[2])<height*window.s[2]*.5f;
}
static const char* fv_scalar_property(const void* object,const char* name,const char* kind,int element) {
    if (!object || !pv_alive(object)) return 0;
    const char* cls=*(const char* const*)((const char*)object+0x10);
    int size=cls ? *(const int*)(cls+0x58) : 0;
    const char* property=cinema_field_property(object,name,kind);
    int offset=property ? *(const int*)(property+0x4c) : -1;
    return property && size>=0x220 && size<=0x10000 && offset>=0x220 && offset<=size-element
        && *(const int*)(property+0x34)==1 && *(const int*)(property+0x38)==element ? property : 0;
}
static void* fv_object_property(const void* object,const char* name) {
    const char* property=fv_scalar_property(object,name,"ObjectProperty",8);
    return property ? *(void* const*)((const char*)object+*(const int*)(property+0x4c)) : 0;
}
static void fv_footer_restore(void) {
    if (!cu_weak_get || !fv_scroll_unattach || !fv_set_relative) return;
    void* footer=cu_weak_get(&fv_footer.footer), *scroll=cu_weak_get(&fv_footer.scroll);
    void* root=cu_weak_get(&fv_footer.root), *parent=cu_weak_get(&fv_footer.parent);
    if (footer && scroll && pv_alive(footer) && pv_alive(scroll)
        && menu_visual_layout(scroll,fv_scroll_class(),0x430)) fv_scroll_unattach(scroll,footer);
    if (fv_frame_owned(root,footer) && *(void* const*)((const char*)root+0xc0)==parent)
        fv_set_relative(root,&fv_footer.original,0,0,0);
    fv_footer.screen=fv_footer.footer=fv_footer.scroll=fv_footer.root=fv_footer.parent=fv_footer.row=(struct cinema_ui_weak){-1,0};
    fv_footer.rows=fv_footer.list_length=0;
}
static void fv_footer_tick(void* screen) {
    if (!fv_scroll_attach || !fv_scroll_find || !fv_relative_transform || !fv_scroll_world || !cu_weak_get || !cu_weak_assign) return;
    if (!screen || !cinema_ui_named(screen,"OptionsUIScreen_BP_C",0x2d0)) {fv_footer_restore();return;}
    void* owner=fv_object_property(screen,"TrifoldBaseBP");
    int new_game=owner && cinema_ui_named(owner,"UI_PauseMenu_Content_NewGame_BP_C",0x3c0);
    int options=owner && (cinema_ui_named(owner,"UI_PauseMenu_Content_Options_BP_C",0x3c0)
                      || cinema_ui_named(owner,"UI_PauseMenu_Content_GameOptions_BP_C",0x3c0));
    if ((!new_game && !options)
        || fv_object_property(owner,"UIScreen")!=screen) {fv_footer_restore();return;}
    void* footer=fv_object_property(owner,"DoorRightScreen");
    const char* expected=new_game ? "NewGameConfirmScreen_BP_C" : "OptionsConfirmScreen_BP_C";
    void* menu=fv_object_property(screen,"CurrentMenu");
    if (!footer || !cinema_ui_named(footer,expected,0x250) || !menu || !pv_alive(menu)
        || !cinema_ui_named(menu,"OptionsScreen_Menu_C",0x220)) {fv_footer_restore();return;}
    void* scroll=fv_object_property(menu,"Scroll");
    const char* list_field=fv_scalar_property(screen,"ButtonList","ArrayProperty",16);
    const char* spacing_field=fv_scalar_property(screen,"EntrySpacing","FloatProperty",4);
    struct {void** data;int count,max;} list={0};float spacing=0;
    if (list_field) memcpy(&list,(const char*)screen+*(const int*)(list_field+0x4c),sizeof list);
    if (spacing_field) memcpy(&spacing,(const char*)screen+*(const int*)(spacing_field+0x4c),4);
    if (!scroll || !pv_alive(scroll) || !menu_visual_layout(scroll,fv_scroll_class(),0x430)
        || *(void* const*)((const char*)scroll+0xa0)!=menu || !list_field || !spacing_field
        || list.count<=0 || list.count>1024 || list.max<list.count || !list.data) {fv_footer_restore();return;}
    if (cu_weak_get(&fv_footer.screen)==screen && cu_weak_get(&fv_footer.footer)==footer
        && cu_weak_get(&fv_footer.scroll)==scroll && fv_footer.list_length==list.count) return;
    fv_footer_restore();
    // ButtonList retains actors from other authored submenus. Count only actual members of this Scroll.
    void* first=0;unsigned rows=0;int first_index=0x7fffffff,last_index=-1;
    for (int i=0;i<list.count;i++) {
        void* actor=list.data[i];int index=-1;
        if (!actor || !pv_alive(actor) || !cinema_ui_named(actor,"OptionsScreen_Button_C",0x220)
            || !fv_scroll_find(scroll,&index,actor) || index<0 || index>2048) continue;
        if (++rows>128) return;
        if (index>last_index) last_index=index;
        // Native Reset can hide a pooled leading actor while leaving its raw component Enabled bytes set.
        if (!(*(const unsigned*)((const char*)actor+0x58)&0x20) && index<first_index) {first=actor;first_index=index;}
    }
    if (!first || !rows) return;
    void* root=*(void* const*)((const char*)footer+0x130);
    if (!fv_frame_owned(root,footer)) return;
    struct fv_plane row={0},button={0};int found_row=0,found_button=0;void* confirm=0;
    void* screens[2]={screen,footer};
    for (unsigned s=0;s<2;s++) {
        struct {void** data;int count,max;} controls;
        memcpy(&controls,(const char*)screens[s]+0x238,sizeof controls);
        if (controls.count<1 || controls.count>512 || controls.max<controls.count || !controls.data) return;
        for (int i=0;i<controls.count;i++) {
            void* part=controls.data[i];char name[64];
            if (!part || !pv_alive(part) || !menu_visual_layout(part,fv_button_class(),0x320)
                || *(void* const*)((const char*)part+0x308)!=screens[s]) continue;
            if (s && *(void* const*)((const char*)part+0x310)) return; // only take originally unparented footer controls
            obj_name(part,name,sizeof name);
            if ((!s && *(void* const*)((const char*)part+0xa0)==first && fv_ui_enabled(part))
                || (s && !strcmp(name,"VR4UIButton_Confirm") && *(void* const*)((const char*)part+0xa0)==footer
                    && fv_descends_from(part,root))) {
                struct fv_plane* plane=s ? &button : &row;
                memcpy(&plane->transform,(const char*)part+0x1c0,sizeof plane->transform);
                memcpy(&plane->width,(const char*)part+0x318,8);
                if (s) {found_button=1;confirm=part;}else found_row=1;
                if (!s) break;
            }
        }
    }
    if (!found_row || !found_button) return;
    struct reload_tf current,relative,content=fv_scroll_world(scroll);
    memcpy(&current,(const char*)root+0x1c0,sizeof current);
    // The anchor already includes leading hidden rows; span only from its native index through the tail.
    unsigned tail_span=(unsigned)(last_index+1-first_index);
    if (!fv_footer_transform(&content,&row,&current,&button,tail_span,spacing,&relative)) return;
    void* row_root=*(void* const*)((const char*)first+0x130);
    if (fv_widget_planes && fv_frame_owned(row_root,first)) {
        struct fv_plane row_visual[16],button_visual[4];
        unsigned row_count=fv_widget_planes(row_root,row_visual,16),button_count=fv_widget_planes(confirm,button_visual,4);
        if (row_count && row_count<=16 && button_count && button_count<=4
            && !fv_footer_visual_depth(&content,&row,&button,row_visual,button_visual,&relative)) return;
    }
    struct reload_tf original=fv_relative_transform(root);
    if (!reload_marker_valid(&original)) return;
    cu_weak_assign(&fv_footer.screen,screen);cu_weak_assign(&fv_footer.footer,footer);cu_weak_assign(&fv_footer.scroll,scroll);
    cu_weak_assign(&fv_footer.row,first);
    cu_weak_assign(&fv_footer.root,root);
    void* parent=*(void* const*)((const char*)root+0xc0);
    if (parent) cu_weak_assign(&fv_footer.parent,parent);
    fv_footer.original=original;fv_footer.rows=(int)rows;fv_footer.list_length=list.count;
    fv_scroll_attach(scroll,footer,&relative);
    LOGF("UIFOOTER appended original %s after %u native rows via authored Scroll",expected,rows);
}
#endif
