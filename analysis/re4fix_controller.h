// Include after phone reflection helpers; run after the native gameplay hand Tick.
static void* (*tc_hand_class)(void);
static void* (*tc_component_class)(void);
static void* (*tc_mesh_class)(void);
static void (*tc_hide)(void*,int,int);
static void* tc_seen_component[2], *tc_seen_mesh[2];
static unsigned tc_check_count[2];
struct controller_check { const void* cls; int size, isa, matched, offset, element, dim; char kind[48]; };

static int controller_is_a(const void* object, const void* cls) {
    const char* actual = object ? *(const char* const*)((const char*)object+0x10) : 0;
    if (!actual || !cls) return 0;
    int depth = *(const int*)((const char*)cls+0x38), actual_depth = *(const int*)(actual+0x38);
    const void* const* chain = *(const void* const**)(actual+0x30);
    return depth >= 0 && actual_depth <= 1024 && depth <= actual_depth && chain
        && chain[depth] == (const char*)cls+0x30;
}
static int controller_art_name(const char* name) {
    return name && !strncmp(name,"CMN_Controller_Quest2_",sizeof("CMN_Controller_Quest2_")-1);
}
static void* controller_component(void* hand, struct controller_check* check) {
    const char* cls = *(const char* const*)((const char*)hand+0x10);
    int size = cls ? *(const int*)(cls+0x58) : 0;
    check->cls = cls; check->size = size;
    if (size < 0x550 || size > 0x10000) return 0;
    check->isa = controller_is_a(hand,tc_hand_class());
    if (!check->isa) return 0;
    void* prop = *(void* const*)(cls+0x70);
    for (unsigned i = 0; prop && i < 96; i++) {
        char name[64] = "", kind[48] = "";
        phone_inspect_fname((const char*)prop+0x28,name,sizeof name);
        if (!strcmp(name,"Controller")) {
            phone_inspect_fname(*(void* const*)((const char*)prop+0x8),kind,sizeof kind);
            check->matched = 1; memcpy(check->kind,kind,sizeof kind);
            check->element = *(const int*)((const char*)prop+0x38);
            check->dim = *(const int*)((const char*)prop+0x34);
            check->offset = *(const int*)((const char*)prop+0x4c);
            if (strcmp(kind,"ObjectProperty") || check->element != 8
                || check->dim != 1 || check->offset != 0x548) return 0;
            return *(void* const*)((const char*)hand+0x548);
        }
        void* next = *(void* const*)((const char*)prop+0x58);
        if (next == prop) break;
        prop = next;
    }
    return 0;
}
static void touch_controller_tick(void* hand) {
    if (!phone_player || !phone_active || !phone_type || !hand || phone_type() != 1) return;
    unsigned side = *(const unsigned char*)((const char*)hand+0x314);
    void* pawn = phone_player(hand);
    if (side > 1 || !pawn || pawn != phone_active()) return;
    if (!tc_hide || !phone_inspect_ready) {
        if (tc_check_count[side] < 4) {
            tc_check_count[side]++;
            LOGF("CONTROLLERCHECK side=%u reason=dependencies hand=%p native=%d reflection=%d",side,hand,tc_hide!=0,phone_inspect_ready);
        }
        return;
    }
    struct controller_check check = {0};
    void* component = controller_component(hand,&check);
    const char* cls = component ? *(const char* const*)((const char*)component+0x10) : 0;
    int size = cls ? *(const int*)(cls+0x58) : 0;
    int is_component = size >= 0x448 && size <= 0x10000 && controller_is_a(component,tc_component_class());
    void* owner = is_component ? *(void* const*)((const char*)component+0x20) : 0;
    void* mesh = is_component && owner == hand ? *(void* const*)((const char*)component+0x440) : 0;
    int is_mesh = mesh && controller_is_a(mesh,tc_mesh_class());
    if (tc_check_count[side] < 4) {
        tc_check_count[side]++;
        const char* reason = !component ? "hand/Controller metadata" : !is_component ? "component class/size"
            : owner != hand ? "component Outer" : !mesh ? "null asset" : !is_mesh ? "asset class" : "binding passed";
        LOGF("CONTROLLERCHECK side=%u reason='%s' hand=%p class=%p size=%x isa=%d metadata=%d kind='%s' off=%x elem=%d dim=%d component=%p class=%p size=%x isStatic=%d Outer=%p mesh=%p isMesh=%d",
             side,reason,hand,check.cls,check.size,check.isa,check.matched,check.kind,check.offset,check.element,check.dim,
             component,cls,size,is_component,owner,mesh,is_mesh);
    }
    if (!is_component || owner != hand || !is_mesh) return;
    char name[96] = "";
    obj_name(mesh,name,sizeof name);
    int allowed = controller_art_name(name);
    if (tc_seen_component[side] != component || tc_seen_mesh[side] != mesh) {
        tc_seen_component[side] = component; tc_seen_mesh[side] = mesh;
        LOGF("CONTROLLERMODEL side=%u component=%p asset=%p '%s' hide=%d",side,component,mesh,name,allowed);
    }
    if (allowed && !(*(const unsigned short*)((const char*)component+0x14c) & 0x400))
        tc_hide(component,1,0); // Controller leaf only: Gun/HandProxy and attached weapons remain visible.
}
// The dynamic tutorial (new game with "Tutorial Assistance") floats Quest controller models with highlighted
// buttons at the hands (AVR4DynamicTutorialManager::UpdateTutorialControllers). The phone instructions name the
// touch icons instead, so the manager is told it cannot display them and closes its tutorial hands.
int hook_tutorial_controllers(void* self) { (void)self; return 0; }
// The weapon-management tutorial spawns AVR4TutorialHand actors itself (Quest controller with button labels at
// the hands); they are hidden once at BeginPlay. Their own SetVisibility calls cannot undo bHiddenInGame.
unsigned long g_tgt_tutorial_hand;
#ifndef CONTROLLER_TEST
__attribute__((naked)) static void gate_tutorial_hand_begin(void* self) {
    asm volatile(".inst 0xa9bd57f6\n" ".inst 0xa9014ff4\n" ".inst 0xa9027bfd\n" ".inst 0x910083fd\n"
        "adrp x16, :got:g_tgt_tutorial_hand\n" "ldr x16, [x16, :got_lo12:g_tgt_tutorial_hand]\n" "ldr x16, [x16]\n" "br x16\n");
}
#else
static void gate_tutorial_hand_begin(void* self);
#endif
void hook_tutorial_hand_begin(void* self) {
    gate_tutorial_hand_begin(self);
    void* root = self ? *(void* const*)((const char*)self + 0x130) : 0;   // AActor::RootComponent
    if (root && tc_hide) {
        tc_hide(root, 1, 2);
        static unsigned logs;
        if (logs < 8) { logs++; LOGF("TUTORIAL hand %p hidden", self); }
    }
}
static void install_touch_controller(unsigned long base) {
    static const unsigned tutorial[4] = {0xd100c3ff,0xa9014ff4,0xa9027bfd,0x910083fd};   // CanDisplayTutorialControllers
    if (memcmp((const void*)(base+0x63f1ac8),tutorial,sizeof tutorial))
        LOGF("TUTORIAL controllers kept: opcode mismatch +63f1ac8");
    else if (patch_code((void*)(base+0x63f1ac8),(void*)&hook_tutorial_controllers))
        LOGF("TUTORIAL controllers kept: patch failed");
    else LOGF("TUTORIAL controllers hidden (CanDisplayTutorialControllers -> false)");
    static const struct { unsigned long addr; unsigned first[2]; } check[] = {
        {0x83adc18,{0xd10483ff,0xa90e5ffc}}, {0x83adcac,{0x1215792b,0x910532aa}},
        {0x83adcdc,{0x71000adf,0x54fffc83}}, {0x8f82460,{0x79429808,0x32160108}},
        {0x8fdb46c,{0xd10183ff,0xf90023f3}}, {0x8fd8478,{0xd10183ff,0xf90023f3}},
        {0x63c56e4,{0xd10183ff,0xf90023f3}}, {0x83ebdc0,{0xf9422008,0xeb01011f}},
        {0x69b6500,{0xaa0003e8,0xf9401000}}, {0x687d798,{0x294b2408,0x2a0203f6}},
    };
    tc_hide = 0;
    for (unsigned i = 0; i < sizeof check/sizeof check[0]; i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,sizeof check[i].first)) {
            LOGF("CONTROLLERMODEL disabled: opcode mismatch +%lx",check[i].addr); return;
        }
    if (!controller_art_name("CMN_Controller_Quest2_Right_Base") || controller_art_name("Wep02")) return;
    tc_hand_class = (void* (*)(void))(base+0x63c56e4);
    tc_component_class = (void* (*)(void))(base+0x8fdb46c);
    tc_mesh_class = (void* (*)(void))(base+0x8fd8478);
    tc_hide = (void (*)(void*,int,int))(base+0x83adc18);
    LOGF("CONTROLLERMODEL ready (reflected Controller leaf, verified Quest2 asset only)");
    static const unsigned hand_begin[4] = {0xa9bd57f6,0xa9014ff4,0xa9027bfd,0x910083fd};   // AVR4TutorialHand::BeginPlay
    g_tgt_tutorial_hand = base+0x636f968;
    if (memcmp((const void*)(base+0x636f958),hand_begin,sizeof hand_begin)) LOGF("TUTORIAL hands kept: opcode mismatch +636f958");
    else if (patch_code((void*)(base+0x636f958),(void*)&hook_tutorial_hand_begin)) LOGF("TUTORIAL hands kept: patch failed");
    else LOGF("TUTORIAL hands hidden at BeginPlay");
}
