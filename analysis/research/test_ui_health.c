#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
struct cinema_ui_weak{int index,serial;};
struct cinema_ui_vec2{float x,y;};struct cinema_ui_anchors{float x,y,z,w;};
static unsigned char objects[12][0x1200],classes[8][0x100],properties[7][0x60];
static int alive[12],serials[12],on_gt,context,cu_ready=1,phone_inspect_ready=1,adds,removes,in_viewport,add_ok,lookups;
static unsigned g_comp_w=2400,g_comp_h=1080;
static float dpi=2;static long now;
static void* active,*bio4,*current_widget;
static int index_of(const void* p){for(int i=0;i<12;i++)if(p==objects[i])return i;for(int i=0;i<8;i++)if(p==classes[i])return 12+i;return -1;}
static int pv_alive(const void* p){int i=index_of(p);return i>=12||(i>=0&&alive[i]);}
static void weak_assign(struct cinema_ui_weak* ref,const void* object){ref->index=index_of(object);ref->serial=ref->index<0?0:ref->index>=12?1:serials[ref->index];}
static void* weak_get(const struct cinema_ui_weak* ref){return ref->index>=12&&ref->index<20?classes[ref->index-12]
    :ref->index>=0&&ref->index<12&&alive[ref->index]&&serials[ref->index]==ref->serial?objects[ref->index]:0;}
static void(*cu_weak_assign)(struct cinema_ui_weak*,const void*)=weak_assign;
static void*(*cu_weak_get)(const struct cinema_ui_weak*)=weak_get;
static void* scene_class(void){return classes[2];}static void* static_class(void){return classes[3];}
static void* component_class(void){return classes[4];}static void* user_class(void){return classes[5];}static void* camera_class(void){return classes[6];}
static void*(*mv_scene_class)(void)=scene_class,*(*mv_static_class)(void)=static_class,*(*cu_component_class)(void)=component_class;
static void*(*cu_user_class)(void)=user_class,*(*cinema_camera_class)(void)=camera_class;
static int menu_visual_layout(const void* p,const void* cls,int size){
    if(!pv_alive(p)||*(const int*)((const char*)cls+0x58)<size)return 0;
    void* actual=*(void*const*)((const char*)p+0x10);
    return actual==cls||(cls==classes[2]&&(actual==classes[3]||actual==classes[4]||actual==classes[6]));
}
static int cinema_object(const void* p,const void* cls,int size){return menu_visual_layout(p,cls,size);}
static int cinema_ui_named(const void* p,const char* name,int size){
    return pv_alive(p)&&*(const int*)(*(const char*const*)((const char*)p+0x10)+0x58)>=size
        && ((*(void*const*)((const char*)p+0x10)==classes[1]&&!strcmp(name,"UI_HealthVignette_BP_C"))
        ||(*(void*const*)((const char*)p+0x10)==classes[5]&&!strcmp(name,"UI_HealthVignette_Widget_C")));
}
static const char* field_names[]={"HealthHud","Instigator","MessageTextWidget","MessageWidget","HeartbeatBG","HeartbeatWaveform","VignetteQte"};
static const char* cinema_field_property(const void* object,const char* name,const char* kind){
    lookups++;
    if(strcmp(kind,"ObjectProperty"))return 0;
    for(int i=0;i<7;i++)if(!strcmp(name,field_names[i])&&((i==0&&object==objects[0])||(i>0&&object==objects[1])))return(const char*)properties[i];
    return 0;
}
static int fv_descends_from(void* part,const void* root){
    for(int i=0;part&&i<16;i++){if(part==root)return 1;if(!menu_visual_layout(part,classes[2],0x1f0))return 0;part=*(void*const*)((char*)part+0xc0);}return 0;
}
static void* get_active(void){return active;}static void* get_bio4(void){return bio4;}static unsigned get_type(void){return(unsigned)context;}
static void*(*phone_active)(void)=get_active,*(*hs_bio4)(void)=get_bio4;static unsigned(*phone_type)(void)=get_type;
static int cinema_on_game_thread(void){return on_gt;}static long ptr_now_ms(void){return now;}
static void* get_widget(const void* component){assert(component==objects[8]);return current_widget;}
static void*(*cu_get_widget)(const void*)=get_widget;
static int is_viewport(const void* widget){assert(widget==current_widget||widget==objects[9]);return in_viewport;}
static void native_add(void* widget,int order){assert(widget==current_widget&&order==8);adds++;in_viewport=add_ok;}
static void native_remove(void* widget){assert(widget==current_widget||widget==objects[9]);removes++;in_viewport=0;}
static int(*cu_in_viewport)(const void*)=is_viewport;static void(*cu_add)(void*,int)=native_add;static void(*cu_remove)(void*)=native_remove;
static struct cinema_ui_vec2 draw_size(const void* component){assert(component==objects[8]);return(struct cinema_ui_vec2){240,50};}
static struct cinema_ui_vec2(*cu_draw_size)(const void*)=draw_size,(*fv_current_draw_size)(const void*)=draw_size;
static float viewport_scale(void* widget){assert(widget==current_widget);return dpi;}static float(*cu_viewport_scale)(void*)=viewport_scale;
static void set_scale(void* p,struct cinema_ui_vec2 v){memcpy((char*)p+0x98,&v,8);}static void set_pivot(void* p,struct cinema_ui_vec2 v){memcpy((char*)p+0xac,&v,8);}
static void set_position(void* p,struct cinema_ui_vec2 v,int remove_dpi){if(remove_dpi){v.x/=dpi;v.y/=dpi;}memcpy((char*)p+0x1f0,&v,8);}
static void set_size(void* p,struct cinema_ui_vec2 v){memcpy((char*)p+0x1f8,&v,8);}static void set_alignment(void* p,struct cinema_ui_vec2 v){memcpy((char*)p+0x200,&v,8);}
static void set_anchors(void* p,struct cinema_ui_anchors v){memcpy((char*)p+0x1e0,&v,16);}
static void set_visibility(void* p,unsigned char v){((unsigned char*)p)[0xc3]=v;}
static void(*cu_set_scale)(void*,struct cinema_ui_vec2)=set_scale,(*cu_set_pivot)(void*,struct cinema_ui_vec2)=set_pivot;
static void(*cu_set_position)(void*,struct cinema_ui_vec2,int)=set_position;static void(*cu_set_size)(void*,struct cinema_ui_vec2)=set_size;
static void(*cu_set_alignment)(void*,struct cinema_ui_vec2)=set_alignment;static void(*cu_set_anchors)(void*,struct cinema_ui_anchors)=set_anchors;
static void(*wt_set_visibility)(void*,unsigned char)=set_visibility;
static void native_hidden(void* part,int hidden,int propagate){assert(!propagate);unsigned short* f=(unsigned short*)((char*)part+0x14c);if(hidden)*f|=0x400;else *f&=~0x400;}
static void(*mv_hidden)(void*,int,int)=native_hidden;
static void(*cinema_health_view)(void*,void*);
#define LOGF(...) ((void)0)
#include "../re4fix_ui_health.h"
static unsigned short* flags(int index){return(unsigned short*)(objects[index]+0x14c);}
static void ready(void){
    memset(objects,0,sizeof objects);memset(classes,0,sizeof classes);memset(properties,0,sizeof properties);memset(&fv_health,0,sizeof fv_health);memset(fv_health_properties,0,sizeof fv_health_properties);
    for(int i=0;i<8;i++)*(int*)(classes[i]+0x58)=0x1200;
    int object_classes[]={0,1,6,2,2,3,3,3,4,5,7,7};
    for(int i=0;i<12;i++){alive[i]=serials[i]=1;*(void**)(objects[i]+0x10)=classes[object_classes[i]];}
    int offsets[]={0x600,0x118,0x220,0x228,0x238,0x240,0x248};int targets[]={1,0,8,9,5,6,7};
    for(int i=0;i<7;i++){*(int*)(properties[i]+0x34)=1;*(int*)(properties[i]+0x38)=8;*(int*)(properties[i]+0x4c)=offsets[i];*(void**)(objects[i?1:0]+offsets[i])=objects[targets[i]];}
    *(void**)(objects[0]+0x2d8)=objects[2];*(void**)(objects[1]+0x130)=objects[3];
    for(int i=3;i<=8;i++){*(void**)(objects[i]+0x20)=objects[1];*(void**)(objects[i]+0xa0)=objects[1];if(i>3)*(void**)(objects[i]+0xc0)=objects[3];*flags(i)=0x20;memset(objects[i]+0x1c0,0xa6,48);}
    set_scale(objects[9],(struct cinema_ui_vec2){1.2f,1.3f});set_pivot(objects[9],(struct cinema_ui_vec2){.1f,.2f});
    set_position(objects[9],(struct cinema_ui_vec2){5,6},0);set_size(objects[9],(struct cinema_ui_vec2){123,45});set_alignment(objects[9],(struct cinema_ui_vec2){.2f,.3f});
    set_anchors(objects[9],(struct cinema_ui_anchors){.1f,.2f,.3f,.4f});objects[9][0x100]=0x55;memset(objects[1]+0x700,0xa5,32);
    active=bio4=objects[0];current_widget=objects[9];on_gt=add_ok=fv_health_ready=1;context=1;adds=removes=in_viewport=0;
    g_comp_w=2400;g_comp_h=1080;dpi=2;now=1000;fv_health_presented_ms=0;lookups=0;
}
static void tick(void){touch_health_view(objects[2],0);}
static void presented(void){touch_health_flat_presented(now);tick();}
int main(void){
    ready();unsigned char actor[0x1200],pawn[0x1200],widget[0x1200],scene[6][0x1200];
    memcpy(actor,objects[1],sizeof actor);memcpy(pawn,objects[0],sizeof pawn);memcpy(widget,objects[9],sizeof widget);memcpy(scene,objects[3],sizeof scene);
    tick();assert(!fv_health.used&&!adds&&*flags(8)==0x20);
    presented();assert(fv_health.used&&fv_health.added&&in_viewport&&adds==1);
    assert(*flags(8)==0x420&&*flags(5)==0x420&&*flags(6)==0x420&&*flags(7)==0x420);
    assert(!memcmp(actor,objects[1],sizeof actor)&&!memcmp(pawn,objects[0],sizeof pawn)&&objects[9][0x100]==0x55);
    for(int i=3;i<=8;i++){unsigned short saved=*flags(i);*flags(i)=0x20;assert(!memcmp(scene[i-3],objects[i],0x1200));*flags(i)=saved;}
    struct cinema_ui_vec2 at,scale;memcpy(&at,objects[9]+0x1f0,8);memcpy(&scale,objects[9]+0x98,8);
    assert(fabsf(at.x*dpi-g_comp_w*.055f)<.001f&&fabsf(at.y*dpi-g_comp_h*.32f)<.001f&&50*scale.y*dpi<=g_comp_h*.05f+.001f);
    assert(lookups==7);for(int i=0;i<20;i++)tick();assert(adds==1&&lookups==7);
    now=1500;tick();assert(fv_health.used);now=1501;tick();assert(!fv_health.used&&!in_viewport&&removes==1);
    for(int i=5;i<=8;i++)assert(*flags(i)==0x20);assert(!memcmp(widget,objects[9],sizeof widget));
    ready();touch_health_flat_presented(now+1);tick();assert(!fv_health.used);
    ready();*flags(8)=0;presented();assert(fv_health.used&&!fv_health.added&&!adds&&*flags(8)==0&&*flags(5)==0x420);
    *flags(8)=0x20;tick();assert(fv_health.added&&adds==1&&*flags(8)==0x420);
    *flags(8)&=~0x20;tick();assert(fv_health.used&&!fv_health.added&&!in_viewport&&*flags(8)==0&&*flags(5)==0x420);
    *flags(5)=*flags(6)=*flags(7)=0;tick();assert(!fv_health.used&&*flags(7)==0);
    ready();*flags(7)|=0x400;presented();context=3;tick();assert(*flags(7)==0x420&&*flags(5)==0x20);
    for(int test=0;test<8;test++){
        ready();touch_health_flat_presented(now);
        if(test==0)*(void**)(objects[1]+0x118)=objects[10];if(test==1)*(void**)(objects[8]+0xa0)=objects[10];
        if(test==2)*(void**)(objects[5]+0xc0)=objects[10];if(test==3)add_ok=0;
        if(test==4)*(int*)(properties[0]+0x38)=4;if(test==5)active=objects[10];if(test==6)on_gt=0;if(test==7)dpi=NAN;
        tick();assert(!fv_health.used&&!in_viewport&&*flags(8)==0x20&&*flags(7)==0x20);
    }
    ready();presented();serials[8]++;context=3;tick();assert(!fv_health.used&&*flags(8)==0x420&&*flags(5)==0x20);
    ready();presented();*(void**)(objects[1]+0x118)=objects[10];tick();assert(!fv_health.used&&*flags(8)==0x420);
    ready();presented();g_comp_w=1920;g_comp_h=1080;dpi=1.5f;tick();assert(fv_health.width==1920&&fv_health.dpi==1.5f&&adds==1);
    ready();presented();memcpy(objects[10],widget,sizeof widget);current_widget=objects[10];*(void**)(objects[1]+0x228)=objects[10];
    tick();assert(fv_health.used&&fv_health.added&&adds==2&&removes==1&&fv_health.original_hidden==0);
    assert(*flags(8)==0x420&&*flags(5)==0x420);now+=501;tick();assert(!fv_health.used&&*flags(8)==0x20&&*flags(5)==0x20&&!memcmp(objects[10],widget,sizeof widget));
    ready();in_viewport=1;presented();assert(!fv_health.used&&in_viewport&&!adds&&!removes&&*flags(5)==0x20);
    puts("PASS: actual confirmed flat-dial-only native VR leaf masking,500ms/future expiry,20 stable frames,live status beside dial/DPI/resize; original HP,QTE,audio,materials,alpha,visibility,all transforms immutable; weak-owned viewport/leaf restoration and binding/thread/add-failure refusals");
    return 0;
}
