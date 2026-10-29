// Present the authored subtitle widget outside the offstage VR theatre.
#ifndef RE4FIX_CINEMA_UI_H
#define RE4FIX_CINEMA_UI_H
struct cinema_ui_weak { int index,serial; };
_Static_assert(sizeof(struct cinema_ui_weak)==8,"native FWeakObjectPtr ABI");
struct cinema_ui_vec2 { float x,y; };
struct cinema_ui_anchors { float min_x,min_y,max_x,max_y; };
_Static_assert(sizeof(struct cinema_ui_vec2)==8,"native FVector2D HFA");
_Static_assert(sizeof(struct cinema_ui_anchors)==16,"native FAnchors HFA");
static void (*cu_weak_assign)(struct cinema_ui_weak*,const void*);
static void* (*cu_weak_get)(const struct cinema_ui_weak*);
static void* (*cu_user_class)(void), *(*cu_component_class)(void), *(*cu_get_widget)(const void*);
static void (*cu_add)(void*,int), (*cu_remove)(void*);
static int (*cu_in_viewport)(const void*);
static struct cinema_ui_vec2 (*cu_draw_size)(const void*);
static float (*cu_viewport_scale)(void*);
static void (*cu_set_scale)(void*,struct cinema_ui_vec2), (*cu_set_pivot)(void*,struct cinema_ui_vec2);
static void (*cu_set_position)(void*,struct cinema_ui_vec2,int);
static void (*cu_set_size)(void*,struct cinema_ui_vec2), (*cu_set_alignment)(void*,struct cinema_ui_vec2);
static void (*cu_set_anchors)(void*,struct cinema_ui_anchors);
static void* (*cu_text_class)(void),*(*cu_tree_class)(void);
static void* (*cu_find_widget)(const void*,const void*);
static float (*cu_render_opacity)(const void*);
static void* cu_font_last;
static unsigned long long cu_font_name;
static unsigned cu_font_logs;
static struct cinema_ui_weak cu_widget={-1,0};
static int cu_added,cu_attempted,cu_ready;
static int cu_layout;
static struct {
    struct cinema_ui_vec2 scale,pivot,position,size,alignment;
    struct cinema_ui_anchors anchors;
} cu_original;
static unsigned cu_logs,cu_rejections;

static int cinema_ui_named(const void* object,const char* expected,int minimum) {
    const char* cls=object ? *(const char* const*)((const char*)object+0x10) : 0;
    int size=cls ? *(const int*)(cls+0x58) : 0;
    if (size<minimum || size>0x10000) return 0;
    char name[96];phone_inspect_fname(cls+0x18,name,sizeof name);
    return !strcmp(name,expected);
}
static void* cinema_ui_property(void* object,const char* class_name,const char* wanted,int offset) {
    if (!cinema_ui_named(object,class_name,offset+8)) return 0;
    const char* cls=*(const char* const*)((const char*)object+0x10);
    const char* field=*(const char* const*)(cls+0x70);
    for (unsigned i=0;field && i<256;i++) {
        char name[64],kind[48];phone_inspect_fname(field+0x28,name,sizeof name);
        if (!strcmp(name,wanted)) {
            phone_inspect_fname(*(const void* const*)(field+0x8),kind,sizeof kind);
            if (strcmp(kind,"ObjectProperty") || *(const int*)(field+0x38)!=8
                || *(const int*)(field+0x34)!=1 || *(const int*)(field+0x4c)!=offset) return 0;
            return *(void* const*)((const char*)object+offset);
        }
        const char* next=*(const char* const*)(field+0x58);if (next==field) break;field=next;
    }
    return 0;
}
// Explicit diagnostic seed only; shares the production ownership and native type guards.
static void* cinema_subtitle_probe(void* pawn) {
    if (!__atomic_load_n(&cu_ready,__ATOMIC_ACQUIRE) || !cinema_on_game_thread()
        || !phone_inspect_ready || !cinema_active || !cinema_type || !cinema_pawn_class
        || cinema_type()!=3 || pawn!=cinema_active()
        || !cinema_object(pawn,cinema_pawn_class(),0x1170)) return 0;
    void* theatre=cinema_ui_property(pawn,"VR4CutscenePlayerPawn_BP_C","TheatreBox",0x1198);
    void* component=cinema_ui_property(theatre,"CutsceneTheatreBox_C","WidgetSubtitles",0x2b0);
    void* widget=cinema_ui_property(theatre,"CutsceneTheatreBox_C","SubtitleWidget",0x338);
    if (!component || !widget || !cinema_object(component,cu_component_class(),0x4a0)
        || *(void* const*)((const char*)component+0x20)!=theatre
        || *(void* const*)((const char*)component+0xa0)!=theatre
        || cu_get_widget(component)!=widget || !cinema_object(widget,cu_user_class(),0x230)
        || !cinema_ui_named(widget,"Subtitles_C",0x230)) return 0;
    return widget;
}
static void cinema_ui_font_probe(void* widget) {
    if (cu_font_logs>=8 || !cinema_on_game_thread() || !phone_inspect_ready
        || !cinema_object(widget,cu_user_class(),0x230)
        || !cinema_ui_named(widget,"Subtitles_C",0x230)) return;
    void* tree=*(void* const*)((const char*)widget+0x1a8);
    if (!cinema_object(tree,cu_tree_class(),0x30)
        || *(void* const*)((const char*)tree+0x20)!=widget) return;
    const char* cls=*(const char* const*)((const char*)widget+0x10);
    int bytes=*(const int*)(cls+0x58);
    const char* field=*(const char* const*)(cls+0x70);
    for (unsigned i=0;field && i<256 && cu_font_logs<8;i++) {
        int offset=*(const int*)(field+0x4c);
        if (offset>=0x230 && offset<=bytes-8 && *(const int*)(field+0x34)==1
            && *(const int*)(field+0x38)==8) {
            char kind[48];phone_inspect_fname(*(const void* const*)(field+8),kind,sizeof kind);
            if (!strcmp(kind,"ObjectProperty")) {
                void* text=*(void* const*)((const char*)widget+offset);
                if (cinema_object(text,cu_text_class(),0x2a0)
                    && *(void* const*)((const char*)text+0x20)==tree
                    && cu_find_widget(widget,(const char*)text+0x18)==text) {
                    unsigned long long name;memcpy(&name,(const char*)text+0x18,8);
                    if (text!=cu_font_last || name!=cu_font_name) {
                        cu_font_last=text;cu_font_name=name;cu_font_logs++;
                        const char* font=(const char*)text+0x188;
                        void* objects[3];memcpy(&objects[0],font,8);memcpy(&objects[1],font+8,8);memcpy(&objects[2],font+0x18,8);
                        char names[3][96]={{0}},classes[3][96]={{0}},fn[64],face[64];
                        for (unsigned n=0;n<3;n++) if (objects[n]) {
                            phone_inspect_fname((const char*)objects[n]+0x18,names[n],sizeof names[n]);
                            const char* oc=*(const char* const*)((const char*)objects[n]+0x10);
                            if (oc) phone_inspect_fname(oc+0x18,classes[n],sizeof classes[n]);
                        }
                        phone_inspect_fname(field+0x28,fn,sizeof fn);phone_inspect_fname(font+0x40,face,sizeof face);
                        LOGF("CINEMA FONT field='%s' child=%p font=%p '%s' class='%s' material=%p '%s' class='%s' outline=%p '%s' class='%s' size=%d face='%s' composite=(%p,%p)",
                             fn,text,objects[0],names[0],classes[0],objects[1],names[1],classes[1],objects[2],names[2],classes[2],
                             *(const int*)(font+0x48),face,*(void* const*)(font+0x30),*(void* const*)(font+0x38));
                        unsigned color[10];float rgba[4];memcpy(color,(const char*)text+0x150,sizeof color);memcpy(rgba,color,sizeof rgba);
                        LOGF("CINEMA FONT color child=%p rgba=(%g,%g,%g,%g) rule=%u raw=%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x opacity=%g rootOpacity=%g",
                             text,rgba[0],rgba[1],rgba[2],rgba[3],*((const unsigned char*)color+0x10),
                             color[0],color[1],color[2],color[3],color[4],color[5],color[6],color[7],color[8],color[9],
                             cu_render_opacity(text),cu_render_opacity(widget));
                    }
                }
            }
        }
        const char* next=*(const char* const*)(field+0x58);if (next==field) break;field=next;
    }
}
static void cinema_ui_restore(void* widget) {
    if (!cu_layout) return;
    cu_set_scale(widget,cu_original.scale);cu_set_pivot(widget,cu_original.pivot);
    cu_set_position(widget,cu_original.position,0);cu_set_size(widget,cu_original.size);
    cu_set_alignment(widget,cu_original.alignment);cu_set_anchors(widget,cu_original.anchors);
    cu_layout=0;
}
static int cinema_ui_layout(void* widget,void* component) {
    int w=__atomic_load_n(&g_comp_w,__ATOMIC_ACQUIRE),h=__atomic_load_n(&g_comp_h,__ATOMIC_ACQUIRE);
    if (h<64 || w<h || w>16384 || h>16384 || (float)w/h>4.0f) return 0;
    float dpi=cu_viewport_scale(widget);
    struct cinema_ui_vec2 draw=cu_draw_size(component),scale,pivot;
    memcpy(&scale,(const char*)widget+0x98,sizeof scale);
    memcpy(&pivot,(const char*)widget+0xac,sizeof pivot);
    if (!isfinite(dpi) || dpi<0.25f || dpi>4.0f
        || !isfinite(draw.x) || !isfinite(draw.y) || draw.x<=0 || draw.y<=0
        || draw.x>16384 || draw.y>16384
        || !isfinite(scale.x) || !isfinite(scale.y) || scale.x<0.01f || scale.y<0.01f
        || scale.x>3.2f || scale.y>3.2f
        || !isfinite(pivot.x) || !isfinite(pivot.y)) return 0;
    cu_original.scale=scale;cu_original.pivot=pivot;
    memcpy(&cu_original.anchors,(const char*)widget+0x1e0,sizeof cu_original.anchors);
    memcpy(&cu_original.position,(const char*)widget+0x1f0,sizeof cu_original.position);
    memcpy(&cu_original.size,(const char*)widget+0x1f8,sizeof cu_original.size);
    memcpy(&cu_original.alignment,(const char*)widget+0x200,sizeof cu_original.alignment);
    // Uniform visual calibration of the p254 authored caption; keep native wrapping and text.
    scale.x*=2.5f;scale.y*=2.5f;
    struct cinema_ui_vec2 size={(float)w*0.88f/(dpi*scale.x),draw.y};
    if (!isfinite(size.x) || size.x<32.0f || size.x>16384.0f) return 0;
    cu_layout=1;
    cu_set_scale(widget,scale);cu_set_pivot(widget,(struct cinema_ui_vec2){0.5f,0});
    cu_set_position(widget,(struct cinema_ui_vec2){w*0.5f,h*0.80f},1);   // room for three caption lines
    cu_set_size(widget,size);cu_set_alignment(widget,(struct cinema_ui_vec2){0.5f,0});
    cu_set_anchors(widget,(struct cinema_ui_anchors){0,0,0,0});
    if (cu_logs<12) {cu_logs++;LOGF("CINEMA UI layout viewport=%dx%d dpi=%g draw=(%g,%g) scale=(%g,%g) width=%g top=80%%",
                                  w,h,dpi,draw.x,draw.y,scale.x,scale.y,size.x);}
    return 1;
}
static void cinema_ui_cleanup(void) {
    void* widget=cu_weak_get(&cu_widget);
    if (widget && cinema_object(widget,cu_user_class(),0x230)
        && cinema_ui_named(widget,"Subtitles_C",0x230)) {
        if (cu_added && cu_in_viewport(widget)) {
            cu_remove(widget);
            if (cu_logs<12) {cu_logs++;LOGF("CINEMA UI removed own viewport subtitle widget=%p",widget);}
        }
        cinema_ui_restore(widget);
    }
    cu_weak_assign(&cu_widget,0);cu_added=cu_attempted=cu_layout=0;cu_font_last=0;
}
static void touch_cinema_ui_tick(void) {
    if (!__atomic_load_n(&cu_ready,__ATOMIC_ACQUIRE) || !cinema_on_game_thread()
        || !phone_inspect_ready || !cinema_active || !cinema_type || !cinema_pawn_class) return;
    if (cinema_type()!=3) {cinema_ui_cleanup();cu_font_logs=0;return;}
    void* pawn=cinema_active();
    void* theatre=cinema_object(pawn,cinema_pawn_class(),0x1170)
        ? cinema_ui_property(pawn,"VR4CutscenePlayerPawn_BP_C","TheatreBox",0x1198) : 0;
    void* component=cinema_ui_property(theatre,"CutsceneTheatreBox_C","WidgetSubtitles",0x2b0);
    void* widget=cinema_ui_property(theatre,"CutsceneTheatreBox_C","SubtitleWidget",0x338);
    if (!component || !widget || !cinema_object(component,cu_component_class(),0x4a0)
        || *(void* const*)((const char*)component+0x20)!=theatre
        || *(void* const*)((const char*)component+0xa0)!=theatre
        || cu_get_widget(component)!=widget || !cinema_object(widget,cu_user_class(),0x230)
        || !cinema_ui_named(widget,"Subtitles_C",0x230)) {
        cinema_ui_cleanup();
        if (cu_rejections<3) {
            cu_rejections++;LOGF("CINEMA UI binding deferred pawn=%p theatre=%p component=%p widget=%p",
                                pawn,theatre,component,widget);
        }
        return;
    }
    void* previous=cu_weak_get(&cu_widget);
    if (previous!=widget) {cinema_ui_cleanup();cu_weak_assign(&cu_widget,widget);}
    cinema_ui_font_probe(widget);
    // Original component visibility remains authoritative for subtitle options and timing.
    unsigned flags=*(const unsigned short*)((const char*)component+0x14c);
    if (!(flags&0x20) || (flags&0x400)) {
        if (cu_added && cu_in_viewport(widget)) cu_remove(widget);
        cinema_ui_restore(widget);
        cu_added=cu_attempted=0;return;
    }
    if (cu_in_viewport(widget) || cu_attempted) return;
    if (!cinema_ui_layout(widget,component)) return;
    cu_attempted=1;cu_add(widget,10);cu_added=cu_in_viewport(widget)!=0;
    if (!cu_added) cinema_ui_restore(widget);
    if (cu_logs<12) {
        cu_logs++;LOGF("CINEMA UI authored Subtitles widget=%p component=%p theatre=%p viewport=%d",
                      widget,component,theatre,cu_added);
    }
}
static void install_cinema_ui(unsigned long base) {
    static const struct { unsigned long address;unsigned words[2]; } check[] = {
        {0x69defcc,{0xf81e0ff3,0xa9017bfd}}, {0x69df1cc,{0xb9400408,0x34000368}},
        {0x7dd5c58,{0xf9400008,0x2a0103e2}}, {0x7dd70f4,{0xd10203ff,0xf9001bf9}},
        {0x7dd7468,{0xf9410408,0xb4000108}}, {0x7e37bec,{0xd10183ff,0xf90023f3}},
        {0x7db1a28,{0xf9424c00,0xd65f03c0}}, {0x7e4bcdc,{0xd10183ff,0xf90023f3}},
        {0x7e37c74,{0x52804604,0x52800105}},
        {0x7da30fc,{0x2d130400,0x17fffeb4}}, {0x7da3274,{0xd101c3ff,0x6d0323e9}},
        {0x7db1fb4,{0xbd443800,0xbd443c01}}, {0x7de6f14,{0xd10143ff,0xfd000be8}},
        {0x7dd762c,{0x6dbd23e9,0xf9000bf3}}, {0x7dd7678,{0xbd01f800,0xbd01fc01}},
        {0x7dd7688,{0xbd01e000,0xbd01e401}}, {0x7dd769c,{0xbd020000,0xbd020401}},
        {0x7e296b4,{0xd10183ff,0xf90023f3}}, {0x7e52bf4,{0xd10183ff,0xf90023f3}},
        {0x7dd5818,{0xf940d400,0xb4000040}}, {0x7da4524,{0xfc1c0fe8,0xa90157f6}},
        {0x7e29738,{0x52805404,0x52800105}}, {0x7e52c70,{0x52800604,0x52800105}},
    };
    if (!base || __atomic_load_n(&cu_ready,__ATOMIC_ACQUIRE) || !phone_inspect_ready) return;
    for (unsigned i=0;i<sizeof check/sizeof check[0];i++)
        if (memcmp((const void*)(base+check[i].address),check[i].words,sizeof check[i].words)) {
            LOGF("CINEMA UI disabled: native mismatch +%lx",check[i].address);return;
        }
    if (*(const unsigned*)(base+0x9f77968+0x24)!=0x498) return;
    if (*(const unsigned*)(base+0x9f701c0+0x24)!=0x90
        || *(const unsigned*)(base+0x9f70190+0x24)!=0xac
        || *(const unsigned*)(base+0x9f7bd18+0x24)!=8
        || *(const unsigned*)(base+0x9f7bd98+0x20)!=28) return;
    if (*(const unsigned*)(base+0x9f6b718+0x24)!=0x1a8
        || *(const unsigned*)(base+0x9f64bd0+0x24)!=0x188
        || *(const unsigned*)(base+0x9f64c30+0x24)!=0x150
        || *(const unsigned*)(base+0x9e51020+0x20)!=0x50
        || *(const unsigned*)(base+0x9e50f98+0x24)!=8
        || *(const unsigned*)(base+0x9e50f68+0x24)!=0x10
        || *(const unsigned*)(base+0x9e51098+0x24)!=8
        || *(const unsigned*)(base+0x9e50f40+0x24)!=0x40
        || *(const unsigned*)(base+0x9e50f18+0x24)!=0x48
        || *(const unsigned*)(base+0x9e503b8+0x20)!=0x28
        || *(const unsigned*)(base+0x9e50348+0x24)!=0x10) return;
    cu_weak_assign=(void (*)(struct cinema_ui_weak*,const void*))(base+0x69defcc);
    cu_weak_get=(void* (*)(const struct cinema_ui_weak*))(base+0x69df1cc);
    cu_user_class=(void* (*)(void))(base+0x7e37bec);
    cu_component_class=(void* (*)(void))(base+0x7e4bcdc);
    cu_get_widget=(void* (*)(const void*))(base+0x7db1a28);
    cu_add=(void (*)(void*,int))(base+0x7dd5c58);cu_remove=(void (*)(void*))(base+0x7dd70f4);
    cu_in_viewport=(int (*)(const void*))(base+0x7dd7468);
    cu_draw_size=(struct cinema_ui_vec2 (*)(const void*))(base+0x7db1fb4);
    cu_viewport_scale=(float (*)(void*))(base+0x7de6f14);
    cu_set_scale=(void (*)(void*,struct cinema_ui_vec2))(base+0x7da30fc);
    cu_set_pivot=(void (*)(void*,struct cinema_ui_vec2))(base+0x7da3274);
    cu_set_position=(void (*)(void*,struct cinema_ui_vec2,int))(base+0x7dd762c);
    cu_set_size=(void (*)(void*,struct cinema_ui_vec2))(base+0x7dd7678);
    cu_set_anchors=(void (*)(void*,struct cinema_ui_anchors))(base+0x7dd7688);
    cu_set_alignment=(void (*)(void*,struct cinema_ui_vec2))(base+0x7dd769c);
    cu_text_class=(void* (*)(void))(base+0x7e296b4);cu_tree_class=(void* (*)(void))(base+0x7e52bf4);
    cu_find_widget=(void* (*)(const void*,const void*))(base+0x7dd5818);
    cu_render_opacity=(float (*)(const void*))(base+0x7da4524);
    __atomic_store_n(&cu_ready,1,__ATOMIC_RELEASE);
    LOGF("CINEMA UI ready: exact authored subtitle widget, native viewport and weak cleanup");
}
#endif
