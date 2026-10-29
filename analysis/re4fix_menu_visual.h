// Include after phone getters. Run after the native MenuPlayerHand Tick on the GameThread.
static void* (*mv_hand_class)(void), *(*mv_pawn_class)(void), *(*mv_meshset_class)(void);
static void* (*mv_scene_class)(void), *(*mv_static_class)(void), *(*mv_skeletal_class)(void);
static void* (*mv_get_mesh)(const void*, unsigned char);
static void (*mv_hidden)(void*, int, int), (*mv_visible)(void*, int, int);
static unsigned mv_log_count[2];

static int menu_visual_layout(const void* object, const void* expected, int minimum) {
    const char* cls = object ? *(const char* const*)((const char*)object+0x10) : 0;
    if (!cls || !expected) return 0;
    int size = *(const int*)(cls+0x58), expected_size = *(const int*)((const char*)expected+0x58);
    int depth = *(const int*)((const char*)expected+0x38), actual_depth = *(const int*)(cls+0x38);
    const void* const* chain = *(const void* const**)(cls+0x30);
    return size>=minimum && size<=0x10000 && expected_size>=minimum && expected_size<=size
        && depth>=0 && depth<=128 && actual_depth>=depth && actual_depth<=256
        && chain && chain[depth]==(const char*)expected+0x30;
}
static int menu_visual_component(const void* component, const void* cls, const void* owner) {
    return menu_visual_layout(component,cls,0x150)
        && *(void* const*)((const char*)component+0x20)==owner;
}
static int menu_visual_avatar_component(const void* component, const void* owner) {
    return menu_visual_component(component,mv_skeletal_class(),owner)
        && menu_visual_layout(component,mv_skeletal_class(),0x438);
}
static void touch_menu_visual_tick(void* hand) {
    void (*hidden)(void*,int,int) = __atomic_load_n(&mv_hidden,__ATOMIC_ACQUIRE);
    if (!hidden || !phone_player || !phone_active || !phone_type || !hand) return;
    unsigned type = phone_type();
    // Menu pawns (2/4/7) and in-world menu overlays of Bio4/Typewriter/Merchant (1/6/8).
    if (type<1 || type>8 || type==3 || type==5 || !menu_visual_layout(hand,mv_hand_class(),0x528)) return;
    unsigned side = *(const unsigned char*)((const char*)hand+0x314);
    if (side>1) return;
    void* pawn = phone_player(hand);
    if (!pawn || pawn!=phone_active()) return;
    int menu_pawn = menu_visual_layout(pawn,mv_pawn_class(),0xbb0);
    if (type==1 && menu_pawn) return; // A pooled MenuPawn must not hide gameplay avatar leaves during a context transition.
    if (!menu_pawn && type!=1 && type!=6 && type!=8) return;

    // The exact authored laser model fields are separate from WidgetInteraction+0x510.
    void* line = *(void* const*)((const char*)hand+0x518);
    void* dot = *(void* const*)((const char*)hand+0x520);
    int rays = 0;
    void* models[2] = {line,dot};
    for (unsigned i=0;i<2;i++) {
        void* model = models[i];
        if (!menu_visual_component(model,mv_static_class(),hand)) continue;
        if (!(*(const unsigned short*)((const char*)model+0x14c)&0x400)) hidden(model,1,0);
        rays++;
    }

    // The native avatar getter selects only MeshSet.Mesh/SecondaryMesh. Reject a
    // MeshSet or mesh attached to another pawn, and never traverse attached props.
    void* meshset = menu_pawn ? *(void* const*)((const char*)pawn+0xba8) : 0;
    void* mesh = 0;
    int avatar = 0;
    if (menu_pawn && menu_visual_layout(meshset,mv_meshset_class(),0x238)) {
        void* root = *(void* const*)((const char*)meshset+0x130);
        void* pawn_root = *(void* const*)((const char*)pawn+0x130);
        void* primary = *(void* const*)((const char*)meshset+0x228);
        void* secondary = *(void* const*)((const char*)meshset+0x230);
        if (root && root==*(void* const*)((const char*)meshset+0x220)
            && menu_visual_component(root,mv_scene_class(),meshset)
            && menu_visual_component(pawn_root,mv_scene_class(),pawn)
            && *(void* const*)((const char*)root+0xc0)==pawn_root
            // GetMesh may read Secondary.SkeletalMesh+0x430 before returning.
            && menu_visual_avatar_component(primary,meshset)
            && (!secondary || menu_visual_avatar_component(secondary,meshset))) {
            mesh = mv_get_mesh(pawn,(unsigned char)side);
            if ((mesh==*(void* const*)((const char*)meshset+0x228)
                 || mesh==*(void* const*)((const char*)meshset+0x230))
                && menu_visual_avatar_component(mesh,meshset)
                && *(void* const*)((const char*)mesh+0xc0)==root) {
                // Native PreBio4Tick restores authored visibility every frame.
                // Use its visibility flag, rather than a sticky hidden-in-game bit.
                if (*(const unsigned short*)((const char*)mesh+0x14c)&0x20) mv_visible(mesh,0,0);
                avatar = 1;
            }
        }
    }
    if (mv_log_count[side]<4) {
        mv_log_count[side]++;
        LOGF("MENU VISUAL side=%u type=%u hand=%p pawn=%p line=%p dot=%p rays=%d meshset=%p mesh=%p avatar=%d",
             side,type,hand,pawn,line,dot,rays,meshset,mesh,avatar);
    }
}
static void install_touch_menu_visual(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[2]; } check[] = {
        {0x83adc18,{0xd10483ff,0xa90e5ffc}}, {0x83ad964,{0xd104c3ff,0xf90073fc}},
        {0x83ad924,{0x79429808,0x52808409}}, {0x8f82460,{0x79429808,0x32160108}},
        {0x6477678,{0xd10183ff,0xf90023f3}}, {0x64781dc,{0xd10183ff,0xf90023f3}},
        {0x6427ad4,{0xd10183ff,0xf90023f3}}, {0x8f82500,{0xd10183ff,0xf90023f3}},
        {0x8fdb46c,{0xd10183ff,0xf90023f3}}, {0x8f94650,{0xd10183ff,0xf90023f3}},
        {0x62af678,{0xf945d408,0xb40002a8}}, {0x62af6c8,{0xf9411500,0xd65f03c0}},
        {0x62af6d8,{0xf9411900,0xb4ffff60}}, {0x62a2378,{0xf9011260,0xf9009a60}},
        {0x62a3a24,{0xf9409a61,0x52804048}}, {0x62a59d0,{0xaa1303e0,0x94000303}},
        {0x6310244,{0xf9028e60,0x948275c7}}, {0x631029c,{0xf9029260,0x948275b1}},
        {0x633d030,{0xf9417800,0xd65f03c0}}, {0x63380dc,{0xd0020168,0xf9431508}},
        {0x6338aec,{0xd0020168,0xf9431508}},
    };
    static const struct { unsigned long params; unsigned offset; } fields[] = {
        {0x9ddce08,0x510}, {0x9ddcdd8,0x518}, {0x9ddcda8,0x520},
        {0x9dac9e0,0xba8}, {0x9da8c10,0x220}, {0x9da8be0,0x228},
        {0x9da8bb0,0x230}, {0xa0b7530,0x130}, {0xa235ba8,0xc0},
    };
    if (!base || __atomic_load_n(&mv_hidden,__ATOMIC_ACQUIRE)) return;
    for (unsigned i=0;i<sizeof check/sizeof check[0];i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,sizeof check[i].first)) {
            LOGF("MENU VISUAL disabled: opcode mismatch +%lx",check[i].addr); return;
        }
    for (unsigned i=0;i<sizeof fields/sizeof fields[0];i++)
        if (*(const unsigned*)(base+fields[i].params+0x24)!=fields[i].offset) {
            LOGF("MENU VISUAL disabled: property mismatch +%lx",fields[i].params); return;
        }
    if (phone_player!=(void* (*)(const void*))(base+0x633d030)
        || phone_active!=(void* (*)(void))(base+0x63380dc)
        || phone_type!=(unsigned (*)(void))(base+0x6338aec)) return;
    mv_hand_class=(void* (*)(void))(base+0x6477678);
    mv_pawn_class=(void* (*)(void))(base+0x64781dc);
    mv_meshset_class=(void* (*)(void))(base+0x6427ad4);
    mv_scene_class=(void* (*)(void))(base+0x8f82500);
    mv_static_class=(void* (*)(void))(base+0x8fdb46c);
    mv_skeletal_class=(void* (*)(void))(base+0x8f94650);
    mv_get_mesh=(void* (*)(const void*,unsigned char))(base+0x62af678);
    mv_visible=(void (*)(void*,int,int))(base+0x83ad964);
    __atomic_store_n(&mv_hidden,(void (*)(void*,int,int))(base+0x83adc18),__ATOMIC_RELEASE);
    LOGF("MENU VISUAL ready: active Pause/Menu/InWorldMenu exact laser and avatar leaves; native pointer retained");
}
