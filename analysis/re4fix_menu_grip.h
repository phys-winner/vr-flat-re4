// Include after re4fix_input.h. Called after native MenuHand Tick, outside g_imx.
#ifndef RE4FIX_MENU_GRIP_H
#define RE4FIX_MENU_GRIP_H

struct mg_ui_object { void* actor; void* component; void* screen; };
struct mg_ui_hit { float position[3], distance; };
_Static_assert(sizeof(struct mg_ui_object)==24,"native FVR4UIObject ABI");
_Static_assert(sizeof(struct mg_ui_hit)==16,"native FVR4UIHitResult ABI");
static void* (*mg_hand_class)(void), *(*mg_pawn_class)(void);
static void* (*mg_button_class)(void), *(*mg_screen_class)(void);
static int (*mg_hover)(struct mg_ui_object*,struct mg_ui_hit*,void*);
static int (*mg_enabled)(const void*);
static void (*mg_press)(void*), (*mg_release)(void*);
static void* mg_actor_event_address;
static void* mg_visible_address;
static struct {
    void* hand; void* pawn;
    unsigned long long hand_name,pawn_name;
    unsigned serial;
} mg_owned;
static void* mg_seen_hand;
static void* mg_seen_pawn;
static unsigned long long mg_seen_hand_name,mg_seen_pawn_name;
static unsigned mg_logs;

static int mg_layout(const void* object,const void* expected,int minimum) {
    const char* cls=object ? *(const char* const*)((const char*)object+0x10) : 0;
    if (!cls || !expected) return 0;
    int bytes=*(const int*)(cls+0x58), expected_bytes=*(const int*)((const char*)expected+0x58);
    int depth=*(const int*)((const char*)expected+0x38), actual_depth=*(const int*)(cls+0x38);
    const void* const* chain=*(const void* const**)(cls+0x30);
    return bytes>=minimum && bytes<=0x10000 && expected_bytes>=minimum && expected_bytes<=bytes
        && depth>=0 && depth<=128 && actual_depth>=depth && actual_depth<=256
        && chain && chain[depth]==(const char*)expected+0x30;
}
static unsigned long long mg_name(const void* object) {
    unsigned long long name;
    memcpy(&name,(const char*)object+0x18,sizeof name);
    return name;
}
static int mg_grip_receiver(void* hand) {
    struct mg_ui_object object={0,0,0};
    struct mg_ui_hit hit={{0,0,0},0};
    if (!mg_hover(&object,&hit,hand) || !object.actor || !object.screen
        || !mg_layout(object.component,mg_button_class(),0x330)
        || !mg_layout(object.screen,mg_screen_class(),0x248)) return 0;
    const char* component=(const char*)object.component;
    void** vtable=*(void***)object.component;
    return *(void* const*)(component+0xa0)==object.actor
        && *(void* const*)(component+0x308)==object.screen
        && *(const unsigned char*)(component+0x320)==2
        && vtable && vtable[0x470/sizeof(void*)]==mg_visible_address
        && mg_enabled(object.component);
}
static int mg_current(void* hand,void** pawn_out,unsigned* type_out) {
    int gt=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE);
    if (gt<=0 || gettid()!=gt || !__atomic_load_n(&g_menu_grip_ready,__ATOMIC_ACQUIRE)
        || !phone_type || !phone_player || !phone_active || !hand) return 0;
    unsigned type=phone_type();
    if ((type!=2 && type!=4 && type!=7) || !mg_layout(hand,mg_hand_class(),0x570)
        || *(const unsigned char*)((const char*)hand+0x314)!=1) return 0;
    void* pawn=phone_player(hand);
    if (!pawn || pawn!=phone_active() || !mg_layout(pawn,mg_pawn_class(),0xbb0)) return 0;
    void** vtable=*(void***)hand;
    if (!vtable || vtable[0x218/sizeof(void*)]!=mg_actor_event_address) return 0;
    *pawn_out=pawn; *type_out=type;
    return 1;
}
// Explicit Back calls this after its owned request is consumed, before TryUnpause.
static void touch_menu_grip_cancel(void* hand) {
    void* pawn=0; unsigned type=0;
    if (!mg_current(hand,&pawn,&type)) return;
    struct menu_pointer_request request;
    touch_menu_pointer_snapshot(&request);
    touch_menu_pointer_choose(request.serial,MP_CANCEL);
    int release=mg_owned.hand==hand && mg_owned.pawn==pawn
        && mg_owned.hand_name==mg_name(hand) && mg_owned.pawn_name==mg_name(pawn);
    memset(&mg_owned,0,sizeof mg_owned);
    if (release) {
        mg_release(hand);
        if (mg_logs<8) { mg_logs++; LOGF("MENU GRIP release before Back hand=%p type=%u",hand,type); }
    }
}
static void touch_menu_grip_tick(void* hand) {
    void* pawn=0; unsigned type=0;
    if (!mg_current(hand,&pawn,&type)) return;
    unsigned long long hand_name=mg_name(hand),pawn_name=mg_name(pawn);
    struct menu_pointer_request request;
    touch_menu_pointer_snapshot(&request);
    // Compare only validated current identities; former pointers remain opaque.
    int changed=mg_seen_hand && (mg_seen_hand!=hand || mg_seen_pawn!=pawn
        || mg_seen_hand_name!=hand_name || mg_seen_pawn_name!=pawn_name);
    mg_seen_hand=hand; mg_seen_pawn=pawn;
    mg_seen_hand_name=hand_name; mg_seen_pawn_name=pawn_name;
    if (changed) {
        touch_menu_pointer_choose(request.serial,MP_CANCEL);
        memset(&mg_owned,0,sizeof mg_owned);
        return;
    }
    if (mg_owned.hand) {
        if (mg_owned.hand!=hand || mg_owned.pawn!=pawn || mg_owned.hand_name!=hand_name
            || mg_owned.pawn_name!=pawn_name) { memset(&mg_owned,0,sizeof mg_owned); return; }
        if (!request.active || request.serial!=mg_owned.serial || request.action!=MP_GRIP) {
            memset(&mg_owned,0,sizeof mg_owned);
            mg_release(hand);
            if (mg_logs<8) { mg_logs++; LOGF("MENU GRIP release hand=%p type=%u",hand,type); }
        }
        return;
    }
    if (!request.active || request.pressed || request.action==MP_TRIGGER || now_ms()<request.press_ms) return;
    int action=mg_grip_receiver(hand) ? MP_GRIP : MP_TRIGGER;
    if (!touch_menu_pointer_choose(request.serial,action)) return;
    struct menu_pointer_request current;
    touch_menu_pointer_snapshot(&current);
    if (!current.active || current.serial!=request.serial || current.action!=MP_GRIP || !current.pressed) return;
    mg_owned.hand=hand; mg_owned.pawn=pawn; mg_owned.hand_name=hand_name;
    mg_owned.pawn_name=pawn_name; mg_owned.serial=request.serial;
    mg_press(hand);
    if (mg_logs<8) { mg_logs++; LOGF("MENU GRIP press hand=%p type=%u gesture=%u",hand,type,request.serial); }
}
static void install_touch_menu_grip(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[2]; } check[] = {
        {0x63769bc,{0xf81b0ff9,0xa9015ff8}}, {0x6371fdc,{0xf81e0ff3,0xa9017bfd}},
        {0x6477678,{0xd10183ff,0xf90023f3}}, {0x64781dc,{0xd10183ff,0xf90023f3}},
        {0x64d8eec,{0xd10183ff,0xf90023f3}}, {0x64db798,{0xd10183ff,0xf90023f3}},
        {0x633c54c,{0x52800028,0x390cf408}}, {0x633c558,{0x390cf41f,0x1405d0a2}},
        {0x64b07a8,{0xf81e0ff3,0xa9017bfd}}, {0x64b07e4,{0xf81e0ff3,0xa9017bfd}},
        {0x87037cc,{0xa9bd57f6,0xa9014ff4}}, {0x637119c,{0x39400028,0x394c8009}},
        {0x83ad924,{0x79429808,0x52808409}}, {0x6477700,{0x5280ae04,0x52800205}},
        {0x637139c,{0xf9418400,0xd65f03c0}}, {0x633d030,{0xf9417800,0xd65f03c0}},
        {0x63380dc,{0xd0020168,0xf9431508}}, {0x6338aec,{0xd0020168,0xf9431508}},
        {0x69afeec,{0xf9000c01,0xf001cd08}}, {0x6376b48,{0x3dc00100,0xf9400908}},
        {0x6376b64,{0x3dc00120,0x52800020}}, {0x6376b6c,{0x3d800260,0x17ffffde}},
    };
    static const struct { unsigned long params; unsigned offset; } fields[] = {
        {0x9e0fb28,0x320}, {0x9e10ed8,0x308}, {0x9e10ea8,0x310},
        {0x9e10370,0}, {0x9e10340,8}, {0x9e10310,0x10},
        {0x9e10098,0}, {0x9e10070,0xc},
    };
    if (!base || __atomic_load_n(&g_menu_grip_ready,__ATOMIC_ACQUIRE)) return;
    for (unsigned i=0;i<sizeof check/sizeof *check;i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,sizeof check[i].first)) {
            LOGF("MENU GRIP disabled: opcode mismatch +%lx",check[i].addr); return;
        }
    for (unsigned i=0;i<sizeof fields/sizeof *fields;i++)
        if (*(const unsigned*)(base+fields[i].params+0x24)!=fields[i].offset) {
            LOGF("MENU GRIP disabled: property mismatch +%lx",fields[i].params); return;
        }
    const char* name=*(const char* const*)(base+0x9e0fee0);
    if (!name || strcmp(name,"EVR4UIAction::Grip") || *(const long long*)(base+0x9e0fee8)!=2
        || phone_player!=(void* (*)(const void*))(base+0x633d030)
        || phone_active!=(void* (*)(void))(base+0x63380dc)
        || phone_type!=(unsigned (*)(void))(base+0x6338aec)) return;
    mg_hand_class=(void* (*)(void))(base+0x6477678);
    mg_pawn_class=(void* (*)(void))(base+0x64781dc);
    mg_button_class=(void* (*)(void))(base+0x64d8eec);
    mg_screen_class=(void* (*)(void))(base+0x64db798);
    mg_hover=(int (*)(struct mg_ui_object*,struct mg_ui_hit*,void*))(base+0x63769bc);
    mg_enabled=(int (*)(const void*))(base+0x6371fdc);
    mg_press=(void (*)(void*))(base+0x633c54c);
    mg_release=(void (*)(void*))(base+0x633c558);
    mg_actor_event_address=(void*)(base+0x87037cc); mg_visible_address=(void*)(base+0x83ad924);
    __atomic_store_n(&g_menu_grip_ready,1,__ATOMIC_RELEASE);
    LOGF("MENU GRIP ready: owned GT native InputGrip, authored hovered UIButton action, gameplay RG retained");
}
#endif
