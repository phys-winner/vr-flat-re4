// Use the existing flat health dial and the authored localized status; retain all native health/QTE logic.
#ifndef RE4FIX_UI_HEALTH_H
#define RE4FIX_UI_HEALTH_H
static long fv_health_presented_ms;
static int fv_health_ready;
static const char* const fv_health_fields[4]={"MessageTextWidget","HeartbeatBG","HeartbeatWaveform","VignetteQte"};
static struct {
    struct cinema_ui_weak pawn,actor,component[4],widget;
    struct cinema_ui_vec2 scale,pivot,position,size,alignment;
    struct cinema_ui_anchors anchors;
    unsigned char visibility,original_hidden,hidden;
    int used,added,layout,width,height;
    float dpi;
} fv_health;
static unsigned fv_health_logs;
static const char* const fv_health_property_names[7]={"HealthHud","Instigator","MessageTextWidget","MessageWidget","HeartbeatBG","HeartbeatWaveform","VignetteQte"};
static struct {struct cinema_ui_weak cls;int offset,used;} fv_health_properties[7];
// Render-thread acknowledgement under the input mutex: atomics only, never call an engine API or lock.
static void touch_health_flat_presented(long now) {
    if(now>0)__atomic_store_n(&fv_health_presented_ms,now,__ATOMIC_RELEASE);
}
static int fv_health_flat_available(long now) {
    long presented=__atomic_load_n(&fv_health_presented_ms,__ATOMIC_ACQUIRE);
    return presented>0 && now>=presented && now-presented<=500;
}
static void* fv_health_object(const void* object,const char* name,int minimum) {
    if(!pv_alive(object))return 0;
    const char* cls=*(const char* const*)((const char*)object+0x10);
    int size=cls ? *(const int*)(cls+0x58):0;
    if(size<minimum || size>0x10000)return 0;
    unsigned slot=0;while(slot<7 && strcmp(name,fv_health_property_names[slot]))slot++;
    if(slot<7 && fv_health_properties[slot].used && cu_weak_get(&fv_health_properties[slot].cls)==cls) {
        int offset=fv_health_properties[slot].offset;
        return offset>=0x28 && offset<=size-8 ? *(void* const*)((const char*)object+offset):0;
    }
    const char* field=cinema_field_property(object,name,"ObjectProperty");
    int offset=field ? *(const int*)(field+0x4c):-1;
    if(!field || offset<0x28 || offset>size-8 || *(const int*)(field+0x34)!=1 || *(const int*)(field+0x38)!=8)return 0;
    if(slot<7) {
        cu_weak_assign(&fv_health_properties[slot].cls,cls);fv_health_properties[slot].offset=offset;fv_health_properties[slot].used=1;
    }
    return *(void* const*)((const char*)object+offset);
}
static int fv_health_actor(void* actor,void* pawn) {
    return pv_alive(pawn) && pv_alive(actor) && cinema_ui_named(actor,"UI_HealthVignette_BP_C",0x220)
        && fv_health_object(actor,"Instigator",0x220)==pawn;
}
static int fv_health_part(void* actor,void* component,unsigned index) {
    if(index>=4 || !pv_alive(actor) || !pv_alive(component)
        || !menu_visual_layout(component,index ? mv_static_class():cu_component_class(),index ? 0x448:0x519)
        || *(void* const*)((const char*)component+0x20)!=actor
        || *(void* const*)((const char*)component+0xa0)!=actor)return 0;
    void* root=*(void* const*)((const char*)actor+0x130);
    return pv_alive(root) && menu_visual_layout(root,mv_scene_class(),0x1f0)
        && *(void* const*)((const char*)root+0x20)==actor && *(void* const*)((const char*)root+0xa0)==actor
        && fv_descends_from(component,root);
}
static int fv_health_widget(void* widget) {
    return pv_alive(widget) && cinema_object(widget,cu_user_class(),0x230)
        && cinema_ui_named(widget,"UI_HealthVignette_Widget_C",0x230);
}
static void fv_health_text_restore(void) {
    if(!fv_health.used || !cu_weak_get)return;
    void* widget=cu_weak_get(&fv_health.widget);
    if(fv_health_widget(widget)) {
        if(fv_health.added && cu_in_viewport(widget))cu_remove(widget);
        if(fv_health.layout) {
            cu_set_scale(widget,fv_health.scale);cu_set_pivot(widget,fv_health.pivot);
            cu_set_position(widget,fv_health.position,0);cu_set_size(widget,fv_health.size);
            cu_set_alignment(widget,fv_health.alignment);cu_set_anchors(widget,fv_health.anchors);
            if(*(const unsigned char*)((const char*)widget+0xc3)==3)wt_set_visibility(widget,fv_health.visibility);
        }
    }
    void* actor=cu_weak_get(&fv_health.actor),*pawn=cu_weak_get(&fv_health.pawn),*component=cu_weak_get(&fv_health.component[0]);
    if((fv_health.hidden&1) && !(fv_health.original_hidden&1) && fv_health_actor(actor,pawn)
        && fv_health_object(actor,fv_health_fields[0],0x220)==component && fv_health_part(actor,component,0)
        && (*(const unsigned short*)((const char*)component+0x14c)&0x400))mv_hidden(component,0,0);
    fv_health.added=fv_health.layout=fv_health.width=fv_health.height=0;fv_health.hidden&=~1u;
}
static void fv_health_restore(void) {
    if(!fv_health.used || !cu_weak_get)return;
    fv_health_text_restore();
    void* actor=cu_weak_get(&fv_health.actor),*pawn=cu_weak_get(&fv_health.pawn);
    if(fv_health_actor(actor,pawn))for(unsigned i=1;i<4;i++) {
        void* component=cu_weak_get(&fv_health.component[i]);
        if((fv_health.hidden&(1u<<i)) && !(fv_health.original_hidden&(1u<<i))
            && fv_health_object(actor,fv_health_fields[i],0x220)==component && fv_health_part(actor,component,i)
            && (*(const unsigned short*)((const char*)component+0x14c)&0x400))mv_hidden(component,0,0);
    }
    memset(&fv_health,0,sizeof fv_health);
}
static int fv_health_layout(void* widget,void* component) {
    int width=g_comp_w,height=g_comp_h;float dpi=cu_viewport_scale(widget);
    struct cinema_ui_vec2 draw=fv_current_draw_size ? fv_current_draw_size(component):cu_draw_size(component);
    if(width<height || height<64 || width>16384 || height>16384 || (float)width/height>4
        || !isfinite(dpi) || dpi<.25f || dpi>4 || !isfinite(draw.x) || !isfinite(draw.y)
        || draw.x<1 || draw.x>16384 || draw.y<1 || draw.y>16384)return 0;
    float fit=fminf(width*.09f/(dpi*draw.x),height*.05f/(dpi*draw.y));
    if(!isfinite(fit) || fit<.001f || fit>32)return 0;
    if(!fv_health.layout) {
        memcpy(&fv_health.scale,(const char*)widget+0x98,8);memcpy(&fv_health.pivot,(const char*)widget+0xac,8);
        memcpy(&fv_health.anchors,(const char*)widget+0x1e0,16);memcpy(&fv_health.position,(const char*)widget+0x1f0,8);
        memcpy(&fv_health.size,(const char*)widget+0x1f8,8);memcpy(&fv_health.alignment,(const char*)widget+0x200,8);
        fv_health.visibility=*(const unsigned char*)((const char*)widget+0xc3);
        if(fv_health.visibility>4)return 0;
        fv_health.layout=1;
    }
    // Keep the native localized status immediately below the health dial under Pause.
    cu_set_scale(widget,(struct cinema_ui_vec2){fit,fit});cu_set_pivot(widget,(struct cinema_ui_vec2){.5f,0});
    cu_set_position(widget,(struct cinema_ui_vec2){width*.055f,height*.32f},1);cu_set_size(widget,draw);
    cu_set_alignment(widget,(struct cinema_ui_vec2){.5f,0});cu_set_anchors(widget,(struct cinema_ui_anchors){0,0,0,0});
    wt_set_visibility(widget,3);fv_health.width=width;fv_health.height=height;fv_health.dpi=dpi;
    return 1;
}
static void touch_health_view(void* camera_object,void* view) {
    (void)view;
    if(!cinema_on_game_thread() || !fv_health_ready || !cu_ready || !phone_inspect_ready || !phone_type || !phone_active
        || !hs_bio4 || !cinema_camera_class || !mv_hidden)return;
    void* pawn=phone_active();
    if(phone_type()!=1 || !pv_alive(pawn) || pawn!=hs_bio4() || !fv_health_flat_available(ptr_now_ms())) {fv_health_restore();return;}
    if(!pv_alive(camera_object) || !cinema_object(camera_object,cinema_camera_class(),0x7b8)
        || *(void* const*)((const char*)pawn+0x2d8)!=camera_object)return;
    void* actor=fv_health_object(pawn,"HealthHud",0x570),*parts[4],*widget=0;
    if(!fv_health_actor(actor,pawn)) {fv_health_restore();return;}
    unsigned visible=0,hidden=0;int changed=fv_health.used && cu_weak_get(&fv_health.actor)!=actor;
    for(unsigned i=0;i<4;i++) {
        parts[i]=fv_health_object(actor,fv_health_fields[i],0x220);
        if(!fv_health_part(actor,parts[i],i)) {fv_health_restore();return;}
        unsigned flags=*(const unsigned short*)((const char*)parts[i]+0x14c);
        if(flags&0x20)visible|=1u<<i;if(flags&0x400)hidden|=1u<<i;
        if(fv_health.used && cu_weak_get(&fv_health.component[i])!=parts[i])changed=1;
    }
    widget=fv_health_object(actor,"MessageWidget",0x220);
    if(!fv_health_widget(widget) || cu_get_widget(parts[0])!=widget) {fv_health_restore();return;}
    if(fv_health.used && cu_weak_get(&fv_health.widget)!=widget)changed=1;
    if(changed) {
        fv_health_restore();visible=hidden=0;
        for(unsigned i=0;i<4;i++) {
            unsigned flags=*(const unsigned short*)((const char*)parts[i]+0x14c);
            if(flags&0x20)visible|=1u<<i;if(flags&0x400)hidden|=1u<<i;
        }
    }
    if(!visible) {fv_health_restore();return;}
    unsigned char visibility=*(const unsigned char*)((const char*)widget+0xc3);
    int show_text=(visible&1) && (!(hidden&1) || (fv_health.hidden&1)) && visibility!=1 && visibility!=2 && visibility<=4;
    if(!fv_health.used) {
        if(cu_in_viewport(widget))return;
        cu_weak_assign(&fv_health.pawn,pawn);cu_weak_assign(&fv_health.actor,actor);cu_weak_assign(&fv_health.widget,widget);
        for(unsigned i=0;i<4;i++)cu_weak_assign(&fv_health.component[i],parts[i]);
        fv_health.original_hidden=(unsigned char)hidden;fv_health.used=1;
    }
    if(show_text) {
        if(fv_health.added && !cu_in_viewport(widget)) {fv_health_restore();return;}
        if(!fv_health.added || fv_health.width!=g_comp_w || fv_health.height!=g_comp_h || fv_health.dpi!=cu_viewport_scale(widget))
            if(!fv_health_layout(widget,parts[0])) {fv_health_restore();return;}
        if(!fv_health.added) {
            cu_add(widget,8);fv_health.added=cu_in_viewport(widget)!=0;
            if(!fv_health.added) {fv_health_restore();return;}
            if(fv_health_logs<6){fv_health_logs++;LOGF("UIHEALTH native status=%p beside confirmed flat HP dial; owned VR render leaves suppressed",widget);}
        }
    } else fv_health_text_restore();
    for(unsigned i=show_text ? 0:1;i<4;i++)if(!(fv_health.original_hidden&(1u<<i))) {
        mv_hidden(parts[i],1,0);fv_health.hidden|=1u<<i;
    }
}
static void install_ui_health(uintptr_t base) {
    if(base && cu_ready && *(const unsigned*)(base+0xa0b75c0+0x24)==0x118) {
        fv_health_ready=1;cinema_health_view=touch_health_view;
    }
}
#endif
