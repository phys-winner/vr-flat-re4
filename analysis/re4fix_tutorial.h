// First-run weapon-management tutorial, Quick Select part. Its native instructions wait for the VR selection
// wheel itself: TriggerHold* (hold the off-hand trigger: the wheel opens), ThumbstickSelect* (push the stick toward
// the handgun until it is selected) and TapTrigger* (tap the trigger: toggle knife <-> handgun). While one of those
// instructions is current, a tap on an equipment icon performs exactly that controller gesture instead of the
// direct Quick Select equip. Include after re4fix_input.h.
#ifndef RE4FIX_TUTORIAL_H
#define RE4FIX_TUTORIAL_H

enum { TUT_NONE, TUT_HOLD, TUT_WHEEL, TUT_TAP };
static void* (*tut_manager)(void);   // AVR4DynamicTutorialManager::GetInstance (static, pending-kill checked)
static volatile long g_tut_gesture;  // (ms << 8) | (stick right << 5) | (trigger right << 4) | kind
static long tut_probe_ms;
static char tut_logged[48];
static void* (*tut_manager_class)(void),*(*tut_label_class)(void);
static struct cinema_ui_weak tut_reload_instruction={-1,0};
static volatile long tut_reload_ui_ms;
static unsigned tut_reload_logs,tut_reload_rejections;
static const void* (*tut_geometry)(const void*);
static struct cinema_ui_vec2 (*tut_local_size)(const void*),(*tut_desired_size)(const void*);
static struct cinema_ui_vec2 (*tut_local_absolute)(const void*,struct cinema_ui_vec2),(*tut_absolute_local)(const void*,struct cinema_ui_vec2);
static struct {
    struct cinema_ui_weak actor,component,line,widget;
    struct cinema_ui_vec2 scale,pivot,position,size,alignment;
    struct cinema_ui_anchors anchors;
    unsigned char visible[2];
    unsigned char line_hidden;
    int used,added;
} tut_reload_ui;

static int touch_tutorial_reload_hint(long now) {
    long stamp=__atomic_load_n(&tut_reload_ui_ms,__ATOMIC_ACQUIRE);
    return stamp>0 && now>=stamp && now-stamp<=400;
}
static int tut_ui_owned(void* part,void* actor) {
    return pv_alive(part) && pv_alive(actor) && cinema_object(part,mv_scene_class(),0x1f0)
        && *(void* const*)((const char*)part+0x20)==actor
        && *(void* const*)((const char*)part+0xa0)==actor;
}
static int tut_caption_bounds(void* widget,float bounds[4]) {
    if(!tut_geometry || !tut_desired_size || !tut_local_size || !tut_local_absolute || !tut_absolute_local)return 0;
    void* tree=*(void* const*)((const char*)widget+0x1a8);
    if(!pv_alive(tree) || !cinema_object(tree,cu_tree_class(),0x30)
        || *(void* const*)((const char*)tree+0x20)!=widget)return 0;
    const void* root=tut_geometry(widget);if(!root)return 0;
    float lo[2]={INFINITY,INFINITY},hi[2]={-INFINITY,-INFINITY};unsigned count=0;
    for(unsigned i=0;i<2;i++) {
        void* text=cinema_ui_property(widget,"ButtonLabelWidget_C",i ? "ButtonLabelAlt" : "ButtonLabel",i ? 0x248 : 0x240);
        if(!pv_alive(text) || !cinema_object(text,cu_text_class(),0x2a0)
            || *(void* const*)((const char*)text+0x20)!=tree || cu_find_widget(widget,(const char*)text+0x18)!=text)return 0;
        unsigned visibility=*(const unsigned char*)((const char*)text+0xc3);
        if(visibility==1 || visibility==2 || cu_render_opacity(text)<=0)continue;
        struct cinema_ui_vec2 desired=tut_desired_size(text);
        if(desired.x<=0 || desired.y<=0)continue;
        const void* geometry=tut_geometry(text);if(!geometry)return 0;
        struct cinema_ui_vec2 size=tut_local_size(geometry);
        if(!isfinite(size.x) || !isfinite(size.y) || size.x<=0 || size.y<=0 || size.x>16384 || size.y>16384)return 0;
        for(unsigned corner=0;corner<4;corner++) {
            struct cinema_ui_vec2 point=tut_local_absolute(geometry,(struct cinema_ui_vec2){(corner&1)?size.x:0,(corner&2)?size.y:0});
            point=tut_absolute_local(root,point);
            if(!isfinite(point.x) || !isfinite(point.y) || fabsf(point.x)>16384 || fabsf(point.y)>16384)return 0;
            lo[0]=fminf(lo[0],point.x);lo[1]=fminf(lo[1],point.y);hi[0]=fmaxf(hi[0],point.x);hi[1]=fmaxf(hi[1],point.y);
        }
        count++;
    }
    bounds[0]=lo[0];bounds[1]=lo[1];bounds[2]=hi[0]-lo[0];bounds[3]=hi[1]-lo[1];
    return count && bounds[2]>=1 && bounds[3]>=1 && bounds[2]<=16384 && bounds[3]<=16384;
}
static void tut_reload_ui_cleanup(void) {
    __atomic_store_n(&tut_reload_ui_ms,0,__ATOMIC_RELEASE);
    if (!tut_reload_ui.used) return;
    void* actor=cu_weak_get(&tut_reload_ui.actor),*component=cu_weak_get(&tut_reload_ui.component);
    void* widget=cu_weak_get(&tut_reload_ui.widget);
    if (pv_alive(widget) && cinema_object(widget,cu_user_class(),0x230)) {
        if (tut_reload_ui.added && cu_in_viewport(widget)) cu_remove(widget);
        cu_set_scale(widget,tut_reload_ui.scale);cu_set_pivot(widget,tut_reload_ui.pivot);
        cu_set_position(widget,tut_reload_ui.position,0);cu_set_size(widget,tut_reload_ui.size);
        cu_set_alignment(widget,tut_reload_ui.alignment);cu_set_anchors(widget,tut_reload_ui.anchors);
    }
    void* parts[2]={component,cu_weak_get(&tut_reload_ui.line)};
    for (unsigned i=0;i<2;i++) if (tut_ui_owned(parts[i],actor)
        && *(void* const*)((const char*)actor+(i ? 0x230 : 0x228))==parts[i]) {
        mv_visible(parts[i],tut_reload_ui.visible[i],0);
        if(i)mv_hidden(parts[i],tut_reload_ui.line_hidden,0);
    }
    memset(&tut_reload_ui,0,sizeof tut_reload_ui);
}
// Reuse the authored caption in the viewport; the obsolete belt line becomes an overlay arrow to Reload.
static void touch_tutorial_ui_tick(void) {
    if (!cinema_on_game_thread() || !cu_ready || !tut_label_class || !tut_manager_class || !mv_visible || !mv_hidden
        || !mv_scene_class || !mv_static_class) return;
    void* manager=tut_manager ? tut_manager() : 0;
    void* current=cu_weak_get(&tut_reload_instruction);
    if (!current || g_touch_context!=1 || menu_mode() || g_touch_weapon!=2 || g_touch_cancel
        || !pv_alive(manager) || !cinema_object(manager,tut_manager_class(),0x2c0)
        || *(void* const*)((const char*)manager+0x298)!=current) { tut_reload_ui_cleanup();return; }
    struct {void** data;int count,max;} labels;
    memcpy(&labels,(const char*)manager+0x2b0,sizeof labels);
    // shortcut: GrabAmmo currently supplies one label; handle each label if an authored variant adds more.
    if (labels.count!=1 || labels.max<labels.count || labels.max>64 || !labels.data) {
        if(tut_reload_rejections<4){tut_reload_rejections++;LOGF("TUTORIAL reload label deferred count=%d max=%d",labels.count,labels.max);}
        tut_reload_ui_cleanup();return;
    }
    void* actor=labels.data[0];
    if (!pv_alive(actor) || !cinema_object(actor,tut_label_class(),0x2b8)
        || !*(const unsigned char*)((const char*)actor+0x2b4)) {tut_reload_ui_cleanup();return;}
    void* component=*(void* const*)((const char*)actor+0x228),*line=*(void* const*)((const char*)actor+0x230);
    if (!tut_ui_owned(component,actor) || !cinema_object(component,cu_component_class(),0x4a0)
        || !tut_ui_owned(line,actor) || !cinema_object(line,mv_static_class(),0x438)) {tut_reload_ui_cleanup();return;}
    void* widget=cu_get_widget(component);
    if (!pv_alive(widget) || !cinema_object(widget,cu_user_class(),0x230)) {tut_reload_ui_cleanup();return;}
    if (tut_reload_ui.used && (cu_weak_get(&tut_reload_ui.actor)!=actor
        || cu_weak_get(&tut_reload_ui.component)!=component || cu_weak_get(&tut_reload_ui.widget)!=widget
        || cu_weak_get(&tut_reload_ui.line)!=line)) tut_reload_ui_cleanup();
    struct cinema_ui_vec2 draw=cu_draw_size(component);
    float dpi=cu_viewport_scale(widget);
    if (!isfinite(dpi) || dpi<.25f || dpi>4 || !isfinite(draw.x) || !isfinite(draw.y)
        || draw.x<1 || draw.y<1 || draw.x>16384 || draw.y>16384
        || g_tw<64 || g_th<64 || g_tw>16384 || g_th>16384) {tut_reload_ui_cleanup();return;}
    float bounds[4];
    if(!tut_caption_bounds(widget,bounds)) {tut_reload_ui_cleanup();return;}
    float scale=fminf(.035f*g_th/(bounds[3]*dpi),.28f*g_tw/(bounds[2]*dpi));
    if (!isfinite(scale) || scale<.01f || scale>5) {tut_reload_ui_cleanup();return;}
    if (!tut_reload_ui.used) {
        if (cu_in_viewport(widget) || !(*(const unsigned short*)((const char*)component+0x14c)&0x20)) return;
        cu_weak_assign(&tut_reload_ui.actor,actor);cu_weak_assign(&tut_reload_ui.component,component);
        cu_weak_assign(&tut_reload_ui.line,line);cu_weak_assign(&tut_reload_ui.widget,widget);
        memcpy(&tut_reload_ui.scale,(const char*)widget+0x98,8);memcpy(&tut_reload_ui.pivot,(const char*)widget+0xac,8);
        memcpy(&tut_reload_ui.anchors,(const char*)widget+0x1e0,16);memcpy(&tut_reload_ui.position,(const char*)widget+0x1f0,8);
        memcpy(&tut_reload_ui.size,(const char*)widget+0x1f8,8);memcpy(&tut_reload_ui.alignment,(const char*)widget+0x200,8);
        tut_reload_ui.visible[0]=!!(*(const unsigned short*)((const char*)component+0x14c)&0x20);
        tut_reload_ui.visible[1]=!!(*(const unsigned short*)((const char*)line+0x14c)&0x20);
        tut_reload_ui.line_hidden=!!(*(const unsigned short*)((const char*)line+0x14c)&0x400);
        tut_reload_ui.used=1;
    }
    struct ctl c=ctl_geometry(C_RELOAD);
    struct ctl fire=ctl_geometry(C_FIRE);
    float right=fminf(g_tw-g_th*.01f,c.cx*g_tw+c.r*g_th),bottom=c.cy*g_th-(c.r+.033f)*g_th;
    if(bottom-bounds[3]*dpi*scale<(fire.cy+fire.r+.008f)*g_th)
        right=fminf(right,fire.cx*g_tw-(fire.r+.012f)*g_th);
    cu_set_scale(widget,(struct cinema_ui_vec2){scale,scale});cu_set_pivot(widget,(struct cinema_ui_vec2){0,0});
    cu_set_size(widget,draw);cu_set_alignment(widget,(struct cinema_ui_vec2){0,0});
    cu_set_anchors(widget,(struct cinema_ui_anchors){0,0,0,0});
    cu_set_position(widget,(struct cinema_ui_vec2){right-(bounds[0]+bounds[2])*dpi*scale,
        bottom-(bounds[1]+bounds[3])*dpi*scale},1);
    if (!tut_reload_ui.added) {cu_add(widget,20);tut_reload_ui.added=cu_in_viewport(widget)!=0;}
    if (!tut_reload_ui.added) {tut_reload_ui_cleanup();return;}
    if(tut_reload_logs<4 && !touch_tutorial_reload_hint(now_ms())) {
        tut_reload_logs++;LOGF("TUTORIAL reload caption=%p component=%p line=%p viewport=1 anchor=%.1f,%.1f scale=%.3f textBounds=%.1f,%.1f,%.1f,%.1f textPixels=%.1fx%.1f",
            widget,component,line,c.cx*g_tw,c.cy*g_th,scale,bounds[0],bounds[1],bounds[2],bounds[3],bounds[2]*dpi*scale,bounds[3]*dpi*scale);
    }
    mv_visible(component,0,0);mv_visible(line,0,0);
    // Authored line animation can re-show bVisible; keep only this obsolete world-space mesh hidden.
    if(!(*(const unsigned short*)((const char*)line+0x14c)&0x400))mv_hidden(line,1,0);
    __atomic_store_n(&tut_reload_ui_ms,now_ms(),__ATOMIC_RELEASE);
}

// GameThread, active gameplay right hand: publish the current wheel instruction.
static void touch_tutorial_tick(long now) {
    if (!tut_manager || now - tut_probe_ms < 100) return;
    tut_probe_ms = now;
    long value = 0;
    void* manager = tut_manager();
    void* current = manager ? *(void* const*)((const char*)manager + 0x298) : 0;   // CurrentTutorial
    char name[48] = "";
    if (current && pv_alive(current)) {
        obj_name(current, name, sizeof name);
        size_t length = strlen(name);
        int kind = !strncmp(name, "TriggerHold", 11) ? TUT_HOLD : !strncmp(name, "ThumbstickSelect", 16) ? TUT_WHEEL
            : !strncmp(name, "TapTrigger", 10) ? TUT_TAP : TUT_NONE;
        int trigger_right = length && name[length - 1] == 'R';
        int stick_right = kind == TUT_WHEEL && length > 16 && name[16] == 'R';
        if (kind) value = (now << 8) | (stick_right << 5) | (trigger_right << 4) | kind;
    }
    if(cu_weak_assign)cu_weak_assign(&tut_reload_instruction,!strncmp(name,"GrabAmmo",8) ? current : 0);
    __atomic_store_n(&g_tut_gesture, value, __ATOMIC_RELEASE);
    if (strcmp(name, tut_logged)) {
        memcpy(tut_logged, name, sizeof tut_logged);
        LOGF("TUTORIAL instruction '%s' wheel_gesture=%ld", name, value & 15);
    }
}

// Input thread (g_imx held): an equipment icon during a wheel instruction drives the native trigger/stick.
static int touch_tutorial_gesture(int z, long now) {
    long value = __atomic_load_n(&g_tut_gesture, __ATOMIC_ACQUIRE);
    if (!value || g_touch_context != 1 || now - (value >> 8) > 500 || now < (value >> 8)) return 0;
    int kind = (int)(value & 15), trigger = (value >> 4) & 1 ? 26 : 28;   // OB_RINDEX / OB_LINDEX bit
    if (kind == TUT_HOLD) {
        g_cmd_until[trigger] = now + 2000;   // the instruction needs the wheel held open for more than a second
    } else if (kind == TUT_WHEEL) {
        if (z != C_WEAPON) return 0;   // the instruction asks for the handgun (to the right of the wheel)
        g_cmd_until[trigger] = now + 1900;
        if ((value >> 5) & 1) { g_cmd_rstick[0] = 1; g_cmd_rstick[1] = 0; g_cmd_rstick_until = now + 1500; }
        else { g_cmd_lstick[0] = 1; g_cmd_lstick[1] = 0; g_cmd_lstick_until = now + 1500; }
    } else if (kind == TUT_TAP) {
        if (z != C_KNIFE && z != C_WEAPON) return 0;
        g_cmd_until[trigger] = now + 150;
    } else return 0;
    LOGF("TUTORIAL wheel gesture %d for icon %d", kind, z);
    return 1;
}

static void install_tutorial(unsigned long base) {
    static const unsigned instance[4] = {0xf0020868, 0xf946b108, 0xf9400108, 0xb40002e8};   // GetInstance
    static const unsigned current[2] = {0xf9414c08, 0xb4000248};   // execGetCurrentTutorial: ldr x8,[x0,#0x298]
    if (memcmp((const void*)(base + 0x625686c), instance, sizeof instance)
        || memcmp((const void*)(base + 0x63f0f44), current, sizeof current)) {
        LOGF("TUTORIAL wheel adapter disabled: opcode mismatch"); return;
    }
    tut_manager = (void* (*)(void))(base + 0x625686c);
    static const struct {unsigned long addr;unsigned first[2];} ui_code[]={
        {0x63f2630,{0xd10183ff,0xf90023f3}}, {0x64d59f4,{0xd10183ff,0xf90023f3}},
        {0x6258c38,{0xb942b808,0x7100051f}}, {0x636e924,{0xf9015401,0xd65f03c0}},
        {0x64d5270,{0x390ad288,0xf94016a8}},
        {0x7da7c7c,{0x14000001,0xf81d0ff5}}, {0x7da6f68,{0x6dbc23e9,0xf9000bf5}},
        {0x7dce618,{0x2d400400,0xd65f03c0}}, {0x7dce5d8,{0xfc41c002,0xfc424003}},
        {0x7dce56c,{0x2d448c02,0x2d439404}},
    };
    static const struct {unsigned long params;unsigned offset;} ui_fields[]={
        {0x9d87c60,0x2b0},{0x9d87d20,0x298},{0x9e0e920,0x228},{0x9e0e8f0,0x230},
    };
    int ui_valid=1;
    for(unsigned i=0;i<sizeof ui_code/sizeof *ui_code;i++)
        if(memcmp((const void*)(base+ui_code[i].addr),ui_code[i].first,8))ui_valid=0;
    for(unsigned i=0;i<sizeof ui_fields/sizeof *ui_fields;i++)
        if(*(const unsigned*)(base+ui_fields[i].params+36)!=ui_fields[i].offset)ui_valid=0;
    if(ui_valid) {
        tut_manager_class=(void* (*)(void))(base+0x63f2630);tut_label_class=(void* (*)(void))(base+0x64d59f4);
        tut_geometry=(const void* (*)(const void*))(base+0x7da7c7c);
        tut_desired_size=(struct cinema_ui_vec2 (*)(const void*))(base+0x7da6f68);
        tut_local_size=(struct cinema_ui_vec2 (*)(const void*))(base+0x7dce618);
        tut_local_absolute=(struct cinema_ui_vec2 (*)(const void*,struct cinema_ui_vec2))(base+0x7dce5d8);
        tut_absolute_local=(struct cinema_ui_vec2 (*)(const void*,struct cinema_ui_vec2))(base+0x7dce56c);
    }
    LOGF("TUTORIAL wheel adapter ready: Quick Select instructions take icon taps");
}
#endif
