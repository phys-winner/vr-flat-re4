// Fit pause options by their fixed authored backing mesh, preserving every attached tab/scroll/footer.
#ifndef RE4FIX_UI_CONSOLE_H
#define RE4FIX_UI_CONSOLE_H
static struct {
    struct cinema_ui_weak owner,root,parent;
    struct reload_tf original;
    int used;
} fv_console;
static int fv_console_basis(void* screen,qf_t* basis) {
    if(!screen || !fv_button_class || !basis)return 0;
    void* root=*(void* const*)((const char*)screen+0x130);
    if(!fv_frame_owned(root,screen))return 0;
    struct {void** data;int count,max;} list;memcpy(&list,(const char*)screen+0x238,sizeof list);
    if(list.count<1 || list.count>512 || list.max<list.count || !list.data)return 0;
    for(int i=0;i<list.count;i++) {
        void* part=list.data[i];struct reload_tf tf;
        if(!pv_alive(part) || !menu_visual_layout(part,fv_button_class(),0x320)
            || *(void* const*)((const char*)part+0x308)!=screen || !fv_descends_from(part,root))continue;
        memcpy(&tf,(const char*)part+0x1c0,sizeof tf);
        if(!reload_marker_valid(&tf))continue;
        *basis=tf.q;return 1;
    }
    return 0;
}
static void fv_console_restore(void) {
    if(!fv_console.used || !cu_weak_get || !fv_set_relative)return;
    void* owner=cu_weak_get(&fv_console.owner),*root=cu_weak_get(&fv_console.root);
    if(fv_frame_owned(root,owner) && *(void* const*)((const char*)root+0xc0)==cu_weak_get(&fv_console.parent))
        fv_set_relative(root,&fv_console.original,0,0,0);
    memset(&fv_console,0,sizeof fv_console);
}
static int fv_console_tick(void* screen,const struct reload_tf* camera) {
    void* options=screen;
    if(screen && cinema_ui_named(screen,"OptionsCoverScreen_BP_C",0x2f2))options=fv_object_property(screen,"OptionsUIScreenBP");
    if(!options || !pv_alive(options) || !cinema_ui_named(options,"OptionsUIScreen_BP_C",0x440)) {fv_console_restore();return 0;}
    void* owner=fv_object_property(options,"TrifoldBaseBP");
    if(!pv_alive(owner) || (!cinema_ui_named(owner,"UI_PauseMenu_Content_Options_BP_C",0x3c0)
        && !cinema_ui_named(owner,"UI_PauseMenu_Content_GameOptions_BP_C",0x3c0)
        && !cinema_ui_named(owner,"UI_PauseMenu_Content_NewGame_BP_C",0x3c0))
        || fv_object_property(owner,"UIScreen")!=options || !camera || !reload_tf_valid(camera)
        || !cu_weak_get || !cu_weak_assign || !fv_relative_transform || !fv_set_relative || !rl_world_transform
        || !mv_static_class) {fv_console_restore();return 0;}
    void* root=*(void* const*)((const char*)owner+0x130),*background=fv_object_property(owner,"Background_Plane");
    if(!fv_frame_owned(root,owner) || !pv_alive(background) || !menu_visual_layout(background,mv_static_class(),0x448)
        || *(void* const*)((const char*)background+0xa0)!=owner || !fv_descends_from(background,root))return 0;
    if(fv_console.used && (cu_weak_get(&fv_console.owner)!=owner || cu_weak_get(&fv_console.root)!=root))fv_console_restore();
    struct reload_tf current,mesh,framed;float origin[3],extent[3];struct fv_plane plane,upright;qf_t basis;
    memcpy(&current,(const char*)root+0x1c0,sizeof current);memcpy(&mesh,(const char*)background+0x1c0,sizeof mesh);
    if(!cinema_movie_screen_bounds(background,origin,extent) || !fv_mesh_plane(&mesh,origin,extent,&plane)
        || !fv_console_basis(options,&basis) || !fv_plane_basis(&plane,basis,&upright))return 0;
    float aspect=g_comp_h ? (float)g_comp_w/g_comp_h : 0;
    if(!fv_frame_transform(camera,&current,&upright,1,aspect,g_fov_h_deg,&framed))return 0;
    void* parent=*(void* const*)((const char*)root+0xc0);
    if(fv_console.used && parent!=cu_weak_get(&fv_console.parent)) {fv_console_restore();return 0;}
    if(!fv_console.used) {
        if(parent && (!pv_alive(parent) || !menu_visual_layout(parent,mv_scene_class(),0x1f0)))return 0;
        struct reload_tf original=fv_relative_transform(root);if(!reload_marker_valid(&original))return 0;
        fv_frame_restore();fv_console.original=original;
        cu_weak_assign(&fv_console.owner,owner);cu_weak_assign(&fv_console.root,root);
        fv_console.parent=(struct cinema_ui_weak){-1,0};if(parent)cu_weak_assign(&fv_console.parent,parent);
        fv_console.used=1;
        LOGF("UICONSOLE fixed authored backing fitted owner=%p screen=%p",owner,options);
    }
    rl_world_transform(root,&framed,0,0,0);
    void* screen_root=*(void* const*)((const char*)options+0x130);
    if(fv_screen_moved && fv_frame_owned(screen_root,options))fv_screen_moved(options,screen_root);
    return 1;
}
static struct {
    struct cinema_ui_weak pawn,screen,root,surface;
    long stamp;
    float fov;
    int used;
} fv_camera_menu;
static void* fv_camera_menu_surface(void* screen) {
    if(!pv_alive(screen) || !cinema_ui_named(screen,"DeathMenu_BP_C",0x498)
        || !fv_button_class || !fv_ui_enabled || !mv_static_class)return 0;
    void* root=*(void* const*)((const char*)screen+0x130);
    void* surface=cinema_ui_property(screen,"DeathMenu_BP_C","Screen",0x318);
    if(!fv_frame_owned(root,screen) || !pv_alive(surface)
        || !menu_visual_layout(surface,mv_static_class(),0x448)
        || *(void* const*)((const char*)surface+0x20)!=screen
        || *(void* const*)((const char*)surface+0xa0)!=screen || !fv_descends_from(surface,root))return 0;
    struct {void**data;int count,max;} controls;memcpy(&controls,(const char*)screen+0x238,sizeof controls);
    if(controls.count<1 || controls.count>16 || controls.max<controls.count || controls.max>64 || !controls.data)return 0;
    unsigned buttons=0;
    for(int i=0;i<controls.count;i++) {
        void* part=controls.data[i];
        if(!pv_alive(part) || !menu_visual_layout(part,fv_button_class(),0x320) || !fv_ui_enabled(part)
            || *(void* const*)((const char*)part+0x308)!=screen)continue;
        void* owner=*(void* const*)((const char*)part+0xa0);
        if(!pv_alive(owner) || !cinema_ui_named(owner,"Button_Death_C",0x220)
            || *(void* const*)((const char*)part+0x20)!=owner || fv_object_property(owner,"ParentMenu")!=screen)return 0;
        buttons++;
    }
    return buttons ? surface:0;
}
// Original DeathMenu spawns Button_Death actors with ParentMenu=Self without attaching their roots.
// Keep those native roots and their animation untouched; use the shared HMD pose for both view and pointer.
static int fv_camera_menu_tick(void* screen,const struct reload_tf* camera,long now) {
    fv_camera_menu.used=0;
    if(!camera || now<=0 || !cinema_on_game_thread() || !cu_weak_assign || !cu_weak_get || !phone_type || phone_type()!=4
        || !phone_active || !mv_pawn_class || !cinema_camera_class
        || !fv_ui_manager || !fv_peek_screen)return 0;
    void* manager=fv_ui_manager();if(!manager || fv_peek_screen(manager)!=screen)return 0;
    void* pawn=phone_active(),*surface=fv_camera_menu_surface(screen);
    if(!surface || !pv_alive(pawn) || !menu_visual_layout(pawn,mv_pawn_class(),0xbb0))return 0;
    void* owned_camera=*(void* const*)((const char*)pawn+0x2d8);
    if(!pv_alive(owned_camera) || !cinema_object(owned_camera,cinema_camera_class(),0x7b8)
        || *(void* const*)((const char*)owned_camera+0xa0)!=pawn)return 0;
    float origin[3],extent[3],fov,yaw,pitch,current_yaw,current_pitch;
    struct reload_tf mesh,desired;memcpy(&mesh,(const char*)surface+0x1c0,sizeof mesh);
    float aspect=g_comp_h ? (float)g_comp_w/g_comp_h:0;
    if(!cinema_movie_screen_bounds(surface,origin,extent)
        || !fv_bounds_camera(camera,&mesh,origin,extent,aspect,&desired,&fov))return 0;
    head_angles_now(&current_yaw,&current_pitch);
    if(!fv_camera_head(camera,&desired,current_yaw,current_pitch,&yaw,&pitch))return 0;
    cu_weak_assign(&fv_camera_menu.pawn,pawn);cu_weak_assign(&fv_camera_menu.screen,screen);
    cu_weak_assign(&fv_camera_menu.root,*(void* const*)((const char*)screen+0x130));cu_weak_assign(&fv_camera_menu.surface,surface);
    fv_camera_menu.stamp=now;fv_camera_menu.fov=fov;fv_camera_menu.used=1;
    g_menu_yaw_off=yaw;g_menu_pitch=pitch;g_menu_view_ms=now;g_menu_view_active=1;
    static unsigned logs;
    if(logs<4){logs++;LOGF("UICAMERA detached authored menu=%p pawn=%p yaw=%.2f pitch=%.2f fov=%.2f roots unchanged",screen,pawn,yaw*57.2957795f,pitch*57.2957795f,fov);}
    return 1;
}
// Keep the authored console zoom only in native menu contexts and on the active pawn camera.
static void fv_menu_camera_view(void* camera, void* view) {
    if (!view || !camera || !fv_ui_manager || !fv_button_class || !fv_ui_enabled || !phone_active
        || !phone_type || !cinema_on_game_thread()) return;
    unsigned type = phone_type();
    long now = ptr_now_ms();
    if(type==4 && fv_camera_menu.used && now>=fv_camera_menu.stamp && now-fv_camera_menu.stamp<300
        && cu_weak_get && fv_peek_screen && cinema_camera_class) {
        void* pawn=cu_weak_get(&fv_camera_menu.pawn),*screen=cu_weak_get(&fv_camera_menu.screen);
        void* surface=cu_weak_get(&fv_camera_menu.surface),*manager=fv_ui_manager();
        if(pawn && pawn==phone_active() && manager && fv_peek_screen(manager)==screen
            && fv_camera_menu_surface(screen)==surface && surface
            && fv_frame_owned(cu_weak_get(&fv_camera_menu.root),screen)
            && *(void* const*)((const char*)pawn+0x2d8)==camera && pv_alive(camera)
            && cinema_object(camera,cinema_camera_class(),0x7b8) && *(void* const*)((const char*)camera+0xa0)==pawn) {
            memcpy((char*)view+0x18,&fv_camera_menu.fov,4);g_fov_h_deg=fv_camera_menu.fov;return;
        }
    }
    if ((type != 2 && type != 4 && type != 7) || fv_lobby_zoom_ms <= 0
        || now < fv_lobby_zoom_ms || now - fv_lobby_zoom_ms >= 300) return;
    void* pawn = phone_active();
    if (!pawn || *(void* const*)((const char*)pawn + 0x2d8) != camera) return;
    float authored; memcpy(&authored, (const char*)view + 0x18, 4);
    float fov = fv_console_fov(authored);
    if (!isfinite(fov)) return;
    memcpy((char*)view + 0x18, &fov, 4);
    g_fov_h_deg = fov;
    static unsigned logs;
    if (logs < 4) { logs++; LOGF("UIFOV authored=%.2f applied=%.2f fixedConsole=1", authored, fov); }
}
#endif
