// Show toolbar shortcuts only for live authored SubAction handlers; the native touch controls stay authoritative.
#ifndef RE4FIX_UI_ACTIONS_H
#define RE4FIX_UI_ACTIONS_H
static int fv_actions_ready;
static uint64_t fv_action_snapshot;
static void* (*fv_title_find)(void*,void*,const unsigned short*,int);
static void* (*fv_title_actor)(const void*,void*);
struct fv_action_route {const unsigned char* code;int offset,count;};
static unsigned touch_menu_toolbar_mask(long now) {
    uint64_t snapshot=__atomic_load_n(&fv_action_snapshot,__ATOMIC_ACQUIRE);
    long stamp=(long)(snapshot>>8);
    return stamp>0 && now>=stamp && now-stamp<=400 && (((unsigned)snapshot>>4)&15)==(unsigned)g_touch_context
        ? (unsigned)snapshot&7 : 0;
}
static void* fv_action_function(void* object,const char* wanted) {
    if (!pv_alive(object) || !phone_inspect_ready) return 0;
    const char* cls=*(const char* const*)((const char*)object+0x10);
    for(unsigned depth=0;cls && depth<16;depth++) {
        const char* fn=*(const char* const*)(cls+0x48);
        for(unsigned i=0;fn && i<512;i++) {
            char name[96];phone_inspect_fname(fn+0x18,name,sizeof name);
            if (!strcmp(name,wanted)) {
                return pv_alive(fn) && cinema_ui_named(fn,"Function",0xc0) && *(const void* const*)(fn+0x20)==cls
                    && !*(const unsigned char*)(fn+0xb4) && !*(const unsigned short*)(fn+0xb6)
                    && *(const unsigned short*)(fn+0xb8)==0xffff ? (void*)fn : 0;
            }
            const char* next=*(const char* const*)(fn+0x28);if(next==fn)break;fn=next;
        }
        const char* next=*(const char* const*)(cls+0x40);if(next==cls)break;cls=next;
    }
    return 0;
}
static struct fv_action_route fv_action_route(void* receiver,void* function,unsigned depth) {
    struct fv_action_route none={0};
    if(depth>=8 || !pv_alive(function) || !cinema_ui_named(function,"Function",0xc0))return none;
    struct {const unsigned char* data;int count,max;} script;
    memcpy(&script,(const char*)function+0x60,sizeof script);
    if(!script.data || script.count<1 || script.count>1048576 || script.max<script.count || script.max>2097152)return none;
    int offset=0;
    // A Blueprint event wrapper points to its exact ubergraph entry. Follow only unconditional redirects.
    if(script.count==18 && script.data[0]==0x46 && script.data[9]==0x1d && script.data[14]==0x16
        && script.data[15]==0x04 && script.data[16]==0x0b && script.data[17]==0x53) {
        void* graph;memcpy(&graph,script.data+1,8);memcpy(&offset,script.data+10,4);
        if(!pv_alive(graph) || !cinema_ui_named(graph,"Function",0xc0))return none;
        memcpy(&script,(const char*)graph+0x60,sizeof script);
        if(!script.data || script.count<1 || script.count>1048576 || script.max<script.count || script.max>2097152)return none;
    } else if(script.count==13 && script.data[0]==0x1b && script.data[9]==0x16
              && script.data[10]==0x04 && script.data[11]==0x0b && script.data[12]==0x53) {
        char name[96];phone_inspect_fname(script.data+1,name,sizeof name);
        return fv_action_route(receiver,fv_action_function(receiver,name),depth+1);
    }
    int seen[16],count=0;
    while(offset>=0 && offset<script.count && script.data[offset]==0x06 && count<16) {
        if(offset>script.count-5)return none;
        for(int i=0;i<count;i++)if(seen[i]==offset)return none;
        seen[count++]=offset;memcpy(&offset,script.data+offset+1,4);
    }
    if(offset<0 || offset>=script.count || count==16 || script.data[offset]==0x53 || script.data[offset]==0x4d
        || (offset<=script.count-3 && script.data[offset]==0x04 && script.data[offset+1]==0x0b && script.data[offset+2]==0x53))return none;
    return (struct fv_action_route){script.data,offset,script.count};
}
static unsigned fv_action_mask(void* receiver) {
    struct fv_action_route a=fv_action_route(receiver,fv_action_function(receiver,"SubAction_A"),0);
    struct fv_action_route b=fv_action_route(receiver,fv_action_function(receiver,"SubAction_B"),0);
    unsigned mask=(a.code ? 1u:0u)|(b.code ? 2u:0u);
    if(mask==3 && ((a.code==b.code && a.offset==b.offset)
        || (a.count-a.offset==b.count-b.offset && !memcmp(a.code+a.offset,b.code+b.offset,(size_t)(a.count-a.offset)))))mask=2;
    return mask;
}
static int fv_action_bool_read(void* object,const char* wanted,int* value) {
    const char* field=cinema_field_property(object,wanted,"BoolProperty");
    const char* cls=object ? *(const char* const*)((const char*)object+0x10):0;
    int size=cls ? *(const int*)(cls+0x58):0,offset=field ? *(const int*)(field+0x4c):-1;
    if(!field || size<0x220 || size>0x10000 || offset<0x220 || offset>=size
        || *(const int*)(field+0x34)!=1 || *(const int*)(field+0x38)!=1)return 0;
    unsigned byte=*(const unsigned char*)(field+0x79),mask=*(const unsigned char*)(field+0x7b);
    // Native FBoolProperty::Identical/ExportText read ByteOffset+79 and FieldMask+7b.
    if(byte || !mask)return 0;
    *value=(*(const unsigned char*)((const char*)object+offset)&mask)!=0;return 1;
}
static int fv_action_bool(void* object,const char* wanted) {
    int value=0;return fv_action_bool_read(object,wanted,&value)&&value;
}
static void* fv_title_current(long now,unsigned type) {
    long lobby=__atomic_load_n(&g_lobby_menu_ms,__ATOMIC_ACQUIRE);
    long menu=__atomic_load_n(&g_menu_tick_ms,__ATOMIC_ACQUIRE);
    if(type!=7 || lobby<=0 || menu<=0 || now<lobby || now-lobby>=500 || now<menu || now-menu>=300
        || !phone_inspect_ready || !phone_active || !mv_pawn_class || !fv_title_find || !fv_title_actor)return 0;
    void* pawn=phone_active();
    if(!pv_alive(pawn) || !menu_visual_layout(pawn,mv_pawn_class(),0xbb0))return 0;
    static const char path[]="/Game/Blueprints/UI/Frontend/FrontendManager.FrontendManager_C";
    unsigned short wide[sizeof path];for(unsigned i=0;i<sizeof path;i++)wide[i]=(unsigned char)path[i];
    // Resolve the already loaded authored class; GetActorOfClass searches only this pawn's World.
    void* cls=fv_title_find(0,0,wide,0);
    if(!pv_alive(cls) || !cinema_ui_named(cls,"BlueprintGeneratedClass",0x100))return 0;
    void* actor=fv_title_actor(pawn,cls);
    return pv_alive(actor) && *(void* const*)((const char*)actor+0x10)==cls
        && cinema_ui_named(actor,"FrontendManager_C",0x220) ? actor:0;
}
static unsigned fv_title_action_mask(long now,unsigned type) {
    void* actor=fv_title_current(now,type);
    int ready,complete,skipping;
    if(!actor || !fv_action_bool(actor,"allowButtonInput?") || !fv_action_bool_read(actor,"ReadyForStart",&ready))return 0;
    if(ready)return 1;
    // Mask bit 2 labels the same native A action as Skip before the title accepts Start.
    return fv_action_bool_read(actor,"LogoIntroComplete",&complete) && !complete
        && fv_action_bool_read(actor,"SkipLogo",&skipping) && !skipping ? 5u:0u;
}
static int fv_title_skip_logos(void* actor,int recenter) {
    if(!actor || !phone_actor_event_address || !cinema_on_game_thread())return 0;
    const char* cls=*(const char* const*)((const char*)actor+0x10),*fn=*(const char* const*)(cls+0x48);
    for(unsigned i=0;fn&&i<256;i++) {
        char name[96];phone_inspect_fname(fn+0x18,name,sizeof name);
        if(!strcmp(name,"InpActEvt_Button A_K2Node_InputActionEvent_9")) {
            if(!pv_alive(fn)||!cinema_ui_named(fn,"Function",0xc0)||*(const void* const*)(fn+0x20)!=cls
                ||*(const unsigned char*)(fn+0xb4)!=1||*(const unsigned short*)(fn+0xb6)!=24
                ||*(const unsigned short*)(fn+0xb8)!=0xffff)return 0;
            struct {const unsigned char* data;int count,max;} script;
            memcpy(&script,fn+0x60,sizeof script);
            int entry=0;if(script.data&&script.count==36)memcpy(&entry,script.data+28,4);
            if(!script.data||script.count!=36||script.max<script.count||script.max>128||entry!=11555
                ||script.data[0]!=0x64||script.data[9]!=0||script.data[18]!=0x46||script.data[27]!=0x1d
                ||script.data[32]!=0x16||script.data[33]!=4||script.data[34]!=0x0b||script.data[35]!=0x53)return 0;
            const char* field=*(const char* const*)(fn+0x50);unsigned parameters=0;
            for(unsigned k=0;field&&k<32;k++) {
                uint64_t flags=*(const uint64_t*)(field+0x40);
                if(flags&0x80) {
                    char kind[48];phone_inspect_fname(*(const void* const*)(field+8),kind,sizeof kind);
                    phone_inspect_fname(field+0x28,name,sizeof name);
                    if(++parameters!=1||strcmp(kind,"StructProperty")||strcmp(name,"Key")
                        ||*(const int*)(field+0x34)!=1||*(const int*)(field+0x38)!=24
                        ||*(const int*)(field+0x4c)||(flags&0x500))return 0;
                }
                const char* next=*(const char* const*)(field+0x20);if(next==field)break;field=next;
            }
            void* const* table=*(void* const* const*)actor;
            if(parameters!=1||!table||table[0x218/sizeof(void*)]!=phone_actor_event_address)return 0;
            // FKey(None) is valid; this wrapper copies it then runs the original synchronous logo-skip handler.
            _Alignas(8) unsigned char key[24]={0};
            // A gameplay swipe must not become the new native menu's viewing direction.
            // Do this once at the verified return handler, before it advances the front end.
            if(recenter){g_look_yaw=0;g_look_pitch=0;}
            ((void(*)(void*,const void*,void*))phone_actor_event_address)(actor,fn,key);return 1;
        }
        const char* next=*(const char* const*)(fn+0x28);if(next==fn)break;fn=next;
    }
    return 0;
}
static int fv_gameplay_entered;
static void fv_title_intro_tick(long now,unsigned type) {
    if(!cinema_on_game_thread())return;
    if(type==1){fv_gameplay_entered=1;return;}
    if(!fv_gameplay_entered&&!mobile_prefs_intro_skip())return;
    void* actor=fv_title_current(now,type);
    int allowed,ready,complete,skipping;
    if(!actor||!fv_action_bool_read(actor,"allowButtonInput?",&allowed)||!allowed
        ||!fv_action_bool_read(actor,"ReadyForStart",&ready)||ready
        ||!fv_action_bool_read(actor,"LogoIntroComplete",&complete)||complete
        ||!fv_action_bool_read(actor,"SkipLogo",&skipping)||skipping)return;
    if(fv_title_skip_logos(actor,fv_gameplay_entered))LOGF("INTRO original Lobby logo skip returning=%d",fv_gameplay_entered);
}
static void fv_actions_publish(void* screen,long now) {
    unsigned mask=0,type=phone_type ? phone_type():0;
    if(fv_actions_ready && now>0 && cinema_on_game_thread() && !screen)mask=fv_title_action_mask(now,type);
    if(fv_actions_ready && now>0 && screen && pv_alive(screen) && cinema_on_game_thread()
        && fv_screen_class && menu_visual_layout(screen,fv_screen_class(),0x248)) {
        void* owner=fv_object_property(screen,"TrifoldBaseBP");
        if(!owner)owner=*(void* const*)((const char*)screen+0xe0);
        if(pv_alive(owner)) {
            char name[96];obj_name(*(void* const*)((const char*)owner+0x10),name,sizeof name);
            if(strncmp(name,"UI_PauseMenu_Content_",21))owner=0;
        } else owner=0;
        mask=fv_action_mask(owner ? owner:screen);
        void* cover=owner ? fv_object_property(owner,"CentralCoverScreen") : 0;
        // Trifold A only opens its cover; once the actual panels are shown the same action does nothing useful.
        if(cover && pv_alive(cover) && screen!=cover)mask&=~1u;
    }
    __atomic_store_n(&fv_action_snapshot,now>0 ? ((uint64_t)now<<8)|((type&15)<<4)|mask : 0,__ATOMIC_RELEASE);
}
static void install_ui_actions(uintptr_t base) {
    // Original ProcessEvent reads UStruct.Script.Data+60 / Num+68 before constructing its FFrame.
    fv_actions_ready=base && *(const unsigned*)(base+0x698e348)==0xb9406a88
        && *(const unsigned*)(base+0x698e450)==0xf9403289;
    static const unsigned find[4]={0xd10243ff,0xa9036ffc,0xa90467fa,0xa9055ff8};
    static const unsigned actor[4]={0xd10283ff,0xa90757f6,0xa9084ff4,0xa9097bfd};
    if(fv_actions_ready && !memcmp((const void*)(base+0x69bcbec),find,sizeof find)
        && !memcmp((const void*)(base+0x88945d8),actor,sizeof actor)
        && *(const unsigned*)(base+0x694a528)==0x3941e409 && *(const unsigned*)(base+0x694a540)==0x3941ec0a) {
        fv_title_find=(void* (*)(void*,void*,const unsigned short*,int))(base+0x69bcbec);
        fv_title_actor=(void* (*)(const void*,void*))(base+0x88945d8);
    }
}
#endif
