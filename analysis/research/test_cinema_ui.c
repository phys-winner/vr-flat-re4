#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
static int g_comp_w,g_comp_h;
static int phone_inspect_ready,thread_ok;
static void* (*cinema_active)(void),*(*cinema_pawn_class)(void);
static unsigned (*cinema_type)(void);
static int cinema_on_game_thread(void) {return thread_ok;}
static int cinema_object(const void* object,const void* target,int minimum) {
    const char* cls=object ? *(const char* const*)((const char*)object+0x10) : 0;
    if(!cls || !target)return 0;
    int size=*(const int*)(cls+0x58),depth=*(const int*)((const char*)target+0x38);
    int child=*(const int*)(cls+0x38);
    const void* const* chain=*(const void* const* const*)(cls+0x30);
    return size>=minimum && size<=0x10000 && depth>=0 && depth<=128
        && child>=depth && child<=256 && chain && chain[depth]==(const char*)target+0x30;
}
enum {PAWN=1,THEATRE,COMPONENT,USER,OBJECT_PROPERTY,OTHER_PROPERTY,THEATRE_FIELD,COMP_FIELD,WIDGET_FIELD,WRONG,TEXT_FIELD,TEXT,TREE,OVERLAY_SLOT,BOX_SLOT,OVERLAY,BOX};
static const char* names[]={"","VR4CutscenePlayerPawn_BP_C","CutsceneTheatreBox_C","WidgetComponent",
    "Subtitles_C","ObjectProperty","FloatProperty","TheatreBox","WidgetSubtitles","SubtitleWidget","Wrong","SubtitleText","TextBlock","WidgetTree","OverlaySlot","SizeBoxSlot","Overlay","SizeBox"};
static void phone_inspect_fname(const void* name,char* out,int capacity) {
    unsigned id=name ? *(const unsigned*)name : 0;assert(id<sizeof names/sizeof names[0]);
    snprintf(out,(unsigned)capacity,"%s",names[id]);
}
static unsigned font_color_logs,font_paint_logs,font_raw_logs,logged_font[20],logged_separate,logged_apply,logged_fallback;
static int logged_outline;static double logged_paint[10];static void* logged_text,*logged_slate,*logged_raw_text;
static void fake_log(const char* format,...) {
    if(strstr(format,"CINEMA FONT color"))font_color_logs++;
    if(strstr(format,"CINEMA FONT paint")){
        va_list args;va_start(args,format);font_paint_logs++;logged_text=va_arg(args,void*);
        logged_outline=va_arg(args,int);logged_separate=va_arg(args,unsigned);logged_apply=va_arg(args,unsigned);
        for(unsigned i=0;i<10;i++)logged_paint[i]=va_arg(args,double);
        logged_slate=va_arg(args,void*);logged_fallback=va_arg(args,unsigned);va_end(args);
    }
    if(strstr(format,"CINEMA FONT raw")){
        va_list args;va_start(args,format);font_raw_logs++;logged_raw_text=va_arg(args,void*);
        for(unsigned i=0;i<20;i++)logged_font[i]=va_arg(args,unsigned);va_end(args);
    }
}
#define LOGF(...) fake_log(__VA_ARGS__)
// Also lets this same regression run against the previous header, before the API is wired.
static void (*cu_set_auto_wrap)(void*,int);
static void* (*cu_overlay_slot_class)(void),*(*cu_box_slot_class)(void);
static void* (*cu_overlay_class)(void),*(*cu_box_class)(void);
static void (*cu_set_overlay_halign)(void*,int),(*cu_set_box_halign)(void*,int);
#include "../re4fix_cinema_ui.h"
static struct {
    unsigned char pawn[0x1290],theatre[0x528],component[0x550],widget[0x280],classes[11][0x100];
    unsigned char props[4][0x70],object_kind[16],other_kind[16];void* chains[11][1];
    unsigned char tree[0x30],text[0x2a0],font_object[0x30],font_material[0x30],outline_material[0x30];
    unsigned char overlay_slot[0x58],box_slot[0x60],overlay[0x130],box[0x158];
} f __attribute__((aligned(16)));
static unsigned type,serial;static int alive,in_view,add_success;
static unsigned component_serial,theatre_serial;static int component_alive,theatre_alive;
static unsigned text_serial,auto_wrap_calls,auto_wrap_enables,auto_wrap_restores;static int text_alive;
static unsigned slot_serial[2],halign_calls[2];static int slot_alive[2];
static unsigned added,removed,weak_gets,weak_assigns;
static unsigned main_pass_calls,main_pass_hides,main_pass_restores;
static float dpi;static struct cinema_ui_vec2 draw;
static unsigned layout_calls;
static int find_ok;
static void* active(void) {return f.pawn;}
static unsigned context(void) {return type;}
static void* pawn_class(void) {return f.classes[0];}
static void* component_class(void) {return f.classes[2];}
static void* user_class(void) {return f.classes[3];}
static void* text_class(void) {return f.classes[5];}
static void* tree_class(void) {return f.classes[6];}
static void* overlay_slot_class(void) {return f.classes[7];}
static void* box_slot_class(void) {return f.classes[8];}
static void* overlay_class(void) {return f.classes[9];}
static void* box_class(void) {return f.classes[10];}
static void* find_widget(const void* widget,const void* name) {assert(widget==f.widget && name==f.text+0x18);return find_ok ? f.text : 0;}
static float render_opacity(const void* widget) {assert(widget==f.widget || widget==f.text);return *(const float*)((const char*)widget+0xc4);}
static void* get_widget(const void* component) {assert(component==f.component);return *(void* const*)((const char*)component+0x498);}
static void weak_assign(struct cinema_ui_weak* weak,const void* object) {
    weak_assigns++;assert(!object || object==f.widget || object==f.component || object==f.theatre || object==f.text || object==f.overlay_slot || object==f.box_slot);
    weak->index=object==f.widget ? 1 : object==f.component ? 2 : object==f.theatre ? 3 : object==f.text ? 4 : object==f.overlay_slot ? 5 : object==f.box_slot ? 6 : -1;
    weak->serial=weak->index==1 ? (int)serial : weak->index==2 ? (int)component_serial
        : weak->index==3 ? (int)theatre_serial : weak->index==4 ? (int)text_serial : weak->index>=5 ? (int)slot_serial[weak->index-5] : 0;
}
static void* weak_get(const struct cinema_ui_weak* weak) {
    weak_gets++;
    if(weak->index==1)return alive && weak->serial==(int)serial ? f.widget : 0;
    if(weak->index==2)return component_alive && weak->serial==(int)component_serial ? f.component : 0;
    if(weak->index==3)return theatre_alive && weak->serial==(int)theatre_serial ? f.theatre : 0;
    if(weak->index==4)return text_alive && weak->serial==(int)text_serial ? f.text : 0;
    if(weak->index>=5 && weak->index<=6) {
        unsigned i=(unsigned)weak->index-5;return slot_alive[i] && weak->serial==(int)slot_serial[i] ? (i ? f.box_slot : f.overlay_slot) : 0;
    }
    return 0;
}
static void set_overlay_halign(void* slot,int alignment) {
    assert(slot==f.overlay_slot && thread_ok && alignment>=0 && alignment<=3);
    if(!alignment)assert(cu_added && in_view);
    halign_calls[0]++;f.overlay_slot[0x50]=(unsigned char)alignment;
}
static void set_box_halign(void* slot,int alignment) {
    assert(slot==f.box_slot && thread_ok && alignment>=0 && alignment<=3);
    if(!alignment)assert(cu_added && in_view);
    halign_calls[1]++;f.box_slot[0x58]=(unsigned char)alignment;
}
static void set_auto_wrap(void* text,int enabled) {
    assert(text==f.text && thread_ok && (enabled==0 || enabled==1));
    if(enabled)assert(cu_added && in_view);
    auto_wrap_calls++;if(enabled)auto_wrap_enables++;else auto_wrap_restores++;
    f.text[0x10d]=(f.text[0x10d]&0xfe)|(unsigned char)enabled;
}
static void set_main_pass(void* component,int enabled) {
    assert(component==f.component && thread_ok && (enabled==0 || enabled==1));
    main_pass_calls++;if(enabled)main_pass_restores++;else main_pass_hides++;
    unsigned* flags=(unsigned*)(f.component+0x208);*flags=(*flags&~0x20000u)|((unsigned)enabled<<17);
}
static int viewport(const void* widget) {assert(widget==f.widget);return in_view;}
static void add(void* widget,int z) {assert(widget==f.widget && z==10);added++;if(add_success)in_view=1;}
static void remove_widget(void* widget) {assert(widget==f.widget && in_view);removed++;in_view=0;}
static struct cinema_ui_vec2 draw_size(const void* component) {assert(component==f.component);return draw;}
static float viewport_scale(void* widget) {assert(widget==f.widget);return dpi;}
static void set_scale(void* widget,struct cinema_ui_vec2 value) {assert(widget==f.widget);memcpy(f.widget+0x98,&value,8);layout_calls++;}
static void set_pivot(void* widget,struct cinema_ui_vec2 value) {assert(widget==f.widget);memcpy(f.widget+0xac,&value,8);layout_calls++;}
static void set_position(void* widget,struct cinema_ui_vec2 value,int remove_dpi) {
    assert(widget==f.widget);if(remove_dpi) {value.x/=dpi;value.y/=dpi;}
    memcpy(f.widget+0x1f0,&value,8);memset(f.widget+0x1e0,0,16);layout_calls++;
}
static void set_size(void* widget,struct cinema_ui_vec2 value) {assert(widget==f.widget);memcpy(f.widget+0x1f8,&value,8);memset(f.widget+0x1e0,0,16);layout_calls++;}
static void set_alignment(void* widget,struct cinema_ui_vec2 value) {assert(widget==f.widget);memcpy(f.widget+0x200,&value,8);layout_calls++;}
static void set_anchors(void* widget,struct cinema_ui_anchors value) {assert(widget==f.widget);memcpy(f.widget+0x1e0,&value,16);layout_calls++;}
static void unchanged_except_layout(const void* before) {
    unsigned char copy[sizeof f];memcpy(copy,&f,sizeof f);
    size_t off=(size_t)(f.widget-(unsigned char*)&f);
    memcpy(copy+off+0x98,(const unsigned char*)before+off+0x98,8);
    memcpy(copy+off+0xac,(const unsigned char*)before+off+0xac,8);
    memcpy(copy+off+0x1e0,(const unsigned char*)before+off+0x1e0,40);
    off=(size_t)(f.component-(unsigned char*)&f)+0x208;
    *(unsigned*)(copy+off)=(*(const unsigned*)(copy+off)&~0x20000u)|(*(const unsigned*)((const char*)before+off)&0x20000u);
    off=(size_t)(f.text-(unsigned char*)&f)+0x10d;
    copy[off]=(copy[off]&0xfe)|(((const unsigned char*)before)[off]&1);
    off=(size_t)(f.overlay_slot-(unsigned char*)&f)+0x50;copy[off]=((const unsigned char*)before)[off];
    off=(size_t)(f.box_slot-(unsigned char*)&f)+0x58;copy[off]=((const unsigned char*)before)[off];
    assert(!memcmp(copy,before,sizeof f));
}
static void property(unsigned index,unsigned name,int offset) {
    unsigned char* p=f.props[index];*(void**)(p+8)=f.object_kind;*(unsigned*)(p+0x28)=name;
    *(int*)(p+0x34)=1;*(int*)(p+0x38)=8;*(int*)(p+0x4c)=offset;
}
static void ready(void) {
    memset(&f,0,sizeof f);
    int sizes[]={0x1290,0x528,0x550,0x280,0x280,0x2a0,0x30,0x58,0x60,0x130,0x158};
    unsigned ids[]={PAWN,THEATRE,COMPONENT,USER,WRONG,TEXT,TREE,OVERLAY_SLOT,BOX_SLOT,OVERLAY,BOX};
    for(unsigned i=0;i<11;i++) {
        f.chains[i][0]=f.classes[i]+0x30;*(void**)(f.classes[i]+0x30)=f.chains[i];
        *(int*)(f.classes[i]+0x58)=sizes[i];*(unsigned*)(f.classes[i]+0x18)=ids[i];
    }
    *(void**)(f.pawn+0x10)=f.classes[0];*(void**)(f.theatre+0x10)=f.classes[1];
    *(void**)(f.component+0x10)=f.classes[2];*(void**)(f.widget+0x10)=f.classes[3];
    *(unsigned*)f.object_kind=OBJECT_PROPERTY;*(unsigned*)f.other_kind=OTHER_PROPERTY;
    property(0,THEATRE_FIELD,0x1198);property(1,COMP_FIELD,0x2b0);property(2,WIDGET_FIELD,0x338);
    *(void**)(f.classes[0]+0x70)=f.props[0];*(void**)(f.classes[1]+0x70)=f.props[1];
    *(void**)(f.props[1]+0x58)=f.props[2];
    *(void**)(f.pawn+0x1198)=f.theatre;*(void**)(f.theatre+0x2b0)=f.component;
    *(void**)(f.theatre+0x338)=f.widget;*(void**)(f.component+0x498)=f.widget;
    *(void**)(f.component+0x20)=f.theatre;*(void**)(f.component+0xa0)=f.theatre;
    *(unsigned short*)(f.component+0x14c)=0x20;
    *(unsigned*)(f.component+0x208)=0xa506a55a;*(unsigned short*)(f.component+0x20c)=0x6b19;f.component[0x20e]=0xa1;
    memset(f.widget+0x40,0xa5,0x100); // Original text/timer/visibility payload is never edited.
    struct cinema_ui_vec2 scale={0.8f,1.2f},pivot={0.3f,0.7f},position={23,41},size={333,177},alignment={0.2f,0.8f};
    struct cinema_ui_anchors anchors={0.1f,0.2f,0.8f,0.9f};
    memcpy(f.widget+0x98,&scale,8);memcpy(f.widget+0xac,&pivot,8);
    memcpy(f.widget+0x1e0,&anchors,16);memcpy(f.widget+0x1f0,&position,8);
    memcpy(f.widget+0x1f8,&size,8);memcpy(f.widget+0x200,&alignment,8);
    *(void**)(f.widget+0x1a8)=f.tree;*(void**)(f.tree+0x10)=f.classes[6];*(void**)(f.tree+0x20)=f.widget;
    *(void**)(f.text+0x10)=f.classes[5];*(void**)(f.text+0x20)=f.tree;*(unsigned*)(f.text+0x18)=TEXT;
    *(void**)(f.overlay_slot+0x10)=f.classes[7];*(void**)(f.box_slot+0x10)=f.classes[8];
    *(void**)(f.overlay+0x10)=f.classes[9];*(void**)(f.box+0x10)=f.classes[10];
    *(void**)(f.tree+0x28)=f.box;*(void**)(f.text+0x28)=f.overlay_slot;
    *(void**)(f.overlay_slot+0x20)=f.overlay;*(void**)(f.overlay_slot+0x28)=f.overlay;*(void**)(f.overlay_slot+0x30)=f.text;
    *(void**)(f.overlay+0x20)=f.tree;*(void**)(f.overlay+0x28)=f.box_slot;
    *(void**)(f.box_slot+0x20)=f.box;*(void**)(f.box_slot+0x28)=f.box;*(void**)(f.box_slot+0x30)=f.overlay;
    *(void**)(f.box+0x20)=f.tree;
    f.overlay_slot[0x50]=f.box_slot[0x58]=2;f.overlay_slot[0x51]=f.box_slot[0x59]=1;
    property(3,TEXT_FIELD,0x230);*(void**)(f.classes[3]+0x70)=f.props[3];*(void**)(f.widget+0x230)=f.text;
    *(float*)(f.text+0xc4)=0.9f;*(float*)(f.widget+0xc4)=0.8f;
    *(void**)(f.text+0x188)=f.font_object;*(void**)(f.text+0x190)=f.font_material;*(void**)(f.text+0x1a0)=f.outline_material;
    *(int*)(f.text+0x1d0)=20;*(unsigned*)(f.text+0x1c8)=TEXT;
    *(int*)(f.text+0x198)=7;f.text[0x19c]=1;f.text[0x19d]=0;f.text[0x1d4]=2;
    {const float outline[4]={.25f,.5f,.75f,.875f},shadow[6]={1.25f,-2.5f,.1f,.2f,.3f,.4f};
        memcpy(f.text+0x1a8,outline,sizeof outline);memcpy(f.text+0x260,shadow,sizeof shadow);}
    *(void**)(f.text+0x290)=(void*)0x1234;
    f.text[0x10d]=0xa4;*(float*)(f.text+0x110)=0;
    void* objects[]={f.font_object,f.font_material,f.outline_material};
    for(unsigned i=0;i<3;i++) {*(void**)((char*)objects[i]+0x10)=f.classes[4];*(unsigned*)((char*)objects[i]+0x18)=WRONG;}
    float color[]={0.6f,0.7f,0.8f,1.0f};memcpy(f.text+0x150,color,16);f.text[0x160]=0;
    phone_inspect_ready=thread_ok=cu_ready=alive=add_success=1;type=3;serial=7;in_view=0;
    component_alive=theatre_alive=1;component_serial=11;theatre_serial=13;
    text_alive=1;text_serial=17;auto_wrap_calls=auto_wrap_enables=auto_wrap_restores=0;
    for(unsigned i=0;i<2;i++){slot_alive[i]=1;slot_serial[i]=19+i;halign_calls[i]=0;}
    cu_widget=(struct cinema_ui_weak){-1,0};cu_added=cu_attempted=cu_layout=0;cu_logs=cu_rejections=0;
    cu_quad_component=cu_quad_theatre=(struct cinema_ui_weak){-1,0};cu_quad_changed=cu_quad_original=0;
    cu_wrap_text=(struct cinema_ui_weak){-1,0};cu_wrap_changed=0;
    cu_fill_slots[0]=cu_fill_slots[1]=(struct cinema_ui_weak){-1,0};cu_fill_changed=0;
    added=removed=weak_gets=weak_assigns=0;
    main_pass_calls=main_pass_hides=main_pass_restores=0;
    layout_calls=0;g_comp_w=2400;g_comp_h=1080;dpi=1.5f;draw=(struct cinema_ui_vec2){1024,128};
    cu_font_last=0;cu_font_name=0;cu_font_logs=font_color_logs=font_paint_logs=font_raw_logs=0;find_ok=1;
    cinema_active=active;cinema_type=context;cinema_pawn_class=pawn_class;
    cu_component_class=component_class;cu_user_class=user_class;cu_get_widget=get_widget;
    cu_set_main_pass=set_main_pass;
    cu_weak_assign=weak_assign;cu_weak_get=weak_get;cu_add=add;cu_remove=remove_widget;cu_in_viewport=viewport;
    cu_draw_size=draw_size;cu_viewport_scale=viewport_scale;cu_set_scale=set_scale;cu_set_pivot=set_pivot;
    cu_set_position=set_position;cu_set_size=set_size;cu_set_alignment=set_alignment;cu_set_anchors=set_anchors;
    cu_text_class=text_class;cu_tree_class=tree_class;cu_find_widget=find_widget;cu_render_opacity=render_opacity;
    cu_set_auto_wrap=set_auto_wrap;
    cu_overlay_slot_class=overlay_slot_class;cu_box_slot_class=box_slot_class;cu_overlay_class=overlay_class;cu_box_class=box_class;
    cu_set_overlay_halign=set_overlay_halign;cu_set_box_halign=set_box_halign;
}
static void declined(void) {
    unsigned char before[sizeof f];memcpy(before,&f,sizeof f);
    touch_cinema_ui_tick();assert(!added && !removed && !memcmp(before,&f,sizeof f));
}
int main(int argc,char** argv) {
    (void)argv;if(argc==999)install_cinema_ui(0);
    // Real p435 car50 geometry: a centered auto-wrapped desired width of127.067DIP
    // must not become the next frame's allotted width inside an828.408DIP canvas.
    ready();g_comp_w=1168;g_comp_h=536;dpi=.4958000183f;
    {const struct cinema_ui_vec2 unit={1,1};memcpy(f.widget+0x98,&unit,8);}
    touch_cinema_ui_tick();
    assert(halign_calls[0]==1 && halign_calls[1]==1 && !f.overlay_slot[0x50] && !f.box_slot[0x58]);
    assert(fabsf(*(float*)(f.widget+0x1f8)-828.4083252f)<.001f);
    type=1;touch_cinema_ui_tick();assert(f.overlay_slot[0x50]==2 && f.box_slot[0x58]==2);
    ready();unsigned char diagnostic_before[sizeof f];memcpy(diagnostic_before,&f,sizeof f);
    for(int i=0;i<20;i++)cinema_ui_font_probe(f.widget);
    assert(cu_font_logs==1 && font_color_logs==1 && !memcmp(diagnostic_before,&f,sizeof f));
    assert(font_paint_logs==1&&font_raw_logs==1&&logged_text==f.text&&logged_raw_text==f.text&&logged_slate==(void*)0x1234);
    assert(logged_outline==7&&logged_separate==1&&logged_apply==0&&logged_fallback==2);
    assert(logged_paint[0]==.25&&logged_paint[1]==.5&&logged_paint[2]==.75&&logged_paint[3]==.875);
    for(unsigned i=0;i<6;i++)assert(logged_paint[4+i]==*(const float*)(f.text+0x260+i*4));
    assert(!memcmp(logged_font,f.text+0x188,sizeof logged_font));
    ready();*(void**)(f.tree+0x20)=f.pawn;cinema_ui_font_probe(f.widget);assert(!cu_font_logs);
    ready();*(void**)(f.text+0x20)=f.widget;cinema_ui_font_probe(f.widget);assert(!cu_font_logs);
    ready();find_ok=0;cinema_ui_font_probe(f.widget);assert(!cu_font_logs);
    ready();*(void**)(f.text+0x10)=f.classes[4];cinema_ui_font_probe(f.widget);assert(!cu_font_logs);
    ready();*(int*)(f.props[3]+0x34)=2;cinema_ui_font_probe(f.widget);assert(!cu_font_logs);
    ready();*(int*)(f.props[3]+0x38)=4;cinema_ui_font_probe(f.widget);assert(!cu_font_logs);
    ready();*(void**)(f.props[3]+8)=f.other_kind;cinema_ui_font_probe(f.widget);assert(!cu_font_logs);
    ready();*(int*)(f.props[3]+0x4c)=0x220;cinema_ui_font_probe(f.widget);assert(!cu_font_logs);
    ready();thread_ok=0;cinema_ui_font_probe(f.widget);assert(!cu_font_logs);
    ready();phone_inspect_ready=0;cinema_ui_font_probe(f.widget);assert(!cu_font_logs);
    ready();for(int i=0;i<20;i++) {cu_font_last=0;cinema_ui_font_probe(f.widget);}assert(cu_font_logs==8 && font_color_logs==8&&font_paint_logs==8&&font_raw_logs==8);
    ready();unsigned char before[sizeof f];memcpy(before,&f,sizeof f);
    assert(cinema_subtitle_probe(f.pawn)==f.widget);assert(!cinema_subtitle_probe(f.theatre));
    for(int i=0;i<20;i++)touch_cinema_ui_tick();
    assert(auto_wrap_calls==1 && auto_wrap_enables==1 && (f.text[0x10d]&1));
    assert(halign_calls[0]==1 && halign_calls[1]==1 && !f.overlay_slot[0x50] && !f.box_slot[0x58]);
    assert(added==1 && in_view && cu_added && !removed && layout_calls==6 && cu_layout);unchanged_except_layout(before);
    assert(main_pass_calls==1 && main_pass_hides==1 && !main_pass_restores && cu_quad_changed);
    assert(!(*(unsigned*)(f.component+0x208)&0x20000) && (*(unsigned*)(f.component+0x208)&0x40000));
    assert(fabsf(*(float*)(f.widget+0x98)-4.0f/3.0f)<0.0001f && fabsf(*(float*)(f.widget+0x9c)-2.0f)<0.0001f);
    assert(*(float*)(f.widget+0xac)==0.5f && *(float*)(f.widget+0xb0)==1);
    assert(*(float*)(f.widget+0x1f0)==800 && fabsf(*(float*)(f.widget+0x1f4)-691.2f)<0.001f);
    assert(fabsf(*(float*)(f.widget+0x1f8)-1056)<0.001f && *(float*)(f.widget+0x1fc)==104);
    assert(*(float*)(f.widget+0x200)==0.5f && *(float*)(f.widget+0x204)==1);
    type=1;touch_cinema_ui_tick();assert(removed==1 && !in_view && !cu_added && cu_widget.serial==0);
    assert(auto_wrap_calls==2 && auto_wrap_restores==1 && !(f.text[0x10d]&1) && !cu_wrap_changed);
    assert(halign_calls[0]==2 && halign_calls[1]==2 && !cu_fill_changed);
    assert(!memcmp(before,&f,sizeof f) && !cu_layout && layout_calls==12);
    assert(main_pass_calls==2 && main_pass_restores==1 && !cu_quad_changed && cu_quad_component.index==-1);
    touch_cinema_ui_tick();assert(removed==1);
    // Original enabled wrapping and explicit wrap widths are never overwritten.
    ready();f.text[0x10d]|=1;*(float*)(f.text+0x110)=361.25f;memcpy(before,&f,sizeof f);
    touch_cinema_ui_tick();assert(!auto_wrap_calls && halign_calls[0]==1 && halign_calls[1]==1);
    type=1;touch_cinema_ui_tick();assert(!auto_wrap_calls && !memcmp(before,&f,sizeof f));
    ready();*(float*)(f.text+0x110)=361.25f;touch_cinema_ui_tick();assert(auto_wrap_enables==1 && *(float*)(f.text+0x110)==361.25f);
    type=1;touch_cinema_ui_tick();assert(auto_wrap_restores==1 && *(float*)(f.text+0x110)==361.25f);
    // Native changes during playback retain priority, with no per-frame forcing.
    ready();touch_cinema_ui_tick();f.text[0x10d]&=0xfe;for(int i=0;i<20;i++)touch_cinema_ui_tick();
    type=1;touch_cinema_ui_tick();assert(auto_wrap_calls==1 && !(f.text[0x10d]&1));
    // Fill each actual slot once, including when AutoWrap is already native; preserve
    // originally filled slots and any native alignment changes during playback.
    ready();f.overlay_slot[0x50]=f.box_slot[0x58]=0;memcpy(before,&f,sizeof f);
    touch_cinema_ui_tick();assert(!halign_calls[0] && !halign_calls[1]);
    type=1;touch_cinema_ui_tick();assert(!memcmp(before,&f,sizeof f));
    for(int original_fill=0;original_fill<2;original_fill++) {
        ready();if(original_fill)f.box_slot[0x58]=0;else f.overlay_slot[0x50]=0;
        memcpy(before,&f,sizeof f);touch_cinema_ui_tick();
        assert(halign_calls[original_fill]==0 && halign_calls[!original_fill]==1);
        assert(!f.overlay_slot[0x50] && !f.box_slot[0x58]);
        type=1;touch_cinema_ui_tick();assert(!memcmp(before,&f,sizeof f));
    }
    for(int changed=0;changed<2;changed++) {
        ready();touch_cinema_ui_tick();
        if(changed)f.box_slot[0x58]=3;else f.overlay_slot[0x50]=1;
        for(int i=0;i<20;i++)touch_cinema_ui_tick();
        type=1;touch_cinema_ui_tick();
        assert(f.overlay_slot[0x50]==(changed?2:1) && f.box_slot[0x58]==(changed?3:2));
        assert(halign_calls[changed]==1 && halign_calls[!changed]==2);
    }
    // Exact native RootWidget -> SizeBoxSlot -> Overlay -> OverlaySlot -> TextBlock.
    for(int guard=0;guard<16;guard++) {
        ready();
        if(guard==0)*(void**)(f.text+0x28)=0;
        if(guard==1)*(void**)(f.overlay_slot+0x10)=f.classes[4];
        if(guard==2)*(void**)(f.overlay_slot+0x30)=f.widget;
        if(guard==3)*(void**)(f.overlay_slot+0x28)=f.widget;
        if(guard==4)*(void**)(f.overlay_slot+0x20)=f.widget;
        if(guard==5)*(void**)(f.overlay+0x20)=f.widget;
        if(guard==6)*(void**)(f.overlay+0x28)=0;
        if(guard==7)*(void**)(f.box_slot+0x10)=f.classes[4];
        if(guard==8)*(void**)(f.box_slot+0x30)=f.text;
        if(guard==9)*(void**)(f.box_slot+0x28)=f.widget;
        if(guard==10)*(void**)(f.box_slot+0x20)=f.widget;
        if(guard==11)*(void**)(f.box+0x20)=f.widget;
        if(guard==12)*(void**)(f.tree+0x28)=f.overlay;
        if(guard==13)f.overlay_slot[0x50]=4;
        if(guard==14)f.box_slot[0x58]=4;
        if(guard==15)slot_alive[1]=0;
        touch_cinema_ui_tick();assert(auto_wrap_calls==1 && !halign_calls[0] && !halign_calls[1]);
        type=1;touch_cinema_ui_tick();assert(!halign_calls[0] && !halign_calls[1] && !cu_fill_changed);
    }
    // Weak identities are checked before dereferencing either expired/recycled slot.
    for(int guard=0;guard<7;guard++) {
        ready();touch_cinema_ui_tick();assert(halign_calls[0]==1 && halign_calls[1]==1);
        if(guard<4) {
            int s=guard/2;if(guard&1)slot_serial[s]++;else slot_alive[s]=0;
            *(void**)((s?f.box_slot:f.overlay_slot)+0x10)=(void*)1;
        }
        if(guard==4)*(void**)(f.text+0x28)=0;
        if(guard==5)*(void**)(f.overlay_slot+0x30)=f.widget;
        if(guard==6)*(void**)(f.tree+0x28)=f.overlay;
        type=1;touch_cinema_ui_tick();assert(halign_calls[0]==1 && halign_calls[1]==1 && !cu_fill_changed);
        assert(cu_fill_slots[0].index==-1 && cu_fill_slots[1].index==-1);
    }
    // Only the reflected SubtitleText child owned by this exact WidgetTree may change.
    for(int guard=0;guard<9;guard++) {
        ready();
        if(guard==0)*(void**)(f.tree+0x20)=f.pawn;
        if(guard==1)*(void**)(f.text+0x20)=f.widget;
        if(guard==2)find_ok=0;
        if(guard==3)*(void**)(f.text+0x10)=f.classes[4];
        if(guard==4)*(int*)(f.props[3]+0x38)=4;
        if(guard==5)*(int*)(f.props[3]+0x34)=2;
        if(guard==6)*(unsigned*)(f.props[3]+0x28)=WRONG;
        if(guard==7)*(int*)(f.props[3]+0x4c)=0x238;
        if(guard==8)*(void**)(f.props[3]+8)=f.other_kind;
        touch_cinema_ui_tick();assert(added==1 && !auto_wrap_calls && !(f.text[0x10d]&1));
    }
    // An expired, recycled, detached, rebound or foreign child is never restored through a stale pointer.
    for(int guard=0;guard<7;guard++) {
        ready();touch_cinema_ui_tick();assert(auto_wrap_calls==1);
        if(guard==0){text_alive=0;*(void**)(f.text+0x10)=(void*)1;}
        if(guard==1){text_serial++;*(void**)(f.text+0x10)=(void*)1;}
        if(guard==2)*(void**)(f.text+0x20)=f.widget;
        if(guard==3)*(void**)(f.tree+0x20)=f.pawn;
        if(guard==4)find_ok=0;
        if(guard==5)*(void**)(f.widget+0x230)=0;
        if(guard==6)*(void**)(f.text+0x10)=f.classes[4];
        type=1;touch_cinema_ui_tick();assert(auto_wrap_calls==1 && !cu_wrap_changed && cu_wrap_text.index==-1);
    }
    for(unsigned t=0;t<=7;t++)if(t!=3) {ready();touch_cinema_ui_tick();type=t;touch_cinema_ui_tick();assert(removed==1);}
    ready();in_view=1;touch_cinema_ui_tick();assert(!added && !cu_added && !layout_calls && !main_pass_calls && !auto_wrap_calls && !halign_calls[0] && !halign_calls[1]);type=1;touch_cinema_ui_tick();assert(!removed && in_view && !layout_calls && !main_pass_calls && !auto_wrap_calls && !halign_calls[0] && !halign_calls[1]);
    ready();memcpy(before,&f,sizeof f);touch_cinema_ui_tick();in_view=0;type=1;touch_cinema_ui_tick();assert(!removed && !memcmp(before,&f,sizeof f)); // Original already removed it.
    ready();touch_cinema_ui_tick();alive=0;*(void**)(f.widget+0x10)=(void*)1;type=1;touch_cinema_ui_tick();assert(!removed && layout_calls==6 && !cu_layout && main_pass_calls==1);
    ready();touch_cinema_ui_tick();serial++;*(void**)(f.widget+0x10)=(void*)1;type=1;touch_cinema_ui_tick();assert(!removed && layout_calls==6 && !cu_layout && main_pass_calls==1);
    ready();memcpy(before,&f,sizeof f);touch_cinema_ui_tick();thread_ok=0;type=1;touch_cinema_ui_tick();
    assert(main_pass_calls==1 && !removed && cu_quad_changed);thread_ok=1;touch_cinema_ui_tick();assert(main_pass_calls==2 && !memcmp(before,&f,sizeof f));
    // Native removal wins immediately, including world presentation and our temporary scale.
    ready();memcpy(before,&f,sizeof f);touch_cinema_ui_tick();in_view=0;touch_cinema_ui_tick();
    assert(added==1 && !removed && !cu_added && !cu_layout && main_pass_restores==1 && !memcmp(before,&f,sizeof f));
    touch_cinema_ui_tick();assert(added==1 && main_pass_calls==2);
    // Preserve an authored main-pass=false value; there is nothing for this helper to suppress/restore.
    ready();*(unsigned*)(f.component+0x208)&=~0x20000u;memcpy(before,&f,sizeof f);touch_cinema_ui_tick();
    assert(added==1 && !main_pass_calls && !cu_quad_changed);type=1;touch_cinema_ui_tick();assert(!memcmp(before,&f,sizeof f));
    // A native main-pass override made while flat is active is respected, rather than reapplied per tick.
    ready();touch_cinema_ui_tick();*(unsigned*)(f.component+0x208)|=0x20000u;touch_cinema_ui_tick();
    assert(main_pass_calls==1);type=1;touch_cinema_ui_tick();assert(main_pass_calls==1);
    // Other native primitive flags change independently and survive our single-bit restoration.
    ready();touch_cinema_ui_tick();*(unsigned*)(f.component+0x208)^=0x41;f.component[0x20c]^=0x12;f.component[0x20e]^=0x80;
    unsigned expected_flags=*(unsigned*)(f.component+0x208)|0x20000;unsigned char flags_c=f.component[0x20c],flags_e=f.component[0x20e];
    type=1;touch_cinema_ui_tick();assert(*(unsigned*)(f.component+0x208)==expected_flags && f.component[0x20c]==flags_c && f.component[0x20e]==flags_e);
    // Screen-space and custom-depth quads are outside the proven world-only/no-depth contract.
    for(int guard=0;guard<2;guard++) {
        ready();if(guard)f.component[0x20e]|=0x10;else f.component[0x429]=1;
        memcpy(before,&f,sizeof f);for(int i=0;i<20;i++)touch_cinema_ui_tick();
        assert(added==1 && !main_pass_calls && !cu_quad_changed);unchanged_except_layout(before);
        type=1;touch_cinema_ui_tick();assert(!memcmp(before,&f,sizeof f));
    }
    // Weak identity is checked before touching an expired/recycled component or its owner.
    for(int guard=0;guard<4;guard++) {
        ready();touch_cinema_ui_tick();
        if(guard==0){component_alive=0;*(void**)(f.component+0x10)=(void*)1;}
        if(guard==1){component_serial++;*(void**)(f.component+0x10)=(void*)1;}
        if(guard==2){theatre_alive=0;*(void**)(f.theatre+0x10)=(void*)1;}
        if(guard==3){theatre_serial++;*(void**)(f.theatre+0x10)=(void*)1;}
        type=1;touch_cinema_ui_tick();assert(main_pass_calls==1 && !cu_quad_changed && cu_quad_component.index==-1);
    }
    // A live component is still refused when native ownership/binding has changed.
    for(int guard=0;guard<8;guard++) {
        ready();touch_cinema_ui_tick();
        if(guard==0)*(void**)(f.component+0xa0)=f.pawn;
        if(guard==1)*(void**)(f.component+0x20)=f.pawn;
        if(guard==2)*(void**)(f.theatre+0x2b0)=0;
        if(guard==3)*(void**)(f.theatre+0x338)=0;
        if(guard==4)*(void**)(f.component+0x498)=0;
        if(guard==5)*(void**)(f.component+0x10)=f.classes[4];
        if(guard==6)*(int*)(f.props[1]+0x38)=4;
        if(guard==7)*(int*)(f.props[2]+0x4c)=0x330;
        type=1;touch_cinema_ui_tick();assert(main_pass_calls==1 && !cu_quad_changed);
    }
    ready();thread_ok=0;declined();assert(!weak_gets && !weak_assigns);
    ready();phone_inspect_ready=0;declined();assert(!weak_gets && !weak_assigns);
    ready();cu_ready=0;declined();assert(!weak_gets && !weak_assigns);
    ready();*(void**)(f.component+0xa0)=f.pawn;declined();
    ready();*(void**)(f.component+0x20)=f.pawn;declined();
    ready();*(void**)(f.component+0x498)=0;declined();
    ready();*(void**)(f.component+0x10)=f.classes[4];declined();
    ready();*(void**)(f.widget+0x10)=f.classes[4];declined();
    ready();*(unsigned*)(f.classes[1]+0x18)=WRONG;declined();
    ready();*(unsigned*)(f.classes[0]+0x18)=WRONG;declined();
    ready();*(int*)(f.props[0]+0x4c)=0x1190;declined();
    ready();*(int*)(f.props[1]+0x34)=2;declined();
    ready();*(int*)(f.props[2]+0x38)=4;declined();
    ready();*(void**)(f.props[2]+8)=f.other_kind;declined();
    ready();*(void**)(f.props[1]+0x58)=f.props[1];declined(); // Bounded cycle with no target.
    ready();*(unsigned short*)(f.component+0x14c)=0;declined();
    ready();*(unsigned short*)(f.component+0x14c)=0x420;declined();
    ready();memcpy(before,&f,sizeof f);touch_cinema_ui_tick();*(unsigned short*)(f.component+0x14c)=0;touch_cinema_ui_tick();assert(removed==1 && !in_view && !cu_layout);
    memcpy(f.component+0x14c,before+(f.component-(unsigned char*)&f)+0x14c,2);assert(!memcmp(before,&f,sizeof f));
    *(unsigned short*)(f.component+0x14c)=0x20;touch_cinema_ui_tick();assert(added==2 && in_view);
    *(unsigned short*)(f.component+0x14c)=0x420;touch_cinema_ui_tick();assert(removed==2 && !cu_layout && main_pass_restores==2);
    assert(*(unsigned short*)(f.component+0x14c)==0x420 && (*(unsigned*)(f.component+0x208)&0x20000));
    ready();memcpy(before,&f,sizeof f);add_success=0;for(int i=0;i<20;i++)touch_cinema_ui_tick();assert(added==1 && !cu_added && !in_view && !cu_layout && !main_pass_calls && !auto_wrap_calls && !memcmp(before,&f,sizeof f));
    ready();g_comp_w=1080;g_comp_h=2400;declined();assert(!layout_calls);
    ready();g_comp_w=0;declined();ready();dpi=NAN;declined();ready();dpi=0;declined();ready();dpi=5;declined();
    ready();draw.y=0;declined();ready();draw.x=NAN;declined();ready();draw.x=20000;declined();
    ready();*(float*)(f.widget+0x98)=NAN;declined();ready();*(float*)(f.widget+0x9c)=-1;declined();
    ready();*(float*)(f.widget+0xac)=NAN;declined();
    // Translation, shear and angle updates made by the original animation survive scale-only restoration.
    ready();touch_cinema_ui_tick();*(float*)(f.widget+0x90)=27;*(float*)(f.widget+0xa0)=0.1f;*(float*)(f.widget+0xa8)=19;
    type=1;touch_cinema_ui_tick();assert(*(float*)(f.widget+0x90)==27 && *(float*)(f.widget+0xa0)==0.1f && *(float*)(f.widget+0xa8)==19);
    for(int wi=0;wi<2;wi++)for(int di=0;di<3;di++) {
        ready();memcpy(before,&f,sizeof f);g_comp_w=wi ? 2167 : 2400;dpi=di==0 ? 0.75f : di==1 ? 1.5f : 2.0f;
        touch_cinema_ui_tick();assert(added==1);
        assert(fabsf(*(float*)(f.widget+0x1f0)*dpi-g_comp_w*0.5f)<0.001f);
        assert(fabsf(*(float*)(f.widget+0x1f4)*dpi-g_comp_h*0.96f)<0.001f);
        assert(fabsf(*(float*)(f.widget+0x1f8)*dpi*(*(float*)(f.widget+0x98))-g_comp_w*0.88f)<0.01f);
        type=1;touch_cinema_ui_tick();assert(!memcmp(before,&f,sizeof f));
    }
    // Original500x500 component,104-DIP caption root: stable visual bounds at every scale/DPI.
    const int extents[][2]={{2336,1080},{1984,912},{1752,808},{1560,720},{1168,536}};
    for(unsigned e=0;e<sizeof extents/sizeof *extents;e++)for(int di=0;di<5;di++) {
        ready();memcpy(before,&f,sizeof f);g_comp_w=extents[e][0];g_comp_h=extents[e][1];
        dpi=di==0?.75f:di==1?1:di==2?1.5f:di==3?2:.4958000183f;
        draw=(struct cinema_ui_vec2){500,500};
        const struct cinema_ui_vec2 unit={1,1};memcpy(f.widget+0x98,&unit,8);
        memcpy(before,&f,sizeof f);touch_cinema_ui_tick();assert(added==1);unchanged_except_layout(before);
        assert(auto_wrap_enables==1 && (f.text[0x10d]&1));
        assert(halign_calls[0]==1 && halign_calls[1]==1 && !f.overlay_slot[0x50] && !f.box_slot[0x58]);
        float sx=*(float*)(f.widget+0x98),sy=*(float*)(f.widget+0x9c);
        float x=*(float*)(f.widget+0x1f0)*dpi,y=*(float*)(f.widget+0x1f4)*dpi;
        float width=*(float*)(f.widget+0x1f8)*dpi*sx,height=*(float*)(f.widget+0x1fc)*dpi*sy;
        assert(fabsf(sx-sy)<0.00001f && fabsf(sy*dpi/g_comp_h-2.5f/1080)<0.000001f);
        assert(fabsf(width/g_comp_w-.88f)<0.000001f);
        assert(fabsf((x-width*.5f)/g_comp_w-.06f)<0.000001f);
        assert(fabsf((x+width*.5f)/g_comp_w-.94f)<0.000001f);
        assert(fabsf(y/g_comp_h-.96f)<0.000001f);
        assert(fabsf((y-height)/g_comp_h-(.96f-104*2.5f/1080))<0.000001f);
        assert(y-height>0 && y<g_comp_h);
        type=1;touch_cinema_ui_tick();assert(!memcmp(before,&f,sizeof f));
    }
    ready();for(int i=0;i<20;i++) {*(unsigned short*)(f.component+0x14c)=0x20;touch_cinema_ui_tick();*(unsigned short*)(f.component+0x14c)=0;touch_cinema_ui_tick();}
    assert(cu_logs==12 && added==20 && removed==20);
    assert(main_pass_calls==40 && main_pass_hides==20 && main_pass_restores==20);
    puts("PASS: actual authored subtitle helper; exact BP ownership, native widget text/opacity/options/timing preserved; original AutoWrap and both slot Fill APIs only after successful flat add; actual car50 regression, centered text preserved, 16 slot-binding refusals, 7 weak slot restore refusals, original enabled/explicit width/native override priority; owned world main-pass, no repeated writes, exact bit/layout/slot restoration, native removal/override, custom-depth/screen-space refusal, weak GC/recycling and changed owner guards; bottom-anchored104-DIP caption bounds/wrapping across25 extent/DPI combinations including live DPI0.4958");
}
