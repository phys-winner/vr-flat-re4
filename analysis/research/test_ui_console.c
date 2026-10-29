#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
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
static unsigned char objects[20][0x600];static int alive[20],serials[20],world_calls,restores,refreshes,bounds_ok;
static unsigned g_comp_w=2400,g_comp_h=1080;static float g_fov_h_deg=90;
static int index_of(const void* p){for(int i=0;i<20;i++)if(p==objects[i])return i;return -1;}
static int pv_alive(const void* p){int i=index_of(p);return i>=0&&alive[i];}
static void weak_assign(struct cinema_ui_weak* ref,const void* object){ref->index=index_of(object);ref->serial=ref->index<0?0:serials[ref->index];}
static void* weak_get(const struct cinema_ui_weak* ref){return ref->index>=0&&ref->index<20&&alive[ref->index]&&serials[ref->index]==ref->serial?objects[ref->index]:0;}
static void(*cu_weak_assign)(struct cinema_ui_weak*,const void*)=weak_assign;
static void*(*cu_weak_get)(const struct cinema_ui_weak*)=weak_get;
static void* scene_class(void){return(void*)1;}static void* static_class(void){return(void*)2;}static void* button_class(void){return(void*)3;}
static void* camera_class(void){return(void*)4;}static void* pawn_class(void){return(void*)5;}
static void*(*mv_scene_class)(void)=scene_class,*(*mv_static_class)(void)=static_class,*(*fv_button_class)(void)=button_class;
static void*(*cinema_camera_class)(void)=camera_class,*(*mv_pawn_class)(void)=pawn_class;
static int death_class_ok=1,death_property_ok=1,death_button_ok=1;
static int menu_visual_layout(const void* p,const void* cls,int size){(void)size;return pv_alive(p)&&(cls==(void*)1
    ||(cls==(void*)2&&(p==objects[3]||p==objects[10]))||(cls==(void*)3&&(p==objects[6]||p==objects[15]||p==objects[18]))
    ||(cls==(void*)4&&p==objects[12])||(cls==(void*)5&&p==objects[11]));}
static int cinema_object(const void* p,const void* cls,int size){return menu_visual_layout(p,cls,size);}
static int cinema_ui_named(const void* p,const char* name,int size){
    (void)size;return pv_alive(p)&&((p==objects[0]&&!strcmp(name,"OptionsUIScreen_BP_C"))
        ||(p==objects[1]&&!strcmp(name,"UI_PauseMenu_Content_GameOptions_BP_C"))
        ||(p==objects[7]&&!strcmp(name,"OptionsCoverScreen_BP_C"))
        ||(p==objects[8]&&death_class_ok&&!strcmp(name,"DeathMenu_BP_C"))
        ||((p==objects[13]||p==objects[16])&&death_button_ok&&!strcmp(name,"Button_Death_C")));
}
static void* cinema_ui_property(void* object,const char* cls,const char* field,int offset){
    assert(!strcmp(cls,"DeathMenu_BP_C")&&!strcmp(field,"Screen")&&offset==0x318);
    return death_property_ok&&cinema_ui_named(object,cls,0x498)?*(void**)((char*)object+offset):0;
}
static void* fv_object_property(const void* object,const char* name){
    if(object==objects[7]&&!strcmp(name,"OptionsUIScreenBP"))return objects[0];
    if(object==objects[0]&&!strcmp(name,"TrifoldBaseBP"))return objects[1];
    if(object==objects[1]&&!strcmp(name,"UIScreen"))return objects[0];
    if((object==objects[13]||object==objects[16])&&!strcmp(name,"ParentMenu"))return *(void*const*)((const char*)object+0x290);
    if(object==objects[1]&&!strcmp(name,"Background_Plane"))return objects[3];return 0;
}
static int fv_frame_owned(void* root,void* owner){return pv_alive(root)&&pv_alive(owner)&&*(void*const*)((char*)root+0xa0)==owner&&*(void*const*)((char*)owner+0x130)==root;}
static int fv_descends_from(void* part,const void* root){return pv_alive(part)&&*(void*const*)((char*)part+0xc0)==root;}
static int cinema_movie_screen_bounds(void* component,float origin[3],float extent[3]){
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
static int enabled(const void* object){return object!=0;}
static void* active(void){assert(on_game_thread);return current_pawn;}
static unsigned type(void){assert(on_game_thread);return current_type;}
static void*(*fv_ui_manager)(void)=manager;
static void* peek(void* object){assert(object==objects[0]);return current_screen;}
static void*(*fv_peek_screen)(void*)=peek;
static int(*fv_ui_enabled)(const void*)=enabled;
static void*(*phone_active)(void)=active;
static unsigned(*phone_type)(void)=type;
static int cinema_on_game_thread(void){return on_game_thread;}
static long ptr_now_ms(void){return current_ms;}
static int g_touch_context,g_menu_view_active;
static float g_look_yaw,g_look_pitch,g_yaw_turned,g_menu_yaw_off,g_menu_pitch;
static long g_lobby_menu_ms,g_menu_view_ms;
static float quick_turn_offset(void){return 0;}
static float mobile_view_pitch(int context,float pitch){return context==2||context==4||context==5||context==7?0:pitch;}
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
    memset(objects,0,sizeof objects);memset(&fv_console,0,sizeof fv_console);
    memset(&fv_camera_menu,0,sizeof fv_camera_menu);death_class_ok=death_property_ok=death_button_ok=1;
    g_menu_view_active=0;g_menu_yaw_off=g_menu_pitch=0;g_menu_view_ms=g_lobby_menu_ms=0;fv_lobby_zoom_ms=0;
    for(int i=0;i<20;i++)alive[i]=serials[i]=1;
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
    ready();*(void**)(objects[3]+0xc0)=objects[6];assert(!fv_console_tick(objects[0],&camera)&&!world_calls);
    ready();alive[1]=0;assert(!fv_console_tick(objects[0],&camera)&&!world_calls);
    ready();assert(fv_console_tick(objects[0],&camera));serials[2]++;fv_console_restore();assert(!restores);
    ready();assert(fv_console_tick(objects[0],&camera));*(void**)(objects[2]+0xc0)=objects[6];fv_console_restore();assert(!restores);
    test_camera_fov();
    test_death_camera();
    puts("PASS: actual fixed backing console/cover fitting and20 stable frames, scroll refresh, original relative restoration; native owner/ancestry/weak serial/parent/bounds guards; actual camera FOV callback accepts only native2/4/7, active camera, GT and positive nonfuture<300ms menu state, preserves all other view bytes");
    return 0;
}
