#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
struct cinema_ui_weak{int index,serial;};
enum{FLOATER=1,COMPONENT,INFO,NOTE,TREE,PANEL,IMAGE,TEXT,SLOT,CONTROLLER};
static unsigned char objects[80][0x600];static int alive[80],serials[80],kinds[80],calls;
static int cu_ready=1;
static int index_of(const void* object){for(int i=0;i<80;i++)if(object==objects[i])return i;return -1;}
static int pv_alive(const void* object){int i=index_of(object);return i>=0&&alive[i];}
static void weak_assign(struct cinema_ui_weak* weak,const void* object){weak->index=index_of(object);weak->serial=weak->index<0?0:serials[weak->index];}
static void* weak_get(const struct cinema_ui_weak* weak){return weak->index>=0&&weak->index<80&&alive[weak->index]&&serials[weak->index]==weak->serial?objects[weak->index]:0;}
static void(*cu_weak_assign)(struct cinema_ui_weak*,const void*)=weak_assign;
static void*(*cu_weak_get)(const struct cinema_ui_weak*)=weak_get;
static void* user_class(void){return(void*)(uintptr_t)INFO;}static void* component_class(void){return(void*)(uintptr_t)COMPONENT;}
static void* image_class(void){return(void*)(uintptr_t)IMAGE;}static void* panel_class(void){return(void*)(uintptr_t)PANEL;}
static void* get_widget(const void* component){return *(void* const*)((const char*)component+0x498);}
static void*(*cu_user_class)(void)=user_class,*(*cu_component_class)(void)=component_class,*(*cu_get_widget)(const void*)=get_widget;
static int cinema_object(const void* object,const void* cls,int minimum){
    int i=index_of(object);if(i<0||!alive[i]||minimum>0x600)return 0;
    return (uintptr_t)cls==(unsigned)kinds[i] || ((uintptr_t)cls==INFO&&kinds[i]==NOTE);
}
static int cinema_ui_named(const void* object,const char* cls,int minimum){
    int kind=!strcmp(cls,"InfoRing_C")?INFO:!strcmp(cls,"NoteInstruction_C")?NOTE:!strcmp(cls,"WidgetTree")?TREE:
        !strcmp(cls,"ControllerIcon3d_C")?CONTROLLER:FLOATER;
    int i=index_of(object);return i>=0&&alive[i]&&kinds[i]==kind&&minimum<=0x600;
}
static int pv_component(const void* component,const void* owner,int minimum){
    return cinema_object(component,(void*)(uintptr_t)COMPONENT,minimum)&&*(void* const*)((const char*)component+0x20)==owner
        &&*(void* const*)((const char*)component+0xa0)==owner;
}
static void* cinema_ui_property(const void* object,const char* cls,const char* field,unsigned offset){
    assert((!strcmp(cls,"FloatingUI_BP_C")&&((!strcmp(field,"Widget")&&offset==0x2d8)||(!strcmp(field,"WidgetNote")&&offset==0x280)))
        ||(!strcmp(cls,"ControllerIcon3d_C")&&!strcmp(field,"Widget1")&&offset==0x270));
    return cinema_ui_named(object,cls,0x578)?*(void* const*)((const char*)object+offset):0;
}
static void phone_inspect_fname(const void*,char*,int);
#include "re4fix_prompt_glyph.h"
static const char* const note_names[]={"Image_A_3","Image_A_6","Image_B_2","Image_X_2","Image_Y_2"};
static void phone_inspect_fname(const void* field,char* out,int capacity){
    uint64_t id;memcpy(&id,field,8);
    const char* name=id>=100&&id<123?pv_glyph_names[id-100]:id>=200&&id<205?note_names[id-200]:
        id==300?"ImageQteRing":id==301?"NextArrow":"CommandText";
    assert((int)strlen(name)<capacity);memcpy(out,name,strlen(name)+1);
}
static float native_opacity(const void* object){float value;assert(pv_alive(object));memcpy(&value,(const char*)object+0xc4,4);return value;}
static void authored_opacity(void* object,float value){memcpy((char*)object+0xc4,&value,4);}
static void set_opacity(void* object,float opacity){
    int i=index_of(object);assert(((i>=9&&i<=36)||i==79)&&isfinite(opacity)&&opacity>=0&&opacity<=1);
    authored_opacity(object,opacity);++calls;
}
static void* panel_slots[2][32];
static void panel_children(unsigned panel,unsigned first,unsigned count,unsigned slot_base){
    for(unsigned i=0;i<count;i++) {
        unsigned slot=slot_base+i;kinds[slot]=SLOT;*(void**)(objects[slot]+0x20)=objects[panel];
        *(void**)(objects[slot]+0x30)=objects[first+i];panel_slots[panel-7][i]=objects[slot];
    }
    struct{void**data;int count,max;} array={panel_slots[panel-7],(int)count,(int)count};memcpy(objects[panel]+0x108,&array,sizeof array);
}
static void setup(void){
    memset(objects,0,sizeof objects);memset(kinds,0,sizeof kinds);memset(pv_glyph_mask,0,sizeof pv_glyph_mask);calls=0;
    for(unsigned i=0;i<80;i++)alive[i]=serials[i]=1;
    kinds[0]=FLOATER;kinds[1]=kinds[2]=COMPONENT;kinds[3]=INFO;kinds[4]=NOTE;kinds[5]=kinds[6]=TREE;kinds[7]=kinds[8]=PANEL;
    *(void**)(objects[0]+0x2d8)=objects[1];*(void**)(objects[0]+0x280)=objects[2];
    for(unsigned i=0;i<2;i++) {
        *(void**)(objects[1+i]+0x20)=objects[0];*(void**)(objects[1+i]+0xa0)=objects[0];*(void**)(objects[1+i]+0x498)=objects[3+i];
        *(void**)(objects[3+i]+0x1a8)=objects[5+i];*(void**)(objects[5+i]+0x20)=objects[3+i];
        *(void**)(objects[5+i]+0x28)=objects[7+i];*(void**)(objects[7+i]+0x20)=objects[5+i];
    }
    for(unsigned i=9;i<=36;i++) {
        kinds[i]=IMAGE;*(void**)(objects[i]+0x20)=objects[i<32?5:6];
        *(uint64_t*)(objects[i]+0x18)=i<32?100+i-9:200+i-32;objects[i][0xc3]=(unsigned char)(i%5);
        authored_opacity(objects[i],(float)(i%5)*.25f);
    }
    for(unsigned i=37;i<=40;i++) {
        kinds[i]=i==38?TEXT:IMAGE;*(void**)(objects[i]+0x20)=i==40?objects[6]:objects[5];
        *(uint64_t*)(objects[i]+0x18)=i==37?300:i==39?301:i==40?100:302;objects[i][0xc3]=3;
        authored_opacity(objects[i],.9f);
    }
    panel_children(7,9,23,42);
    for(unsigned i=0;i<4;i++) {
        kinds[65+i]=SLOT;*(void**)(objects[65+i]+0x20)=objects[7];*(void**)(objects[65+i]+0x30)=objects[37+i];panel_slots[0][23+i]=objects[65+i];
    }
    *(int*)(objects[7]+0x110)=*(int*)(objects[7]+0x114)=27;
    panel_children(8,32,5,69);
    pv_glyph_ready=1;pv_image_class=image_class;pv_panel_class=panel_class;pv_glyph_opacity=native_opacity;pv_glyph_set_opacity=set_opacity;
}
static void controller_setup(void){
    kinds[74]=CONTROLLER;kinds[75]=COMPONENT;kinds[76]=INFO;kinds[77]=TREE;kinds[78]=PANEL;kinds[79]=IMAGE;kinds[41]=SLOT;
    *(void**)(objects[74]+0x270)=objects[75];*(void**)(objects[75]+0x20)=objects[74];*(void**)(objects[75]+0xa0)=objects[74];
    *(void**)(objects[75]+0x498)=objects[76];*(void**)(objects[76]+0x1a8)=objects[77];*(void**)(objects[77]+0x20)=objects[76];
    *(void**)(objects[77]+0x28)=objects[78];*(void**)(objects[78]+0x20)=objects[77];*(void**)(objects[79]+0x20)=objects[77];
    *(uint64_t*)(objects[79]+0x18)=100;objects[79][0xc3]=0;
    authored_opacity(objects[79],1);
    *(void**)(objects[41]+0x20)=objects[78];*(void**)(objects[41]+0x30)=objects[79];
    static void* child[1];child[0]=objects[41];
    struct{void**data;int count,max;} array={child,1,1};memcpy(objects[78]+0x108,&array,sizeof array);
}
static void assert_native(void){for(unsigned i=9;i<=36;i++)assert(objects[i][0xc3]==i%5&&native_opacity(objects[i])==(float)(i%5)*.25f);}
static void assert_transparent(void){for(unsigned i=9;i<=36;i++)assert(native_opacity(objects[i])==0&&objects[i][0xc3]==i%5);}
int main(void){
    setup();unsigned char before[sizeof objects];memcpy(before,objects,sizeof before);
    pv_glyph_tick(objects[0],0,0,0);assert(!calls&&!memcmp(before,objects,sizeof before));
    pv_glyph_tick(objects[0],0,0,1);assert_transparent();int changed=calls;assert(changed>0);
    pv_glyph_tick(objects[0],0,0,1);assert(calls==changed);
    for(unsigned i=0;i<80;i++)for(unsigned byte=0;byte<0x600;byte++)
        if(i<9||i>36||byte<0xc4||byte>=0xc8)assert(objects[i][byte]==before[i*0x600+byte]);
    pv_glyph_tick(objects[0],0,0,0);assert_native();assert(!memcmp(before,objects,sizeof before));
    setup();for(unsigned i=9;i<=36;i++)authored_opacity(objects[i],1);
    pv_glyph_tick(objects[0],0,0,1);assert_transparent();pv_glyph_tick(objects[0],0,0,0);
    for(unsigned i=9;i<=36;i++)assert(native_opacity(objects[i])==1&&objects[i][0xc3]==i%5);
    // Repeated authored prompt updates and a pause/resume keep every Visibility, slot, caption and other byte.
    setup();memcpy(before,objects,sizeof before);
    for(unsigned frame=0;frame<18;frame++) {
        for(unsigned i=9;i<=36;i++)authored_opacity(objects[i],(float)(i%5)*.25f);
        pv_glyph_tick(objects[0],0,0,1);assert_transparent();
        for(unsigned i=0;i<80;i++)for(unsigned byte=0;byte<0x600;byte++)
            if(i<9||i>36||byte<0xc4||byte>=0xc8)assert(objects[i][byte]==before[i*0x600+byte]);
        if(frame==8){pv_glyph_tick(0,0,0,1);assert_native();pv_glyph_tick(objects[0],0,0,1);assert_transparent();}
    }
    pv_glyph_tick(0,0,0,0);assert(!memcmp(before,objects,sizeof before));
    setup();pv_glyph_tick(objects[0],0,0,1);authored_opacity(objects[9],.25f);pv_glyph_tick(objects[0],0,0,1);pv_glyph_tick(0,0,0,1);
    assert(native_opacity(objects[9])==.25f);for(unsigned i=10;i<=36;i++)assert(native_opacity(objects[i])==(float)(i%5)*.25f);
    setup();pv_glyph_tick(objects[0],0,0,1);authored_opacity(objects[9],.5f);pv_glyph_tick(0,0,0,0);assert(native_opacity(objects[9])==.5f);
    setup();pv_glyph_tick(objects[0],0,0,1);serials[9]++;pv_glyph_tick(0,0,0,0);assert(native_opacity(objects[9])==0);
    setup();pv_glyph_tick(objects[0],0,0,1);*(void**)(objects[1]+0x498)=0;pv_glyph_tick(objects[0],0,0,1);for(unsigned i=9;i<32;i++)assert(native_opacity(objects[i])==(float)(i%5)*.25f);
    setup();*(void**)(objects[5]+0x20)=objects[4];pv_glyph_tick(objects[0],0,0,1);for(unsigned i=9;i<32;i++)assert(native_opacity(objects[i])==(float)(i%5)*.25f);
    setup();*(int*)(objects[7]+0x110)=65;pv_glyph_tick(objects[0],0,0,1);for(unsigned i=9;i<32;i++)assert(native_opacity(objects[i])==(float)(i%5)*.25f);
    setup();*(void**)(objects[42]+0x30)=objects[7];pv_glyph_tick(objects[0],0,0,1);assert(native_opacity(objects[9])==1);
    setup();authored_opacity(objects[9],NAN);pv_glyph_tick(objects[0],0,0,1);assert(isnan(native_opacity(objects[9]))&&native_opacity(objects[11])==.25f);
    setup();controller_setup();void* controllers[4]={objects[74],0,0,0};memcpy(before,objects,sizeof before);
    pv_glyph_tick(objects[0],controllers,4,0);assert(!calls&&!memcmp(before,objects,sizeof before));
    pv_glyph_tick(objects[0],controllers,4,1);assert_transparent();assert(native_opacity(objects[79])==0&&objects[79][0xc3]==0&&objects[37][0xc3]==3&&objects[39][0xc3]==3);
    for(unsigned i=0;i<80;i++)for(unsigned byte=0;byte<0x600;byte++)
        if(((i<9||i>36)&&i!=79)||byte<0xc4||byte>=0xc8)assert(objects[i][byte]==before[i*0x600+byte]);
    pv_glyph_tick(objects[0],controllers,4,0);assert(!memcmp(before,objects,sizeof before));
    pv_glyph_tick(objects[0],controllers,4,1);pv_glyph_tick(objects[0],0,0,1);assert(native_opacity(objects[79])==1);
    controllers[0]=0;controllers[3]=objects[74];
    pv_glyph_tick(objects[0],controllers,~0u,1);assert(native_opacity(objects[79])==0&&pv_glyph_mask[5].count==1);
    for(unsigned i=2;i<5;i++)assert(!pv_glyph_mask[i].count);
    pv_glyph_tick(0,0,0,0);assert(native_opacity(objects[79])==1);controllers[0]=objects[74];controllers[3]=0;
    setup();controller_setup();*(void**)(objects[75]+0xa0)=objects[0];pv_glyph_tick(objects[0],controllers,1,1);assert(native_opacity(objects[79])==1);
    setup();controller_setup();kinds[74]=FLOATER;pv_glyph_tick(objects[0],controllers,1,1);assert(native_opacity(objects[79])==1);
    setup();controller_setup();kinds[76]=NOTE;pv_glyph_tick(objects[0],controllers,1,1);assert(native_opacity(objects[79])==1);
    setup();controller_setup();pv_glyph_tick(objects[0],controllers,1,1);*(void**)(objects[75]+0x498)=0;
    pv_glyph_tick(objects[0],controllers,1,1);assert(native_opacity(objects[79])==1);
#ifdef _WIN32
    unsigned char* image=VirtualAlloc(0,0xa000000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(image);
    const unsigned addresses[]={0x7e01294,0x7e1495c,0x7da4524,0x7da46d4,0x9f6b718+36,0x9f5abd8+36,0x9f5a358+36,0x9f7bf38+36,0x9f6ff30+36};
    const unsigned getter[]={0xd10183ff,0xf90023f3,0xa9057bfd,0x910143fd},opacity_getter[]={0xfc1c0fe8,0xa90157f6,0xa9024ff4,0xa9037bfd},setter[]={0xfc1d0fe8,0xf90007f5,0xa9014ff4,0xa9027bfd};
    memcpy(image+addresses[0],getter,16);memcpy(image+addresses[1],getter,16);memcpy(image+addresses[2],opacity_getter,16);memcpy(image+addresses[3],setter,16);
    const unsigned offsets[]={0x1a8,0x108,0x30,0x28,0xc4};for(unsigned i=0;i<5;i++)*(unsigned*)(image+addresses[4+i])=offsets[i];
    install_touch_prompt_glyph((uintptr_t)image);assert(pv_glyph_ready);
    for(unsigned i=0;i<9;i++){*(unsigned*)(image+addresses[i])^=1;install_touch_prompt_glyph((uintptr_t)image);assert(!pv_glyph_ready);*(unsigned*)(image+addresses[i])^=1;}
    cu_ready=0;install_touch_prompt_glyph((uintptr_t)image);assert(!pv_glyph_ready);VirtualFree(image,0,MEM_RELEASE);
#endif
    puts("PASS: exact InfoRing23/Note5/Floater+bounded4Controller glyphs use native render opacity only; Off/On/pause/resume/Off18authoredframes preserve every Visibility/slot/caption/layout/ring/arrow/foreign byte; original and latest authored alpha, weak serial, source loss, ownership, malformed/cyclic tree and9installer guards");
    return 0;
}
