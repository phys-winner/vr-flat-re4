// Diagnostic builds only: original transition APIs, consumed once on the GameThread.
#ifdef RE4_QA_USER_TRANSITIONS
static unsigned qa_transition_request;
static int qa_user_transitions_command(const char* action) {
    unsigned value=0;
    if(!strcmp(action,"mercend"))value=1;
    else if(!strcmp(action,"inspect"))value=2;
    else if(!strcmp(action,"chapter"))value=3;
    else if(!strcmp(action,"luis"))value=4;
    else if(!strcmp(action,"watchleft"))value=5;
    else if(!strcmp(action,"watchright"))value=6;
    else if(!strcmp(action,"capoff"))value=7;
    else if(!strcmp(action,"cap60"))value=8;
    else if(!strcmp(action,"door101"))value=9;
    else if(!strcmp(action,"door212"))value=10;
    else if(!strcmp(action,"spinel"))value=11;
    else if(!strcmp(action,"merchant"))value=12;
    else if(!strcmp(action,"treasurestate"))value=13;
    else if(!strcmp(action,"merchantintro"))value=14;
    else if(!strcmp(action,"houseintro"))value=15;
    else if(!strncmp(action,"room",4) && strlen(action)==7) {
        char* end=0;unsigned long room=strtoul(action+4,&end,16);
        if(end!=action+7 || !((room>=0x100 && room<=0x120) || (room>=0x200 && room<=0x22c)
            || (room>=0x300 && room<=0x333) || (room>=0x400 && room<=0x404 && room!=0x401)))return 0;
        value=0x10000u|(unsigned)room;
    }
    if(!value)return 0;
    __atomic_store_n(&qa_transition_request,value,__ATOMIC_RELEASE);return 1;
}
// R106's authored AEV2 schedules its original yielding scene callback in SceExec.
// Validate the registered level: zero would call that callback on this thread.
static int qa_luis_fixture_ready(const unsigned char* game,const unsigned char* manager,
    uintptr_t base,unsigned replay,const void* current_task,const unsigned char* tasks) {
    if(!game || !manager || !tasks || replay
        || (current_task && (uintptr_t)current_task!=base+0xa40a0f8 && (uintptr_t)current_task!=(uintptr_t)-1))return 0;
    unsigned short room;memcpy(&room,game+0x5038,sizeof room);
    if(room!=0x106 || (game[0x535a]&0x20))return 0;
    const unsigned char *header,*entries;memcpy(&header,manager,sizeof header);
    memcpy(&entries,manager+8,sizeof entries);
    if(!header || (uintptr_t)entries!=(uintptr_t)header+0x10)return 0;
    unsigned short version,count;memcpy(&version,header+4,sizeof version);memcpy(&count,header+6,sizeof count);
    if(memcmp(header,"AEV",4) || version!=0x104 || count!=17)return 0;
    const unsigned char* entry=entries+2*0x100;
    uintptr_t callback,argument;memcpy(&callback,entry+0x48,sizeof callback);memcpy(&argument,entry+0x40,sizeof argument);
    if(!(entry[0x38]&1) || entry[0x39]!=2 || entry[0x3a]!=2 || entry[0x3e]!=0x14
        || callback!=base+0x600e294 || argument || entry[0x50]!=2 || entry[0x51]!=1)return 0;
    // The explicit kind1 above makes SceExec skip pCTask lookup, including native idle(-1).
    // Native SceExec chooses a free task from 19 down to 8. Never enqueue twice.
    int free_task=0;
    for(unsigned i=0;i<12;i++) {
        const unsigned char* task=tasks+i*0x40;uintptr_t function;
        memcpy(&function,task+0x18,sizeof function);
        if(!task[0])free_task=1;
        else if(function==callback)return 0;
    }
    return free_task;
}
static void qa_user_transitions_tick(void) {
    if(!cinema_on_game_thread() || !g_ue4_base_cached)return;
    unsigned request=__atomic_exchange_n(&qa_transition_request,0,__ATOMIC_ACQ_REL);
    if(!request)return;
    const unsigned long base=g_ue4_base_cached;
    static const struct{unsigned long addr;unsigned a,b;} checks[]={
        {0x61d177c,0x90020ca8,0xf9446508},
        {0x638453c,0xb001fee8,0xf9469908},
        {0x6382924,0xf81d0ff5,0xa9014ff4},
        {0x61e5ca4,0xd10243ff,0xa9055ff8},
        {0x61e5e20,0xd10243ff,0xf90023f9},
        {0x6155664,0xb0021f68,0xf9402d08},
        {0x615bc6c,0xf81e0ff3,0xa9017bfd},
        {0x616107c,0xf81b0ff9,0xa9015ff8},
        {0x61627bc,0xb0021f09,0x2a0003e8},
        {0x600e294,0xd100c3ff,0xa9014ff4},
    };
    for(unsigned i=0;i<sizeof checks/sizeof *checks;i++) {
        unsigned actual[2];memcpy(actual,(const void*)(base+checks[i].addr),sizeof actual);
        if(actual[0]!=checks[i].a || actual[1]!=checks[i].b) {
            LOGF("QA transition rejected: preimage +%lx",checks[i].addr);return;
        }
    }
    unsigned type=cinema_type ? cinema_type():0;
    void* pawn=cinema_active ? cinema_active():0;
    if(request==2) {
        __atomic_store_n(&g_ui_dump_request,1,__ATOMIC_RELEASE);
        __atomic_store_n(&g_phone_request,3,__ATOMIC_RELEASE);
        LOGF("QA transition inspect type=%u pawn=%p scene=%p valid=%d closing=%d last=%d movie=%d",
            type,pawn,cinema_scene.pawn,cinema_scene.valid,cinema_scene.closing,cinema_last.valid,
            type==3 && pawn ? cinema_movie_probe(pawn,0)!=0:0);return;
    }
    if(request==13) {
        (void)qa_treasure_snapshot();
        return;
    }
    if(type!=1 || !pawn || !rl_player_valid || !rl_player_valid() || !rl_paused || rl_paused()) {
        LOGF("QA transition rejected: inactive/unready gameplay type=%u pawn=%p",type,pawn);return;
    }
    if(request==14 || request==15) {
        if(request==14)(void)qa_authored_merchant_reveal();
        else (void)qa_authored_house_entry();
        return;
    }
    if(request==5 || request==6) {
        int side=(int)request-5;
        LOGF("QA watch native Settings.WatchHand=%d accepted=%d",side,qa_watch_set_and_attach(side));
        return;
    }
    if(request==7 || request==8) {
        (void)qa_frame_pacing_test_cap(request==7 ? 0:60);
        return;
    }
    if(request==9 || request==10) {
        (void)qa_authored_door(request==9 ? 0x101:0x212);
        return;
    }
    if(request==11 || request==12) {
        if(request==11)(void)qa_authored_spinel_pickup();
        else (void)qa_original_merchant_entry();
        return;
    }
    int merc=((int(*)(void))(base+0x61d177c))();
    if(request==1) {
        if(!merc){LOGF("QA transition rejected: no Mercenaries match");return;}
        ((void(*)(void))(base+0x638453c))();
        LOGF("QA transition original MercenariesEndMatchWithResult");
    } else if(request==3) {
        if(merc){LOGF("QA transition rejected: chapter summary during Mercenaries");return;}
        ((void(*)(int,int))(base+0x61e5ca4))(0,0);
        ((void(*)(void))(base+0x61e5e20))();
        LOGF("QA transition original StartChapterSummary(0,0) and AdvanceToSummary (layout fixture)");
    } else if(request==4) {
        const unsigned char* game=*(const unsigned char* const*)(base+0xa3f5e48);
        const unsigned char* manager=*(const unsigned char* const*)(base+0xa542058);
        const void* task=*(void* const*)(base+0xa543150);
        unsigned replay=*(const unsigned char*)(base+0xa52d8d0);
        if(merc || (uintptr_t)game!=base+0xa501bd8 || (uintptr_t)manager!=base+0xa541b38
            || !qa_luis_fixture_ready(game,manager,base,replay,task,(const unsigned char*)(base+0xa5436e8))) {
            LOGF("QA transition Luis rejected: need fresh room106, native enabled AEV2, level20, no replay/active task game=%p manager=%p task=%p replay=%u",
                game,manager,task,replay);return;
        }
        ((void(*)(unsigned))(base+0x6155664))(2);
        LOGF("QA transition original SceAtExecute(2): R106S00 scheduled by native SceExec (full Luis route)");
    } else {
        if(merc){LOGF("QA transition rejected: room jump during Mercenaries");return;}
        ((void(*)(int))(base+0x6382924))((int)(request&0xffff));
        LOGF("QA transition original JumpToRoomNo room=%03x",request&0xffff);
    }
}
#endif
