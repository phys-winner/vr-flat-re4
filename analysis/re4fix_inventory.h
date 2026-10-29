// Include after phone/reload helpers; process explicit inventory requests on GameThread.
#ifndef RE4FIX_INVENTORY_H
#define RE4FIX_INVENTORY_H
#include <math.h>
#include <string.h>
static int pv_alive(const void* object);

static int (*inventory_game_over)(void), (*inventory_qte)(void), (*inventory_pending_load)(void);
static int (*inventory_try_pause)(unsigned short);
struct inventory_drag_ray {float origin[3],direction[3];};
struct inventory_drag_plane {float normal[3],distance;};
_Static_assert(sizeof(struct inventory_drag_ray)==24&&sizeof(struct inventory_drag_plane)==16,"native FRay/FPlane ABI");
static void* (*inventory_drag_screen_class)(void),*(*inventory_drag_item_class)(void),*(*inventory_drag_case_class)(void);
static struct inventory_drag_ray (*inventory_drag_ray)(const void*);
static struct inventory_drag_plane (*inventory_drag_plane)(const void*);
static void* (*fv_ui_manager)(void),*(*fv_peek_screen)(void*);
static const void* inventory_drag_caller;
#ifndef RE4FIX_INVENTORY_TARGET_GATE
unsigned long g_tgt_inventory_drag;
__attribute__((naked)) static void gate_inventory_drag(void* item,const struct reload_tf* transform,void* parent,float scale) {
    asm volatile(
        ".inst 0xd10303ff\n" ".inst 0x6d0633ed\n" ".inst 0x6d072beb\n" ".inst 0x6d0823e9\n"
        "adrp x16, :got:g_tgt_inventory_drag\n" "ldr x16, [x16, :got_lo12:g_tgt_inventory_drag]\n"
        "ldr x16, [x16]\n" "br x16\n");
}
#else
#define gate_inventory_drag RE4FIX_INVENTORY_TARGET_GATE
#endif

static int inventory_drag_pose(struct reload_tf* target,struct inventory_drag_ray ray,struct inventory_drag_plane plane) {
    if(!reload_marker_valid(target))return 0;
    float normal=0,direction=0,dot=0,height=0;
    for(unsigned k=0;k<3;k++) {
        if(!isfinite(ray.origin[k])||!isfinite(ray.direction[k])||!isfinite(plane.normal[k]))return 0;
        normal+=plane.normal[k]*plane.normal[k];direction+=ray.direction[k]*ray.direction[k];
        dot+=ray.direction[k]*plane.normal[k];height+=target->p[k]*plane.normal[k];
    }
    if(!isfinite(plane.distance)||fabsf(normal-1)>.01f||fabsf(direction-1)>.01f||fabsf(dot)<.03f)return 0;
    float shift=(plane.distance+4.0f-height)/dot;
    if(!isfinite(shift)||fabsf(shift)>400)return 0;
    for(unsigned k=0;k<3;k++)target->p[k]+=shift*ray.direction[k];
    return reload_marker_valid(target);
}
// Adjust the native producer: a later InventoryUIScreen tick overwrites post-hand visual targets.
static void inventory_drag_dispatch(void* item,const struct reload_tf* transform,void* parent,float scale,const void* caller) {
    const struct reload_tf* chosen=transform;
    struct reload_tf target;
    if(caller!=inventory_drag_caller||!inventory_drag_caller||parent||!transform||!cinema_on_game_thread()
        ||!phone_type||phone_type()!=2||!phone_active||!fv_ui_manager||!fv_peek_screen||g_ptr_down<=0)goto forward;
    void* pawn=phone_active(),*manager=fv_ui_manager(),*screen=manager?fv_peek_screen(manager):0;
    if(!pawn||!pv_alive(screen)||!menu_visual_layout(screen,inventory_drag_screen_class(),0x3b0)
        ||*((const unsigned char*)screen+0x270)!=1||*(void* const*)((const char*)screen+0x2c0)!=item)goto forward;
    void* hand=*(void* const*)((const char*)screen+0x330);
    void* attache=*(void* const*)((const char*)screen+0x2f8);
    if(!attache)attache=*(void* const*)((const char*)screen+0x2f0);
    if(!pv_alive(item)||!menu_visual_layout(item,inventory_drag_item_class(),0x320)
        ||!pv_alive(hand)||!mv_hand_class||!menu_visual_layout(hand,mv_hand_class(),0x570)
        ||!phone_player||phone_player(hand)!=pawn||*((const unsigned char*)hand+0x314)!=1
        ||!*((const unsigned char*)hand+0x33d)||!pv_alive(attache)
        ||!menu_visual_layout(attache,inventory_drag_case_class(),0x2a8)
        ||!isfinite(scale)||scale<=0||scale>10)goto forward;
    void* root=*(void* const*)((const char*)attache+0x130);
    if(!pv_alive(root)||!mv_scene_class||!menu_visual_layout(root,mv_scene_class(),0x1f0)
        ||*(void* const*)((const char*)root+0x20)!=attache||*(void* const*)((const char*)root+0xa0)!=attache)goto forward;
    target=*transform;
    struct inventory_drag_ray ray=inventory_drag_ray(hand);
    struct inventory_drag_plane plane=inventory_drag_plane(attache);
    float original[3];memcpy(original,target.p,sizeof original);
    if(inventory_drag_pose(&target,ray,plane)) {
        chosen=&target;
        static unsigned logs;
        if(logs<4){logs++;LOGF("INVENTORY native lift item=%p p=(%.2f,%.2f,%.2f) original=(%.2f,%.2f,%.2f) lift=4cm",item,target.p[0],target.p[1],target.p[2],original[0],original[1],original[2]);}
    }
forward:
    gate_inventory_drag(item,chosen,parent,scale);
}
static void hook_inventory_drag(void* item,const struct reload_tf* transform,void* parent,float scale) {
    inventory_drag_dispatch(item,transform,parent,scale,__builtin_return_address(0));
}

static void (*inventory_aim_prepare)(void*);
static void (*inventory_aim_result)(int);
static void touch_inventory_tick(void* hand) {
    int (*try_pause)(unsigned short)=__atomic_load_n(&inventory_try_pause,__ATOMIC_ACQUIRE);
    if (!try_pause || !phone_player || !phone_active || !phone_type || !rl_paused
        || !hand || *(const unsigned char*)((const char*)hand+0x314)!=1) return;
    if (!__atomic_load_n(&g_touch_inventory_request,__ATOMIC_ACQUIRE)) return;
    unsigned type=phone_type();
    void* pawn=phone_player(hand);
    // Inactive pooled gameplay hands must leave the request for the active hand.
    if (type==1 && (!pawn || pawn!=phone_active())) return;
    if (!__atomic_exchange_n(&g_touch_inventory_request,0,__ATOMIC_ACQ_REL)) return;
    if (type!=1) { LOGF("INVENTORY unavailable: active type=%u",type); return; }
    const char* cls=*(const char* const*)((const char*)pawn+0x10);
    int bytes=cls ? *(const int*)(cls+0x58) : 0;
    if (bytes<0xc14 || bytes>0x10000) {
        LOGF("INVENTORY unavailable: pawn layout size=%d",bytes); return;
    }
    if (rl_paused() || inventory_game_over() || inventory_qte() || inventory_pending_load()) {
        LOGF("INVENTORY unavailable: paused, game over, QTE or pending load"); return;
    }
    float cooldown=*(const float*)((const char*)pawn+0xc10);
    if (!isfinite(cooldown) || cooldown>0.0f) {
        LOGF("INVENTORY unavailable: pause cooldown=%.3f",cooldown); return;
    }
    if(inventory_aim_prepare)inventory_aim_prepare(hand);
    int accepted=try_pause(0xff02); // Two-byte start info: Inventory=2, NoItem=255.
    if(inventory_aim_result)inventory_aim_result(accepted);
    LOGF("INVENTORY native TryPause accepted=%d",accepted);
}

static void install_touch_inventory(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[2]; } check[] = {
        {0x633d030,{0xf9417800,0xd65f03c0}}, {0x63380dc,{0xd0020168,0xf9431508}},
        {0x6338aec,{0xd0020168,0xf9431508}}, {0x6333dd0,{0x39000001,0x39000402}},
        {0x633223c,{0xf81d0ff5,0xa9014ff4}}, {0x61d4050,{0xb0020c68,0xf946bd08}},
        {0x62dd310,{0xb0020448,0xf9415508}}, {0x61d4760,{0xb0020c88,0xf9446508}},
        {0x62b4a20,{0xbd4c1260,0x1e202008}}, {0x61d0934,{0xb0020ca8,0xf9446508}},
    };
    if (!base || __atomic_load_n(&inventory_try_pause,__ATOMIC_ACQUIRE)) return;
    for (unsigned i=0;i<sizeof check/sizeof check[0];i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,sizeof check[i].first)) {
            LOGF("INVENTORY disabled: opcode mismatch +%lx",check[i].addr); return;
        }
    if (*(const long long*)(base+0x9df6738)!=2
        || phone_player!=(void* (*)(const void*))(base+0x633d030)
        || phone_active!=(void* (*)(void))(base+0x63380dc)
        || phone_type!=(unsigned (*)(void))(base+0x6338aec)
        || rl_paused!=(int (*)(void))(base+0x61d0934)) {
        LOGF("INVENTORY disabled: enum or shared getter dependency mismatch"); return;
    }
    inventory_game_over=(int (*)(void))(base+0x61d4050);
    inventory_qte=(int (*)(void))(base+0x62dd310);
    inventory_pending_load=(int (*)(void))(base+0x61d4760);
    __atomic_store_n(&inventory_try_pause,(int (*)(unsigned short))(base+0x633223c),__ATOMIC_RELEASE);
    static const struct{unsigned long at;unsigned words[2];} drag_checks[]={
        {0x6453f7c,{0xd10183ff,0xf90023f3}},{0x63c0274,{0xd10183ff,0xf90023f3}},
        {0x63c1c6c,{0xd10183ff,0xf90023f3}},{0x633e52c,{0xf81d0ff5,0xa9014ff4}},
        {0x620db10,{0xd101c3ff,0xf9002bf3}},{0x620b93c,{0xd10303ff,0x6d0633ed}},
        {0x620b980,{0xbd02d000,0xf9018c02}},{0x620b988,{0xf9016008,0xad150801}},
        {0x620b944,{0x6d072beb,0x6d0823e9}},{0x62e5d54,{0xaa1f03e2,0x97fc96f9}},
    };
    static const struct{unsigned long at;unsigned offset;} drag_fields[]={
        {0x9dc4c20,0x270},{0x9dc4b38,0x2c0},{0x9dc4a48,0x2f0},
        {0x9dc3ba8,0x40},{0x9dc3c68,0x8},{0x9dc3c98,0x0},{0x9d6d4d0,0x318},
    };
    int drag=mv_hand_class&&(void*)mv_hand_class==(void*)(base+0x6477678);
    for(unsigned i=0;i<sizeof drag_checks/sizeof *drag_checks;i++)
        if(memcmp((const void*)(base+drag_checks[i].at),drag_checks[i].words,8))drag=0;
    for(unsigned i=0;i<sizeof drag_fields/sizeof *drag_fields;i++)
        if(*(const unsigned*)(base+drag_fields[i].at+0x24)!=drag_fields[i].offset)drag=0;
    if(drag) {
        inventory_drag_screen_class=(void*(*)(void))(base+0x6453f7c);
        inventory_drag_item_class=(void*(*)(void))(base+0x63c0274);
        inventory_drag_case_class=(void*(*)(void))(base+0x63c1c6c);
        inventory_drag_ray=(struct inventory_drag_ray(*)(const void*))(base+0x633e52c);
        inventory_drag_plane=(struct inventory_drag_plane(*)(const void*))(base+0x620db10);
        inventory_drag_caller=(const void*)(base+0x62e5d5c);
#ifndef RE4FIX_INVENTORY_TARGET_GATE
        g_tgt_inventory_drag=base+0x620b94c;
        if(patch_code((void*)(base+0x620b93c),(void*)&hook_inventory_drag)) {
            inventory_drag_caller=0;LOGF("INVENTORY lift disabled: native target hook failed");
        }
#endif
    }
    LOGF("INVENTORY installed: explicit active gameplay request, native Inventory pause");
}
#endif
