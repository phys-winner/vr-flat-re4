// Research prototype only. No main/production include or input hook installs it.
// Caller supplies a fresh explicit touch on the original active native key menu.
#ifdef RE4_KEY_NATIVE_PROTOTYPE_ENABLED
#ifndef RE4_KEY_NATIVE_SELECT_PROTOTYPE_H
#define RE4_KEY_NATIVE_SELECT_PROTOTYPE_H
#ifndef RE4_KEY_PROTO_SELECT
#define RE4_KEY_PROTO_SELECT(base,menu,hand,item,position) ((void(*)(void*,void*,unsigned char,const float*))((base)+0x62f8e88))((menu),(hand),(unsigned char)(item),(position))
#define RE4_KEY_PROTO_CAN_SLOT(base,slot,item) ((int(*)(void*,unsigned char))((base)+0x62f7368))((slot),(unsigned char)(item))
#define RE4_KEY_PROTO_CAN_AUTO(base,owner,item) ((int(*)(void*,void*))((base)+0x62f564c))((owner),(item))
#define RE4_KEY_PROTO_AUTO(base,item) ((void(*)(void*))((base)+0x62ee0c4))(item)
#define RE4_KEY_PROTO_GRIP(base,hand) ((void)(base),f_GetCurrentGrip?f_GetCurrentGrip(hand):0)
#define RE4_KEY_PROTO_HAND(base,hand,pawn) ((void)(base),pv_alive(hand) && rl_hand_class && phone_player \
    && menu_visual_layout(hand,rl_hand_class(),0x510) && phone_player(hand)==(pawn) && !ti_down(hand))
#define RE4_KEY_PROTO_GRIP_TYPE(base,grip) ((void)(base),rl_grip_class && menu_visual_layout(grip,rl_grip_class(),0x460))
#define RE4_KEY_PROTO_WEAK_ASSIGN(out,object) cu_weak_assign(out,object)
#define RE4_KEY_PROTO_WEAK_GET(weak) cu_weak_get(weak)
#endif
#ifndef RE4_KEY_PROTO_RESET
#define RE4_KEY_PROTO_RESET(base,pickup) ((void(*)(void*))((base)+0x62eddd0))(pickup)
#define RE4_KEY_PROTO_RELEASE_HAND(base,hand) ((void(*)(void*))((base)+0x6298460))(hand)
#define RE4_KEY_PROTO_DESTROY(base,pickup) ((int(*)(void*,unsigned char,unsigned char))((base)+0x870a604))((pickup),0,1)
#endif
enum qa_key_native_result {QA_KEY_NATIVE_REFUSED,QA_KEY_NATIVE_SELECTED_UNPLACED,QA_KEY_NATIVE_PLACED_PENDING_USE};
// Same-GameThread refusal cleanup only, for this Select's still-owned temporary.
// Reset first prevents PrimaryGripReleased from entering Release/TryPlace at target.
static int qa_key_cleanup_unplaced(void* owner,void* hand,void* pickup,unsigned item) {
    uintptr_t base=g_ue4_base_cached;
    if(!qa_treasure_context(1,0x104) || item>0xfe || !pv_alive(owner) || !pv_alive(hand) || !pv_alive(pickup)
        || !menu_visual_layout(owner,RE4_QA_KEY_RECEPTACLE_CLASS(base),0x41c)
        || !menu_visual_layout(pickup,RE4_QA_TREASURE_CLASS(base),0x4e0)
        || ((const unsigned char*)pickup)[0x380]!=3 || ((const unsigned char*)pickup)[0x4dd]
        || *(void* const*)((const char*)pickup+0x3f8)
        || *(const unsigned short*)((const char*)pickup+0x4c0)!=item
        || *(void* const*)((const char*)owner+0x3d0)!=pickup || RE4_QA_KEY_COUNT(base,item)!=1)return 0;
    void* pawn=RE4_QA_KEY_PAWN(base),*grip=*(void* const*)((const char*)pickup+0x260);
    void* world=pv_alive(pawn)?RE4_QA_KEY_WORLD(base,pawn):0;
    if(!pv_alive(world) || RE4_QA_KEY_WORLD(base,owner)!=world || RE4_QA_KEY_WORLD(base,pickup)!=world
        || !RE4_KEY_PROTO_HAND(base,hand,pawn) || !pv_alive(grip) || !RE4_KEY_PROTO_GRIP_TYPE(base,grip)
        || RE4_KEY_PROTO_GRIP(base,hand)!=grip
        || *(void* const*)((const char*)grip+0x20)!=pickup || *(void* const*)((const char*)grip+0xa0)!=pickup
        || *(void* const*)((const char*)pickup+0x288)!=hand)return 0;
    static const struct {uintptr_t at;unsigned words[2];} checks[]={
        {0x62eddd0,{0xd10283ff,0x6d033bef}},{0x6298460,{0x52800021,0x14000c23}},
        {0x870a604,{0xa9bd57f6,0xa9014ff4}},
    };
    for(unsigned i=0;i<sizeof checks/sizeof *checks;i++)
        if(memcmp((const void*)(base+checks[i].at),checks[i].words,8))return 0;
    struct cinema_ui_weak weak[3];void* objects[3]={pickup,hand,owner};
    for(unsigned i=0;i<3;i++)RE4_KEY_PROTO_WEAK_ASSIGN(weak+i,objects[i]);
    RE4_KEY_PROTO_RESET(base,pickup);
    for(unsigned i=0;i<3;i++)if(RE4_KEY_PROTO_WEAK_GET(weak+i)!=objects[i])return 0;
    if(((const unsigned char*)pickup)[0x4c9] || ((const unsigned char*)pickup)[0x4ca]
        || *(void* const*)((const char*)pickup+0x288) || RE4_QA_KEY_COUNT(base,item)!=1
        || RE4_KEY_PROTO_GRIP(base,hand)!=grip)return 0;
    RE4_KEY_PROTO_RELEASE_HAND(base,hand);
    for(unsigned i=0;i<3;i++)if(RE4_KEY_PROTO_WEAK_GET(weak+i)!=objects[i])return 0;
    if(RE4_KEY_PROTO_GRIP(base,hand) || ((const unsigned char*)pickup)[0x4dd]
        || RE4_QA_KEY_COUNT(base,item)!=1)return 0;
    int destroyed=RE4_KEY_PROTO_DESTROY(base,pickup);
    return destroyed && !RE4_KEY_PROTO_GRIP(base,hand) && RE4_QA_KEY_COUNT(base,item)==1;
}
static enum qa_key_native_result qa_key_select_at_slot(void* menu,void* owner,void* slot,void* hand,unsigned item,int explicit_touch,void** selected) {
    uintptr_t base=g_ue4_base_cached;if(!selected)return QA_KEY_NATIVE_REFUSED;*selected=0;
    if(!explicit_touch || !qa_treasure_context(1,0x104) || item>0xfe)return QA_KEY_NATIVE_REFUSED;
    static const struct {uintptr_t at;unsigned words[2];} checks[]={
        {0x62f8e88,{0xd10203ff,0xf9001bf9}},{0x62f7368,{0x39530808,0x34000368}},
        {0x62f564c,{0xfc1b0fe8,0xa9015ff8}},{0x62ee0c4,{0xd10283ff,0xa90757f6}},
        {0x629a774,{0xf9421408,0xb4000268}},
    };
    for(unsigned i=0;i<sizeof checks/sizeof *checks;i++)
        if(memcmp((const void*)(base+checks[i].at),checks[i].words,8))return QA_KEY_NATIVE_REFUSED;
    void* pawn=RE4_QA_KEY_PAWN(base);void* world=pv_alive(pawn)?RE4_QA_KEY_WORLD(base,pawn):0;
    if(!pv_alive(world) || !pv_alive(owner) || !pv_alive(menu) || !pv_alive(slot)
        || !menu_visual_layout(owner,RE4_QA_KEY_RECEPTACLE_CLASS(base),0x41c)
        || !menu_visual_layout(menu,RE4_QA_KEY_MENU_CLASS(base),0x2a0)
        || !menu_visual_layout(slot,RE4_QA_KEY_SLOT_CLASS(base),0x4f8)
        || RE4_QA_KEY_WORLD(base,owner)!=world || RE4_QA_KEY_WORLD(base,menu)!=world
        || *(void* const*)((const char*)menu+0x298)!=owner || *(void* const*)((const char*)owner+0x3d8)!=menu
        || *(void* const*)((const char*)slot+0x20)!=owner || *(void* const*)((const char*)slot+0xa0)!=owner
        || *(void* const*)((const char*)slot+0x4f0)!=owner
        || ((const unsigned char*)owner)[0x3e0]!=2 || !((const unsigned char*)owner)[0x414]
        || !((const unsigned char*)owner)[0x416]
        || *(void* const*)((const char*)owner+0x3d0)
        || !RE4_KEY_PROTO_HAND(base,hand,pawn) || RE4_KEY_PROTO_GRIP(base,hand)
        || RE4_QA_KEY_COUNT(base,item)!=1 || !RE4_KEY_PROTO_CAN_SLOT(base,slot,item))return QA_KEY_NATIVE_REFUSED;
    struct {const void** data;int count,capacity;} slots;memcpy(&slots,(const char*)owner+0x3c0,sizeof slots);
    struct {const unsigned char* data;int count,capacity;} items;memcpy(&items,(const char*)menu+0x250,sizeof items);
    if(!qa_key_span(slots.data,slots.count,slots.capacity,8) || ((uintptr_t)slots.data&7)
        || !qa_key_span(items.data,items.count,items.capacity,1))return QA_KEY_NATIVE_REFUSED;
    int owned_slot=0,offered_item=0;
    for(int i=0;i<slots.count;i++)if(slots.data[i]==slot)owned_slot=1;
    for(int i=0;i<items.count;i++)if(items.data[i]==item)offered_item=1;
    if(!owned_slot || !offered_item)return QA_KEY_NATIVE_REFUSED;
    float target[3];memcpy(target,(const char*)slot+0x1d0,12);
    for(unsigned i=0;i<3;i++)if(!isfinite(target[i]) || fabsf(target[i])>10000000)return QA_KEY_NATIVE_REFUSED;
    struct cinema_ui_weak weak[4];void* objects[4]={pawn,owner,slot,hand};
    for(unsigned i=0;i<4;i++)RE4_KEY_PROTO_WEAK_ASSIGN(weak+i,objects[i]);
    RE4_KEY_PROTO_SELECT(base,menu,hand,item,target); // Original void ABI; no actor return.
    for(unsigned i=0;i<4;i++)if(RE4_KEY_PROTO_WEAK_GET(weak+i)!=objects[i])return QA_KEY_NATIVE_SELECTED_UNPLACED;
    if(RE4_QA_KEY_PAWN(base)!=pawn || RE4_QA_KEY_WORLD(base,owner)!=world)return QA_KEY_NATIVE_SELECTED_UNPLACED;
    void* pickup=*(void* const*)((const char*)owner+0x3d0);
    if(!pv_alive(pickup) || !menu_visual_layout(pickup,RE4_QA_TREASURE_CLASS(base),0x4e0)
        || *(const unsigned short*)((const char*)pickup+0x4c0)!=item
        || ((const unsigned char*)pickup)[0x380]!=3
        || *(void* const*)((const char*)pickup+0x288)!=hand
        || !((const unsigned char*)pickup)[0x4ca])return QA_KEY_NATIVE_SELECTED_UNPLACED;
    void* grip=*(void* const*)((const char*)pickup+0x260);
    if(!pv_alive(grip) || !RE4_KEY_PROTO_GRIP_TYPE(base,grip)
        || *(void* const*)((const char*)grip+0x20)!=pickup || *(void* const*)((const char*)grip+0xa0)!=pickup
        || RE4_KEY_PROTO_GRIP(base,hand)!=grip)return QA_KEY_NATIVE_SELECTED_UNPLACED;
    *selected=pickup;
    float actual[3];
    if(!qa_key_root(base,pickup,actual) || RE4_QA_KEY_COUNT(base,item)!=1
        || !RE4_KEY_PROTO_CAN_AUTO(base,owner,pickup))return QA_KEY_NATIVE_SELECTED_UNPLACED;
    LOGF("QA KEY native prototype item=%02x spawn_target=(%.3f,%.3f,%.3f) actual_root=(%.3f,%.3f,%.3f) original_CanAutoPlace=1",
        item,target[0],target[1],target[2],actual[0],actual[1],actual[2]);
    RE4_KEY_PROTO_AUTO(base,pickup); // Stock CanAutoPlace -> Release(false,NULL) -> TryPlace.
    if(RE4_KEY_PROTO_WEAK_GET(weak+1)!=owner || RE4_KEY_PROTO_WEAK_GET(weak+2)!=slot
        || *(void* const*)((const char*)slot+0x4e8)!=pickup)return QA_KEY_NATIVE_SELECTED_UNPLACED;
    return QA_KEY_NATIVE_PLACED_PENDING_USE; // Stock delayed UseItem/delegates remain hardware gates.
}
#endif
#endif
