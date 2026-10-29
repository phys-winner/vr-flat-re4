// Include after weapon/interact. Native Settings owns the alternative equip mode.
#ifndef RE4FIX_QUICKSELECT_H
#define RE4FIX_QUICKSELECT_H
struct touch_quickselect_state { unsigned available; int selected,fire_side,mode; };
enum { QS_HELD_WORLD_GUN=4 };   // publication only; never an inventory slot/equip request
static const void* (*qs_settings)(void);
static void (*qs_copy)(void*,const void*),(*qs_set)(const void*),(*qs_destroy)(void*);
static void* (*qs_prop)(const void*,unsigned char),*(*qs_hand)(const void*,unsigned char);
static void* (*qs_native_hand)(const void*,unsigned char);
static void* (*qs_equip)(void*,unsigned char),*(*qs_gun_class)(void);
static const unsigned char* (*qs_prop_data)(void);
static int (*qs_allowed)(const void*),(*qs_base_allowed)(const void*),(*qs_context)(const void*);
static int qs_ready;
static unsigned long long qs_ui,qs_request;
static int qs_busy;static unsigned qs_pose_generation;
static void* qs_pawn;static unsigned long long qs_pawn_name;static unsigned qs_generation;
static struct { void* object[5];unsigned long long name[5];long until;int slot; } qs_pending;
static unsigned qs_logs;

static int qs_slot(int slot) { return slot==0 || slot==1 || slot==2 || slot==3 || slot==6; }
static int qs_gt(void) {
    int gt=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE);
    return gt>0 && gettid()==gt;
}
static unsigned long long qs_name(const void* object) {
    unsigned long long value=0;if(object)memcpy(&value,(const char*)object+0x18,8);return value;
}
static void qs_log(const char* event,int slot,void* item,void* hand) {
    if(qs_logs<16){++qs_logs;LOGF("QUICKSELECT %s slot=%d item=%p hand=%p",event,slot,item,hand);}
}
// GetProp and GetPrimaryHand perform the original UObject lifetime checks.
static void* qs_player(void* hand) {
    if(!qs_ready || !qs_gt() || !tw_typed(hand,tw_hand_class(),0x510)
        || *(const unsigned char*)((const char*)hand+0x314)>1 || phone_type()!=1) return 0;
    void* pawn=phone_player(hand);
    return pawn && pawn==phone_active() && tw_typed(pawn,tw_pawn_class(),0x10d0)
        && qs_native_hand && qs_native_hand(pawn,*(const unsigned char*)((const char*)hand+0x314))==hand ? pawn : 0;
}
static int qs_binding(void* pawn,int slot,void** objects) {
    if(!qs_slot(slot))return 0;
    void* item=qs_prop(pawn,(unsigned char)slot);
    if(!tw_typed(item,ti_prop_class(),0x4e0)
        || *(void* const*)((const char*)item+0x3c0)!=pawn
        || *(const unsigned char*)((const char*)item+0x2dc)!=slot)return 0;
    void* hand=qs_hand(pawn,(unsigned char)slot);
    void* primary=*(void* const*)((const char*)item+0x270);
    if(!tw_typed(hand,tw_hand_class(),0x510) || phone_player(hand)!=pawn
        || *(const unsigned char*)((const char*)hand+0x314)>1
        || !reload_grip_registered(primary) || !tw_typed(primary,tw_grip_class(),0x460)
        || *(void* const*)((const char*)primary+0x20)!=item
        || *(void* const*)((const char*)primary+0x300)!=item || !ti_parent(primary))return 0;
    objects[0]=pawn;objects[1]=item;objects[2]=hand;objects[3]=primary;
    objects[4]=*(void* const*)((const char*)item+0x10);
    return 1;
}
static int qs_can_equip(const void* item) {
    void* const* vtable=*(void* const* const*)item;
    return vtable && vtable[0x678/sizeof(void*)]
        && ((int (*)(const void*))vtable[0x678/sizeof(void*)])(item);
}
static int qs_current(void** objects) {
    return rl_primary(objects[1])==objects[2] && f_GetCurrentGrip(objects[2])==objects[3];
}
static int qs_mode(void) {
    const unsigned char* settings=qs_settings();
    return settings && settings[0x3d]<=1 ? settings[0x3d] : -1;
}
static int qs_equip_request(void* hand,void* pawn,unsigned long long name,int slot,void** objects) {
    const unsigned char* source=qs_settings();
    if(!source || source[0x3d]>1)return 0;
    // Only the native deep copy owns the arrays/text in the 0xb8-byte struct.
    struct __attribute__((aligned(16))) {unsigned char bytes[0xb8];} copy;
    int changed=source[0x3d]==0;
    if(changed){qs_copy(&copy,source);copy.bytes[0x3d]=1;qs_set(&copy);}
    int accepted=qs_mode()==1 && qs_player(hand)==pawn && qs_name(pawn)==name
        && !g_touch_cancel && g_touch_context==1 && !g_reload_phase && !g_touch_reload_request
        && qs_allowed(pawn) && qs_binding(pawn,slot,objects)
        && qs_context(objects[1]) && qs_can_equip(objects[1]);
    if(accepted)(void)qs_equip(pawn,(unsigned char)slot);
    if(changed){if(!accepted){copy.bytes[0x3d]=0;qs_set(&copy);}qs_destroy(&copy);}
    return accepted;
}
static struct touch_quickselect_state touch_quickselect_state(long now) {
    struct touch_quickselect_state out={0,-1,-1,0};
    unsigned long long value=__atomic_load_n(&qs_ui,__ATOMIC_ACQUIRE);
    if(!value || now<=0 || (unsigned)((unsigned)now-(unsigned)(value>>32))>500)return out;
    out.available=(unsigned)value&0x3ff;out.selected=((unsigned)value>>10&15)-1;
    out.fire_side=((unsigned)value>>14&3)-1;out.mode=((unsigned)value>>16)&1;return out;
}
static int touch_quickselect_request(int slot,long now) {
    unsigned long long snapshot=__atomic_load_n(&qs_ui,__ATOMIC_ACQUIRE);
    if(!qs_slot(slot) || !snapshot || now<=0 || g_touch_context!=1 || g_touch_cancel
        || (unsigned)((unsigned)now-(unsigned)(snapshot>>32))>500
        || !((unsigned)snapshot&(1u<<slot)))return 0;
    unsigned generation=(unsigned)snapshot>>17;
    unsigned long long request=((unsigned long long)(unsigned)now<<32)|(generation<<5)|(unsigned)(slot+1);
    __atomic_store_n(&qs_busy,1,__ATOMIC_RELEASE);__atomic_store_n(&qs_request,request,__ATOMIC_RELEASE);return 1;
}
static void touch_quickselect_cancel(void) {
    __atomic_store_n(&qs_request,0,__ATOMIC_RELEASE);__atomic_store_n(&qs_ui,0,__ATOMIC_RELEASE);
    __atomic_store_n(&qs_busy,0,__ATOMIC_RELEASE);
    __atomic_add_fetch(&qs_pose_generation,1,__ATOMIC_ACQ_REL);
}
static int touch_quickselect_ready(void) {return __atomic_load_n(&qs_ready,__ATOMIC_ACQUIRE);}
// Native QuickSelect drives the weapon icons only while it publishes fresh snapshots; without one (the first-run
// weapon-management tutorial keeps the world paused, so nothing is published) the icons use the holster grab.
static int touch_quickselect_live(long now) {
    unsigned long long value=__atomic_load_n(&qs_ui,__ATOMIC_ACQUIRE);
    return touch_quickselect_ready() && value && now>0 && (unsigned)((unsigned)now-(unsigned)(value>>32))<=500;
}
static int touch_quickselect_busy(void) {return __atomic_load_n(&qs_busy,__ATOMIC_ACQUIRE)
    || __atomic_load_n(&qs_request,__ATOMIC_ACQUIRE)!=0;}
static unsigned touch_quickselect_pose_generation(void) {return __atomic_load_n(&qs_pose_generation,__ATOMIC_ACQUIRE);}
static void* touch_equipped_gun(void* hand) {
    void* pawn=qs_player(hand);
    if(!pawn || !qs_gun_class)return 0;
    const unsigned char* data=qs_prop_data();
    for(int slot=0;slot<2;slot++) {
        if(!data || slot!=data[6])continue;
        void* gun=qs_prop(pawn,(unsigned char)slot);
        if(qs_hand(pawn,(unsigned char)slot)!=hand)continue;
        if(!tw_typed(gun,qs_gun_class(),0xd40) || *(void* const*)((const char*)gun+0x3c0)!=pawn
            || *(const unsigned char*)((const char*)gun+0x2dc)!=slot)continue;
        void* grip=*(void* const*)((const char*)gun+0x270);
        if(reload_grip_registered(grip) && tw_typed(grip,tw_grip_class(),0x460)
            && *(void* const*)((const char*)grip+0x20)==gun
            && *(void* const*)((const char*)grip+0x300)==gun
            && rl_primary(gun)==hand && f_GetCurrentGrip(hand)==grip)return gun;
    }
    // Range/world guns (not a pawn prop slot) held by their primary grip in this hand.
    void* grip=f_GetCurrentGrip(hand);
    if(grip && reload_grip_registered(grip) && tw_typed(grip,tw_grip_class(),0x460)) {
        void* gun=*(void* const*)((const char*)grip+0x20);
        if(tw_typed(gun,qs_gun_class(),0xd40) && *(void* const*)((const char*)gun+0x270)==grip
            && *(void* const*)((const char*)grip+0x300)==gun && rl_primary(gun)==hand
            && gun!=qs_prop(pawn,0) && gun!=qs_prop(pawn,1))return gun;
    }
    return 0;
}
static int touch_quickselect_world_retention(void* hand) {
    void* pawn=qs_player(hand),*gun=touch_equipped_gun(hand);
    if(!pawn || !gun || *(void* const*)((const char*)gun+0x3c0)!=pawn
        || gun==qs_prop(pawn,0) || gun==qs_prop(pawn,1) || g_touch_cancel || g_touch_context!=1
        || g_reload_phase || g_touch_reload_request || touch_quickselect_busy())return 0;
    int mode=qs_mode();
    if(mode!=0)return mode==1;
    unsigned long long pawn_name=qs_name(pawn),gun_name=qs_name(gun);
    struct __attribute__((aligned(16))) {unsigned char bytes[0xb8];} copy;
    qs_copy(&copy,qs_settings());copy.bytes[0x3d]=1;qs_set(&copy);
    // The same native Quick Select retention used by icon-equipped guns prevents a tap grab dropping on release.
    int accepted=qs_mode()==1 && qs_player(hand)==pawn && qs_name(pawn)==pawn_name
        && !g_touch_cancel && g_touch_context==1 && touch_equipped_gun(hand)==gun && qs_name(gun)==gun_name;
    if(!accepted){copy.bytes[0x3d]=0;qs_set(&copy);}
    qs_destroy(&copy);
    if(accepted)qs_log("world gun retained",QS_HELD_WORLD_GUN,gun,hand);
    return accepted;
}
static void touch_quickselect_tick(void* hand,long now) {
    if(!qs_gt() || !qs_ready || !hand || !tw_typed(hand,tw_hand_class(),0x510)
        || *(const unsigned char*)((const char*)hand+0x314)!=1)return;
    void* pawn=qs_player(hand);
    // A pooled hand cannot retire the actual current pawn's publisher.
    if(!pawn && phone_type()==1 && g_touch_context==1 && !g_touch_cancel
        && rl_paused && !rl_paused() && rl_player_valid && rl_player_valid()
        && qs_pawn && qs_pawn==phone_active() && tw_typed(qs_pawn,tw_pawn_class(),0x10d0)
        && qs_name(qs_pawn)==qs_pawn_name && qs_native_hand) {
        void* current=qs_native_hand(qs_pawn,1);
        if(current && current!=hand && qs_player(current)==qs_pawn)return;
    }
    // The first-run weapon tutorial keeps the world paused for Communicator/Game while the player practises
    // with the hands (tw_grab_paused); other pauses still retire the publisher. Its Immersive part checks real
    // holster grabs, so the icons fall back to them there; only its Quick Select part uses this publisher.
    if(!pawn || now<=0 || g_touch_context!=1 || g_touch_cancel || tw_grab_paused() || !rl_player_valid()
        || (rl_paused() && qs_mode()!=1)) {
        touch_quickselect_cancel();memset(&qs_pending,0,sizeof qs_pending);qs_pawn=0;return;
    }
    unsigned long long name=qs_name(pawn);
    if(qs_pawn!=pawn || qs_pawn_name!=name) {
        touch_quickselect_cancel();memset(&qs_pending,0,sizeof qs_pending);
        qs_pawn=pawn;qs_pawn_name=name;qs_generation=(qs_generation+1)&0x7fff;
        if(!qs_generation)qs_generation=1;
    }
    int mode=qs_mode(),selected=-1,side=-1;
    const unsigned char* data=qs_prop_data();int native_slot=data && data[6]<=9 ? data[6] : -1;
    unsigned available=0;
    int allowed=!g_reload_phase && !g_touch_reload_request
        && (qs_allowed(pawn) || (mode==0 && qs_base_allowed(pawn)));
    const int slots[]={0,1,2,3,6};
    for(unsigned i=0;i<sizeof slots/sizeof *slots;i++) {
        void* objects[5];int slot=slots[i];
        if(qs_binding(pawn,slot,objects)) {
            // Holster context authorizes a new selection. A live held PrimaryGrip
            // remains authoritative even while that selection gate is closed.
            if(allowed && qs_context(objects[1]) && qs_can_equip(objects[1]))available|=1u<<slot;
            // PropEquipped's native mask0x18f intentionally excludes Consumable6;
            // a real held consumable is authoritative while its previous gun slot is retained.
            if((slot==native_slot || slot==6) && qs_current(objects)){selected=slot;side=*(const unsigned char*)((const char*)objects[2]+0x314);}
        }
    }
    unsigned long long request=__atomic_exchange_n(&qs_request,0,__ATOMIC_ACQ_REL);
    if(request) {
        memset(&qs_pending,0,sizeof qs_pending);
        __atomic_add_fetch(&qs_pose_generation,1,__ATOMIC_ACQ_REL);
        int slot=((unsigned)request&31)-1;void* objects[5];
        if(((unsigned)request>>5)!=qs_generation
            || (unsigned)((unsigned)now-(unsigned)(request>>32))>500 || !allowed
            || !qs_binding(pawn,slot,objects) || !qs_context(objects[1]) || !qs_can_equip(objects[1])
            || !qs_equip_request(hand,pawn,name,slot,objects))qs_log("rejected",slot,0,0);
        else {
            // EquipProp preserves CanBeEquippedToSlot, original holster and PutInHand callbacks.
            for(int i=0;i<5;i++){qs_pending.object[i]=objects[i];qs_pending.name[i]=qs_name(objects[i]);}
            qs_pending.slot=slot;qs_pending.until=now+1800;qs_log("requested",slot,objects[1],objects[2]);
        }
        mode=qs_mode();
    }
    if(qs_pending.until) {
        void* objects[5];int same=qs_binding(pawn,qs_pending.slot,objects);
        for(int i=0;same && i<5;i++)same=objects[i]==qs_pending.object[i] && qs_name(objects[i])==qs_pending.name[i];
        if(!same || g_touch_cancel || g_reload_phase || now>=qs_pending.until || qs_pending.until-now>1800) {
            qs_log(same ? "timed out/cancelled" : "owner changed",qs_pending.slot,0,0);memset(&qs_pending,0,sizeof qs_pending);
        } else if(qs_current(objects) && (qs_pending.slot==6
            || ((data=qs_prop_data()) && data[6]==qs_pending.slot))) {
            selected=qs_pending.slot;side=*(const unsigned char*)((const char*)objects[2]+0x314);
            qs_log("confirmed",selected,objects[1],objects[2]);memset(&qs_pending,0,sizeof qs_pending);
        }
    }
    // Counter/challenge guns have no pawn prop slot: route Fire to their real
    // held primary grip without assigning a favourite or changing native readiness.
    if(!qs_pending.until)for(int s=0;s<2;s++) {
        void* held=qs_native_hand(pawn,(unsigned char)s),*gun=touch_equipped_gun(held);
        if(gun && gun!=qs_prop(pawn,0) && gun!=qs_prop(pawn,1)) {selected=QS_HELD_WORLD_GUN;side=s;}
    }
    if(!qs_pending.until && !__atomic_load_n(&qs_request,__ATOMIC_ACQUIRE))__atomic_store_n(&qs_busy,0,__ATOMIC_RELEASE);
    unsigned low=available|((unsigned)(selected+1)<<10)|((unsigned)(side+1)<<14)
        |((mode==1 ? 1u : 0u)<<16)|(qs_generation<<17);
    __atomic_store_n(&qs_ui,((unsigned long long)(unsigned)now<<32)|low,__ATOMIC_RELEASE);
}
static void install_touch_quickselect(unsigned long base) {
    static const struct {unsigned long addr;unsigned first[2];} check[]={
        {0x62c7e04,{0xa9be4ff4,0xa9017bfd}},{0x5fe1978,{0xf81c0ff7,0xa90157f6}},
        {0x62c80d4,{0xd105c3ff,0xf90093fc}},{0x62c7fd0,{0xa9bc5ff8,0xa90157f6}},
        {0x62a7ad4,{0x12001c28,0x7100251f}},{0x62a7b34,{0xd10143ff,0xf90013f5}},
        {0x62ab7fc,{0xa9bd57f6,0xa9014ff4}},{0x63c767c,{0xd100c3ff,0xa9014ff4}},
        {0x621c528,{0xf81e0ff3,0xa9017bfd}},
        {0x5f2b67c,{0xd00221a8,0xf9469908}},
        {0x62bc710,{0xaa0003e8,0xf941e800}},{0x6420bc0,{0xd10183ff,0xf90023f3}},
        {0x64390b0,{0x52801703,0x72a3fc24}},
        {0x62a8fc0,{0xf81e0ff3,0xa9017bfd}},
    };
    qs_ready=0;touch_quickselect_cancel();
    if(!base || !tw_hand_class || !tw_pawn_class || !tw_grip_class || !ti_prop_class || !ti_parent
        || !phone_player || !phone_active || !phone_type || !rl_primary || !f_GetCurrentGrip
        || !rl_paused || !rl_player_valid)return;
    if(tw_hand_class!=(void* (*)(void))(base+0x6424b5c)
        || tw_pawn_class!=(void* (*)(void))(base+0x642c718)
        || tw_grip_class!=(void* (*)(void))(base+0x64192f4)
        || ti_prop_class!=(void* (*)(void))(base+0x6432600)
        || ti_parent!=(void* (*)(const void*))(base+0x6280170)
        || phone_player!=(void* (*)(const void*))(base+0x633d030)
        || phone_active!=(void* (*)(void))(base+0x63380dc)
        || phone_type!=(unsigned (*)(void))(base+0x6338aec)
        || rl_primary!=(void* (*)(const void*))(base+0x62bbc30)
        || f_GetCurrentGrip!=(void* (*)(const void*))(base+0x629a774)
        || rl_paused!=(int (*)(void))(base+0x61d0934)
        || rl_player_valid!=(int (*)(void))(base+0x61d401c))return;
    for(unsigned i=0;i<sizeof check/sizeof *check;i++)
        if(memcmp((const void*)(base+check[i].addr),check[i].first,8)){
            LOGF("QUICKSELECT disabled: opcode mismatch +%lx",check[i].addr);return;
        }
    if(*(const unsigned*)(base+0x9db59b0+0x24)!=0x3d
        || *(const unsigned*)(base+0x9db0e60+0x24)!=0x270)return;
    qs_settings=(const void* (*)(void))(base+0x62c7e04);
    qs_copy=(void (*)(void*,const void*))(base+0x5fe1978);qs_set=(void (*)(const void*))(base+0x62c80d4);
    qs_destroy=(void (*)(void*))(base+0x62c7fd0);qs_prop=(void* (*)(const void*,unsigned char))(base+0x62a7ad4);
    qs_hand=(void* (*)(const void*,unsigned char))(base+0x62a7b34);qs_equip=(void* (*)(void*,unsigned char))(base+0x62ab7fc);
    qs_native_hand=(void* (*)(const void*,unsigned char))(base+0x62a8fc0);
    qs_allowed=(int (*)(const void*))(base+0x63c767c);qs_context=(int (*)(const void*))(base+0x62bc710);
    qs_base_allowed=(int (*)(const void*))(base+0x621c528);
    qs_prop_data=(const unsigned char* (*)(void))(base+0x5f2b67c);
    qs_gun_class=(void* (*)(void))(base+0x6420bc0);__atomic_store_n(&qs_ready,1,__ATOMIC_RELEASE);
    LOGF("QUICKSELECT ready: original assigned favorites and native mode/equip/retention");
}
#endif
