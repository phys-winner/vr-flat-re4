// Preserve the original hold-to-push action; native cPlPush still moves and collides the object.
#ifndef RE4FIX_PUSH_OBJECTS_H
#define RE4FIX_PUSH_OBJECTS_H
#include <stdint.h>
#include <string.h>

static void* const* push_objects_player_slot;
static const unsigned char* push_objects_em_manager;
static struct cinema_ui_weak push_objects_pawn={-1,0};
static void* push_objects_player;
static unsigned push_objects_face;
static uint64_t push_objects_snapshot;

#ifdef RE4_QA_USER_TRANSITIONS
static unsigned touch_push_objects_input_mask(long now);
static int push_objects_diag_ready;
static int push_objects_diag_pending=-1;
static uint64_t push_objects_diag_until,push_objects_diag_next;
static const unsigned char* push_objects_diag_keys;
static void* const* push_objects_diag_game;
static int touch_push_objects_diag_request(long ms) {
    if(ms<0 || ms>15000)return 0;
    __atomic_store_n(&push_objects_diag_pending,(int)ms,__ATOMIC_RELEASE);return 1;
}
static int push_objects_diag_slot(uintptr_t address,uintptr_t data,unsigned count,unsigned stride) {
    return address>=data && (address-data)%stride==0 && (address-data)/stride<count
        ? (int)((address-data)/stride) : -1;
}
static int push_objects_diag_live(const unsigned char* rack) {
    unsigned flags;short life;memcpy(&flags,rack+8,4);memcpy(&life,rack+0x3e8,2);
    return (flags&0x201u)==1 && rack[0x118]==0x55 && life>0;
}
static void push_objects_diag_rack(const unsigned char* rack,unsigned slot,int selected) {
    float pose[3],yaw,range[4],frame[12];unsigned flags;
    memcpy(pose,rack+0xa4,12);memcpy(&yaw,rack+0xb4,4);memcpy(range,rack+0x10fc,16);
    memcpy(frame,rack+0x109c,48);memcpy(&flags,rack+8,4);
    LOGF("PUSHRACK slot=%u target=%d ptr=%p ets=%u subtype=%u flags=%08x p=%.3f,%.3f,%.3f yaw=%.5f motion=%p rangeflags=%02x bounds=%.3f,%.3f,%.3f,%.3f frame_p=%.3f,%.3f,%.3f frame_xz=%.5f,%.5f,%.5f,%.5f",
        slot,selected,rack,rack[0x61e],rack[0x119],flags,pose[0],pose[1],pose[2],yaw,
        *(void* const*)(rack+0x228),rack[0x110c],range[0],range[1],range[2],range[3],
        frame[3],frame[7],frame[11],frame[0],frame[2],frame[8],frame[10]);
}
// Explicit QA request only. Native object addresses are used for this GT sample, never retained.
static void touch_push_objects_diag_tick(long now) {
    if(now<=0 || !push_objects_diag_ready)return;
    int request=__atomic_exchange_n(&push_objects_diag_pending,-1,__ATOMIC_ACQ_REL);
    if(request>=0) {push_objects_diag_until=request?(uint64_t)now+(unsigned)request:0;push_objects_diag_next=0;}
    if(!push_objects_diag_until || (uint64_t)now>=push_objects_diag_until || (uint64_t)now<push_objects_diag_next)return;
    push_objects_diag_next=(uint64_t)now+200;
    const unsigned char* player=push_objects_player_slot ? *push_objects_player_slot:0;
    uintptr_t data=0;unsigned count=0,stride=0;int pool=0;
    if(push_objects_em_manager) {
        memcpy(&data,push_objects_em_manager+8,sizeof data);memcpy(&count,push_objects_em_manager+0x10,4);memcpy(&stride,push_objects_em_manager+0x14,4);
        pool=data>=4096 && !(data&3) && count>=1 && count<=4096 && stride>=0x1110 && stride<=0x10000
            && !(stride&3) && data<=UINTPTR_MAX-(uintptr_t)count*stride;
    }
    const unsigned char* push=player?*(const unsigned char* const*)(player+0x9b8):0;
    const unsigned char* target=0;unsigned side=4;int selected=-1;
    if(push && *(const void* const*)push==player) {
        memcpy(&target,push+8,sizeof target);memcpy(&side,push+0x14,4);
        if(pool && side<=3)selected=push_objects_diag_slot((uintptr_t)target,data,count,stride);
        if(selected>=0 && !push_objects_diag_live(target))selected=-1;
    }
    float position[3]={0},yaw=0;uint64_t held=0;
    if(player) {memcpy(position,player+0xa4,12);memcpy(&yaw,player+0xb4,4);}
    if(push_objects_diag_keys)memcpy(&held,push_objects_diag_keys+0x18,8);
    void* game=push_objects_diag_game?*push_objects_diag_game:0;unsigned short room=0;
    if(game)memcpy(&room,(const char*)game+0x5038,2);
    uint64_t sample=__atomic_load_n(&push_objects_snapshot,__ATOMIC_ACQUIRE);long stamp=(long)(sample>>16);
    LOGF("PUSHSTATE ms=%ld room=%03x pl=%p action=%u stage=%u p=%.3f,%.3f,%.3f yaw=%.5f push=%p target=%p slot=%d side=%u held=%016llx cap=%03x age=%ld focus=%d context=%d cancel=%u pool=%p count=%u stride=%x pool_valid=%d",
        now,room,player,player?player[0x115]:0,player?player[0x116]:0,position[0],position[1],position[2],yaw,
        push,target,selected,side,(unsigned long long)held,touch_push_objects_input_mask(now),stamp>0?now-stamp:-1,
        g_app_input_focus,g_touch_context,(unsigned)g_touch_cancel,(void*)data,count,stride,pool);
    if(!pool)return;
    unsigned logged=0;
    if(selected>=0) {push_objects_diag_rack(target,(unsigned)selected,1);logged++;}
    for(unsigned i=0;i<count && logged<8;i++) {
        const unsigned char* rack=(const unsigned char*)(data+(uintptr_t)i*stride);
        if((int)i==selected || !push_objects_diag_live(rack))continue;
        push_objects_diag_rack(rack,i,0);logged++;
    }
}
#endif

// Input threads consume only this bounded capability, never legacy or UObject pointers.
static unsigned touch_push_objects_input_mask(long now) {
    uint64_t sample=__atomic_load_n(&push_objects_snapshot,__ATOMIC_ACQUIRE);
    long stamp=(long)(sample>>16);
    return __atomic_load_n(&g_app_input_focus,__ATOMIC_ACQUIRE)
        && __atomic_load_n(&g_touch_context,__ATOMIC_ACQUIRE)==1
        && !__atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE)
        && stamp>0 && now>=stamp && now-stamp<=250 ? (unsigned)sample&0x303u : 0;
}
static void touch_push_objects_clear(void) {
    __atomic_store_n(&push_objects_snapshot,0,__ATOMIC_RELEASE);
    push_objects_pawn=(struct cinema_ui_weak){-1,0};push_objects_player=0;push_objects_face=0;
}
static int push_objects_target_live(const void* player) {
    if(!player || !push_objects_em_manager)return 0;
    const unsigned char* push=*(const unsigned char* const*)((const char*)player+0x9b8);
    if(!push || *(void* const*)push!=player)return 0;
    unsigned side;memcpy(&side,push+0x14,4);if(side>3)return 0;
    const unsigned char* target=*(const unsigned char* const*)(push+8);
    uintptr_t data=(uintptr_t)*(const void* const*)(push_objects_em_manager+8);
    unsigned count,stride;
    memcpy(&count,push_objects_em_manager+0x10,4);memcpy(&stride,push_objects_em_manager+0x14,4);
    uintptr_t address=(uintptr_t)target;
    if(!data || !target || count<1 || count>4096 || stride<0x1110 || stride>0x10000
        || address<data || (address-data)%stride || (address-data)/stride>=count)return 0;
    unsigned flags;short life;memcpy(&flags,target+8,4);memcpy(&life,target+0x3e8,2);
    return (flags&0x201u)==1 && target[0x118]==0x55 && target[0x119]!=4 && life>0;
}
// Owning right GameThread hand only; original scene and action eligibility remain authoritative.
static void touch_push_objects_tick(void* hand,long now) {
    if(!cinema_on_game_thread())return;
    if(!push_objects_player_slot || !cu_weak_assign || !cu_weak_get || !ti_ready || !phone_active || !phone_type || !phone_player
        || !rl_paused || !rl_player_valid || now<=0 || !g_app_input_focus || g_touch_context!=1 || g_touch_cancel
        || phone_type()!=1 || rl_paused() || !rl_player_valid()) {touch_push_objects_clear();return;}
    void* pawn=phone_active();
    if(!pawn) {touch_push_objects_clear();return;}
    if(!interact_bound_hand(hand,pawn,1))return; // A pooled or left hand cannot replace the active sample.
#ifdef RE4_QA_USER_TRANSITIONS
    touch_push_objects_diag_tick(now);
#endif
    void* player=*push_objects_player_slot;
    if(!push_objects_target_live(player)) {touch_push_objects_clear();return;}
    const unsigned char* bytes=(const unsigned char*)player;
    unsigned state=bytes[0x116];
    if(bytes[0x115]==9 && state!=0 && state!=1 && state!=20 && state!=21) {
        touch_push_objects_clear();return;
    }
    unsigned action=71,command=interact_prompt(&action);
    int bit=interact_face_bit(command);
    if(action==6 && bit>=0) {
        cu_weak_assign(&push_objects_pawn,pawn);push_objects_player=player;push_objects_face=1u<<bit;
    } else {
        if(command || cu_weak_get(&push_objects_pawn)!=pawn || player!=push_objects_player
            || bytes[0x115]!=9 || (state!=0 && state!=1 && state!=20 && state!=21)) {
            touch_push_objects_clear();return;
        }
    }
    __atomic_store_n(&push_objects_snapshot,((uint64_t)(unsigned long)now<<16)|push_objects_face,__ATOMIC_RELEASE);
}

static void install_touch_push_objects(uintptr_t base) {
    static const struct {uintptr_t address;unsigned words[2];} checks[]={
        {0x5fb8444,{0xb0021d48,0xf946bd08}}, // Authored Push callback -> pPL, action9.
        {0x5fb8450,{0xf9400108,0xb9011509}},
        {0x5fad8a8,{0x39406508,0x36100128}}, // Held Bio4Key bit10 during lean.
        {0x5fad9fc,{0x39406508,0x361000c8}}, // Same held bit while pushing.
        {0x5fada64,{0xf944de60,0x9400a913}},
        {0x5fd707c,{0xd10283ff,0xa9046ffc}}, // Shared native cPlPush target selection.
        {0x5fd7324,{0xa9be4ff4,0xa9017bfd}},
        {0x5fd7380,{0xfc1b0fea,0x6d0123e9}},
        {0x62a54c0,{0x36000116,0x900205e9}}, // Original held UPlayerInput -> Bio4Key.
    };
    push_objects_player_slot=0;push_objects_em_manager=0;
#ifdef RE4_QA_USER_TRANSITIONS
    push_objects_diag_ready=0;push_objects_diag_keys=0;push_objects_diag_game=0;
#endif
    if(!base)return;
    for(unsigned i=0;i<sizeof checks/sizeof *checks;i++)
        if(memcmp((const void*)(base+checks[i].address),checks[i].words,8)) {
            LOGF("PUSH input disabled: original preimage mismatch +%lx",(unsigned long)checks[i].address);return;
        }
    if(*(const uintptr_t*)(base+0xa3f9c98+9*8)!=base+0x5fad7e0)return;
    const char* entry=(const char*)(base+0x9d4ecf0+6*16);
    const char* name=*(const char* const*)entry;
    if(*(const long long*)(entry+8)!=6 || !name || strcmp(name,"EActionButtonType::Push"))return;
    push_objects_player_slot=(void* const*)(base+0xa4e9b40);
    push_objects_em_manager=(const unsigned char*)(base+0xa4e9b68);
#ifdef RE4_QA_USER_TRANSITIONS
    static const struct {uintptr_t address;unsigned words[2];} diagnostic[]={
        {0x5ea9084,{0x2a0503f4,0x2a0403f5}}, // ETS id passed in w5.
        {0x5ea9114,{0x39187a74,0xb50000b9}}, // Same id -> rack+0x61e.
        {0x5ea9130,{0xf9400329,0xb900ae68}}, // Native Vec -> rack+0xa4.
        {0x5ea9d78,{0xbd10fe61,0xbd110262}}, // Original range storage.
    };
    int valid=1;
    for(unsigned i=0;i<sizeof diagnostic/sizeof *diagnostic;i++)
        if(memcmp((const void*)(base+diagnostic[i].address),diagnostic[i].words,8))valid=0;
    if(valid) {push_objects_diag_keys=(const unsigned char*)(base+0xa501aa0);push_objects_diag_game=(void* const*)(base+0xa3f5e48);push_objects_diag_ready=1;}
#endif
    LOGF("PUSH input ready: original action6/cPlPush held face; native motion, range and collision retained");
}
#endif
