#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
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
enum {PAWN=1,THEATRE,COMPONENT,USER,OBJECT_PROPERTY,OTHER_PROPERTY,THEATRE_FIELD,COMP_FIELD,WIDGET_FIELD,WRONG,TEXT_FIELD,TEXT,TREE};
static const char* names[]={"","VR4CutscenePlayerPawn_BP_C","CutsceneTheatreBox_C","WidgetComponent",
    "Subtitles_C","ObjectProperty","FloatProperty","TheatreBox","WidgetSubtitles","SubtitleWidget","Wrong","SubtitleText","TextBlock","WidgetTree"};
static void phone_inspect_fname(const void* name,char* out,int capacity) {
    unsigned id=name ? *(const unsigned*)name : 0;assert(id<sizeof names/sizeof names[0]);
    snprintf(out,(unsigned)capacity,"%s",names[id]);
}
static unsigned font_color_logs;
static void fake_log(const char* format,...) {if(strstr(format,"CINEMA FONT color"))font_color_logs++;}
#define LOGF(...) fake_log(__VA_ARGS__)
#include "../re4fix_cinema_ui.h"
static struct {
    unsigned char pawn[0x1290],theatre[0x528],component[0x550],widget[0x280],classes[7][0x100];
    unsigned char props[4][0x70],object_kind[16],other_kind[16];void* chains[7][1];
    unsigned char tree[0x30],text[0x2a0],font_object[0x30],font_material[0x30],outline_material[0x30];
} f __attribute__((aligned(16)));
static unsigned type,serial;static int alive,in_view,add_success;
static unsigned added,removed,weak_gets,weak_assigns;
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
static void* find_widget(const void* widget,const void* name) {assert(widget==f.widget && name==f.text+0x18);return find_ok ? f.text : 0;}
static float render_opacity(const void* widget) {assert(widget==f.widget || widget==f.text);return *(const float*)((const char*)widget+0xc4);}
static void* get_widget(const void* component) {assert(component==f.component);return *(void* const*)((const char*)component+0x498);}
static void weak_assign(struct cinema_ui_weak* weak,const void* widget) {
    weak_assigns++;assert(!widget || widget==f.widget);
    weak->index=widget ? 1 : -1;weak->serial=widget ? (int)serial : 0;
}
static void* weak_get(const struct cinema_ui_weak* weak) {
    weak_gets++;return alive && weak->index==1 && weak->serial==(int)serial ? f.widget : 0;
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
    assert(!memcmp(copy,before,sizeof f));
}
static void property(unsigned index,unsigned name,int offset) {
    unsigned char* p=f.props[index];*(void**)(p+8)=f.object_kind;*(unsigned*)(p+0x28)=name;
    *(int*)(p+0x34)=1;*(int*)(p+0x38)=8;*(int*)(p+0x4c)=offset;
}
static void ready(void) {
    memset(&f,0,sizeof f);
    int sizes[]={0x1290,0x528,0x550,0x280,0x280,0x2a0,0x30};
    unsigned ids[]={PAWN,THEATRE,COMPONENT,USER,WRONG,TEXT,TREE};
    for(unsigned i=0;i<7;i++) {
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
    memset(f.widget+0x40,0xa5,0x100); // Original text/timer/visibility payload is never edited.
    struct cinema_ui_vec2 scale={0.8f,1.2f},pivot={0.3f,0.7f},position={23,41},size={333,177},alignment={0.2f,0.8f};
    struct cinema_ui_anchors anchors={0.1f,0.2f,0.8f,0.9f};
    memcpy(f.widget+0x98,&scale,8);memcpy(f.widget+0xac,&pivot,8);
    memcpy(f.widget+0x1e0,&anchors,16);memcpy(f.widget+0x1f0,&position,8);
    memcpy(f.widget+0x1f8,&size,8);memcpy(f.widget+0x200,&alignment,8);
    *(void**)(f.widget+0x1a8)=f.tree;*(void**)(f.tree+0x10)=f.classes[6];*(void**)(f.tree+0x20)=f.widget;
    *(void**)(f.text+0x10)=f.classes[5];*(void**)(f.text+0x20)=f.tree;*(unsigned*)(f.text+0x18)=TEXT;
    property(3,TEXT_FIELD,0x230);*(void**)(f.classes[3]+0x70)=f.props[3];*(void**)(f.widget+0x230)=f.text;
    *(float*)(f.text+0xc4)=0.9f;*(float*)(f.widget+0xc4)=0.8f;
    *(void**)(f.text+0x188)=f.font_object;*(void**)(f.text+0x190)=f.font_material;*(void**)(f.text+0x1a0)=f.outline_material;
    *(int*)(f.text+0x1d0)=20;*(unsigned*)(f.text+0x1c8)=TEXT;
    void* objects[]={f.font_object,f.font_material,f.outline_material};
    for(unsigned i=0;i<3;i++) {*(void**)((char*)objects[i]+0x10)=f.classes[4];*(unsigned*)((char*)objects[i]+0x18)=WRONG;}
    float color[]={0.6f,0.7f,0.8f,1.0f};memcpy(f.text+0x150,color,16);f.text[0x160]=0;
    phone_inspect_ready=thread_ok=cu_ready=alive=add_success=1;type=3;serial=7;in_view=0;
    cu_widget=(struct cinema_ui_weak){-1,0};cu_added=cu_attempted=cu_layout=0;cu_logs=cu_rejections=0;
    added=removed=weak_gets=weak_assigns=0;
    layout_calls=0;g_comp_w=2400;g_comp_h=1080;dpi=1.5f;draw=(struct cinema_ui_vec2){1024,128};
    cu_font_last=0;cu_font_name=0;cu_font_logs=font_color_logs=0;find_ok=1;
    cinema_active=active;cinema_type=context;cinema_pawn_class=pawn_class;
    cu_component_class=component_class;cu_user_class=user_class;cu_get_widget=get_widget;
    cu_weak_assign=weak_assign;cu_weak_get=weak_get;cu_add=add;cu_remove=remove_widget;cu_in_viewport=viewport;
    cu_draw_size=draw_size;cu_viewport_scale=viewport_scale;cu_set_scale=set_scale;cu_set_pivot=set_pivot;
    cu_set_position=set_position;cu_set_size=set_size;cu_set_alignment=set_alignment;cu_set_anchors=set_anchors;
    cu_text_class=text_class;cu_tree_class=tree_class;cu_find_widget=find_widget;cu_render_opacity=render_opacity;
}
static void declined(void) {
    unsigned char before[sizeof f];memcpy(before,&f,sizeof f);
    touch_cinema_ui_tick();assert(!added && !removed && !memcmp(before,&f,sizeof f));
}
int main(int argc,char** argv) {
    (void)argv;if(argc==999)install_cinema_ui(0);
    ready();unsigned char diagnostic_before[sizeof f];memcpy(diagnostic_before,&f,sizeof f);
    for(int i=0;i<20;i++)cinema_ui_font_probe(f.widget);
    assert(cu_font_logs==1 && font_color_logs==1 && !memcmp(diagnostic_before,&f,sizeof f));
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
    ready();for(int i=0;i<20;i++) {cu_font_last=0;cinema_ui_font_probe(f.widget);}assert(cu_font_logs==8 && font_color_logs==8);
    ready();unsigned char before[sizeof f];memcpy(before,&f,sizeof f);
    assert(cinema_subtitle_probe(f.pawn)==f.widget);assert(!cinema_subtitle_probe(f.theatre));
    for(int i=0;i<20;i++)touch_cinema_ui_tick();
    assert(added==1 && in_view && cu_added && !removed && layout_calls==6 && cu_layout);unchanged_except_layout(before);
    assert(*(float*)(f.widget+0x98)==2.0f && *(float*)(f.widget+0x9c)==3.0f);
    assert(*(float*)(f.widget+0xac)==0.5f && *(float*)(f.widget+0xb0)==0);
    assert(*(float*)(f.widget+0x1f0)==800 && *(float*)(f.widget+0x1f4)==576);   // top at 80%: room for 3 lines
    assert(fabsf(*(float*)(f.widget+0x1f8)-704)<0.001f && *(float*)(f.widget+0x1fc)==128);
    type=1;touch_cinema_ui_tick();assert(removed==1 && !in_view && !cu_added && cu_widget.serial==0);
    assert(!memcmp(before,&f,sizeof f) && !cu_layout && layout_calls==12);
    touch_cinema_ui_tick();assert(removed==1);
    for(unsigned t=0;t<=7;t++)if(t!=3) {ready();touch_cinema_ui_tick();type=t;touch_cinema_ui_tick();assert(removed==1);}
    ready();in_view=1;touch_cinema_ui_tick();assert(!added && !cu_added && !layout_calls);type=1;touch_cinema_ui_tick();assert(!removed && in_view && !layout_calls);
    ready();memcpy(before,&f,sizeof f);touch_cinema_ui_tick();in_view=0;type=1;touch_cinema_ui_tick();assert(!removed && !memcmp(before,&f,sizeof f)); // Original already removed it.
    ready();touch_cinema_ui_tick();alive=0;*(void**)(f.widget+0x10)=(void*)1;type=1;touch_cinema_ui_tick();assert(!removed && layout_calls==6 && !cu_layout);
    ready();touch_cinema_ui_tick();serial++;*(void**)(f.widget+0x10)=(void*)1;type=1;touch_cinema_ui_tick();assert(!removed && layout_calls==6 && !cu_layout);
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
    ready();memcpy(before,&f,sizeof f);add_success=0;for(int i=0;i<20;i++)touch_cinema_ui_tick();assert(added==1 && !cu_added && !in_view && !cu_layout && !memcmp(before,&f,sizeof f));
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
        assert(fabsf(*(float*)(f.widget+0x1f4)*dpi-g_comp_h*0.80f)<0.001f);
        assert(fabsf(*(float*)(f.widget+0x1f8)*dpi*(*(float*)(f.widget+0x98))-g_comp_w*0.88f)<0.01f);
        type=1;touch_cinema_ui_tick();assert(!memcmp(before,&f,sizeof f));
    }
    ready();for(int i=0;i<20;i++) {*(unsigned short*)(f.component+0x14c)=0x20;touch_cinema_ui_tick();*(unsigned short*)(f.component+0x14c)=0;touch_cinema_ui_tick();}
    assert(cu_logs==12 && added==20 && removed==20);
    puts("PASS: actual authored subtitle helper; exact BP ownership, DPI-aware bottom placement, 2.5x scale, native canvas and width; only owned layout changed/restored; text/timers and original translation/shear/angle preserved; option-hide, add failure, weak GC/recycle, pre-existing viewport and foreign GT guards");
}
