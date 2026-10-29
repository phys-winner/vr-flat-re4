// Explicit read-only probe after native GamePlayerHand::Tick. Include after phone.h.
// Root supplies volatile long g_knife_diag_until (CLOCK_MONOTONIC milliseconds).
static void* (*kd_primary)(const void*);
static unsigned (*kd_interactivity)(const void*);
static int (*kd_blending)(const void*), (*kd_flourishing)(const void*);
static int (*kd_player_valid)(void);
static long kd_window, kd_end, kd_next;
static void* kd_config_object;

static float knife_diag_speed(const void* object, unsigned offset) {
    const float* v=(const float*)((const char*)object+offset);
    return sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
}
static void touch_knife_diag(void* hand) {
    long until=__atomic_load_n(&g_knife_diag_until,__ATOMIC_ACQUIRE);
    if (!until || !kd_primary || !phone_player || !phone_active || !phone_type
        || !f_GetCurrentGrip || !hand) return;
    long now=ptr_now_ms();
    if (until<=now) return;
    if (until!=kd_window) {
        kd_window=until; kd_end=until-now>5000 ? now+5000 : until;
        kd_next=0; kd_config_object=0;
    }
    if (now>=kd_end || *(const unsigned char*)((char*)hand+0x314)!=1) return;
    void* pawn=phone_player(hand);
    if (!pawn || pawn!=phone_active() || now<kd_next) return;
    kd_next=now+100;
    unsigned type=phone_type();
    if (type!=1) { LOGF("KNIFEDIAG unavailable: active type=%u",type); return; }
    void* grip=f_GetCurrentGrip(hand);
    void* knife=grip ? *(void**)((char*)grip+0x20) : 0;
    void* cls=knife ? *(void**)((char*)knife+0x10) : 0;
    char kind[96]="";
    if (cls) obj_name(cls,kind,sizeof kind);
    int size=cls ? *(const int*)((char*)cls+0x58) : 0;
    if (strcmp(kind,"VR4GamePlayerKnife_Leon_BP_C") || size<0x6d0 || size>0x10000) {
        LOGF("KNIFEDIAG unavailable: grip=%p owner=%p class='%s' size=%x",grip,knife,kind,size);
        return;
    }
    if (*(void**)((char*)knife+0x3c0)!=pawn) {
        LOGF("KNIFEDIAG unavailable: knife=%p owning pawn mismatch",knife); return;
    }
    const char* k=(const char*)knife;
    if (kd_config_object!=knife) {
        kd_config_object=knife;
        LOGF("KNIFECFG knife=%p smooth=%.3f curves=%p/%p "
             "g1(rise/fall/start/stop/speed)=%.3f/%.3f/%.3f/%.3f/%.1f "
             "g2=%.3f/%.3f/%.3f/%.3f/%.1f g3=%.3f/%.3f/%.3f/%.3f/%.1f",
             knife,*(const float*)(k+0x608),*(void* const*)(k+0x5f8),*(void* const*)(k+0x600),
             *(const float*)(k+0x5a4),*(const float*)(k+0x5a8),*(const float*)(k+0x5ac),
             *(const float*)(k+0x5b0),*(const float*)(k+0x5b4),
             *(const float*)(k+0x5c0),*(const float*)(k+0x5c4),*(const float*)(k+0x5c8),
             *(const float*)(k+0x5cc),*(const float*)(k+0x5d0),
             *(const float*)(k+0x5dc),*(const float*)(k+0x5e0),*(const float*)(k+0x5e4),
             *(const float*)(k+0x5e8),*(const float*)(k+0x5ec));
    }
    void* primary=*(void* const*)(k+0x270) ? kd_primary(knife) : 0;
    LOGF("KNIFEDIAG knife=%p hand=%p primary=%p inter=%u blend=%d flourish=%d valid=%d "
         "v680/v698/v68c=%.1f/%.1f/%.1f signal=%.3f/%.3f/%.3f "
         "gate=%u/%u/%u history=%d handv=%.1f overlaps=%d anim=%d elapsed=%ld weapon=%d",
         knife,hand,primary,kd_interactivity(knife)&255u,kd_blending(knife),
         kd_flourishing(knife),kd_player_valid(),knife_diag_speed(knife,0x680),
         knife_diag_speed(knife,0x698),knife_diag_speed(knife,0x68c),
         *(const float*)(k+0x5b8),*(const float*)(k+0x5d4),*(const float*)(k+0x5f0),
         *(const unsigned char*)(k+0x6a4),*(const unsigned char*)(k+0x6a5),
         *(const unsigned char*)(k+0x6a6),*(const int*)((char*)hand+0x300),
         knife_diag_speed(hand,0x308),*(const int*)(k+0x6c8),g_anim_kind,
         now-g_anim_t0,g_touch_weapon);
}
static void install_touch_knife_diag(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[2]; } check[] = {
        {0x629a774,{0xf9421408,0xb4000268}}, {0x633d030,{0xf9417800,0xd65f03c0}},
        {0x63380dc,{0xd0020168,0xf9431508}}, {0x6338aec,{0xd0020168,0xf9431508}},
        {0x62bbc30,{0xf9413800,0x17ff1a5f}}, {0x629f8cc,{0xf81e0ff3,0xa9017bfd}},
        {0x62bb880,{0xf81e0ff3,0xa9017bfd}}, {0x62bb584,{0xf81e0ff3,0xa9017bfd}},
        {0x61d401c,{0xb0020c68,0xf946bd08}},
        // UObject Outer, UStruct size, native knife size and sampled field layouts.
        {0x69b6500,{0xaa0003e8,0xf9401000}}, {0x687d798,{0x294b2408,0x2a0203f6}},
        {0x64266bc,{0x5280da04,0x52800205}}, {0x633aeb8,{0xbd430808,0xbd430c09}},
        {0x629f6f4,{0x395a9268,0x34000828}}, {0x629f80c,{0x91169260,0x911a0261}},
        {0x629f450,{0x91177260,0x4ea81d00}}, {0x61ce778,{0x2d420001,0x1e212040}},
        {0x61ce794,{0x39406268,0xbd001660}}, {0x629f158,{0xbd460a60,0x911a6274}},
        {0x629f9a4,{0x911b0014,0xf81683a8}},
    };
    kd_primary=0;
    for (unsigned i=0;i<sizeof check/sizeof check[0];i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,sizeof check[i].first)) {
            LOGF("KNIFEDIAG disabled: opcode mismatch +%lx",check[i].addr); return;
        }
    kd_primary=(void* (*)(const void*))(base+0x62bbc30);
    kd_interactivity=(unsigned (*)(const void*))(base+0x629f8cc);
    kd_blending=(int (*)(const void*))(base+0x62bb880);
    kd_flourishing=(int (*)(const void*))(base+0x62bb584);
    kd_player_valid=(int (*)(void))(base+0x61d401c);
    LOGF("KNIFEDIAG ready (explicit window <=5s, sample >=100ms)");
}
