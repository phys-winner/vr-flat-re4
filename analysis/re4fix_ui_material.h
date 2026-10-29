// Native widget material swaps retain each component's own render texture and restore on screen exit.
#ifndef RE4FIX_UI_MATERIAL_H
#define RE4FIX_UI_MATERIAL_H
static void* (*uim_widget_class)(void), *(*uim_interface_class)(void), *(*uim_dynamic_class)(void), *(*uim_material_class)(void);
static void* (*uim_get)(const void*,int), *(*uim_base)(void*);
static void* (*uim_texture)(void*,unsigned long long);
static void (*uim_name)(void*,const char*,int);
static unsigned long long uim_slate_ui;
static void (*uim_set)(void*,int,void*);
static void (*uim_sort)(void*,int);
static void* (*uim_find)(void*,void*,const unsigned short*,int);
static void* (*uim_load)(void*,void*,const unsigned short*,const unsigned short*,unsigned,void*,int,void*);
static int uim_ready;
static unsigned uim_logs,uim_apply_logs,uim_rejected;
static long uim_load_ms;
static struct cinema_ui_weak uim_front_material={-1,0};
static struct {
    struct cinema_ui_weak widget,original,applied;
    int used,seen,had_original;
    int changed_material,original_priority,applied_priority,priority_owned;
} uim_records[128];

static int uim_reject(unsigned reason,const char* why,void* widget,void* value) {
    if (reason<32 && !(uim_rejected&(1u<<reason))) {
        uim_rejected|=1u<<reason;LOGF("UIMATERIAL reject=%s widget=%p value=%p",why,widget,value);
    }
    return 0;
}
static int uim_typed(const void* object,void* cls,int minimum) {
    return object && pv_alive((void*)object) && cinema_object(object,cls,minimum);
}
static int uim_world_widget(void* widget) {
    return uim_ready && cinema_on_game_thread() && cu_weak_assign && cu_weak_get
        && uim_typed(widget,uim_widget_class(),0x508)
        && *(const unsigned char*)((const char*)widget+0x429)==0;
}
static void* uim_shader(void* material) {
    if (!uim_typed(material,uim_interface_class(),0x90)) return 0;
    void* shader=uim_base(material);
    return uim_typed(shader,uim_material_class(),0x230) ? shader : 0;
}
static void* uim_parent_material(void* widget) {
    void* material=uim_get(widget,0);
    // A row's runtime MID contains its render texture. Keep only the authored material/MIC parent.
    for (unsigned depth=0;depth<8;depth++) {
        if (!uim_typed(material,uim_interface_class(),0x90)) return 0;
        if (!cinema_object(material,uim_dynamic_class(),0xd8)) return material;
        void* parent=*(void* const*)((const char*)material+0xd0);
        if (parent==material) return 0;
        material=parent;
    }
    return 0;
}
static int uim_mid_owned(void* widget,void* material) {
    void* mid=*(void* const*)((const char*)widget+0x500);
    void* texture=*(void* const*)((const char*)widget+0x4f8);
    if (!uim_slate_ui) uim_name(&uim_slate_ui,"SlateUI",1);
    return uim_typed(mid,uim_dynamic_class(),0xd8)
        && *(void* const*)((const char*)mid+0x20)==widget
        && *(void* const*)((const char*)mid+0xd0)==material
        && texture && uim_texture(mid,uim_slate_ui)==texture;
}
static int uim_override(void* widget,void** material) {
    struct {void** data;int count,max;} list;
    memcpy(&list,(const char*)widget+0x408,sizeof list);
    if (list.count<0 || list.count>32 || list.max<list.count || list.max>4096 || (list.count&&!list.data)) return 0;
    *material=list.count ? list.data[0] : 0;
    return !*material || uim_typed(*material,uim_interface_class(),0x90);
}
static void uim_restore(unsigned index) {
    void* widget=cu_weak_get(&uim_records[index].widget);
    void* applied=cu_weak_get(&uim_records[index].applied);
    void* original=cu_weak_get(&uim_records[index].original);
    if (uim_world_widget(widget) && applied && uim_get(widget,0)==applied) {
        int priority=*(const int*)((const char*)widget+0x238);
        if (uim_records[index].priority_owned>0 && priority==uim_records[index].applied_priority
            && priority!=uim_records[index].original_priority) uim_sort(widget,uim_records[index].original_priority);
        if (uim_records[index].changed_material
            && (!uim_records[index].had_original || uim_typed(original,uim_interface_class(),0x90)))
            uim_set(widget,0,original);
    }
    memset(&uim_records[index],0,sizeof uim_records[index]);
}
static void ui_material_begin(void) {
    if (!uim_ready || !cinema_on_game_thread()) return;
    for (unsigned i=0;i<128;i++) uim_records[i].seen=0;
}
static void ui_material_end(void) {
    if (!uim_ready || !cinema_on_game_thread() || !cu_weak_get || !cu_weak_assign) return;
    for (unsigned i=0;i<128;i++) if (uim_records[i].used && !uim_records[i].seen) uim_restore(i);
}
static int uim_bind(void* widget,void* material) {
    if (!uim_world_widget(widget)) return uim_reject(0,"target-world-widget",widget,material);
    if (!*(void* const*)((const char*)widget+0x4f8)) return uim_reject(1,"target-render-texture",widget,material);
    if (!uim_typed(material,uim_interface_class(),0x90)) return uim_reject(2,"material-interface",widget,material);
    int slot=-1;
    for (unsigned i=0;i<128;i++) if (uim_records[i].used && cu_weak_get(&uim_records[i].widget)==widget) {slot=(int)i;break;}
    if (slot>=0) {
        void* applied=cu_weak_get(&uim_records[slot].applied);
        if (applied==material && uim_get(widget,0)==material && uim_mid_owned(widget,material)) {
            uim_records[slot].seen=1;return 1;
        }
        uim_restore((unsigned)slot);
    }
    int changed=uim_get(widget,0)!=material || !uim_mid_owned(widget,material);
    void* original=0;
    if (!uim_override(widget,&original)) return uim_reject(3,"override-array",widget,material);
    if (slot<0) for (unsigned i=0;i<128;i++) if (!uim_records[i].used) {slot=(int)i;break;}
    if (slot<0) return uim_reject(4,"record-capacity",widget,material);
    cu_weak_assign(&uim_records[slot].widget,widget);
    cu_weak_assign(&uim_records[slot].original,original);
    cu_weak_assign(&uim_records[slot].applied,material);
    uim_records[slot].had_original=original!=0;uim_records[slot].used=uim_records[slot].seen=1;
    uim_records[slot].changed_material=changed;
    uim_records[slot].original_priority=*(const int*)((const char*)widget+0x238);
    // SetMaterial creates a fresh component-owned MID, then binds SlateUI, tint and opacity natively.
    if (changed) uim_set(widget,0,material);
    if (uim_get(widget,0)!=material || !uim_mid_owned(widget,material)) {
        void* mid=*(void* const*)((const char*)widget+0x500);
        if (uim_get(widget,0)!=material) uim_reject(5,"material-readback",widget,material);
        else if (!uim_typed(mid,uim_dynamic_class(),0xd8)) uim_reject(6,"dynamic-instance-type",widget,mid);
        else if (*(void* const*)((const char*)mid+0x20)!=widget) uim_reject(7,"dynamic-instance-outer",widget,*(void* const*)((const char*)mid+0x20));
        else if (*(void* const*)((const char*)mid+0xd0)!=material) uim_reject(8,"dynamic-instance-parent",widget,*(void* const*)((const char*)mid+0xd0));
        else uim_reject(9,"SlateUI-texture-readback",widget,uim_texture(mid,uim_slate_ui));
        uim_restore((unsigned)slot);return 0;
    }
    if (uim_apply_logs<8) {
        uim_apply_logs++;LOGF("UIMATERIAL bound widget=%p material=%p mid=%p texture=%p",widget,material,
            *(void* const*)((const char*)widget+0x500),*(void* const*)((const char*)widget+0x4f8));
    }
    return 1;
}
static int ui_material_clip(void* widget,void* row_widget) {
    if (!uim_world_widget(widget)) return uim_reject(0,"target-world-widget",widget,row_widget);
    if (!uim_world_widget(row_widget)) return uim_reject(10,"source-world-widget",widget,row_widget);
    void* material=uim_parent_material(row_widget), *shader=uim_shader(material);
    char name[80]="";if (shader) obj_name(shader,name,sizeof name);
    if (strcmp(name,"Widget3DPassThrough_PlaneClip4_Mat")) {
        if (!(uim_rejected&(1u<<11))) LOGF("UIMATERIAL clip source=%p material=%p shader=%p name='%s'",row_widget,material,shader,name);
        return uim_reject(11,"source-clip-shader",widget,row_widget);
    }
    return uim_bind(widget,material);
}
static int ui_material_front(void* widget,int priority) {
    if (!uim_world_widget(widget)) return uim_reject(0,"target-world-widget",widget,0);
    if (priority<1 || priority>128) return uim_reject(13,"front-sort-priority",widget,0);
    void* material=cu_weak_get(&uim_front_material), *shader=uim_shader(material);
    if (!shader || !(*(const unsigned*)((const char*)shader+0x22c)&1)) {
        long now=ptr_now_ms();
        if (uim_load_ms && now>=uim_load_ms && now-uim_load_ms<1000) return 0;
        uim_load_ms=now;
        static const char* const paths[]={
            "/Game/Common/Materials/CMN_ArmWidget3DPassThrough_NoDepth_MAT.CMN_ArmWidget3DPassThrough_NoDepth_MAT",
            "/Game/Blueprints/Debug/DebugMenu/Widget3DNoDepth.Widget3DNoDepth",
            "/Game/Blueprints/Debug/DebugMenu/Widget3DNoDepth_NotOccluded.Widget3DNoDepth_NotOccluded"
        };
        material=0;
        for (unsigned i=0;i<sizeof paths/sizeof *paths;i++) {
            unsigned short path[160];unsigned n=0;
            for (;paths[i][n] && n<159;n++) path[n]=(unsigned char)paths[i][n];path[n]=0;
            void* candidate=uim_find(uim_interface_class(),0,path,0);
            if (!candidate) candidate=uim_load(uim_interface_class(),0,path,0,0,0,1,0);
            void* candidate_shader=uim_shader(candidate);
            if (!candidate_shader || !(*(const unsigned*)((const char*)candidate_shader+0x22c)&1)) continue;
            material=candidate;cu_weak_assign(&uim_front_material,material);break;
        }
        if (!material) return uim_reject(12,"no-depth-material-unavailable",widget,0);
        if (uim_logs<4) {uim_logs++;LOGF("UIMATERIAL existing no-depth widget material=%p",material);}
    }
    if (!uim_bind(widget,material)) return 0;
    for (unsigned i=0;i<128;i++) if (uim_records[i].used && cu_weak_get(&uim_records[i].widget)==widget) {
        int current=*(const int*)((const char*)widget+0x238);
        if (uim_records[i].priority_owned<0) return 1;
        if (uim_records[i].priority_owned && current!=uim_records[i].applied_priority) {
            uim_records[i].priority_owned=-1;return 1; // Keep a foreign writer's layer until this ownership ends.
        }
        if (current!=priority) uim_sort(widget,priority);
        if (*(const int*)((const char*)widget+0x238)!=priority) {uim_records[i].priority_owned=-1;return 0;}
        uim_records[i].applied_priority=priority;uim_records[i].priority_owned=1;return 1;
    }
    return 0;
}
static void install_ui_material(unsigned long base) {
    static const struct {unsigned long addr;unsigned words[2];} checks[]={
        {0x7dacffc, {0xf81e0ff3, 0xa9017bfd}}, // UWidgetComponent::SetMaterial
        {0x7db2130, {0x37f80121, 0xb9441008}}, // UWidgetComponent::GetMaterial
        {0x847a868, {0xf9400008, 0xf9414901}}, // UMaterialInterface::GetBaseMaterial
        {0x8478af0, {0xd10143ff, 0xa9034ff4}}, // UMaterialInstanceDynamic::K2_GetTextureParameterValue
        {0x67c7644, {0xf81d0ff5, 0xa9014ff4}}, // FName::FName(const char*,EFindName)
        {0x69bcbec, {0xd10243ff, 0xa9036ffc}}, // StaticFindObject
        {0x69bf1a4, {0xd105c3ff, 0xa9116ffc}}, // StaticLoadObject
        {0x7e4bcdc, {0xd10183ff, 0xf90023f3}}, // UWidgetComponent::GetPrivateStaticClass
        {0x8eaebb0, {0xd10183ff, 0xf90023f3}}, // UMaterialInterface::GetPrivateStaticClass
        {0x8eada14, {0xd10183ff, 0xf90023f3}}, // UMaterialInstanceDynamic::GetPrivateStaticClass
        {0x8e481bc, {0xd10183ff, 0xf90023f3}}, // UMaterial::GetPrivateStaticClass
        {0x8e480c0, {0xb9422c08, 0x32000108}}, // UMaterial.bDisableDepthTest: bit0 at0x22c
        {0x8385604, {0xb9423808, 0x6b01011f}}, // UPrimitiveComponent::SetTranslucentSortPriority
    };
    uim_ready=0;
    for (unsigned i=0;i<sizeof checks/sizeof *checks;i++)
        if (memcmp((const void*)(base+checks[i].addr),checks[i].words,8)) {LOGF("UIMATERIAL disabled: native mismatch +%lx",checks[i].addr);return;}
    static const struct {unsigned long address;unsigned offset;} fields[]={
        {0xa1cdde0,0xd0}, {0x9f777b8,0x500}, {0x9f777e8,0x4f8}, {0xa1d4da8,0x408}, {0x9f77e20,0x429}, {0xa21c708,0x238}
    };
    for (unsigned i=0;i<sizeof fields/sizeof *fields;i++)
        if (*(const unsigned*)(base+fields[i].address+36)!=fields[i].offset) {LOGF("UIMATERIAL disabled: field mismatch +%lx",fields[i].address);return;}
    uim_set=(void (*)(void*,int,void*))(base+0x7dacffc);uim_get=(void* (*)(const void*,int))(base+0x7db2130);
    uim_sort=(void (*)(void*,int))(base+0x8385604);
    uim_base=(void* (*)(void*))(base+0x847a868);
    uim_texture=(void* (*)(void*,unsigned long long))(base+0x8478af0);uim_name=(void (*)(void*,const char*,int))(base+0x67c7644);
    uim_find=(void* (*)(void*,void*,const unsigned short*,int))(base+0x69bcbec);
    uim_load=(void* (*)(void*,void*,const unsigned short*,const unsigned short*,unsigned,void*,int,void*))(base+0x69bf1a4);
    uim_widget_class=(void* (*)(void))(base+0x7e4bcdc);uim_interface_class=(void* (*)(void))(base+0x8eaebb0);
    uim_dynamic_class=(void* (*)(void))(base+0x8eada14);uim_material_class=(void* (*)(void))(base+0x8e481bc);
    uim_ready=1;
    LOGF("UIMATERIAL ready: authored clip shader and existing no-depth widget materials");
}
#endif
