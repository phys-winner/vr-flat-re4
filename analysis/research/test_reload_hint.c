#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
struct cinema_ui_weak {int index,serial;};
struct cinema_ui_vec2 {float x,y;};
struct cinema_ui_anchors {float min_x,min_y,max_x,max_y;};
static unsigned char manager[0x2c0],instruction[0x68],label[0x2b8],component[0x550],line[0x510],widget[0x300],foreign[0x550];
static unsigned char text[2][0x2a0],tree[0x30];
static unsigned char classes[8][0x80];
static void* chains[8][2];
static void* objects[]={manager,instruction,label,component,line,widget,foreign,tree,text[0],text[1]};
static size_t sizes[]={sizeof manager,sizeof instruction,sizeof label,sizeof component,sizeof line,sizeof widget,sizeof foreign,sizeof tree,sizeof text[0],sizeof text[1]};
static int serials[10],alive[10],on_gt,menu,cu_ready,viewport,add_ok,add_calls,remove_calls,logs;
struct geometry {struct cinema_ui_vec2 size;float m[4],p[2];};
static struct geometry geometry[3];
static struct cinema_ui_vec2 desired[2];
static float fixture_dpi;
static int g_touch_context,g_touch_weapon,g_touch_cancel,g_reload_phase,g_touch_weapon_request;
static int g_touch_pause,g_touch_inventory_request,g_touch_interact_request,qs_busy;
static long clock_ms,g_cmd_until[32],g_cmd_lstick_until,g_cmd_rstick_until;
static float g_tw,g_th,g_cmd_lstick[2],g_cmd_rstick[2];
static const char* instruction_name;
static volatile long hs_reload_ready_ms;
enum {C_FIRE=0,C_RELOAD=8,C_WEAPON=7,C_KNIFE=6};
struct ctl {float cx,cy,r;};
static struct ctl reload_geometry,fire_geometry;
static struct ctl ctl_geometry(int z){assert(z==C_RELOAD || z==C_FIRE);return z==C_FIRE ? fire_geometry : reload_geometry;}
static long now_ms(void){return clock_ms;}
static int menu_mode(void){return menu;}
static int cinema_on_game_thread(void){return on_gt;}
static int cinema_object(const void*,const void*,int);
static void* get_manager(void){return manager;}
static void* manager_class(void){return classes[0];}
static void* label_class(void){return classes[1];}
static void* scene_class(void){return classes[2];}
static void* component_class(void){return classes[3];}
static void* static_class(void){return classes[4];}
static void* user_class(void){return classes[5];}
static void* text_class(void){return classes[6];}
static void* tree_class(void){return classes[7];}
static void weak_assign(struct cinema_ui_weak* out,const void* p){
    out->index=-1;out->serial=0;for(unsigned i=0;i<10;i++)if(p==objects[i]){out->index=(int)i;out->serial=serials[i];}
}
static void* weak_get(const struct cinema_ui_weak* p){return p->index>=0 && p->index<10 && alive[p->index] && serials[p->index]==p->serial ? objects[p->index] : 0;}
static int pv_alive(const void* p){struct cinema_ui_weak w;weak_assign(&w,p);return p && weak_get(&w)==p;}
static void obj_name(void* p,char* out,size_t n){assert(p==instruction);snprintf(out,n,"%s",instruction_name);}
static void* get_widget(const void* p){assert(p==component);return widget;}
static int in_viewport(const void* p){assert(p==widget);return viewport;}
static void add_widget(void* p,int z){assert(p==widget && z==20);add_calls++;if(add_ok)viewport=1;}
static void remove_widget(void* p){assert(p==widget);remove_calls++;viewport=0;}
static struct cinema_ui_vec2 draw_size(const void* p){assert(p==component);return (struct cinema_ui_vec2){500,500};}
static float viewport_scale(void* p){assert(p==widget);return fixture_dpi;}
static void set_scale(void* p,struct cinema_ui_vec2 a){assert(p==widget);memcpy((char*)p+0x98,&a,8);}
static void set_pivot(void* p,struct cinema_ui_vec2 a){assert(p==widget);memcpy((char*)p+0xac,&a,8);}
static void set_position(void* p,struct cinema_ui_vec2 a,int remove_dpi){assert(p==widget);if(remove_dpi){a.x/=fixture_dpi;a.y/=fixture_dpi;}memcpy((char*)p+0x1f0,&a,8);}
static void set_size(void* p,struct cinema_ui_vec2 a){assert(p==widget);memcpy((char*)p+0x1f8,&a,8);}
static void set_alignment(void* p,struct cinema_ui_vec2 a){assert(p==widget);memcpy((char*)p+0x200,&a,8);}
static void set_anchors(void* p,struct cinema_ui_anchors a){assert(p==widget);memcpy((char*)p+0x1e0,&a,16);}
static void set_visibility(void* p,int visible,int propagation){
    assert((p==component || p==line) && propagation==0);
    unsigned short* flags=(unsigned short*)((char*)p+0x14c);*flags=(*flags&~0x20u)|(visible ? 0x20u : 0);
}
static void set_hidden(void* p,int hidden,int propagation){
    assert(p==line && propagation==0);
    unsigned short* flags=(unsigned short*)((char*)p+0x14c);*flags=(*flags&~0x400u)|(hidden ? 0x400u : 0);
}
static void (*cu_weak_assign)(struct cinema_ui_weak*,const void*)=weak_assign;
static void* (*cu_weak_get)(const struct cinema_ui_weak*)=weak_get;
static void* (*mv_scene_class)(void)=scene_class,*(*mv_static_class)(void)=static_class;
static void* (*cu_component_class)(void)=component_class,*(*cu_user_class)(void)=user_class;
static void* (*cu_get_widget)(const void*)=get_widget;
static int (*cu_in_viewport)(const void*)=in_viewport;
static void (*cu_add)(void*,int)=add_widget,(*cu_remove)(void*)=remove_widget;
static struct cinema_ui_vec2 (*cu_draw_size)(const void*)=draw_size;
static float (*cu_viewport_scale)(void*)=viewport_scale;
static void (*cu_set_scale)(void*,struct cinema_ui_vec2)=set_scale,(*cu_set_pivot)(void*,struct cinema_ui_vec2)=set_pivot;
static void (*cu_set_size)(void*,struct cinema_ui_vec2)=set_size,(*cu_set_alignment)(void*,struct cinema_ui_vec2)=set_alignment;
static void (*cu_set_position)(void*,struct cinema_ui_vec2,int)=set_position;
static void (*cu_set_anchors)(void*,struct cinema_ui_anchors)=set_anchors;
static void (*mv_visible)(void*,int,int)=set_visibility;
static void (*mv_hidden)(void*,int,int)=set_hidden;
static void* (*cu_text_class)(void)=text_class,*(*cu_tree_class)(void)=tree_class;
static void* find_widget(const void* p,const void* name){assert(p==widget);return name==text[0]+0x18 ? text[0] : name==text[1]+0x18 ? text[1] : 0;}
static void* (*cu_find_widget)(const void*,const void*)=find_widget;
static float opacity_get(const void* p){return *(const float*)((const char*)p+0xc4);}
static float (*cu_render_opacity)(const void*)=opacity_get;
static void* cinema_ui_property(void* p,const char* cls,const char* wanted,int offset){
    assert(p==widget && !strcmp(cls,"ButtonLabelWidget_C"));
    if(offset==0x240 && !strcmp(wanted,"ButtonLabel"))return text[0];
    if(offset==0x248 && !strcmp(wanted,"ButtonLabelAlt"))return text[1];
    assert(0);return 0;
}
static const void* geometry_get(const void* p){return p==widget ? &geometry[0] : p==text[0] ? &geometry[1] : p==text[1] ? &geometry[2] : 0;}
static struct cinema_ui_vec2 local_size_get(const void* p){return ((const struct geometry*)p)->size;}
static struct cinema_ui_vec2 desired_get(const void* p){assert(p==text[0] || p==text[1]);return desired[p==text[1]];}
static struct cinema_ui_vec2 local_absolute(const void* p,struct cinema_ui_vec2 v){
    const struct geometry* g=p;return (struct cinema_ui_vec2){g->m[0]*v.x+g->m[2]*v.y+g->p[0],g->m[1]*v.x+g->m[3]*v.y+g->p[1]};
}
static struct cinema_ui_vec2 absolute_local(const void* p,struct cinema_ui_vec2 v){
    const struct geometry* g=p;float d=g->m[0]*g->m[3]-g->m[1]*g->m[2];v.x-=g->p[0];v.y-=g->p[1];
    return (struct cinema_ui_vec2){(v.x*g->m[3]-v.y*g->m[2])/d,(v.y*g->m[0]-v.x*g->m[1])/d};
}
#define LOGF(...) ((void)++logs)

/* Native reload route fixtures. The production predicate below calls all these shared guards. */
static unsigned char hand[0x510],other[0x510],gun[0x1310],pawn[0x10d0],ammo[0x620],grip[0x460],holster[0x360];
static int paused,valid,interactivity,ammo_available,reloading,pistol,family,other_owned,layout_ok,ammo_owned,holster_ok;
static void* off_held;
static int touch_quickselect_busy(void){return qs_busy;}
static int reload_layout(const void* p,int minimum){return layout_ok && p && minimum<=0x1310;}
static void* player(const void* h){return h==hand || (h==other && other_owned) ? pawn : 0;}
static void* active(void){return pawn;}
static void* off_hand(const void* p){assert(p==pawn);return other;}
static void* primary(const void* p){assert(p==gun);return hand;}
static int is_paused(void){return paused;}
static int player_valid(void){return valid;}
static int interactivity_get(const void* p){assert(p==gun);return interactivity;}
static int ammo_get(const void* p){assert(p==gun);return ammo_available;}
static int reloading_get(void* p){assert(p==gun);return reloading;}
static void* held_get(const void* p){assert(p==other);return off_held;}
static int reload_is_pistol(void* p){assert(p==gun);return pistol;}
static int reload_family_info(void* p,int* offset){assert(p==gun);*offset=0xd60;return family;}
static void* reload_ammo_primary(void* p,void* owner){assert(p==gun && owner==pawn);return ammo_owned ? grip : 0;}
static void* reload_ammo_holster_grip(void* p,void* owner,void** out){assert(p==ammo && owner==pawn);*out=holster;return holster_ok ? grip : 0;}
static void* (*rl_player)(const void*)=player,*(*rl_active)(void)=active,*(*rl_left)(const void*)=off_hand,*(*rl_right)(const void*)=off_hand;
static void* (*rl_primary)(const void*)=primary,*(*rl_pistol_class)(void)=label_class,*(*f_GetCurrentGrip)(const void*)=held_get;
static int (*rl_paused)(void)=is_paused,(*rl_player_valid)(void)=player_valid,(*rl_interactivity)(const void*)=interactivity_get;
static int (*rl_ammo_available)(const void*)=ammo_get,(*rl_reloading)(void*)=reloading_get;
#include RELOAD_HINT_HELPERS
#include "re4fix_tutorial.h"

static void class_set(unsigned id,int size,int parent){
    *(int*)(classes[id]+0x58)=size;*(void**)(classes[id]+0x30)=chains[id];
    int depth=parent<0 ? 0 : 1;*(int*)(classes[id]+0x38)=depth;
    if(parent>=0)chains[id][0]=classes[parent]+0x30;chains[id][depth]=classes[id]+0x30;
}
static void* labels[1];
static void setup(void){
    memset(&tut_reload_ui,0,sizeof tut_reload_ui);tut_probe_ms=0;tut_logged[0]=0;tut_reload_ui_ms=0;
    for(unsigned i=0;i<10;i++){memset(objects[i],0,sizes[i]);serials[i]=1;alive[i]=1;}
    memset(classes,0,sizeof classes);memset(chains,0,sizeof chains);
    class_set(0,0x2c0,-1);class_set(1,0x2b8,-1);class_set(2,0x1f0,-1);
    class_set(3,0x550,2);class_set(4,0x510,2);class_set(5,0x300,-1);class_set(6,0x2a0,-1);class_set(7,0x30,-1);
    *(void**)(manager+0x10)=classes[0];*(void**)(label+0x10)=classes[1];
    *(void**)(component+0x10)=classes[3];*(void**)(line+0x10)=classes[4];*(void**)(widget+0x10)=classes[5];
    *(void**)(tree+0x10)=classes[7];*(void**)(tree+0x20)=widget;*(void**)(widget+0x1a8)=tree;
    for(unsigned i=0;i<2;i++){*(void**)(text[i]+0x10)=classes[6];*(void**)(text[i]+0x20)=tree;*(float*)(text[i]+0xc4)=1;}
    geometry[0]=(struct geometry){{500,500},{.1785f,0,0,.1785f},{2200,800}};
    geometry[1]=(struct geometry){{300,48},{.1785f,0,0,.1785f},{2200+125*.1785f,800+225*.1785f}};
    geometry[2]=(struct geometry){{0,0},{.1785f,0,0,.1785f},{2200,800}};
    desired[0]=(struct cinema_ui_vec2){300,48};desired[1]=(struct cinema_ui_vec2){0,0};
    *(void**)(manager+0x298)=instruction;labels[0]=label;*(void**)(manager+0x2b0)=labels;
    *(int*)(manager+0x2b8)=*(int*)(manager+0x2bc)=1;*(void**)(label+0x228)=component;*(void**)(label+0x230)=line;label[0x2b4]=1;
    *(void**)(component+0x20)=*(void**)(component+0xa0)=label;*(void**)(line+0x20)=*(void**)(line+0xa0)=label;
    *(unsigned short*)(component+0x14c)=0x81|0x20;*(unsigned short*)(line+0x14c)=0x81|0x20;
    memset(widget+0x90,0x5a,0x80);memset(widget+0x1e0,0x5a,0x28);
    on_gt=cu_ready=add_ok=1;menu=viewport=add_calls=remove_calls=logs=0;
    g_touch_context=1;g_touch_weapon=2;g_touch_cancel=0;g_tw=2400;g_th=1080;clock_ms=10000;fixture_dpi=1.5f;
    reload_geometry=(struct ctl){.925f,.85f,.066f};fire_geometry=(struct ctl){.895f,.61f,.093f};instruction_name="GrabAmmoVignetteSit";
    tut_manager=get_manager;tut_manager_class=manager_class;tut_label_class=label_class;
    tut_geometry=geometry_get;tut_local_size=local_size_get;tut_desired_size=desired_get;
    tut_local_absolute=local_absolute;tut_absolute_local=absolute_local;
    touch_tutorial_tick(clock_ms);
}
static void test_tutorial(void){
    unsigned char before[sizeof widget],actor_before[sizeof label];
    setup();memcpy(before,widget,sizeof before);memcpy(actor_before,label,sizeof actor_before);
    touch_tutorial_ui_tick();assert(viewport && add_calls==1 && touch_tutorial_reload_hint(clock_ms));
    assert(!memcmp(label,actor_before,sizeof label));assert(*(unsigned short*)(component+0x14c)==0x81 && *(unsigned short*)(line+0x14c)==0x481);
    assert(*(unsigned short*)(line+0x14c)&0x400);
    struct cinema_ui_vec2 pos;memcpy(&pos,widget+0x1f0,8);
    assert(pos.x*1.5f<2400 && pos.y*1.5f<(reload_geometry.cy-reload_geometry.r)*g_th);
    struct cinema_ui_vec2 scale;memcpy(&scale,widget+0x98,8);float bounds[4];assert(tut_caption_bounds(widget,bounds));
    assert(fabsf(bounds[0]-125)<.002f && fabsf(bounds[1]-225)<.002f && fabsf(bounds[2]-300)<.002f && fabsf(bounds[3]-48)<.002f);
    assert(fabsf(bounds[3]*scale.y*1.5f-g_th*.035f)<.01f); // text height, independent of the empty500x500 author canvas
    assert(fabsf((pos.x+(bounds[0]+bounds[2])*scale.x)*1.5f-fminf(g_tw-g_th*.01f,reload_geometry.cx*g_tw+reload_geometry.r*g_th))<.02f);
    *(unsigned short*)(line+0x14c)|=0x20;assert(*(unsigned short*)(line+0x14c)&0x400); // a later native re-show stays hidden
    clock_ms+=200;reload_geometry.cx=.8f;reload_geometry.cy=.7f;touch_tutorial_ui_tick();assert(add_calls==1);
    assert(!touch_tutorial_reload_hint(clock_ms+401));
    g_touch_context=2;touch_tutorial_ui_tick();assert(!viewport && remove_calls==1 && !touch_tutorial_reload_hint(clock_ms));
    assert(!memcmp(widget,before,sizeof widget));assert(*(unsigned short*)(component+0x14c)==0xa1 && *(unsigned short*)(line+0x14c)==0xa1);
    setup();add_ok=0;memcpy(before,widget,sizeof before);touch_tutorial_ui_tick();assert(!viewport && !tut_reload_ui.used && !memcmp(widget,before,sizeof widget));
    setup();on_gt=0;touch_tutorial_ui_tick();assert(!add_calls);
    setup();menu=1;touch_tutorial_ui_tick();assert(!add_calls);
    setup();g_touch_weapon=1;touch_tutorial_ui_tick();assert(!add_calls);
    setup();g_touch_cancel=1;touch_tutorial_ui_tick();assert(!add_calls);
    setup();desired[0].x=0;touch_tutorial_ui_tick();assert(!add_calls);
    setup();geometry[0].m[0]=0;touch_tutorial_ui_tick();assert(!add_calls);
    setup();*(void**)(text[0]+0x20)=foreign;touch_tutorial_ui_tick();assert(!add_calls);
    setup();*(int*)(manager+0x2b8)=2;touch_tutorial_ui_tick();assert(!add_calls);
    setup();*(int*)(manager+0x2bc)=65;touch_tutorial_ui_tick();assert(!add_calls);
    setup();*(void**)(component+0xa0)=foreign;touch_tutorial_ui_tick();assert(!add_calls);
    setup();label[0x2b4]=0;touch_tutorial_ui_tick();assert(!add_calls);
    setup();viewport=1;touch_tutorial_ui_tick();assert(!add_calls && !tut_reload_ui.used);
    setup();touch_tutorial_ui_tick();serials[1]++;touch_tutorial_ui_tick();assert(!viewport && remove_calls==1);
    setup();touch_tutorial_ui_tick();alive[2]=0;touch_tutorial_ui_tick();assert(!viewport && remove_calls==1);
    setup();touch_tutorial_ui_tick();instruction_name="TriggerHoldL";clock_ms+=100;touch_tutorial_tick(clock_ms);touch_tutorial_ui_tick();assert(!viewport && remove_calls==1);
    puts("PASS: actual GrabAmmo caption promotion, preserved native label, configured/DPI anchor, add failure, context/menu/cancel/type/ownership/array/weak/dead-owner cleanup and expiry");
}
static void test_captured_caption(void){
    setup();g_tw=2780;g_th=1264;fixture_dpi=caption_capture[0][2];
    for(unsigned i=0;i<3;i++) {
        geometry[i].size=(struct cinema_ui_vec2){caption_capture[i][0],caption_capture[i][1]};
        memcpy(geometry[i].m,caption_capture[i]+7,16);memcpy(geometry[i].p,caption_capture[i]+11,8);
    }
    desired[0]=geometry[1].size;
    float intrinsic[4];assert(tut_caption_bounds(widget,intrinsic));
    assert(fabsf(intrinsic[0]-83.312f)<.01f && fabsf(intrinsic[1]-201.007f)<.01f);
    assert(fabsf(intrinsic[2]-333.3734f)<.01f && fabsf(intrinsic[3]-93.9873f)<.01f);
    float first=0;
    for(unsigned frame=0;frame<20;frame++) {
        touch_tutorial_ui_tick();assert(viewport);
        struct cinema_ui_vec2 scale,pos;memcpy(&scale,widget+0x98,8);memcpy(&pos,widget+0x1f0,8);
        if(!frame)first=scale.x;else assert(fabsf(first-scale.x)<.0001f);
        assert(first>.39f && first<.42f); // the live .119 root scale was measuring500px, rather than the94px text
        float left=pos.x*fixture_dpi+intrinsic[0]*scale.x*fixture_dpi;
        float top=pos.y*fixture_dpi+intrinsic[1]*scale.y*fixture_dpi;
        float right=left+intrinsic[2]*scale.x*fixture_dpi,bottom=top+intrinsic[3]*scale.y*fixture_dpi;
        assert(fabsf(bottom-top-44.24f)<.02f && left>=0 && right<=g_tw && top>(fire_geometry.cy+fire_geometry.r)*g_th);
        assert(bottom<(reload_geometry.cy-reload_geometry.r-.018f)*g_th);
        geometry[0].m[0]=geometry[0].m[3]=scale.x*fixture_dpi;geometry[0].m[1]=geometry[0].m[2]=0;
        geometry[0].p[0]=pos.x*fixture_dpi;geometry[0].p[1]=pos.y*fixture_dpi;
        memcpy(geometry[1].m,geometry[0].m,16);
        struct cinema_ui_vec2 translated=local_absolute(&geometry[0],(struct cinema_ui_vec2){intrinsic[0],intrinsic[1]});
        geometry[1].p[0]=translated.x;geometry[1].p[1]=translated.y;clock_ms+=17;
    }
    puts("PASS: captured p327 native500px canvas/333x94 text bounds ->44.24px caption, no Fire/Reload overlap,20 feedback frames preserve scale/anchor; zero-size Alt ignored");
}
static void reload_setup(void){
    g_touch_context=1;g_touch_cancel=g_reload_phase=g_touch_weapon_request=g_touch_pause=g_touch_inventory_request=g_touch_interact_request=qs_busy=0;
    paused=reloading=family=0;valid=ammo_available=pistol=other_owned=layout_ok=ammo_owned=holster_ok=1;interactivity=2;off_held=0;
    memset(hand,0,sizeof hand);memset(other,0,sizeof other);memset(gun,0,sizeof gun);hand[0x314]=1;*(void**)(gun+0x8e0)=ammo;
}
static void test_reload_predicate(void){
    reload_setup();assert(hs_reload_possible(hand,gun));
    int* gates[]={&g_touch_cancel,&g_reload_phase,&g_touch_weapon_request,&g_touch_pause,&g_touch_inventory_request,&g_touch_interact_request,&qs_busy,&paused,&reloading};
    for(unsigned i=0;i<sizeof gates/sizeof *gates;i++){reload_setup();*gates[i]=1;assert(!hs_reload_possible(hand,gun));}
    int* needed[]={&valid,&ammo_available,&other_owned,&layout_ok,&ammo_owned,&holster_ok};
    for(unsigned i=0;i<sizeof needed/sizeof *needed;i++){reload_setup();*needed[i]=0;assert(!hs_reload_possible(hand,gun));}
    reload_setup();g_touch_context=2;assert(!hs_reload_possible(hand,gun));
    reload_setup();pistol=0;assert(!hs_reload_possible(hand,gun));family=1;assert(hs_reload_possible(hand,gun));
    reload_setup();off_held=foreign;assert(!hs_reload_possible(hand,gun));off_held=grip;holster_ok=0;assert(hs_reload_possible(hand,gun));
    reload_setup();hand[0x314]=0;other[0x314]=1;assert(hs_reload_possible(hand,gun));
    reload_setup();*(float*)(gun+0xd50)=NAN;assert(!hs_reload_possible(hand,gun));
    hs_reload_ready_ms=1000;assert(touch_hud_reload_needed(1000) && touch_hud_reload_needed(1400));
    assert(!touch_hud_reload_needed(999) && !touch_hud_reload_needed(1401));hs_reload_ready_ms=0;assert(!touch_hud_reload_needed(1000));
    puts("PASS: actual reload predicate uses native supported-gun/ammo/free-offhand/holster gates, both hands, all busy/context/pause cancellation and bounded publication");
}
int main(void){test_tutorial();test_captured_caption();test_reload_predicate();return 0;}
