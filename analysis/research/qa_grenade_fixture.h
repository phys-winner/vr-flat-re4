// Temporary device QA only. Do not include or define RE4_QA_GRENADE_FIXTURE in a release build.
#if defined(RE4_QA_GRENADE_FIXTURE)
static volatile int qa_grenade_requested;
static int qa_grenade_used;
static int qa_grenade_fixture_command(const char* line) {
    char command[32],extra[2];
    if(sscanf(line,"%31s %1s",command,extra)!=1 || strcmp(command,"qagrenadefixture"))return 0;
    __atomic_store_n(&qa_grenade_requested,1,__ATOMIC_RELEASE);return 1;
}
static void qa_grenade_fixture_tick(void* hand) {
    if(!__atomic_load_n(&qa_grenade_requested,__ATOMIC_ACQUIRE) || !qs_player(hand)
        || !cinema_on_game_thread() || !gr_context() || rl_paused() || !rl_player_valid())return;
    if(!__atomic_exchange_n(&qa_grenade_requested,0,__ATOMIC_ACQ_REL))return;
    if(qa_grenade_used){LOGF("QA GRENADE fixture already consumed; no action");return;}
    unsigned long base=g_ue4_base_cached;
    const struct {unsigned offset,a,b;} guards[]={
        {0x5ffd1e4,0xd10283ff,0xa9046ffc},{0x62e3198,0xa9bf7bfd,0x910003fd},
        {0x5ffd6c0,0x97fcac6b,0xf94016c8},{0x5ffd6cc,0x3940cbe9,0x39002909},
        {0x5ffd6e0,0x39003109,0x3940d7e9},{0x61539ac,0x94063dfb,0x79415261},
        {0x61539b4,0x2a0003e2,0x2a1503e0},{0x61539bc,0x97faa60a,0x2a0003f4}
    };
    for(unsigned i=0;i<sizeof guards/sizeof *guards;i++) {
        const unsigned* code=(const unsigned*)(base+guards[i].offset);
        if(code[0]!=guards[i].a || code[1]!=guards[i].b){LOGF("QA GRENADE original API mismatch; no action");return;}
    }
    if(*(void**)(base+0xa361da0)!=(void*)(base+0xa500780))return;
    unsigned size=((unsigned (*)(void))(base+0x62e3198))();if(size>3)return;
    qa_grenade_used=1;
    // Original autopacking owns receipt, coordinates, orientation and InAttacheCase; ordinary inventory UI equips it.
    int accepted=((int (*)(unsigned,unsigned short,unsigned))(base+0x5ffd1e4))(1,1,size);
    LOGF("QA GRENADE original PutInCase id1 qty1 boardSize=%u accepted=%d; equip through original inventory UI",size,accepted);
}
#endif
