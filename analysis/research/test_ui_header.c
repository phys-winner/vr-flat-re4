#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
typedef struct {float x,y,z,w;} qf_t;
struct reload_tf {qf_t q;float p[3],s[3],pad[2];};
struct cinema_ui_weak {int index,serial;};
struct ui_target {float x,y;void* component;};
struct fv_ui_object {void* actor;void* component;void* screen;};
struct fv_ui_click_state {unsigned char action,pad[3];int count;float press[3],release[3],press_time,release_time;void* hand;};
enum {OBJECTS=25,CLASSES=8};
static unsigned char objects[OBJECTS][0x1300],classes[CLASSES][0x100];
static void* chains[CLASSES][2];
static int alive[OBJECTS],serials[OBJECTS],on_gt=1,type=2,property_mask=7,clicked;
static int resumes,cancels,perform_native_back,last_pause_subscreen,last_pause_screen;
static long tick_ns=1000000000L;
static void* current_menu,*active_top;
static const char* object_names[OBJECTS];
static struct ui_target g_ui_targets[16];
static int g_ui_target_count,g_ui_target_mx,locked;
static long g_ui_targets_ms;
static void* volatile g_ui_direct_click;
static unsigned g_comp_w=2400,g_comp_h=1080;
static float g_fov_h_deg=90;
static volatile int g_touch_back_request;
static struct {struct cinema_ui_weak screen,footer,scroll;} fv_footer;
static int index_of(const void* p){for(int i=0;i<OBJECTS;i++)if(p==objects[i])return i;return -1;}
static int pv_alive(const void* p){int i=index_of(p);return i>=0&&alive[i];}
static int cinema_on_game_thread(void){return on_gt;}
static void* scene_class(void){return classes[0];}
static void* button_class(void){return classes[1];}
static void* screen_class(void){return classes[2];}
static void* child_class(void){return classes[5];}
static void* (*mv_scene_class)(void)=scene_class;
static void* (*fv_button_class)(void)=button_class;
static void* (*fv_screen_class)(void)=screen_class;
static void* (*pv_child_class)(void)=child_class;
static int menu_visual_layout(const void* p,const void* cls,int minimum){
    if(!pv_alive(p))return 0;
    const char* actual=*(const char* const*)((const char*)p+0x10);
    return (actual==cls || (cls==classes[0]&&(actual==(const char*)classes[1]||actual==(const char*)classes[5]))
            ||(cls==classes[2]&&actual==(const char*)classes[7]))
        && *(const int*)(actual+0x58)>=minimum;
}
static int cinema_ui_named(const void* p,const char* name,int minimum){
    int i=index_of(p);return pv_alive(p)&&object_names[i]&&!strcmp(object_names[i],name)
        && *(const int*)(*(const char* const*)((const char*)p+0x10)+0x58)>=minimum;
}
static void* fv_object_property(const void* p,const char* name){
    if(p==objects[0]&&!strcmp(name,"CurrentMenu"))return property_mask&1?current_menu:0;
    if((p==objects[1]||p==objects[5])&&!strcmp(name,"OptionsScreen"))return property_mask&2?*(void* const*)((const char*)p+0x300):0;
    if((p==objects[1]||p==objects[5])&&!strcmp(name,"BackButton"))return property_mask&4?*(void* const*)((const char*)p+0x240):0;
    if(p==objects[10]&&!strcmp(name,"Ref_ContentActor"))return *(void* const*)((const char*)p+0x398);
    if(p==objects[10]&&!strcmp(name,"Ref_Header_InGame"))return *(void* const*)((const char*)p+0x3b8);
    if(p==objects[10]&&!strcmp(name,"Content_Actor"))return *(void* const*)((const char*)p+0x358);
    if(p==objects[22]&&!strcmp(name,"CentralCoverScreen"))return *(void* const*)((const char*)p+0x3a8);
    if(p==objects[15]&&!strcmp(name,"Ref_Parent_PauseMenu"))return *(void* const*)((const char*)p+0x240);
    if(p==objects[15]&&!strcmp(name,"Button_Inventory"))return *(void* const*)((const char*)p+0x290);
    if(p==objects[17]&&!strcmp(name,"Ref_Parent_Header"))return *(void* const*)((const char*)p+0x2f8);
    if(p==objects[17]&&!strcmp(name,"Button"))return *(void* const*)((const char*)p+0x238);
    return 0;
}
static int enabled(const void* p){return pv_alive(p)&&((const unsigned char*)p)[0x264];}
static int (*fv_ui_enabled)(const void*)=enabled;
static void* active(void){return alive[9]?objects[9]:0;}
static void* manager(void){return objects[8];}
static void* peek(void* m){assert(m==objects[8]);return pv_alive(active_top)?active_top:0;}
static unsigned context(void){return (unsigned)type;}
static void* (*phone_active)(void)=active;
static unsigned (*phone_type)(void)=context;
static void* (*fv_ui_manager)(void)=manager;
static void* (*fv_peek_screen)(void*)=peek;
static struct reload_tf head(void* p){assert(p==objects[9]);return (struct reload_tf){{0,0,0,1},{-6,0,15171.5f},{1,1,1},{0,0}};}
static struct reload_tf (*aim_head)(void*)=head;
static qf_t pose_camera_delta_ue(void){return (qf_t){0,0,0,1};}
static qf_t q_mul(qf_t a,qf_t b){assert(b.w==1);return a;}
static int reload_tf_valid(const struct reload_tf* tf){return tf->q.w==1;}
#ifdef UI_PROJECT_FRAGMENT
#include Q_ROT_FRAGMENT
#include UI_PROJECT_FRAGMENT
#endif
static int fv_footer_control_visible(void* part,void* scroll){return part==objects[11]&&scroll==objects[12]&&enabled(part);}
static void* weak_get(const struct cinema_ui_weak* weak){return weak->index>=0&&weak->index<OBJECTS&&alive[weak->index]&&serials[weak->index]==weak->serial?objects[weak->index]:0;}
static void* (*cu_weak_get)(const struct cinema_ui_weak*)=weak_get;
static int current_clock_gettime(int clock,struct timespec* ts){(void)clock;ts->tv_sec=tick_ns/1000000000L;ts->tv_nsec=tick_ns%1000000000L;return 0;}
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif
#define clock_gettime current_clock_gettime
static int pthread_mutex_lock(int* m){assert(m==&g_ui_target_mx&&!locked);locked=1;return 0;}
static int pthread_mutex_unlock(int* m){assert(m==&g_ui_target_mx&&locked);locked=0;return 0;}
static void obj_name(const void* p,char* out,int capacity){assert(pv_alive(p)&&capacity>10);memcpy(out,"BackButton",11);}
#define LOGF(...) ((void)0)
static void* player(const void* hand){return hand==objects[13]?objects[9]:0;}
static void* (*phone_player)(const void*)=player;
static void* pause_pawn_class(void){return classes[4];}
static void touch_menu_grip_cancel(void* hand){assert(hand==objects[13]&&!locked);cancels++;}
static int try_resume(unsigned char reason){assert(reason==0&&cancels==resumes+clicked+1);resumes++;return 1;}
#ifdef UNPAUSE_BACK_FRAGMENT
#include UNPAUSE_BACK_FRAGMENT
#endif
#ifdef FRAME_OWNED_FRAGMENT
#include FRAME_OWNED_FRAGMENT
#include FRAME_DESCENDS_FRAGMENT
#include UI_CLICK_FRAGMENT
#include UI_TARGET_FRAGMENT
#else
static void touch_ui_targets_tick(long now){(void)now;}
#endif
static void native_click(void* component,const void* object,const void* state){
    const struct fv_ui_object* actual=object;const struct fv_ui_click_state* click=state;
    assert(!locked&&actual->component==component&&actual->actor==*(void* const*)((const char*)component+0x20));
    assert(actual->screen==((component==objects[11]||component==objects[21])?objects[10]:objects[0]));
    assert(click->action==0&&click->count==1&&!click->hand);clicked++;
    if(perform_native_back){
        last_pause_subscreen=0;
        if(component==objects[21]){last_pause_screen=2;active_top=objects[23];}
        else active_top=objects[14];
    }
}
static void set_ptr(int index,unsigned offset,void* p){memcpy(objects[index]+offset,&p,sizeof p);}
static void setup(void){
    memset(objects,0,sizeof objects);memset(classes,0,sizeof classes);
    for(unsigned i=0;i<OBJECTS;i++){alive[i]=1;serials[i]=1;set_ptr(i,0x10,classes[0]);object_names[i]=0;}
    for(unsigned i=0;i<CLASSES;i++){chains[i][0]=classes[i]+0x30;*(void***)(classes[i]+0x30)=chains[i];}
    *(int*)(classes[0]+0x58)=0x200;*(int*)(classes[1]+0x58)=0x330;
    *(int*)(classes[2]+0x58)=0x488;*(int*)(classes[3]+0x58)=0x341;
    *(int*)(classes[4]+0x58)=0x1300;set_ptr(9,0x10,classes[4]);set_ptr(9,0x11b0,objects[10]);
    *(int*)(classes[5]+0x58)=0x220;*(int*)(classes[6]+0x58)=0x500;*(int*)(classes[7]+0x58)=0x1300;
    chains[7][0]=classes[2]+0x30;chains[7][1]=classes[7]+0x30;*(int*)(classes[7]+0x38)=1;
    set_ptr(10,0x10,classes[7]);object_names[10]="UI_PauseMenu_BP_C";objects[13][0x314]=1;
    set_ptr(14,0x10,classes[2]);object_names[14]="OptionsCoverScreen_BP_C";
    set_ptr(15,0x10,classes[6]);object_names[15]="UI_PauseMenu_Header_BP_C";
    set_ptr(16,0x10,classes[5]);set_ptr(17,0x10,classes[6]);object_names[17]="UI_PauseMenu_Header_Button_C";
    set_ptr(22,0x10,classes[6]);object_names[22]="UI_PauseMenu_Content_GameOptions_BP_C";
    set_ptr(23,0x10,classes[2]);object_names[23]="InventoryUIScreen_BP_C";
    set_ptr(10,0x398,objects[22]);set_ptr(10,0x3b8,objects[15]);set_ptr(10,0x130,objects[18]);set_ptr(18,0xa0,objects[10]);
    set_ptr(22,0xe0,0);set_ptr(22,0x3a8,objects[14]);set_ptr(14,0xe0,objects[22]);
    set_ptr(10,0x358,objects[24]);set_ptr(24,0x10,classes[5]);set_ptr(24,0x20,objects[10]);set_ptr(24,0xa0,objects[10]);set_ptr(24,0x1f8,objects[22]);
    set_ptr(15,0x240,objects[10]);set_ptr(15,0x290,objects[16]);set_ptr(15,0x130,objects[19]);set_ptr(19,0xa0,objects[15]);set_ptr(19,0xc0,objects[18]);
    set_ptr(16,0x20,objects[15]);set_ptr(16,0xa0,objects[15]);set_ptr(16,0xc0,objects[19]);set_ptr(16,0x1f8,objects[17]);
    set_ptr(17,0x2f8,objects[15]);set_ptr(17,0x238,objects[21]);set_ptr(17,0x130,objects[20]);set_ptr(20,0xa0,objects[17]);set_ptr(20,0xc0,objects[16]);
    set_ptr(21,0x10,classes[1]);set_ptr(21,0x20,objects[17]);set_ptr(21,0xa0,objects[17]);set_ptr(21,0x308,objects[10]);set_ptr(21,0xc0,objects[20]);objects[21][0x264]=1;
    set_ptr(0,0x10,classes[2]);object_names[0]="OptionsUIScreen_BP_C";
    set_ptr(1,0x10,classes[3]);set_ptr(5,0x10,classes[3]);object_names[1]=object_names[5]="OptionsScreen_Menu_C";
    set_ptr(2,0x10,classes[1]);set_ptr(6,0x10,classes[1]);set_ptr(11,0x10,classes[1]);
    set_ptr(0,0x130,objects[4]);set_ptr(4,0xa0,objects[0]);
    set_ptr(1,0x130,objects[3]);set_ptr(3,0xa0,objects[1]);set_ptr(3,0xc0,objects[4]);
    set_ptr(5,0x130,objects[7]);set_ptr(7,0xa0,objects[5]);set_ptr(7,0xc0,objects[4]);
    set_ptr(1,0x240,objects[2]);set_ptr(5,0x240,objects[6]);
    set_ptr(1,0x300,objects[0]);set_ptr(5,0x300,objects[0]);
    for(unsigned i=0;i<2;i++){
        int b=i?6:2,m=i?5:1,r=i?7:3;
        set_ptr(b,0xa0,objects[m]);set_ptr(b,0x20,objects[m]);set_ptr(b,0xc0,objects[r]);set_ptr(b,0x308,objects[0]);
        objects[b][0x264]=1;float point[3]={93.6f,-62.7f,15202.2f};memcpy(objects[b]+0x1d0,point,12);
    }
    static void* entries[2];entries[0]=objects[6];entries[1]=objects[2];
    struct {void** data;int count,max;} list={entries,2,2};memcpy(objects[0]+0x238,&list,sizeof list);
    fv_footer.screen=fv_footer.footer=fv_footer.scroll=(struct cinema_ui_weak){-1,0};
    current_menu=objects[1];type=2;property_mask=7;on_gt=1;clicked=0;
    active_top=objects[0];perform_native_back=0;last_pause_subscreen=8;last_pause_screen=9;resumes=cancels=0;
    g_touch_back_request=0;unpause_try=try_resume;unpause_pawn_class=pause_pawn_class;unpause_screen_class=screen_class;
    pv_child_class=child_class;
    g_ui_direct_click=0;g_ui_target_count=0;g_ui_targets_ms=0;memset(g_ui_targets,0,sizeof g_ui_targets);
#ifdef UI_CLICK_FRAGMENT
    fv_ui_click=native_click;
    unpause_ui_back=fv_options_back_action;
#endif
}
static void tick(void){tick_ns+=10000000L;touch_ui_targets_tick(tick_ns/1000000L);assert(!locked);}
static void accepts_header(void){
    tick();assert(g_ui_target_count==1&&g_ui_targets[0].component==objects[2]);
    assert(fabsf(g_ui_targets[0].x-445)<1&&fabsf(g_ui_targets[0].y-170)<1);
    unsigned char before[sizeof objects];memcpy(before,objects,sizeof objects);
    g_ui_direct_click=objects[2];tick();assert(clicked==1&&!g_ui_direct_click);
    tick();assert(clicked==1&&!memcmp(before,objects,sizeof objects));
}
static void rejects_header(void){g_ui_direct_click=objects[2];tick();assert(!g_ui_target_count&&!clicked&&!g_ui_direct_click);}
static void rejects_cover(void){
    active_top=objects[14];perform_native_back=1;g_touch_back_request=1;touch_unpause_tick(objects[13]);
    assert(!clicked&&!resumes&&cancels==1&&!g_touch_back_request&&last_pause_screen==9&&active_top==objects[14]);
}
int main(void){
    setup();accepts_header();
    for(unsigned k=0;k<3;k++){setup();property_mask&=~(1<<k);rejects_header();}
    setup();set_ptr(1,0x300,objects[10]);rejects_header();
    setup();object_names[0]="InventoryUIScreen_BP_C";rejects_header();
    setup();object_names[1]="Other_Menu_C";rejects_header();
    setup();set_ptr(2,0x308,objects[10]);rejects_header();
    setup();set_ptr(2,0x310,objects[12]);rejects_header();
    for(int action=1;action<5;action++){setup();objects[2][0x320]=(unsigned char)action;rejects_header();}
    setup();set_ptr(2,0xa0,objects[5]);rejects_header();
    setup();set_ptr(2,0x20,objects[5]);rejects_header();
    setup();set_ptr(2,0xc0,objects[7]);rejects_header();
    setup();set_ptr(3,0xc0,0);rejects_header();
    setup();set_ptr(3,0xa0,objects[5]);rejects_header();
    setup();set_ptr(4,0xa0,objects[5]);rejects_header();
    setup();objects[2][0x264]=0;rejects_header();
    setup();on_gt=0;rejects_header();
    for(unsigned k=0;k<5;k++){setup();alive[k]=0;rejects_header();}
    setup();alive[9]=0;rejects_header();
    setup();set_ptr(2,0x10,classes[0]);rejects_header();
    setup();*(int*)(classes[1]+0x58)=0x310;rejects_header();
    for(int c=0;c<=8;c++){if(c==1||c==2||c==4||c==7)continue;setup();type=c;rejects_header();}
    setup();type=4;accepts_header();setup();type=7;accepts_header();
    setup();tick();g_ui_direct_click=objects[6];tick();assert(!clicked&&g_ui_target_count==1&&g_ui_targets[0].component==objects[2]);
    setup();tick();g_ui_direct_click=objects[2];current_menu=objects[5];tick();assert(!clicked&&g_ui_target_count==1&&g_ui_targets[0].component==objects[6]);
    setup();tick();g_ui_direct_click=objects[2];alive[2]=0;tick();assert(!clicked&&!g_ui_target_count);
    setup();memcpy(objects[2]+0x1d0,(float[]){93.6f,-200,15202.2f},12);rejects_header();
    setup();memcpy(objects[2]+0x1d0,(float[]){595,0,15171.5f},12);rejects_header();
    setup();type=1;tick();assert(g_ui_target_count==2);g_ui_direct_click=objects[2];tick();assert(clicked==1);
    setup();fv_footer.screen=(struct cinema_ui_weak){0,1};fv_footer.footer=(struct cinema_ui_weak){10,1};fv_footer.scroll=(struct cinema_ui_weak){12,1};
    static void* footer_entries[1];footer_entries[0]=objects[11];
    struct {void** data;int count,max;} footer_list={footer_entries,1,1};memcpy(objects[10]+0x238,&footer_list,sizeof footer_list);
    set_ptr(11,0x308,objects[10]);set_ptr(11,0x20,objects[10]);objects[11][0x264]=1;
    memcpy(objects[11]+0x1d0,(float[]){93.6f,0,15145.8f},12);tick();assert(g_ui_target_count==2);
    g_ui_direct_click=objects[11];tick();assert(clicked==1);g_ui_direct_click=objects[2];tick();assert(clicked==2);
    setup();perform_native_back=1;g_touch_back_request=1;touch_unpause_tick(objects[13]);
    assert(clicked==1&&!resumes&&cancels==1&&!g_touch_back_request&&last_pause_subscreen==0&&active_top==objects[14]);
    touch_unpause_tick(objects[13]);assert(clicked==1&&!resumes&&cancels==1);
    g_touch_back_request=1;touch_unpause_tick(objects[13]);assert(clicked==2&&!resumes&&cancels==2&&!g_touch_back_request&&active_top==objects[23]&&last_pause_screen==2);
    g_touch_back_request=1;touch_unpause_tick(objects[13]);assert(clicked==2&&resumes==1&&cancels==3&&!g_touch_back_request);
    setup();objects[2][0x264]=0;g_touch_back_request=1;touch_unpause_tick(objects[13]);assert(!clicked&&!resumes&&cancels==1&&!g_touch_back_request);
    setup();property_mask=0;g_touch_back_request=1;touch_unpause_tick(objects[13]);assert(!clicked&&!resumes&&cancels==1);
    setup();fv_ui_click=0;g_touch_back_request=1;touch_unpause_tick(objects[13]);assert(!clicked&&!resumes&&cancels==1);
    setup();active_top=objects[10];g_touch_back_request=1;touch_unpause_tick(objects[13]);assert(!clicked&&resumes==1&&cancels==1);
    setup();g_touch_back_request=1;touch_unpause_tick(objects[8]);assert(!clicked&&!resumes&&!cancels&&g_touch_back_request);
    setup();on_gt=0;assert(!fv_options_back_action(objects[10])&&!clicked&&!resumes);
    setup();pv_child_class=0;rejects_cover();
    setup();object_names[10]="ForeignPause_C";rejects_cover();
    setup();object_names[15]="ForeignHeader_C";rejects_cover();
    setup();object_names[17]="OtherHeaderButton_C";rejects_cover();
    setup();object_names[22]="UI_PauseMenu_Content_NewGame_BP_C";rejects_cover();
    const unsigned fields[][2]={{10,0x398},{10,0x3b8},{10,0x358},{24,0x20},{24,0xa0},{24,0x1f8},{22,0x3a8},{14,0xe0},{15,0x240},{15,0x290},
        {16,0x20},{16,0xa0},{16,0x1f8},{17,0x2f8},{17,0x238},{21,0x20},{21,0xa0},{21,0x308},
        {18,0xa0},{19,0xa0},{19,0xc0},{21,0xc0}};
    for(unsigned i=0;i<sizeof fields/sizeof *fields;i++){setup();set_ptr((int)fields[i][0],fields[i][1],0);rejects_cover();}
    for(int i=15;i<=22;i++){setup();alive[i]=0;rejects_cover();}
    setup();set_ptr(21,0x310,objects[12]);rejects_cover();
    setup();objects[21][0x264]=0;rejects_cover();
    setup();objects[21][0x320]=2;rejects_cover();
    setup();set_ptr(21,0x10,classes[0]);rejects_cover();
    setup();set_ptr(16,0x10,classes[0]);rejects_cover();
    setup();set_ptr(24,0x10,classes[0]);rejects_cover();
    setup();alive[24]=0;rejects_cover();
    setup();set_ptr(19,0xc0,objects[19]);rejects_cover();
    puts("PASS: current Options header Back publishes exact native Click in Pause2/4/7; pooled/foreign/disabled/replaced/dead/context/geometry rejected; popup/footer delegates and all native bytes preserved");
    puts("PASS: actual production wt_project/component location matches retained Back image; Options subsection -> Options cover -> original Inventory tab -> ordinary resume; each Back performs one native action, updates remembered tab only through original delegate and refuses foreign/dead/disabled/malformed cover bindings");
    return 0;
}
