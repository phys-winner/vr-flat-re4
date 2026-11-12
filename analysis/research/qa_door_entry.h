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
#ifndef RE4_QA_DOOR_ROOMSAVE
#define RE4_QA_DOOR_ROOMSAVE(base,owner,room) ((const unsigned char*(*)(const void*,unsigned short))((base)+0x614cdb0))((const void*)(owner),(unsigned short)(room))
#endif
static int qa_authored_door(unsigned destination) {
    unsigned source=destination==0x101?0x100:destination==0x212?0x211:destination==0x104?0x107:0;
    unsigned id=destination==0x212?1:0,count=destination==0x101?40:destination==0x212?12:7;
    static const unsigned poses[3][4]={
        {0xc74bb067,0x42b52440,0x46aff88c,0x4007cf34}, // Original R100 AEV0 -> R101.
        {0x457dc000,0x00000000,0x44f78000,0xbfc90fdc}, // Original R211 AEV1 -> R212.
        {0x46d7a7e6,0xc290170c,0xc6d89e82,0xbf91c8a2}, // Original R107 AEV0 -> R104.
    };
    const unsigned* pose=poses[destination==0x104?2:destination==0x212];
    const char* reason="context";int invoked=0,accepted=0;unsigned actual_source=0;
    const unsigned char* save=0;int room_save9_before=-1,room_save9_after=-1;
    int global_key9_before=-1,global_key9_after=-1;unsigned global_key_word_before=0,global_key_word_after=0;
    int save_code_ok=-1,save_owner_ok=-1,door_params_ok=-1,save_table_ok=-1,save_result_ok=-1;
    unsigned getter_words[2]={0},door_parameters=0;unsigned short door_condition=0;
    uintptr_t save_owner=0,saves=0,door_callback=0,door_arg=0,catalog=0;
    unsigned save_index=0,save_total=0;
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
    if(destination==0x104) {
        // R104's authored completion is EBio4FlagType::Key(1), flag9.
        // RoomSaveFlag is the property name; its default namespace is GLOBAL Key.
        memcpy(&global_key_word_before,game+0x5378,4);global_key_word_after=global_key_word_before;
        global_key9_before=global_key9_after=!!(global_key_word_before&0x00400000u);
        reason="global Key9 already fulfilled";if(global_key9_before)goto done;
        static const unsigned getter[2]={0x53083c28,0x7100251f};
        memcpy(getter_words,(const void*)(base+0x614cdb0),8);
        memcpy(&save_owner,(const void*)(base+0xa361d50),8);
        memcpy(&door_condition,entry+0x92,2);memcpy(&door_callback,entry+0x98,8);
        memcpy(&door_parameters,entry+0xa0,4);memcpy(&door_arg,entry+0xa8,8);
        // Original ConvertAEV leaves 0x94..97, 0xa4..a7 and 0xb0..ff untouched.
        // Keep every Door-consumed condition/callback/transition field exact.
        save_code_ok=!memcmp(getter_words,getter,8);
        save_owner_ok=save_owner>=4096 && !(save_owner&7) && save_owner<=UINTPTR_MAX-0x38;
        door_params_ok=!door_condition&&!door_callback&&!door_parameters&&!door_arg;
        reason="room save getter preimage";if(!save_code_ok)goto done;
        reason="room save owner";if(!save_owner_ok)goto done;
        reason="authored door parameters";if(!door_params_ok)goto done;
        memcpy(&saves,(const void*)(save_owner+0x30),8);
        memcpy(&catalog,(const void*)(base+0xa364288),8);
        save_table_ok=saves>=4096 && !(saves&3) && catalog>=4096 && !(catalog&7) && catalog<=UINTPTR_MAX-160;
        reason="room save table";if(!save_table_ok)goto done;
        // Native getRoomSavePtr counts active (first byte == 1) 24-byte rows in
        // the ten live 16-byte group descriptors; R104 is group1,row4.
        int found=0;
        for(unsigned group=0;group<10;group++) {
            uintptr_t rows;unsigned short rows_count;
            memcpy(&rows,(const void*)(catalog+16*group),8);memcpy(&rows_count,(const void*)(catalog+16*group+8),2);
            if(rows_count>255 || (rows_count && (rows<4096 || rows>UINTPTR_MAX-24u*rows_count))){save_table_ok=0;goto done;}
            for(unsigned row=0;row<rows_count;row++)if(*(const unsigned char*)(rows+24*row)==1) {
                if(group==1 && row==4){save_index=save_total;found=1;}
                save_total++;
            }
        }
        save_table_ok=found && saves<=UINTPTR_MAX-save_total*216u;
        if(!save_table_ok)goto done;
        save=RE4_QA_DOOR_ROOMSAVE(base,save_owner,0x104);
        save_result_ok=(uintptr_t)save==saves+save_index*216u;
        reason="room save result";if(!save_result_ok)goto done;
        // This different namespace is observed for preservation only; it is not
        // the emblem gate's fulfillment predicate.
        unsigned flags;memcpy(&flags,save+4,4);room_save9_before=!!(flags&0x00400000u);
        room_save9_after=room_save9_before;
    }
    invoked=1;RE4_QA_DOOR_EXECUTE(base,id);
    // A native lock/Ashley refusal remains a refusal; no retry or state modification.
    unsigned short next,previous;memcpy(&next,game+0x2c,2);memcpy(&previous,game+0x503c,2);memcpy(&mode,game+0x24,4);
    accepted=mode==4 && next==destination && previous==source && !memcmp(game+0x30,pose,16);
    reason=accepted?"native transition requested":"native guard/deferred";
    if(save) {
        unsigned flags;memcpy(&flags,save+4,4);room_save9_after=!!(flags&0x00400000u);
        if(room_save9_after!=room_save9_before){accepted=0;reason="native RoomSave9 changed";}
    }
    if(global_key9_before>=0) {
        memcpy(&global_key_word_after,game+0x5378,4);global_key9_after=!!(global_key_word_after&0x00400000u);
        if(global_key_word_after!=global_key_word_before){accepted=0;reason="native global Key flags changed";}
    }
done:
    LOGF("QA authored door source=%03x expected=%03x id=%u destination=%03x invoked=%d accepted=%d global_key9=%d/%d global_key_word=%08x/%08x room_save9=%d/%d code=%d:%08x/%08x owner=%d:%llx params=%d:%04x/%llx/%08x/%llx table=%d:%llx catalog=%llx index=%u/%u result=%d:%llx reason=%s",
        actual_source,source,id,destination,invoked,accepted,global_key9_before,global_key9_after,
        global_key_word_before,global_key_word_after,room_save9_before,room_save9_after,
        save_code_ok,getter_words[0],getter_words[1],save_owner_ok,(unsigned long long)save_owner,
        door_params_ok,(unsigned)door_condition,(unsigned long long)door_callback,door_parameters,(unsigned long long)door_arg,
        save_table_ok,(unsigned long long)saves,(unsigned long long)catalog,save_index,save_total,save_result_ok,(unsigned long long)(uintptr_t)save,reason);
    return accepted;
}
#endif
#endif
