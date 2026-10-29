#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
struct cinema_ui_weak {int index,serial;};
struct cinema_ui_vec2 {float x,y;};
struct cinema_ui_anchors {float min_x,min_y,max_x,max_y;};
static unsigned char objects[11][0x900],classes[11][0x100],properties[4][0x70],object_kind[16];
static int alive[11],serials[11],thread_ok,phone_inspect_ready,cu_ready;
static int g_comp_w,g_comp_h;
static size_t g_ue4_base_cached;
static int in_view,add_success,added,removed,layouts,hides,shows;
static void* viewport_widget;
static float dpi;
static struct cinema_ui_vec2 draw,header_draw;
static const char* names[]={"","OptionsUIScreen_BP_C","UI_PauseMenu_Content_Options_BP_C",
    "DoorDescriptionPanel_BP_C","WidgetComponent","UI_MainMenu_Info_Widget_C","Wrong",
    "ObjectProperty","DescriptionPanel","Description_Widget","DescriptionWidget","FloatProperty",
    "UI_PauseMenu_Content_GameOptions_BP_C","OptionsConfirmScreen_BP_C",
    "UI_MainMenu_Options_Confirm_Header_Widget_C","Header Widget","WrongHeader"};
static int index_of(const void* p) {for(int i=0;i<11;i++)if(p==objects[i])return i;return -1;}
static int pv_alive(const void* p) {int i=index_of(p);return i>=0&&alive[i];}
static int cinema_on_game_thread(void) {return thread_ok;}
static int cinema_object(const void* p,const void* cls,int minimum) {
    int i=index_of(p);return pv_alive(p)&&*(int*)(classes[i]+0x58)>=minimum
        && ((cls==(void*)1&&(i==4||i==9||i==10))||(cls==(void*)2&&(i==3||i==8)));
}
static void phone_inspect_fname(const void* name,char* out,int capacity) {
    unsigned id=*(const unsigned*)name;assert(id<sizeof names/sizeof names[0]);
    snprintf(out,(unsigned)capacity,"%s",names[id]);
}
#include DESCRIPTION_BINDING_FRAGMENT
static void* fv_object_property(const void* p,const char* name) {
    if(!pv_alive(p))return 0;
    int off=p==objects[0]&&!strcmp(name,"TrifoldBaseBP")?0x448:
        p==objects[1]&&!strcmp(name,"UIScreen")?0x370:p==objects[1]&&!strcmp(name,"DoorLeftScreen")?0x3a0:
        p==objects[1]&&!strcmp(name,"DoorRightScreen")?0x398:-1;
    return off<0?0:*(void* const*)((const char*)p+off);
}
static void* (*cu_user_class)(void),*(*cu_component_class)(void),*(*cu_get_widget)(const void*);
static void (*mv_visible)(void*,int,int),(*wt_set_visibility)(void*,unsigned char);
static void (*cu_weak_assign)(struct cinema_ui_weak*,const void*);
static void* (*cu_weak_get)(const struct cinema_ui_weak*);
static void (*cu_add)(void*,int),(*cu_remove)(void*);
static int (*cu_in_viewport)(const void*);
static struct cinema_ui_vec2 (*cu_draw_size)(const void*);
static float (*cu_viewport_scale)(void*);
static void (*cu_set_scale)(void*,struct cinema_ui_vec2),(*cu_set_pivot)(void*,struct cinema_ui_vec2);
static void (*cu_set_position)(void*,struct cinema_ui_vec2,int),(*cu_set_size)(void*,struct cinema_ui_vec2);
static void (*cu_set_alignment)(void*,struct cinema_ui_vec2),(*cu_set_anchors)(void*,struct cinema_ui_anchors);
#define LOGF(...) ((void)0)
#include "../re4fix_ui_description.h"
static void* user_class(void) {return (void*)1;}
static void* component_class(void) {return (void*)2;}
static void* get_widget(const void* p) {assert(p==objects[3]||p==objects[8]);return *(void* const*)((const char*)p+0x498);}
static void weak_assign(struct cinema_ui_weak* ref,const void* p) {ref->index=index_of(p);ref->serial=p?serials[ref->index]:0;}
static void* weak_get(const struct cinema_ui_weak* ref) {
    return ref->index>=0&&ref->index<11&&alive[ref->index]&&serials[ref->index]==ref->serial?objects[ref->index]:0;
}
static void visible(void* component,int show,int propagate) {
    assert(component==objects[3]&&!propagate);
    unsigned short* flags=(unsigned short*)(objects[3]+0x14c);
    if(show){*flags|=0x20;shows++;}else{*flags&=~0x20;hides++;}
}
static void require_widget(const void* widget) {assert(widget==objects[4]||widget==objects[9]);}
static void widget_visibility(void* widget,unsigned char value) {require_widget(widget);*((unsigned char*)widget+0xc3)=value;}
static int viewport(const void* widget) {require_widget(widget);return in_view&&viewport_widget==widget;}
static void add(void* widget,int z) {require_widget(widget);assert(z==9&&!in_view);added++;in_view=add_success;if(in_view)viewport_widget=widget;}
static void remove_widget(void* widget) {require_widget(widget);assert(in_view&&viewport_widget==widget);in_view=0;viewport_widget=0;removed++;}
static struct cinema_ui_vec2 draw_size(const void* component) {assert(component==objects[3]||component==objects[8]);return component==objects[3]?draw:header_draw;}
static float viewport_scale(void* widget) {require_widget(widget);return dpi;}
static void set_scale(void* widget,struct cinema_ui_vec2 v) {require_widget(widget);memcpy((char*)widget+0x98,&v,8);layouts++;}
static void set_pivot(void* widget,struct cinema_ui_vec2 v) {require_widget(widget);memcpy((char*)widget+0xac,&v,8);layouts++;}
static void set_position(void* widget,struct cinema_ui_vec2 v,int remove_dpi) {
    require_widget(widget);if(remove_dpi){v.x/=dpi;v.y/=dpi;}memcpy((char*)widget+0x1f0,&v,8);memset((char*)widget+0x1e0,0,16);layouts++;
}
static void set_size(void* widget,struct cinema_ui_vec2 v) {require_widget(widget);memcpy((char*)widget+0x1f8,&v,8);memset((char*)widget+0x1e0,0,16);layouts++;}
static void set_alignment(void* widget,struct cinema_ui_vec2 v) {require_widget(widget);memcpy((char*)widget+0x200,&v,8);layouts++;}
static void set_anchors(void* widget,struct cinema_ui_anchors v) {require_widget(widget);memcpy((char*)widget+0x1e0,&v,16);layouts++;}
static void ready(void) {
    memset(objects,0,sizeof objects);memset(classes,0,sizeof classes);memset(properties,0,sizeof properties);
    for(int i=0;i<11;i++){alive[i]=1;serials[i]=7;*(void**)(objects[i]+0x10)=classes[i];*(int*)(classes[i]+0x58)=0x900;*(unsigned*)(classes[i]+0x18)=(unsigned)i+1;}
    *(unsigned*)(classes[7]+0x18)=13;*(int*)(classes[7]+0x58)=0x29a;
    *(unsigned*)(classes[8]+0x18)=4;*(unsigned*)(classes[9]+0x18)=14;*(int*)(classes[9]+0x58)=0x248;
    *(unsigned*)(classes[10]+0x18)=16;
    *(void**)(objects[0]+0x448)=objects[1];*(void**)(objects[1]+0x370)=objects[0];*(void**)(objects[1]+0x3a0)=objects[2];
    *(void**)(objects[0]+0x438)=objects[2];*(void**)(objects[2]+0x258)=objects[3];*(void**)(objects[2]+0x268)=objects[4];
    *(void**)(objects[3]+0x20)=objects[2];*(void**)(objects[3]+0xa0)=objects[2];*(void**)(objects[3]+0x498)=objects[4];
    *(unsigned short*)(objects[3]+0x14c)=0x20;
    *(void**)(objects[1]+0x398)=objects[7];*(void**)(objects[7]+0x258)=objects[8];
    *(void**)(objects[8]+0x20)=objects[7];*(void**)(objects[8]+0xa0)=objects[7];*(void**)(objects[8]+0x498)=objects[9];
    *(unsigned*)object_kind=7;
    int offsets[]={0x438,0x258,0x268,0x258};
    for(unsigned i=0;i<4;i++){
        *(void**)(properties[i]+8)=object_kind;*(unsigned*)(properties[i]+0x28)=i==3?15:8+i;
        *(int*)(properties[i]+0x34)=1;*(int*)(properties[i]+0x38)=8;*(int*)(properties[i]+0x4c)=offsets[i];
    }
    *(void**)(classes[0]+0x70)=properties[0];*(void**)(classes[2]+0x70)=properties[1];*(void**)(properties[1]+0x58)=properties[2];
    *(void**)(classes[7]+0x70)=properties[3];
    struct cinema_ui_vec2 scale={0.8f,1.2f},pivot={0.4f,0.3f},position={25,49},size={512,1024},alignment={0.2f,0.7f};
    struct cinema_ui_anchors anchors={0.2f,0.1f,0.8f,0.9f};
    memcpy(objects[4]+0x98,&scale,8);memcpy(objects[4]+0xac,&pivot,8);memcpy(objects[4]+0x1f0,&position,8);
    memcpy(objects[4]+0x1f8,&size,8);memcpy(objects[4]+0x200,&alignment,8);memcpy(objects[4]+0x1e0,&anchors,16);
    memcpy(objects[9]+0x98,&scale,8);memcpy(objects[9]+0xac,&pivot,8);memcpy(objects[9]+0x1f0,&position,8);
    size=(struct cinema_ui_vec2){700,972};
    memcpy(objects[9]+0x1f8,&size,8);memcpy(objects[9]+0x200,&alignment,8);memcpy(objects[9]+0x1e0,&anchors,16);
    objects[9][0x230]=0x62;
    memset(objects[4]+0x400,0xa5,64);
    fv_description.screen=fv_description.panel=fv_description.component=fv_description.widget=(struct cinema_ui_weak){-1,0};
    fv_description.saved=fv_description.added=fv_description.hidden=fv_description.width=fv_description.height=fv_description.confirmation=0;
    fv_description_ready=phone_inspect_ready=cu_ready=thread_ok=add_success=1;
    g_comp_w=2400;g_comp_h=1080;dpi=1.5f;draw=(struct cinema_ui_vec2){512,1024};header_draw=(struct cinema_ui_vec2){700,972};
    added=removed=layouts=hides=shows=in_view=0;viewport_widget=0;
    cu_user_class=user_class;cu_component_class=component_class;cu_get_widget=get_widget;
    mv_visible=visible;wt_set_visibility=widget_visibility;cu_weak_assign=weak_assign;cu_weak_get=weak_get;
    cu_add=add;cu_remove=remove_widget;cu_in_viewport=viewport;cu_draw_size=draw_size;cu_viewport_scale=viewport_scale;
    cu_set_scale=set_scale;cu_set_pivot=set_pivot;cu_set_position=set_position;cu_set_size=set_size;
    cu_set_alignment=set_alignment;cu_set_anchors=set_anchors;
}
static void declined(void) {
    unsigned char before[sizeof objects];memcpy(before,objects,sizeof objects);
    fv_description_tick(objects[0]);assert(!added&&!removed&&!hides&&!layouts&&!memcmp(before,objects,sizeof objects));
}
static void bounds(void) {
    struct cinema_ui_vec2 scale,position,size;
    require_widget(viewport_widget);
    struct cinema_ui_vec2 canvas=viewport_widget==objects[4]?draw:header_draw;
    memcpy(&scale,(char*)viewport_widget+0x98,8);memcpy(&position,(char*)viewport_widget+0x1f0,8);memcpy(&size,(char*)viewport_widget+0x1f8,8);
    assert(scale.x==scale.y&&size.x==canvas.x&&size.y==canvas.y);
    assert(fabsf(position.x*dpi-g_comp_w*.01f)<.001f&&fabsf(position.y*dpi-g_comp_h*.23f)<.001f);
    assert(scale.x*dpi*size.x<=g_comp_w*.20f+.01f&&scale.y*dpi*size.y<=g_comp_h*.70f+.01f);
    assert((position.x+scale.x*size.x)*dpi<=g_comp_w*.21f+.01f);
    assert((position.y+scale.y*size.y)*dpi<=g_comp_h*.93f+.01f);
}
int main(void) {
    ready();unsigned char before[sizeof objects];memcpy(before,objects,sizeof objects);
    fv_description_tick(objects[0]);assert(added==1&&hides==1&&in_view&&objects[4][0xc3]==3);bounds();
    int layout_count=layouts;fv_description_tick(objects[0]);assert(added==1&&hides==1&&layouts==layout_count);
    objects[4][0x400]=0x17;fv_description_tick(objects[0]);assert(objects[4][0x400]==0x17);
    g_comp_w=1920;g_comp_h=1200;fv_description_tick(objects[0]);assert(added==1&&layouts>layout_count);bounds();
    dpi=2;fv_description_tick(objects[0]);bounds();
    fv_description_tick(0);assert(!in_view&&removed==1&&shows==1&&objects[4][0x400]==0x17);
    objects[4][0x400]=0xa5;assert(!memcmp(before,objects,sizeof objects));
    ready();*(unsigned*)(classes[1]+0x18)=12;fv_description_tick(objects[0]);assert(added==1);fv_description_tick(0);
    ready();g_comp_w=2780;g_comp_h=1264;draw=(struct cinema_ui_vec2){640,900};
    fv_description_tick(objects[0]);bounds();fv_description_tick(0);
    ready();thread_ok=0;declined();
    ready();*(void**)(objects[1]+0x370)=0;declined();
    ready();*(void**)(objects[1]+0x3a0)=0;declined();
    ready();*(void**)(objects[3]+0x20)=objects[1];declined();
    ready();*(void**)(objects[3]+0xa0)=objects[1];declined();
    ready();*(void**)(objects[3]+0x498)=objects[5];declined();
    ready();*(unsigned*)(classes[4]+0x18)=6;declined();
    ready();*(int*)(properties[0]+0x4c)=0x430;declined();
    ready();*(int*)(properties[1]+0x34)=2;declined();
    ready();*(int*)(properties[2]+0x38)=4;declined();
    ready();*(unsigned*)object_kind=11;declined();
    ready();objects[4][0xc3]=1;declined();
    ready();objects[4][0xc3]=2;declined();
    ready();*(unsigned short*)(objects[3]+0x14c)=0x420;declined();
    ready();*(unsigned short*)(objects[3]+0x14c)=0;declined();
    ready();*(unsigned*)(objects[0]+0x58)=0x20;declined();
    ready();draw.y=NAN;declined();
    ready();draw.x=0;declined();
    ready();dpi=0;declined();
    ready();g_comp_h=0;declined();
    ready();in_view=1;viewport_widget=objects[4];declined();
    ready();add_success=0;memcpy(before,objects,sizeof objects);fv_description_tick(objects[0]);assert(added==1&&!hides&&!memcmp(before,objects,sizeof objects));
    ready();fv_description_tick(objects[0]);serials[4]++;layout_count=layouts;fv_description_tick(0);assert(!removed&&layouts==layout_count&&shows==1);
    ready();fv_description_tick(objects[0]);serials[3]++;fv_description_tick(0);assert(removed==1&&!shows);
    ready();fv_description_tick(objects[0]);objects[4][0xc3]=2;fv_description_tick(0);assert(objects[4][0xc3]==2);
    ready();memcpy(before,objects,sizeof objects);fv_description_tick(objects[0]);
    *(unsigned short*)(objects[8]+0x14c)=0x20;fv_description_tick(objects[0]);
    assert(fv_description.confirmation&&viewport_widget==objects[9]&&added==2&&removed==1&&hides==1&&shows==1);
    assert(*(unsigned short*)(objects[8]+0x14c)==0x20&&objects[9][0xc3]==3);bounds();
    layout_count=layouts;objects[9][0x230]=0x35;fv_description_tick(objects[0]);
    assert(added==2&&layouts==layout_count&&objects[9][0x230]==0x35);
    *(unsigned short*)(objects[8]+0x14c)=0;fv_description_tick(objects[0]);
    assert(!fv_description.confirmation&&viewport_widget==objects[4]&&added==3&&removed==2&&hides==2&&shows==1);
    assert(*(unsigned short*)(objects[8]+0x14c)==0&&objects[9][0xc3]==0&&objects[9][0x230]==0x35);bounds();
    fv_description_tick(0);assert(!in_view&&removed==3&&shows==2);
    objects[9][0x230]=0x62;assert(!memcmp(before,objects,sizeof objects));
    ready();*(unsigned short*)(objects[8]+0x14c)=0x20;fv_description_tick(objects[0]);
    assert(fv_description.confirmation&&added==1&&!hides);bounds();
    *(void**)(objects[1]+0x370)=0;fv_description_tick(objects[0]);assert(removed==1&&!in_view);
    assert(*(unsigned short*)(objects[8]+0x14c)==0x20);
    ready();*(unsigned short*)(objects[8]+0x14c)=0x20;in_view=1;viewport_widget=objects[9];declined();
    ready();*(unsigned short*)(objects[8]+0x14c)=0x20;*(void**)(objects[8]+0x20)=objects[1];
    fv_description_tick(objects[0]);assert(!fv_description.confirmation&&viewport_widget==objects[4]);fv_description_tick(0);
    ready();*(unsigned short*)(objects[8]+0x14c)=0x20;*(void**)(objects[8]+0xa0)=objects[1];
    fv_description_tick(objects[0]);assert(!fv_description.confirmation&&viewport_widget==objects[4]);fv_description_tick(0);
    ready();*(unsigned short*)(objects[8]+0x14c)=0x20;*(void**)(objects[8]+0x498)=objects[10];
    fv_description_tick(objects[0]);assert(!fv_description.confirmation&&viewport_widget==objects[4]);fv_description_tick(0);
    ready();*(unsigned short*)(objects[8]+0x14c)=0x20;*(int*)(properties[3]+0x4c)=0x250;
    fv_description_tick(objects[0]);assert(!fv_description.confirmation&&viewport_widget==objects[4]);fv_description_tick(0);
    ready();*(unsigned short*)(objects[8]+0x14c)=0x20;*(int*)(properties[3]+0x34)=2;
    fv_description_tick(objects[0]);assert(!fv_description.confirmation&&viewport_widget==objects[4]);fv_description_tick(0);
    ready();*(unsigned short*)(objects[8]+0x14c)=0x420;
    fv_description_tick(objects[0]);assert(!fv_description.confirmation&&viewport_widget==objects[4]);fv_description_tick(0);
    ready();*(unsigned short*)(objects[8]+0x14c)=0x20;*(unsigned*)(objects[7]+0x58)=0x20;
    fv_description_tick(objects[0]);assert(!fv_description.confirmation&&viewport_widget==objects[4]);fv_description_tick(0);
    ready();*(unsigned short*)(objects[8]+0x14c)=0x20;fv_description_tick(objects[0]);serials[9]++;
    layout_count=layouts;fv_description_tick(0);assert(!removed&&layouts==layout_count&&!hides&&!shows);
    puts("PASS: actual description/question helper: exact reciprocal bindings, native property bounds, complete canvas fit/DPI/resize, no repeated add/layout, live text, touch passthrough, owned cleanup, weak lifetime and invalid/foreign refusal; native header visibility controls both mode transitions without scene writes");
}
