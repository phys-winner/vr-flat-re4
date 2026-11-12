#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include UI_CURVE_CAPTURE
#include UI_MERC_CAPTURE
typedef struct{float x,y,z,w;}qf_t;
struct __attribute__((aligned(16)))reload_tf{qf_t q;float p[3],s[3],padding[2];};
static qf_t q_mul(qf_t a,qf_t b){return(qf_t){a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
static void q_rot(qf_t q,const float v[3],float o[3]){
    float d=q.x*v[0]+q.y*v[1]+q.z*v[2],k=q.w*q.w-q.x*q.x-q.y*q.y-q.z*q.z;
    o[0]=2*d*q.x+k*v[0]+2*q.w*(q.y*v[2]-q.z*v[1]);o[1]=2*d*q.y+k*v[1]+2*q.w*(q.z*v[0]-q.x*v[2]);o[2]=2*d*q.z+k*v[2]+2*q.w*(q.x*v[1]-q.y*v[0]);
}
static int reload_marker_valid(const struct reload_tf* tf){
    float norm=tf->q.x*tf->q.x+tf->q.y*tf->q.y+tf->q.z*tf->q.z+tf->q.w*tf->q.w;
    if(!isfinite(norm)||fabsf(norm-1)>.01f)return 0;
    for(unsigned i=0;i<3;i++)if(!isfinite(tf->p[i])||!isfinite(tf->s[i])||tf->s[i]<=0)return 0;return 1;
}
static int reload_tf_valid(const struct reload_tf* tf){return reload_marker_valid(tf)&&tf->s[0]==1&&tf->s[1]==1&&tf->s[2]==1;}
#include "../re4fix_ui_frame.h"
struct cinema_ui_weak{int index,serial;};
enum{OBJECT_COUNT=350};
static unsigned char objects[OBJECT_COUNT][0x600];static int alive[OBJECT_COUNT],serials[OBJECT_COUNT],world_calls,restores,refreshes,bounds_ok;
static int merc_mode;static unsigned merc_kind[OBJECT_COUNT];
static int merchant_mode,merchant_class_ok,merchant_frame_ok,merchant_inventory_ok;
static unsigned merchant_getter[2];static uintptr_t g_ue4_base_cached;
static unsigned g_comp_w=2400,g_comp_h=1080;static float g_fov_h_deg=90;
static float g_ptr_u,g_ptr_v;
#include UI_POINTER_FRAGMENT
static int index_of(const void* p){for(int i=0;i<OBJECT_COUNT;i++)if(p==objects[i])return i;return -1;}
static int pv_alive(const void* p){int i=index_of(p);return i>=0&&alive[i];}
static void weak_assign(struct cinema_ui_weak* ref,const void* object){ref->index=index_of(object);ref->serial=ref->index<0?0:serials[ref->index];}
static void* weak_get(const struct cinema_ui_weak* ref){return ref->index>=0&&ref->index<OBJECT_COUNT&&alive[ref->index]&&serials[ref->index]==ref->serial?objects[ref->index]:0;}
static void(*cu_weak_assign)(struct cinema_ui_weak*,const void*)=weak_assign;
static void*(*cu_weak_get)(const struct cinema_ui_weak*)=weak_get;
static void* scene_class(void){return(void*)1;}static void* static_class(void){return(void*)2;}static void* button_class(void){return(void*)3;}
static void* screen_class(void){return(void*)7;}
static void* scroll_class(void){return(void*)10;}static void*(*fv_scroll_class)(void)=scroll_class;
static void*(*fv_screen_class)(void)=screen_class;
static void* camera_class(void){return(void*)4;}static void* pawn_class(void){return(void*)5;}
static void*(*mv_scene_class)(void)=scene_class,*(*mv_static_class)(void)=static_class,*(*fv_button_class)(void)=button_class;
static void*(*cinema_camera_class)(void)=camera_class,*(*mv_pawn_class)(void)=pawn_class;
static int death_class_ok=1,death_property_ok=1,death_button_ok=1,results_mode,case_mode,curved_mode;
static void* actor_class(void){return(void*)8;}static void*(*pv_actor_class)(void)=actor_class;
static void* child_class(void){return(void*)9;}static void*(*pv_child_class)(void)=child_class;
static unsigned char parent_property[0x60];static int parent_component_class_ok=1;
static const char* cinema_field_property(const void* object,const char* name,const char* kind){
    return (object==objects[10]||object==objects[22]||(merchant_mode&&(object==objects[32]||object==objects[34])))&&!strcmp(name,"ParentComponent")&&!strcmp(kind,"WeakObjectProperty")?(const char*)parent_property:0;
}
static const unsigned case_parts[]={15,16,17,18,19,1,2};
static const char* const case_fields[]={"Background_Plane","Frame_Mesh_Back","Frame_Mesh_Sides",
    "DoorMesh_Left","DoorMesh_Right","DoorExtension_Left","DoorExtension_Right"};
static float case_origins[20][3],case_extents[20][3];static unsigned bounds_calls,case_panel_queries[2];
static int menu_visual_layout(const void* p,const void* cls,int size){(void)size;
    if(merc_mode&&pv_alive(p)) {
        unsigned kind=merc_kind[index_of(p)];
        if(cls==(void*)1)return !!(kind&1);if(cls==(void*)3)return !!(kind&4);
        if(cls==(void*)6)return !!(kind&2);if(cls==(void*)8)return !!(kind&16);
        if(cls==(void*)10)return !!(kind&8);
    }
    return pv_alive(p)&&(cls==(void*)1
    ||(cls==(void*)2&&(p==objects[3]||p==objects[10]||(case_mode&&(p==objects[15]||p==objects[16]||p==objects[17]||p==objects[18]||p==objects[19]||p==objects[1]||p==objects[2]))))||(cls==(void*)3&&(p==objects[6]||p==objects[15]||p==objects[18]))
    ||(cls==(void*)4&&p==objects[12])||(cls==(void*)5&&p==objects[11])
    ||(cls==(void*)6&&(p==objects[10]||(curved_mode&&p==objects[21])||(case_mode&&(p==objects[24]||p==objects[27]||p==objects[28]))))
    ||(cls==(void*)7&&((p==objects[8]&&(results_mode||case_mode))||(case_mode&&(p==objects[22]||p==objects[25]))))
    ||(cls==(void*)8&&(p==objects[8]||p==objects[10]||p==objects[13]||p==objects[22]||p==objects[25]||(merchant_mode&&(p==objects[30]||p==objects[32]||p==objects[34]))))
    ||(cls==(void*)9&&parent_component_class_ok&&(p==objects[20]||p==objects[21]||p==objects[29]||(merchant_mode&&(p==objects[36]||p==objects[37])))));}
static int cinema_object(const void* p,const void* cls,int size){return menu_visual_layout(p,cls,size);}
static int cinema_ui_named(const void* p,const char* name,int size){
    (void)size;return pv_alive(p)&&((p==objects[0]&&!strcmp(name,"OptionsUIScreen_BP_C"))
        ||(p==objects[1]&&!strcmp(name,"UI_PauseMenu_Content_GameOptions_BP_C"))
        ||(p==objects[7]&&!strcmp(name,"OptionsCoverScreen_BP_C"))
        ||(p==objects[8]&&death_class_ok&&!results_mode&&!strcmp(name,"DeathMenu_BP_C"))
        ||(p==objects[8]&&results_mode==1&&!strcmp(name,"UI_MercenariesResults_BP_C"))
        ||(p==objects[8]&&results_mode==2&&!strcmp(name,"UI_ChapterSummary_BP_C"))
        ||(p==objects[13]&&case_mode==1&&!strcmp(name,"UI_MercMenu_Content_CreateGame_BP_C"))
        ||(p==objects[13]&&case_mode==2&&!strcmp(name,"UI_PauseMenu_Content_NewGame_BP_C"))
        ||(p==objects[13]&&case_mode==3&&!strcmp(name,"UI_PauseMenu_Content_Options_BP_C"))
        ||(merchant_mode&&merchant_class_ok&&p==objects[30]&&!strcmp(name,"UI_MerchantScreen_BP_C"))
        ||(merchant_mode&&merchant_frame_ok&&p==objects[32]&&!strcmp(name,"UI_MerchantScreen_InventoryFrame_BP_C"))
        ||(merchant_mode&&merchant_inventory_ok&&p==objects[34]&&!strcmp(name,"InventoryUIScreen_BP_C"))
        ||((p==objects[13]||p==objects[16])&&death_button_ok&&!strcmp(name,"Button_Death_C")));
}
static void* cinema_ui_property(void* object,const char* cls,const char* field,int offset){
    assert(!strcmp(cls,"DeathMenu_BP_C")&&!strcmp(field,"Screen")&&offset==0x318);
    return death_property_ok&&cinema_ui_named(object,cls,0x498)?*(void**)((char*)object+offset):0;
}
static void* fv_object_property(const void* object,const char* name){
    if(merchant_mode&&object==objects[32]) {
        if(!strcmp(name,"Ref_InventoryUI"))return *(void* const*)(objects[32]+0x3d0);
        if(!strcmp(name,"MerchantScreen"))return *(void* const*)(objects[32]+0x268);
    }
    if(case_mode && object==objects[13])for(unsigned i=0;i<7;i++)if(!strcmp(name,case_fields[i]))return objects[case_parts[i]];
    if(object==objects[8]&&((results_mode==1&&!strcmp(name,"Widget_Results"))
        ||(results_mode==2&&!strcmp(name,"Widget_Unlocks"))))return objects[10];
    if(object==objects[7]&&!strcmp(name,"OptionsUIScreenBP"))return objects[0];
    if(object==objects[0]&&!strcmp(name,"TrifoldBaseBP"))return objects[1];
    if(object==objects[1]&&!strcmp(name,"UIScreen"))return objects[0];
    if((object==objects[13]||object==objects[16])&&!strcmp(name,"ParentMenu"))return *(void*const*)((const char*)object+0x290);
    if(object==objects[1]&&!strcmp(name,"Background_Plane"))return objects[3];return 0;
}
static int fv_frame_owned(void* root,void* owner){return pv_alive(root)&&pv_alive(owner)&&*(void*const*)((char*)root+0xa0)==owner&&*(void*const*)((char*)owner+0x130)==root;}
static int cinema_movie_screen_bounds(void* component,float origin[3],float extent[3]){
    if(case_mode){int i=index_of(component);assert(i>=0);bounds_calls++;memcpy(origin,case_origins[i],12);memcpy(extent,case_extents[i],12);return bounds_ok;}
    if(component==objects[10]){origin[0]=-30.0933837890625f;origin[1]=origin[2]=0;extent[0]=29.57433319091797f;extent[1]=208.4534149169922f;extent[2]=142.9199981689453f;return bounds_ok;}
    assert(component==objects[3]);memset(origin,0,12);extent[0]=extent[1]=50;extent[2]=0;return bounds_ok;
}
static struct reload_tf original;
static struct reload_tf relative(const void* root){assert(root==objects[2]);struct reload_tf tf;memcpy(&tf,objects[2]+0x1c0,sizeof tf);return tf;}
static void native_world(void* root,const struct reload_tf* tf,int sweep,void* hit,int teleport){
    assert(root==objects[2]&&!sweep&&!hit&&!teleport);world_calls++;
    struct reload_tf previous;memcpy(&previous,objects[2]+0x1c0,sizeof previous);
    qf_t inverse={-previous.q.x,-previous.q.y,-previous.q.z,previous.q.w},delta=q_mul(tf->q,inverse);
    for(unsigned child=3;child<=6;child++) {
    if(child==5)continue;
    struct reload_tf mesh;memcpy(&mesh,objects[child]+0x1c0,sizeof mesh);
    float local[3],offset[3];for(unsigned i=0;i<3;i++)local[i]=mesh.p[i]-previous.p[i];q_rot(inverse,local,offset);
    for(unsigned i=0;i<3;i++)offset[i]=offset[i]*tf->s[i]/previous.s[i];q_rot(tf->q,offset,local);
    for(unsigned i=0;i<3;i++){mesh.p[i]=tf->p[i]+local[i];mesh.s[i]*=tf->s[i]/previous.s[i];}
    mesh.q=q_mul(delta,mesh.q);memcpy(objects[child]+0x1c0,&mesh,sizeof mesh);
    }
    memcpy(objects[2]+0x1c0,tf,sizeof *tf);
}
static void native_relative(void* root,const struct reload_tf* tf,int sweep,void* hit,int teleport){restores++;assert(!memcmp(tf,&original,sizeof original));native_world(root,tf,sweep,hit,teleport);}
static void refresh(void* screen,void* root){assert(screen==objects[0]&&root==objects[4]);refreshes++;}
static void(*rl_world_transform)(void*,const struct reload_tf*,int,void*,int)=native_world;
static void(*fv_set_relative)(void*,const struct reload_tf*,int,void*,int)=native_relative;
static struct reload_tf(*fv_relative_transform)(const void*)=relative;
static void(*fv_screen_moved)(void*,void*)=refresh;
static void fv_frame_restore(void){}
static unsigned current_type;
static long current_ms, fv_lobby_zoom_ms;
static int on_game_thread;
static void* current_pawn,*current_screen;
static void* manager(void){return objects[0];}
static int enabled(const void* object){return merc_mode&&pv_alive(object)?*((const unsigned char*)object+0x264)!=0:object!=0;}
static void* active(void){assert(on_game_thread);return current_pawn;}
static unsigned type(void){assert(on_game_thread);return current_type;}
static void*(*fv_ui_manager)(void)=manager;
static void* peek(void* object){assert(object==objects[0]);return current_screen;}
static void*(*fv_peek_screen)(void*)=peek;
static int(*fv_ui_enabled)(const void*)=enabled;
static void*(*phone_active)(void)=active;
static unsigned(*phone_type)(void)=type;
struct cinema_ui_vec2{float x,y;};
static void* widget_class(void){return(void*)6;}
static void*(*cu_component_class)(void)=widget_class;
static struct cinema_ui_vec2 draw_size(const void* part){
    if(merc_mode&&(merc_kind[index_of(part)]&2)){int size[2];memcpy(size,(const char*)part+0x450,8);return(struct cinema_ui_vec2){(float)size[0],(float)size[1]};}
    if(curved_mode&&(part==objects[10]||part==objects[21])){int size[2];memcpy(size,(const char*)part+0x450,8);return(struct cinema_ui_vec2){(float)size[0],(float)size[1]};}
    if(case_mode&&(part==objects[24]||part==objects[27]||part==objects[28])){
        if(part==objects[24])case_panel_queries[0]++;if(part==objects[27])case_panel_queries[1]++;
        int size[2];memcpy(size,(const char*)part+0x450,sizeof size);return(struct cinema_ui_vec2){(float)size[0],(float)size[1]};
    }
    assert(part==objects[10]);return(struct cinema_ui_vec2){1200,700};
}
static struct cinema_ui_vec2(*cu_draw_size)(const void*)=draw_size;
static int(*fv_control_plane)(void*,struct fv_plane*);
#include UI_WIDGET_FRAGMENT
static int cinema_on_game_thread(void){return on_game_thread;}
static long ptr_now_ms(void){return current_ms;}
static int g_touch_context,g_menu_view_active;
static float g_look_yaw,g_look_pitch,g_yaw_turned,g_menu_yaw_off,g_menu_pitch;
static long g_lobby_menu_ms,g_menu_view_ms;
static float quick_turn_offset(void){return 0;}
#include "../re4fix_mobile_view.h"
static int mock_clock_gettime(int clock_id,struct timespec* ts){(void)clock_id;ts->tv_sec=current_ms/1000;ts->tv_nsec=(current_ms%1000)*1000000;return 0;}
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif
#define clock_gettime mock_clock_gettime
#include UI_HEAD_FRAGMENT
#undef clock_gettime
#define LOGF(...) ((void)0)
#include "../re4fix_ui_console.h"
static struct reload_tf camera={{0,0,0,1},{0,0,160},{1,1,1},{0,0}};
static void* parts[1];
static void ready(void){
    merchant_mode=0;merchant_class_ok=merchant_frame_ok=merchant_inventory_ok=1;g_ue4_base_cached=0;
    merc_mode=0;memset(merc_kind,0,sizeof merc_kind);
    memset(objects,0,sizeof objects);memset(&fv_console,0,sizeof fv_console);
    memset(&fv_camera_menu,0,sizeof fv_camera_menu);memset(fv_case_mesh_bounds,0,sizeof fv_case_mesh_bounds);
    fv_case_parent_class=0;parent_component_class_ok=1;memset(parent_property,0,sizeof parent_property);
    *(int*)(parent_property+0x4c)=0x160;*(int*)(parent_property+0x34)=1;*(int*)(parent_property+0x38)=8;
    death_class_ok=death_property_ok=death_button_ok=1;results_mode=case_mode=curved_mode=0;bounds_calls=0;memset(case_panel_queries,0,sizeof case_panel_queries);
    fv_current_draw_size=draw_size;
    g_menu_view_active=0;g_menu_yaw_off=g_menu_pitch=0;g_menu_view_ms=g_lobby_menu_ms=0;fv_lobby_zoom_ms=0;
    for(int i=0;i<OBJECT_COUNT;i++)alive[i]=serials[i]=1;
    *(void**)(objects[1]+0x130)=objects[2];*(void**)(objects[2]+0xa0)=objects[1];*(void**)(objects[2]+0xc0)=objects[5];
    *(void**)(objects[3]+0xa0)=objects[1];*(void**)(objects[3]+0xc0)=objects[2];
    *(void**)(objects[0]+0x130)=objects[4];*(void**)(objects[4]+0xa0)=objects[0];
    *(void**)(objects[4]+0xc0)=objects[2];*(void**)(objects[6]+0xc0)=objects[4];*(void**)(objects[6]+0x308)=objects[0];
    parts[0]=objects[6];struct{void**data;int count,max;} list={parts,1,1};memcpy(objects[0]+0x238,&list,sizeof list);
    original=(struct reload_tf){{0,0,0,1},{200,0,160},{1,1,1},{0,0}};
    memcpy(objects[2]+0x1c0,&original,sizeof original);
    memcpy(objects[4]+0x1c0,&original,sizeof original);memcpy(objects[6]+0x1c0,&original,sizeof original);
    // Native authored backing up differs from its UI: the old mesh-only basis turns all text upside down.
    struct reload_tf mesh=original;mesh.q=(qf_t){0,-.7071067811865475f,0,.7071067811865475f};memcpy(objects[3]+0x1c0,&mesh,sizeof mesh);
    world_calls=restores=refreshes=0;bounds_ok=1;
}
static void check_camera_fov(int expected){
    unsigned char view[0x100],before[sizeof view];memset(view,0x5a,sizeof view);
    float authored=90;memcpy(view+0x18,&authored,4);memcpy(before,view,sizeof view);
    g_fov_h_deg=90;fv_menu_camera_view(objects[7],view);
    float actual;memcpy(&actual,view+0x18,4);
    assert(fabsf(actual-(expected?67.380135f:90))<.0001f&&g_fov_h_deg==actual);
    memcpy(view+0x18,before+0x18,4);assert(!memcmp(view,before,sizeof view));
}
static void test_camera_fov(void){
    ready();current_pawn=objects[5];*(void**)(objects[5]+0x2d8)=objects[7];
    on_game_thread=1;current_ms=1000;fv_lobby_zoom_ms=1000;
    for(current_type=0;current_type<=8;current_type++)check_camera_fov(current_type==2||current_type==4||current_type==7);
    current_type=7;fv_lobby_zoom_ms=701;check_camera_fov(1);fv_lobby_zoom_ms=700;check_camera_fov(0);
    fv_lobby_zoom_ms=1001;check_camera_fov(0);fv_lobby_zoom_ms=0;check_camera_fov(0);fv_lobby_zoom_ms=-1;check_camera_fov(0);
    fv_lobby_zoom_ms=1000;on_game_thread=0;check_camera_fov(0);on_game_thread=1;
    current_pawn=0;check_camera_fov(0);current_pawn=objects[5];*(void**)(objects[5]+0x2d8)=objects[6];check_camera_fov(0);
}
static void test_merchant_fov(void){
    ready();current_pawn=objects[11];*(void**)(objects[11]+0x2d8)=objects[12];*(void**)(objects[12]+0xa0)=objects[11];
    current_type=g_touch_context=8;on_game_thread=1;current_ms=1000;
    const unsigned widths[]={1080,1920,2336,2400,2640,4320};
    unsigned char view[0x100],before[sizeof view];
    for(unsigned i=0;i<sizeof widths/sizeof *widths;i++){
        g_comp_w=widths[i];g_comp_h=1080;memset(view,0x5a,sizeof view);float authored=90;memcpy(view+0x18,&authored,4);memcpy(before,view,sizeof view);
        fv_menu_camera_view(objects[12],view);float fov;memcpy(&fov,view+0x18,4);
        assert(fabsf(2*atanf(tanf(fov*.00872664626f)*g_comp_h/g_comp_w)*57.2957795f-60)<.0001f);
        assert(fov==g_fov_h_deg);memcpy(view+0x18,before+0x18,4);assert(!memcmp(view,before,sizeof view));
    }
    for(unsigned refusal=0;refusal<8;refusal++){
        ready();current_pawn=objects[11];*(void**)(objects[11]+0x2d8)=objects[12];*(void**)(objects[12]+0xa0)=objects[11];
        current_type=8;on_game_thread=1;g_comp_w=2336;g_comp_h=1080;
        if(refusal==0)current_pawn=0;if(refusal==1)*(void**)(objects[11]+0x2d8)=objects[7];
        if(refusal==2)on_game_thread=0;if(refusal==3)g_comp_h=0;if(refusal==4)g_comp_w=100;
        if(refusal==5)g_comp_w=4321;if(refusal==6)*(void**)(objects[12]+0xa0)=objects[10];if(refusal==7)alive[11]=0;
        memset(view,0x5a,sizeof view);float authored=90;memcpy(view+0x18,&authored,4);memcpy(before,view,sizeof view);
        fv_menu_camera_view(objects[12],view);assert(!memcmp(view,before,sizeof view));
    }
    g_comp_w=2400;g_comp_h=1080;
    puts("PASS: actual merchant camera keeps60-degree vertical view across6 display aspects including reported devices; only FOV bytes change;8 inactive/foreign/invalid-size refusals");
}
static void merchant_detail_ready(int inventory){
    ready();merchant_mode=1;current_type=g_touch_context=8;on_game_thread=1;current_ms=1000;g_comp_w=2424;g_comp_h=1080;
    current_pawn=objects[11];*(void**)(objects[11]+0x2d8)=objects[12];*(void**)(objects[12]+0xa0)=objects[11];
    current_screen=inventory?objects[34]:objects[30];objects[30][0x314]=4;
    for(unsigned actor=30;actor<=34;actor+=2){*(void**)(objects[actor]+0x130)=objects[actor+1];*(void**)(objects[actor+1]+0xa0)=objects[actor];}
    *(void**)(objects[34]+0xe0)=objects[32];*(void**)(objects[32]+0xe0)=objects[30];
    *(void**)(objects[34]+0x10)=objects[38];*(void**)(objects[32]+0x10)=objects[39];
    *(void**)(objects[32]+0x268)=objects[30];*(void**)(objects[32]+0x3d0)=objects[34];
    merchant_getter[0]=0x394c5000;merchant_getter[1]=0xd65f03c0;g_ue4_base_cached=(uintptr_t)merchant_getter-0x63168dc;
}
static void merchant_detail_weak(void){
    for(unsigned child=0;child<2;child++){
        unsigned actor=child?32:34,parent=child?30:32,component=36+child;
        *(void**)(objects[actor]+0xe0)=0;struct cinema_ui_weak weak={(int)component,serials[component]};memcpy(objects[actor]+0x160,&weak,sizeof weak);
        *(void**)(objects[component]+0x1f8)=objects[actor];*(void**)(objects[component]+0xa0)=objects[parent];*(void**)(objects[component]+0x20)=objects[parent];
        *(void**)(objects[actor+1]+0xc0)=objects[component];*(void**)(objects[component]+0xc0)=objects[parent+1];
    }
}
static void check_merchant_detail(float vertical){
    static unsigned char unchanged[sizeof objects];memcpy(unchanged,objects,sizeof objects);
    unsigned char view[0x100],before[sizeof view];
    for(unsigned frame=0;frame<20;frame++){
        memset(view,0x5a,sizeof view);float authored=90;memcpy(view+0x18,&authored,4);memcpy(before,view,sizeof view);
        fv_menu_camera_view(objects[12],view);float actual;memcpy(&actual,view+0x18,4);
        float actual_vertical=2*atanf(tanf(actual*.00872664626f)*g_comp_h/g_comp_w)*57.2957795f;
        if(fabsf(actual_vertical-vertical)>=.0001f||actual!=g_fov_h_deg)fprintf(stderr,"merchant detail top=%d state=%u width=%u expected=%.3f actual=%.3f\n",index_of(current_screen),objects[30][0x314],g_comp_w,vertical,actual_vertical);
        assert(fabsf(actual_vertical-vertical)<.0001f&&actual==g_fov_h_deg);
        memcpy(view+0x18,before+0x18,4);assert(!memcmp(view,before,sizeof view));
    }
    assert(!memcmp(unchanged,objects,sizeof objects));
    struct reload_tf camera={{0,0,0,1},{0,0,0},{1,1,1},{0,0}};
    for(unsigned u=0;u<=4;u++)for(unsigned v=0;v<=4;v++){
        g_ptr_u=(float)u/4;g_ptr_v=(float)v/4;float direction[3],position[3],sx,sy,depth;
        native_pointer(direction,position);
        float world[3]={-direction[2]*1000,direction[0]*1000,direction[1]*1000};
        assert(wt_project(&camera,world,&sx,&sy,&depth));
        assert(fabsf(sx-g_ptr_u*g_comp_w)<.003f&&fabsf(sy-g_ptr_v*g_comp_h)<.003f&&depth>5);
        for(unsigned k=0;k<3;k++)assert(fabsf(position[k]+direction[k]*.35f)<.000001f);
    }
}
static void test_merchant_detail(void){
    const unsigned widths[]={1080,1920,2336,2424,2640,4320};
    for(unsigned inventory=0;inventory<2;inventory++)for(unsigned state=0;state<18;state++)for(unsigned i=0;i<6;i++){
        merchant_detail_ready((int)inventory);objects[30][0x314]=(unsigned char)state;g_comp_w=widths[i];
        check_merchant_detail(i&&(state==4||state==12||state==14)?45:60);
    }
    for(unsigned refusal=0;refusal<24;refusal++){
        merchant_detail_ready(1);
        if(refusal==0)g_ue4_base_cached=0;if(refusal==1)merchant_getter[0]=0;if(refusal==2)current_screen=0;if(refusal==3)alive[0]=0;
        if(refusal==4)alive[30]=0;if(refusal==5)merchant_class_ok=0;if(refusal==6)*(void**)(objects[31]+0xa0)=objects[1];
        if(refusal==7)merchant_inventory_ok=0;if(refusal==8)alive[34]=0;if(refusal==9)merchant_frame_ok=0;
        if(refusal==10)*(void**)(objects[32]+0x3d0)=objects[1];if(refusal==11)*(void**)(objects[32]+0x268)=0;
        if(refusal==12)*(void**)(objects[32]+0xe0)=objects[1];if(refusal==13)*(void**)(objects[34]+0xe0)=objects[1];
        if(refusal==14)*(void**)(objects[35]+0xa0)=objects[1];if(refusal==15)alive[32]=0;if(refusal==16)alive[31]=0;
        if(refusal==17)fv_peek_screen=0;if(refusal==18)merchant_getter[1]=0;if(refusal==19)*(void**)(objects[30]+0x130)=0;
        if(refusal==20)*(void**)(objects[32]+0x3d0)=0;if(refusal==21)*(void**)(objects[32]+0x268)=objects[13];
        if(refusal==22)*(void**)(objects[33]+0xa0)=objects[1];if(refusal==23)objects[30][0x314]=255;
        check_merchant_detail(60);fv_peek_screen=peek;
    }
    merchant_detail_ready(1);merchant_detail_weak();check_merchant_detail(45);
    for(unsigned refusal=0;refusal<6;refusal++){
        merchant_detail_ready(1);merchant_detail_weak();
        if(refusal==0)serials[36]++;if(refusal==1)*(void**)(objects[36]+0x1f8)=objects[1];
        if(refusal==2)*(void**)(objects[36]+0xa0)=objects[1];if(refusal==3)*(void**)(objects[36]+0x20)=objects[1];
        if(refusal==4)*(void**)(objects[35]+0xc0)=objects[1];if(refusal==5)*(void**)(objects[37]+0x1f8)=objects[1];
        check_merchant_detail(60);
    }
    g_comp_w=2400;g_comp_h=1080;
    puts("PASS: actual merchant45-degree detail view only states4/12/14 at16:9 or wider; all18 states across6 aspects and both owned top routes;24 guards plus6 weak-owner refusals,20 stable repeats, view-only writes; actual hand-ray/project round trips25 pixels per case");
}
static void* death_parts[2];
static struct reload_tf death_camera(void){
    float yaw,pitch;head_angles_now(&yaw,&pitch);
    qf_t head=q_mul((qf_t){0,0,-sinf(yaw*.5f),cosf(yaw*.5f)},(qf_t){0,-sinf(pitch*.5f),0,cosf(pitch*.5f)});
    float inherited=52*.0174532925199433f;
    return(struct reload_tf){q_mul((qf_t){0,0,sinf(inherited*.5f),cosf(inherited*.5f)},head),{8,0,19966.4f},{1,1,1},{0,0}};
}
static void death_ready(void){
    ready();current_ms=1000;current_type=4;g_touch_context=4;on_game_thread=1;current_pawn=objects[11];current_screen=objects[8];
    g_look_yaw=52*.0174532925199433f;g_look_pitch=.3f;g_yaw_turned=0;
    *(void**)(objects[8]+0x130)=objects[9];*(void**)(objects[9]+0xa0)=objects[8];
    *(void**)(objects[8]+0x318)=objects[10];*(void**)(objects[10]+0xa0)=*(void**)(objects[10]+0x20)=objects[8];
    *(void**)(objects[10]+0xc0)=objects[9];
    *(void**)(objects[11]+0x2d8)=objects[12];*(void**)(objects[12]+0xa0)=objects[11];
    struct reload_tf root={{0,0,0,1},{0,0,20000},{1,1,1},{0,0}};
    memcpy(objects[9]+0x1c0,&root,sizeof root);
    struct reload_tf surface={{0,0,0,1},{608.8058471679688f,.00042438507080078125f,20000.00079345703125f},{2.174999952316284f,2,1.75f},{0,0}};
    memcpy(objects[10]+0x1c0,&surface,sizeof surface);
    for(unsigned i=0;i<2;i++) {
        unsigned actor=13+3*i,button=actor+2;
        *(void**)(objects[actor]+0x130)=objects[actor+1];*(void**)(objects[actor]+0x290)=objects[8];
        *(void**)(objects[actor+1]+0xa0)=objects[actor];
        *(void**)(objects[button]+0x308)=objects[8];*(void**)(objects[button]+0xa0)=*(void**)(objects[button]+0x20)=objects[actor];
        *(void**)(objects[button]+0xc0)=objects[actor+1];death_parts[i]=objects[button];
        // The original Button_Death roots are separate world roots, not DeathMenu descendants.
        struct reload_tf button_root={{0,0,0,1},{516,i?72.0f:-72.0f,19854},{1,1,1},{0,0}};
        memcpy(objects[actor+1]+0x1c0,&button_root,sizeof button_root);button_root.q=(qf_t){0,0,1,0};memcpy(objects[button]+0x1c0,&button_root,sizeof button_root);
        *(float*)(objects[button]+0x318)=120;*(float*)(objects[button]+0x31c)=30;
    }
    struct{void**data;int count,max;} list={death_parts,2,2};memcpy(objects[8]+0x238,&list,sizeof list);
}
static void death_view(int expected){
    unsigned char view[0x100],before[sizeof view];memset(view,0x5a,sizeof view);float fov=90;memcpy(view+0x18,&fov,4);memcpy(before,view,sizeof view);
    fv_menu_camera_view(objects[12],view);memcpy(&fov,view+0x18,4);
    if(expected)assert(fabsf(fov-fv_camera_menu.fov)<.0001f);else assert(fov==90);
    memcpy(view+0x18,before+0x18,4);assert(!memcmp(view,before,sizeof view));
}
static void test_death_camera(void){
    death_ready();unsigned char before[sizeof objects];memcpy(before,objects,sizeof before);
    float yaw,pitch;head_angles_now(&yaw,&pitch);assert(yaw==0&&pitch==0);
    struct reload_tf current=death_camera(),surface;memcpy(&surface,objects[10]+0x1c0,sizeof surface);
    assert(!fv_descends_from(objects[15],objects[9]));
    assert(fv_camera_menu_tick(objects[8],&current,current_ms)&&fv_camera_menu.used&&g_menu_view_active);
    head_angles_now(&yaw,&pitch);assert(fabsf(yaw-52*.0174532925199433f)<.0001f&&pitch>0&&pitch<.15f);
    float stable_yaw=yaw,stable_pitch=pitch,stable_fov=fv_camera_menu.fov;
    assert(stable_fov>90&&stable_fov<130);death_view(1);
    float origin[3],extent[3];assert(cinema_movie_screen_bounds(objects[10],origin,extent));
    for(unsigned frame=0;frame<20;frame++) {
        current_ms+=16;current=death_camera();assert(fv_camera_menu_tick(objects[8],&current,current_ms));
        head_angles_now(&yaw,&pitch);assert(fabsf(yaw-stable_yaw)<.0001f&&fabsf(pitch-stable_pitch)<.0001f&&fabsf(fv_camera_menu.fov-stable_fov)<.0001f);
        current=death_camera();qf_t inv={-current.q.x,-current.q.y,-current.q.z,current.q.w};
        float tangent=tanf(fv_camera_menu.fov*.00872664625997165f);
        for(unsigned corner=0;corner<8;corner++) {
            float local[3],world[3],at[3];
            for(unsigned k=0;k<3;k++)local[k]=(origin[k]+(corner&(1u<<k)?extent[k]:-extent[k]))*surface.s[k];
            q_rot(surface.q,local,world);for(unsigned k=0;k<3;k++)world[k]+=surface.p[k]-current.p[k];q_rot(inv,world,at);
            assert(at[0]>1&&fabsf(at[1])/at[0]<=tangent*.861f&&fabsf(at[2])/at[0]<=tangent/(2400.0f/1080)*.861f);
        }
        // All corners of the original120x30 hit planes also fit without any native object write.
        for(unsigned actor=15;actor<=18;actor+=3) {
            struct reload_tf button;memcpy(&button,objects[actor]+0x1c0,sizeof button);
            for(unsigned corner=0;corner<4;corner++) {
                float local[3]={0,corner&1?60.0f:-60.0f,corner&2?15.0f:-15.0f},world[3],at[3];q_rot(button.q,local,world);
                for(unsigned k=0;k<3;k++)world[k]+=button.p[k]-current.p[k];q_rot(inv,world,at);
                assert(at[0]>1&&fabsf(at[1])/at[0]<tangent&&fabsf(at[2])/at[0]<tangent/(2400.0f/1080));
            }
        }
    }
    assert(!memcmp(before,objects,sizeof before)&&!world_calls&&!restores&&!refreshes);
    current_ms=fv_camera_menu.stamp+299;death_view(1);head_angles_now(&yaw,&pitch);assert(fabsf(yaw-stable_yaw)<.0001f);
    current_ms++;death_view(0);head_angles_now(&yaw,&pitch);assert(!yaw&&!pitch);
    current_ms=fv_camera_menu.stamp-1;death_view(0);head_angles_now(&yaw,&pitch);assert(!yaw&&!pitch);
    death_ready();current=death_camera();assert(fv_camera_menu_tick(objects[8],&current,current_ms));serials[8]++;death_view(0);
    death_ready();current=death_camera();assert(fv_camera_menu_tick(objects[8],&current,current_ms));current_screen=objects[0];death_view(0);
    for(unsigned refusal=0;refusal<12;refusal++) {
        death_ready();current=death_camera();
        if(refusal==0)current_type=1;if(refusal==1)on_game_thread=0;if(refusal==2)death_class_ok=0;
        if(refusal==3)death_property_ok=0;if(refusal==4)death_button_ok=0;if(refusal==5)current_screen=objects[0];
        if(refusal==6)*(void**)(objects[13]+0x290)=objects[0];if(refusal==7)*(void**)(objects[12]+0xa0)=objects[0];
        if(refusal==8)*(void**)(objects[10]+0xa0)=objects[0];if(refusal==9)*(int*)(objects[8]+0x240)=17;
        if(refusal==10)bounds_ok=0;if(refusal==11)current.q.w=NAN;
        assert(!fv_camera_menu_tick(objects[8],&current,current_ms)&&!fv_camera_menu.used&&!g_menu_view_active);
    }
    puts("PASS: actual DeathMenu detached native roots remain byte-identical; original bounds+52degree inherited anchor fit8 corners and both120x30 hit planes; real head_angles_now context4 correction stable20frames, stale/future refusal, weak/current screen and12 ownership/class/context/geometry guards");
}
static void test_results_camera(int chapter){
    death_ready();results_mode=chapter+1;
    static void* children[1];children[0]=objects[10];
    struct{void**data;int count,max;} child_list={children,1,1};memcpy(objects[9]+0xd0,&child_list,sizeof child_list);
    struct reload_tf widget={{0,0,1,0},{600,0,20000},{.5f,.5f,.5f},{0,0}};
    memcpy(objects[10]+0x1c0,&widget,sizeof widget);
    *(unsigned short*)(objects[10]+0x14c)=0x20;
    *(float*)(objects[10]+0x45c)=*(float*)(objects[10]+0x460)=.5f;
    unsigned char before[sizeof objects];memcpy(before,objects,sizeof before);
    float stable_yaw=0,stable_pitch=0,stable_fov=0;
    for(unsigned frame=0;frame<40;frame++){
        struct reload_tf current=death_camera();
        assert(fv_camera_menu_tick(objects[8],&current,current_ms));
        float yaw,pitch;head_angles_now(&yaw,&pitch);
        if(!frame){stable_yaw=yaw;stable_pitch=pitch;stable_fov=fv_camera_menu.fov;}
        assert(fabsf(yaw-stable_yaw)<.0001f&&fabsf(pitch-stable_pitch)<.0001f&&fabsf(fv_camera_menu.fov-stable_fov)<.0001f);
        assert(fabsf(yaw-52*.0174532925199433f)<.001f);death_view(1);
        struct reload_tf mesh,desired;float origin[3],extent[3],fov;
        assert(fv_camera_menu_bounds(objects[8],objects[9],&mesh,origin,extent));
        current=death_camera();assert(fv_bounds_camera(&current,&mesh,origin,extent,2400.0f/1080,&desired,&fov));
        qf_t inv={-current.q.x,-current.q.y,-current.q.z,current.q.w};float tangent=tanf(fov*.00872664625997165f);
        for(unsigned corner=0;corner<8;corner++){
            float local[3],world[3],at[3];
            for(unsigned k=0;k<3;k++)local[k]=(origin[k]+(corner&(1u<<k)?extent[k]:-extent[k]))*mesh.s[k];
            q_rot(mesh.q,local,world);for(unsigned k=0;k<3;k++)world[k]+=mesh.p[k]-current.p[k];q_rot(inv,world,at);
            assert(at[0]>1&&fabsf(at[1])/at[0]<tangent*.861f&&fabsf(at[2])/at[0]<tangent/(2400.0f/1080)*.861f);
        }
        current_ms+=16;
    }
    assert(!memcmp(before,objects,sizeof before)&&!world_calls&&!restores&&!refreshes);
    *(void**)(objects[9]+0xa0)=objects[0];struct reload_tf current=death_camera();assert(!fv_camera_menu_tick(objects[8],&current,current_ms));
    *(void**)(objects[9]+0xa0)=objects[8];*(float*)(objects[10]+0x45c)=NAN;assert(!fv_camera_menu_tick(objects[8],&current,current_ms));
    *(float*)(objects[10]+0x45c)=.5f;*(unsigned short*)(objects[10]+0x14c)=0;assert(!fv_camera_menu_tick(objects[8],&current,current_ms));
    printf("PASS: actual %s widget and detached button planes fit original camera for40 stable frames; shared actual widget size/pivot helper, all8 frustum corners, native roots byte-identical, owner/hidden/invalid pivot refusal\n",chapter==2?"unlisted native UIScreen":chapter?"ChapterSummary":"Mercenaries");
}
static void curved_ready(float inherited){
    death_ready();results_mode=2;curved_mode=1;
    g_look_yaw=inherited*.0174532925199433f;g_look_pitch=0;
    struct reload_tf root={{0,0,0,1},{-10000,-10000,-10000},{1,1,1},{0,0}};
    memcpy(objects[9]+0x1c0,&root,sizeof root);
    root.p[2]=10000;memcpy(objects[20]+0x1c0,&root,sizeof root);*(void**)(objects[20]+0xc0)=objects[9];
    *(void**)(objects[20]+0xa0)=objects[8];
    root.p[0]=-9677.1695f;root.p[1]=-9999.99976f;root.p[2]=9951.625f;
    root.q=(qf_t){0,0,1,0};root.s[0]=root.s[1]=root.s[2]=.22375f;
    memcpy(objects[10]+0x1c0,&root,sizeof root);*(void**)(objects[10]+0x20)=*(void**)(objects[10]+0xa0)=objects[8];
    *(void**)(objects[10]+0xc0)=objects[20];objects[10][0x518]=1;
    *(float*)(objects[10]+0x51c)=64.0761947631836f;
    *(int*)(objects[10]+0x450)=1920;*(int*)(objects[10]+0x454)=1080;
    *(float*)(objects[10]+0x45c)=*(float*)(objects[10]+0x460)=.5f;
    *(unsigned short*)(objects[10]+0x14c)=0x20;
    static void* children[2][1];children[0][0]=objects[20];children[1][0]=objects[10];
    struct{void**data;int count,max;} links={children[0],1,1};memcpy(objects[9]+0xd0,&links,sizeof links);
    links.data=children[1];memcpy(objects[20]+0xd0,&links,sizeof links);
    struct reload_tf button={{0,0,.9776f,-.2102f},{-9653.7f,-9844.6f,9838.4f},{2,2,2},{0,0}};
    float norm=sqrtf(button.q.z*button.q.z+button.q.w*button.q.w);button.q.z/=norm;button.q.w/=norm;
    memcpy(objects[15]+0x1c0,&button,sizeof button);*(float*)(objects[15]+0x318)=44;*(float*)(objects[15]+0x31c)=13;
    struct reload_tf button_root=button;button_root.q=(qf_t){0,0,.2102f,.9776f};
    norm=sqrtf(button_root.q.z*button_root.q.z+button_root.q.w*button_root.q.w);button_root.q.z/=norm;button_root.q.w/=norm;
    memcpy(objects[14]+0x1c0,&button_root,sizeof button_root);*(void**)(objects[14]+0xc0)=objects[23];
    *(void**)(objects[23]+0xc0)=objects[9];
    *(void**)(objects[23]+0xa0)=objects[8];
    struct reload_tf button_widget=button_root;float widget_offset[3];
    q_rot(button_root.q,(float[3]){1.7806957960128784f*2,-1.1162678248410884e-7f*2,-7.62939453125e-6f*2},widget_offset);
    for(unsigned k=0;k<3;k++){button_widget.p[k]+=widget_offset[k];button_widget.s[k]=.2f;}
    memcpy(objects[21]+0x1c0,&button_widget,sizeof button_widget);
    *(void**)(objects[21]+0xa0)=*(void**)(objects[21]+0x20)=objects[13];*(void**)(objects[21]+0xc0)=objects[14];
    *(int*)(objects[21]+0x450)=540;*(int*)(objects[21]+0x454)=220;
    *(float*)(objects[21]+0x45c)=*(float*)(objects[21]+0x460)=.5f;*(float*)(objects[21]+0x51c)=22;
    objects[21][0x518]=1;*(unsigned short*)(objects[21]+0x14c)=0x20;
    static void* button_children[2][1];button_children[0][0]=objects[14];button_children[1][0]=objects[21];
    struct{void**data;int count,max;} button_links={button_children[0],1,1};memcpy(objects[23]+0xd0,&button_links,sizeof button_links);
    button_links.data=button_children[1];memcpy(objects[14]+0xd0,&button_links,sizeof button_links);
    static void* root_children[2];root_children[0]=objects[20];root_children[1]=objects[23];
    struct{void**data;int count,max;} root_links={root_children,2,2};memcpy(objects[9]+0xd0,&root_links,sizeof root_links);
    struct{void**data;int count,max;} controls={death_parts,1,1};memcpy(objects[8]+0x238,&controls,sizeof controls);
}
static struct reload_tf curved_camera(float inherited){
    float yaw,pitch;head_angles_now(&yaw,&pitch);
    qf_t head=q_mul((qf_t){0,0,-sinf(yaw*.5f),cosf(yaw*.5f)},(qf_t){0,-sinf(pitch*.5f),0,cosf(pitch*.5f)});
    float half=inherited*.00872664625997165f;
    return(struct reload_tf){q_mul((qf_t){0,0,sinf(half),cosf(half)},head),{-9992,-10000,9965.3f},{1,1,1},{0,0}};
}
static void test_curved_results_camera(void){
    const float headings[]={0,-110.67f,77,179,-179};
    const float arcs[]={22,64.0761947631836f};
    for(int family=1;family<=3;family++)for(unsigned ai=0;ai<2;ai++)for(unsigned heading=0;heading<sizeof headings/sizeof *headings;heading++) {
        curved_ready(headings[heading]);unsigned char before[sizeof objects];memcpy(before,objects,sizeof before);
        results_mode=family;*(float*)(objects[10]+0x51c)=arcs[ai];memcpy(before,objects,sizeof before);
        struct fv_plane planar[2];assert(fv_collect_widgets(objects[9],planar,2)==0);
        struct fv_plane curved[4];assert(fv_camera_widgets(objects[8],objects[9],curved,4)==4);
        float stable_yaw=0,stable_pitch=0,stable_fov=0;
        for(unsigned frame=0;frame<40;frame++) {
            struct reload_tf current=curved_camera(headings[heading]);
            assert(fv_camera_menu_tick(objects[8],&current,current_ms));
            float yaw,pitch;head_angles_now(&yaw,&pitch);
            if(!frame){stable_yaw=yaw;stable_pitch=pitch;stable_fov=fv_camera_menu.fov;}
            assert(fabsf(yaw-stable_yaw)<.0001f&&fabsf(pitch-stable_pitch)<.0001f&&fabsf(fv_camera_menu.fov-stable_fov)<.0001f);
            current=curved_camera(headings[heading]);qf_t inv={-current.q.x,-current.q.y,-current.q.z,current.q.w};
            struct reload_tf widget;memcpy(&widget,objects[10]+0x1c0,sizeof widget);
            float arc=arcs[ai]*.0174532925199433f,radius=1920/arc,tangent=tanf(fv_camera_menu.fov*.00872664625997165f);
            for(unsigned sample=0;sample<=100;sample++)for(unsigned edge=0;edge<2;edge++) {
                float theta=arc*((float)sample/100-.5f);
                float local[3]={radius*(cosf(arc*.5f)-cosf(theta)),-radius*sinf(theta),edge?540.0f:-540.0f},world[3],at[3];
                for(unsigned k=0;k<3;k++)local[k]*=widget.s[k];q_rot(widget.q,local,world);
                for(unsigned k=0;k<3;k++)world[k]+=widget.p[k]-current.p[k];q_rot(inv,world,at);
                assert(at[0]>1&&fabsf(at[1])/at[0]<=tangent*.861f&&fabsf(at[2])/at[0]<=tangent/(2400.0f/1080)*.861f);
            }
            memcpy(&widget,objects[21]+0x1c0,sizeof widget);arc=22*.0174532925199433f;radius=540/arc;
            for(unsigned sample=0;sample<=100;sample++)for(unsigned edge=0;edge<2;edge++) {
                float theta=arc*((float)sample/100-.5f);
                float local[3]={radius*(cosf(arc*.5f)-cosf(theta)),-radius*sinf(theta),edge?110.0f:-110.0f},world[3],at[3];
                for(unsigned k=0;k<3;k++)local[k]*=widget.s[k];q_rot(widget.q,local,world);
                for(unsigned k=0;k<3;k++)world[k]+=widget.p[k]-current.p[k];q_rot(inv,world,at);
                assert(at[0]>1&&fabsf(at[1])/at[0]<=tangent*.861f&&fabsf(at[2])/at[0]<=tangent/(2400.0f/1080)*.861f);
            }
            current_ms+=16;
        }
        assert(!memcmp(before,objects,sizeof before)&&!world_calls&&!restores&&!refreshes);
    }
    for(unsigned refusal=0;refusal<8;refusal++) {
        curved_ready(-110.67f);struct reload_tf current=curved_camera(-110.67f);
        *(unsigned short*)(objects[21]+0x14c)=0;
        if(refusal==0)objects[10][0x518]=2;if(refusal==1)*(float*)(objects[10]+0x51c)=NAN;
        if(refusal==2)*(float*)(objects[10]+0x51c)=0;if(refusal==3)*(float*)(objects[10]+0x51c)=361;
        if(refusal==4)*(int*)(objects[10]+0x450)=0;if(refusal==5)*(float*)(objects[10]+0x45c)=NAN;
        if(refusal==6)*(unsigned short*)(objects[10]+0x14c)=0;if(refusal==7)*(void**)(objects[10]+0x20)=objects[0];
        assert(!fv_camera_menu_tick(objects[8],&current,current_ms));
    }
    for(unsigned refusal=0;refusal<4;refusal++) {
        curved_ready(-110.67f);struct fv_plane child[2];
        assert(fv_camera_widget_planes(objects[21],objects[8],child,2)==2);
        if(refusal==0)*(void**)(objects[21]+0x20)=objects[8];
        if(refusal==1)*(void**)(objects[14]+0xc0)=0;
        if(refusal==2)*(void**)(objects[14]+0xa0)=objects[8];
        if(refusal==3)*(void**)(objects[21]+0xc0)=objects[20];
        assert(!fv_camera_widget_planes(objects[21],objects[8],child,2));
    }
    puts("PASS: Cylinder1920x1080 main panel plus real ChildActor-owned Cylinder22degree540x220 Next widget and its44x13hitplane fit404 curved-edge samples for40 stable frames,3 nativeUIScreen families/2 main arcs/5 inherited headings including-110.67; native roots unchanged, planar root fitter still refuses,8 malformed/hidden/foreign refusals");
    unsigned total=0;
    for(unsigned capture=0;capture<sizeof native_curves/sizeof *native_curves;capture++) {
        curved_ready(0);const float* size=native_curves[capture].size;
        struct reload_tf unit={{0,0,0,1},{0,0,0},{1,1,1},{0,0}};memcpy(objects[10]+0x1c0,&unit,sizeof unit);
        *(float*)(objects[10]+0x51c)=native_curves[capture].arc;
        memcpy(objects[10]+0x45c,native_curves[capture].pivot,8);
        *(int*)(objects[10]+0x450)=(int)size[0];*(int*)(objects[10]+0x454)=(int)size[1];
        struct fv_plane planes[2];memset(planes,0x5a,sizeof planes);unsigned char saved[sizeof planes];memcpy(saved,planes,sizeof planes);
        assert(fv_camera_widget_planes(objects[10],objects[8],planes,0)==1&&!memcmp(saved,planes,sizeof planes));
        assert(fv_camera_widget_planes(objects[10],objects[8],planes,1)==2&&!memcmp(saved,planes,sizeof planes));
        assert(fv_camera_widget_planes(objects[10],objects[8],planes,2)==2);
        for(unsigned i=0;i<native_curves[capture].count;i++) {
            const float* vertex=native_curve_vertices[native_curves[capture].first+i];
            float tolerance=.004f;
            assert(vertex[0]>=planes[0].transform.p[0]-tolerance&&vertex[0]<=planes[1].transform.p[0]+tolerance);
            assert(fabsf(vertex[1]-planes[0].transform.p[1])<=planes[0].width*.5f+tolerance);
            assert(fabsf(vertex[2]-planes[0].transform.p[2])<=planes[0].height*.5f+tolerance);
            total++;
        }
    }
    printf("PASS: actual production cylinder bounds enclose%u original ARM64-generated mesh vertices across24 arc/pivot combinations; zero/one-plane capacity refusal writes nothing\n",total);
}
static void merc_ready(void){
    curved_ready(-110.67f);merc_mode=1;results_mode=1;
    static void* children[303];
    for(unsigned i=0;i<303;i++)children[i]=objects[merc_edges[i]];
    for(unsigned i=0;i<304;i++) {
        unsigned index=merc_nodes[i].index;unsigned char* part=objects[index];memset(part,0,sizeof objects[index]);
        merc_kind[index]=merc_nodes[i].kind;
        *(void**)(part+0xa0)=objects[merc_nodes[i].owner];*(void**)(part+0x20)=objects[merc_nodes[i].outer];
        *(void**)(part+0xc0)=merc_nodes[i].parent?objects[merc_nodes[i].parent]:0;
        struct{void**data;int count,max;} links={children+merc_nodes[i].first,(int)merc_nodes[i].count,(int)merc_nodes[i].count};
        memcpy(part+0xd0,&links,sizeof links);memcpy(part+0x1c0,merc_nodes[i].tf,48);
        *(unsigned short*)(part+0x14c)=(unsigned short)merc_nodes[i].flags;
    }
    for(unsigned i=0;i<5;i++) {
        unsigned actor=merc_owners[i].actor;merc_kind[actor]=16;
        *(void**)(objects[actor]+0x130)=objects[merc_owners[i].root];
    }
    memcpy(objects[10]+0x438,merc_main_widget,sizeof merc_main_widget);
    for(unsigned w=0;w<2;w++) {
        unsigned char* part=objects[w?24:21];
        *(int*)(part+0x450)=540;*(int*)(part+0x454)=220;
        *(float*)(part+0x45c)=*(float*)(part+0x460)=.5f;
        part[0x518]=1;*(float*)(part+0x51c)=22;
    }
    static void* controls[7];
    for(unsigned i=0;i<7;i++) {
        unsigned char* part=objects[merc_controls[i]];controls[i]=part;
        *(void**)(part+0x308)=objects[8];part[0x264]=i==0||i==2||i==3;
        *(float*)(part+0x318)=i<2?44:i==3?20:60;
        *(float*)(part+0x31c)=i<2?13:i==3?20:8;
    }
    struct{void**data;int count,max;} list={controls,7,7};memcpy(objects[8]+0x238,&list,sizeof list);
}
static struct reload_tf merc_camera(float heading){
    struct reload_tf eye=curved_camera(heading);eye.p[0]=-9998.6f;eye.p[2]=9967.7f;return eye;
}
static void test_live_merc_camera(void){
    merc_ready();unsigned char before[sizeof objects];memcpy(before,objects,sizeof before);
    struct fv_plane planes[64];unsigned count=fv_camera_widgets(objects[8],objects[9],planes,64);
    assert(count==6); // Three visible cylinders, including the disabled native Retry receiver's widget.
    assert(!enabled(objects[18]) && (objects[24][0x14c]&0x20));
    const float headings[]={-110.67f,0,179,-179};
    for(unsigned h=0;h<4;h++) {
        g_look_yaw=headings[h]*.0174532925199433f;g_menu_view_active=0;
        float stable_yaw=0,stable_pitch=0,stable_fov=0;
        for(unsigned frame=0;frame<40;frame++) {
            struct reload_tf eye=merc_camera(headings[h]);assert(fv_camera_menu_tick(objects[8],&eye,current_ms));
            float yaw,pitch;head_angles_now(&yaw,&pitch);
            if(!frame){stable_yaw=yaw;stable_pitch=pitch;stable_fov=fv_camera_menu.fov;}
            assert(fabsf(yaw-stable_yaw)<.0001f&&fabsf(pitch-stable_pitch)<.0001f&&fabsf(fv_camera_menu.fov-stable_fov)<.0001f);
            eye=merc_camera(headings[h]);qf_t inv={-eye.q.x,-eye.q.y,-eye.q.z,eye.q.w};
            const unsigned widgets[]={10,21,24};
            for(unsigned w=0;w<3;w++) {
                unsigned char* part=objects[widgets[w]];struct reload_tf tf;memcpy(&tf,part+0x1c0,sizeof tf);
                float width=(float)*(int*)(part+0x450),height=(float)*(int*)(part+0x454);
                float arc=*(float*)(part+0x51c)*.0174532925199433f,radius=width/arc;
                float tangent=tanf(fv_camera_menu.fov*.00872664625997165f);
                for(unsigned sample=0;sample<=100;sample++)for(unsigned edge=0;edge<2;edge++) {
                    float angle=arc*((float)sample/100-.5f),local[3]={radius*(cosf(arc*.5f)-cosf(angle)),-radius*sinf(angle),edge?height*.5f:-height*.5f},at[3],world[3];
                    for(unsigned k=0;k<3;k++)local[k]*=tf.s[k];q_rot(tf.q,local,world);
                    for(unsigned k=0;k<3;k++)world[k]+=tf.p[k]-eye.p[k];q_rot(inv,world,at);
                    assert(at[0]>1&&fabsf(at[1])/at[0]<=tangent*.861f&&fabsf(at[2])/at[0]<=tangent/(2400.0f/1080)*.861f);
                }
            }
            current_ms+=16;
        }
    }
    assert(!memcmp(before,objects,sizeof before)&&!world_calls&&!restores&&!refreshes);
    for(unsigned refusal=0;refusal<9;refusal++) {
        merc_ready();assert(fv_camera_ui_actor(objects[8],objects[13])&&fv_camera_ui_actor(objects[8],objects[16]));
        if(refusal==0)merc_kind[13]&=~16u;
        if(refusal==1)merc_kind[15]&=~4u;
        if(refusal==2)*(void**)(objects[15]+0x308)=objects[0];
        if(refusal==3)*(void**)(objects[15]+0xa0)=objects[8];
        if(refusal==4)*(void**)(objects[15]+0x20)=objects[8];
        if(refusal==5)*(void**)(objects[13]+0x130)=objects[17];
        if(refusal==6)*(void**)(objects[15]+0xc0)=objects[9];
        if(refusal==7)*(void**)(objects[14]+0xc0)=0;
        if(refusal==8){void** registered=*(void***)(objects[8]+0x238);registered[0]=objects[18];}
        assert(!fv_camera_ui_actor(objects[8],objects[13]));
    }
    merc_ready();merc_kind[15]=(merc_kind[15]&~4u)|8u;objects[15][0x264]=0;
    assert(fv_camera_ui_actor(objects[8],objects[13]));fv_scroll_class=0;
    assert(!fv_camera_ui_actor(objects[8],objects[13]));fv_scroll_class=scroll_class;
    merc_ready();memcpy(before,objects,sizeof before);
    // Pruned display actors are not native UI receivers, even if their scene objects remain live.
    assert(!fv_camera_ui_actor(objects[8],objects[26])&&!fv_camera_ui_actor(objects[8],objects[27]));
    assert(!memcmp(before,objects,sizeof before));
    // A genuinely oversized screen-owned tree still rejects at the unchanged scene-node cap.
    for(unsigned i=0;i<304;i++)if(merc_nodes[i].owner==26||merc_nodes[i].owner==27)
        *(void**)(objects[merc_nodes[i].index]+0xa0)=objects[8];
    assert(fv_camera_widgets(objects[8],objects[9],planes,64)==65);
    merc_ready();*(int*)(objects[9]+0xd8)=*(int*)(objects[9]+0xdc)=257;
    assert(fv_camera_widgets(objects[8],objects[9],planes,64)==65);
    puts("PASS: exact live Merc304-node/303-edge graph retains256 traversal cap; both132-node decorative display actors pruned, all3 visible cylinders including disabled Retry fit606 native edge samples for40 stable frames at4 inherited headings; every native object byte unchanged");
    puts("PASS: registered foreign UI actor scope requires native Button/Scroll type, same screen, component Owner/Outer, owned root and real attachment chain;9 changed/foreign registration refusals, disabled Scroll accepted, missing Scroll capability refused, oversized/malformed screen-owned trees still decline at256");
}
static void case_ready(void){
    ready();case_mode=1;current_ms=1000;current_type=2;g_touch_context=2;on_game_thread=1;
    current_screen=objects[8];current_pawn=objects[11];fv_lobby_zoom_ms=current_ms;
    *(void**)(objects[11]+0x2d8)=objects[12];*(void**)(objects[12]+0xa0)=objects[11];
    *(void**)(objects[8]+0xe0)=objects[10]; // Original deferred spawn Owner=MercGrid.
    // Actual p406 ChildActor topology: Grid.Owner is null; native ParentComponent carries its parent.
    *(void**)(objects[10]+0x10)=(void*)(uintptr_t)0x1000;*(void**)(objects[10]+0x130)=objects[7];
    *(void**)(objects[7]+0xa0)=objects[10];*(void**)(objects[7]+0xc0)=objects[29];
    weak_assign((struct cinema_ui_weak*)(objects[10]+0x160),objects[29]);
    *(void**)(objects[29]+0x20)=*(void**)(objects[29]+0xa0)=objects[13];
    *(void**)(objects[29]+0x1f8)=objects[10];*(void**)(objects[29]+0xc0)=objects[14];
    *(void**)(objects[13]+0x130)=objects[14];*(void**)(objects[14]+0xa0)=objects[13];
    // Actual p403 eye and original Merc case mesh bounds/local offsets. Exercise an opened-door pose.
    const float origins[7][3]={{0,0,0},{23.993147f,.12184906f,-.13763428f},{-4.610346f,.03901672f,-.12802124f},
        {.22019958f,42.471111f,.00024414f},{.10132670f,-42.471310f,-.00022507f},
        {-1.0079956f,30.837496f,0},{-1.1388311f,-30.799873f,.01393509f}};
    const float extents[7][3]={{50,50,0},{.68556976f,94.458855f,51.294373f},{8.772614f,114.82268f,52.989166f},
        {3.945572f,46.297977f,51.337646f},{3.945572f,46.297985f,51.337646f},
        {3.406403f,43.023586f,51.337646f},{3.406403f,43.023575f,51.337646f}};
    struct reload_tf frame={{0,0,0,1},{214,0,15150},{1,1,1},{0,0}};memcpy(objects[14]+0x1c0,&frame,sizeof frame);
    for(unsigned i=0;i<7;i++) {
        unsigned part=case_parts[i];*(void**)(objects[part]+0xa0)=objects[13];*(void**)(objects[part]+0xc0)=objects[14];
        *(const void**)(objects[part]+0x440)=(const void*)(uintptr_t)(i+1);
        memcpy(case_origins[part],origins[i],12);memcpy(case_extents[part],extents[i],12);
        struct reload_tf mesh=frame;
        if(i==0){mesh.p[0]-=2.3466511f;mesh.q=(qf_t){0,-.7071067811865475f,0,.7071067811865475f};mesh.s[1]=1.860711f;}
        else if(i<3){mesh.p[0]-=4.9888544f;mesh.p[1]=.000061f;}
        else{
            int left=i==3||i==5;float yaw=(left?-160:160)*.0174532925199433f;
            mesh.q=(qf_t){0,0,sinf(yaw*.5f),cosf(yaw*.5f)};mesh.p[0]-=13.717896f;mesh.p[1]=left?-97.869156f:97.869156f;
            float local[3]={i<5?.644428f:1.30278f,i<5?(left?1.507194f:-1.801804f):(left?23.5f:-23.5f),0},world[3];
            q_rot(mesh.q,local,world);for(unsigned k=0;k<3;k++)mesh.p[k]+=world[k];
        }
        memcpy(objects[part]+0x1c0,&mesh,sizeof mesh);
    }
    static void* links[7][2];
    links[0][0]=objects[20];links[0][1]=objects[21];
    struct{void**data;int count,max;} list={links[0],2,2};memcpy(objects[14]+0xd0,&list,sizeof list);
    for(unsigned i=0;i<2;i++) {
        unsigned actor=22+i*3,actor_root=actor+1,widget=actor+2,child=20+i;
        *(void**)(objects[actor]+0x130)=objects[actor_root];*(void**)(objects[actor]+0xe0)=objects[13];
        *(void**)(objects[actor_root]+0xa0)=objects[actor];*(void**)(objects[actor_root]+0xc0)=objects[child];
        *(void**)(objects[child]+0x20)=*(void**)(objects[child]+0xa0)=objects[13];*(void**)(objects[child]+0xc0)=objects[14];
        *(void**)(objects[child]+0x1f8)=objects[actor];
        *(void**)(objects[widget]+0xa0)=objects[actor];*(void**)(objects[widget]+0xc0)=objects[actor_root];
        links[1+i*2][0]=objects[actor_root];list=(typeof(list)){links[1+i*2],1,1};memcpy(objects[child]+0xd0,&list,sizeof list);
        links[2+i*2][0]=objects[widget];list=(typeof(list)){links[2+i*2],1,1};memcpy(objects[actor_root]+0xd0,&list,sizeof list);
        struct reload_tf panel={{0,0,1,0},{170,i?145:-145,15140},{.1f,.1f,.1f},{0,0}};
        memcpy(objects[widget]+0x1c0,&panel,sizeof panel);*(unsigned short*)(objects[widget]+0x14c)=0x20;
        const int size[2]={1000,1300};memcpy(objects[widget]+0x450,size,sizeof size);
        *(float*)(objects[widget]+0x45c)=*(float*)(objects[widget]+0x460)=.5f;
    }
    // A clipped scrolling child must not enlarge its authored panel canvas.
    links[6][0]=objects[28];list=(typeof(list)){links[6],1,1};memcpy(objects[24]+0xd0,&list,sizeof list);
    *(void**)(objects[28]+0xa0)=objects[22];*(void**)(objects[28]+0xc0)=objects[24];
    struct reload_tf offscreen={{0,0,1,0},{10,1000000,0},{1,1,1},{0,0}};memcpy(objects[28]+0x1c0,&offscreen,sizeof offscreen);
    *(unsigned short*)(objects[28]+0x14c)=0x20;*(float*)(objects[28]+0x45c)=*(float*)(objects[28]+0x460)=.5f;
    const int size[2]={1000,1300};memcpy(objects[28]+0x450,size,sizeof size);
}
static void case_view(unsigned char view[0x100]){
    memset(view,0,0x100);float eye[3]={-14,0,15171.5f},authored=90;
    memcpy(view,eye,12);memcpy(view+0x18,&authored,4);
}
static void case_decorative_branch(void){
    static void* roots[3],*grid[1],*branches[2],*leaves[2][200];
    roots[0]=objects[20];roots[1]=objects[21];roots[2]=objects[29];grid[0]=objects[7];
    struct{void**data;int count,max;} list={roots,3,3};memcpy(objects[14]+0xd0,&list,sizeof list);
    list=(typeof(list)){grid,1,1};memcpy(objects[29]+0xd0,&list,sizeof list);
    branches[0]=objects[30];branches[1]=objects[31];
    list=(typeof(list)){branches,2,2};memcpy(objects[7]+0xd0,&list,sizeof list);
    for(unsigned i=30;i<OBJECT_COUNT;i++){
        *(void**)(objects[i]+0xa0)=objects[10];
        *(void**)(objects[i]+0xc0)=objects[i<32?7:i<232?30:31];
    }
    for(unsigned i=0;i<268;i++)leaves[i<200?0:1][i<200?i:i-200]=objects[32+i];
    list=(typeof(list)){leaves[0],200,200};memcpy(objects[30]+0xd0,&list,sizeof list);
    list=(typeof(list)){leaves[1],68,68};memcpy(objects[31]+0xd0,&list,sizeof list);
}
static void test_captured_case_meshes(void){
    // Source p406_merc_live_bounds.json SHA256: 97c5bca0d4185364f8a0ddee5e95217ef749baa3dd17e8d842ea1f06b75dffc3.
    case_ready();
    static const float captured[7][12]={
        {0.0f,-0.7071067690849304f,0.0f,0.7071067094802856f,211.65335083007812f,0.0f,15150.0f,1.0f,1.8607109785079956f,1.0f,0.0f,0.0f},
        {0.0f,0.0f,0.0f,1.0f,209.01113891601562f,6.0999998822808266e-05f,15150.0f,1.0f,1.0f,1.0f,0.0f,0.0f},
        {0.0f,0.0f,0.0f,1.0f,209.01113891601562f,6.0999998822808266e-05f,15150.0f,1.0f,1.0f,1.0f,0.0f,0.0f},
        {0.0f,0.0f,0.9170600771903992f,0.39874911308288574f,198.7403106689453f,-98.4257583618164f,15150.0f,1.0f,1.0f,1.0f,0.0f,0.0f},
        {0.0f,0.0f,-0.9170600771903992f,0.39874911308288574f,198.5248565673828f,98.62667846679688f,15150.0f,1.0f,1.0f,1.0f,0.0f,0.0f},
        {0.0f,0.0f,0.9170600771903992f,0.39874911308288574f,194.4312744140625f,-103.32514953613281f,15150.0f,1.0f,1.0f,1.0f,0.0f,0.0f},
        {0.0f,0.0f,-0.9170600771903992f,0.39874911308288574f,194.4312744140625f,103.32514953613281f,15150.0f,1.0f,1.0f,1.0f,0.0f,0.0f},
    };
    for(unsigned i=0;i<7;i++)memcpy(objects[case_parts[i]]+0x1c0,captured[i],sizeof captured[i]);
    // Captured panel source SHA256: c0107c32e51f43cc1741d556cd4500bb680c377dd87f884099bc69b9a9ba9b8c.
    {const float tf[12]={0.0f,0.0f,0.9170600771903992f,0.39874911308288574f,167.0940399169922f,-122.6954345703125f,15150.0f,0.10000000149011612f,0.10000000149011612f,0.10000000149011612f,0.0f,0.0f};
        const int size[2]={682,980};const float pivot[2]={0.5f,0.5f};
        memcpy(objects[24]+0x1c0,tf,sizeof tf);memcpy(objects[24]+0x450,size,sizeof size);memcpy(objects[24]+0x45c,pivot,sizeof pivot);
        *(unsigned short*)(objects[24]+0x14c)=0x4063;objects[24][0x429]=0;objects[24][0x518]=0;objects[22][0x58]=128;}
    {const float tf[12]={0.0f,0.0f,-0.9170600771903992f,0.39874911308288574f,166.76931762695312f,122.928466796875f,15124.3583984375f,0.10000000149011612f,0.10000000149011612f,0.10000000149011612f,0.0f,0.0f};
        const int size[2]={682,400};const float pivot[2]={0.5f,0.5f};
        memcpy(objects[27]+0x1c0,tf,sizeof tf);memcpy(objects[27]+0x450,size,sizeof size);memcpy(objects[27]+0x45c,pivot,sizeof pivot);
        *(unsigned short*)(objects[27]+0x14c)=0x4063;objects[27][0x429]=0;objects[27][0x518]=0;objects[25][0x58]=128;}
    // p407's real walk reached256 because physical inventory/character actors contributed344 components.
    case_decorative_branch();
    unsigned char before[sizeof objects],view[0x100];memcpy(before,objects,sizeof before);
    for(unsigned frame=0;frame<10;frame++){
        case_view(view);fv_menu_camera_view(objects[12],view);float fov;memcpy(&fov,view+0x18,4);
        assert(fabsf(fov-104.8379586f)<.001f&&fov<130);
        assert(case_panel_queries[0]==frame+1&&case_panel_queries[1]==frame+1);
        float tangent=tanf(fov*.00872664625997165f);
        for(unsigned i=0;i<7;i++)for(unsigned corner=0;corner<8;corner++){
            unsigned part=case_parts[i];struct reload_tf tf;memcpy(&tf,objects[part]+0x1c0,sizeof tf);
            float local[3],at[3];for(unsigned k=0;k<3;k++)local[k]=(case_origins[part][k]+(corner&(1u<<k)?case_extents[part][k]:-case_extents[part][k]))*tf.s[k];
            q_rot(tf.q,local,at);at[0]+=tf.p[0]+14;at[1]+=tf.p[1];at[2]+=tf.p[2]-15171.5f;
            assert(at[0]>144&&fabsf(at[1])/at[0]<tangent*.861f&&fabsf(at[2])/at[0]<tangent/(2400.0f/1080)*.861f);
        }
        for(unsigned i=0;i<2;i++)for(unsigned corner=0;corner<4;corner++){
            unsigned part=i?27:24;struct reload_tf tf;memcpy(&tf,objects[part]+0x1c0,sizeof tf);
            int size[2];memcpy(size,objects[part]+0x450,sizeof size);
            float local[3]={0,(corner&1?.5f:-.5f)*size[0]*tf.s[1],(corner&2?.5f:-.5f)*size[1]*tf.s[2]},at[3];
            q_rot(tf.q,local,at);at[0]+=tf.p[0]+14;at[1]+=tf.p[1];at[2]+=tf.p[2]-15171.5f;
            assert(at[0]>1&&fabsf(at[1])/at[0]<tangent*.861f&&fabsf(at[2])/at[0]<tangent/(2400.0f/1080)*.861f);
        }
        current_ms+=16;fv_lobby_zoom_ms=current_ms;
    }
    assert(bounds_calls==7&&!memcmp(before,objects,sizeof before));
    // The capacity still declines a genuinely oversized case, while unknown actor-owned branches are pruned.
    struct fv_case_view walk={.eye={-14,0,15171.5f},.forward={1,0,0},.right={0,1,0},.up={0,0,1},.aspect=2400.0f/1080};
    *(void**)(objects[7]+0xa0)=0;memcpy(before,objects,sizeof before);
    assert(fv_console_case_widgets(objects[14],&walk)&&!memcmp(before,objects,sizeof before));
    *(void**)(objects[7]+0xa0)=objects[13];
    for(unsigned i=30;i<OBJECT_COUNT;i++)*(void**)(objects[i]+0xa0)=objects[13];
    memcpy(before,objects,sizeof before);
    assert(!fv_console_case_widgets(objects[14],&walk)&&!memcmp(before,objects,sizeof before));
    puts("PASS: actual p406 captured7 mesh transforms/bounds and both captured panel TF/drawsize/pivot/visibility fit all64 corners at104.838degrees below130; native weak ChildActor parent resolution,10stableframes, no native object writes");
    puts("PASS: actual bounded case traversal prunes270 live physical inventory descendants, preserves both native UIScreen canvases and all native bytes; unknown actor owner pruned, genuinely oversized case still declines at unchanged256 capacity");
}
static void test_case_camera(void){
    case_ready();unsigned char before[sizeof objects],view[0x100],view_before[0x100];memcpy(before,objects,sizeof before);
    float stable=0;unsigned old_clipped=0;
    for(unsigned frame=0;frame<40;frame++){
        case_view(view);memcpy(view_before,view,sizeof view);fv_menu_camera_view(objects[12],view);
        float fov;memcpy(&fov,view+0x18,4);if(!frame)stable=fov;
        assert(fov>80&&fov<130&&fabsf(fov-stable)<.0001f&&g_fov_h_deg==fov);
        memcpy(view+0x18,view_before+0x18,4);assert(!memcmp(view,view_before,sizeof view));
        float tangent=tanf(fov*.00872664625997165f),old_tangent=tanf(fv_console_fov(90)*.00872664625997165f);
        for(unsigned i=0;i<7;i++)for(unsigned corner=0;corner<8;corner++){
            unsigned part=case_parts[i];struct reload_tf mesh;memcpy(&mesh,objects[part]+0x1c0,sizeof mesh);
            float local[3],world[3];for(unsigned k=0;k<3;k++)local[k]=(case_origins[part][k]+(corner&(1u<<k)?case_extents[part][k]:-case_extents[part][k]))*mesh.s[k];
            q_rot(mesh.q,local,world);world[0]+=mesh.p[0]+14;world[1]+=mesh.p[1];world[2]+=mesh.p[2]-15171.5f;
            assert(world[0]>1&&fabsf(world[1])/world[0]<tangent*.861f&&fabsf(world[2])/world[0]<tangent/(2400.0f/1080)*.861f);
            if(fabsf(world[1])/world[0]>old_tangent||fabsf(world[2])/world[0]>old_tangent/(2400.0f/1080))old_clipped++;
        }
        for(unsigned panel=0;panel<2;panel++)for(unsigned corner=0;corner<4;corner++){
            float depth=184,y=(panel?145:-145)+(corner&1?50:-50),z=15140-15171.5f+(corner&2?65:-65);
            assert(fabsf(y)/depth<tangent*.861f&&fabsf(z)/depth<tangent/(2400.0f/1080)*.861f);
        }
        // Arbitrary row/item/hover changes cannot affect the fixed case fit.
        objects[6][0x200]=(unsigned char)frame;current_ms+=16;fv_lobby_zoom_ms=current_ms;
    }
    objects[6][0x200]=before[6*0x600+0x200];assert(old_clipped&&bounds_calls==7);
    assert(!memcmp(before,objects,sizeof before)&&!world_calls&&!restores&&!refreshes);
    for(unsigned refusal=0;refusal<11;refusal++){
        case_ready();case_view(view);float fov=67.380135f;
        if(refusal==0)*(void**)(objects[8]+0xe0)=objects[8];
        if(refusal==1)*(void**)(objects[14]+0xa0)=objects[8];
        if(refusal==2)*(void**)(objects[18]+0xa0)=objects[8];
        if(refusal==3)*(void**)(objects[18]+0xc0)=objects[8];
        if(refusal==4)*(float*)(objects[18]+0x1c0)=NAN;
        if(refusal==5)*(float*)view=NAN;
        if(refusal==6)*(float*)(view+0xc)=NAN;
        if(refusal==7)view[0x31]=1;
        if(refusal==8)g_comp_h=0;
        if(refusal==9)*(void**)(objects[18]+0x440)=0;
        if(refusal==10)bounds_ok=0;
        assert(!fv_console_case_fov(objects[8],view,&fov)&&fov==67.380135f);g_comp_h=1080;
    }
    for(unsigned ordinary=2;ordinary<=3;ordinary++){
        case_ready();case_mode=(int)ordinary;memcpy(before,objects,sizeof before);
        for(unsigned frame=0;frame<20;frame++){
            case_view(view);memcpy(view_before,view,sizeof view);fv_menu_camera_view(objects[12],view);
            float fov;memcpy(&fov,view+0x18,4);assert(fabsf(fov-67.380135f)<.0001f);
            memcpy(view+0x18,view_before+0x18,4);assert(!memcmp(view,view_before,sizeof view));
            current_ms+=16;fv_lobby_zoom_ms=current_ms;
        }
        assert(!bounds_calls&&!memcmp(before,objects,sizeof before));
    }
    for(unsigned refusal=0;refusal<12;refusal++){
        case_ready();
        if(refusal==0)serials[29]++;
        if(refusal==1)alive[29]=0;
        if(refusal==2)parent_component_class_ok=0;
        if(refusal==3)*(void**)(objects[29]+0x1f8)=objects[8];
        if(refusal==4)*(void**)(objects[29]+0x20)=objects[8];
        if(refusal==5)*(void**)(objects[29]+0xa0)=objects[8];
        if(refusal==6)*(void**)(objects[7]+0xc0)=objects[14];
        if(refusal==7)*(void**)(objects[14]+0xa0)=objects[8];
        if(refusal==8)*(void**)(objects[29]+0xc0)=objects[8];
        if(refusal==9)*(int*)(parent_property+0x4c)=0x158;
        if(refusal==10)*(int*)(parent_property+0x34)=2;
        if(refusal==11)*(int*)(parent_property+0x38)=4;
        assert(!fv_console_case_owner(objects[8]));
    }
    case_ready();assert(!*(void**)(objects[10]+0xe0)&&fv_console_case_owner(objects[8])==objects[13]);
    *(void**)(objects[10]+0xe0)=objects[13];assert(fv_console_case_owner(objects[8])==objects[13]);
    case_ready();current_screen=objects[22];*(void**)(objects[22]+0xe0)=0;
    *(void**)(objects[22]+0x10)=(void*)(uintptr_t)0x1001;
    weak_assign((struct cinema_ui_weak*)(objects[22]+0x160),objects[20]);
    assert(fv_console_case_owner(current_screen)==objects[13]);
    case_view(view);fv_menu_camera_view(objects[12],view);float child_fov;memcpy(&child_fov,view+0x18,4);assert(fabsf(child_fov-stable)<.0001f);
    printf("PASS: actual final-view case FOV %.4f fits56 native-box corners and2 detached panel canvases at p403 eye for40 stable frames; old67.38 clips; native Owner plus weak ParentComponent bridge, cached7 assets, clipped-scroll/item/hover independence, all roots/view bytes unchanged and11 invalid-binding/geometry/view refusals\n",stable);
    puts("PASS: exact native Merc CreateGame/layout capability only; ordinary NewGame/Options retain67.38 for20 stable frames each despite identical wide decorative wings and panel topology; no bounds queries or native/view writes");
    puts("PASS: actual p406 null Grid.Owner/ParentComponent weak bridge and direct ChildActor screen both reach CreateGame;12 stale/invalid weak/class/ChildActor backlink/Outer/owner/root/attachment/ABI refusals; ordinary Actor.Owner still works");
}
static void test_lobby_heading(void){
    ready();current_ms=10000;g_menu_view_active=0;g_look_pitch=.41f;g_yaw_turned=.37f;
    const long stamps[]={0,10001,9500,9000,10000,9751,9501};
    for(int type=0;type<9;type++)for(int heading=-8;heading<=8;heading++)for(unsigned s=0;s<sizeof stamps/sizeof *stamps;s++){
        g_touch_context=type;g_look_yaw=heading*.39f;g_lobby_menu_ms=stamps[s];
        int lobby=stamps[s]>0&&current_ms>=stamps[s]&&current_ms-stamps[s]<500;
        float expected=type==4||((type==2||type==7)&&lobby)?0:g_look_yaw+g_yaw_turned;
        float yaw,pitch;head_angles_now(&yaw,&pitch);
        assert(fabsf(yaw-expected)<.00001f&&pitch==mobile_view_pitch(type,g_look_pitch));
        assert(g_look_yaw==heading*.39f&&g_yaw_turned==.37f&&g_look_pitch==.41f);
    }
    puts("PASS: actual head_angles_now1071 context/heading/stamp cases; native Lobby2/7 yaw centred only with positive nonfuture<500ms capability, missing/expired/future and in-world contexts preserve gameplay yaw/quick-turn/pitch; type4 correction retained without gameplay-state writes");
}
int main(void){
    ready();assert(fv_console_tick(objects[0],&camera)&&fv_console.used&&world_calls==1&&refreshes==1);
    struct reload_tf button;memcpy(&button,objects[6]+0x1c0,sizeof button);float up[3],axis[3]={0,0,1};q_rot(button.q,axis,up);assert(up[2]>.999f);
    struct reload_tf fitted;memcpy(&fitted,objects[2]+0x1c0,sizeof fitted);assert(fabsf(fitted.p[0]-100)<.001f);
    for(int i=0;i<20;i++){
        int ok=fv_console_tick(objects[0],&camera);
        if(!ok){struct reload_tf r,m;memcpy(&r,objects[2]+0x1c0,sizeof r);memcpy(&m,objects[3]+0x1c0,sizeof m);
            printf("console repeat=%d q=%g,%g,%g,%g p=%g,%g,%g s=%g,%g,%g meshQ=%g,%g,%g,%g meshS=%g,%g,%g used=%d\n",i,r.q.x,r.q.y,r.q.z,r.q.w,r.p[0],r.p[1],r.p[2],r.s[0],r.s[1],r.s[2],m.q.x,m.q.y,m.q.z,m.q.w,m.s[0],m.s[1],m.s[2],fv_console.used);fflush(stdout);}
        assert(ok);
    }
    struct reload_tf repeated;memcpy(&repeated,objects[2]+0x1c0,sizeof repeated);
    for(unsigned i=0;i<3;i++)assert(fabsf(fitted.p[i]-repeated.p[i])<.001f&&fabsf(fitted.s[i]-repeated.s[i])<.001f);
    assert(fv_console_tick(objects[7],&camera));fv_console_tick(0,&camera);assert(restores==1&&!fv_console.used&&!memcmp(objects[2]+0x1c0,&original,sizeof original));
    ready();bounds_ok=0;assert(!fv_console_tick(objects[0],&camera)&&!world_calls);
    ready();*(void**)(objects[3]+0xa0)=objects[6];assert(!fv_console_tick(objects[0],&camera)&&!world_calls);
    ready();*(void**)(objects[3]+0xc0)=objects[7];assert(!fv_console_tick(objects[0],&camera)&&!world_calls);
    ready();alive[1]=0;assert(!fv_console_tick(objects[0],&camera)&&!world_calls);
    ready();assert(fv_console_tick(objects[0],&camera));serials[2]++;fv_console_restore();assert(!restores);
    ready();assert(fv_console_tick(objects[0],&camera));*(void**)(objects[2]+0xc0)=objects[6];fv_console_restore();assert(!restores);
    test_camera_fov();
    test_merchant_fov();
    test_merchant_detail();
    test_death_camera();
    test_results_camera(0);test_results_camera(1);test_results_camera(2);
    test_curved_results_camera();
    test_live_merc_camera();
    test_case_camera();test_captured_case_meshes();
    test_lobby_heading();
    puts("PASS: actual fixed backing console/cover fitting and20 stable frames, scroll refresh, original relative restoration; native owner/ancestry/weak serial/parent/bounds guards; fixed-console camera FOV accepts native2/4/7 with active camera, GT and positive nonfuture<300ms menu state; merchant8 uses its separately validated fixed vertical view; all other view bytes preserved");
    return 0;
}
