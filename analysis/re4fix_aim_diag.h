// Read-only, one explicit request; include after reload/body helpers and q_rot.
#ifndef RE4FIX_AIM_DIAG_H
#define RE4FIX_AIM_DIAG_H
static struct reload_tf (*aim_head)(const void*);
static struct reload_tf (*aim_sights)(const void*);
static struct reload_tf (*aim_muzzle)(const void*);
struct __attribute__((aligned(16))) aim_fire_tf { struct reload_tf tf; unsigned char scale_type,padding[15]; };
_Static_assert(sizeof(struct aim_fire_tf)==64 && offsetof(struct aim_fire_tf,scale_type)==48,"native UnifiedScaleTransform ABI");
static struct aim_fire_tf (*aim_fire)(const void*);
static int (*aim_prop_interactivity)(const void*);
static void* (*aim_holster)(const void*);
static int (*aim_ammo_current)(const void*,int);
static int (*aim_ammo_capacity)(const void*), (*aim_ammo_reserves)(const void*);
static int (*aim_gate_trigger)(const void*), (*aim_gate_blocked)(const void*), (*aim_gate_ready)(const void*);
static int (*aim_gate_pistol_ready)(const void*);
static int (*aim_gate_blending)(const void*), (*aim_gate_flourishing)(const void*), (*aim_gate_debug)(void);
static void* (*aim_gate_hand_class)(void);
static unsigned aim_gate_logs;

static void aim_log_grip(const char* label, void* grip) {
    void* owner = grip ? *(void**)((char*)grip+0x20) : 0;
    char name[96]="", cls[96]="";
    if (owner) {
        obj_name(owner,name,sizeof name);
        obj_name(*(void**)((char*)owner+0x10),cls,sizeof cls);
    }
    LOGF("AIMGRIP %s grip=%p registered=%d owner=%p name='%s' class='%s'",
         label,grip,grip ? reload_grip_registered(grip) : 0,owner,name,cls);
}

static int aim_tf_valid(const struct reload_tf* t) {
    float norm = t->q.x*t->q.x + t->q.y*t->q.y + t->q.z*t->q.z + t->q.w*t->q.w;
    if (!isfinite(norm) || fabsf(norm - 1.0f) > 0.01f) return 0;
    for (int i = 0; i < 3; i++)
        if (!isfinite(t->p[i]) || !isfinite(t->s[i]) || fabsf(t->s[i]) < 0.000001f) return 0;
    return 1;
}
static void aim_log_tf(const char* name, const struct reload_tf* t, const struct reload_tf* head) {
    qf_t inv = {-head->q.x,-head->q.y,-head->q.z,head->q.w};
    float delta[3] = {t->p[0]-head->p[0],t->p[1]-head->p[1],t->p[2]-head->p[2]}, view_p[3];
    float x_axis[3] = {1,0,0}, world_f[3], view_f[3];
    q_rot(inv,delta,view_p); q_rot(t->q,x_axis,world_f); q_rot(inv,world_f,view_f);
    LOGF("AIM %s p=%.3f,%.3f,%.3f q=%.4f,%.4f,%.4f,%.4f scale=%.3f,%.3f,%.3f view_delta=%.3f,%.3f,%.3f view_forward=%.4f,%.4f,%.4f yaw_error=%.2f pitch_error=%.2f",
         name,t->p[0],t->p[1],t->p[2],t->q.x,t->q.y,t->q.z,t->q.w,t->s[0],t->s[1],t->s[2],
         view_p[0],view_p[1],view_p[2],view_f[0],view_f[1],view_f[2],
         atan2f(view_f[1],view_f[0])*57.29578f,
         atan2f(view_f[2],hypotf(view_f[0],view_f[1]))*57.29578f);
}
static void touch_aim_diag(void* hand) {
    if (!aim_head || !aim_prop_interactivity || !aim_holster || !body_player || !rl_gun1 || !hand ||
        *(const unsigned char*)((const char*)hand+0x314) != 1 ||
        !__atomic_load_n(&g_aim_diag_request,__ATOMIC_ACQUIRE)) return;
    void* pawn = body_player(hand);
    if (!pawn || pawn != body_active()) return;
    if (!__atomic_exchange_n(&g_aim_diag_request,0,__ATOMIC_ACQ_REL)) return;
    if (body_type() != 1) { LOGF("AIM rejected: active pawn is not Bio4"); return; }
    void* gun = rl_gun1(pawn);
    void* cls = gun ? *(void**)((char*)gun+0x10) : 0;
    int size = cls ? *(const int*)((char*)cls+0x58) : 0;
    const char* hand_cls=*(const char* const*)((const char*)hand+0x10);
    int hand_size=hand_cls ? *(const int*)(hand_cls+0x58) : 0;
    char name[96]="", kind[96]="";
    if (gun) obj_name(gun,name,sizeof name);
    if (cls) obj_name(cls,kind,sizeof kind);
    int fields_valid = gun && size>=0x3c8 && size<=0x10000;
    void* primary_grip = fields_valid ? *(void**)((char*)gun+0x270) : 0;
    void* primary_hand = primary_grip ? rl_primary(gun) : 0;
    void* current_grip = f_GetCurrentGrip(hand);
    void* holster = fields_valid ? aim_holster(gun) : 0;
    void* holster_cls = holster ? *(void**)((char*)holster+0x10) : 0;
    int holster_size = holster_cls ? *(const int*)((char*)holster_cls+0x58) : 0;
    int holster_valid = holster && holster_size>=0x350 && holster_size<=0x10000;
    void* holster_grip = holster_valid ? *(void**)((char*)holster+0x248) : 0;
    char holster_name[96]="", holster_kind[96]="";
    if (holster) obj_name(holster,holster_name,sizeof holster_name);
    if (holster_cls) obj_name(holster_cls,holster_kind,sizeof holster_kind);
    LOGF("AIMGRAB pawn=%p hand=%p gun=%p name='%s' class='%s' size=%x pistol=%d "
         "gun_pawn=%p primary=%p current=%p inter=%d base_inter=%d "
         "holster=%p name='%s' class='%s' size=%x holster_pawn=%p",
         pawn,hand,gun,name,kind,size,gun ? reload_is_pistol(gun) : 0,
         fields_valid ? *(void**)((char*)gun+0x3c0) : 0,primary_hand,current_grip,
         fields_valid ? rl_interactivity(gun) : -1,
         fields_valid ? aim_prop_interactivity(gun) : -1,
         holster,holster_name,holster_kind,holster_size,
         holster_valid ? *(void**)((char*)holster+0x348) : 0);
    aim_log_grip("gun_primary",primary_grip);
    aim_log_grip("current",current_grip);
    aim_log_grip("gun_holster",holster_grip);
    if (fields_valid && size>=0xd30 && aim_ammo_current && aim_ammo_capacity && aim_ammo_reserves
        && rl_player_valid && rl_paused && rl_player_valid() && !rl_paused()
        && *(void* const*)((const char*)gun+0x3c0)==pawn && primary_hand==hand && current_grip==primary_grip
        && primary_grip && reload_grip_registered(primary_grip)
        && *(void* const*)((const char*)primary_grip+0x20)==gun
        && reload_is_pistol(gun) && rl_interactivity(gun)==2) {
        int current=aim_ammo_current(gun,0),include_ejected=aim_ammo_current(gun,1);
        int capacity=aim_ammo_capacity(gun),reserves=aim_ammo_reserves(gun);
        LOGF("AIMAMMO gun=%p current=%d include_ejected=%d capacity=%d reserves=%d reload_phase=%d",
             gun,current,include_ejected,capacity,reserves,g_reload_phase);
        if (aim_gate_logs<32 && aim_gate_trigger && aim_gate_blocked && aim_gate_ready
            && aim_gate_blending && aim_gate_flourishing && aim_gate_debug && aim_gate_hand_class
            && hand_size>=0x520 && hand_size<=0x10000 && reload_is_a(hand,aim_gate_hand_class())) {
            int trigger=aim_gate_trigger(gun),blocked=aim_gate_blocked(gun),ready=aim_gate_ready(gun);
            int blending=aim_gate_blending(gun),flourishing=aim_gate_flourishing(gun),debug=aim_gate_debug();
            int pistol_ready=-1,cocked=-1;
            if (size>=0xde0 && aim_gate_pistol_ready && rl_grip_class) {
                void* slide=*(void* const*)((const char*)gun+0xd38);
                const void* const* vtable=*(const void* const* const*)gun;
                if (vtable && vtable[0x778/sizeof(void*)]==(const void*)aim_gate_pistol_ready
                    && reload_grip_registered(slide) && reload_layout(slide,0x460)
                    && reload_is_a(slide,rl_grip_class())
                    && *(void* const*)((const char*)slide+0x20)==gun
                    && *(void* const*)((const char*)slide+0x300)==gun) {
                    pistol_ready=aim_gate_pistol_ready(gun);
                    cocked=*(const unsigned char*)((const char*)gun+0xdd5);
                }
            }
            aim_gate_logs++;
            LOGF("AIMGATES gun=%p hand=%p trigger=%d blocked=%d ready=%d hand_trigger=%u raise=%.6f fire_timer=%.6f blend=%d flourish=%d debug=%d pistol_ready=%d cocked=%d",
                 gun,hand,trigger,blocked,ready,(unsigned)*(const unsigned char*)((const char*)hand+0x33c),
                 *(const float*)((const char*)hand+0x51c),*(const float*)((const char*)gun+0xc58),
                 blending,flourishing,debug,pistol_ready,cocked);
        }
    }
    if (!fields_valid || primary_hand != hand || !reload_is_pistol(gun)) {
        LOGF("AIM rejected: this right hand is not holding the pistol"); return;
    }
    void* root = *(void**)((char*)hand+0x130);
    if (!root || !*(void**)((char*)pawn+0x2d8) || !*(void**)((char*)gun+0x540)) {
        LOGF("AIM rejected: missing hand/head/gun component"); return;
    }
    struct reload_tf h, camera = aim_head(pawn), sights = aim_sights(gun), muzzle = aim_muzzle(gun);
    memcpy(&h,(char*)root+0x1c0,sizeof h);
    if (!aim_tf_valid(&camera) || !aim_tf_valid(&h) || !aim_tf_valid(&sights) || !aim_tf_valid(&muzzle)) {
        const struct reload_tf* values[] = {&camera,&h,&sights,&muzzle};
        const char* names[] = {"head","hand","sights","muzzle"};
        for (unsigned i = 0; i < 4; i++) {
            const struct reload_tf* t = values[i];
            LOGF("AIMRAW %s q=%.6f,%.6f,%.6f,%.6f p=%.6f,%.6f,%.6f scale=%.6f,%.6f,%.6f valid=%d",
                 names[i],t->q.x,t->q.y,t->q.z,t->q.w,t->p[0],t->p[1],t->p[2],t->s[0],t->s[1],t->s[2],aim_tf_valid(t));
        }
        LOGF("AIM rejected: invalid native transform"); return;
    }
    LOGF("AIM probe pawn=%p hand=%p gun=%p reload_phase=%d (UE axes +X forward,+Y right,+Z up)",
         pawn,hand,gun,g_reload_phase);
    aim_log_tf("head",&camera,&camera); aim_log_tf("hand",&h,&camera);
    aim_log_tf("sights",&sights,&camera); aim_log_tf("muzzle",&muzzle,&camera);
    if (aim_fire && size>=0x630 && hand_size>=0x51c && hand_size<=0x10000
        && current_grip==primary_grip && primary_grip && reload_grip_registered(primary_grip)
        && *(void* const*)((const char*)primary_grip+0x20)==gun
        && *(void* const*)((const char*)gun+0x3c0)==pawn && rl_interactivity(gun)==2
        && rl_player_valid && rl_paused && rl_player_valid() && !rl_paused()) {
        struct aim_fire_tf fire=aim_fire(gun);
        // Native handgun's legacy -X shot becomes FireBone local +Y in UE.
        struct reload_tf shot=fire.tf;
        shot.q=q_mul(shot.q,(qf_t){0,0,0.70710678f,0.70710678f});
        if (aim_tf_valid(&shot)) {
            LOGF("AIMFIRE scale_type=%u native_axis=+Y",(unsigned)fire.scale_type);
            aim_log_tf("fire",&shot,&camera);
        } else LOGF("AIMFIRE rejected: invalid native FireBone transform");
    }
}
static void install_touch_aim_diag(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[2]; } check[] = {
        {0x6341aec,{0xf9416c09,0xad4e8122}}, {0x6290628,{0xf942a009,0xf9430c01}},
        {0x62912f0,{0xf942a009,0xf9431401}}, {0x633caa8,{0xf9409a88,0xf9406108}},
        {0x62bb5cc,{0xa9be4ff4,0xa9017bfd}}, {0x62bbd7c,{0xf941e808,0xb4000268}},
        {0x62bbc30,{0xf9413800,0x17ff1a5f}}, {0x62c0510,{0xf9412669,0xf8357909}},
        {0x62c0418,{0xf941a400,0xb4ffff00}}, {0x62bb648,{0xf941e260,0xb4ffff40}},
        {0x6291744,{0xa9be4ff4,0xa9017bfd}}, {0x629182c,{0xf81e0ff3,0xa9017bfd}},
        {0x62918d4,{0xf81d0ff5,0xa9014ff4}}, {0x61d401c,{0xb0020c68,0xf946bd08}},
        {0x61d0934,{0xb0020ca8,0xf9446508}},
        {0x6290d8c,{0xd10683ff,0xfd009bea}}, {0x6380de8,{0xad408042,0x3dc00041}},
    };
    if (!base || aim_head) return;
    if (!body_player || !body_active || !body_type || !rl_gun1 || !rl_primary || !rl_pistol_class
        || !rl_interactivity || !f_GetCurrentGrip || !rl_player_valid || !rl_paused) {
        LOGF("AIM diagnostic disabled: body/reload dependencies unavailable"); return;
    }
    for (unsigned i=0;i<sizeof check/sizeof check[0];i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,sizeof check[i].first)) {
            LOGF("AIM diagnostic disabled: opcode mismatch +%lx",check[i].addr); return;
        }
    aim_sights = (struct reload_tf (*)(const void*))(base+0x6290628);
    aim_muzzle = (struct reload_tf (*)(const void*))(base+0x62912f0);
    aim_prop_interactivity = (int (*)(const void*))(base+0x62bb5cc);
    aim_holster = (void* (*)(const void*))(base+0x62bbd7c);
    aim_ammo_current = (int (*)(const void*,int))(base+0x6291744);
    aim_ammo_capacity = (int (*)(const void*))(base+0x629182c);
    aim_ammo_reserves = (int (*)(const void*))(base+0x62918d4);
    aim_fire = (struct aim_fire_tf (*)(const void*))(base+0x6290d8c);
    aim_head = (struct reload_tf (*)(const void*))(base+0x6341aec);
    LOGF("AIM diagnostic installed (explicit gun/holster grab state and held-pistol transform)");
    static const struct { unsigned long addr; unsigned first[2]; } gate_check[] = {
        {0x628d978,{0xf81e0ff3,0xa9017bfd}}, {0x628d20c,{0xa9bf7bfd,0x910003fd}},
        {0x628d268,{0xf81e0ff3,0xa9017bfd}}, {0x633d040,{0x394cf000,0xd65f03c0}},
        {0x6214c24,{0xbd451c00,0xd65f03c0}}, {0x628d2d8,{0xbd4c5800,0x1e202008}},
        {0x62bb880,{0xf81e0ff3,0xa9017bfd}}, {0x62bb584,{0xf81e0ff3,0xa9017bfd}},
        {0x6338b5c,{0x2a1f03e0,0xd65f03c0}}, {0x63c56e4,{0xd10183ff,0xf90023f3}},
        {0x63c576c,{0x5280a404,0x52800205}},
        {0x62b6034,{0xf81e0ff3,0xa9017bfd}}, {0x62b6040,{0x39775408,0x340000a8}},
        {0x62b604c,{0xf9469c00,0x97ff3158}}, {0x62825b0,{0xf9417c08,0xb4000268}},
        {0x64192f4,{0xd10183ff,0xf90023f3}}, {0x642e548,{0xd10183ff,0xf90023f3}},
        {0x641937c,{0x52808c04,0x52800205}},
        {0x642e5d0,{0x5281bc04,0x52800205}},
    };
    for (unsigned i=0;i<sizeof gate_check/sizeof gate_check[0];i++)
        if (memcmp((const void*)(base+gate_check[i].addr),gate_check[i].first,sizeof gate_check[i].first)) {
            LOGF("AIMGATES disabled: opcode mismatch +%lx",gate_check[i].addr); return;
        }
    static const struct { unsigned long addr; unsigned value; } gate_fields[] = {
        {0x9dadefc,0xd38}, {0x9d9e2bc,0x300},
    };
    for (unsigned i=0;i<sizeof gate_fields/sizeof gate_fields[0];i++)
        if (*(const unsigned*)(base+gate_fields[i].addr)!=gate_fields[i].value) {
            LOGF("AIMGATES disabled: field mismatch +%lx",gate_fields[i].addr); return;
        }
    aim_gate_trigger=(int (*)(const void*))(base+0x628d978);
    aim_gate_blocked=(int (*)(const void*))(base+0x628d20c);
    aim_gate_ready=(int (*)(const void*))(base+0x628d268);
    aim_gate_pistol_ready=(int (*)(const void*))(base+0x62b6034);
    aim_gate_blending=(int (*)(const void*))(base+0x62bb880);
    aim_gate_flourishing=(int (*)(const void*))(base+0x62bb584);
    aim_gate_debug=(int (*)(void))(base+0x6338b5c);
    aim_gate_hand_class=(void* (*)(void))(base+0x63c56e4);
    LOGF("AIMGATES installed: explicit owned-pistol state only, native const firing gates, limit32");
}
#endif
