// Communicator input adaptation, run after native GamePlayerHand::Tick on GameThread.
// request 1 is an explicit player A press; request 2 logs state; request 3 inspects reflection.
static void* (*phone_player)(const void*);
static void* (*phone_active)(void);
static unsigned (*phone_type)(void);
static void* (*phone_class)(void);
static void* (*phone_actor)(const void*,void*);
static void* phone_actor_event_address;
static void (*phone_skip_line)(void*);   // AVR4Communicator::SkipToNextLine (native, checks Running itself)
#include "re4fix_phone_inspect.h"
#include <stdint.h>
static volatile uint64_t phone_answer_snapshot;
static volatile uint64_t phone_skip_snapshot;
// The overlay consumes a value snapshot; native objects are only read by the active communicator hand.
static int touch_phone_answer_visible(long now) {
    uint64_t snapshot=__atomic_load_n(&phone_answer_snapshot,__ATOMIC_ACQUIRE);
    long stamp=(long)(snapshot>>1);
    return g_touch_context==5 && stamp>0 && now>=stamp && now-stamp<=400 && (snapshot&1);
}
static int touch_phone_skip_visible(long now) {
    uint64_t snapshot=__atomic_load_n(&phone_skip_snapshot,__ATOMIC_ACQUIRE);
    long stamp=(long)(snapshot>>1);
    return g_touch_context==5 && stamp>0 && now>=stamp && now-stamp<=400 && (snapshot&1);
}
static int phone_communicator_layout(void* actor,void* expected) {
    const char* cls=actor ? *(const char* const*)((const char*)actor+0x10) : 0;
    if (!cls || !expected) return 0;
    int size=*(const int*)(cls+0x58),depth=*(const int*)((const char*)expected+0x38);
    int actual=*(const int*)(cls+0x38);
    const void* const* chain=*(const void* const* const*)(cls+0x30);
    return size>=0x738 && size<=0x10000 && depth>=0 && depth<=128 && actual>=depth && actual<=256
        && chain && chain[depth]==(const char*)expected+0x30;
}
static void phone_answer_sample(void* actor,long now) {
    int visible=actor && !*(const unsigned char*)((const char*)actor+0x734)
        && !*(const unsigned char*)((const char*)actor+0x735)
        && !__atomic_load_n(&g_phone_zoom_start,__ATOMIC_ACQUIRE);
    __atomic_store_n(&phone_answer_snapshot,actor && now>0 ? ((uint64_t)now<<1)|(unsigned)visible : 0,__ATOMIC_RELEASE);
    int skippable=actor && phone_skip_line && *(const unsigned char*)((const char*)actor+0x734)
        && !*(const unsigned char*)((const char*)actor+0x735);
    __atomic_store_n(&phone_skip_snapshot,actor && now>0 ? ((uint64_t)now<<1)|(unsigned)skippable : 0,__ATOMIC_RELEASE);
}

static int touch_menu_active(void* hand) {
    if (!hand || !phone_player || !phone_active) return 0;
    void* pawn = phone_player(hand);
    return pawn && pawn == phone_active();
}

static void* phone_device(void* pawn) {
    if (!phone_inspect_ready || !pawn) return 0;
    void* cls=*(void**)((char*)pawn+0x10);
    char kind[96]=""; obj_name(cls,kind,sizeof kind);
    if (strcmp(kind,"VR4CommunicatorPlayerPawn_BP_C")) return 0;
    int bytes=*(const int*)((char*)cls+0x58);
    if (bytes<8 || bytes>0x10000) return 0;
    void* prop=*(void**)((char*)cls+0x70);
    for (unsigned i=0;prop && i<256;i++) {
        char name[64]="",type[48]="";
        phone_inspect_fname((char*)prop+0x28,name,sizeof name);
        if (!strcmp(name,"Device")) {
            phone_inspect_fname(*(void**)((char*)prop+0x8),type,sizeof type);
            int off=*(const int*)((char*)prop+0x4c);
            if (strcmp(type,"ObjectProperty") || *(const int*)((char*)prop+0x38)!=8
                || *(const int*)((char*)prop+0x34)!=1 || off<0 || off>bytes-8) return 0;
            void* device=*(void**)((char*)pawn+off);
            if (!device) return 0;
            obj_name(*(void**)((char*)device+0x10),kind,sizeof kind);
            return !strcmp(kind,"CommunicatorDevice_C") ? device : 0;
        }
        void* next=*(void**)((char*)prop+0x58);
        if (next==prop) break;
        prop=next;
    }
    return 0;
}
static int phone_button_shortcut(void* device) {
    if (!device || !phone_actor_event_address) return 0;
    void* cls=*(void**)((char*)device+0x10);
    void* fn=*(void**)((char*)cls+0x48);
    for (unsigned i=0;fn && i<96;i++) {
        char name[96]="",kind[48]="";
        obj_name(fn,name,sizeof name);
        if (!strcmp(name,"ButtonShortcut")) {
            obj_name(*(void**)((char*)fn+0x10),kind,sizeof kind);
            if (strcmp(kind,"Function") || *(void**)((char*)fn+0x20)!=cls
                || *(const unsigned*)((char*)fn+0xb0)!=0x0c000000
                || *(const unsigned char*)((char*)fn+0xb4)!=0
                || *(const unsigned short*)((char*)fn+0xb6)!=0
                || *(const unsigned short*)((char*)fn+0xb8)!=0xffff) return 0;
            void** vtable=*(void***)device;
            if (!vtable || vtable[0x218/sizeof(void*)]!=phone_actor_event_address) return 0;
            ((void (*)(void*,void*,void*))vtable[0x218/sizeof(void*)])(device,fn,0);
            return 1;
        }
        void* next=*(void**)((char*)fn+0x28);
        if (next==fn) break;
        fn=next;
    }
    return 0;
}
static void touch_phone_tick(void* hand) {
    if (!phone_type || !phone_player || !phone_active || !hand || *(const unsigned char*)((char*)hand+0x314)!=1) return;
    unsigned type=phone_type();
    __atomic_store_n(&g_touch_context,(int)type,__ATOMIC_RELEASE);
    if (type!=5) {
        __atomic_store_n(&g_phone_zoom_start,0,__ATOMIC_RELEASE);
        __atomic_store_n(&phone_answer_snapshot,0,__ATOMIC_RELEASE);
        __atomic_store_n(&phone_skip_snapshot,0,__ATOMIC_RELEASE);
    }
    int request=__atomic_load_n(&g_phone_request,__ATOMIC_ACQUIRE);
    if (!request && type!=5) return;
    void* pawn=phone_player(hand);
    // Inactive gameplay hands also Tick during pawn transitions. Only the active
    // communicator hand may consume A; leave its request queued for that hand.
    if (request==3 && type==3) pawn=phone_active();
    if ((type==5 || request==3) && (!pawn || pawn!=phone_active())) return;
    void* actor=0;
    if (type==5) {
        void* cls=phone_class ? phone_class() : 0;
        actor=cls && phone_actor ? phone_actor(pawn,cls) : 0;
        if (!phone_communicator_layout(actor,cls)) actor=0;
        phone_answer_sample(actor,ptr_now_ms());
    }
    if (!request) return;
    request=__atomic_exchange_n(&g_phone_request,0,__ATOMIC_ACQ_REL);
    if (!request) return;
    if (request==3 && type==3) {
        phone_inspect_objects(pawn,cinema_subtitle_probe(pawn),hand);
        return;
    }
    if (request==3 && type==1) {
        void* cls=*(void**)((char*)pawn+0x10);
        char kind[96]=""; obj_name(cls,kind,sizeof kind);
        int bytes=cls ? *(const int*)((char*)cls+0x58) : 0;
        if (!strcmp(kind,"VR4Bio4PlayerPawn_BP_C") && bytes>=0xbb0 && bytes<=0x10000)
            phone_inspect_objects(pawn,*(void**)((char*)pawn+0xba8),hand);
        return;
    }
    if (type!=5) {
        LOGF("PHONE request=%d ignored: active type=%u pawn=%p",request,type,pawn); return;
    }
    if (request==3) {
        phone_inspect_objects(pawn,actor,hand);
        phone_view_inspect(pawn,phone_device(pawn));
        return;
    }
    if (!actor) { LOGF("PHONE request=%d: no Communicator actor",request); return; }
    char name[64]="";
    obj_name(actor,name,sizeof name);
    int running=*(const unsigned char*)((char*)actor+0x734);
    int finished=*(const unsigned char*)((char*)actor+0x735);
    int line=*(const int*)((char*)actor+0x718), count=*(const int*)((char*)actor+0x71c);
    const void* data=*(void* const*)((char*)actor+0x708);
    LOGF("PHONE request=%d actor=%p '%s' running=%d finished=%d line=%d count=%d time=%.3f data=%p audio=%p",
         request,actor,name,running,finished,line,count,*(const float*)((char*)actor+0x720),
         data,*(void* const*)((char*)actor+0x6f0));
    if (request!=1) return;
    if (running && !finished && phone_skip_line) {
        // A tap during the conversation skips the current line, like the original face button.
        phone_skip_line(actor);
        LOGF("PHONE tap -> native SkipToNextLine line=%d/%d -> %d",line,count,*(const int*)((char*)actor+0x718));
        return;
    }
    void* device=phone_device(pawn);
    if (!phone_button_shortcut(device)) {
        LOGF("PHONE physical button ignored: Device/receiver metadata mismatch"); return;
    }
    if (!__atomic_load_n(&g_phone_zoom_start,__ATOMIC_ACQUIRE))
        __atomic_store_n(&g_phone_zoom_start,ptr_now_ms(),__ATOMIC_RELEASE);
    phone_answer_sample(actor,ptr_now_ms());
    LOGF("PHONE action=Device.ButtonShortcut device=%p -> running=%d line=%d finished=%d audio=%p",device,
         *(const unsigned char*)((char*)actor+0x734),*(const int*)((char*)actor+0x718),
         *(const unsigned char*)((char*)actor+0x735),*(void* const*)((char*)actor+0x6f0));
}
static void install_touch_phone(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[2]; } check[] = {
        {0x633d030,{0xf9417800,0xd65f03c0}}, {0x63380dc,{0xd0020168,0xf9431508}},
        {0x6338aec,{0xd0020168,0xf9431508}}, {0x6325c14,{0xd103c3ff,0xa9096ffc}},
        {0x63d9da8,{0xd10183ff,0xf90023f3}}, {0x87037cc,{0xa9bd57f6,0xa9014ff4}},
        {0x698e2cc,{0xa9bb67fa,0xa9015ff8}},
        {0x6233fc8,{0x395cd268,0x34000a28}}, {0x62345f0,{0x395cd008,0x34000088}},
    };
    for (unsigned i=0;i<sizeof check/sizeof check[0];i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,sizeof check[i].first)) {
            LOGF("PHONE disabled: opcode mismatch +%lx",check[i].addr); return;
        }
    phone_player=(void* (*)(const void*))(base+0x633d030);
    phone_active=(void* (*)(void))(base+0x63380dc);
    phone_class=(void* (*)(void))(base+0x63d9da8);
    phone_actor=(void* (*)(const void*,void*))(base+0x6325c14);
    phone_actor_event_address=(void*)(base+0x87037cc);
    phone_type=(unsigned (*)(void))(base+0x6338aec);
    static const unsigned skip_words[3]={0x1e2703e0,0x2a1f03e1,0x17fffe78};
    if (!memcmp((const void*)(base+0x6234878),skip_words,sizeof skip_words))
        phone_skip_line=(void (*)(void*))(base+0x6234878);
    install_phone_inspect(base);
    LOGF("PHONE adaptation installed (explicit A only)");
}
