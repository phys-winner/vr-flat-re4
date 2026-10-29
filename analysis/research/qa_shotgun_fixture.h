// Temporary device QA only; include solely in a frozen QA source and never save this inventory.
#if defined(RE4_QA_SHOTGUN_FIXTURE)
#include <stdint.h>
static volatile int qa_shotgun_requested;
static int qa_shotgun_used;
static int qa_shotgun_fixture_command(const char* line) {
    char command[32],extra[2];
    if(!line || sscanf(line,"%31s %1s",command,extra)!=1 || strcmp(command,"qashotgunfixture"))return 0;
    __atomic_store_n(&qa_shotgun_requested,1,__ATOMIC_RELEASE);return 1;
}
static void* qa_shotgun_fixture_context(void* hand) {
    if(!cinema_on_game_thread() || !gr_context() || rl_paused() || !rl_player_valid())return 0;
    return qs_player(hand);
}
static int qa_shotgun_fixture_original_api(uintptr_t base) {
    if(!base)return 0;
    const struct {unsigned offset,a,b;} guards[]={
        {0x5ffd1e4,0xd10283ff,0xa9046ffc},{0x62e3198,0xa9bf7bfd,0x910003fd},
        {0x5ffd6c0,0x97fcac6b,0xf94016c8},{0x5ffd6cc,0x3940cbe9,0x39002909},
        {0x5ffd6e0,0x39003109,0x3940d7e9},{0x61539ac,0x94063dfb,0x79415261},
        {0x61539b4,0x2a0003e2,0x2a1503e0},{0x61539bc,0x97faa60a,0x2a0003f4}
    };
    for(unsigned i=0;i<sizeof guards/sizeof *guards;i++) {
        const unsigned* code=(const unsigned*)(base+guards[i].offset);
        if(code[0]!=guards[i].a || code[1]!=guards[i].b)return 0;
    }
    return *(void**)(base+0xa361da0)==(void*)(base+0xa500780)
        && *(const uint64_t*)(base+0x9dc5b48)==44 && *(const uint64_t*)(base+0x9dc5a08)==24;
}
static void qa_shotgun_fixture_apply(void* hand,unsigned size,int (*put_in_case)(unsigned,unsigned short,unsigned)) {
    void* pawn=qa_shotgun_fixture_context(hand);
    if(!pawn || size>3 || qa_shotgun_used || !put_in_case)return;
    unsigned long long name=qs_name(pawn);qa_shotgun_used=1;
    // Original autopack owns acquisition, placement and flags; the player equips through the ordinary inventory.
    int gun=put_in_case(44,1,size),ammo=0;
    if(gun && qa_shotgun_fixture_context(hand)==pawn && qs_name(pawn)==name)ammo=put_in_case(24,20,size);
    LOGF("QA SHOTGUN original PutInCase Shotgun44 qty1 accepted=%d Bullet_12gg24 qty20 accepted=%d boardSize=%u; equip through original inventory UI; DO NOT SAVE",gun,ammo,size);
}
static void qa_shotgun_fixture_tick(void* hand) {
    if(!__atomic_load_n(&qa_shotgun_requested,__ATOMIC_ACQUIRE) || !qa_shotgun_fixture_context(hand))return;
    if(!__atomic_exchange_n(&qa_shotgun_requested,0,__ATOMIC_ACQ_REL))return;
    if(qa_shotgun_used){LOGF("QA SHOTGUN fixture already consumed; no action");return;}
    uintptr_t base=g_ue4_base_cached;
    if(!qa_shotgun_fixture_original_api(base)){LOGF("QA SHOTGUN original API/enum mismatch; no action");return;}
    unsigned size=((unsigned (*)(void))(base+0x62e3198))();
    qa_shotgun_fixture_apply(hand,size,(int (*)(unsigned,unsigned short,unsigned))(base+0x5ffd1e4));
}
#endif
