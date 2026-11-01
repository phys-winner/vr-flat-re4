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
static void (*cu_set_main_pass)(void*,int);
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
static void (*cu_set_auto_wrap)(void*,int);
static void* (*cu_overlay_slot_class)(void),*(*cu_box_slot_class)(void);
static void* (*cu_overlay_class)(void),*(*cu_box_class)(void);
static void (*cu_set_overlay_halign)(void*,int),(*cu_set_box_halign)(void*,int);
static void* cu_font_last;
static unsigned long long cu_font_name;
static unsigned cu_font_logs;
static struct cinema_ui_weak cu_widget={-1,0};
static struct cinema_ui_weak cu_quad_component={-1,0},cu_quad_theatre={-1,0};
static int cu_quad_changed,cu_quad_original;
static struct cinema_ui_weak cu_wrap_text={-1,0};
static int cu_wrap_changed;
static struct cinema_ui_weak cu_fill_slots[2]={{-1,0},{-1,0}};
static unsigned char cu_fill_original[2];
static unsigned cu_fill_changed;
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
                        float outline_rgba[4],shadow[6];unsigned raw_font[20];
                        memcpy(outline_rgba,font+0x20,sizeof outline_rgba);
                        memcpy(shadow,(const char*)text+0x260,sizeof shadow);memcpy(raw_font,font,sizeof raw_font);
                        LOGF("CINEMA FONT paint child=%p outlineSize=%d separateFill=%u applyShadow=%u outlineRGBA=(%g,%g,%g,%g) shadowOffset=(%g,%g) shadowRGBA=(%g,%g,%g,%g) slate=%p fallback=%u",
                             text,*(const int*)(font+0x10),(unsigned)*(const unsigned char*)(font+0x14),
                             (unsigned)*(const unsigned char*)(font+0x15),
                             outline_rgba[0],outline_rgba[1],outline_rgba[2],outline_rgba[3],
                             shadow[0],shadow[1],shadow[2],shadow[3],shadow[4],shadow[5],
                             *(void* const*)((const char*)text+0x290),(unsigned)*(const unsigned char*)(font+0x4c));
                        LOGF("CINEMA FONT raw child=%p font80=%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x",
                             text,raw_font[0],raw_font[1],raw_font[2],raw_font[3],raw_font[4],raw_font[5],raw_font[6],raw_font[7],
                             raw_font[8],raw_font[9],raw_font[10],raw_font[11],raw_font[12],raw_font[13],raw_font[14],raw_font[15],
                             raw_font[16],raw_font[17],raw_font[18],raw_font[19]);
                    }
                }
            }
        }
        const char* next=*(const char* const*)(field+0x58);if (next==field) break;field=next;
    }
}
static void* cinema_ui_subtitle_text(void* widget) {
    if (!cinema_object(widget,cu_user_class(),0x238)
        || !cinema_ui_named(widget,"Subtitles_C",0x238)) return 0;
    void* tree=*(void* const*)((const char*)widget+0x1a8);
    if (!cinema_object(tree,cu_tree_class(),0x30)
        || *(void* const*)((const char*)tree+0x20)!=widget) return 0;
    void* text=cinema_ui_property(widget,"Subtitles_C","SubtitleText",0x230);
    return cinema_object(text,cu_text_class(),0x2a0)
        && *(void* const*)((const char*)text+0x20)==tree
        && cu_find_widget(widget,(const char*)text+0x18)==text ? text : 0;
}
static int cinema_ui_subtitle_slots(void* widget,void* text,void** slots) {
    void* tree=*(void* const*)((const char*)widget+0x1a8);
    void* slot=*(void* const*)((const char*)text+0x28);
    if (!cinema_object(slot,cu_overlay_slot_class(),0x58)
        || *(void* const*)((const char*)slot+0x30)!=text) return 0;
    void* overlay=*(void* const*)((const char*)slot+0x28);
    if (!cinema_object(overlay,cu_overlay_class(),0x130)
        || *(void* const*)((const char*)slot+0x20)!=overlay
        || *(void* const*)((const char*)overlay+0x20)!=tree) return 0;
    slots[0]=slot;slot=*(void* const*)((const char*)overlay+0x28);
    if (!cinema_object(slot,cu_box_slot_class(),0x60)
        || *(void* const*)((const char*)slot+0x30)!=overlay) return 0;
    void* box=*(void* const*)((const char*)slot+0x28);
    if (!cinema_object(box,cu_box_class(),0x158)
        || *(void* const*)((const char*)slot+0x20)!=box
        || *(void* const*)((const char*)box+0x20)!=tree
        || *(void* const*)((const char*)tree+0x28)!=box) return 0;
    slots[1]=slot;return 1;
}
static void cinema_ui_wrap_restore(void* widget) {
    void* text=cu_weak_get(&cu_wrap_text);
    if (text && cinema_ui_subtitle_text(widget)==text) {
        void* owned[2]={cu_weak_get(&cu_fill_slots[0]),cu_weak_get(&cu_fill_slots[1])},*slots[2];
        if (cu_fill_changed && owned[0] && owned[1]
            && *(void* const*)((const char*)text+0x28)==owned[0]
            && cinema_ui_subtitle_slots(widget,text,slots)
            && slots[0]==owned[0] && slots[1]==owned[1]) {
            if ((cu_fill_changed&1) && !*(const unsigned char*)((const char*)slots[0]+0x50))
                cu_set_overlay_halign(slots[0],cu_fill_original[0]);
            if ((cu_fill_changed&2) && !*(const unsigned char*)((const char*)slots[1]+0x58))
                cu_set_box_halign(slots[1],cu_fill_original[1]);
        }
        if (cu_wrap_changed && (*(const unsigned char*)((const char*)text+0x10d)&1))
            cu_set_auto_wrap(text,0);
    }
    cu_weak_assign(&cu_wrap_text,0);cu_wrap_changed=0;
    for (unsigned i=0;i<2;i++) cu_weak_assign(&cu_fill_slots[i],0);
    cu_fill_changed=0;
}
static void cinema_ui_wrap(void* widget) {
    // Use Slate's original word wrapping for the allotted phone width; preserve authored newlines.
    void* text=cinema_ui_subtitle_text(widget);
    if (!text) return;
    cu_weak_assign(&cu_wrap_text,text);
    if (cu_weak_get(&cu_wrap_text)!=text) return;
    void* slots[2];
    if (cinema_ui_subtitle_slots(widget,text,slots)) {
        unsigned char original[2]={*(const unsigned char*)((const char*)slots[0]+0x50),
                                   *(const unsigned char*)((const char*)slots[1]+0x58)};
        if (original[0]<=3 && original[1]<=3) {
            for (unsigned i=0;i<2;i++) cu_weak_assign(&cu_fill_slots[i],slots[i]);
            if (cu_weak_get(&cu_fill_slots[0])==slots[0] && cu_weak_get(&cu_fill_slots[1])==slots[1]) {
                memcpy(cu_fill_original,original,sizeof original);
                // Centered slots shrink to wrapped desired size and feed that narrow width back.
                // Fill both containers; the TextBlock's own centered justification stays native.
                if (original[0]) {cu_set_overlay_halign(slots[0],0);cu_fill_changed|=1;}
                if (original[1]) {cu_set_box_halign(slots[1],0);cu_fill_changed|=2;}
            }
        }
    }
    if (!(*(const unsigned char*)((const char*)text+0x10d)&1)) {
        cu_set_auto_wrap(text,1);cu_wrap_changed=1;
    }
    if (cu_logs<12) {cu_logs++;LOGF("CINEMA UI wrap text=%p fill=%u nativeAlign=(%u,%u) autoChanged=%d",
                                  text,cu_fill_changed,(unsigned)cu_fill_original[0],(unsigned)cu_fill_original[1],cu_wrap_changed);}
}
static void cinema_ui_restore(void* widget) {
    cinema_ui_wrap_restore(widget);
    if (!cu_layout) return;
    cu_set_scale(widget,cu_original.scale);cu_set_pivot(widget,cu_original.pivot);
    cu_set_position(widget,cu_original.position,0);cu_set_size(widget,cu_original.size);
    cu_set_alignment(widget,cu_original.alignment);cu_set_anchors(widget,cu_original.anchors);
    cu_layout=0;
}
static int cinema_ui_quad_binding(void* component,void* theatre,void* widget) {
    return component && theatre && widget
        && cinema_ui_property(theatre,"CutsceneTheatreBox_C","WidgetSubtitles",0x2b0)==component
        && cinema_ui_property(theatre,"CutsceneTheatreBox_C","SubtitleWidget",0x338)==widget
        && cinema_object(component,cu_component_class(),0x4a0)
        && *(void* const*)((const char*)component+0x20)==theatre
        && *(void* const*)((const char*)component+0xa0)==theatre
        && cu_get_widget(component)==widget && cinema_object(widget,cu_user_class(),0x230)
        && cinema_ui_named(widget,"Subtitles_C",0x230);
}
static void cinema_ui_quad_restore(void) {
    if (cu_quad_changed) {
        void* component=cu_weak_get(&cu_quad_component);
        void* theatre=cu_weak_get(&cu_quad_theatre);
        void* widget=cu_weak_get(&cu_widget);
        if (cinema_ui_quad_binding(component,theatre,widget)
            && !(*(const unsigned*)((const char*)component+0x208)&0x20000))
            cu_set_main_pass(component,cu_quad_original);
    }
    cu_weak_assign(&cu_quad_component,0);cu_weak_assign(&cu_quad_theatre,0);
    cu_quad_changed=cu_quad_original=0;
}
static void cinema_ui_quad_hide(void* component,void* theatre,void* widget) {
    // Change only our owned world quad, after the same authored widget is visible in our viewport.
    // Native visibility still drives timing/options. Shared Slate opacity and tint stay native.
    if (cu_weak_get(&cu_quad_component)==component && cu_weak_get(&cu_quad_theatre)==theatre) return;
    cinema_ui_quad_restore();
    if (!cinema_ui_quad_binding(component,theatre,widget)
        || *(const unsigned char*)((const char*)component+0x429)!=0
        || (*(const unsigned char*)((const char*)component+0x20e)&0x10)) return;
    // The original Widget3D proxy has no separate depth pass. Refuse an authored custom-depth draw.
    cu_weak_assign(&cu_quad_component,component);cu_weak_assign(&cu_quad_theatre,theatre);
    if (cu_weak_get(&cu_quad_component)!=component || cu_weak_get(&cu_quad_theatre)!=theatre) {
        cinema_ui_quad_restore();return;
    }
    cu_quad_original=(*(const unsigned*)((const char*)component+0x208)&0x20000)!=0;
    if (cu_quad_original) {
        cu_set_main_pass(component,0);
        cu_quad_changed=(*(const unsigned*)((const char*)component+0x208)&0x20000)==0;
        if (cu_logs<12) {cu_logs++;LOGF("CINEMA UI suppressed owned world subtitle quad=%p native main-pass=%d",component,cu_quad_original);}
    }
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
    // The authored104-DIP caption root stays above the bottom margin at every render scale.
    float magnification=2.5f*(float)h/(1080.0f*dpi);
    scale.x*=magnification;scale.y*=magnification;
    struct cinema_ui_vec2 size={(float)w*0.88f/(dpi*scale.x),104.0f};
    if (!isfinite(size.x) || size.x<32.0f || size.x>16384.0f) return 0;
    cu_layout=1;
    cu_set_scale(widget,scale);cu_set_pivot(widget,(struct cinema_ui_vec2){0.5f,1});
    cu_set_position(widget,(struct cinema_ui_vec2){w*0.5f,h*0.96f},1);
    cu_set_size(widget,size);cu_set_alignment(widget,(struct cinema_ui_vec2){0.5f,1});
    cu_set_anchors(widget,(struct cinema_ui_anchors){0,0,0,0});
    if (cu_logs<12) {cu_logs++;LOGF("CINEMA UI layout viewport=%dx%d dpi=%g draw=(%g,%g) scale=(%g,%g) width=%g bottom=96%%",
                                  w,h,dpi,draw.x,draw.y,scale.x,scale.y,size.x);}
    return 1;
}
static void cinema_ui_cleanup(void) {
    cinema_ui_quad_restore();
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
    cinema_ui_wrap_restore(0);
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
        cinema_ui_quad_restore();
        if (cu_added && cu_in_viewport(widget)) cu_remove(widget);
        cinema_ui_restore(widget);
        cu_added=cu_attempted=0;return;
    }
    if (cu_in_viewport(widget)) {
        if (cu_added) cinema_ui_quad_hide(component,theatre,widget);
        return;
    }
    cinema_ui_quad_restore();
    if (cu_attempted) {cinema_ui_restore(widget);cu_added=0;return;}
    if (!cinema_ui_layout(widget,component)) return;
    cu_attempted=1;cu_add(widget,10);cu_added=cu_in_viewport(widget)!=0;
    if (cu_added) {cinema_ui_wrap(widget);cinema_ui_quad_hide(component,theatre,widget);}
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
        {0x838db28,{0xb9420808,0x4a484429}}, {0x8f5f8f0,{0xb9420808,0x320f0108}},
        {0x8f5f4e0,{0xb24c0169,0xd370fd29}},
        {0x7d89f04,{0xd10103ff,0xf90013f3}}, {0x7d89f24,{0x39443409,0xf9414800}},
        {0x7e2a0bc,{0x39443408,0x32000108}},
        {0x7e13474,{0xd10183ff,0xf90023f3}}, {0x7e202c4,{0xd10183ff,0xf90023f3}},
        {0x7e12a14,{0xd10183ff,0xf90023f3}}, {0x7e1f864,{0xd10183ff,0xf90023f3}},
        {0x7e134f8,{0x52800b04,0x52800105}}, {0x7e20348,{0x52800c04,0x52800105}},
        {0x7e12a98,{0x52802604,0x52800105}}, {0x7e1f8e8,{0x52802b04,0x52800105}},
        {0x7d6e744,{0xf9401c08,0x39014001}}, {0x7d8284c,{0xa9be4ff4,0xa9017bfd}},
    };
    if (!base || __atomic_load_n(&cu_ready,__ATOMIC_ACQUIRE) || !phone_inspect_ready) return;
    for (unsigned i=0;i<sizeof check/sizeof check[0];i++)
        if (memcmp((const void*)(base+check[i].address),check[i].words,sizeof check[i].words)) {
            LOGF("CINEMA UI disabled: native mismatch +%lx",check[i].address);return;
        }
    if (*(const unsigned*)(base+0x9f77968+0x24)!=0x498
        || *(const unsigned*)(base+0x9f77e20+0x24)!=0x429) return;
    if (*(const unsigned*)(base+0x9f701c0+0x24)!=0x90
        || *(const unsigned*)(base+0x9f70190+0x24)!=0xac
        || *(const unsigned*)(base+0x9f7bd18+0x24)!=8
        || *(const unsigned*)(base+0x9f7bd98+0x20)!=28) return;
    if (*(const unsigned*)(base+0x9f6b718+0x24)!=0x1a8
        || *(const unsigned*)(base+0x9f64bd0+0x24)!=0x188
        || *(const unsigned*)(base+0x9f64c30+0x24)!=0x150
        || *(const unsigned*)(base+0x9f64b70+0x24)!=0x260
        || *(const unsigned*)(base+0x9f64b40+0x24)!=0x268
        || *(const unsigned*)(base+0x9e51138+0x24)!=0
        || *(const unsigned*)(base+0x9e51068+0x24)!=0x10
        || *(const unsigned*)(base+0x9e51020+0x20)!=0x50
        || *(const unsigned*)(base+0x9e50f98+0x24)!=8
        || *(const unsigned*)(base+0x9e50f68+0x24)!=0x10
        || *(const unsigned*)(base+0x9e51098+0x24)!=8
        || *(const unsigned*)(base+0x9e50f40+0x24)!=0x40
        || *(const unsigned*)(base+0x9e50f18+0x24)!=0x48
        || *(const unsigned*)(base+0x9e503b8+0x20)!=0x28
        || *(const unsigned*)(base+0x9e50348+0x24)!=0x10) return;
    if (*(const unsigned*)(base+0x9f70308+0x24)!=0x28
        || *(const unsigned*)(base+0x9f5a388+0x24)!=0x28
        || *(const unsigned*)(base+0x9f5a358+0x24)!=0x30
        || *(const unsigned*)(base+0x9f7bf38+0x24)!=0x28
        || *(const unsigned*)(base+0x9f5a260+0x24)!=0x50
        || *(const unsigned*)(base+0x9f5fb10+0x24)!=0x58) return;
    cu_weak_assign=(void (*)(struct cinema_ui_weak*,const void*))(base+0x69defcc);
    cu_weak_get=(void* (*)(const struct cinema_ui_weak*))(base+0x69df1cc);
    cu_user_class=(void* (*)(void))(base+0x7e37bec);
    cu_component_class=(void* (*)(void))(base+0x7e4bcdc);
    cu_get_widget=(void* (*)(const void*))(base+0x7db1a28);
    cu_set_main_pass=(void (*)(void*,int))(base+0x838db28);
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
    cu_set_auto_wrap=(void (*)(void*,int))(base+0x7d89f04);
    cu_overlay_slot_class=(void* (*)(void))(base+0x7e13474);cu_box_slot_class=(void* (*)(void))(base+0x7e202c4);
    cu_overlay_class=(void* (*)(void))(base+0x7e12a14);cu_box_class=(void* (*)(void))(base+0x7e1f864);
    cu_set_overlay_halign=(void (*)(void*,int))(base+0x7d6e744);
    cu_set_box_halign=(void (*)(void*,int))(base+0x7d8284c);
    __atomic_store_n(&cu_ready,1,__ATOMIC_RELEASE);
    LOGF("CINEMA UI ready: exact authored subtitle widget, native viewport and weak cleanup");
}
#endif
