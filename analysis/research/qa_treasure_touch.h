// QA only: existing authored Spinel collection and original merchant entry, never synthetic inventory.
#ifdef RE4_QA_USER_TRANSITIONS
#ifndef RE4_QA_TREASURE_TOUCH_H
#define RE4_QA_TREASURE_TOUCH_H
#include <stdint.h>
#include <string.h>
#include <math.h>
#ifndef RE4_QA_TREASURE_COUNT
#define RE4_QA_TREASURE_COUNT(base) ((int(*)(void*,int))((base)+0x5f33f14))((void*)((base)+0xa500780),0x57)
#endif
#ifndef RE4_QA_TREASURE_FIND
#define RE4_QA_TREASURE_FIND(base,id) ((void*(*)(int))((base)+0x62e9b5c))(id)
#endif
#ifndef RE4_QA_TREASURE_CLASS
#define RE4_QA_TREASURE_CLASS(base) ((void*(*)(void))((base)+0x645ace8))()
#endif
#ifndef RE4_QA_TREASURE_COLLECT
#define RE4_QA_TREASURE_COLLECT(base,actor) ((void(*)(void*))((base)+0x62ec678))(actor)
#endif
#ifndef RE4_QA_MERCHANT_OPEN
#define RE4_QA_MERCHANT_OPEN(base) ((int(*)(unsigned,unsigned))((base)+0x617d214))(0x10,0)
#endif
#ifndef RE4_QA_MERCHANT_COMPONENT
#define RE4_QA_MERCHANT_COMPONENT(base,model) ((void*(*)(const void*))((base)+0x631493c))(model)
#endif
#ifndef RE4_QA_MERCHANT_CLASS
#define RE4_QA_MERCHANT_CLASS(base) ((void*(*)(void))((base)+0x6485860))()
#endif
#ifndef RE4_QA_MERCHANT_OWNER
#define RE4_QA_MERCHANT_OWNER(base,model) ((void*(*)(const void*))((base)+0x69df1cc))((const char*)(model)+0x3d4)
#endif
#ifndef RE4_QA_SCENE_ROOMSAVE
#define RE4_QA_SCENE_ROOMSAVE(base,room) ((const unsigned char*(*)(const void*,unsigned short))((base)+0x614cdb0))((const void*)((base)+0xa540ce8),(unsigned short)(room))
#endif
#ifndef RE4_QA_SCENE_PTR
#define RE4_QA_SCENE_PTR(base,id) ((const unsigned char*(*)(unsigned))((base)+0x6153ec0))(id)
#endif
#ifndef RE4_QA_SCENE_EXECUTE
#define RE4_QA_SCENE_EXECUTE(base,id) ((int(*)(unsigned))((base)+0x6155664))(id)
#endif
#ifndef RE4_QA_SCENE_READY
#define RE4_QA_SCENE_READY(base) (!((int(*)(void))((base)+0x61d4050))() && !((int(*)(void))((base)+0x61d4760))() && !((int(*)(void))((base)+0x62dd310))() && ((int(*)(void))((base)+0x621a8f8))()>0)
#endif
#ifndef RE4_QA_DIAG_MERCHANT_CLASS
#define RE4_QA_DIAG_MERCHANT_CLASS(base) ((void*(*)(void))((base)+0x6488adc))()
#endif
#ifndef RE4_QA_DIAG_MERCHANT_INVENTORY
#define RE4_QA_DIAG_MERCHANT_INVENTORY(base,screen) ((void*(*)(const void*))((base)+0x63191f4))(screen)
#endif
#ifndef RE4_QA_DIAG_MERCHANT_STATE
#define RE4_QA_DIAG_MERCHANT_STATE(base,screen) ((unsigned char(*)(const void*))((base)+0x63168dc))(screen)
#endif
#ifndef RE4_QA_DIAG_CASE_PLANE
#define RE4_QA_DIAG_CASE_PLANE(base,object) ((struct inventory_drag_plane(*)(const void*))((base)+0x620dc24))(object)
#endif
#ifndef RE4_QA_DIAG_BINDINGS
#define RE4_QA_DIAG_BINDINGS(base) (inventory_drag_screen_class==(void*(*)(void))((base)+0x6453f7c) \
    && inventory_drag_item_class==(void*(*)(void))((base)+0x63c0274) \
    && inventory_drag_case_class==(void*(*)(void))((base)+0x63c1c6c) \
    && inventory_drag_ray==(struct inventory_drag_ray(*)(const void*))((base)+0x633e52c) \
    && mg_hover==(int(*)(struct mg_ui_object*,struct mg_ui_hit*,void*))((base)+0x63769bc) \
    && mg_enabled==(int(*)(const void*))((base)+0x6371fdc) \
    && mg_button_class==(void*(*)(void))((base)+0x64d8eec) \
    && mg_screen_class==(void*(*)(void))((base)+0x64db798) \
    && mv_scene_class==(void*(*)(void))((base)+0x8f82500) \
    && fv_ui_manager==(void*(*)(void))((base)+0x6373060) \
    && fv_peek_screen==(void*(*)(void*))((base)+0x6373328))
#endif
static unsigned qa_treasure_logs;
static int qa_treasure_finite(const float* values,unsigned count) {
    for(unsigned i=0;i<count;i++)if(!isfinite(values[i]))return 0;
    return 1;
}
static void qa_treasure_case_diag(const char* role,void* object,const struct inventory_drag_ray* ray) {
    uintptr_t base=g_ue4_base_cached;
    if(!pv_alive(object) || !menu_visual_layout(object,inventory_drag_case_class(),0x2a8)) {
        LOGF("QA TREASURE case=%s object=%p rejected=not_live_typed_case",role,object);return;
    }
    const char* actor=(const char*)object;void* root=*(void* const*)(actor+0x130);
    if(!pv_alive(root) || !menu_visual_layout(root,mv_scene_class(),0x1f0)
        || *(const void* const*)((const char*)root+0x20)!=object
        || *(const void* const*)((const char*)root+0xa0)!=object) {
        LOGF("QA TREASURE case=%s object=%p root=%p rejected=root_owner_or_type",role,object,root);return;
    }
    float transform[12];memcpy(transform,(const char*)root+0x1c0,sizeof transform);
    if(!qa_treasure_finite(transform,7) || !qa_treasure_finite(transform+8,3)) {
        LOGF("QA TREASURE case=%s object=%p rejected=nonfinite_transform",role,object);return;
    }
    struct inventory_drag_plane plane=RE4_QA_DIAG_CASE_PLANE(base,object);
    if(!qa_treasure_finite(plane.normal,3) || !isfinite(plane.distance)) {
        LOGF("QA TREASURE case=%s object=%p rejected=nonfinite_plane",role,object);return;
    }
    float dot=0;for(unsigned i=0;i<3;i++)dot+=plane.normal[i]*ray->direction[i];
    LOGF("QA TREASURE case=%s object=%p hidden=%u root=%p q=(%.5f,%.5f,%.5f,%.5f) p=(%.3f,%.3f,%.3f) s=(%.3f,%.3f,%.3f) interaction_plane=(%.5f,%.5f,%.5f,%.3f) ray_dot=%.5f read_only",
        role,object,((const unsigned char*)object)[0x58]>>5&1u,root,
        transform[0],transform[1],transform[2],transform[3],transform[4],transform[5],transform[6],
        transform[8],transform[9],transform[10],plane.normal[0],plane.normal[1],plane.normal[2],plane.distance,dot);
}
static void qa_treasure_menu_diag(void) {
    uintptr_t base=g_ue4_base_cached;
    if(!cinema_on_game_thread() || !cinema_type || cinema_type()!=8)return;
    if(!RE4_QA_DIAG_BINDINGS(base)) {LOGF("QA TREASURE menu diag rejected=native_bindings");return;}
    static const struct {uintptr_t at;unsigned words[2];} checks[]={
        {0x6488adc,{0xd10183ff,0xf90023f3}},{0x63191f4,{0xf9418c00,0xd65f03c0}},
        {0x63168dc,{0x394c5000,0xd65f03c0}},
        {0x620dc24,{0xd101c3ff,0xf9002bf3}},{0x633e52c,{0xf81d0ff5,0xa9014ff4}},
        {0x63769bc,{0xf81b0ff9,0xa9015ff8}},{0x6371fdc,{0xf81e0ff3,0xa9017bfd}},
    };
    for(unsigned i=0;i<sizeof checks/sizeof *checks;i++)
        if(memcmp((const void*)(base+checks[i].at),checks[i].words,8)) {
            LOGF("QA TREASURE menu diag rejected=preimage at=%lx",(unsigned long)checks[i].at);return;
        }
    void* hand=mg_seen_hand,*pawn=0;unsigned type=0;
    if(!pv_alive(hand) || !mg_current(hand,&pawn,&type) || type!=8 || pawn!=cinema_active()) {
        LOGF("QA TREASURE menu diag rejected=current_live_owned_hand hand=%p type=%u",hand,type);return;
    }
    struct mg_ui_object hovered={0,0,0};struct mg_ui_hit hit={{0,0,0},0};
    int hover=mg_hover(&hovered,&hit,hand);
    struct inventory_drag_ray ray=inventory_drag_ray(hand);
    if(!qa_treasure_finite(ray.origin,3) || !qa_treasure_finite(ray.direction,3)) {
        LOGF("QA TREASURE menu diag rejected=nonfinite_ray hand=%p",hand);return;
    }
    int button=pv_alive(hovered.actor) && pv_alive(hovered.component) && pv_alive(hovered.screen)
        && menu_visual_layout(hovered.component,mg_button_class(),0x330)
        && menu_visual_layout(hovered.screen,mg_screen_class(),0x248)
        && *(void* const*)((const char*)hovered.component+0xa0)==hovered.actor
        && *(void* const*)((const char*)hovered.component+0x308)==hovered.screen;
    int action=button ? *((const unsigned char*)hovered.component+0x320):-1;
    int enabled=button ? mg_enabled(hovered.component):-1;
    float button_position[3]={0,0,0};
    if(button)memcpy(button_position,(const char*)hovered.component+0x1d0,sizeof button_position);
    LOGF("QA TREASURE menu hand=%p pawn=%p grip=%u hover=%d actor=%p component=%p screen=%p typed_owned_button=%d action=%d enabled=%d button_p=(%.3f,%.3f,%.3f) hit=(%.3f,%.3f,%.3f) distance=%.3f ray_origin=(%.3f,%.3f,%.3f) ray_direction=(%.5f,%.5f,%.5f) read_only",
        hand,pawn,*((const unsigned char*)hand+0x33d),hover,hovered.actor,hovered.component,hovered.screen,button,action,enabled,
        button_position[0],button_position[1],button_position[2],hit.position[0],hit.position[1],hit.position[2],hit.distance,
        ray.origin[0],ray.origin[1],ray.origin[2],ray.direction[0],ray.direction[1],ray.direction[2]);
    void* manager=fv_ui_manager();void* top=pv_alive(manager)?fv_peek_screen(manager):0;void* screen=0;
    if(pv_alive(top) && menu_visual_layout(top,inventory_drag_screen_class(),0x3b0))screen=top;
    else if(pv_alive(top) && menu_visual_layout(top,RE4_QA_DIAG_MERCHANT_CLASS(base),0x320)) {
        LOGF("QA TREASURE merchant screen=%p state=%u read_only",top,(unsigned)RE4_QA_DIAG_MERCHANT_STATE(base,top));
        screen=RE4_QA_DIAG_MERCHANT_INVENTORY(base,top);
    }
    if(!pv_alive(screen) || !menu_visual_layout(screen,inventory_drag_screen_class(),0x3b0)) {
        LOGF("QA TREASURE menu inventory rejected=active_typed_screen top=%p inventory=%p",top,screen);return;
    }
    const char* inventory_screen=(const char*)screen;
    void* item=*(void* const*)(inventory_screen+0x2c0);
    int item_valid=pv_alive(item) && menu_visual_layout(item,inventory_drag_item_class(),0x320);
    void* primary=*(void* const*)(inventory_screen+0x258),*secondary=*(void* const*)(inventory_screen+0x260);
    LOGF("QA TREASURE menu inventory=%p top=%p UIState=%u active=%p item_valid=%d item_state=%d item_hand=%p primary=%p secondary=%p initial_case=%p current_case=%p grab_hand=%p read_only",
        screen,top,*((const unsigned char*)screen+0x270),item,item_valid,item_valid?*((const unsigned char*)item+0x240):-1,
        item_valid?*(void* const*)((const char*)item+0x248):0,primary,secondary,
        *(void* const*)(inventory_screen+0x2f0),*(void* const*)(inventory_screen+0x2f8),*(void* const*)(inventory_screen+0x330));
    qa_treasure_case_diag("primary",primary,&ray);
    qa_treasure_case_diag("secondary",secondary,&ray);
}
static const unsigned char* qa_treasure_context(int mutation,unsigned room) {
    uintptr_t base=g_ue4_base_cached;
    if(!base || !cinema_on_game_thread() || !cinema_type || !cinema_active
        || !rl_player_valid || !rl_paused)return 0;
    unsigned type=cinema_type();void* pawn=cinema_active();
    if(!pawn || !pv_alive(pawn) || (mutation ? type!=1 || !rl_player_valid() || rl_paused()
        : type!=1 && type!=2 && type!=8))return 0;
    const unsigned char* game=*(const unsigned char* const*)(base+0xa3f5e48);
    if((uintptr_t)game!=base+0xa501bd8)return 0;
    unsigned short current;unsigned mode;memcpy(&current,game+0x5038,2);memcpy(&mode,game+0x24,4);
    if((room && current!=room) || (mutation && mode!=3))return 0;
    static const unsigned count_code[2]={0xf81c0ff9,0xa9015ff8};
    if(memcmp((const void*)(base+0x5f33f14),count_code,8)
        || *(const uintptr_t*)(base+0xa361da0)!=base+0xa500780)return 0;
    const unsigned char* inventory=(const unsigned char*)(base+0xa500780);
    uintptr_t data;int count;memcpy(&data,inventory+0x20,8);memcpy(&count,inventory+0x30,4);
    if(data<4096 || (data&1) || count<1 || count>4096 || data>UINTPTR_MAX-(unsigned)count*14u)return 0;
    const unsigned char* player=*(const unsigned char* const*)(base+0xa4e9b40);
    return player && player[0x3dd] ? game:0;
}
static int qa_treasure_snapshot(void) {
    const unsigned char* game=qa_treasure_context(0,0);
    if(!game) {LOGF("QA TREASURE snapshot rejected: native context/inventory");return 0;}
    unsigned money,mode;unsigned short room;memcpy(&money,game+0x5034,4);memcpy(&room,game+0x5038,2);memcpy(&mode,game+0x24,4);
    int count=RE4_QA_TREASURE_COUNT(g_ue4_base_cached);
    if(count<0 || count>65535) {LOGF("QA TREASURE snapshot rejected: native count=%d",count);return 0;}
    if(qa_treasure_logs<24) {
        qa_treasure_logs++;
        LOGF("QA TREASURE snapshot=%u/24 room=%03x type=%u mode=%u item=0057 Spinel_count=%d pesetas=%u read_only",
            qa_treasure_logs,room,cinema_type(),mode,count,money);
        qa_treasure_menu_diag();
    }
    return 1;
}
static int qa_authored_spinel_pickup(void) {
    const char* reason="context";int invoked=0,accepted=0,before=-1,after=-1;
    const unsigned char* game=qa_treasure_context(1,0x101);uintptr_t base=g_ue4_base_cached;
    if(!game)goto done;
    static const struct {uintptr_t at;unsigned words[2];} checks[]={
        {0x62ec678,{0xd10103ff,0xa90157f6}},{0x6153664,{0xf81d0ff5,0xa9014ff4}},
        {0x615bce8,{0xa9bf7bfd,0x910003fd}},
        {0x61538d4,{0xd10143ff,0xa9015ff8}},{0x62e9b5c,{0xd10283ff,0xa90757f6}},
        {0x6153ec0,{0xf0021f68,0xf9402d08}},{0x62e9b7c,{0x97f9a8d1,0xb4000a00}},
        {0x645ace8,{0xd10183ff,0xf90023f3}},
    };
    reason="preimage";
    for(unsigned i=0;i<sizeof checks/sizeof *checks;i++)
        if(memcmp((const void*)(base+checks[i].at),checks[i].words,8))goto done;
    if(*(const uintptr_t*)(base+0x9cee050)!=base+0x615bce8)goto done;
    const unsigned char* manager=*(const unsigned char* const*)(base+0xa542058);
    reason="ITA header";
    if((uintptr_t)manager!=base+0xa541b38)goto done;
    const unsigned char *header,*entries;memcpy(&header,manager+0x10,8);memcpy(&entries,manager+0x18,8);
    if((uintptr_t)header<4096 || ((uintptr_t)header&3) || (uintptr_t)header>UINTPTR_MAX-16-41u*0xd0
        || (uintptr_t)entries!=(uintptr_t)header+16)goto done;
    unsigned short version,count;memcpy(&version,header+4,2);memcpy(&count,header+6,2);
    if(memcmp(header,"ITA",4) || version!=0x105 || count!=41)goto done;
    const unsigned char* entry=entries+9*0xd0;
    reason="authored Spinel";
    unsigned short item,save_id,quantity;memcpy(&item,entry+0xa4,2);memcpy(&quantity,entry+0xa8,2);memcpy(&save_id,entry+0xaa,2);
    if((entry[0x38]&3)!=3 || entry[0x39]!=3 || entry[0x3a]!=0x89 || item!=0x57 || save_id!=32 || quantity>1
        || !*(const uintptr_t*)(entry+0x90))goto done;
    reason="live owned pickup";
    // Original GetItemBySceAtId uses SceAtPtr; exact actor binding proves registration.
    void* actor=RE4_QA_TREASURE_FIND(base,0x89);
    if(!actor || !pv_alive(actor) || !menu_visual_layout(actor,RE4_QA_TREASURE_CLASS(base),0x4e0)
        || *(const void* const*)((const char*)actor+0x3f8)!=entry
        || *(const unsigned short*)((const char*)actor+0x4c0)!=0x57
        || !((const unsigned char*)actor)[0x4c2] || !((const unsigned char*)actor)[0x4c3]
        || ((const unsigned char*)actor)[0x4dc])goto done;
    before=RE4_QA_TREASURE_COUNT(base);
    reason="already owned/invalid count";if(before!=0)goto done;
    invoked=1;RE4_QA_TREASURE_COLLECT(base,actor);after=RE4_QA_TREASURE_COUNT(base);
    accepted=after==1;reason=accepted?"original authored pickup collected":"native collection refused";
done:
    LOGF("QA TREASURE authored R101 ITA9 native_id=89 save_id=32 invoked=%d accepted=%d Spinel=%d->%d reason=%s",
        invoked,accepted,before,after,reason);
    return accepted;
}
static int qa_original_merchant_entry(void) {
    const char* reason="context";int invoked=0;uintptr_t base=g_ue4_base_cached;
    if(!qa_treasure_context(1,0x104))goto done;
    static const struct {uintptr_t at;unsigned words[2];} checks[]={
        {0x617d214,{0xd100c3ff,0xf9000bf3}},{0x631493c,{0xf81e0ff3,0xa9017bfd}},
        {0x6485860,{0xd10183ff,0xf90023f3}},{0x69df1cc,{0xb9400408,0x34000368}},
        {0x83289ec,{0xf9405008,0xb4000068}},
    };
    reason="preimage";
    for(unsigned i=0;i<sizeof checks/sizeof *checks;i++)
        if(memcmp((const void*)(base+checks[i].at),checks[i].words,8))goto done;
    const unsigned char* manager=(const unsigned char*)(base+0xa4e9b68);
    uintptr_t data;unsigned count,stride;memcpy(&data,manager+8,8);memcpy(&count,manager+0x10,4);memcpy(&stride,manager+0x14,4);
    reason="EmMgr";
    if(data<4096 || (data&3) || count<1 || count>4096 || stride<0x1110 || stride>0x10000
        || data>UINTPTR_MAX-(uintptr_t)count*stride)goto done;
    reason="live R104 merchant";int found=0;
    for(unsigned i=0;i<count;i++) {
        const unsigned char* actor=(const unsigned char*)(data+(uintptr_t)i*stride);unsigned flags;memcpy(&flags,actor+8,4);
        if((flags&0x201)!=1 || !((actor[0x118]==0x10 && actor[0x119]==6) || actor[0x118]==0x18))continue;
        // Quest merchants use cEm10 subtype6; the owned native component is authoritative.
        void* component=RE4_QA_MERCHANT_COMPONENT(base,actor);
        if(!component || !pv_alive(component) || !menu_visual_layout(component,RE4_QA_MERCHANT_CLASS(base),0xc0))continue;
        void* owner=RE4_QA_MERCHANT_OWNER(base,actor);
        if(!owner || !pv_alive(owner) || *(const void* const*)((const char*)component+0xa0)!=owner)continue;
        found=1;break;
    }
    if(!found)goto done;
    (void)RE4_QA_MERCHANT_OPEN(base);invoked=1;reason="original SubScreenOpen(16,0); verify native context8 after transition";
done:
    LOGF("QA TREASURE merchant entry invoked=%d reason=%s",invoked,reason);
    return invoked;
}
// Separate setup actions: run the original yielding room scene through its scheduler.
static int qa_authored_room_scene(unsigned room) {
    int house=room==0x101,invoked=0,scheduled=0;const char* reason="context";
    unsigned id=house?8:18,kind=house?2:1;uintptr_t base=g_ue4_base_cached;
    uintptr_t callback=base+(house?0x6003ed8:0x60093c0);
    if((room!=0x101 && room!=0x104) || !qa_treasure_context(1,room))goto done;
    static const struct {uintptr_t at;unsigned words[2];} checks[]={
        {0x614cdb0,{0x53083c28,0x7100251f}},{0x6155664,{0xb0021f68,0xf9402d08}},
        {0x6153ec0,{0xf0021f68,0xf9402d08}},{0x615bc6c,{0xf81e0ff3,0xa9017bfd}},
        {0x616107c,{0xf81b0ff9,0xa9015ff8}},{0x61627bc,{0xb0021f09,0x2a0003e8}},
        {0x60093c0,{0xd10143ff,0xf90013f5}},{0x6003ed8,{0xd10243ff,0xa90467fa}},
        {0x61d4050,{0xb0020c68,0xf946bd08}},{0x61d4760,{0xb0020c88,0xf9446508}},
        {0x62dd310,{0xb0020448,0xf9415508}},{0x621a8f8,{0xa9bf7bfd,0x910003fd}},
    };
    reason="preimage";
    for(unsigned i=0;i<sizeof checks/sizeof *checks;i++)
        if(memcmp((const void*)(base+checks[i].at),checks[i].words,8))goto done;
    if(*(const uintptr_t*)(base+0xa361d50)!=base+0xa540ce8
        || *(const uintptr_t*)(base+0xa542058)!=base+0xa541b38
        || *(const uintptr_t*)(base+0x9cee040)!=base+0x615bc6c)goto done;
    reason="native live/story state";
    if(!RE4_QA_SCENE_READY(base) || *(const unsigned char*)(base+0xa52d8d0))goto done;
    uintptr_t saves=*(const uintptr_t*)(base+0xa540ce8+0x30);
    if(saves<4096 || (saves&3) || saves>UINTPTR_MAX-512u*0xd8u)goto done;
    const unsigned char* save=RE4_QA_SCENE_ROOMSAVE(base,room);uintptr_t address=(uintptr_t)save;
    if(address<saves || address>=saves+512u*0xd8u || (address-saves)%0xd8u
        || (house ? (save[6]&0x80) || (save[7]&1):(save[5]&2)))goto done;
    reason="registered authored callback";
    const unsigned char* entry=RE4_QA_SCENE_PTR(base,id);
    if((uintptr_t)entry<4096 || ((uintptr_t)entry&3) || (uintptr_t)entry>UINTPTR_MAX-0x100u)goto done;
    uintptr_t function,argument;memcpy(&function,entry+0x48,8);memcpy(&argument,entry+0x40,8);
    // Original registration may resolve a dynamic entry; never duplicate its mixed-list walker.
    if(!(entry[0x38]&1) || entry[0x39]!=2 || entry[0x3a]!=id || entry[0x3e]!=0x14
        || function!=callback || argument || entry[0x50]!=2 || entry[0x51]!=kind)goto done;
    reason="busy native scheduler";
    uintptr_t current=*(const uintptr_t*)(base+0xa543150);
    if(current && current!=base+0xa40a0f8 && current!=UINTPTR_MAX)goto done;
    const unsigned char* tasks=(const unsigned char*)(base+0xa5436e8);int free_task=0;
    for(unsigned i=0;i<12;i++) {
        const unsigned char* task=tasks+i*0x40;memcpy(&function,task+0x18,8);
        if(!task[0])free_task=1;
        else if(function==callback)goto done;
    }
    if(!free_task)goto done;
    invoked=1;(void)RE4_QA_SCENE_EXECUTE(base,id);
    for(unsigned i=0;i<12;i++) {
        const unsigned char* task=tasks+i*0x40;memcpy(&function,task+0x18,8);memcpy(&argument,task+0x10,8);
        if(task[0] && task[1]==14 && task[3]==kind && function==callback && !argument)scheduled++;
    }
    reason=scheduled==1?"original yielding scene scheduled; verify scene and native final pose":"native scheduling refused";
done:
    LOGF("QA TREASURE authored scene room=%03x id=%u kind=%u invoked=%d scheduled=%d reason=%s",room,id,kind,invoked,scheduled,reason);
    return scheduled==1;
}
static int qa_authored_merchant_reveal(void) {return qa_authored_room_scene(0x104);}
static int qa_authored_house_entry(void) {return qa_authored_room_scene(0x101);}
#endif
#endif
