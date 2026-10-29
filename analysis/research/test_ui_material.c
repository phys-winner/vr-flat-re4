#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include GUARD_HEADER

struct fake_object {union {void* alignment;unsigned char bytes[0x600];};void* overrides[1];};
static struct fake_object widget[130],row,clip,front,original,external,mic,source_mid,mids[600];
static unsigned char class_widget[0x60],class_interface[0x60],class_material[0x60],class_instance[0x60],class_dynamic[0x60];
static const void* widget_chain[1],*interface_chain[1],*material_chain[2],*instance_chain[2],*dynamic_chain[3];
static struct {const void* pointer;int serial,alive;} objects[1200];
static unsigned object_count,mid_count;
static int on_gt=1,sets,sorts,finds,loads,found=1,fail_once,wrong_texture,emitted_logs;
static long now=1000;
struct cinema_ui_weak {int index,serial;};
static void weak_assign(struct cinema_ui_weak* weak,const void* object){
    *weak=(struct cinema_ui_weak){-1,0};if(!object)return;
    for(unsigned i=0;i<object_count;i++)if(objects[i].pointer==object){*weak=(struct cinema_ui_weak){(int)i,objects[i].serial};return;}
    assert(object_count<1200);objects[object_count]=(typeof(*objects)){object,1,1};*weak=(struct cinema_ui_weak){(int)object_count++,1};
}
static void* weak_get(const struct cinema_ui_weak* weak){
    if(weak->index<0||weak->index>=(int)object_count)return 0;
    return objects[weak->index].alive&&objects[weak->index].serial==weak->serial?(void*)objects[weak->index].pointer:0;
}
static void (*cu_weak_assign)(struct cinema_ui_weak*,const void*)=weak_assign;
static void* (*cu_weak_get)(const struct cinema_ui_weak*)=weak_get;
static int pv_alive(void* object){for(unsigned i=0;i<object_count;i++)if(objects[i].pointer==object)return objects[i].alive;return 0;}
static int cinema_on_game_thread(void){return on_gt;}
static long ptr_now_ms(void){return now;}
static void obj_name(const void* object,char* out,int size){snprintf(out,size,"%s",object==clip.bytes?"Widget3DPassThrough_PlaneClip4_Mat":"OtherMaterial");}
#define LOGF(...) ((void)++emitted_logs)
#include MATERIAL_HEADER

static void* widget_class(void){return class_widget;}
static void* interface_class(void){return class_interface;}
static void* dynamic_class(void){return class_dynamic;}
static void* material_class(void){return class_material;}
static void make_class(unsigned char* cls,int size,int depth,const void** chain){
    *(int*)(cls+0x58)=size;*(int*)(cls+0x38)=depth;*(const void***)(cls+0x30)=chain;
}
static void make_object(struct fake_object* object,void* cls){
    memset(object,0,sizeof *object);*(void**)(object->bytes+0x10)=cls;
    struct cinema_ui_weak weak;weak_assign(&weak,object->bytes);
}
static void make_widget(struct fake_object* object){
    make_object(object,class_widget);*(void**)(object->bytes+0x408)=object->overrides;
    *(void**)(object->bytes+0x4d8)=original.bytes;*(void**)(object->bytes+0x4f8)=object->bytes+0x580;
}
static void* get_material(const void* component,int slot){
    assert(slot==0);const unsigned char* bytes=component;
    return *(int*)(bytes+0x410)&&**(void***)(bytes+0x408)?**(void***)(bytes+0x408):*(void* const*)(bytes+0x4d8);
}
static void set_material(void* component,int slot,void* material){
    assert(slot==0&&mid_count<600);sets++;
    unsigned char* bytes=component;**(void***)(bytes+0x408)=material;*(int*)(bytes+0x410)=1;*(int*)(bytes+0x414)=1;
    struct fake_object* mid=&mids[mid_count++];make_object(mid,class_dynamic);
    *(void**)(mid->bytes+0x20)=component;*(void**)(mid->bytes+0xd0)=get_material(component,0);
    *(void**)(mid->bytes+0xe0)=*(void**)(bytes+0x4f8);
    if(fail_once){fail_once=0;*(void**)(mid->bytes+0x20)=&row;}
    if(wrong_texture){wrong_texture=0;*(void**)(mid->bytes+0xe0)=row.bytes+0x580;}
    *(void**)(bytes+0x500)=mid->bytes;
}
static void set_priority(void* component,int priority){
    assert(cinema_on_game_thread()&&uim_world_widget(component));sorts++;
    *(int*)((unsigned char*)component+0x238)=priority;
}
static void* get_base(void* material){
    for(int depth=0;material&&depth<8;depth++){
        if(*(void**)((unsigned char*)material+0x10)==class_material)return material;
        material=*(void**)((unsigned char*)material+0xd0);
    }return 0;
}
static void name_ctor(void* name,const char* value,int add){assert(!strcmp(value,"SlateUI")&&add==1);*(unsigned long long*)name=17;}
static void* get_texture(void* material,unsigned long long name){assert(name==17);return *(void**)((unsigned char*)material+0xe0);}
static void* find_material(void* cls,void* outer,const unsigned short* path,int exact){
    assert(cls==class_interface&&!outer&&!exact);char ascii[160];unsigned i=0;
    for(;path[i]&&i<159;i++)ascii[i]=(char)path[i];ascii[i]=0;
    assert(strstr(ascii,"/Game/")&&strstr(ascii,"NoDepth"));finds++;return found?front.bytes:0;
}
static void* load_material(void* cls,void* outer,const unsigned short* path,const unsigned short* file,unsigned flags,void* map,int reconcile,void* context){
    assert(!file&&!flags&&!map&&reconcile==1&&!context);loads++;(void)find_material(cls,outer,path,0);return front.bytes;
}
static void configure(void){
    uim_widget_class=widget_class;uim_interface_class=interface_class;uim_dynamic_class=dynamic_class;uim_material_class=material_class;
    uim_set=set_material;uim_sort=set_priority;uim_get=get_material;uim_base=get_base;uim_texture=get_texture;uim_name=name_ctor;uim_find=find_material;uim_load=load_material;uim_ready=1;
}
int main(void){
    unsigned char* image=calloc(1,0xa300000);assert(image);
    for(unsigned i=0;i<sizeof fixture_checks/sizeof *fixture_checks;i++){
        fixture_image(image);image[fixture_checks[i]]^=1;install_ui_material((uintptr_t)image);assert(!uim_ready);
    }
    fixture_image(image);install_ui_material((uintptr_t)image);assert(uim_ready);free(image);configure();
    int diagnostic_before=emitted_logs;
    assert(!uim_reject(19,"fixture",0,0)&&emitted_logs==diagnostic_before+1);
    assert(!uim_reject(19,"fixture",0,0)&&!uim_reject(32,"fixture",0,0)&&emitted_logs==diagnostic_before+1);
    interface_chain[0]=class_interface+0x30;widget_chain[0]=class_widget+0x30;
    material_chain[0]=instance_chain[0]=dynamic_chain[0]=class_interface+0x30;
    material_chain[1]=class_material+0x30;instance_chain[1]=dynamic_chain[1]=class_instance+0x30;dynamic_chain[2]=class_dynamic+0x30;
    make_class(class_widget,0x600,0,widget_chain);make_class(class_interface,0x88,0,interface_chain);
    make_class(class_material,0x300,1,material_chain);make_class(class_instance,0x100,1,instance_chain);make_class(class_dynamic,0x100,2,dynamic_chain);
    make_object(&original,class_material);make_object(&external,class_material);make_object(&clip,class_material);make_object(&front,class_material);
    *(unsigned*)(front.bytes+0x22c)=1;make_object(&mic,class_instance);*(void**)(mic.bytes+0xd0)=clip.bytes;
    make_object(&source_mid,class_dynamic);*(void**)(source_mid.bytes+0xd0)=mic.bytes;
    make_widget(&row);row.overrides[0]=source_mid.bytes;*(int*)(row.bytes+0x410)=*(int*)(row.bytes+0x414)=1;
    make_widget(&widget[0]);make_widget(&widget[1]);
    ui_material_begin();assert(ui_material_clip(widget[0].bytes,row.bytes));assert(get_material(widget[0].bytes,0)==mic.bytes&&uim_mid_owned(widget[0].bytes,mic.bytes));
    int before=sets;assert(ui_material_clip(widget[0].bytes,row.bytes)&&sets==before);ui_material_end();
    ui_material_begin();ui_material_end();assert(get_material(widget[0].bytes,0)==original.bytes&&!widget[0].overrides[0]);
    widget[0].overrides[0]=external.bytes;
    ui_material_begin();assert(ui_material_clip(widget[0].bytes,row.bytes));ui_material_end();
    ui_material_begin();ui_material_end();assert(get_material(widget[0].bytes,0)==external.bytes);
    ui_material_begin();assert(ui_material_front(widget[1].bytes,3));assert(get_material(widget[1].bytes,0)==front.bytes&&finds==1&&!loads);ui_material_end();
    widget[1].overrides[0]=external.bytes;before=sets;ui_material_begin();ui_material_end();assert(sets==before&&get_material(widget[1].bytes,0)==external.bytes);
    *(unsigned*)(front.bytes+0x22c)=0;uim_front_material=(struct cinema_ui_weak){-1,0};now+=2000;before=sets;
    ui_material_begin();assert(!ui_material_front(widget[1].bytes,3)&&sets==before);int calls=finds;assert(!ui_material_front(widget[1].bytes,3)&&finds==calls);ui_material_end();
    *(unsigned*)(front.bytes+0x22c)=1;now+=2000;found=0;assert(ui_material_front(widget[1].bytes,3)&&loads==1);
    ui_material_begin();ui_material_end();
    before=sets;on_gt=0;assert(!ui_material_front(widget[1].bytes,3)&&!ui_material_clip(widget[0].bytes,row.bytes));ui_material_begin();ui_material_end();assert(sets==before);on_gt=1;
    widget[0].bytes[0x429]=1;assert(!ui_material_clip(widget[0].bytes,row.bytes));widget[0].bytes[0x429]=0;
    row.overrides[0]=original.bytes;assert(!ui_material_clip(widget[0].bytes,row.bytes));row.overrides[0]=source_mid.bytes;
    *(void**)(source_mid.bytes+0xd0)=source_mid.bytes;assert(!ui_material_clip(widget[0].bytes,row.bytes));*(void**)(source_mid.bytes+0xd0)=mic.bytes;
    fail_once=1;assert(!ui_material_clip(widget[0].bytes,row.bytes)&&get_material(widget[0].bytes,0)==external.bytes);
    wrong_texture=1;assert(!ui_material_clip(widget[0].bytes,row.bytes)&&get_material(widget[0].bytes,0)==external.bytes);
    ui_material_begin();assert(ui_material_clip(widget[0].bytes,row.bytes));ui_material_end();
    struct cinema_ui_weak dead;weak_assign(&dead,widget[0].bytes);objects[dead.index].alive=0;before=sets;
    ui_material_begin();ui_material_end();assert(sets==before);objects[dead.index].alive=1;
    make_widget(&widget[2]);*(int*)(widget[2].bytes+0x238)=-7;
    ui_material_begin();assert(ui_material_front(widget[2].bytes,2));ui_material_end();
    assert(*(int*)(widget[2].bytes+0x238)==2);
    int sort_before=sorts;before=sets;
    ui_material_begin();assert(ui_material_front(widget[2].bytes,2));ui_material_end();
    assert(sorts==sort_before&&sets==before);
    ui_material_begin();assert(ui_material_front(widget[2].bytes,5));ui_material_end();
    assert(*(int*)(widget[2].bytes+0x238)==5&&sorts==sort_before+1&&sets==before);
    ui_material_begin();ui_material_end();
    assert(*(int*)(widget[2].bytes+0x238)==-7&&get_material(widget[2].bytes,0)==original.bytes);
    sort_before=sorts;ui_material_begin();assert(ui_material_clip(widget[2].bytes,row.bytes));ui_material_end();
    ui_material_begin();ui_material_end();assert(sorts==sort_before&&*(int*)(widget[2].bytes+0x238)==-7);
    ui_material_begin();assert(ui_material_front(widget[2].bytes,1));ui_material_end();
    *(int*)(widget[2].bytes+0x238)=19;sort_before=sorts;
    ui_material_begin();assert(ui_material_front(widget[2].bytes,4));ui_material_end();
    assert(sorts==sort_before&&*(int*)(widget[2].bytes+0x238)==19);
    ui_material_begin();ui_material_end();assert(sorts==sort_before&&*(int*)(widget[2].bytes+0x238)==19);
    ui_material_begin();assert(ui_material_front(widget[2].bytes,1));ui_material_end();
    widget[2].overrides[0]=external.bytes;sort_before=sorts;before=sets;
    ui_material_begin();ui_material_end();
    assert(sorts==sort_before&&sets==before&&*(int*)(widget[2].bytes+0x238)==1&&get_material(widget[2].bytes,0)==external.bytes);
    make_widget(&widget[2]);set_material(widget[2].bytes,0,front.bytes);*(int*)(widget[2].bytes+0x238)=11;
    void* old_mid=*(void**)(widget[2].bytes+0x500);before=sets;
    ui_material_begin();assert(ui_material_front(widget[2].bytes,3));ui_material_end();assert(sets==before);
    ui_material_begin();ui_material_end();
    assert(sets==before&&get_material(widget[2].bytes,0)==front.bytes&&*(void**)(widget[2].bytes+0x500)==old_mid&&*(int*)(widget[2].bytes+0x238)==11);
    make_widget(&widget[2]);sort_before=sorts;before=sets;
    assert(!ui_material_front(widget[2].bytes,0)&&!ui_material_front(widget[2].bytes,129)&&sorts==sort_before&&sets==before);
    ui_material_begin();assert(ui_material_front(widget[2].bytes,2));ui_material_end();weak_assign(&dead,widget[2].bytes);
    objects[dead.index].serial++;sort_before=sorts;before=sets;ui_material_begin();ui_material_end();assert(sorts==sort_before&&sets==before);
    for(unsigned i=0;i<129;i++)make_widget(&widget[i]);
    ui_material_begin();for(unsigned i=0;i<128;i++)assert(ui_material_clip(widget[i].bytes,row.bytes));
    before=sets;assert(!ui_material_clip(widget[128].bytes,row.bytes)&&sets==before);ui_material_end();
    ui_material_begin();ui_material_end();for(unsigned i=0;i<128;i++)assert(get_material(widget[i].bytes,0)==original.bytes);
    puts("PASS: production material/MID/SlateUI ownership and raw override restoration; front sort updates/native cleanup preserve foreign priority/material and recycled identities, existing front material avoids MID recreation, clip priorities unchanged; guarded load/throttle and 128-slot limits");
    return 0;
}
