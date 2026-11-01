// QA only: dispatch the real enabled authored doorway, including its native guards.
#ifdef RE4_QA_USER_TRANSITIONS
#ifndef RE4_QA_DOOR_ENTRY_H
#define RE4_QA_DOOR_ENTRY_H
#include <stdint.h>
#include <string.h>
#ifndef RE4_QA_DOOR_EXECUTE
#define RE4_QA_DOOR_EXECUTE(base,id) ((void(*)(unsigned))((base)+0x6155664))(id)
#endif
#ifndef RE4_QA_DOOR_PTR
#define RE4_QA_DOOR_PTR(base,id) ((const void*(*)(unsigned))((base)+0x6153ec0))(id)
#endif
static int qa_authored_door(unsigned destination) {
    unsigned source=destination==0x101?0x100:destination==0x212?0x211:0;
    unsigned id=destination==0x101?0:1,count=destination==0x101?40:12;
    static const unsigned poses[2][4]={
        {0xc74bb067,0x42b52440,0x46aff88c,0x4007cf34}, // Original R100 AEV0 -> R101.
        {0x457dc000,0x00000000,0x44f78000,0xbfc90fdc}, // Original R211 AEV1 -> R212.
    };
    const unsigned* pose=poses[destination==0x212];
    const char* reason="context";int invoked=0,accepted=0;unsigned actual_source=0;
    uintptr_t base=g_ue4_base_cached;
    if(!source || !base || !cinema_on_game_thread() || !cinema_type || !cinema_active
        || !rl_player_valid || !rl_paused || cinema_type()!=1 || !rl_player_valid() || rl_paused())goto done;
    void* pawn=cinema_active();
    if(!pawn || !pv_alive(pawn))goto done;
    static const unsigned execute[2]={0xb0021f68,0xf9402d08};
    static const unsigned door[2]={0xf81d0ff5,0xa9014ff4};
    static const unsigned get[2]={0xf0021f68,0xf9402d08};
    reason="preimage";
    if(memcmp((const void*)(base+0x6155664),execute,8) || memcmp((const void*)(base+0x61532c4),door,8)
        || memcmp((const void*)(base+0x6153ec0),get,8)
        || *(const uintptr_t*)(base+0x9cee030)!=base+0x61532c4)goto done;
    const unsigned char* game=*(const unsigned char* const*)(base+0xa3f5e48);
    const unsigned char* manager=*(const unsigned char* const*)(base+0xa542058);
    const unsigned char* player=*(const unsigned char* const*)(base+0xa4e9b40);
    reason="native state";
    if((uintptr_t)game!=base+0xa501bd8 || (uintptr_t)manager!=base+0xa541b38 || !player || !player[0x3dd])goto done;
    unsigned short room;unsigned mode;memcpy(&room,game+0x5038,2);actual_source=room;memcpy(&mode,game+0x24,4);
    if(room!=source || mode!=3)goto done;
    const unsigned char *header,*entries;memcpy(&header,manager,8);memcpy(&entries,manager+8,8);
    reason="AEV header";
    if((uintptr_t)header<4096 || ((uintptr_t)header&3) || (uintptr_t)header>UINTPTR_MAX-16-count*0x100u
        || (uintptr_t)entries!=(uintptr_t)header+16)goto done;
    unsigned short version,actual_count;memcpy(&version,header+4,2);memcpy(&actual_count,header+6,2);
    if(memcmp(header,"AEV",4) || version!=0x104 || actual_count!=count)goto done;
    const unsigned char* entry=entries+id*0x100u;
    reason="authored door";
    if(!(entry[0x38]&1) || entry[0x39]!=1 || entry[0x3a]!=id || entry[0x90]!=(destination>>8)
        || entry[0x91]!=(destination&255) || memcmp(entry+0x80,pose,16))goto done;
    // Native lookup handles its mixed AEV/ITA list without assuming one record stride.
    reason="registered door";
    if(RE4_QA_DOOR_PTR(base,id)!=entry)goto done;
    invoked=1;RE4_QA_DOOR_EXECUTE(base,id);
    // A native lock/Ashley refusal remains a refusal; no retry or state modification.
    unsigned short next,previous;memcpy(&next,game+0x2c,2);memcpy(&previous,game+0x503c,2);memcpy(&mode,game+0x24,4);
    accepted=mode==4 && next==destination && previous==source && !memcmp(game+0x30,pose,16);
    reason=accepted?"native transition requested":"native guard/deferred";
done:
    LOGF("QA authored door source=%03x expected=%03x id=%u destination=%03x invoked=%d accepted=%d reason=%s",
        actual_source,source,id,destination,invoked,accepted,reason);
    return accepted;
}
#endif
#endif
