// Hide only proved controller mesh leaves; preserve authored text/progress/QTE state.
#ifndef RE4FIX_PROMPT_VISUAL_H
#define RE4FIX_PROMPT_VISUAL_H
#include <stdint.h>
static void* (*pv_floater_class)(void),*(*pv_child_class)(void),*(*pv_actor_class)(void);
static int pv_ready;
static unsigned pv_logs,pv_rejections;
static void touch_tutorial_ui_tick(void);
static int mobile_prefs_hide_gamepad_hints(void);
static const struct {const char* name;unsigned offset;} pv_artwork[]={
    {"QuestStickL1",0x238},{"QuestStickR1",0x240},{"QuestStickL",0x248},{"QuestStickR",0x250},
    {"QuestLT",0x298},{"QuestRT",0x2a0},{"QuestCntlR1",0x2a8},{"QuestCntlL1",0x2b0},
    {"QuestY",0x2b8},{"QuestX",0x2c0},{"QuestB",0x2c8},{"QuestA",0x2d0},
    {"QuestCntlL",0x2f0},{"QuestCntlR",0x2f8},
};
struct prompt_visual_mask {
    struct cinema_ui_weak actor,mesh[14];
    const void* actor_class;
    unsigned char visible[14];
    int used;
};
static struct prompt_visual_mask pv_mask[4];
struct prompt_visual_targets {void* floater,*controller[4];unsigned count;};
static int pv_alive(const void* object) {
    if(!object || !cu_weak_assign || !cu_weak_get)return 0;
    struct cinema_ui_weak weak={-1,0};cu_weak_assign(&weak,object);
    return cu_weak_get(&weak)==object;
}
static int pv_component(const void* component,const void* owner,int minimum) {
    return pv_alive(component) && cinema_object(component,mv_scene_class(),minimum)
        && *(void* const*)((const char*)component+0x20)==owner
        && *(void* const*)((const char*)component+0xa0)==owner;
}
static void* touch_prompt_visual_probe(void) {
    if(!pv_ready || !cinema_on_game_thread() || !ti_ready || !ti_manager || !ti_manager_class
        || !cinema_active || !cinema_type || !comfort_player_class)return 0;
    unsigned type=cinema_type();void* pawn=cinema_active();
    if((type!=1 && type!=3) || !pv_alive(pawn)
        || !cinema_object(pawn,comfort_player_class(),0x570))return 0;
    void* manager=ti_manager();
    if(!pv_alive(manager) || !cinema_object(manager,ti_manager_class(),0x410))return 0;
    // GetTextPromptUi can spawn: inspect only the existing authored property instead.
    void* floater=*(void* const*)((const char*)manager+0x270);
    if(!pv_alive(floater) || !cinema_object(floater,pv_floater_class(),0x250)
        || *(void* const*)((const char*)floater+0x230)!=pawn)return 0;
    void* root=*(void* const*)((const char*)floater+0x130);
    return pv_component(root,floater,0x150)?floater:0;
}
static struct prompt_visual_targets touch_prompt_visual_targets(int diagnostic) {
    struct prompt_visual_targets out={0,{0},0};
    out.floater=touch_prompt_visual_probe();
    if(!out.floater || !phone_inspect_ready || !mv_scene_class)return out;
    void* queue[32]={*(void* const*)((const char*)out.floater+0x130)};unsigned count=1;
    for(unsigned i=0;i<count;i++) {
        void* component=queue[i];
        if(!pv_component(component,out.floater,0x150))continue;
        if(diagnostic) {
            char name[80],kind[80];phone_inspect_fname((const char*)component+0x18,name,sizeof name);
            phone_inspect_fname((const char*)*(void* const*)((const char*)component+0x10)+0x18,kind,sizeof kind);
            LOGF("PROMPT scene id=%u component=%p name='%s' class='%s'",i,component,name,kind);
        }
        if(cinema_object(component,pv_child_class(),0x220)) {
            void* actor=*(void* const*)((const char*)component+0x1f8);
            if(pv_alive(actor) && cinema_object(actor,pv_actor_class(),0x220)) {
                char name[80];phone_inspect_fname((const char*)*(void* const*)((const char*)actor+0x10)+0x18,name,sizeof name);
                void* root=*(void* const*)((const char*)actor+0x130);
                int owned=pv_component(root,actor,0x150)
                    && *(void* const*)((const char*)root+0xc0)==component;
                if(diagnostic)LOGF("PROMPT child actor=%p class='%s' component=%p root=%p owned=%d",actor,name,component,root,owned);
                if(owned && !strcmp(name,"ControllerIcon3d_C") && out.count<4)out.controller[out.count++]=actor;
            }
        }
        struct {void** data;int count,max;} children;
        memcpy(&children,(const char*)component+0xd0,sizeof children);
        if(children.count<0 || children.count>32 || children.max<children.count || children.max>128
            || (children.count && !children.data))continue;
        for(int j=0;j<children.count && count<32;j++) {
            void* child=children.data[j];
            if(!pv_component(child,out.floater,0x150)
                || *(void* const*)((const char*)child+0xc0)!=component)continue;
            int duplicate=0;for(unsigned k=0;k<count;k++)if(queue[k]==child)duplicate=1;
            if(!duplicate)queue[count++]=child;
        }
    }
    return out;
}
static int pv_mesh_owned(void* actor,void* mesh,unsigned index) {
    if(index>=14 || !pv_component(mesh,actor,0x438)
        || !cinema_object(mesh,mv_static_class(),0x438)
        || *(void* const*)((const char*)actor+pv_artwork[index].offset)!=mesh)return 0;
    void* root=*(void* const*)((const char*)actor+0x130);
    if(!pv_component(root,actor,0x150))return 0;
    void* node=mesh;
    // Only this actor's attached geometry. Do not traverse child actors or widgets.
    for(unsigned i=0;i<32;i++) {
        if(node==root)return 1;
        void* parent=*(void* const*)((const char*)node+0xc0);
        if(parent==node || !pv_component(parent,actor,0x150))return 0;
        node=parent;
    }
    return 0;
}
static void pv_restore(struct prompt_visual_mask* mask) {
    if(!mask->used)return;
    void* actor=cu_weak_get(&mask->actor);
    if(actor && *(const void* const*)((const char*)actor+0x10)==mask->actor_class
        && cinema_object(actor,pv_actor_class(),0x3f8)) {
        for(unsigned i=0;i<14;i++) {
            void* mesh=cu_weak_get(&mask->mesh[i]);
            if(pv_mesh_owned(actor,mesh,i)
                && !!(*(const unsigned short*)((const char*)mesh+0x14c)&0x20)!=mask->visible[i])
                mv_visible(mesh,mask->visible[i],0);
        }
    }
    memset(mask,0,sizeof *mask);
}
static int pv_bind(struct prompt_visual_mask* mask,void* actor) {
    if(!cinema_object(actor,pv_actor_class(),0x3f8))return 0;
    struct prompt_visual_mask next={0};
    for(unsigned i=0;i<14;i++) {
        void* mesh=cinema_ui_property(actor,"ControllerIcon3d_C",pv_artwork[i].name,pv_artwork[i].offset);
        if(!pv_mesh_owned(actor,mesh,i)) {
            if(pv_rejections<4){pv_rejections++;LOGF("PROMPT artwork refused actor=%p field='%s' mesh=%p",actor,pv_artwork[i].name,mesh);}
            return 0;
        }
        cu_weak_assign(&next.mesh[i],mesh);
        next.visible[i]=!!(*(const unsigned short*)((const char*)mesh+0x14c)&0x20);
    }
    cu_weak_assign(&next.actor,actor);next.actor_class=*(const void* const*)((const char*)actor+0x10);
    next.used=1;*mask=next;return 1;
}
#include "re4fix_prompt_glyph.h"
// Root calls once on GameThread after original prompt/body work, including context exits.
static void touch_prompt_visual_tick(void) {
    touch_tutorial_ui_tick();
    if(!pv_ready || !cinema_on_game_thread() || !mv_visible || !mv_static_class)return;
    if(!mobile_prefs_hide_gamepad_hints()) {
        for(unsigned i=0;i<4;i++)pv_restore(&pv_mask[i]);
        pv_glyph_tick(0,0,0,0);
        return;
    }
    struct prompt_visual_targets targets=touch_prompt_visual_targets(0);
    pv_glyph_tick(targets.floater,targets.controller,targets.count,1);
    for(unsigned j=0;j<4;j++)if(pv_mask[j].used) {
        void* actor=cu_weak_get(&pv_mask[j].actor);int current=0;
        for(unsigned i=0;i<targets.count;i++)if(actor && actor==targets.controller[i])current=1;
        if(!current || *(const void* const*)((const char*)actor+0x10)!=pv_mask[j].actor_class)pv_restore(&pv_mask[j]);
    }
    for(unsigned j=0;j<targets.count;j++) {
        void* actor=targets.controller[j];struct prompt_visual_mask* mask=0;
        for(unsigned i=0;i<4;i++)if(pv_mask[i].used && cu_weak_get(&pv_mask[i].actor)==actor)mask=&pv_mask[i];
        if(!mask) {
            for(unsigned i=0;i<4;i++)if(!pv_mask[i].used){mask=&pv_mask[i];break;}
            if(!mask || !pv_bind(mask,actor))continue;
            if(pv_logs<8){pv_logs++;LOGF("PROMPT artwork actor=%p ownedStaticMeshes=14 textWidgets/progress retained",actor);}
        }
        int valid=1;
        for(unsigned i=0;i<14;i++)if(!pv_mesh_owned(actor,cu_weak_get(&mask->mesh[i]),i))valid=0;
        if(!valid){pv_restore(mask);continue;}
        for(unsigned i=0;i<14;i++) {
            void* mesh=cu_weak_get(&mask->mesh[i]);
            // Native EVisibilityPropagation=0 changes only this mesh and dirties its render state.
            // Reapply only if the authored Blueprint made the mesh visible again this frame.
            if(*(const unsigned short*)((const char*)mesh+0x14c)&0x20)mv_visible(mesh,0,0);
        }
    }
}
static void install_touch_prompt_visual(uintptr_t base) {
    static const struct {unsigned long addr;unsigned first[2];} code[]={
        {0x640690c,{0xd10183ff,0xf90023f3}},
        {0x6406994,{0x52804a04,0x52800205}},
        {0x8d0c07c,{0xd10183ff,0xf90023f3}},
        {0x8d0c104,{0x52804404,0x52800205}},
        {0x8c6e3bc,{0xd10183ff,0xf90023f3}},
        {0x62da5bc,{0xf9413800,0xb5000200}},
        {0x62685a4,{0x94033ece,0xf9011a60}},
        {0x83ad964,{0xd104c3ff,0xf90073fc}},
        {0x83ad924,{0x79429808,0x52808409}},
        {0x8fdb46c,{0xd10183ff,0xf90023f3}},
    };
    static const struct {unsigned long params;unsigned offset;} fields[]={
        {0x9dbfb50,0x270},{0x9d934c8,0x230},{0xa0b7530,0x130},
        {0xa235b20,0xd0},{0xa235ba8,0xc0},{0xa1087b8,0x1f8},
    };
    pv_ready=0;
    if(!base || !mv_scene_class || !mv_static_class || !mv_visible
        || !cu_weak_assign || !cu_weak_get || !phone_inspect_ready)return;
    for(unsigned i=0;i<sizeof code/sizeof *code;i++)
        if(memcmp((const void*)(base+code[i].addr),code[i].first,8))return;
    for(unsigned i=0;i<sizeof fields/sizeof *fields;i++)
        if(*(const unsigned*)(base+fields[i].params+36)!=fields[i].offset)return;
    if(mv_visible!=(void (*)(void*,int,int))(base+0x83ad964)
        || mv_static_class!=(void* (*)(void))(base+0x8fdb46c))return;
    pv_floater_class=(void* (*)(void))(base+0x640690c);
    pv_child_class=(void* (*)(void))(base+0x8d0c07c);
    pv_actor_class=(void* (*)(void))(base+0x8c6e3bc);pv_ready=1;
    install_touch_prompt_glyph(base);
}
#endif
