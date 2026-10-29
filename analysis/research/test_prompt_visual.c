#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
static int cinema_object(const void*,const void*,int);
struct cinema_ui_weak {int index,serial;};
static unsigned char pawn[0x600],manager[0x420],floater[0x580],controller[0x400],ring[0x300];
static unsigned char root[0x300],child[0x300],controller_root[0x300],ring_child[0x300],ring_root[0x300];
static unsigned char meshes[14][0x500],widgets[2][0x560],replacement[0x500];
static unsigned char classes[9][0x80],fields[14][0x60],field_kind[8];
static void* chains[9][2];
static unsigned char prompt_component[0x560],prompt_user[0x230],prompt_tree[0x100],prompt_panel[0x180],prompt_image[0x180],prompt_caption[0x180],prompt_slots[2][0x40];
static unsigned char prompt_classes[3][0x80];static void* prompt_chains[3][1];
static void* objects[35]={pawn,manager,floater,controller,ring,root,child,controller_root,ring_child,ring_root,
    meshes[0],meshes[1],meshes[2],meshes[3],meshes[4],meshes[5],meshes[6],meshes[7],
    meshes[8],meshes[9],meshes[10],meshes[11],meshes[12],meshes[13],widgets[0],widgets[1],replacement,
    prompt_component,prompt_user,prompt_tree,prompt_panel,prompt_image,prompt_caption,prompt_slots[0],prompt_slots[1]};
static size_t sizes[35]={sizeof pawn,sizeof manager,sizeof floater,sizeof controller,sizeof ring,
    sizeof root,sizeof child,sizeof controller_root,sizeof ring_child,sizeof ring_root,
    0x500,0x500,0x500,0x500,0x500,0x500,0x500,0x500,0x500,0x500,0x500,0x500,0x500,0x500,0x560,0x560,0x500,
    0x560,0x230,0x100,0x180,0x180,0x180,0x40,0x40};
static void* children[3];
static int alive[35],serials[35],on_gt=1,ti_ready=1,phone_inspect_ready=1,logs,visible_calls;
static unsigned context=1;
static void* get_active(void){return pawn;}static unsigned get_type(void){return context;}
static void* get_manager(void){return manager;}
static void* pawn_class(void){return classes[0];}static void* manager_class(void){return classes[1];}
static void* floater_class(void){return classes[2];}static void* actor_class(void){return classes[3];}
static void* scene_class(void){return classes[5];}static void* child_class(void){return classes[6];}
static void* static_class(void){return classes[7];}
static void weak_assign(struct cinema_ui_weak* w,const void* p){
    w->index=-1;w->serial=0;for(int i=0;i<35;i++)if(p==objects[i]){w->index=i;w->serial=serials[i];}
}
static void* weak_get(const struct cinema_ui_weak* w){
    return w->index>=0 && w->index<35 && alive[w->index] && serials[w->index]==w->serial?objects[w->index]:0;
}
static int cinema_on_game_thread(void){return on_gt;}
static void phone_inspect_fname(const void* p,char* out,int size){
    static const char* names[]={"QuestStickL1","QuestStickR1","QuestStickL","QuestStickR","QuestLT","QuestRT",
        "QuestCntlR1","QuestCntlL1","QuestY","QuestX","QuestB","QuestA","QuestCntlL","QuestCntlR"};
    uint64_t id;memcpy(&id,p,8);const char* name=id>=100 && id<114?names[id-100]:
        id==33?"ControllerIcon3d_C":id==44?"InfoRing_C":id==77?"WidgetComponent":id==88?"ObjectProperty":"SceneComponent";
    assert((int)strlen(name)<size);memcpy(out,name,strlen(name)+1);
}
static void set_visibility(void* mesh,int visible,int propagation){
    assert(propagation==0 && (visible==0 || visible==1));int selected=0;
    for(int i=0;i<14;i++)if(mesh==meshes[i])selected=1;
    if(mesh==replacement)selected=1;assert(selected);
    unsigned short* flags=(unsigned short*)((char*)mesh+0x14c);
    *flags=(*flags&~0x20)|(visible?0x20:0);visible_calls++;
}
static void* (*cinema_active)(void)=get_active,*(*comfort_player_class)(void)=pawn_class;
static unsigned (*cinema_type)(void)=get_type;
static void* (*ti_manager)(void)=get_manager,*(*ti_manager_class)(void)=manager_class,*(*mv_scene_class)(void)=scene_class;
static void* (*mv_static_class)(void)=static_class;
static void (*mv_visible)(void*,int,int)=set_visibility;
static void (*cu_weak_assign)(struct cinema_ui_weak*,const void*)=weak_assign;
static void* (*cu_weak_get)(const struct cinema_ui_weak*)=weak_get;
#define LOGF(...) ((void)++logs)
static void touch_tutorial_ui_tick(void) {}
static int hide_hints=1;
static int mobile_prefs_hide_gamepad_hints(void) {return hide_hints;}
static int cu_ready;
static void* no_widget(const void* component){(void)component;return 0;}
static void*(*cu_user_class)(void)=pawn_class,*(*cu_component_class)(void)=scene_class,*(*cu_get_widget)(const void*)=no_widget;
#include "re4fix_prompt_visual.h"
extern void fixture_prompt_image(unsigned char*);
extern unsigned long fixture_prompt_code[],fixture_prompt_fields[];
extern unsigned fixture_prompt_count,fixture_prompt_field_count;
static void set_class(unsigned index,int size,int depth){
    memset(classes[index],0,sizeof classes[index]);chains[index][depth]=classes[index]+0x30;
    *(void**)(classes[index]+0x30)=chains[index];*(int*)(classes[index]+0x38)=depth;*(int*)(classes[index]+0x58)=size;
}
static void link_component(void* component,void* owner,void* parent){
    *(void**)((char*)component+0x20)=owner;*(void**)((char*)component+0xa0)=owner;*(void**)((char*)component+0xc0)=parent;
}
static void setup(void){
    memset(pv_mask,0,sizeof pv_mask);pv_logs=pv_rejections=0;
    for(int i=0;i<35;i++){memset(objects[i],0,sizes[i]);alive[i]=1;serials[i]=1;}
    set_class(0,0x570,0);set_class(1,0x410,0);set_class(2,0x578,0);set_class(3,0x220,0);
    set_class(4,0x3f8,1);set_class(5,0x1f0,0);set_class(6,0x220,1);set_class(7,0x438,1);set_class(8,0x550,1);
    chains[4][0]=classes[3]+0x30;
    chains[6][0]=chains[7][0]=chains[8][0]=classes[5]+0x30;
    *(void**)(pawn+0x10)=classes[0];*(void**)(manager+0x10)=classes[1];*(void**)(floater+0x10)=classes[2];
    *(void**)(controller+0x10)=classes[4];*(uint64_t*)(classes[4]+0x18)=33;
    *(void**)(ring+0x10)=classes[3];*(uint64_t*)(classes[3]+0x18)=44;
    *(void**)(root+0x10)=classes[5];*(void**)(controller_root+0x10)=classes[5];*(void**)(ring_root+0x10)=classes[5];
    *(void**)(child+0x10)=classes[6];*(void**)(ring_child+0x10)=classes[6];
    *(void**)(manager+0x270)=floater;*(void**)(floater+0x230)=pawn;*(void**)(floater+0x130)=root;
    *(void**)(controller+0x130)=controller_root;*(void**)(ring+0x130)=ring_root;
    link_component(root,floater,0);link_component(child,floater,root);link_component(ring_child,floater,root);
    link_component(controller_root,controller,child);link_component(ring_root,ring,ring_child);
    *(void**)(child+0x1f8)=controller;*(void**)(ring_child+0x1f8)=ring;
    children[0]=child;children[1]=ring_child;*(void**)(root+0xd0)=children;*(int*)(root+0xd8)=2;*(int*)(root+0xdc)=2;
    *(uint64_t*)field_kind=88;*(void**)(classes[4]+0x70)=fields[0];
    for(unsigned i=0;i<14;i++) {
        memset(fields[i],0,sizeof fields[i]);*(void**)(fields[i]+8)=field_kind;
        *(uint64_t*)(fields[i]+0x28)=100+i;*(int*)(fields[i]+0x34)=1;
        *(int*)(fields[i]+0x38)=8;*(int*)(fields[i]+0x4c)=pv_artwork[i].offset;
        *(void**)(fields[i]+0x58)=i==13?0:fields[i+1];
        *(void**)(controller+pv_artwork[i].offset)=meshes[i];*(void**)(meshes[i]+0x10)=classes[7];
        link_component(meshes[i],controller,controller_root);
        *(unsigned short*)(meshes[i]+0x14c)=(unsigned short)(0x481|((i&1)?0:0x20));
    }
    *(void**)(replacement+0x10)=classes[7];link_component(replacement,controller,controller_root);
    *(unsigned short*)(replacement+0x14c)=0x20;
    for(unsigned i=0;i<2;i++){
        *(void**)(widgets[i]+0x10)=classes[8];link_component(widgets[i],controller,controller_root);
        *(unsigned short*)(widgets[i]+0x14c)=0x20;
    }
    *(void**)(controller+0x228)=widgets[0];*(void**)(controller+0x270)=widgets[1];
    *(void**)(controller+0x398)=ring;*(void**)(controller+0x3b0)=ring;
    pv_ready=1;pv_floater_class=floater_class;pv_child_class=child_class;pv_actor_class=actor_class;
    mv_visible=set_visibility;mv_static_class=static_class;
    on_gt=ti_ready=phone_inspect_ready=1;context=1;logs=visible_calls=0;hide_hints=1;
}
static unsigned short flags(unsigned i){return *(unsigned short*)(meshes[i]+0x14c);}
static void assert_original(void){for(unsigned i=0;i<14;i++)assert(flags(i)==(unsigned short)(0x481|((i&1)?0:0x20)));}
static void assert_masked(void){for(unsigned i=0;i<14;i++)assert(flags(i)==0x481);}
#include "world_prompt_fixture.h"
int main(void){
    check_world_prompt_artwork();
    setup();hide_hints=0;touch_prompt_visual_tick();assert_original();assert(!visible_calls);
    hide_hints=1;touch_prompt_visual_tick();assert_masked();assert(visible_calls==7);
    hide_hints=0;touch_prompt_visual_tick();assert_original();assert(visible_calls==14);
    touch_prompt_visual_tick();assert(visible_calls==14);
    hide_hints=1;touch_prompt_visual_tick();assert_masked();assert(visible_calls==21);
    setup();struct prompt_visual_targets result=touch_prompt_visual_targets(1);
    assert(result.floater==floater && result.count==1 && result.controller[0]==controller && logs<=5);
    unsigned char before[sizeof floater];memcpy(before,floater,sizeof before);result=touch_prompt_visual_targets(0);assert(!memcmp(before,floater,sizeof before));
    context=3;assert(touch_prompt_visual_probe()==floater);context=4;assert(!touch_prompt_visual_probe());
    setup();on_gt=0;assert(!touch_prompt_visual_probe());
    setup();alive[2]=0;assert(!touch_prompt_visual_probe());
    setup();*(void**)(manager+0x270)=0;assert(!touch_prompt_visual_probe());
    setup();*(void**)(floater+0x230)=ring;assert(!touch_prompt_visual_probe());
    setup();*(void**)(root+0x20)=ring;assert(!touch_prompt_visual_probe());
    setup();*(void**)(root+0xa0)=ring;assert(!touch_prompt_visual_probe());
    setup();alive[3]=0;assert(!touch_prompt_visual_targets(0).count);
    setup();*(void**)(controller_root+0xc0)=ring_child;assert(!touch_prompt_visual_targets(0).count);
    setup();*(void**)(child+0x20)=ring;assert(!touch_prompt_visual_targets(0).count);
    setup();*(void**)(child+0xc0)=ring_root;assert(!touch_prompt_visual_targets(0).count);
    setup();*(int*)(root+0xd8)=-1;assert(!touch_prompt_visual_targets(0).count);
    setup();*(int*)(root+0xd8)=33;assert(!touch_prompt_visual_targets(0).count);
    setup();*(int*)(root+0xdc)=129;assert(!touch_prompt_visual_targets(0).count);
    setup();*(void**)(root+0xd0)=0;assert(!touch_prompt_visual_targets(0).count);
    setup();children[1]=child;assert(touch_prompt_visual_targets(0).count==1);
    /* All actor/Floater bytes, widgets, InfoRing and progress remain byte-identical. */
    setup();unsigned char actor_before[sizeof controller],widget_before[sizeof widgets],ring_before[sizeof ring];
    memcpy(before,floater,sizeof before);memcpy(actor_before,controller,sizeof actor_before);
    memcpy(widget_before,widgets,sizeof widgets);memcpy(ring_before,ring,sizeof ring);
    touch_prompt_visual_tick();assert_masked();assert(visible_calls==7);
    assert(!memcmp(actor_before,controller,sizeof controller) && !memcmp(before,floater,sizeof floater));
    assert(!memcmp(widget_before,widgets,sizeof widgets) && !memcmp(ring_before,ring,sizeof ring));
    touch_prompt_visual_tick();assert(visible_calls==7);
    *(unsigned short*)(meshes[1]+0x14c)|=0x20;touch_prompt_visual_tick();assert_masked();assert(visible_calls==8);
    context=4;touch_prompt_visual_tick();assert_original();assert(visible_calls==15);
    touch_prompt_visual_tick();assert(visible_calls==15);
    setup();on_gt=0;touch_prompt_visual_tick();assert(!visible_calls);assert_original();
    setup();context=5;touch_prompt_visual_tick();assert(!visible_calls);assert_original();
    /* Bind refuses before any setter for a metadata/type/attachment/ownership failure. */
    for(unsigned i=0;i<14;i++){
        setup();*(int*)(fields[i]+0x4c)^=8;touch_prompt_visual_tick();assert(!visible_calls);assert_original();
        setup();*(int*)(fields[i]+0x34)=2;touch_prompt_visual_tick();assert(!visible_calls);
        setup();*(int*)(fields[i]+0x38)=4;touch_prompt_visual_tick();assert(!visible_calls);
        setup();*(void**)(meshes[i]+0x10)=classes[8];touch_prompt_visual_tick();assert(!visible_calls);
        setup();*(void**)(meshes[i]+0xa0)=ring;touch_prompt_visual_tick();assert(!visible_calls);
        setup();*(void**)(meshes[i]+0x20)=ring;touch_prompt_visual_tick();assert(!visible_calls);
        setup();*(void**)(meshes[i]+0xc0)=meshes[i];touch_prompt_visual_tick();assert(!visible_calls);
        setup();*(void**)(meshes[i]+0xc0)=ring_root;touch_prompt_visual_tick();assert(!visible_calls);
    }
    setup();*(uint64_t*)field_kind=44;touch_prompt_visual_tick();assert(!visible_calls);
    setup();*(int*)(classes[4]+0x58)=0x300;touch_prompt_visual_tick();assert(!visible_calls);
    setup();touch_prompt_visual_tick();alive[3]=0;context=4;int count=visible_calls;touch_prompt_visual_tick();assert(count==visible_calls);
    setup();touch_prompt_visual_tick();serials[10]++;context=4;touch_prompt_visual_tick();
    assert(!(flags(0)&0x20));for(unsigned i=1;i<14;i++)assert(flags(i)==(unsigned short)(0x481|((i&1)?0:0x20)));
    setup();touch_prompt_visual_tick();*(void**)(controller+pv_artwork[0].offset)=replacement;
    context=4;touch_prompt_visual_tick();assert(!(flags(0)&0x20) && *(unsigned short*)(replacement+0x14c)==0x20);
    setup();touch_prompt_visual_tick();*(void**)(manager+0x270)=0;touch_prompt_visual_tick();assert_original();
    setup();touch_prompt_visual_tick();*(void**)(meshes[0]+0xa0)=ring;context=4;touch_prompt_visual_tick();assert(!(flags(0)&0x20));
    setup();link_component(meshes[0],controller,meshes[1]);link_component(meshes[1],controller,meshes[0]);
    touch_prompt_visual_tick();assert(!visible_calls);
#ifdef _WIN32
    uintptr_t base=0x20000000;unsigned char* image=VirtualAlloc((void*)base,0xb000000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(image==(void*)base);
    fixture_prompt_image(image);mv_visible=(void (*)(void*,int,int))(base+0x83ad964);mv_static_class=(void* (*)(void))(base+0x8fdb46c);
    install_touch_prompt_visual(base);assert(pv_ready);
    for(unsigned i=0;i<fixture_prompt_count;i++)for(unsigned j=0;j<2;j++){
        *(unsigned*)(image+fixture_prompt_code[i]+4*j)^=1;install_touch_prompt_visual(base);assert(!pv_ready);
        *(unsigned*)(image+fixture_prompt_code[i]+4*j)^=1;
    }
    for(unsigned i=0;i<fixture_prompt_field_count;i++){
        *(unsigned*)(image+fixture_prompt_fields[i]+36)^=1;install_touch_prompt_visual(base);assert(!pv_ready);
        *(unsigned*)(image+fixture_prompt_fields[i]+36)^=1;
    }
    mv_visible=set_visibility;install_touch_prompt_visual(base);assert(!pv_ready);
    mv_visible=(void (*)(void*,int,int))(base+0x83ad964);mv_static_class=static_class;install_touch_prompt_visual(base);assert(!pv_ready);
    install_touch_prompt_visual(0);assert(!pv_ready);assert(VirtualFree(image,0,MEM_RELEASE));
#endif
    puts("PASS: actual mesh-only masking/restoration, unchanged widget/caption/progress/actor, metadata/ownership/attachment/weak serial/type/thread and installer refusals");return 0;
}
