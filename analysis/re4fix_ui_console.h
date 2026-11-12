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
static const void* fv_case_parent_class;
static void* fv_console_actor_parent(void* actor) {
    void* owner=*(void* const*)((const char*)actor+0xe0);
    if(owner)return owner;
    if(!cu_weak_get || !pv_child_class)return 0;
    const void* cls=*(void* const*)((const char*)actor+0x10);
    if(!cls)return 0;
    if(cls!=fv_case_parent_class) {
        const char* property=cinema_field_property(actor,"ParentComponent","WeakObjectProperty");
        if(!property || *(const int*)(property+0x4c)!=0x160 || *(const int*)(property+0x34)!=1
            || *(const int*)(property+0x38)!=sizeof(struct cinema_ui_weak))return 0;
        fv_case_parent_class=cls;
    }
    struct cinema_ui_weak weak;memcpy(&weak,(const char*)actor+0x160,sizeof weak);
    void* component=cu_weak_get(&weak);
    if(!pv_alive(component) || !menu_visual_layout(component,pv_child_class(),0x220)
        || *(void* const*)((const char*)component+0x1f8)!=actor)return 0;
    owner=*(void* const*)((const char*)component+0xa0);
    if(!pv_alive(owner) || !menu_visual_layout(owner,pv_actor_class(),0x220)
        || *(void* const*)((const char*)component+0x20)!=owner)return 0;
    void* root=*(void* const*)((const char*)actor+0x130),*parent_root=*(void* const*)((const char*)owner+0x130);
    if(!fv_frame_owned(root,actor) || *(void* const*)((const char*)root+0xc0)!=component
        || !fv_frame_owned(parent_root,owner) || !fv_descends_from(component,parent_root))return 0;
    return owner;
}
// The Merc create-game case owns active side panels; its inventory screen belongs to the grid child.
static void* fv_console_case_owner(void* screen) {
    if(!pv_actor_class || !fv_screen_class || !pv_alive(screen)
        || !menu_visual_layout(screen,fv_screen_class(),0x248))return 0;
    void* actor=screen;
    for(unsigned depth=0;depth<5;depth++) {
        if(!pv_alive(actor) || !menu_visual_layout(actor,pv_actor_class(),0x220))return 0;
        if(cinema_ui_named(actor,"UI_MercMenu_Content_CreateGame_BP_C",0x220)
            && fv_object_property(actor,"Background_Plane") && fv_object_property(actor,"Frame_Mesh_Back"))return actor;
        void* next=depth==0 ? fv_object_property(screen,"TrifoldBaseBP"):0;
        if(!next)next=fv_console_actor_parent(actor);
        if(next==actor)return 0;
        actor=next;
    }
    return 0;
}
static struct {
    struct cinema_ui_weak component;
    const void* asset;
    float origin[3],extent[3];
    int used;
} fv_case_mesh_bounds[8];
static int fv_console_case_bounds(void* component,float origin[3],float extent[3]) {
    const void* asset=*(void* const*)((const char*)component+0x440);
    if(!asset || !cu_weak_assign || !cu_weak_get)return 0;
    for(unsigned i=0;i<8;i++)if(fv_case_mesh_bounds[i].used && fv_case_mesh_bounds[i].asset==asset
        && cu_weak_get(&fv_case_mesh_bounds[i].component)==component) {
        memcpy(origin,fv_case_mesh_bounds[i].origin,12);memcpy(extent,fv_case_mesh_bounds[i].extent,12);return 1;
    }
    if(!cinema_movie_screen_bounds(component,origin,extent))return 0;
    static unsigned next;
    unsigned slot=next++%8;cu_weak_assign(&fv_case_mesh_bounds[slot].component,component);
    fv_case_mesh_bounds[slot].asset=asset;fv_case_mesh_bounds[slot].used=1;
    memcpy(fv_case_mesh_bounds[slot].origin,origin,12);memcpy(fv_case_mesh_bounds[slot].extent,extent,12);return 1;
}
struct fv_case_view {float eye[3],forward[3],right[3],up[3],aspect,tangent;};
static int fv_console_case_expand(struct fv_case_view* camera,const struct reload_tf* tf,
    const float origin[3],const float extent[3]) {
    if(!reload_marker_valid(tf))return 0;
    for(unsigned k=0;k<3;k++)if(!isfinite(origin[k]) || !isfinite(extent[k]) || extent[k]<0 || extent[k]>100000)return 0;
    for(unsigned corner=0;corner<8;corner++) {
        float local[3],world[3],depth=0,horizontal=0,vertical=0;
        for(unsigned k=0;k<3;k++)local[k]=(origin[k]+(corner&(1u<<k)?extent[k]:-extent[k]))*tf->s[k];
        q_rot(tf->q,local,world);
        for(unsigned k=0;k<3;k++) {
            world[k]+=tf->p[k]-camera->eye[k];depth+=world[k]*camera->forward[k];
            horizontal+=world[k]*camera->right[k];vertical+=world[k]*camera->up[k];
        }
        if(!isfinite(depth) || depth<=1)return 0;
        camera->tangent=fmaxf(camera->tangent,fmaxf(fabsf(horizontal)/depth,fabsf(vertical)*camera->aspect/depth));
    }
    return 1;
}
static int fv_console_case_widgets(void* root,struct fv_case_view* camera) {
    void* case_owner=*(void* const*)((const char*)root+0xa0);
    void* queue[256]={root};unsigned count=1;
    for(unsigned i=0;i<count;i++) {
        void* part=queue[i];
        if(!pv_alive(part) || !menu_visual_layout(part,mv_scene_class(),0x1f0))return 0;
        void* owner=*(void* const*)((const char*)part+0xa0);
        int screen_owner=pv_alive(owner) && menu_visual_layout(owner,fv_screen_class(),0x248);
        // Physical inventory and character actors own no panel canvases; keep their descendants out of this walk.
        if(owner!=case_owner && !screen_owner)continue;
        if(screen_owner) {
            if(*(const unsigned*)((const char*)owner+0x58)&0x20)continue;
            struct fv_plane plane;
            if(fv_widget_plane(part,&plane)) {
                float origin[3]={0},extent[3]={0,plane.width*.5f,plane.height*.5f};
                if(!fv_console_case_expand(camera,&plane.transform,origin,extent))return 0;
                // The authored panel canvas owns its rows; their unclipped scroll contents are not camera bounds.
                continue;
            }
        }
        struct{void**data;int count,max;} children;memcpy(&children,(const char*)part+0xd0,sizeof children);
        if(children.count<0 || children.count>256 || children.max<children.count || (children.count&&!children.data))return 0;
        for(int k=0;k<children.count;k++) {
            void* child=children.data[k];unsigned seen=0;
            for(unsigned j=0;j<count;j++)if(queue[j]==child)seen=1;
            if(seen)continue;
            if(count==256)return 0;
            queue[count++]=child;
        }
    }
    return 1;
}
// Fit the fixed complete case, including its doors, from the final native eye.
// Scrolling rows, item placement and hover animation never change these bounds.
static int fv_console_case_fov(void* screen,const void* view,float* fov) {
    void* owner=fv_console_case_owner(screen);
    if(!owner || !view || !fov || !mv_static_class || !g_comp_h)return 0;
    void* root=*(void* const*)((const char*)owner+0x130);
    if(!fv_frame_owned(root,owner))return 0;
    struct fv_case_view camera={0};float rotation[3];camera.aspect=(float)g_comp_w/g_comp_h;
    memcpy(camera.eye,view,12);memcpy(rotation,(const char*)view+0xc,12);
    if(!isfinite(camera.aspect) || camera.aspect<1 || camera.aspect>4 || *(const unsigned char*)((const char*)view+0x31))return 0;
    for(unsigned k=0;k<3;k++)if(!isfinite(camera.eye[k]) || !isfinite(rotation[k]))return 0;
    float p=rotation[0]*.0174532925199433f,y=rotation[1]*.0174532925199433f,r=rotation[2]*.0174532925199433f;
    float cp=cosf(p),sp=sinf(p),cy=cosf(y),sy=sinf(y),cr=cosf(r),sr=sinf(r);
    float forward[3]={cp*cy,cp*sy,sp},right[3]={sr*sp*cy-cr*sy,sr*sp*sy+cr*cy,-sr*cp};
    float up[3]={-cr*sp*cy-sr*sy,cy*sr-cr*sp*sy,cr*cp};
    memcpy(camera.forward,forward,12);memcpy(camera.right,right,12);memcpy(camera.up,up,12);
    static const char* const fields[]={"Background_Plane","Frame_Mesh_Back","Frame_Mesh_Sides",
        "DoorMesh_Left","DoorMesh_Right","DoorExtension_Left","DoorExtension_Right"};
    unsigned count=0;
    for(unsigned i=0;i<sizeof fields/sizeof *fields;i++) {
        void* part=fv_object_property(owner,fields[i]);
        if(!part)continue;
        if(!pv_alive(part) || !menu_visual_layout(part,mv_static_class(),0x448)
            || *(void* const*)((const char*)part+0xa0)!=owner || !fv_descends_from(part,root))return 0;
        struct reload_tf tf;float origin[3],extent[3];memcpy(&tf,(const char*)part+0x1c0,sizeof tf);
        if(!fv_console_case_bounds(part,origin,extent) || !fv_console_case_expand(&camera,&tf,origin,extent))return 0;
        count++;
    }
    if(!fv_console_case_widgets(root,&camera))return 0;
    float fitted=2*atanf(camera.tangent/.86f)*57.2957795130823f;
    if(count<4 || !isfinite(fitted) || fitted<1 || fitted>130)return 0;
    *fov=fmaxf(*fov,fitted);return isfinite(*fov);
}
static struct {
    struct cinema_ui_weak pawn,screen,root,surface;
    long stamp;
    float fov;
    int used;
} fv_camera_menu;
static void* fv_camera_menu_surface(void* screen) {
    if(!pv_alive(screen) || !fv_button_class || !fv_ui_enabled || !mv_static_class)return 0;
    if(!cinema_ui_named(screen,"DeathMenu_BP_C",0x498)) {
        if(!fv_screen_class || !menu_visual_layout(screen,fv_screen_class(),0x248))return 0;
        void* root=*(void* const*)((const char*)screen+0x130);
        return fv_frame_owned(root,screen) ? root:0;
    }
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
// Camera bounds include the native cylinder depth; root-reposition fitting stays planar-only.
static unsigned fv_camera_widget_planes(void* part,void* screen,struct fv_plane* out,unsigned capacity) {
    struct fv_plane plane;
    if(fv_widget_plane(part,&plane)) {
        if(!capacity)return 1;
        out[0]=plane;return 1;
    }
    if(!pv_alive(part) || !cu_component_class || !fv_current_draw_size
        || !menu_visual_layout(part,cu_component_class(),0x520) || !pv_actor_class)return 0;
    void* owner=*(void* const*)((const char*)part+0xa0);
    if(!pv_alive(owner) || !menu_visual_layout(owner,pv_actor_class(),0x220)
        || *(void* const*)((const char*)part+0x20)!=owner)return 0;
    void* owner_root=*(void* const*)((const char*)owner+0x130);
    void* screen_root=*(void* const*)((const char*)screen+0x130);
    if(!fv_frame_owned(owner_root,owner) || !fv_descends_from(part,owner_root)
        || !fv_descends_from(owner_root,screen_root))return 0;
    unsigned flags=*(const unsigned short*)((const char*)part+0x14c);
    if((flags&0x420)!=0x20 || *(const unsigned char*)((const char*)part+0x429)
        || *(const unsigned char*)((const char*)part+0x518)!=1)return 0;
    struct cinema_ui_vec2 size=fv_current_draw_size(part);float pivot[2],arc;
    memcpy(pivot,(const char*)part+0x45c,sizeof pivot);memcpy(&arc,(const char*)part+0x51c,sizeof arc);
    memcpy(&plane.transform,(const char*)part+0x1c0,sizeof plane.transform);
    if(!reload_marker_valid(&plane.transform) || !isfinite(size.x) || !isfinite(size.y)
        || size.x<=0 || size.x>16384 || size.y<=0 || size.y>16384
        || !isfinite(pivot[0]) || !isfinite(pivot[1]) || pivot[0]<0 || pivot[0]>1 || pivot[1]<0 || pivot[1]>1
        || !isfinite(arc) || arc<=0 || arc>360)return 0;
    float half=arc*.00872664625997165f,radius=size.x/(2*half),sine=sinf(half);
    float depth=radius*(1-cosf(half)),width=2*radius*(half<=1.5707963267948966f?sine:1);
    float y=-(.5f-pivot[0])*2*radius*sine,z=(pivot[1]-.5f)*size.y;
    if(!isfinite(depth) || !isfinite(width) || !isfinite(y) || depth<0 || width<=0 || width>16384)return 0;
    if(capacity<2)return capacity+1;
    plane.width=width;plane.height=size.y;
    for(unsigned i=0;i<2;i++) {
        out[i]=plane;float local[3]={(i?0:-depth)*plane.transform.s[0],y*plane.transform.s[1],z*plane.transform.s[2]},world[3];
        q_rot(plane.transform.q,local,world);
        for(unsigned k=0;k<3;k++)out[i].transform.p[k]+=world[k];
    }
    return 2;
}
static int fv_camera_ui_actor(void* screen,void* owner) {
    if(!pv_alive(owner) || !pv_actor_class || !fv_button_class
        || !menu_visual_layout(owner,pv_actor_class(),0x220))return 0;
    void* root=*(void* const*)((const char*)owner+0x130);
    if(!fv_frame_owned(root,owner)
        || !fv_descends_from(root,*(void* const*)((const char*)screen+0x130)))return 0;
    struct{void**data;int count,max;} controls;memcpy(&controls,(const char*)screen+0x238,sizeof controls);
    if(controls.count<0 || controls.count>512 || controls.max<controls.count || controls.max>1024
        || (controls.count&&!controls.data))return 0;
    for(int i=0;i<controls.count;i++) {
        void* part=controls.data[i];
        if(!pv_alive(part) || (!menu_visual_layout(part,fv_button_class(),0x320)
            && (!fv_scroll_class || !menu_visual_layout(part,fv_scroll_class(),0x430))))continue;
        // Disabled receivers can retain visible authored labels; registration owns their geometry.
        if(*(void* const*)((const char*)part+0xa0)==owner && *(void* const*)((const char*)part+0x20)==owner
            && *(void* const*)((const char*)part+0x308)==screen && fv_descends_from(part,root))return 1;
    }
    return 0;
}
static unsigned fv_camera_widgets(void* screen,void* root,struct fv_plane* out,unsigned capacity) {
    void* queue[256]={root};unsigned queued=1,count=0;
    for(unsigned i=0;i<queued;i++) {
        void* part=queue[i];
        if(!pv_alive(part) || !menu_visual_layout(part,mv_scene_class(),0x1f0))continue;
        void* owner=*(void* const*)((const char*)part+0xa0);
        if(owner!=screen && !fv_camera_ui_actor(screen,owner))continue;
        unsigned added=fv_camera_widget_planes(part,screen,out+count,capacity-count);
        if(added>capacity-count)return capacity+1;
        count+=added;
        struct{void**data;int count,max;} children;memcpy(&children,(const char*)part+0xd0,sizeof children);
        if(children.count<0 || children.count>256 || children.max<children.count || (children.count&&!children.data))return capacity+1;
        for(int k=0;k<children.count;k++) {
            void* child=children.data[k];unsigned seen=0;
            for(unsigned j=0;j<queued;j++)if(queue[j]==child)seen=1;
            if(seen)continue;
            if(queued==256)return capacity+1;
            queue[queued++]=child;
        }
    }
    return count;
}
static int fv_camera_menu_bounds(void* screen,void* surface,struct reload_tf* mesh,float origin[3],float extent[3]) {
    memcpy(mesh,(const char*)surface+0x1c0,sizeof *mesh);
    if(cinema_ui_named(screen,"DeathMenu_BP_C",0x498))return cinema_movie_screen_bounds(surface,origin,extent);
    if(!fv_frame_owned(surface,screen))return 0;
    struct fv_plane planes[64];unsigned count=fv_camera_widgets(screen,surface,planes,64);
    if(!count || count>64)return 0;
    struct {void**data;int count,max;} controls;memcpy(&controls,(const char*)screen+0x238,sizeof controls);
    if(controls.count<0 || controls.count>512 || controls.max<controls.count || controls.max>1024
        || (controls.count && !controls.data))return 0;
    for(int i=0;i<controls.count;i++) {
        void* part=controls.data[i];
        struct fv_plane plane;
        if(!pv_alive(part) || !fv_component_plane(part,&plane)
            || *(void* const*)((const char*)part+0x308)!=screen)continue;
        if(count==64)return 0;
        planes[count++]=plane;
    }
    *mesh=planes[0].transform;mesh->s[0]=mesh->s[1]=mesh->s[2]=1;
    qf_t inverse={-mesh->q.x,-mesh->q.y,-mesh->q.z,mesh->q.w};
    float lo[3]={INFINITY,INFINITY,INFINITY},hi[3]={-INFINITY,-INFINITY,-INFINITY};
    for(unsigned i=0;i<count;i++) {
        struct fv_plane* plane=planes+i;
        if(!reload_marker_valid(&plane->transform) || !isfinite(plane->width) || !isfinite(plane->height)
            || plane->width<=0 || plane->width>16384 || plane->height<=0 || plane->height>16384)return 0;
        for(unsigned corner=0;corner<4;corner++) {
            float local[3]={0,(corner&1 ? .5f:-.5f)*plane->width*plane->transform.s[1],
                (corner&2 ? .5f:-.5f)*plane->height*plane->transform.s[2]},world[3],aligned[3];
            q_rot(plane->transform.q,local,world);
            for(unsigned k=0;k<3;k++)world[k]+=plane->transform.p[k]-mesh->p[k];
            q_rot(inverse,world,aligned);
            for(unsigned k=0;k<3;k++){lo[k]=fminf(lo[k],aligned[k]);hi[k]=fmaxf(hi[k],aligned[k]);}
        }
    }
    for(unsigned k=0;k<3;k++){origin[k]=(lo[k]+hi[k])*.5f;extent[k]=(hi[k]-lo[k])*.5f;}
    return reload_marker_valid(mesh);
}
// Detached native screens can own separate ChildActor buttons; keep every root and animation intact.
// Use the shared HMD pose so the visible geometry and original pointer rays always agree.
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
    struct reload_tf mesh,desired;
    float aspect=g_comp_h ? (float)g_comp_w/g_comp_h:0;
    // Sample authored geometry after undoing an earlier phone-owned root fit.
    fv_frame_restore();
    if(!fv_camera_menu_bounds(screen,surface,&mesh,origin,extent)
        || !fv_bounds_camera(camera,&mesh,origin,extent,aspect,&desired,&fov))return 0;
    head_angles_now(&current_yaw,&current_pitch);
    if(!fv_camera_head(camera,&desired,current_yaw,current_pitch,&yaw,&pitch))return 0;
    int entered=cu_weak_get(&fv_camera_menu.pawn)!=pawn || cu_weak_get(&fv_camera_menu.screen)!=screen
        || now<fv_camera_menu.stamp || now-fv_camera_menu.stamp>=300;
    cu_weak_assign(&fv_camera_menu.pawn,pawn);cu_weak_assign(&fv_camera_menu.screen,screen);
    cu_weak_assign(&fv_camera_menu.root,*(void* const*)((const char*)screen+0x130));cu_weak_assign(&fv_camera_menu.surface,surface);
    fv_camera_menu.stamp=now;fv_camera_menu.fov=fov;fv_camera_menu.used=1;
    g_menu_yaw_off=yaw;g_menu_pitch=pitch;g_menu_view_ms=now;g_menu_view_active=1;
    static unsigned logs;
    if(entered && logs<16){logs++;LOGF("UICAMERA detached authored menu=%p pawn=%p yaw=%.2f pitch=%.2f fov=%.2f roots unchanged",screen,pawn,yaw*57.2957795f,pitch*57.2957795f,fov);}
    return 1;
}
// Keep the authored console zoom only in native menu contexts and on the active pawn camera.
static void fv_menu_camera_view(void* camera, void* view) {
    if (!view || !camera || !fv_ui_manager || !fv_button_class || !fv_ui_enabled || !phone_active
        || !phone_type || !cinema_on_game_thread()) return;
    unsigned type = phone_type();
    long now = ptr_now_ms();
    if(type==8) {
        void* pawn=phone_active();
        if(!mv_pawn_class || !cinema_camera_class || !pv_alive(pawn)
            || !menu_visual_layout(pawn,mv_pawn_class(),0xbb0)
            || *(void* const*)((const char*)pawn+0x2d8)!=camera || !pv_alive(camera)
            || !cinema_object(camera,cinema_camera_class(),0x7b8) || *(void* const*)((const char*)camera+0xa0)!=pawn
            || g_comp_h<64 || g_comp_w<g_comp_h || g_comp_w>16384 || g_comp_h>16384
            || (float)g_comp_w/g_comp_h>4.0f)return;
        float tangent=0.57735026919f;
        if(fv_peek_screen && g_ue4_base_cached && (float)g_comp_w/g_comp_h>=16.0f/9.0f) {
            void* manager=fv_ui_manager();
            void* top=pv_alive(manager)?fv_peek_screen(manager):0,*screen=top;
            if(cinema_ui_named(top,"InventoryUIScreen_BP_C",0x3b0)) {
                void* frame=fv_frame_owned(*(void* const*)((const char*)top+0x130),top)?fv_console_actor_parent(top):0;
                screen=cinema_ui_named(frame,"UI_MerchantScreen_InventoryFrame_BP_C",0x3d8)
                    && fv_frame_owned(*(void* const*)((const char*)frame+0x130),frame)
                    && fv_object_property(frame,"Ref_InventoryUI")==top ? fv_object_property(frame,"MerchantScreen"):0;
                if(screen && fv_console_actor_parent(frame)!=screen)screen=0;
            }
            static const unsigned getter[2]={0x394c5000,0xd65f03c0};
            if(cinema_ui_named(screen,"UI_MerchantScreen_BP_C",0x320)
                && fv_frame_owned(*(void* const*)((const char*)screen+0x130),screen)
                && !memcmp((const void*)(g_ue4_base_cached+0x63168dc),getter,sizeof getter)) {
                unsigned state=*((const unsigned char*)screen+0x314);
                // Stable buy/tune-up pages need readable text; transfers and sales keep the complete low table.
                if(state==4 || state==12 || state==14)tangent=0.41421356237f;
            }
        }
        float fov=2*atanf((float)g_comp_w/g_comp_h*tangent)*57.295779513f;
        memcpy((char*)view+0x18,&fov,4);g_fov_h_deg=fov;return;
    }
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
    void* manager=fv_ui_manager();void* screen=manager && fv_peek_screen ? fv_peek_screen(manager):0;
    int case_fit=fv_console_case_fov(screen,view,&fov);
    static struct cinema_ui_weak last_case={-1,0};static unsigned case_logs;
    if(case_fit && cu_weak_assign && cu_weak_get && cu_weak_get(&last_case)!=screen && case_logs<16) {
        case_logs++;cu_weak_assign(&last_case,screen);LOGF("UICASE complete authored case screen=%p fov=%.2f roots unchanged",screen,fov);
    }
    memcpy((char*)view + 0x18, &fov, 4);
    g_fov_h_deg = fov;
    static unsigned logs;
    if (logs < 4) { logs++; LOGF("UIFOV authored=%.2f applied=%.2f fixedConsole=1 case=%d", authored, fov,case_fit); }
}
#endif
