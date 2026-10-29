#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef struct {float x,y,z,w;} qf_t;
struct __attribute__((aligned(16))) reload_tf {qf_t q;float p[3],s[3],padding[2];};
static void q_rot(qf_t q,const float v[3],float o[3]) {   // production rotation (re4fix.c)
    float d=q.x*v[0]+q.y*v[1]+q.z*v[2],uu=q.x*q.x+q.y*q.y+q.z*q.z,k=q.w*q.w-uu;
    float cx=q.y*v[2]-q.z*v[1],cy=q.z*v[0]-q.x*v[2],cz=q.x*v[1]-q.y*v[0];
    o[0]=2*d*q.x+k*v[0]+2*q.w*cx;o[1]=2*d*q.y+k*v[1]+2*q.w*cy;o[2]=2*d*q.z+k*v[2]+2*q.w*cz;
}
static int reload_tf_valid(const struct reload_tf* t) {
    float norm=t->q.x*t->q.x+t->q.y*t->q.y+t->q.z*t->q.z+t->q.w*t->q.w;
    if (!isfinite(norm) || fabsf(norm-1)>0.01f)return 0;
    for(int i=0;i<3;i++)if(!isfinite(t->p[i]) || !isfinite(t->s[i]) || fabsf(t->s[i]-1)>0.01f)return 0;
    return 1;
}
static volatile int g_touch_gt_tid;
static unsigned g_comp_w,g_comp_h;
static long current_tid;
static int phone_inspect_ready;
static void* phone_actor_event_address;
static void phone_inspect_fname(const void* name,char* out,int size) {
    static const char* names[]={"","BoolProperty","UseFlatCutsceneScreen","VR4GameInstance_BP_C","OtherClass","IntProperty",
        "ObjectProperty","TheatreBox","VR4CutscenePlayerPawn_BP_C","CutsceneTheatreBox_C","VideoToPlay",
        "Opening_android","FileMediaSource","VideoCompleted","Function","VideoComplete","r333_H265_6MB","Screen","FlatScreen"};
    unsigned id=0;if(name)memcpy(&id,name,4);
    const char* value=id<sizeof names/sizeof names[0] ? names[id] : "";
    assert(size>(int)strlen(value));memcpy(out,value,strlen(value)+1);
}
#define SYS_gettid 1
static long syscall(long number) {assert(number==SYS_gettid);return current_tid;}
static int patch_code(void* target,void* hook) {(void)target;(void)hook;return 0;}
static unsigned update_original,view_original,subtitle_original;
static void native_update(void* pawn,const struct reload_tf* transform,float fov_y,int screen_on) {
    assert(pawn && transform && isfinite(fov_y));(void)screen_on;update_original++;
}
static void native_view(void* camera,float dt,void* view) {
    assert(camera && dt==0.02f && view);view_original++;memset(view,0x5a,0x5e0);
}
static void native_subtitle(void* manager,const void* key,float seconds) {
    assert(manager && key);(void)seconds;subtitle_original++;
}
#define LOGF(...) ((void)0)
#define RE4FIX_CINEMA_UPDATE_GATE native_update
#define RE4FIX_CINEMA_VIEW_GATE native_view
#define RE4FIX_CINEMA_SUBTITLE_GATE native_subtitle
#include "../re4fix_cinema.h"
static struct {
    unsigned char pawn[0x1200],camera[0x800],other[0x1200],cls[3][0x100];
    void* chain[3][1];
} f __attribute__((aligned(16)));
static void* active;
static unsigned type;
static void* get_active(void) {return active;}
static unsigned get_type(void) {return type;}
static void* pawn_class(void) {return f.cls[0];}
static void* camera_class(void) {return f.cls[1];}
static unsigned rotation_calls;
static struct cinema_rotator get_rotation(const qf_t* q) {
    assert(q && q->w==1);rotation_calls++;
    return (struct cinema_rotator){-12,62,3};
}
static struct reload_tf scene={ {0,0,0,1},{101,202,303},{1,1,1},{0,0} };
static unsigned char view[0x5e0] __attribute__((aligned(16)));
static void ready(void) {
    memset(&f,0,sizeof f);memset(&cinema_scene,0,sizeof cinema_scene);
    memset(&cinema_last,0,sizeof cinema_last);
    for(int i=0;i<3;i++) {
        f.chain[i][0]=f.cls[i]+0x30;
        *(void**)(f.cls[i]+0x30)=f.chain[i];
        *(int*)(f.cls[i]+0x58)=i==1 ? 0x800 : 0x1200;
    }
    *(void**)(f.pawn+0x10)=f.cls[0];*(void**)(f.camera+0x10)=f.cls[1];
    *(void**)(f.pawn+0x2d8)=f.camera;*(void**)(f.camera+0xa0)=f.pawn;
    *(unsigned long long*)(f.pawn+0x18)=0x123456789abcdef0ULL;
    active=f.pawn;type=3;current_tid=g_touch_gt_tid=101;
    g_comp_w=g_comp_h=0;
    cinema_active=get_active;cinema_type=get_type;cinema_pawn_class=pawn_class;
    cinema_camera_class=camera_class;cinema_rotator=get_rotation;
    update_original=view_original=rotation_calls=cinema_scene_logs=cinema_view_logs=0;
    cinema_entry_logs=cinema_reject_mask=0;
    cinema_subtitle_logs=subtitle_original=0;
    memset(view,0x5a,sizeof view);
}
static void capture(void) {
    hook_cinema_update(f.pawn,&scene,50,1);
    assert(update_original==1 && cinema_scene.valid && cinema_scene.fov_y==50);
}
static void declined(void) {
    unsigned char before[sizeof view];memcpy(before,view,sizeof before);
    assert(!cinema_flat_view(f.camera,view));assert(!memcmp(before,view,sizeof before));
}
static void check_view(void) {
    unsigned char expected[sizeof view];memset(expected,0x5a,sizeof expected);
    float rot[]={-12,62,3};float aspect=4.0f/3.0f;
    float fov=2*atanf(tanf(50*0.5f*(float)acos(-1.0)/180)*aspect)*180/(float)acos(-1.0);
    memcpy(expected,scene.p,12);memcpy(expected+12,rot,12);
    memcpy(expected+24,&fov,4);memcpy(expected+44,&aspect,4);
    expected[48]|=1;expected[49]=0;
    assert(!memcmp(view,expected,24));
    assert(fabsf(*(float*)(view+24)-fov)<0.00001f);
    memcpy(view+24,expected+24,4);assert(!memcmp(view,expected,sizeof view));
}
static unsigned char game_instance[0x400],instance_class[0x100],instance_field[0x60],instance_kind[0x10],movie_video[0x100];
static int movie_video_alive=1;
static unsigned game_instance_calls,bool_sets;
static void* native_game_instance(const void* context) {assert(context==f.pawn);game_instance_calls++;return game_instance;}
static void native_set_bool(void* object,unsigned long long name,int value) {
    assert(object==game_instance && name==2 && value==1);bool_sets++;game_instance[0x200]|=4;
}
static void weak_assign(struct cinema_ui_weak* ref,const void* object) {
    assert(!object || object==game_instance || object==movie_video);
    ref->index=object==game_instance ? 1 : object==movie_video ? 2 : -1;ref->serial=7;
}
static void* weak_get(const struct cinema_ui_weak* ref) {
    return ref->serial!=7 ? 0 : ref->index==1 ? game_instance : ref->index==2 && movie_video_alive ? movie_video : 0;
}
static struct {
    unsigned char theatre[0x600],theatre_class[0x100],video_class[0x100],function_class[0x100];
    unsigned char fields[5][0x80],kinds[2][0x10],complete[0xc0];void* vtable[70];
} movie;
static int completed;
static void movie_event(void* receiver,const void* fn,void* params) {
    assert(receiver==movie.theatre && fn==movie.complete && !params);completed++;
}
static void movie_probe_ready(void) {
    ready();memset(&movie,0,sizeof movie);memset(movie_video,0,sizeof movie_video);
    memset(&cinema_movie_cache,0,sizeof cinema_movie_cache);
    phone_inspect_ready=movie_video_alive=1;completed=0;phone_actor_event_address=(void*)movie_event;
    cu_weak_assign=weak_assign;cu_weak_get=weak_get;
    *(unsigned*)(f.cls[0]+0x18)=8;*(void**)(f.cls[0]+0x70)=movie.fields[0];
    *(void**)(f.pawn+0x1198)=movie.theatre;*(void**)(movie.theatre+0x10)=movie.theatre_class;
    *(unsigned*)(movie.theatre_class+0x18)=9;*(int*)(movie.theatre_class+0x58)=sizeof movie.theatre;
    *(void**)(movie.theatre_class+0x70)=movie.fields[1];*(void**)(movie.theatre_class+0x48)=movie.complete;
    *(void**)(movie_video+0x10)=movie.video_class;*(unsigned*)(movie_video+0x18)=11;
    *(unsigned*)(movie.video_class+0x18)=12;*(int*)(movie.video_class+0x58)=sizeof movie_video;
    *(unsigned*)movie.kinds[0]=6;*(unsigned*)movie.kinds[1]=1;
    const unsigned names[]={7,10,13,17,18},offsets[]={0x1198,0x478,0x4c5,0x2f8,0x238};
    for(unsigned i=0;i<5;i++) {
        *(void**)(movie.fields[i]+8)=movie.kinds[i==2];*(unsigned*)(movie.fields[i]+0x28)=names[i];
        *(int*)(movie.fields[i]+0x34)=1;*(int*)(movie.fields[i]+0x38)=i==2?1:8;
        *(int*)(movie.fields[i]+0x4c)=(int)offsets[i];
    }
    *(void**)(movie.fields[1]+0x58)=movie.fields[2];*(void**)(movie.theatre+0x478)=movie_video;
    *(void**)(movie.fields[2]+0x58)=movie.fields[3];*(void**)(movie.fields[3]+0x58)=movie.fields[4];
    *(void***)movie.theatre=movie.vtable;movie.vtable[0x218/sizeof(void*)]=(void*)movie_event;
    *(unsigned*)(movie.function_class+0x18)=14;*(void**)(movie.complete+0x10)=movie.function_class;
    *(unsigned*)(movie.complete+0x18)=15;*(void**)(movie.complete+0x20)=movie.theatre_class;
    *(unsigned*)(movie.complete+0xb0)=0x0c000000;*(unsigned short*)(movie.complete+0xb8)=0xffff;
}
static void movie_probe_checks(void) {
    movie_probe_ready();assert(cinema_movie_probe(f.pawn,1)==movie.theatre);
    unsigned char before[sizeof movie];memcpy(before,&movie,sizeof movie);
    assert(cinema_movie_complete(movie.theatre) && completed==1 && !memcmp(before,&movie,sizeof movie));
    *(unsigned*)(movie_video+0x18)=16;assert(!cinema_movie_probe(f.pawn,1) && cinema_movie_probe(f.pawn,0)==movie.theatre);
    movie_probe_ready();movie.theatre[0x4c5]=1;assert(!cinema_movie_probe(f.pawn,0) && !cinema_movie_complete(movie.theatre));
    movie_probe_ready();current_tid++;assert(!cinema_movie_probe(f.pawn,0) && !cinema_movie_complete(movie.theatre));
    movie_probe_ready();type=1;assert(!cinema_movie_probe(f.pawn,0));
    movie_probe_ready();movie_video_alive=0;assert(!cinema_movie_probe(f.pawn,0));
    movie_probe_ready();*(int*)(movie.fields[1]+0x38)=4;assert(!cinema_movie_probe(f.pawn,0));
    movie_probe_ready();++*(int*)(movie.fields[2]+0x4c);assert(!cinema_movie_probe(f.pawn,0));
    movie_probe_ready();*(unsigned*)(movie.video_class+0x18)=4;assert(!cinema_movie_probe(f.pawn,0));
    const unsigned offsets[]={0xb0,0xb4,0xb6,0xb8};
    for(unsigned i=0;i<4;i++){movie_probe_ready();movie.complete[offsets[i]]^=1;assert(!cinema_movie_complete(movie.theatre) && !completed);}
    movie_probe_ready();*(void**)(movie.complete+0x20)=movie.video_class;assert(!cinema_movie_complete(movie.theatre));
    movie_probe_ready();movie.vtable[0x218/sizeof(void*)]=0;assert(!cinema_movie_complete(movie.theatre));
    movie_probe_ready();*(unsigned*)(movie.complete+0x18)=4;*(void**)(movie.complete+0x28)=movie.complete;assert(!cinema_movie_complete(movie.theatre));
    phone_inspect_ready=0;phone_actor_event_address=0;cu_weak_assign=0;cu_weak_get=0;
    puts("PASS: authored FMV VideoComplete dispatch, opening-only identity, immutable native objects; reflected type/owner/size/arity/return/vtable/weak/thread/completion guards");
}
static void close_from_view(void* camera,void* out) {
    assert(camera==f.camera && out==view);type=1;active=f.other;
}
static void terminal_view_checks(void) {
    ready();capture();assert(cinema_flat_view(f.camera,view));
    cinema_capture(f.pawn,&scene,50,0);assert(!cinema_scene.valid && cinema_scene.closing);
    memset(view,0x5a,sizeof view);assert(cinema_flat_view(f.camera,view));check_view();
    *(unsigned long long*)(f.pawn+0x18)^=1;declined();
    ready();capture();assert(cinema_flat_view(f.camera,view));cinema_capture(f.pawn,&scene,50,0);
    type=1;declined();assert(!cinema_last.valid);type=3;declined();
    ready();cinema_capture(f.pawn,&scene,50,0);declined();
    ready();capture();assert(cinema_flat_view(f.camera,view));
    cinema_capture(f.pawn,&scene,NAN,1);cinema_capture(f.pawn,&scene,50,0);
    assert(!cinema_last.valid);declined();
    movie_probe_ready();capture();assert(cinema_flat_view(f.camera,view));
    unsigned char expected[sizeof view];memcpy(expected,view,sizeof expected);
    movie.theatre[0x4c5]=1;cinema_scene.valid=0;memset(view,0x5a,sizeof view);
    assert(cinema_flat_view(f.camera,view) && !memcmp(view,expected,sizeof view));
    phone_inspect_ready=0;phone_actor_event_address=0;cu_weak_assign=0;cu_weak_get=0;
    ready();capture();cinema_view_extra=close_from_view;
    hook_cinema_view(f.camera,0.02f,view);assert(type==1 && active==f.other);check_view();
    cinema_view_extra=0;
    puts("PASS: screen-off/movie-completed terminal frames retain owned flat projection; no-cache/FName/context guards; synchronous skip cannot expose the native theatre frame");
}
static void movie_setting_ready(void) {
    ready();memset(game_instance,0,sizeof game_instance);memset(instance_class,0,sizeof instance_class);
    memset(instance_field,0,sizeof instance_field);memset(instance_kind,0,sizeof instance_kind);
    *(void**)(game_instance+0x10)=instance_class;*(int*)(instance_class+0x58)=sizeof game_instance;
    *(unsigned*)(instance_class+0x18)=3;*(void**)(instance_class+0x70)=instance_field;
    *(unsigned*)(instance_field+0x28)=2;*(void**)(instance_field+8)=instance_kind;*(unsigned*)instance_kind=1;
    *(int*)(instance_field+0x34)=1;*(int*)(instance_field+0x38)=1;*(int*)(instance_field+0x4c)=0x200;
    phone_inspect_ready=1;cinema_flat_instance=(struct cinema_ui_weak){-1,0};
    cinema_game_instance=native_game_instance;cinema_set_bool=native_set_bool;cu_weak_assign=weak_assign;cu_weak_get=weak_get;
    bool_sets=game_instance_calls=0;game_instance[0x200]=0x80;
}
static void movie_setting_checks(void) {
    movie_setting_ready();cinema_flat_movie_setting(f.camera);
    assert(bool_sets==1 && game_instance[0x200]==0x84 && cinema_flat_instance.serial==7);
    cinema_flat_movie_setting(f.camera);assert(bool_sets==1);
    cinema_flat_instance.serial=8;cinema_flat_movie_setting(f.camera);assert(bool_sets==2);
    movie_setting_ready();current_tid++;cinema_flat_movie_setting(f.camera);assert(!bool_sets && !game_instance_calls);
    movie_setting_ready();phone_inspect_ready=0;cinema_flat_movie_setting(f.camera);assert(!bool_sets && !game_instance_calls);
    movie_setting_ready();cinema_flat_movie_setting(f.other);assert(!bool_sets && !game_instance_calls);
    movie_setting_ready();*(unsigned*)(instance_class+0x18)=4;cinema_flat_movie_setting(f.camera);assert(!bool_sets);
    movie_setting_ready();*(unsigned*)instance_kind=5;cinema_flat_movie_setting(f.camera);assert(!bool_sets);
    movie_setting_ready();*(int*)(instance_field+0x34)=2;cinema_flat_movie_setting(f.camera);assert(!bool_sets);
    movie_setting_ready();*(int*)(instance_field+0x38)=4;cinema_flat_movie_setting(f.camera);assert(!bool_sets);
    movie_setting_ready();*(int*)(instance_field+0x4c)=0x400;cinema_flat_movie_setting(f.camera);assert(!bool_sets);
    movie_setting_ready();*(int*)(instance_field+0x4c)=0x80;cinema_flat_movie_setting(f.camera);assert(!bool_sets);
    phone_inspect_ready=0;cinema_game_instance=0;cinema_set_bool=0;cu_weak_assign=0;cu_weak_get=0;
    puts("PASS: actual authored-flat setting uses native reflected BoolProperty setter, exact GameInstance/property/layout/GT/camera guards and one weak-lifetime update");
}
int main(int argc,char** argv) {
    (void)argv;if(argc==999)install_touch_cinema(0);
    _Static_assert(sizeof(struct reload_tf)==48 && offsetof(struct reload_tf,s)==28,"native transform");
    ready();capture();hook_cinema_view(f.camera,0.02f,view);
    assert(view_original==1 && rotation_calls==1);check_view();
    assert(*(void**)(f.pawn+0x2d8)==f.camera && *(void**)(f.camera+0xa0)==f.pawn);
    // Gameplay/menu/phone return immediately to original camera output.
    for(unsigned t=0;t<=7;t++)if(t!=3) {
        ready();capture();type=t;declined();assert(!cinema_scene.valid);
        hook_cinema_view(f.camera,0.02f,view);
        assert(view_original==1 && !rotation_calls);
        for(unsigned i=0;i<sizeof view;i++)assert(view[i]==0x5a);
    }
    ready();capture();current_tid=102;declined();assert(!rotation_calls);
    ready();capture();g_touch_gt_tid=0;declined();assert(!rotation_calls);
    ready();capture();active=f.other;declined();
    ready();capture();*(unsigned long long*)(f.pawn+0x18)^=1ULL<<32;declined();
    ready();capture();*(void**)(f.pawn+0x2d8)=f.other;declined();
    ready();capture();*(void**)(f.camera+0xa0)=f.other;declined();
    ready();capture();*(void**)(f.camera+0x10)=f.cls[2];declined();
    ready();capture();*(void**)(f.pawn+0x10)=f.cls[2];declined();
    ready();capture();*(int*)(f.cls[0]+0x58)=0x1167;declined();
    ready();capture();*(int*)(f.cls[1]+0x58)=0x7b7;declined();
    ready();capture();*(int*)(f.cls[0]+0x38)=129;declined();
    ready();capture();cinema_camera_class=0;declined();
    ready();capture();cinema_rotator=0;declined();
    ready();capture();assert(!cinema_flat_view(f.camera,0));
    // Invalid incoming scenes never reuse an older frame.
    float bad[]={NAN,INFINITY,-1,0,0.5f,171};
    for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++) {
        ready();capture();cinema_capture(f.pawn,&scene,bad[i],1);
        assert(!cinema_scene.valid);declined();
    }
    ready();capture();cinema_capture(f.pawn,&scene,50,0);assert(!cinema_scene.valid);declined();
    ready();capture();cinema_capture(f.pawn,0,50,1);assert(!cinema_scene.valid);declined();
    struct reload_tf bad_scene=scene;bad_scene.p[1]=NAN;
    ready();capture();cinema_capture(f.pawn,&bad_scene,50,1);assert(!cinema_scene.valid);declined();
    bad_scene=scene;bad_scene.s[2]=0;
    ready();capture();cinema_capture(f.pawn,&bad_scene,50,1);assert(!cinema_scene.valid);declined();
    bad_scene=scene;bad_scene.q.w=0.9f;
    ready();capture();cinema_capture(f.pawn,&bad_scene,50,1);assert(!cinema_scene.valid);declined();
    // Independent projection relation: horizontal/vertical tangent ratio is authored aspect.
    ready();for(int degrees=1;degrees<=170;degrees++) {
        cinema_capture(f.pawn,&scene,(float)degrees,1);
        assert(cinema_flat_view(f.camera,view));
        double y=(double)degrees*acos(-1.0)/360;
        double x=(double)*(float*)(view+24)*acos(-1.0)/360;
        assert(fabs(tan(x)/tan(y)-4.0/3.0)<0.00003);
    }
    assert(cinema_scene_logs==8 && cinema_view_logs==8);
    // Independent native-camera projection: y coverage stays unchanged, x coverage uses the surface aspect.
    unsigned extents[][2]={{2167,1080},{2400,1080},{1920,1080},{1600,1200},{1080,1080},{16384,4096}};
    for(unsigned v=0;v<sizeof extents/sizeof extents[0];v++) {
        ready();g_comp_w=extents[v][0];g_comp_h=extents[v][1];
        double aspect=(double)g_comp_w/g_comp_h;
        for(int degrees=1;degrees<=170;degrees++) {
            cinema_capture(f.pawn,&scene,(float)degrees,1);
            assert(cinema_flat_view(f.camera,view));
            double vertical=tan((double)degrees*acos(-1.0)/360);
            double horizontal=tan((double)*(float*)(view+24)*acos(-1.0)/360);
            assert(fabs(horizontal/vertical-aspect)<0.00015);
            assert(fabs(*(float*)(view+44)-aspect)<0.000001);
            // Native constrained perspective m00=1/tan(x/2),m11=aspect*m00.
            double m00=1/horizontal,m11=aspect*m00;
            assert(fabs(m11*vertical-1)<0.00003);
            assert(fabs(m00*(vertical*aspect)-1)<0.00003);
            assert((view[48]&1)==1 && view[49]==0);
        }
    }
    unsigned invalid[][2]={{0,0},{2400,0},{1080,2400},{20,20},{16385,1080},{5000,1080}};
    for(unsigned v=0;v<sizeof invalid/sizeof invalid[0];v++) {
        ready();capture();g_comp_w=invalid[v][0];g_comp_h=invalid[v][1];
        assert(cinema_flat_view(f.camera,view));
        assert(*(float*)(view+44)==4.0f/3.0f);
    }
    // Entry/rejection logs are bounded and never inspect C3 objects on a foreign thread.
    ready();capture();for(int i=0;i<20;i++)hook_cinema_view(f.camera,0.02f,view);
    assert(cinema_entry_logs==8);
    ready();capture();*(void**)(f.camera+0xa0)=f.other;
    for(int i=0;i<20;i++)declined();assert(cinema_reject_mask==(1u<<5));
    ready();capture();current_tid=102;hook_cinema_view(f.camera,0.02f,view);
    assert(!cinema_entry_logs && !cinema_reject_mask && !rotation_calls);
    ready();unsigned short caption[]={'r','1','0','0','_','0','1',0};
    struct {unsigned short* data;int count,capacity;} key={caption,8,8};
    for(int i=0;i<20;i++)hook_cinema_subtitle(f.other,&key,3.25f);
    assert(subtitle_original==20 && cinema_subtitle_logs==16);
    type=1;cinema_flat_view(f.camera,view);assert(!cinema_subtitle_logs);
    hook_cinema_subtitle(f.other,&key,3.25f);assert(subtitle_original==21 && !cinema_subtitle_logs);
    ready();current_tid=102;hook_cinema_subtitle(f.other,&key,3.25f);
    assert(subtitle_original==1 && !cinema_subtitle_logs);
    ready();key.count=513;hook_cinema_subtitle(f.other,&key,3.25f);assert(!cinema_subtitle_logs);
    key.count=8;key.capacity=7;hook_cinema_subtitle(f.other,&key,3.25f);assert(!cinema_subtitle_logs);
    key.capacity=8;hook_cinema_subtitle(f.other,&key,NAN);assert(!cinema_subtitle_logs);
    hook_cinema_subtitle(f.other,&key,601);assert(!cinema_subtitle_logs);
    assert(subtitle_original==4);
    movie_setting_checks();
    movie_probe_checks();
    terminal_view_checks();
    puts("PASS: actual cinema C original-first hooks, native output fields only; GT/type/pawn/FName/camera/owner/layout guards; black/invalid/stale recovery; 170 native-aspect plus1020 independent widescreen matrix cases,2167/2400 and portrait/invalid refusal; bounded logs");
}
