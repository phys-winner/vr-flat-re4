// QA only: collect the two real R104 emblem pickups through their original API.
#ifdef RE4_QA_USER_TRANSITIONS
#ifndef RE4_QA_KEY_ITEM_USE_H
#define RE4_QA_KEY_ITEM_USE_H
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifndef RE4_QA_KEY_COUNT
#define RE4_QA_KEY_COUNT(base,item) ((int(*)(void*,int))((base)+0x5f33f14))((void*)((base)+0xa500780),(int)(item))
#endif
#ifndef RE4_QA_KEY_FIND_NAME
#define RE4_QA_KEY_FIND_NAME(base,name) ((void(*)(uint64_t*,const char*,int))((base)+0x67c7644))((name),"HexagonalEmblem",0)
#define RE4_QA_KEY_RECEPTACLE(base,name) ((void*(*)(uint64_t))((base)+0x62f27dc))(name)
#define RE4_QA_KEY_RECEPTACLE_CLASS(base) ((void*(*)(void))((base)+0x645d984))()
#define RE4_QA_KEY_SLOT_CLASS(base) ((void*(*)(void))((base)+0x645f684))()
#define RE4_QA_KEY_MENU_CLASS(base) ((void*(*)(void))((base)+0x6460494))()
#define RE4_QA_KEY_WORLD(base,actor) ((void*(*)(const void*))((base)+0x8702bc4))(actor)
#define RE4_QA_KEY_SCENE_CLASS(base) ((void)(base),mv_scene_class?mv_scene_class():0)
#define RE4_QA_KEY_PAWN_CLASS(base) ((void)(base),comfort_player_class?comfort_player_class():0)
#define RE4_QA_KEY_PAWN(base) ((void)(base),cinema_active())
#endif
#ifndef RE4_QA_KEY_FIXTURE_GET
#define RE4_QA_KEY_FIXTURE_GET(base,item) ((int(*)(void*,unsigned short,unsigned short))((base)+0x5f2886c))((void*)((base)+0xa500780),(unsigned short)(item),1)
#define RE4_QA_KEY_FIXTURE_ERASE(base,item) ((void(*)(void*,void*))((base)+0x5f33a1c))((void*)((base)+0xa500780),(item))
#define RE4_QA_KEY_FIXTURE_PROP(base,item,out) ((void(*)(unsigned short,void*))((base)+0x5f32148))((unsigned short)(item),(out))
#define RE4_QA_KEY_FIXTURE_RECIPE(base,first,second,out) ((int(*)(unsigned short,unsigned short,unsigned short*))((base)+0x5f34198))((unsigned short)(first),(unsigned short)(second),(out))
#define RE4_QA_KEY_FIXTURE_INVENTORY(base) ((void*(*)(void))((base)+0x62199c8))()
#define RE4_QA_KEY_FIXTURE_INVENTORY_CLASS(base) ((void*(*)(void))((base)+0x645023c))()
#define RE4_QA_KEY_FIXTURE_PAWN_CLASS(base) ((void*(*)(void))((base)+0x63c8a5c))()
#endif
#ifndef RE4_QA_KEY_ROOM_SAVE
#define RE4_QA_KEY_ROOM_SAVE(base,manager) ((const unsigned char*(*)(const void*,unsigned short))((base)+0x614cdb0))((manager),0x104)
#define RE4_QA_KEY_SAVE_OPEN(base) ((void(*)(unsigned,unsigned))((base)+0x5f1fe34))(0xff,1)
#endif
static int qa_key_item_preimages(uintptr_t base) {
    static const struct {uintptr_t at;unsigned words[2];} checks[]={
        {0x62ec678,{0xd10103ff,0xa90157f6}},{0x62e9b5c,{0xd10283ff,0xa90757f6}},
        {0x62e9b7c,{0x97f9a8d1,0xb4000a00}},{0x645ace8,{0xd10183ff,0xf90023f3}},
        {0x6153ec0,{0xf0021f68,0xf9402d08}},
        {0x6150fc0,{0x3940e90a,0x5219014a}},{0x6151008,{0x39400109,0x52190129}},
    };
    for(unsigned i=0;i<sizeof checks/sizeof *checks;i++)
        if(memcmp((const void*)(base+checks[i].at),checks[i].words,8))return 0;
    return 1;
}
static int qa_authored_emblem_pickup(unsigned item) {
    const char* reason="unsupported item";int invoked=0,accepted=0,before=-1,after=-1;
    /* Original SceAtInit XORs each on-disk ITA id with 0x80 before spawning. */
    unsigned index=item==0xa4?15:16,id=item==0xa4?0x8f:0x90,save_id=item==0xa4?16:17;
    if(item!=0xa4 && item!=0xa5)goto done;
    reason="context";
    const unsigned char* game=qa_treasure_context(1,0x104);uintptr_t base=g_ue4_base_cached;
    if(!game)goto done;
    reason="preimage";if(!qa_key_item_preimages(base))goto done;
    const unsigned char* manager=*(const unsigned char* const*)(base+0xa542058);
    reason="ITA manager";if((uintptr_t)manager!=base+0xa541b38)goto done;
    const unsigned char *header,*entries;memcpy(&header,manager+0x10,8);memcpy(&entries,manager+0x18,8);
    reason="ITA header";
    if((uintptr_t)header<4096 || ((uintptr_t)header&3) || (uintptr_t)header>UINTPTR_MAX-16-27u*0xd0
        || (uintptr_t)entries!=(uintptr_t)header+16)goto done;
    unsigned short version,count;memcpy(&version,header+4,2);memcpy(&count,header+6,2);
    if(memcmp(header,"ITA",4) || version!=0x105 || count!=27)goto done;
    const unsigned char* entry=entries+index*0xd0;
    unsigned short actual,quantity,save;memcpy(&actual,entry+0xa4,2);memcpy(&quantity,entry+0xa8,2);memcpy(&save,entry+0xaa,2);
    LOGF("QA KEY ITA index=%u expected_item=%04x flags=%02x kind=%u id=%02x item=%04x quantity=%u save_id=%u model=%p read_only",
        index,item,entry[0x38],entry[0x39],entry[0x3a],actual,quantity,save,*(void* const*)(entry+0x90));
    reason="authored emblem";
    if((entry[0x38]&3)!=3 || entry[0x39]!=3 || entry[0x3a]!=id || actual!=item
        || save!=save_id || quantity>1 || !*(const uintptr_t*)(entry+0x90))goto done;
    reason="live owned pickup";
    void* actor=RE4_QA_TREASURE_FIND(base,id);
    if(!actor || !pv_alive(actor) || !menu_visual_layout(actor,RE4_QA_TREASURE_CLASS(base),0x4e0)
        || *(const void* const*)((const char*)actor+0x3f8)!=entry
        || *(const unsigned short*)((const char*)actor+0x4c0)!=item
        || !((const unsigned char*)actor)[0x4c2] || !((const unsigned char*)actor)[0x4c3]
        || ((const unsigned char*)actor)[0x4dc])goto done;
    before=RE4_QA_KEY_COUNT(base,item);
    reason="already owned/invalid count";if(before!=0)goto done;
    invoked=1;RE4_QA_TREASURE_COLLECT(base,actor);after=RE4_QA_KEY_COUNT(base,item);
    accepted=after==1;reason=accepted?"original authored pickup collected":"native collection refused";
done:
    LOGF("QA KEY authored R104 item=%04x native_id=%02x save_id=%u invoked=%d accepted=%d count=%d->%d reason=%s",
        item,id,save_id,invoked,accepted,before,after,reason);
    return accepted;
}
static int qa_key_span(const void* data,int count,int capacity,unsigned stride) {
    uintptr_t address=(uintptr_t)data;
    return count>=0 && count<=16 && capacity>=count && capacity<=64
        && (!count || (address>=4096 && address<=UINTPTR_MAX-(unsigned)count*stride));
}
static int qa_key_room_save_flag(uintptr_t base,int* flag) {
    static const unsigned getter[2]={0x53083c28,0x7100251f};
    if(memcmp((const void*)(base+0x614cdb0),getter,8))return 0;
    const unsigned char* manager=*(const unsigned char* const*)(base+0xa361d50);
    const unsigned char* table=*(const unsigned char* const*)(base+0xa364288);
    if((uintptr_t)manager<4096 || ((uintptr_t)manager&7) || (uintptr_t)manager>UINTPTR_MAX-0x38
        || (uintptr_t)table<4096 || ((uintptr_t)table&7) || (uintptr_t)table>UINTPTR_MAX-160)return 0;
    uintptr_t saves;memcpy(&saves,manager+0x30,8);
    if(saves<4096 || (saves&3))return 0;
    unsigned total=0,index=0;int found=0;
    for(unsigned group=0;group<10;group++) {
        const unsigned char* entries;unsigned short count;
        memcpy(&entries,table+16*group,8);memcpy(&count,table+16*group+8,2);
        if(count>255 || (count && ((uintptr_t)entries<4096 || (uintptr_t)entries>UINTPTR_MAX-24u*count)))return 0;
        for(unsigned row=0;row<count;row++)if(entries[row*24]==1) {
            if(group==1 && row==4){index=total;found=1;}
            total++;
        }
    }
    if(!found || total>2550 || saves>UINTPTR_MAX-total*216u)return 0;
    const unsigned char* save=RE4_QA_KEY_ROOM_SAVE(base,manager);
    if((uintptr_t)save!=saves+index*216u)return 0;
    unsigned flags;memcpy(&flags,save+4,4);*flag=!!(flags&0x00400000u);
    LOGF("QA KEY RoomSave room=104 records=%u dynamic_index=%u record=%p flag9=%d read_only",total,index,save,*flag);
    return 1;
}
static int qa_key_global_key_flag(uintptr_t base,int* flag) {
    static const struct {uintptr_t at;unsigned words[2];} checks[]={
        {0x61e1f70,{0xa9be4ff4,0xa9017bfd}},{0x62f2678,{0xf81a0ffb,0xa90167fa}},
    };
    if(!base || !flag)return 0;
    for(unsigned i=0;i<sizeof checks/sizeof *checks;i++)
        if(memcmp((const void*)(base+checks[i].at),checks[i].words,8))return 0;
    // Key1 is GLOBAL Game+5378. RoomScenario2 uses a different RoomSave array.
    if(*(const uintptr_t*)(base+0xa361d30)!=base+0xa3f5e48)return 0;
    const unsigned char* game=*(const unsigned char* const*)(base+0xa3f5e48);
    if((uintptr_t)game!=base+0xa501bd8)return 0;
    unsigned key_word;memcpy(&key_word,game+0x5378,4);*flag=!!(key_word&0x00400000u);
    LOGF("QA KEY GlobalKey id=9 game=%p word5378=%08x key9=%d read_only",game,key_word,*flag);
    return 1;
}
static int qa_key_root(uintptr_t base,const void* actor,float position[3]) {
    void* root=*(void* const*)((const char*)actor+0x130);
    if(!pv_alive(root) || !menu_visual_layout(root,RE4_QA_KEY_SCENE_CLASS(base),0x1f0)
        || *(void* const*)((const char*)root+0x20)!=actor
        || *(void* const*)((const char*)root+0xa0)!=actor)return 0;
    memcpy(position,(const char*)root+0x1d0,12);
    for(unsigned i=0;i<3;i++)if(!isfinite(position[i]) || fabsf(position[i])>10000000)return 0;
    return 1;
}
static int qa_key_receptacle_snapshot(uintptr_t base) {
    const char* reason="active gameplay pawn";
    if(cinema_type()!=1)goto rejected;
    static const struct {uintptr_t at;unsigned words[2];} checks[]={
        {0x62f27dc,{0xd10283ff,0xa90757f6}},{0x67c7644,{0xf81d0ff5,0xa9014ff4}},
        {0x645d984,{0xd10183ff,0xf90023f3}},{0x645f684,{0xd10183ff,0xf90023f3}},
        {0x6460494,{0xd10183ff,0xf90023f3}},{0x8702bc4,{0xf81e0ff3,0xa9017bfd}},
    };
    reason="native lookup preimages";
    for(unsigned i=0;i<sizeof checks/sizeof *checks;i++)
        if(memcmp((const void*)(base+checks[i].at),checks[i].words,8))goto rejected;
    void* pawn=RE4_QA_KEY_PAWN(base);
    reason="live typed pawn";
    if(!pv_alive(pawn) || !menu_visual_layout(pawn,RE4_QA_KEY_PAWN_CLASS(base),0x570))goto rejected;
    void* world=RE4_QA_KEY_WORLD(base,pawn);
    reason="live native world";if(!pv_alive(world))goto rejected;
    uint64_t name=0;RE4_QA_KEY_FIND_NAME(base,&name);
    reason="existing receptacle name";if(!name)goto rejected;
    const unsigned char* actor=RE4_QA_KEY_RECEPTACLE(base,name);
    reason="live typed same-world receptacle";
    if(!pv_alive(actor) || !menu_visual_layout(actor,RE4_QA_KEY_RECEPTACLE_CLASS(base),0x41c)
        || RE4_QA_KEY_WORLD(base,actor)!=world)goto rejected;
    uint64_t authored_name;memcpy(&authored_name,actor+0x244,8);
    if(authored_name!=name)goto rejected;
    float position[3],player_position[3];
    reason="owned finite scene roots";
    if(!qa_key_root(base,actor,position) || !qa_key_root(base,pawn,player_position))goto rejected;
    struct {const void** data;int count,capacity;} slots;memcpy(&slots,actor+0x3c0,sizeof slots);
    reason="bounded two-slot array";
    if(slots.count!=2 || !qa_key_span(slots.data,slots.count,slots.capacity,8) || ((uintptr_t)slots.data&7))goto rejected;
    LOGF("QA KEY receptacle=%p world=%p pawn=%p p=(%.3f,%.3f,%.3f) pawn_p=(%.3f,%.3f,%.3f) enabled=%u state=%u fulfilled=%u menu=%p menu_item=%p placed=%p read_only",
        actor,world,pawn,position[0],position[1],position[2],player_position[0],player_position[1],player_position[2],
        actor[0x416],actor[0x3e0],actor[0x41a],*(void* const*)(actor+0x3d8),*(void* const*)(actor+0x3d0),*(void* const*)(actor+0x3b8));
    unsigned halves=0;
    for(int i=0;i<slots.count;i++) {
        const unsigned char* slot=slots.data[i];reason="live typed owned slot";
        if(!pv_alive(slot) || !menu_visual_layout(slot,RE4_QA_KEY_SLOT_CLASS(base),0x4f8)
            || *(const void* const*)(slot+0x20)!=actor || *(const void* const*)(slot+0xa0)!=actor
            || *(const void* const*)(slot+0x4f0)!=actor)goto rejected;
        struct {const unsigned char* data;int count,capacity;} allowed;memcpy(&allowed,slot+0x4b0,sizeof allowed);
        reason="bounded slot allowed items";
        if(!qa_key_span(allowed.data,allowed.count,allowed.capacity,1))goto rejected;
        unsigned required=slot[0x4c0],bit=required==0xa4?1:required==0xa5?2:0;
        reason="two distinct original halves";if(!bit || (halves&bit))goto rejected;halves|=bit;
        const void* item=*(const void* const*)(slot+0x4e8);
        int live_item=pv_alive(item) && menu_visual_layout(item,RE4_QA_TREASURE_CLASS(base),0x4e0);
        float slot_position[3];memcpy(slot_position,slot+0x1d0,12);
        reason="finite slot location";
        for(unsigned k=0;k<3;k++)if(!isfinite(slot_position[k]) || fabsf(slot_position[k])>10000000)goto rejected;
        LOGF("QA KEY slot=%d component=%p required=%02x can_receive=%u item=%p live_typed_item=%d item_id=%04x p=(%.3f,%.3f,%.3f) allowed_count=%d read_only",
            i,slot,required,slot[0x4c2],item,live_item,live_item?*(const unsigned short*)((const char*)item+0x4c0):0xffff,
            slot_position[0],slot_position[1],slot_position[2],allowed.count);
        for(int k=0;k<allowed.count;k++)LOGF("QA KEY slot=%d allowed[%d]=%02x read_only",i,k,allowed.data[k]);
    }
    const unsigned char* menu=*(const unsigned char* const*)(actor+0x3d8);
    if(menu) {
        reason="live typed same-world owned key menu";
        if(!pv_alive(menu) || !menu_visual_layout(menu,RE4_QA_KEY_MENU_CLASS(base),0x2a0)
            || RE4_QA_KEY_WORLD(base,menu)!=world || *(const void* const*)(menu+0x298)!=actor)goto rejected;
        float menu_position[3];reason="owned finite menu root";if(!qa_key_root(base,menu,menu_position))goto rejected;
        struct {const unsigned char* data;int count,capacity;} items;memcpy(&items,menu+0x250,sizeof items);
        struct {const void** data;int count,capacity;} buttons;memcpy(&buttons,menu+0x268,sizeof buttons);
        reason="bounded menu item/button arrays";
        if(!qa_key_span(items.data,items.count,items.capacity,1) || !qa_key_span(buttons.data,buttons.count,buttons.capacity,8)
            || (buttons.count && ((uintptr_t)buttons.data&7)))goto rejected;
        LOGF("QA KEY menu=%p owner=%p p=(%.3f,%.3f,%.3f) items=%d buttons=%d recently_used=%02x read_only",
            menu,actor,menu_position[0],menu_position[1],menu_position[2],items.count,buttons.count,menu[0x27a]);
        for(int i=0;i<items.count;i++)LOGF("QA KEY menu item[%d]=%02x read_only",i,items.data[i]);
    }
    return 1;
rejected:
    LOGF("QA KEY receptacle snapshot rejected=%s read_only",reason);return 0;
}
/* Deliberate ownership fixture only. It does not open either authored chest. */
static int qa_key_inventory_fixture(void) {
    uintptr_t base=g_ue4_base_cached;const char* reason="fresh active native R104 context";
    int invoked=0,accepted=0,rollback_ok=1,key9=-1;void* added[2]={0,0};
    if(!qa_treasure_context(1,0x104))goto done;
    reason="original global Key9 must be unset";
    if(!qa_key_global_key_flag(base,&key9) || key9)goto done;
    static const struct {uintptr_t at;unsigned words[2];} checks[]={
        {0x5f2886c,{0xd10183ff,0xf9000bf9}},{0x5f33a1c,{0xd10183ff,0xa90167fa}},
        {0x5f32148,{0x12003c08,0x7104451f}},{0x5f34198,{0x90ff1e88,0xaa1f03e9}},
        {0x62199c8,{0xf81e0ff3,0xa9017bfd}},{0x645023c,{0xd10183ff,0xf90023f3}},
        {0x63c8a5c,{0xd10183ff,0xf90023f3}},
    };
    reason="native fixture preimages";
    for(unsigned i=0;i<sizeof checks/sizeof *checks;i++)
        if(memcmp((const void*)(base+checks[i].at),checks[i].words,8))goto done;
    void* pawn=RE4_QA_KEY_PAWN(base);void* inventory=RE4_QA_KEY_FIXTURE_INVENTORY(base);
    reason="live typed pawn-owned initialized inventory";
    if(!pv_alive(pawn) || !menu_visual_layout(pawn,RE4_QA_KEY_FIXTURE_PAWN_CLASS(base),0x1608)
        || !pv_alive(inventory) || !menu_visual_layout(inventory,RE4_QA_KEY_FIXTURE_INVENTORY_CLASS(base),0xf8)
        || *(void* const*)((const char*)pawn+0x1600)!=inventory || !((const unsigned char*)inventory)[0xf0])goto done;
    reason="must own no emblem half or combined key";
    if(RE4_QA_KEY_COUNT(base,0xa4) || RE4_QA_KEY_COUNT(base,0xa5) || RE4_QA_KEY_COUNT(base,0xa6))goto done;
    for(unsigned item=0xa4;item<=0xa6;item++) {
        unsigned char property[8]={0};RE4_QA_KEY_FIXTURE_PROP(base,item,property);
        unsigned short maximum;memcpy(&maximum,property+4,2);
        reason="original single-quantity key enum";if(property[2]!=7 || property[3]!=1 || maximum!=1)goto done;
    }
    unsigned short combined=0xffff;reason="original emblem recipe";
    if(!RE4_QA_KEY_FIXTURE_RECIPE(base,0xa4,0xa5,&combined) || combined!=0xa6)goto done;
    combined=0xffff;
    if(!RE4_QA_KEY_FIXTURE_RECIPE(base,0xa5,0xa4,&combined) || combined!=0xa6)goto done;
    const unsigned char* manager=(const unsigned char*)(base+0xa500780);
    const unsigned char* data;int capacity;
    memcpy(&data,manager+0x20,8);memcpy(&capacity,manager+0x30,4);
    reason="native bounded Leon inventory capacity";
    if((uintptr_t)data<4096 || ((uintptr_t)data&1) || capacity<2 || capacity>4096
        || (uintptr_t)data>UINTPTR_MAX-(unsigned)capacity*14u || manager[0x1b])goto done;
    void* free_items[2]={0,0};unsigned found=0;
    for(int i=0;i<capacity && found<2;i++)if(!(data[i*14+4]&1))free_items[found++]=(void*)(data+i*14);
    reason="two originally free native slots";if(found!=2)goto done;
    reason="native original matching R104 receptacle";if(!qa_key_receptacle_snapshot(base))goto done;
    for(unsigned i=0;i<2;i++) {
        unsigned item=0xa4+i;invoked++;
        int result=RE4_QA_KEY_FIXTURE_GET(base,item);void* got=*(void* const*)(manager+0x28);
        reason="native inventory storage changed";
        if(*(const void* const*)(manager+0x20)!=data || *(const int*)(manager+0x30)!=capacity)goto rollback;
        const unsigned char* candidate=free_items[i];unsigned short actual,quantity;
        memcpy(&actual,candidate,2);memcpy(&quantity,candidate+2,2);
        if(actual==item && quantity==1 && (candidate[4]&1) && candidate[5]==0)added[i]=free_items[i];
        reason="native get/pointer/count postcondition";
        if(!result || got!=free_items[i] || !added[i] || RE4_QA_KEY_COUNT(base,0xa4)!=1
            || RE4_QA_KEY_COUNT(base,0xa5)!=(int)i || RE4_QA_KEY_COUNT(base,0xa6)!=0)goto rollback;
    }
    accepted=1;reason="explicit QA inventory ownership prepared; chests/progress untouched";goto done;
rollback:
    for(int i=1;i>=0;i--)if(added[i]) {
        const unsigned char* item=added[i];unsigned short actual;memcpy(&actual,item,2);
        if(*(const void* const*)(manager+0x20)!=data || *(const int*)(manager+0x30)!=capacity
            || actual!=0xa4u+(unsigned)i || !(item[4]&1) || item[5]) {rollback_ok=0;continue;}
        RE4_QA_KEY_FIXTURE_ERASE(base,added[i]);
    }
    rollback_ok=rollback_ok && !RE4_QA_KEY_COUNT(base,0xa4) && !RE4_QA_KEY_COUNT(base,0xa5) && !RE4_QA_KEY_COUNT(base,0xa6);
done:
    LOGF("QA KEY EXPLICIT_INVENTORY_FIXTURE invoked=%d accepted=%d rollback_ok=%d global_key9=%d reason=%s NOT_AUTHORED_CHEST_COMPLETION",
        invoked,accepted,rollback_ok,key9,reason);
    return accepted;
}
static int qa_key_item_snapshot(void) {
    const unsigned char* game=qa_treasure_context(0,0x104);uintptr_t base=g_ue4_base_cached;
    if(!game){LOGF("QA KEY snapshot rejected: native R104 context/inventory");return 0;}
    int right=RE4_QA_KEY_COUNT(base,0xa4),left=RE4_QA_KEY_COUNT(base,0xa5),full=RE4_QA_KEY_COUNT(base,0xa6);
    if(right<0 || right>65535 || left<0 || left>65535 || full<0 || full>65535) {
        LOGF("QA KEY snapshot rejected: native counts");return 0;
    }
    int room9=-1,key9=-1;int room_valid=qa_key_room_save_flag(base,&room9),key_valid=qa_key_global_key_flag(base,&key9);
    LOGF("QA KEY snapshot room=104 type=%u right_a4=%d left_a5=%d full_a6=%d room_save_flag9=%d room_flag_valid=%d global_key9=%d key_flag_valid=%d read_only",
        cinema_type(),right,left,full,room9,room_valid,key9,key_valid);
    qa_key_receptacle_snapshot(base);
    return 1;
}
static int qa_key_save_fixture(void) {
    uintptr_t base=g_ue4_base_cached;const char* reason="active native R104 context";int invoked=0,requested=0,room9=-1,key9=-1;
    const unsigned char* game=qa_treasure_context(1,0x104);if(!game)goto done;
    static const struct {uintptr_t at;unsigned words[2];} checks[]={
        {0x5f1fe34,{0xd0022209,0xf9469929}},{0x5f1fcf0,{0xa9bd57f6,0xa9014ff4}},
        {0x5f177c8,{0xa9bd57f6,0xa9014ff4}},
        {0x62199c8,{0xf81e0ff3,0xa9017bfd}},{0x645023c,{0xd10183ff,0xf90023f3}},
        {0x63c8a5c,{0xd10183ff,0xf90023f3}},
    };
    reason="original save API preimages";
    for(unsigned i=0;i<sizeof checks/sizeof *checks;i++)
        if(memcmp((const void*)(base+checks[i].at),checks[i].words,8))goto done;
    unsigned mode_flags;memcpy(&mode_flags,game+4,4);reason="native Story save only; card idle";
    if((mode_flags&0x260u) || *(const uintptr_t*)(base+0xa4ff390))goto done;
    void* pawn=RE4_QA_KEY_PAWN(base);void* inventory=RE4_QA_KEY_FIXTURE_INVENTORY(base);
    reason="live typed native player/inventory";
    if(!pv_alive(pawn) || !menu_visual_layout(pawn,RE4_QA_KEY_FIXTURE_PAWN_CLASS(base),0x1608)
        || !pv_alive(inventory) || !menu_visual_layout(inventory,RE4_QA_KEY_FIXTURE_INVENTORY_CLASS(base),0xf8)
        || *(void* const*)((const char*)pawn+0x1600)!=inventory || !((const unsigned char*)inventory)[0xf0])goto done;
    reason="bounded stock key counts";
    for(unsigned item=0xa4;item<=0xa6;item++){int count=RE4_QA_KEY_COUNT(base,item);if(count<0 || count>1)goto done;}
    reason="validated separate native R104 RoomSave and global Key flags";
    if(!qa_key_room_save_flag(base,&room9) || !qa_key_global_key_flag(base,&key9))goto done;
    invoked=1;RE4_QA_KEY_SAVE_OPEN(base);
    requested=*(const uintptr_t*)(base+0xa4ff390)!=0;
    reason=requested?"original Story save menu requested; choose QA slot in native typewriter UI":"native save menu request refused";
done:
    LOGF("QA KEY SAVE_FIXTURE invoked=%d requested=%d room_save_flag9=%d global_key9=%d reason=%s SAVE_NOT_YET_VERIFIED",
        invoked,requested,room9,key9,reason);
    return requested;
}
#ifndef RE4_QA_KEY_STORY_SOURCE
#define RE4_QA_KEY_STORY_SOURCE "/storage/emulated/0/Android/data/com.Armature.VR4/files/savegame00.sav"
#define RE4_QA_KEY_STORY_COPY "/data/data/com.Armature.VR4/re4qa_story_before.sav"
#endif
static int qa_key_story_snapshot(void) {
    const char* reason="native R104 context";FILE *source=0,*copy=0;unsigned char* data=0;
    long length=0;int accepted=0,created=0;
    if(!qa_treasure_context(0,0x104))goto done;
    reason="open source";source=fopen(RE4_QA_KEY_STORY_SOURCE,"rb");if(!source)goto done;
    reason="source size";
    if(fseek(source,0,SEEK_END) || (length=ftell(source))<16 || length>16*1024*1024 || fseek(source,0,SEEK_SET))goto done;
    reason="read source";data=malloc((size_t)length);if(!data)goto done;
    if(fread(data,1,(size_t)length,source)!=(size_t)length || fgetc(source)!=EOF || ferror(source))goto done;
    reason="source changed";if(fseek(source,0,SEEK_SET))goto done;
    for(long at=0;at<length;) {
        unsigned char block[4096];size_t size=(size_t)(length-at);if(size>sizeof block)size=sizeof block;
        if(fread(block,1,size,source)!=size || memcmp(block,data+at,size))goto done;
        at+=(long)size;
    }
    if(fgetc(source)!=EOF || ferror(source))goto done;
    reason="private copy already exists/open failed";
    copy=fopen(RE4_QA_KEY_STORY_COPY,"wbx");if(!copy)goto done;created=1;
    reason="write private copy";
    if(fwrite(data,1,(size_t)length,copy)!=(size_t)length || fflush(copy))goto done;
    {int closed=fclose(copy);copy=0;if(closed)goto done;}
    accepted=1;reason="consistent source copied; game save unchanged";
done:
    if(source)fclose(source);
    if(copy)fclose(copy);
    if(created && !accepted && remove(RE4_QA_KEY_STORY_COPY))reason="partial private copy removal failed";
    free(data);LOGF("QA KEY Story snapshot accepted=%d bytes=%ld reason=%s",accepted,length,reason);
    return accepted;
}
#endif
#endif
