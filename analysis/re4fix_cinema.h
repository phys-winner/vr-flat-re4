// Flat cinema view: preserve native Blueprint playback, replace only its owned view output.
#ifndef RE4FIX_CINEMA_H
#define RE4FIX_CINEMA_H

struct cinema_rotator { float pitch,yaw,roll; };
static struct cinema_rotator (*cinema_rotator)(const qf_t*);
static void* (*cinema_active)(void), *(*cinema_pawn_class)(void), *(*cinema_camera_class)(void);
static unsigned (*cinema_type)(void);
static struct {
    void* pawn;
    unsigned long long pawn_name;
    struct reload_tf transform;
    float fov_y;
    int valid,closing;
} cinema_scene;
static struct {
    void* pawn,*camera;
    unsigned long long pawn_name;
    unsigned char projection[0x32];
    int valid;
} cinema_last;
static unsigned cinema_scene_logs,cinema_view_logs,cinema_entry_logs,cinema_reject_mask,cinema_subtitle_logs;
unsigned long g_tgt_cinema_update,g_tgt_cinema_view,g_tgt_cinema_subtitle;

static int cinema_object(const void* object,const void* target,int minimum) {
    if (!object || !target) return 0;
    const char* cls=*(const char* const*)((const char*)object+0x10);
    if (!cls) return 0;
    int size=*(const int*)(cls+0x58),depth=*(const int*)((const char*)target+0x38);
    int child_depth=*(const int*)(cls+0x38);
    if (size<minimum || size>0x10000 || depth<0 || depth>128
        || child_depth<depth || child_depth>256) return 0;
    const void* const* chain=*(const void* const* const*)(cls+0x30);
    return chain && chain[depth]==(const char*)target+0x30;
}
static int cinema_on_game_thread(void) {
    int tid=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE);
    return tid>0 && tid==(int)syscall(SYS_gettid);
}
#include "re4fix_cinema_ui.h"
static int cinema_view_rejected(unsigned stage,const char* reason,void* camera,void* pawn) {
    if (stage<16 && !(cinema_reject_mask&(1u<<stage))) {
        cinema_reject_mask|=1u<<stage;
        LOGF("CINEMA view reject=%s camera=%p active=%p scene=%p valid=%d",reason,camera,pawn,
             cinema_scene.pawn,cinema_scene.valid);
    }
    return 0;
}
static void cinema_capture(void* pawn,const struct reload_tf* transform,float fov_y,int screen_on) {
    if (!cinema_on_game_thread() || !cinema_active || !cinema_type || !cinema_pawn_class) return;
    if (cinema_type()!=3 || pawn!=cinema_active()
        || !cinema_object(pawn,cinema_pawn_class(),0x1168)) return;
    cinema_scene.valid=0;
    cinema_scene.closing=!screen_on;
    if (!screen_on || !transform || !reload_tf_valid(transform)
        || !isfinite(fov_y) || fov_y<1.0f || fov_y>170.0f) {
        if (screen_on) cinema_last.valid=0;
        return;
    }
    cinema_scene.pawn=pawn;
    memcpy(&cinema_scene.pawn_name,(const char*)pawn+0x18,8);
    cinema_scene.transform=*transform;
    cinema_scene.fov_y=fov_y;
    cinema_scene.valid=1;
    if (cinema_scene_logs<8) {
        ++cinema_scene_logs;
        LOGF("CINEMA scene pawn=%p fovY=%.4f screenOn=%d p=(%.3f,%.3f,%.3f)",
             pawn,fov_y,screen_on,transform->p[0],transform->p[1],transform->p[2]);
    }
}
// Flat FMV: while the theatre plays a movie the authored Bio4 scene is empty (camera at the scene origin),
// so frame the theatre's visible video plane instead. The camera keeps the authored VR viewing distance
// along the plane normal; the FOV fits the whole plane into the phone viewport (dark theatre = bars).
static unsigned cinema_movie_logs;
static void* (*cinema_game_instance)(const void*);
static void (*cinema_set_bool)(void*, unsigned long long, int);
static struct cinema_ui_weak cinema_flat_instance = {-1,0};
// Reflected descriptor of a (possibly inherited) property.
static const char* cinema_field_property(const void* object,const char* wanted,const char* want_kind) {
    const char* cls=object ? *(const char* const*)((const char*)object+0x10) : 0;
    for (unsigned depth=0;cls && depth<12;depth++) {
        const char* field=*(const char* const*)(cls+0x70);
        for (unsigned i=0;field && i<512;i++) {
            char name[64],kind[48];phone_inspect_fname(field+0x28,name,sizeof name);
            if (!strcmp(name,wanted)) {
                phone_inspect_fname(*(const void* const*)(field+0x8),kind,sizeof kind);
                return strcmp(kind,want_kind) ? 0 : field;
            }
            const char* next=*(const char* const*)(field+0x58);if (next==field) break;field=next;
        }
        const char* super=*(const char* const*)(cls+0x40);
        if (super==cls) break;
        cls=super;
    }
    return 0;
}
static int cinema_field_offset(const void* object,const char* wanted,const char* want_kind) {
    const char* field = cinema_field_property(object, wanted, want_kind);
    return field ? *(const int*)(field + 0x4c) : -1;
}
// The original theatre graph selects its authored FlatScreen when this GameInstance option is true.
static void cinema_flat_movie_setting(void* camera) {
    if (!cinema_game_instance || !cinema_set_bool || !cinema_active || !phone_inspect_ready
        || !cinema_on_game_thread() || !cu_weak_get || !cu_weak_assign) return;
    void* pawn = cinema_active();
    if (!pawn || *(void* const*)((const char*)pawn + 0x2d8) != camera) return;
    void* instance = cinema_game_instance(pawn);
    if (!instance || cu_weak_get(&cinema_flat_instance) == instance
        || !cinema_ui_named(instance, "VR4GameInstance_BP_C", 0x100)) return;
    const char* field = cinema_field_property(instance, "UseFlatCutsceneScreen", "BoolProperty");
    const char* cls = *(const char* const*)((const char*)instance + 0x10);
    int size = *(const int*)(cls + 0x58), offset = field ? *(const int*)(field + 0x4c) : -1;
    if (!field || offset < 0x100 || offset >= size || *(const int*)(field + 0x34) != 1
        || *(const int*)(field + 0x38) != 1) return;
    unsigned long long name; memcpy(&name, field + 0x28, sizeof name);
    cinema_set_bool(instance, name, 1);
    cu_weak_assign(&cinema_flat_instance, instance);
    LOGF("CINEMA authored flat movie screen selected (GameInstance=%p)", instance);
}
static void cinema_movie_describe(const char* label,void* screen) {
    char name[64]="",mesh[64]="",material[64]="",kind[64]="",parent[64]="";
    if (!screen) { LOGF("CINEMA MOVIE %s=null",label);return; }
    phone_inspect_fname((const char*)screen+0x18,name,sizeof name);
    void* asset=*(void* const*)((const char*)screen+0x440);
    if (asset) phone_inspect_fname((const char*)asset+0x18,mesh,sizeof mesh);
    int overrides=cinema_field_offset(screen,"OverrideMaterials","ArrayProperty");
    if (overrides>0 && overrides<0x480) {
        struct { void** data;int count,max; } list;memcpy(&list,(const char*)screen+overrides,sizeof list);
        if (list.data && list.count>0 && list.count<=list.max && list.max<64 && list.data[0]) {
            void* m=list.data[0];
            phone_inspect_fname((const char*)m+0x18,material,sizeof material);
            phone_inspect_fname(*(const char* const*)((const char*)m+0x10)+0x18,kind,sizeof kind);
            int parent_offset=cinema_field_offset(m,"Parent","ObjectProperty");
            void* p=parent_offset>0 && parent_offset<0x400 ? *(void* const*)((const char*)m+parent_offset) : 0;
            if (p) phone_inspect_fname((const char*)p+0x18,parent,sizeof parent);
        }
    }
    const float* tf=(const float*)((const char*)screen+0x1c0);
    LOGF("CINEMA MOVIE %s=%p '%s' mesh='%s' flags=%04x material='%s' class='%s' parent='%s' q=(%.4f,%.4f,%.4f,%.4f) p=(%.1f,%.1f,%.1f) s=(%.4f,%.4f,%.4f)",
         label,screen,name,mesh,*(const unsigned short*)((const char*)screen+0x14c),material,kind,parent,
         tf[0],tf[1],tf[2],tf[3],tf[4],tf[5],tf[6],tf[7],tf[8],tf[9]);
}
static void cinema_cross(const float a[3],const float b[3],float out[3]) {
    out[0]=a[1]*b[2]-a[2]*b[1];out[1]=a[2]*b[0]-a[0]*b[2];out[2]=a[0]*b[1]-a[1]*b[0];
}
static int cinema_prop_check(const void* object,const char* class_name,const char* wanted,
                             const char* want_kind,int offset,int element) {
    if (!cinema_ui_named(object,class_name,offset+element)) return 0;
    const char* cls=*(const char* const*)((const char*)object+0x10);
    const char* field=*(const char* const*)(cls+0x70);
    for (unsigned i=0;field && i<256;i++) {
        char name[64],kind[48];phone_inspect_fname(field+0x28,name,sizeof name);
        if (!strcmp(name,wanted)) {
            phone_inspect_fname(*(const void* const*)(field+0x8),kind,sizeof kind);
            return !strcmp(kind,want_kind) && *(const int*)(field+0x38)==element
                && *(const int*)(field+0x34)==1 && *(const int*)(field+0x4c)==offset;
        }
        const char* next=*(const char* const*)(field+0x58);if (next==field) break;field=next;
    }
    return 0;
}
static void* cinema_movie_probe(void* pawn,int opening_only) {
    if (!phone_inspect_ready || !cu_weak_assign || !cu_weak_get || !cinema_on_game_thread()
        || !cinema_type || !cinema_active || cinema_type()!=3 || pawn!=cinema_active()) return 0;
    void* theatre=cinema_ui_property(pawn,"VR4CutscenePlayerPawn_BP_C","TheatreBox",0x1198);
    if (!theatre || !cinema_prop_check(theatre,"CutsceneTheatreBox_C","VideoCompleted","BoolProperty",0x4c5,1)
        || *(const unsigned char*)((const char*)theatre+0x4c5)) return 0;
    void* video=cinema_ui_property(theatre,"CutsceneTheatreBox_C","VideoToPlay",0x478);
    struct cinema_ui_weak weak={-1,0};cu_weak_assign(&weak,video);
    if (!video || cu_weak_get(&weak)!=video || !cinema_ui_named(video,"FileMediaSource",0x40)) return 0;
    if (opening_only) {
        char name[64];phone_inspect_fname((const char*)video+0x18,name,sizeof name);
        if (strcmp(name,"Opening_android")) return 0;
    }
    return theatre;
}
// The authored FMV completion closes Media/audio and notifies the native pawn through its own graph.
static int cinema_movie_complete(void* theatre) {
    if (!theatre || !phone_actor_event_address || !cinema_on_game_thread()
        || !cinema_active || cinema_movie_probe(cinema_active(),0)!=theatre) return 0;
    const char* cls=*(const char* const*)((const char*)theatre+0x10);
    const char* fn=*(const char* const*)(cls+0x48);
    for (unsigned i=0;fn && i<128;i++) {
        char name[64],kind[48];phone_inspect_fname(fn+0x18,name,sizeof name);
        if (!strcmp(name,"VideoComplete")) {
            phone_inspect_fname(*(const char* const*)(fn+0x10)+0x18,kind,sizeof kind);
            if (strcmp(kind,"Function") || *(const void* const*)(fn+0x20)!=cls
                || *(const unsigned*)(fn+0xb0)!=0x0c000000 || *(const unsigned char*)(fn+0xb4)
                || *(const unsigned short*)(fn+0xb6) || *(const unsigned short*)(fn+0xb8)!=0xffff) return 0;
            void* const* vtable=*(void* const* const*)theatre;
            if (!vtable || vtable[0x218/sizeof(void*)]!=phone_actor_event_address) return 0;
            ((void (*)(void*,const void*,void*))phone_actor_event_address)(theatre,fn,0);
            return 1;
        }
        const char* next=*(const char* const*)(fn+0x28);if (next==fn) break;fn=next;
    }
    return 0;
}
// Reflection is validated once per class/asset; per-frame reads use the proven offsets.
static struct {
    const void* pawn_class,*theatre_class,*screen_class,*mesh;
    float origin[3],extent[3];
} cinema_movie_cache;
static int cinema_movie_screen_bounds(void* screen,float origin[3],float extent[3]) {
    const void* cls=*(const void* const*)((const char*)screen+0x10);
    if (cls!=cinema_movie_cache.screen_class) {
        if (cinema_field_offset(screen,"StaticMesh","ObjectProperty")!=0x440) return 0;
        cinema_movie_cache.screen_class=cls;
    }
    void* mesh=*(void* const*)((const char*)screen+0x440);
    if (!mesh) return 0;
    if (mesh!=cinema_movie_cache.mesh) {
        if (!cinema_ui_named(mesh,"StaticMesh",0x10c)
            || cinema_field_offset(mesh,"ExtendedBounds","StructProperty")!=0xf0) return 0;
        const float* b=(const float*)((const char*)mesh+0xf0);   // FBoxSphereBounds Origin/BoxExtent
        for (unsigned i=0;i<6;i++) if (!isfinite(b[i]) || fabsf(b[i])>100000.0f) return 0;
        memcpy(cinema_movie_cache.origin,b,12);memcpy(cinema_movie_cache.extent,b+3,12);
        cinema_movie_cache.mesh=mesh;
        char name[64]="";phone_inspect_fname((const char*)mesh+0x18,name,sizeof name);
        LOGF("CINEMA MOVIE mesh=%p '%s' boundsOrigin=(%.1f,%.1f,%.1f) extent=(%.1f,%.1f,%.1f)",
             mesh,name,b[0],b[1],b[2],b[3],b[4],b[5]);
    }
    memcpy(origin,cinema_movie_cache.origin,12);memcpy(extent,cinema_movie_cache.extent,12);
    return 1;
}
static int cinema_movie_view(void* pawn,void* view,float aspect) {
    if (!phone_inspect_ready) return 0;
    const void* pawn_class=*(const void* const*)((const char*)pawn+0x10);
    if (pawn_class!=cinema_movie_cache.pawn_class) {
        if (!cinema_prop_check(pawn,"VR4CutscenePlayerPawn_BP_C","TheatreBox","ObjectProperty",0x1198,8)) return 0;
        cinema_movie_cache.pawn_class=pawn_class;
    }
    void* theatre=*(void* const*)((const char*)pawn+0x1198);
    if (!theatre) return 0;
    const void* theatre_class=*(const void* const*)((const char*)theatre+0x10);
    if (theatre_class!=cinema_movie_cache.theatre_class) {
        if (!cinema_prop_check(theatre,"CutsceneTheatreBox_C","VideoToPlay","ObjectProperty",0x478,8)
            || !cinema_prop_check(theatre,"CutsceneTheatreBox_C","VideoCompleted","BoolProperty",0x4c5,1)
            || !cinema_prop_check(theatre,"CutsceneTheatreBox_C","Screen","ObjectProperty",0x2f8,8)
            || !cinema_prop_check(theatre,"CutsceneTheatreBox_C","FlatScreen","ObjectProperty",0x238,8)) return 0;
        cinema_movie_cache.theatre_class=theatre_class;
    }
    void* video=*(void* const*)((const char*)theatre+0x478);
    if (!video) return 0;
    if (*(const unsigned char*)((const char*)theatre+0x4c5)) return -1;
    void* screens[2]={*(void* const*)((const char*)theatre+0x2f8),*(void* const*)((const char*)theatre+0x238)};
    if (cinema_movie_logs<2) {
        char vn[64]="";phone_inspect_fname((const char*)video+0x18,vn,sizeof vn);
        LOGF("CINEMA MOVIE theatre=%p video=%p '%s'",theatre,video,vn);
        cinema_movie_describe("Screen",screens[0]);cinema_movie_describe("FlatScreen",screens[1]);
    }
    // The visible authored video surface: curved CinemaScreen (default) or the flat Plane variant.
    void* screen=0;
    for (unsigned i=0;i<2 && !screen;i++)
        if (screens[i] && *(void* const*)((const char*)screens[i]+0xa0)==theatre
            && (*(const unsigned short*)((const char*)screens[i]+0x14c)&0x20)) screen=screens[i];
    float origin[3],extent[3];
    if (!screen || !cinema_movie_screen_bounds(screen,origin,extent)) return 0;
    const float* tf=(const float*)((const char*)screen+0x1c0);
    qf_t q={tf[0],tf[1],tf[2],tf[3]};
    const float* eye=(const float*)view;   // native theatre viewer written by the original GetCameraView
    float corners[8][3],centre[3]={0,0,0};
    for (unsigned c=0;c<8;c++) {
        float local[3]={(origin[0]+((c&1)?extent[0]:-extent[0]))*tf[7],   // scalar FTransform: q0 p4 s7
                        (origin[1]+((c&2)?extent[1]:-extent[1]))*tf[8],
                        (origin[2]+((c&4)?extent[2]:-extent[2]))*tf[9]},world[3];
        q_rot(q,local,world);
        for (unsigned k=0;k<3;k++) { corners[c][k]=tf[4+k]+world[k]-eye[k];centre[k]+=corners[c][k]*0.125f; }
    }
    float m=sqrtf(centre[0]*centre[0]+centre[1]*centre[1]+centre[2]*centre[2]);
    if (!isfinite(m) || m<1.0f) return 0;
    float forward[3]={centre[0]/m,centre[1]/m,centre[2]/m},right[3],top[3];
    static const float up[3]={0,0,1};
    cinema_cross(up,forward,right);
    m=sqrtf(right[0]*right[0]+right[1]*right[1]+right[2]*right[2]);
    if (!isfinite(m) || m<1e-3f) return 0;
    right[0]/=m;right[1]/=m;right[2]/=m;
    cinema_cross(forward,right,top);
    // Smallest symmetric frustum that contains every corner of the screen bounds.
    float tx=0,ty=0;
    for (unsigned c=0;c<8;c++) {
        float x=corners[c][0]*forward[0]+corners[c][1]*forward[1]+corners[c][2]*forward[2];
        float y=corners[c][0]*right[0]+corners[c][1]*right[1]+corners[c][2]*right[2];
        float z=corners[c][0]*top[0]+corners[c][1]*top[1]+corners[c][2]*top[2];
        if (!(x>1.0f)) return 0;
        tx=fmaxf(tx,fabsf(y)/x);ty=fmaxf(ty,fabsf(z)/x);
    }
    float fov=2.0f*atanf(fmaxf(tx,ty*aspect))*57.2957795f;
    if (!isfinite(fov)) return 0;
    fov=fminf(fmaxf(fov,10.0f),170.0f);
    // Rotation matrix with columns (forward,right,top), R[i][j] = column j component i -> quaternion
    // (same result as native FQuat(FMatrix) with the axes as matrix rows).
    float m00=forward[0],m11=right[1],m22=top[2],trace=m00+m11+m22;
    qf_t r;
    if (trace>0) {
        float s=sqrtf(trace+1.0f)*2.0f;
        r.w=0.25f*s;r.x=(right[2]-top[1])/s;r.y=(top[0]-forward[2])/s;r.z=(forward[1]-right[0])/s;
    } else if (m00>m11 && m00>m22) {
        float s=sqrtf(1.0f+m00-m11-m22)*2.0f;
        r.w=(right[2]-top[1])/s;r.x=0.25f*s;r.y=(right[0]+forward[1])/s;r.z=(top[0]+forward[2])/s;
    } else if (m11>m22) {
        float s=sqrtf(1.0f+m11-m00-m22)*2.0f;
        r.w=(top[0]-forward[2])/s;r.x=(right[0]+forward[1])/s;r.y=0.25f*s;r.z=(top[1]+right[2])/s;
    } else {
        float s=sqrtf(1.0f+m22-m00-m11)*2.0f;
        r.w=(forward[1]-right[0])/s;r.x=(top[0]+forward[2])/s;r.y=(top[1]+right[2])/s;r.z=0.25f*s;
    }
    struct cinema_rotator rotation=cinema_rotator(&r);
    if (!isfinite(rotation.pitch) || !isfinite(rotation.yaw) || !isfinite(rotation.roll)) return 0;
    memcpy((char*)view+0xc,&rotation,12);
    memcpy((char*)view+0x18,&fov,4);
    memcpy((char*)view+0x2c,&aspect,4);
    *(unsigned char*)((char*)view+0x30)|=1;
    *(unsigned char*)((char*)view+0x31)=0;
    if (cinema_movie_logs<4) {
        ++cinema_movie_logs;
        LOGF("CINEMA MOVIE view screen=%p eye=(%.1f,%.1f,%.1f) centre=(%.1f,%.1f,%.1f) tan=(%.3f,%.3f) fovX=%.2f aspect=%.4f rotation=(%.2f,%.2f,%.2f)",
             screen,eye[0],eye[1],eye[2],centre[0],centre[1],centre[2],tx,ty,fov,aspect,
             rotation.pitch,rotation.yaw,rotation.roll);
    }
    return 1;
}
static int cinema_flat_view(void* camera,void* view) {
    if (!view || !cinema_on_game_thread() || !cinema_active || !cinema_type
        || !cinema_pawn_class || !cinema_camera_class || !cinema_rotator) return 0;
    if (cinema_type()!=3) { cinema_scene.valid=cinema_scene.closing=cinema_last.valid=0;cinema_subtitle_logs=0;cinema_movie_logs=0;return 0; }
    void* pawn=cinema_active();
    unsigned long long name=0;
    if (!cinema_object(pawn,cinema_pawn_class(),0x1168))
        return cinema_view_rejected(0,"active pawn layout",camera,pawn);
    memcpy(&name,(const char*)pawn+0x18,8);
    if (*(void* const*)((const char*)pawn+0x2d8)!=camera)
        return cinema_view_rejected(3,"not active Camera",camera,pawn);
    if (!cinema_object(camera,cinema_camera_class(),0x7b8))
        return cinema_view_rejected(4,"native VR4Camera layout",camera,pawn);
    if (*(void* const*)((const char*)camera+0xa0)!=pawn)
        return cinema_view_rejected(5,"Camera owner",camera,pawn);
    // Preserve the native vertical composition while opening the scene across the phone viewport.
    unsigned width=__atomic_load_n(&g_comp_w,__ATOMIC_RELAXED);
    unsigned height=__atomic_load_n(&g_comp_h,__ATOMIC_RELAXED);
    float aspect=4.0f/3.0f;
    if (height>=64 && width>=height && width<=16384 && height<=16384 && width<=4*height)
        aspect=(float)width/(float)height;
    int movie=cinema_movie_view(pawn,view,aspect);
    if (movie>0) goto captured;
    // Native screen-off/completion precedes the pawn switch; keep its last flat frame through that gap.
    if ((cinema_scene.closing || movie<0) && cinema_last.valid
        && cinema_last.pawn==pawn && cinema_last.camera==camera && cinema_last.pawn_name==name) {
        memcpy(view,cinema_last.projection,sizeof cinema_last.projection);return 1;
    }
    if (!cinema_scene.valid) return cinema_view_rejected(1,"scene not ready/screen off",camera,pawn);
    if (pawn!=cinema_scene.pawn || name!=cinema_scene.pawn_name)
        return cinema_view_rejected(2,"scene pawn pointer/FName",camera,pawn);
    struct cinema_rotator rotation=cinema_rotator(&cinema_scene.transform.q);
    float fov=2.0f*atanf(tanf(cinema_scene.fov_y*0.00872664625997165f)*aspect)*57.2957795130823f;
    if (!isfinite(rotation.pitch) || !isfinite(rotation.yaw) || !isfinite(rotation.roll)
        || !isfinite(fov) || fov<1.0f || fov>179.0f)
        return cinema_view_rejected(6,"rotation/FOV",camera,pawn);
    memcpy((char*)view+0x0,cinema_scene.transform.p,12);
    memcpy((char*)view+0xc,&rotation,12);
    memcpy((char*)view+0x18,&fov,4);
    memcpy((char*)view+0x2c,&aspect,4);
    *(unsigned char*)((char*)view+0x30)|=1; // Matching the live viewport gives native full-frame projection.
    *(unsigned char*)((char*)view+0x31)=0; // Perspective enum.
    if (cinema_view_logs<8) {
        ++cinema_view_logs;
        LOGF("CINEMA flat camera=%p pawn=%p fovX=%.4f aspect=%.6f viewport=%ux%u rotation=(%.3f,%.3f,%.3f)",
             camera,pawn,fov,aspect,width,height,rotation.pitch,rotation.yaw,rotation.roll);
    }
captured:
    cinema_last.pawn=pawn;cinema_last.camera=camera;cinema_last.pawn_name=name;
    memcpy(cinema_last.projection,view,sizeof cinema_last.projection);cinema_last.valid=1;
    return 1;
}

#ifndef RE4FIX_CINEMA_UPDATE_GATE
__attribute__((naked)) static void gate_cinema_update(void* pawn,const struct reload_tf* transform,float fov_y,int screen_on) {
    asm volatile(".inst 0xd101c3ff\n" ".inst 0xa9054ff4\n" ".inst 0xa9067bfd\n" ".inst 0x910183fd\n"
        "adrp x16, :got:g_tgt_cinema_update\n" "ldr x16, [x16, :got_lo12:g_tgt_cinema_update]\n"
        "ldr x16, [x16]\n" "br x16\n");
}
#else
#define gate_cinema_update RE4FIX_CINEMA_UPDATE_GATE
#endif
#ifndef RE4FIX_CINEMA_VIEW_GATE
__attribute__((naked)) static void gate_cinema_view(void* camera,float dt,void* view) {
    asm volatile(".inst 0xd10543ff\n" ".inst 0x6d0d33ed\n" ".inst 0x6d0e2beb\n" ".inst 0x6d0f23e9\n"
        "adrp x16, :got:g_tgt_cinema_view\n" "ldr x16, [x16, :got_lo12:g_tgt_cinema_view]\n"
        "ldr x16, [x16]\n" "br x16\n");
}
#else
#define gate_cinema_view RE4FIX_CINEMA_VIEW_GATE
#endif
static void hook_cinema_update(void* pawn,const struct reload_tf* transform,float fov_y,int screen_on) {
    gate_cinema_update(pawn,transform,fov_y,screen_on);
    cinema_capture(pawn,transform,fov_y,screen_on);
}
// Set by re4fix_view_extra.h (gameplay FOV / aim zoom / hold-to-skip); runs inside every GetCameraView.
static void (*cinema_view_extra)(void* camera,void* view);
static void (*cinema_menu_view)(void* camera,void* view);
static void (*cinema_health_view)(void* camera,void* view);
static void (*cinema_typewriter_view)(void* camera,void* view);
static void hook_cinema_view(void* camera,float dt,void* view) {
    gate_cinema_view(camera,dt,view);
    cinema_flat_movie_setting(camera);
    touch_cinema_ui_tick();
    cinema_flat_view(camera,view);
    if (cinema_view_extra) cinema_view_extra(camera,view);
    if (cinema_menu_view) cinema_menu_view(camera,view);
    if (cinema_health_view) cinema_health_view(camera,view);
    if (cinema_typewriter_view) cinema_typewriter_view(camera,view);
    if (cinema_on_game_thread() && cinema_type && cinema_active && cinema_camera_class
        && cinema_type()==3 && cinema_entry_logs<8) {
        ++cinema_entry_logs;
        int typed=cinema_object(camera,cinema_camera_class(),0x7b8);
        LOGF("CINEMA GetView camera=%p active=%p typed=%d owner=%p ViewProxy=%p sceneValid=%d",
             camera,cinema_active(),typed,
             typed ? *(void* const*)((const char*)camera+0xa0) : 0,
             typed ? *(void* const*)((const char*)camera+0x7b0) : 0,cinema_scene.valid);
    }
}
static void cinema_subtitle_diagnostic(const void* key,float seconds) {
    if (!key || !cinema_on_game_thread() || !cinema_type || !cinema_active || !cinema_pawn_class
        || cinema_type()!=3 || cinema_subtitle_logs>=16) return;
    void* pawn=cinema_active();
    if (!cinema_object(pawn,cinema_pawn_class(),0x1168)) return;
    const unsigned short* text=*(const unsigned short* const*)key;
    int count=*(const int*)((const char*)key+8),capacity=*(const int*)((const char*)key+12);
    if (!text || count<=0 || count>512 || capacity<count || capacity>8192
        || !isfinite(seconds) || seconds<0 || seconds>600) return;
    char name[96];unsigned i=0;
    for (;i<sizeof name-1 && i<(unsigned)count && text[i];i++) name[i]=text[i]<128 ? (char)text[i] : '?';
    name[i]=0;cinema_subtitle_logs++;
    LOGF("CINEMA subtitle pawn=%p key=%s duration=%.3f nativeOriginal=1",pawn,name,seconds);
}
#ifndef RE4FIX_CINEMA_SUBTITLE_GATE
__attribute__((naked)) static void gate_cinema_subtitle(void* manager,const void* key,float seconds) {
    asm volatile(".inst 0xd10183ff\n" ".inst 0xfd0013e8\n" ".inst 0xa90357f6\n" ".inst 0xa9044ff4\n"
        "adrp x16, :got:g_tgt_cinema_subtitle\n" "ldr x16, [x16, :got_lo12:g_tgt_cinema_subtitle]\n"
        "ldr x16, [x16]\n" "br x16\n");
}
#else
#define gate_cinema_subtitle RE4FIX_CINEMA_SUBTITLE_GATE
#endif
static void hook_cinema_subtitle(void* manager,const void* key,float seconds) {
    gate_cinema_subtitle(manager,key,seconds);
    cinema_subtitle_diagnostic(key,seconds);
}
static void install_touch_cinema(unsigned long base) {
    static const struct { unsigned long addr;unsigned first[2]; } check[] = {
        {0x63e5b50,{0xd101c3ff,0xa9054ff4}}, {0x63e5b58,{0xa9067bfd,0x910183fd}},
        {0x622d80c,{0xd10543ff,0x6d0d33ed}}, {0x622d814,{0x6d0e2beb,0x6d0f23e9}},
        {0x622df88,{0xf943da95,0xb4000235}}, {0x622dfd4,{0xf940eaa8,0xb941daa9}},
        {0x63380dc,{0xd0020168,0xf9431508}}, {0x6338aec,{0xd0020168,0xf9431508}},
        {0x63e60c8,{0xd10183ff,0xf90023f3}}, {0x63d4d30,{0xd10183ff,0xf90023f3}},
        {0x66df968,{0x6dbc2beb,0x6d0123e9}}, {0x6245250,{0xbd415500,0x9406823f}},
        {0x5d4f194,{0xbd4156c0,0x7108051f}}, {0x5d4f1e0,{0x52955568,0x72a7f548}},
        {0x5d36538,{0x72a791c8,0x1e210800}}, {0x63403c0,{0xf9416e60,0x1e2703e0}},
        {0x8301c9c,{0x29002668,0xb9000a6a}}, {0x622e068,{0xbd001a60,0xb9420288}},
        {0x622e074,{0xb9002e68,0x39481288}}, {0x622e080,{0x3900c269,0x39481288}},
        {0x622e09c,{0x3900c668,0xb941f688}},
        {0x63e2824,{0xd10183ff,0xfd0013e8}}, {0x63e282c,{0xa90357f6,0xa9044ff4}},
        {0x8892194,{0xa9bf7bfd,0x910003fd}}, {0x89766dc,{0xa9bd57f6,0xa9014ff4}},
    };
    static const struct { unsigned long params;unsigned offset; } fields[] = {
        {0x9dff840,0x2d8}, {0x9d80360,0x30}, {0x9d80388,0x0},
        {0xa0f9448,0x0}, {0xa0f9418,0xc}, {0xa0f93f0,0x18},
        {0xa0f9328,0x2c}, {0xa0f9288,0x31}, {0x9d77400,0x7b0},
    };
    if (!base || cinema_rotator) return;
    for (unsigned i=0;i<sizeof check/sizeof check[0];i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,sizeof check[i].first)) {
            LOGF("CINEMA disabled: opcode mismatch +%lx",check[i].addr);return;
        }
    for (unsigned i=0;i<sizeof fields/sizeof fields[0];i++)
        if (*(const unsigned*)(base+fields[i].params+0x24)!=fields[i].offset) {
            LOGF("CINEMA disabled: property mismatch +%lx",fields[i].params);return;
        }
    cinema_active=(void* (*)(void))(base+0x63380dc);
    cinema_type=(unsigned (*)(void))(base+0x6338aec);
    cinema_pawn_class=(void* (*)(void))(base+0x63e60c8);
    cinema_camera_class=(void* (*)(void))(base+0x63d4d30);
    cinema_rotator=(struct cinema_rotator (*)(const qf_t*))(base+0x66df968);
    cinema_game_instance=(void* (*)(const void*))(base+0x8892194);
    cinema_set_bool=(void (*)(void*,unsigned long long,int))(base+0x89766dc);
    g_tgt_cinema_update=base+0x63e5b50+16;
    g_tgt_cinema_view=base+0x622d80c+16;
    g_tgt_cinema_subtitle=base+0x63e2824+16;
    if (patch_code((void*)(base+0x63e5b50),(void*)&hook_cinema_update)!=0
        || patch_code((void*)(base+0x622d80c),(void*)&hook_cinema_view)!=0
        || patch_code((void*)(base+0x63e2824),(void*)&hook_cinema_subtitle)!=0) {
        cinema_rotator=0;LOGF("CINEMA disabled: native hook installation failed");return;
    }
    LOGF("CINEMA ready: original Blueprint playback, owned native camera output uses authored scene/FOV/aspect");
    install_cinema_ui(base);
}
#endif
