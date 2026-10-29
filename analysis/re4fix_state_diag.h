// One explicit console request, consumed by the right gameplay hand on GameThread.
struct pawn_diag_vec2 { float x, y; };
_Static_assert(sizeof(struct pawn_diag_vec2) == 8, "UE4 FVector2D HFA ABI");
static void* (*pd_player)(const void*);
static void* (*pd_active)(void);
static unsigned (*pd_type)(void);
static int (*pd_valid)(void), (*pd_regular)(const void*);
static int (*pd_over)(void), (*pd_qte)(void), (*pd_pending)(void);
static struct pawn_diag_vec2 (*pd_move)(const void*);
static void* (*pd_settings)(void);
static unsigned long pd_base;

static void touch_pawn_diag(void* hand) {
    if (!pd_player || !hand || *(const unsigned char*)((char*)hand+0x314) != 1
        || !__atomic_exchange_n(&g_pawn_diag_request,0,__ATOMIC_ACQ_REL)) return;
    void* pawn = pd_player(hand);
    void* active = pd_active();
    char pn[64]="", pc[64]="", an[64]="", ac[64]="";
    obj_name(pawn,pn,sizeof pn); obj_name(active,an,sizeof an);
    if (pawn) obj_name(*(void**)((char*)pawn+0x10),pc,sizeof pc);
    if (active) obj_name(*(void**)((char*)active+0x10),ac,sizeof ac);
    void* settings = pd_settings();
    struct pawn_diag_vec2 move = pawn ? pd_move(pawn) : (struct pawn_diag_vec2){0,0};
    LOGF("PAWNSTATE hand=%p side=1 pawn=%p '%s' class='%s' active=%p '%s' class='%s' type=%u",
         hand,pawn,pn,pc,active,an,ac,pd_type());
    LOGF("PAWNGATES valid=%d regular=%d context=%d move=%.3f,%.3f MovementStick=%d input=%p grip=%p over=%d QTE=%d pending=%d cooldown=%.3f pausequeue=%d",
         pd_valid(),pawn ? pd_regular(pawn) : -1,
         pawn ? *(const unsigned char*)((char*)pawn+0xcd1) : -1,move.x,move.y,
         settings ? *(const unsigned char*)((char*)settings+0x33) : -1,
         pawn ? *(void**)((char*)pawn+0xf8) : 0,f_GetCurrentGrip(hand),pd_over(),pd_qte(),pd_pending(),
         pawn ? *(const float*)((char*)pawn+0xc10) : -1.0,
         pawn ? *(const unsigned char*)((char*)pawn+0x4c0) : -1);
    const unsigned char* pl = *(const unsigned char**)(pd_base+0xa4e9b40);
    const unsigned char* game = *(const unsigned char**)(pd_base+0xa3f5e48);
    LOGF("BIO4GATES pPL=%p pG=%p game_mode=%d system50a8=%08x stop50ae=%02x flags19a=%02x pl_state114=%d state115=%d prev_a74=%d camera_a24=%d bio_init3dd=%d",
         pl,game,game ? game[0x24] : -1,game ? *(const unsigned*)(game+0x50a8) : 0,
         game ? game[0x50ae] : 0,game ? game[0x19a] : 0,pl ? pl[0x114] : -1,
         pl ? pl[0x115] : -1,pl ? *(const int*)(pl+0xa74) : -1,pl ? pl[0xa24] : -1,
         pl ? pl[0x3dd] : -1);
}
static void install_pawn_diag(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[2]; } check[] = {
        {0x633d030,{0xf9417800,0xd65f03c0}}, {0x63380dc,{0xd0020168,0xf9431508}},
        {0x6338aec,{0xd0020168,0xf9431508}}, {0x61d401c,{0xb0020c68,0xf946bd08}},
        {0x6216efc,{0xa9bf7bfd,0x910003fd}}, {0x62aa4fc,{0xd10483ff,0xfd0063ea}},
        {0x62c7e04,{0xa9be4ff4,0xa9017bfd}}, {0x61d4050,{0xb0020c68,0xf946bd08}},
        {0x62dd310,{0xb0020448,0xf9415508}}, {0x61d4760,{0xb0020c88,0xf9446508}},
    };
    for (unsigned i=0;i<sizeof check/sizeof check[0];i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,sizeof check[i].first)) {
            LOGF("PAWNSTATE disabled: opcode mismatch +%lx",check[i].addr); return;
        }
    pd_player = (void* (*)(const void*))(base+0x633d030);
    pd_active = (void* (*)(void))(base+0x63380dc);
    pd_type = (unsigned (*)(void))(base+0x6338aec);
    pd_valid = (int (*)(void))(base+0x61d401c);
    pd_regular = (int (*)(const void*))(base+0x6216efc);
    pd_move = (struct pawn_diag_vec2 (*)(const void*))(base+0x62aa4fc);
    pd_settings = (void* (*)(void))(base+0x62c7e04);
    pd_over = (int (*)(void))(base+0x61d4050);
    pd_qte = (int (*)(void))(base+0x62dd310);
    pd_pending = (int (*)(void))(base+0x61d4760);
    pd_base = base;
    LOGF("PAWNSTATE diagnostic installed (explicit state request only)");
}
