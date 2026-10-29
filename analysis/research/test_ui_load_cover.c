#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
typedef struct {float x,y,z,w;} qf_t;
struct reload_tf {qf_t q;float p[3],s[3],padding[2];};
struct cinema_ui_weak {int index,serial;};
struct ui_target {float x,y;void* component;};
struct fv_ui_object {void* actor,*component,*screen;};
struct fv_ui_click_state {unsigned char action,pad[3];int count;float press[3],release[3],press_time,release_time;void* hand;};
enum {COVER,OTHER,ROOT,BUTTON_ROOT,CONTINUE,LOAD,POOLED,EXTRA,PAWN,MANAGER,COUNT};
static unsigned char objects[COUNT][0x500],classes[4][0x100];
static int alive[COUNT],visible[COUNT],on_gt=1,context=7,clicks[COUNT];
static void* top,*continue_ref,*load_ref;
static long tick_ns=1000000000L;
static unsigned g_comp_w=2400,g_comp_h=1080;
static float g_fov_h_deg=67.380135f;
static struct ui_target g_ui_targets[16];
static int g_ui_target_count,g_ui_target_mx,locked;
static long g_ui_targets_ms;
static void* volatile g_ui_direct_click;
static struct {struct cinema_ui_weak screen,footer,scroll;} fv_footer;
static int index_of(const void* p){for(int i=0;i<COUNT;i++)if(p==objects[i])return i;return -1;}
static int pv_alive(const void* p){int i=index_of(p);return i>=0&&alive[i];}
static int cinema_on_game_thread(void){return on_gt;}
static void* scene_class(void){return classes[0];}
static void* button_class(void){return classes[1];}
static void* screen_class(void){return classes[2];}
static void* (*mv_scene_class)(void)=scene_class;
static void* (*fv_button_class)(void)=button_class;
static void* (*fv_screen_class)(void)=screen_class;
static int menu_visual_layout(const void* p,const void* cls,int minimum){
    if(!pv_alive(p))return 0;
    const void* actual=*(const void* const*)((const char*)p+0x10);
    return (actual==cls || (cls==classes[0]&&actual==classes[1]) || (cls==classes[2]&&actual==classes[3]))
        && *(const int*)((const char*)actual+0x58)>=minimum;
}
static int cinema_ui_named(const void* p,const char* name,int minimum){
    return pv_alive(p)&&*(const void* const*)((const char*)p+0x10)==classes[3]
        && !strcmp(name,"LoadGameCoverScreen_BP_C")&&*(int*)(classes[3]+0x58)>=minimum;
}
static void* fv_object_property(const void* p,const char* name){
    if(p!=objects[COVER])return 0;
    return !strcmp(name,"VR4UIButton_continue")?continue_ref:!strcmp(name,"VR4UIButton_loadSave")?load_ref:0;
}
// Native IsEnabled is an external seam: the original also checks component visibility/actor hidden state.
static int enabled(const void* p){int i=index_of(p);return pv_alive(p)&&visible[i]&&objects[i][0x264];}
static int (*fv_ui_enabled)(const void*)=enabled;
static void* active(void){return pv_alive(objects[PAWN])?objects[PAWN]:0;}
static void* manager(void){return objects[MANAGER];}
static void* peek(void* m){assert(m==objects[MANAGER]);return top;}
static unsigned type(void){return (unsigned)context;}
static void* (*phone_active)(void)=active,*(*fv_ui_manager)(void)=manager,*(*fv_peek_screen)(void*)=peek;
static unsigned (*phone_type)(void)=type;
static struct reload_tf head(const void* p){assert(p==objects[PAWN]);return (struct reload_tf){{0,0,0,1},{-14,0,15171.5f},{1,1,1},{0,0}};}
static struct reload_tf (*aim_head)(const void*)=head;
static qf_t pose_camera_delta_ue(void){return (qf_t){0,0,0,1};}
static qf_t q_mul(qf_t a,qf_t b){assert(b.w==1);return a;}
static int reload_tf_valid(const struct reload_tf* tf){return tf->q.w==1;}
#include Q_ROT_FRAGMENT
#include UI_PROJECT_FRAGMENT
static void* weak_get(const struct cinema_ui_weak* ref){(void)ref;return 0;}
static void* (*cu_weak_get)(const struct cinema_ui_weak*)=weak_get;
static void* fv_options_header_back(void* p){(void)p;return 0;}
static int fv_footer_control_visible(void* p,void* scroll){(void)p;(void)scroll;return 0;}
static int current_time(int clock,struct timespec* ts){(void)clock;ts->tv_sec=tick_ns/1000000000L;ts->tv_nsec=tick_ns%1000000000L;return 0;}
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif
#define clock_gettime current_time
static int pthread_mutex_lock(int* mx){assert(mx==&g_ui_target_mx&&!locked);locked=1;return 0;}
static int pthread_mutex_unlock(int* mx){assert(mx==&g_ui_target_mx&&locked);locked=0;return 0;}
static void obj_name(const void* p,char* out,int capacity){assert(pv_alive(p)&&capacity>10);memcpy(out,"Continue",9);}
#define LOGF(...) ((void)0)
#include FRAME_OWNED_FRAGMENT
#include FRAME_DESCENDS_FRAGMENT
#include UI_CLICK_FRAGMENT
#include LOAD_COVER_FRAGMENT
#include UI_TARGET_TICK_FRAGMENT
static void native_click(void* component,const void* object,const void* state){
    const struct fv_ui_object* actual=object;const struct fv_ui_click_state* click=state;
    int i=index_of(component);assert(i==CONTINUE||i==LOAD);
    assert(!locked&&actual->actor==objects[COVER]&&actual->component==component&&actual->screen==top);
    assert(click->action==0&&click->count==1&&!click->hand);clicks[i]++;
}
static void set_ptr(int i,unsigned offset,void* value){memcpy(objects[i]+offset,&value,8);}
static void setup(void){
    memset(objects,0,sizeof objects);memset(classes,0,sizeof classes);memset(clicks,0,sizeof clicks);
    for(int i=0;i<COUNT;i++){alive[i]=visible[i]=1;set_ptr(i,0x10,classes[0]);}
    *(int*)(classes[0]+0x58)=0x200;*(int*)(classes[1]+0x58)=0x330;
    *(int*)(classes[2]+0x58)=0x248;*(int*)(classes[3]+0x58)=0x400;
    set_ptr(COVER,0x10,classes[3]);set_ptr(OTHER,0x10,classes[2]);set_ptr(COVER,0x130,objects[ROOT]);
    set_ptr(ROOT,0xa0,objects[COVER]);set_ptr(BUTTON_ROOT,0xa0,objects[COVER]);set_ptr(BUTTON_ROOT,0xc0,objects[ROOT]);
    for(int i=CONTINUE;i<=EXTRA;i++){
        set_ptr(i,0x10,classes[1]);set_ptr(i,0x20,objects[COVER]);set_ptr(i,0xa0,objects[COVER]);
        set_ptr(i,0x308,objects[COVER]);set_ptr(i,0xc0,objects[BUTTON_ROOT]);objects[i][0x264]=1;
        struct reload_tf tf={{0,0,1,0},{203.9f,0,i==LOAD?15142.9f:15154.4f},{.8f,1,1},{0,0}};
        memcpy(objects[i]+0x1c0,&tf,sizeof tf);
    }
    set_ptr(POOLED,0x20,objects[OTHER]);set_ptr(POOLED,0xa0,objects[OTHER]);set_ptr(POOLED,0x308,objects[OTHER]);
    static void* entries[4];entries[0]=objects[POOLED];entries[1]=objects[CONTINUE];entries[2]=objects[LOAD];entries[3]=objects[EXTRA];
    struct {void** data;int count,max;} list={entries,4,4};memcpy(objects[COVER]+0x238,&list,sizeof list);
    top=objects[COVER];continue_ref=objects[CONTINUE];load_ref=objects[LOAD];on_gt=1;context=7;
    g_ui_direct_click=0;g_ui_target_count=0;g_ui_targets_ms=0;locked=0;fv_ui_click=native_click;
    fv_footer.screen=fv_footer.footer=fv_footer.scroll=(struct cinema_ui_weak){-1,0};
}
static void tick(void){tick_ns+=10000000L;touch_ui_targets_tick(tick_ns/1000000L);assert(!locked);}
static void tap(float x,float y){
    // Actual input release's nearest projected-target policy, followed by original GT callback dispatch.
    float best=.075f*1080;best*=best;void* picked=0;
    for(int i=0;i<g_ui_target_count;i++){float dx=g_ui_targets[i].x-x,dy=g_ui_targets[i].y-y;
        if(dx*dx+dy*dy<best){best=dx*dx+dy*dy;picked=g_ui_targets[i].component;}}
    g_ui_direct_click=picked;tick();
}
static void rejects_continue(void){tick();g_ui_direct_click=objects[CONTINUE];tick();assert(!clicks[CONTINUE]&&!g_ui_direct_click);}
int main(void){
    setup();tick();assert(g_ui_target_count==2);
    assert(g_ui_targets[0].component==objects[CONTINUE]&&g_ui_targets[1].component==objects[LOAD]);
    assert(fabsf(g_ui_targets[0].x-1200)<.1f&&fabsf(g_ui_targets[0].y-681)<1);
    assert(fabsf(g_ui_targets[1].x-1200)<.1f&&fabsf(g_ui_targets[1].y-776)<1);
    unsigned char original[sizeof objects];memcpy(original,objects,sizeof original);
    tap(1200,679);assert(clicks[CONTINUE]==1&&!clicks[LOAD]);tick();assert(clicks[CONTINUE]==1);
    tap(1200,776);assert(clicks[LOAD]==1);assert(!memcmp(original,objects,sizeof original));
    setup();on_gt=0;rejects_continue();
    for(int c=0;c<=8;c++){if(c==1||c==7)continue;setup();context=c;rejects_continue();}
    setup();set_ptr(COVER,0x10,classes[2]);rejects_continue();
    setup();continue_ref=0;rejects_continue();
    setup();continue_ref=objects[POOLED];rejects_continue();
    for(unsigned offset=0x20;offset<=0x308;offset=offset==0x20?0xa0:offset==0xa0?0x308:0x309){setup();set_ptr(CONTINUE,offset,objects[OTHER]);rejects_continue();}
    for(unsigned action=1;action<5;action++){setup();objects[CONTINUE][0x320]=(unsigned char)action;rejects_continue();}
    setup();objects[CONTINUE][0x264]=0;rejects_continue();setup();visible[CONTINUE]=0;rejects_continue();
    setup();alive[CONTINUE]=0;rejects_continue();setup();alive[PAWN]=0;rejects_continue();
    setup();set_ptr(CONTINUE,0x10,classes[0]);rejects_continue();
    setup();*(int*)(classes[1]+0x58)=0x320;rejects_continue();
    setup();set_ptr(ROOT,0xa0,objects[OTHER]);rejects_continue();
    setup();set_ptr(BUTTON_ROOT,0xc0,0);rejects_continue();
    setup();set_ptr(CONTINUE,0xc0,objects[CONTINUE]);rejects_continue();
    setup();tick();g_ui_direct_click=objects[CONTINUE];top=objects[OTHER];tick();assert(!clicks[CONTINUE]&&!g_ui_target_count);
    setup();tick();g_ui_direct_click=objects[CONTINUE];objects[CONTINUE][0x264]=0;tick();assert(!clicks[CONTINUE]);
    setup();g_ui_direct_click=objects[POOLED];tick();assert(!clicks[CONTINUE]&&!clicks[LOAD]);
    setup();g_ui_direct_click=objects[EXTRA];tick();assert(!clicks[CONTINUE]&&!clicks[LOAD]);
    puts("PASS: actual Load cover target/Click C path; captured p380 coordinates; both original actions; pooled, wrong type/class/ownership/action/visibility/ancestry, stale-screen and repeated-dispatch guards");
    return 0;
}
