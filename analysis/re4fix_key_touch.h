// Include after interact, prompt_visual and phone_view. GameThread only.
// A fresh phone Check reaches the original key-scene confirm/delegate path.
#ifndef RE4FIX_KEY_TOUCH_H
#define RE4FIX_KEY_TOUCH_H
#include <stdint.h>
#include <limits.h>
static uintptr_t kt_base;
static int kt_ready;
static unsigned kt_logs;
static void* (*kt_receptor_class)(void),*(*kt_box_class)(void),*(*kt_controller_class)(void);
static void* (*kt_world)(const void*),*(*kt_weapon_hand)(const void*);
static float (*kt_distance)(const void*,const void*,const float*);
static int (*kt_looking)(const float*,const struct pv_scene_tf*,float,unsigned char,float);
static struct {
    struct cinema_ui_weak pawn,manager,actor;
    uint64_t handle;
    unsigned phase;
    int cursor;
    long start;
} kt_request;
struct kt_delegate {const unsigned char* instance;int units,pad;};
struct kt_list {const struct kt_delegate* data;int count,capacity;};
_Static_assert(sizeof(struct kt_delegate)==16 && sizeof(struct kt_list)==16,"native delegate array ABI");

static void touch_key_item_cancel(void) {memset(&kt_request,0,sizeof kt_request);}
static int kt_typed(const void* object,const void* cls,int minimum) {
    return pv_alive(object) && menu_visual_layout(object,cls,minimum);
}
static int kt_span(const struct kt_list* list) {
    return list->count>=0 && list->count<=32 && list->capacity>=list->count && list->capacity<=64
        && (!list->count || (list->data && !((uintptr_t)list->data&7)
            && (uintptr_t)list->data<=UINTPTR_MAX-(size_t)list->count*sizeof *list->data));
}
static int kt_scene(const void* component,const void* owner,int minimum) {
    return ti_scene_class && kt_typed(component,ti_scene_class(),0x1f0) && interact_layout(component,minimum)
        && *(const void* const*)((const char*)component+0x20)==owner
        && *(const void* const*)((const char*)component+0xa0)==owner;
}
static int kt_context(void* pawn) {
    if(!kt_ready || !cinema_on_game_thread() || !cu_weak_assign || !cu_weak_get
        || !phone_active || !phone_type || !phone_player || !rl_paused || !rl_player_valid
        || !ti_get_hand || !kt_weapon_hand || !kt_controller_class
        || __atomic_load_n(&g_touch_context,__ATOMIC_ACQUIRE)!=1
        || !__atomic_load_n(&g_app_input_focus,__ATOMIC_ACQUIRE)
        || __atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE)
        || phone_type()!=1 || phone_active()!=pawn || rl_paused() || !rl_player_valid())return 0;
    float body[3];if(!pv_alive(pawn) || !interact_body_origin(pawn,body))return 0;
    void* hand=kt_weapon_hand(pawn);
    if(!pv_alive(hand) || !interact_layout(hand,0x510))return 0;
    unsigned side=*((const unsigned char*)hand+0x314);
    if(side>1 || !interact_bound_hand(hand,pawn,(int)side)
        || ti_get_hand(pawn,(unsigned char)side)!=hand
        || *(void* const*)((const char*)hand+0x2f0)!=pawn)return 0;
    // Original WasKeyJustPressed requires this real pawn's PlayerController/PlayerInput.
    void* controller=*(void* const*)((const char*)pawn+0x258);
    return kt_typed(controller,kt_controller_class(),0x350)
        && pv_alive(*(void* const*)((const char*)controller+0x348));
}
static int kt_geometry(void* actor,void* pawn) {
    void* head=*(void* const*)((const char*)pawn+0x2d8);
    void* root=*(void* const*)((const char*)actor+0x130);
    void* box=*(void* const*)((const char*)actor+0x238);
    if(!pv_head_transform || !kt_distance || !kt_looking || !kt_box_class
        || !kt_scene(head,pawn,0x1f0) || !kt_scene(root,actor,0x1f0))return 0;
    // NULL MessageVolume is authored and uses native head-to-root distance.
    if(box && (!kt_scene(box,actor,0x42c) || !kt_typed(box,kt_box_class(),0x42c)))return 0;
    struct pv_scene_tf head_tf=pv_head_transform(pawn);
    if(!pv_tf_finite(&head_tf))return 0;
    float point[3];memcpy(point,(const char*)root+0x1d0,sizeof point);
    for(unsigned i=0;i<3;i++)if(!isfinite(point[i]))return 0;
    if(box) {
        struct pv_scene_tf box_tf;memcpy(&box_tf,(const char*)box+0x1c0,sizeof box_tf);
        float norm=0;for(unsigned i=0;i<4;i++)norm+=box_tf.q[i]*box_tf.q[i];
        if(!isfinite(norm) || fabsf(norm-1)>.01f)return 0;
        // Native safe reciprocal accepts mirrored and zero component scales.
        for(unsigned i=0;i<3;i++)if(!isfinite(box_tf.p[i]) || !isfinite(box_tf.s[i]))return 0;
        const float* extent=(const float*)((const char*)box+0x420);
        for(unsigned i=0;i<3;i++)if(!isfinite(extent[i]) || extent[i]<0)return 0;
    }
    float radius=*(const float*)((const char*)actor+0x360),angle=*(const float*)((const char*)actor+0x364);
    if(!isfinite(radius) || radius<=0 || !isfinite(angle) || angle<0 || angle>180)return 0;
    float distance=kt_distance(actor,box,head_tf.p);
    return isfinite(distance) && distance>=0 && distance<radius*radius
        && kt_looking(point,&head_tf,angle,*((const unsigned char*)actor+0x414),5.0f);
}
static int kt_actor_ready(void* actor,void* manager,void* pawn) {
    if(!kt_typed(actor,kt_receptor_class(),0x420))return 0;
    const unsigned char* bytes=actor;
    if(bytes[0x3e0]!=1 || !bytes[0x414] || !bytes[0x416] || !bytes[0x418]
        || bytes[0x419] || bytes[0x41a] || pv_alive(*(void* const*)(bytes+0x3d0)))return 0;
    void* world=kt_world(pawn);
    return pv_alive(world) && kt_world(manager)==world && kt_world(actor)==world && kt_geometry(actor,pawn);
}
static void* kt_message_actor(void* manager,void* pawn,uint64_t* handle) {
    // Next-list removal is native after the first Check. The same scene still
    // owns its original OnFinishMessages UFunction delegate until text ends.
    const unsigned char* bytes=manager;
    int index=*(const int*)(bytes+0x2a4),count=*(const int*)(bytes+0x2ac);
    unsigned message=*(const unsigned*)(bytes+0x2c4);
    if(bytes[0x2a0]!=1 || bytes[0x2c8]!=1 || count<1 || count>256 || index<0 || index>count
        || (message!=UINT_MAX && !index))return 0;
    struct kt_list list;memcpy(&list,bytes+0x3e0,sizeof list);
    if(!kt_span(&list))return 0;
    const struct kt_delegate* finished=0;
    for(int i=0;i<list.count;i++)if(list.data[i].units) {
        if(finished)return 0;finished=list.data+i;
    }
    if(!finished || finished->units!=3 || !finished->instance || ((uintptr_t)finished->instance&7)
        || *(const uintptr_t*)finished->instance!=kt_base+0x9d2d500)return 0;
    void* actor=cu_weak_get((const struct cinema_ui_weak*)(finished->instance+0x18));
    if(!kt_actor_ready(actor,manager,pawn)
        || *(const unsigned*)((const char*)actor+0x340)!=message)return 0;
    struct kt_delegate local;memcpy(&local,(const char*)actor+0x390,sizeof local);
    if(local.units!=3 || !local.instance || ((uintptr_t)local.instance&7)
        || *(const uintptr_t*)local.instance!=kt_base+0x9d2d500
        || cu_weak_get((const struct cinema_ui_weak*)(local.instance+0x18))!=actor)return 0;
    uint64_t name=*(const uint64_t*)(local.instance+0x10),id=*(const uint64_t*)(local.instance+0x28);
    uintptr_t function=*(const uintptr_t*)(local.instance+8),clone_function=*(const uintptr_t*)(finished->instance+8);
    if(!name || !id || *(const uint64_t*)(finished->instance+0x10)!=name
        || *(const uint64_t*)(finished->instance+0x28)!=id
        || (function && clone_function && function!=clone_function))return 0;
    *handle=id;return actor;
}
static void* kt_pending_actor(void* manager,void* pawn,uint64_t* handle,unsigned* phase) {
    if(!ti_manager || !ti_manager_class || !kt_receptor_class || !kt_world
        || !kt_typed(manager,ti_manager_class(),0x410) || ti_manager()!=manager
        || !*((const unsigned char*)manager+0x2c1))return 0;
    struct kt_list list;memcpy(&list,(const char*)manager+0x3f8,sizeof list);
    if(!kt_span(&list))return 0;
    const struct kt_delegate* next=0;
    for(int i=0;i<list.count;i++)if(list.data[i].units) {
        if(next)return 0;next=list.data+i; // Broadcast must not reach a foreign listener.
    }
    if(!next) {
        void* actor=kt_message_actor(manager,pawn,handle);
        if(actor)*phase=2;return actor; // Tombstones may keep native Num nonzero.
    }
    if(next->units!=2 || !next->instance || ((uintptr_t)next->instance&7)
        || *(const uintptr_t*)next->instance!=kt_base+0x9d2d420)return 0;
    void* actor=*(void* const*)(next->instance+8);
    if(!kt_actor_ready(actor,manager,pawn))return 0;
    const unsigned char* bytes=actor;
    struct kt_delegate local;memcpy(&local,bytes+0x3e8,sizeof local);
    uint64_t native_handle;memcpy(&native_handle,bytes+0x3f8,sizeof native_handle);
    if(!native_handle || local.units!=2 || !local.instance || ((uintptr_t)local.instance&7)
        || *(const uintptr_t*)local.instance!=kt_base+0x9d2d420
        || *(void* const*)(local.instance+8)!=actor
        || *(const uint64_t*)(local.instance+0x18)!=native_handle
        || *(const uint64_t*)(next->instance+0x18)!=native_handle)return 0;
    *handle=native_handle;*phase=1;return actor;
}
static int touch_key_item_interact(void* pawn,long now) {
    if(now<=0 || !kt_context(pawn)) {touch_key_item_cancel();return 0;}
    void* manager=ti_manager();uint64_t handle=0;unsigned phase=0;
    void* actor=kt_pending_actor(manager,pawn,&handle,&phase);
    if(!actor) {touch_key_item_cancel();return 0;}
    int cursor=phase==2 ? *(const int*)((const char*)manager+0x2a4):0;
    if(kt_request.start && now>=kt_request.start && now-kt_request.start<=300
        && cu_weak_get(&kt_request.pawn)==pawn && cu_weak_get(&kt_request.manager)==manager
        && cu_weak_get(&kt_request.actor)==actor && kt_request.handle==handle && kt_request.phase==phase
        && kt_request.cursor==cursor)return 1;
    touch_key_item_cancel();
    cu_weak_assign(&kt_request.pawn,pawn);cu_weak_assign(&kt_request.manager,manager);cu_weak_assign(&kt_request.actor,actor);
    if(cu_weak_get(&kt_request.pawn)!=pawn || cu_weak_get(&kt_request.manager)!=manager
        || cu_weak_get(&kt_request.actor)!=actor) {touch_key_item_cancel();return 0;}
    kt_request.handle=handle;kt_request.phase=phase;kt_request.cursor=cursor;kt_request.start=now;
    if(kt_logs<16) {kt_logs++;LOGF("INTERACT key Check queued actor=%p handle=%llx phase=%u",actor,(unsigned long long)handle,phase);}
    return 1;
}
static void touch_key_item_idle_tick(void) {
    if(!kt_request.start || !cinema_on_game_thread())return;
    long now=ptr_now_ms();void* pawn=cu_weak_get(&kt_request.pawn);
    if(!pawn || now<kt_request.start || now-kt_request.start>300 || !kt_context(pawn))touch_key_item_cancel();
}
static void touch_key_item_tick(void* hand,int gameplay,int cancelled) {
    touch_key_item_idle_tick();
    // Inactive pooled hands must not cancel an active gameplay pawn's request.
    if(kt_request.start && phone_player && phone_player(hand)==cu_weak_get(&kt_request.pawn)
        && (!gameplay || cancelled))touch_key_item_cancel();
}
static int kt_confirm_result(void* manager,uintptr_t caller,long now,int original) {
    // The native Tick keeps this in X19; unrelated callers cannot consume the token.
    if(!kt_request.start || caller!=kt_base+0x62dc500 || !cinema_on_game_thread()
        || cu_weak_get(&kt_request.manager)!=manager)return original;
    void* pawn=cu_weak_get(&kt_request.pawn),*owner=cu_weak_get(&kt_request.actor);
    uint64_t handle=0;unsigned phase=0;
    int valid=now>=kt_request.start && now-kt_request.start<=300 && pawn && owner
        && cu_weak_get(&kt_request.manager)==manager && kt_context(pawn)
        && kt_pending_actor(manager,pawn,&handle,&phase)==owner && handle==kt_request.handle && phase==kt_request.phase
        && (phase!=2 || *(const int*)((const char*)manager+0x2a4)==kt_request.cursor);
    touch_key_item_cancel();
    if(valid && !original && kt_logs<16) {kt_logs++;LOGF("INTERACT key Check native confirm actor=%p phase=%u",owner,phase);}
    return original ? original:valid;
}

#if defined(__aarch64__) && !defined(RE4_KEY_TOUCH_TEST)
unsigned long g_tgt_key_confirm;
__attribute__((naked)) static int gate_key_confirm(void) {
    asm volatile(".inst 0xd10143ff\n" ".inst 0xa90257f6\n" ".inst 0xa9034ff4\n" ".inst 0xa9047bfd\n"
        "adrp x16, :got:g_tgt_key_confirm\n" "ldr x16, [x16, :got_lo12:g_tgt_key_confirm]\n"
        "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((used,noinline)) static int key_touch_confirm_hook(void* manager,uintptr_t caller) {
    int original=gate_key_confirm();
    return kt_confirm_result(manager,caller,ptr_now_ms(),original);
}
__attribute__((naked)) static int hook_key_confirm_dispatch(void) {
    asm volatile("mov x0,x19\n" "mov x1,x30\n" "b key_touch_confirm_hook\n");
}
#endif
static void install_touch_key_touch(uintptr_t base) {
    kt_ready=0;touch_key_item_cancel();
    if(!base || !ti_ready || !ti_manager || !ti_manager_class || !cu_weak_assign || !cu_weak_get)return;
    static const struct {uintptr_t at;unsigned words[2];} checks[]={
        {0x62dc2a4,{0xd10143ff,0xa90257f6}},{0x62dc2ac,{0xa9034ff4,0xa9047bfd}},
        {0x62dc4fc,{0x97ffff6a,0x36000880}},{0x645d984,{0xd10183ff,0xf90023f3}},
        {0x62f50d4,{0xb4000c61,0xbd41d021}},{0x62f5260,{0x900202e9,0xf9409808}},
        {0x6352430,{0x6dbd23e9,0xf9000bf3}},{0x62acc38,{0xf81e0ff3,0xa9017bfd}},
        {0x6341aec,{0xf9416c09,0xad4e8122}},{0x8ce93b8,{0xd10183ff,0xf90023f3}},
        {0x8f4451c,{0xd10183ff,0xf90023f3}},{0x62f5d30,{0xaa1f03e0,0xd65f03c0}},
        {0x8702bc4,{0xf81e0ff3,0xa9017bfd}},
        {0x62f6c18,{0x91006000,0x141ba16c}},{0x62f6c88,{0xf9401400,0xd65f03c0}},
        {0x62f24d4,{0xf81b0ff9,0xa9015ff8}},{0x62db848,{0xa9be4ff4,0xa9017bfd}},
        {0x62dbb9c,{0xf81d0ff5,0xa9014ff4}},
    };
    for(unsigned i=0;i<sizeof checks/sizeof *checks;i++)if(memcmp((const void*)(base+checks[i].at),checks[i].words,8)) {
        LOGF("KEYTOUCH disabled: native opcode mismatch +%lx",(unsigned long)checks[i].at);return;
    }
    if(*(const unsigned*)(base+0x9dcdb88+0x24)!=0x238
        || *(const unsigned*)(base+0x9dcd3a8+0x24)!=0x360
        || *(const unsigned*)(base+0x9dcd380+0x24)!=0x364
        || memcmp((const void*)(base+0x423f561),"OnFinishMessages",17)
        || *(const uintptr_t*)(base+0x9d2d420)!=base+0x62f5d30
        || *(const uintptr_t*)(base+0x9d2d500)!=base+0x62f6c18)return;
    kt_base=base;
    kt_receptor_class=(void*(*)(void))(base+0x645d984);
    kt_box_class=(void*(*)(void))(base+0x8ce93b8);
    kt_controller_class=(void*(*)(void))(base+0x8f4451c);
    kt_world=(void*(*)(const void*))(base+0x8702bc4);
    kt_weapon_hand=(void*(*)(const void*))(base+0x62acc38);
    kt_distance=(float(*)(const void*,const void*,const float*))(base+0x62f50d4);
    kt_looking=(int(*)(const float*,const struct pv_scene_tf*,float,unsigned char,float))(base+0x6352430);
#if defined(__aarch64__) && !defined(RE4_KEY_TOUCH_TEST)
    g_tgt_key_confirm=base+0x62dc2b4;
    if(patch_code((void*)(base+0x62dc2a4),(void*)hook_key_confirm_dispatch))return;
    kt_ready=1;interact_key_hook=touch_key_item_interact;interact_key_tick=touch_key_item_tick;
    LOGF("KEYTOUCH installed: explicit Check -> owned native next-message confirm");
#endif
}
#endif
