// QA-only equivalent of SetWatchSetting's explicit Watch.UpdateArm call, on this pawn's existing actor.
#ifndef RE4_QA_WATCH_BINDING_H
#define RE4_QA_WATCH_BINDING_H
#ifdef RE4_QA_USER_TRANSITIONS
#include <stdint.h>
#include <string.h>
static unsigned qa_watch_binding_logs;
static struct {
    void* pawn;void* actor;void* function;
    int requested,settings_before,settings_after,arm_before,arm_after;
    uint64_t socket_before,socket_after;
    int prevalidated,settings_accepted,callback,attached;
    const char* reason;
} qa_watch_binding_last;
struct qa_watch_binding {
    void* pawn;void* actor;void* mesh;void* root;
    const char* arm_field;const char* socket_field[2];
    int arm;uint64_t socket,configured[2];
    const char* reason;
};
static const char* qa_watch_property(void* object,const char* name,const char* kind,int bytes) {
    if(!pv_alive(object))return 0;
    const char* cls=*(const char* const*)((const char*)object+0x10);
    int size=cls?*(const int*)(cls+0x58):0;
    const char* field=cinema_field_property(object,name,kind);
    int offset=field?*(const int*)(field+0x4c):-1;
    return field && size>=0x220 && size<=0x10000 && offset>=0x28 && offset<=size-bytes
        && *(const int*)(field+0x34)==1 && *(const int*)(field+0x38)==bytes?field:0;
}
static void* qa_watch_object(void* object,const char* name) {
    const char* field=qa_watch_property(object,name,"ObjectProperty",8);
    return field?*(void* const*)((const char*)object+*(const int*)(field+0x4c)):0;
}
static int qa_watch_component(void* component,void* owner,void* cls,int minimum) {
    return pv_alive(component) && menu_visual_layout(component,cls,minimum)
        && *(void* const*)((const char*)component+0x20)==owner
        && *(void* const*)((const char*)component+0xa0)==owner;
}
static int qa_watch_read_binding(void* pawn,struct qa_watch_binding* out) {
    memset(out,0,sizeof *out);
    out->reason="pawn";
    if(!pv_alive(pawn) || !cinema_ui_named(pawn,"VR4Bio4PlayerPawn_BP_C",0x10d0))return 0;
    out->pawn=pawn;out->reason="watch_owner";
    void* actor=qa_watch_object(pawn,"watchActor");
    if(!pv_alive(actor) || !cinema_ui_named(actor,"UI_WristWatch_BP_C",0x220)
        || qa_watch_object(actor,"Pawn")!=pawn)return 0;
    out->actor=actor;out->reason="meshset";
    void* meshset=qa_watch_object(pawn,"MeshSet");
    if(!pv_alive(meshset) || !menu_visual_layout(meshset,mv_meshset_class(),0x238))return 0;
    out->reason="mesh";
    void* mesh=qa_watch_object(meshset,"Mesh");
    if(mesh!=qa_watch_object(actor,"Mesh") || !qa_watch_component(mesh,meshset,mv_skeletal_class(),0x438))return 0;
    out->reason="root";
    void* root=*(void* const*)((const char*)actor+0x130);
    if(!qa_watch_component(root,actor,mv_scene_class(),0x1f0)
        || *(void* const*)((const char*)root+0xc0)!=mesh)return 0;
    out->reason="fields";
    const char* arm=qa_watch_property(actor,"ActiveArm","EnumProperty",1);
    const char* left=qa_watch_property(actor,"LeftWristSocket","NameProperty",8);
    const char* right=qa_watch_property(actor,"RightWristSocket","NameProperty",8);
    if(!arm || !left || !right)return 0;
    out->pawn=pawn;out->actor=actor;out->mesh=mesh;out->root=root;
    out->arm_field=arm;out->socket_field[0]=left;out->socket_field[1]=right;
    out->arm=*((const unsigned char*)actor+*(const int*)(arm+0x4c));
    out->reason="active_arm";
    if((unsigned)out->arm>1)return 0;
    memcpy(&out->configured[0],(const char*)actor+*(const int*)(left+0x4c),8);
    memcpy(&out->configured[1],(const char*)actor+*(const int*)(right+0x4c),8);
    memcpy(&out->socket,(const char*)root+0xc8,8);
    out->reason="socket";
    char names[2][64];
    for(unsigned i=0;i<2;i++) {
        phone_inspect_fname(&out->configured[i],names[i],sizeof names[i]);
        if(!names[i][0] || !strcmp(names[i],"None"))return 0;
    }
    return out->configured[0]!=out->configured[1] && out->socket==out->configured[out->arm];
}
static int qa_watch_refuse(const char* reason) {
    qa_watch_binding_last.reason=reason;
    if(qa_watch_binding_logs<8) {
        qa_watch_binding_logs++;
        LOGF("QA WATCH refused requested=%d pawn=%p actor=%p reason=%s",qa_watch_binding_last.requested,
             qa_watch_binding_last.pawn,qa_watch_binding_last.actor,reason);
    }
    return 0;
}
static void* qa_watch_own_function(void* actor,const char* wanted) {
    const char* cls=*(const char* const*)((const char*)actor+0x10);
    const char* fn=*(const char* const*)(cls+0x48);
    for(unsigned i=0;fn && i<256;i++) {
        if(!pv_alive(fn) || !cinema_ui_named(fn,"Function",0xc0))return 0;
        char name[64];phone_inspect_fname(fn+0x18,name,sizeof name);
        if(!strcmp(name,wanted))return *(const void* const*)(fn+0x20)==cls?(void*)fn:0;
        const char* next=*(const char* const*)(fn+0x28);if(next==fn)return 0;fn=next;
    }
    return 0;
}
static void* qa_watch_update_function(void* actor) {
    const char* fn=qa_watch_own_function(actor,"UpdateArm");
    const char* attach=qa_watch_own_function(actor,"AttachToArm");
    if(!fn || !attach || *(const unsigned*)(fn+0xb0)!=0x0c420000u
        || *(const unsigned char*)(fn+0xb4)!=2 || *(const unsigned short*)(fn+0xb6)!=2
        || *(const unsigned short*)(fn+0xb8)!=0xffff)return 0;
    const char* field=*(const char* const*)(fn+0x50);const char* params[2]={0,0};unsigned count=0;
    for(unsigned i=0;field && i<16;i++) {
        uint64_t flags=*(const uint64_t*)(field+0x40);
        if(flags&0x80) {
            char kind[48],name[48];phone_inspect_fname(*(const void* const*)(field+8),kind,sizeof kind);
            phone_inspect_fname(field+0x28,name,sizeof name);
            int which=!strcmp(name,"NewHand")?0:!strcmp(name,"Callback")?1:-1;
            if(which<0 || params[which] || ++count>2 || *(const int*)(field+0x34)!=1
                || *(const int*)(field+0x38)!=1 || *(const int*)(field+0x4c)!=which
                || strcmp(kind,which?"BoolProperty":"EnumProperty")
                || (flags&0x500u)!=(which?0x100u:0))return 0;
            if(which && (*(const unsigned char*)(field+0x79) || *(const unsigned char*)(field+0x7a)!=1
                || (*(const unsigned char*)(field+0x7b)!=1 && *(const unsigned char*)(field+0x7b)!=0xff)))return 0;
            params[which]=field;
        }
        const char* next=*(const char* const*)(field+0x20);if(next==field)return 0;field=next;
    }
    if(field || count!=2 || !params[0] || !params[1])return 0;
    struct {const unsigned char* data;int count,max;} script;memcpy(&script,fn+0x60,sizeof script);
    const unsigned char* code=script.data;
    if(!code || script.count!=37 || script.max<script.count || script.max>128
        || code[0]!=0x45 || code[13]!=0 || code[22]!=0x16 || code[23]!=0x14 || code[24]!=0x48
        || code[33]!=0x27 || code[34]!=4 || code[35]!=0x0b || code[36]!=0x53)return 0;
    const void* input;const void* output;uint32_t called[3],target[2];
    memcpy(&input,code+14,8);memcpy(&output,code+25,8);memcpy(called,code+1,12);memcpy(target,attach+0x18,8);
    // Original execLocalVirtualFunction +6994518/+699451c consumes FScriptName comparison/number at0/+8.
    if(input!=params[0] || output!=params[1] || called[0]!=target[0] || called[2]!=target[1])return 0;
    void* const* table=*(void* const* const*)actor;
    return table && table[0x218/sizeof(void*)]==phone_actor_event_address?(void*)fn:0;
}
static int qa_watch_set_and_attach(int side) {
    memset(&qa_watch_binding_last,0,sizeof qa_watch_binding_last);
    qa_watch_binding_last.requested=side;
    qa_watch_binding_last.settings_before=qa_watch_binding_last.settings_after=-1;
    qa_watch_binding_last.arm_before=qa_watch_binding_last.arm_after=-1;
    qa_watch_binding_last.reason="context";
    if((unsigned)side>1 || !cinema_on_game_thread() || !phone_inspect_ready || !phone_actor_event_address
        || !phone_active || !phone_type || phone_type()!=1 || !mv_meshset_class || !mv_skeletal_class || !mv_scene_class)return qa_watch_refuse("context");
    void* pawn=phone_active();struct qa_watch_binding before,after;
    if(!qa_watch_read_binding(pawn,&before)) {
        qa_watch_binding_last.pawn=pawn;qa_watch_binding_last.actor=before.actor;
        return qa_watch_refuse(before.reason);
    }
    qa_watch_binding_last.pawn=pawn;qa_watch_binding_last.actor=before.actor;
    qa_watch_binding_last.arm_before=before.arm;qa_watch_binding_last.socket_before=before.socket;
    void* function=qa_watch_update_function(before.actor);
    if(!function)return qa_watch_refuse("function");
    qa_watch_binding_last.function=function;qa_watch_binding_last.prevalidated=1;
    const unsigned char* settings=qs_settings?qs_settings():0;
    if(settings && settings[0x3c]<=1)qa_watch_binding_last.settings_before=settings[0x3c];
    int accepted=touch_flat_controls_qa_watch(side);qa_watch_binding_last.settings_accepted=accepted;
    unsigned char params[2]={(unsigned char)side,0};
    if(accepted)((void(*)(void*,const void*,void*))phone_actor_event_address)(before.actor,function,params);
    qa_watch_binding_last.callback=params[1];
    settings=qs_settings?qs_settings():0;
    if(settings && settings[0x3c]<=1)qa_watch_binding_last.settings_after=settings[0x3c];
    if(phone_type()==1 && phone_active()==pawn && qa_watch_read_binding(pawn,&after) && after.actor==before.actor
        && after.mesh==before.mesh && after.root==before.root) {
        qa_watch_binding_last.arm_after=after.arm;qa_watch_binding_last.socket_after=after.socket;
        qa_watch_binding_last.attached=accepted && params[1]==1 && after.arm==side
            && after.socket==before.configured[side] && qa_watch_binding_last.settings_after==side;
    }
    qa_watch_binding_last.reason=qa_watch_binding_last.attached?"attached":accepted?"callback_or_binding":"settings";
    if(qa_watch_binding_logs<8) {
        qa_watch_binding_logs++;
        char old_name[64]="unknown",new_name[64]="unknown";
        phone_inspect_fname(&qa_watch_binding_last.socket_before,old_name,sizeof old_name);
        if(qa_watch_binding_last.arm_after>=0)phone_inspect_fname(&qa_watch_binding_last.socket_after,new_name,sizeof new_name);
        LOGF("QA WATCH binding requested=%d pawn=%p actor=%p settings=%d->%d ActiveArm=%d->%d socket='%s'->'%s' accepted=%d callback=%d attached=%d reason=%s",
             side,pawn,before.actor,qa_watch_binding_last.settings_before,qa_watch_binding_last.settings_after,
             before.arm,qa_watch_binding_last.arm_after,old_name,new_name,accepted,params[1],
             qa_watch_binding_last.attached,qa_watch_binding_last.reason);
    }
    return qa_watch_binding_last.attached;
}
#endif
#endif
