#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef struct {float x,y,z,w;} qf_t;
struct __attribute__((aligned(16))) reload_tf {qf_t q;float p[3],s[3],padding[2];};
struct cinema_ui_weak {int index,serial;};
struct cinema_ui_vec2 {float x,y;};
static qf_t q_mul(qf_t a,qf_t b) {
    return (qf_t){a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
                  a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
static void q_rot(qf_t q,const float v[3],float out[3]) {
    float dot=q.x*v[0]+q.y*v[1]+q.z*v[2],k=q.w*q.w-q.x*q.x-q.y*q.y-q.z*q.z;
    out[0]=2*dot*q.x+k*v[0]+2*q.w*(q.y*v[2]-q.z*v[1]);
    out[1]=2*dot*q.y+k*v[1]+2*q.w*(q.z*v[0]-q.x*v[2]);
    out[2]=2*dot*q.z+k*v[2]+2*q.w*(q.x*v[1]-q.y*v[0]);
}
static int reload_marker_valid(const struct reload_tf* tf) {
    float norm=tf->q.x*tf->q.x+tf->q.y*tf->q.y+tf->q.z*tf->q.z+tf->q.w*tf->q.w;
    if(!isfinite(norm)||fabsf(norm-1)>.01f)return 0;
    for(unsigned k=0;k<3;k++)if(!isfinite(tf->p[k])||!isfinite(tf->s[k])||tf->s[k]<=0)return 0;
    return 1;
}
static int reload_tf_valid(const struct reload_tf* tf) {
    if(!reload_marker_valid(tf))return 0;
    for(unsigned k=0;k<3;k++)if(fabsf(tf->s[k]-1)>.01f)return 0;
    return 1;
}
enum {OWNER,CATEGORY,MAIN,INFO,CAT_ROOT,MAIN_ROOT,INFO_ROOT,CAT_PARENT,MAIN_PARENT,INFO_PARENT,
      CAT_WIDGET,MAIN_WIDGET,INFO_WIDGET,MAIN_USER,INFO_USER,SCROLL,BACK,OFFSCREEN,OTHER,FOREIGN,GENERIC,GENERIC_ROOT,GENERIC_BUTTON,OBJECTS};
enum {SCENE_CLASS,BUTTON_CLASS,WIDGET_CLASS,SCROLL_CLASS,OWNER_CLASS,CAT_CLASS,MAIN_CLASS,INFO_CLASS,MAIN_USER_CLASS,INFO_USER_CLASS,OTHER_CLASS,CLASSES};
static unsigned char objects[OBJECTS][0x1000],classes[CLASSES][0x100],properties[10][0x60],object_kind[16];
static void* chains[CLASSES][2];
static int alive[OBJECTS],serials[OBJECTS],name_count;
static const char* names[64];
static unsigned transforms,restores,refreshes;
static int g_comp_w,g_comp_h;
static float g_fov_h_deg;
static struct cinema_ui_vec2 current_size[OBJECTS];
static struct reload_tf relative_tf[OBJECTS];
static int object_index(const void* object) {for(int i=0;i<OBJECTS;i++)if(object==objects[i])return i;return -1;}
static int pv_alive(const void* object) {int i=object_index(object);return i>=0&&alive[i];}
static int name_id(const char* text) {
    for(int i=0;i<name_count;i++)if(!strcmp(names[i],text))return i;
    assert(name_count<64);names[name_count]=text;return name_count++;
}
static void phone_inspect_fname(const void* name,char* out,int capacity) {
    unsigned id=*(const unsigned*)name;assert(id<(unsigned)name_count);snprintf(out,(unsigned)capacity,"%s",names[id]);
}
static void obj_name(const void* object,char* out,int capacity) {(void)object;snprintf(out,(unsigned)capacity,"fixture");}
#include MANUAL_BINDING_FRAGMENT
static void* scene_class(void) {return classes[SCENE_CLASS];}
static void* button_class(void) {return classes[BUTTON_CLASS];}
static void* widget_class(void) {return classes[WIDGET_CLASS];}
static void* scroll_class(void) {return classes[SCROLL_CLASS];}
static void* (*mv_scene_class)(void)=scene_class;
static void (*rl_world_transform)(void*,const struct reload_tf*,int,void*,int);
static void (*cu_weak_assign)(struct cinema_ui_weak*,const void*);
static void* (*cu_weak_get)(const struct cinema_ui_weak*);
#define LOGF(...) ((void)0)
#include MANUAL_FRAME_FRAGMENT
static void* (*cu_component_class)(void);
static struct cinema_ui_vec2 (*cu_draw_size)(const void*);
#include MANUAL_WIDGET_FRAGMENT
static void* (*fv_scroll_class)(void);
#include MANUAL_SCROLL_FRAGMENT
static struct reload_tf (*fv_relative_transform)(const void*);
static void (*fv_set_relative)(void*,const struct reload_tf*,int,void*,int);
static void* (*cu_get_widget)(const void*);
#include "../re4fix_ui_manual.h"
static void weak_assign(struct cinema_ui_weak* ref,const void* object) {
    ref->index=object_index(object);assert(ref->index>=0);ref->serial=serials[ref->index];
}
static void* weak_get(const struct cinema_ui_weak* ref) {
    return ref->index>=0&&ref->index<OBJECTS&&alive[ref->index]&&serials[ref->index]==ref->serial?objects[ref->index]:0;
}
static void* get_widget(const void* component) {return *(void* const*)((const char*)component+0x498);}
static struct cinema_ui_vec2 draw_size(const void* component) {
    struct cinema_ui_vec2 value;memcpy(&value,(const char*)component+0x438,sizeof value);return value;
}
static struct cinema_ui_vec2 actual_size(const void* component) {int i=object_index(component);assert(i>=0);return current_size[i];}
static int enabled(const void* component) {
    void* actor=*(void* const*)((const char*)component+0xa0);
    return *(const unsigned char*)((const char*)component+0x264)&&actor&&pv_alive(actor)
        && (*(const unsigned short*)((const char*)component+0x14c)&0x20)&&!(*(const unsigned*)((const char*)actor+0x58)&0x20);
}
static struct reload_tf world(const void* object) {struct reload_tf value;memcpy(&value,(const char*)object+0x1c0,sizeof value);return value;}
static void store_world(void* object,const struct reload_tf* value) {memcpy((char*)object+0x1c0,value,sizeof *value);}
static struct reload_tf compose(struct reload_tf parent,struct reload_tf local) {
    struct reload_tf out=local;float p[3],moved[3];out.q=q_mul(parent.q,local.q);
    for(unsigned k=0;k<3;k++){p[k]=local.p[k]*parent.s[k];out.s[k]=local.s[k]*parent.s[k];}
    q_rot(parent.q,p,moved);for(unsigned k=0;k<3;k++)out.p[k]=parent.p[k]+moved[k];return out;
}
static void set_world(void* root,const struct reload_tf* transform,int sweep,void* hit,int teleport) {
    assert(!sweep&&!hit&&!teleport);transforms++;
    struct reload_tf old=world(root);qf_t inverse={-old.q.x,-old.q.y,-old.q.z,old.q.w},correction=q_mul(transform->q,inverse);
    float scale=transform->s[0]/old.s[0];
    for(int i=0;i<OBJECTS;i++)if(objects[i]!=root&&fv_descends_from(objects[i],root)) {
        struct reload_tf value=world(objects[i]);float delta[3],moved[3];
        for(unsigned k=0;k<3;k++)delta[k]=(value.p[k]-old.p[k])*scale;
        q_rot(correction,delta,moved);value.q=q_mul(correction,value.q);
        for(unsigned k=0;k<3;k++){value.p[k]=transform->p[k]+moved[k];value.s[k]*=scale;}
        store_world(objects[i],&value);
    }
    store_world(root,transform);
}
static struct reload_tf get_relative(const void* root) {int i=object_index(root);assert(i>=0);return relative_tf[i];}
static void set_relative(void* root,const struct reload_tf* transform,int sweep,void* hit,int teleport) {
    int i=object_index(root);assert(i>=0);restores++;relative_tf[i]=*transform;
    void* parent=*(void**)((char*)root+0xc0);assert(parent&&pv_alive(parent));
    struct reload_tf value=compose(world(parent),*transform);set_world(root,&value,sweep,hit,teleport);
}
static void refresh(void* screen,void* root) {
    assert(fv_frame_owned(root,screen));refreshes++;
    if(screen==objects[CATEGORY]) {
        struct reload_tf row=world(objects[SCROLL]),local={{0,0,0,1},{0,0,-300},{1,1,1},{0,0}};
        row=compose(row,local);store_world(objects[OFFSCREEN],&row);
    }
}
static void add_field(int index,int cls,const char* name,int offset) {
    unsigned char* field=properties[index];*(void**)(field+8)=object_kind;*(unsigned*)(field+0x28)=(unsigned)name_id(name);
    *(int*)(field+0x34)=1;*(int*)(field+0x38)=8;*(int*)(field+0x4c)=offset;
    *(void**)(field+0x58)=*(void**)(classes[cls]+0x70);*(void**)(classes[cls]+0x70)=field;
}
static void set_class(int object,int cls) {*(void**)(objects[object]+0x10)=classes[cls];}
static void controls(int actor,void** parts,int count) {struct {void** data;int count,max;} list={parts,count,count};memcpy(objects[actor]+0x238,&list,sizeof list);}
static void children(int parent,void** parts,int count) {
    struct {void** data;int count,max;} list={parts,count,count};memcpy(objects[parent]+0xd0,&list,sizeof list);
    for(int i=0;i<count;i++)*(void**)((char*)parts[i]+0xc0)=objects[parent];
}
static void ready(void) {
    memset(objects,0,sizeof objects);memset(classes,0,sizeof classes);memset(properties,0,sizeof properties);memset(chains,0,sizeof chains);
    memset(current_size,0,sizeof current_size);name_count=0;
    const char* class_names[]={"SceneComponent","VR4UIButtonComponent","WidgetComponent","VR4UIScrollComponent",
        "UI_PauseMenu_Content_PlayingManual_BP_C","PlayingManualEntrySelection_C","PlayingManualMain_C","PlayingManualInfoText_C",
        "PlayingManualMainWidget_C","PlayingManualInfoTextWidget_C","OtherScreen_C"};
    const int class_sizes[]={0x1f0,0x330,0x550,0x430,0x428,0x2e7,0x3b0,0x299,0xd24,0x900,0x300};
    for(int i=0;i<CLASSES;i++) {
        *(unsigned*)(classes[i]+0x18)=(unsigned)name_id(class_names[i]);*(int*)(classes[i]+0x58)=class_sizes[i];
        *(void**)(classes[i]+0x30)=chains[i];*(int*)(classes[i]+0x38)=i>0&&i<4?1:0;
        chains[i][0]=classes[i<4?SCENE_CLASS:i]+0x30;chains[i][1]=classes[i]+0x30;
    }
    for(int i=0;i<OBJECTS;i++){alive[i]=1;serials[i]=7;set_class(i,SCENE_CLASS);*(unsigned short*)(objects[i]+0x14c)=0x20;}
    set_class(OWNER,OWNER_CLASS);set_class(CATEGORY,CAT_CLASS);set_class(MAIN,MAIN_CLASS);set_class(INFO,INFO_CLASS);
    set_class(MAIN_USER,MAIN_USER_CLASS);set_class(INFO_USER,INFO_USER_CLASS);set_class(OTHER,OTHER_CLASS);set_class(GENERIC,OTHER_CLASS);
    set_class(CAT_WIDGET,WIDGET_CLASS);set_class(MAIN_WIDGET,WIDGET_CLASS);set_class(INFO_WIDGET,WIDGET_CLASS);
    set_class(SCROLL,SCROLL_CLASS);set_class(BACK,BUTTON_CLASS);set_class(OFFSCREEN,BUTTON_CLASS);set_class(GENERIC_BUTTON,BUTTON_CLASS);
    *(unsigned*)object_kind=(unsigned)name_id("ObjectProperty");
    add_field(0,OWNER_CLASS,"DoorLeftScreen",0x3a0);add_field(1,OWNER_CLASS,"UIScreen",0x370);add_field(2,OWNER_CLASS,"DoorRightScreen",0x398);
    add_field(3,CAT_CLASS,"ContentParent",0x290);add_field(4,CAT_CLASS,"main",0x2c0);add_field(5,CAT_CLASS,"InfoText",0x2b8);
    add_field(6,MAIN_CLASS,"Widget_Main",0x258);add_field(7,MAIN_CLASS,"Widget",0x328);
    add_field(8,INFO_CLASS,"Widget_InfoText",0x258);add_field(9,INFO_CLASS,"Info",0x278);
    *(void**)(objects[OWNER]+0x3a0)=objects[CATEGORY];*(void**)(objects[OWNER]+0x370)=objects[MAIN];*(void**)(objects[OWNER]+0x398)=objects[INFO];
    *(void**)(objects[CATEGORY]+0x290)=objects[OWNER];*(void**)(objects[CATEGORY]+0x2c0)=objects[MAIN];*(void**)(objects[CATEGORY]+0x2b8)=objects[INFO];
    *(void**)(objects[MAIN]+0x258)=objects[MAIN_WIDGET];*(void**)(objects[MAIN]+0x328)=objects[MAIN_USER];
    *(void**)(objects[INFO]+0x258)=objects[INFO_WIDGET];*(void**)(objects[INFO]+0x278)=objects[INFO_USER];
    *(void**)(objects[MAIN_WIDGET]+0x498)=objects[MAIN_USER];*(void**)(objects[INFO_WIDGET]+0x498)=objects[INFO_USER];
    struct reload_tf identity={{0,0,0,1},{0,0,0},{1,1,1},{0,0}};
    for(int i=0;i<OBJECTS;i++){store_world(objects[i],&identity);relative_tf[i]=identity;}
    const qf_t rotations[3]={{0,0,.9170600744f,.3987490689f},{0,0,1,0},{0,0,-.9170600744f,.3987490689f}};
    const float positions[3][3]={{166,-120,15151},{211,0,15171},{166,120,15151}};
    for(unsigned i=0;i<3;i++) {
        int actor=CATEGORY+(int)i,root=CAT_ROOT+(int)i,parent=CAT_PARENT+(int)i,widget=CAT_WIDGET+(int)i;
        struct reload_tf value=identity;value.q=rotations[i];memcpy(value.p,positions[i],12);
        store_world(objects[parent],&value);store_world(objects[root],&value);
        *(void**)(objects[actor]+0xe0)=objects[OWNER];*(void**)(objects[actor]+0x130)=objects[root];
        *(void**)(objects[root]+0xa0)=objects[actor];*(void**)(objects[root]+0xc0)=objects[parent];
        *(void**)(objects[widget]+0x20)=*(void**)(objects[widget]+0xa0)=objects[actor];
        value.s[0]=value.s[1]=value.s[2]=.1f;store_world(objects[widget],&value);
        float requested[2]={500,500},pivot[2]={.5f,.5f};memcpy(objects[widget]+0x438,requested,8);memcpy(objects[widget]+0x45c,pivot,8);
    }
    current_size[CAT_WIDGET]=(struct cinema_ui_vec2){700,1000};current_size[MAIN_WIDGET]=(struct cinema_ui_vec2){1288,724};
    current_size[INFO_WIDGET]=(struct cinema_ui_vec2){700,972};
    static void* cat_children[3],*main_children[1],*info_children[1],*cat_parts[3];
    cat_children[0]=objects[CAT_WIDGET];cat_children[1]=objects[SCROLL];cat_children[2]=objects[BACK];
    main_children[0]=objects[MAIN_WIDGET];info_children[0]=objects[INFO_WIDGET];
    children(CAT_ROOT,cat_children,3);children(MAIN_ROOT,main_children,1);children(INFO_ROOT,info_children,1);
    for(int i=SCROLL;i<=OFFSCREEN;i++) {
        *(void**)(objects[i]+0xa0)=objects[CATEGORY];*(void**)(objects[i]+0x308)=objects[CATEGORY];objects[i][0x264]=1;
        struct reload_tf value=world(objects[CAT_ROOT]);store_world(objects[i],&value);
        float dims[2]={i==SCROLL?58:6,i==SCROLL?70:8};memcpy(objects[i]+0x318,dims,8);
    }
    *(void**)(objects[OFFSCREEN]+0xc0)=objects[FOREIGN];*(void**)(objects[OFFSCREEN]+0x310)=objects[SCROLL];
    struct reload_tf back=world(objects[BACK]),offset=identity;offset.p[1]=25;offset.p[2]=42;
    back=compose(back,offset);store_world(objects[BACK],&back);
    cat_parts[0]=objects[SCROLL];cat_parts[1]=objects[BACK];cat_parts[2]=objects[OFFSCREEN];controls(CATEGORY,cat_parts,3);controls(MAIN,0,0);controls(INFO,0,0);
    cu_weak_assign=weak_assign;cu_weak_get=weak_get;cu_component_class=widget_class;cu_draw_size=draw_size;cu_get_widget=get_widget;
    fv_button_class=button_class;fv_ui_enabled=enabled;fv_scroll_class=scroll_class;fv_control_plane=fv_scroll_plane;
    fv_current_draw_size=actual_size;fv_widget_planes=fv_collect_widgets;fv_relative_transform=get_relative;fv_set_relative=set_relative;
    rl_world_transform=set_world;fv_screen_moved=refresh;
    fv_manual_owner=(struct cinema_ui_weak){-1,0};fv_framed.screen=fv_framed.root=(struct cinema_ui_weak){-1,0};
    for(unsigned i=0;i<3;i++){fv_manual[i].actor=fv_manual[i].root=fv_manual[i].parent=(struct cinema_ui_weak){-1,0};fv_manual[i].changed=0;}
    refresh(objects[CATEGORY],objects[CAT_ROOT]);transforms=restores=refreshes=0;
    g_comp_w=2780;g_comp_h=1264;g_fov_h_deg=90;
    memset(objects[MAIN_USER]+0x900,0xa5,64);memset(objects[INFO_USER]+0x500,0x62,64);
}
static struct reload_tf camera(void) {return (struct reload_tf){{0,0,0,1},{-6,0,15171.45f},{1,1,1},{0,0}};}
static void close_transform(struct reload_tf a,struct reload_tf b) {
    for(unsigned i=0;i<3;i++){assert(fabsf(a.p[i]-b.p[i])<.01f);assert(fabsf(a.s[i]-b.s[i])<.0001f);}
    float dot=a.q.x*b.q.x+a.q.y*b.q.y+a.q.z*b.q.z+a.q.w*b.q.w;assert(fabsf(fabsf(dot)-1)<.0001f);
}
static void check_bounds(unsigned panel,const struct reload_tf* view) {
    void* actor=objects[CATEGORY+panel],*root=objects[CAT_ROOT+panel];struct reload_tf at=world(root);
    struct fv_plane planes[64];unsigned count=fv_screen_planes(actor,root,&at,planes,64,0);assert(count&&count<=64);
    qf_t inverse={-view->q.x,-view->q.y,-view->q.z,view->q.w};float tangent=tanf(g_fov_h_deg*.00872664625997165f),aspect=(float)g_comp_w/g_comp_h;
    for(unsigned i=0;i<count;i++)for(unsigned corner=0;corner<4;corner++) {
        struct fv_plane* p=planes+i;float point[3]={0,((corner&1)?.5f:-.5f)*p->width*p->transform.s[1],((corner&2)?.5f:-.5f)*p->height*p->transform.s[2]},at_world[3],in_view[3];
        q_rot(p->transform.q,point,at_world);for(unsigned k=0;k<3;k++)at_world[k]+=p->transform.p[k]-view->p[k];
        q_rot(inverse,at_world,in_view);assert(in_view[0]>0);
        float x=.5f+.5f*in_view[1]/(in_view[0]*tangent),y=.5f-.5f*in_view[2]*aspect/(in_view[0]*tangent);
        const struct fv_region* r=fv_manual_regions+panel;
        assert(x>=r->x-.0001f&&x<=r->x+r->width+.0001f&&y>=r->y-.0001f&&y<=r->y+r->height+.0001f);
    }
}
static void refused(void) {
    unsigned char before[sizeof objects];memcpy(before,objects,sizeof objects);struct reload_tf view=camera();
    assert(!fv_manual_tick(objects[CATEGORY],&view)&&!transforms&&!restores&&!memcmp(before,objects,sizeof objects));
}
static void geometry_cases(void) {
    for(unsigned n=0;n<36;n++) {
        ready();struct reload_tf view=camera();
        float yaw=n*.23f,pitch=.35f*sinf((float)n),roll=.2f*cosf((float)n);
        view.q=q_mul(q_mul((qf_t){0,0,sinf(yaw*.5f),cosf(yaw*.5f)},
                             (qf_t){0,sinf(pitch*.5f),0,cosf(pitch*.5f)}),
                             (qf_t){sinf(roll*.5f),0,0,cosf(roll*.5f)});
        g_comp_w=1200+(int)(n%11)*240;g_comp_h=1200;g_fov_h_deg=30+(float)(n%10)*11;
        assert(fv_manual_tick(objects[CATEGORY],&view));
        for(unsigned i=0;i<3;i++)check_bounds(i,&view);
        struct reload_tf at=world(objects[CAT_ROOT]);struct fv_plane planes[64];
        assert(fv_screen_planes(objects[CATEGORY],objects[CAT_ROOT],&at,planes,64,0)==3);
        fv_manual_restore();assert(restores==3);
    }
    struct reload_tf view=camera(),root={{0,0,1,0},{100,0,15171},{1,1,1},{0,0}},out;
    struct fv_plane plane={root,60,100};struct fv_region invalid={.9f,.1f,.2f,.8f};
    assert(!fv_frame_region(&view,&root,&plane,1,2.2f,90,&invalid,&out));
    invalid=(struct fv_region){0,0,NAN,1};assert(!fv_frame_region(&view,&root,&plane,1,2.2f,90,&invalid,&out));
    assert(!fv_frame_transform(&view,&root,&plane,1,.69f,90,&out));
    assert(!fv_frame_transform(&view,&root,&plane,1,2.2f,20,&out));
    puts("PASS: all three Manual regions fit actual rendered/button corners across 36 camera yaw/pitch/roll, aspect and FOV cases; offscreen Scroll rows never expand bounds and generic FOV/aspect refusal is unchanged");
}
int main(void) {
    ready();struct reload_tf view=camera(),original[3];for(unsigned i=0;i<3;i++)original[i]=world(objects[CAT_ROOT+i]);
    assert(fv_manual_tick(objects[CATEGORY],&view)&&transforms==3&&refreshes==3);
    for(unsigned i=0;i<3;i++)check_bounds(i,&view);
    struct reload_tf stable[3];for(unsigned i=0;i<3;i++)stable[i]=world(objects[CAT_ROOT+i]);
    for(unsigned n=0;n<20;n++){assert(fv_manual_tick(objects[CATEGORY],&view));for(unsigned i=0;i<3;i++)close_transform(stable[i],world(objects[CAT_ROOT+i]));}
    objects[MAIN_USER][0x900]=0x35;objects[INFO_USER][0x500]=0x17;
    current_size[MAIN_WIDGET]=(struct cinema_ui_vec2){1600,900};assert(fv_manual_tick(objects[CATEGORY],&view));for(unsigned i=0;i<3;i++)check_bounds(i,&view);
    g_comp_w=1920;g_comp_h=1200;g_fov_h_deg=67.380135f;assert(fv_manual_tick(objects[CATEGORY],&view));for(unsigned i=0;i<3;i++)check_bounds(i,&view);
    assert(!fv_manual_tick(0,&view)&&restores==3);for(unsigned i=0;i<3;i++)close_transform(original[i],world(objects[CAT_ROOT+i]));
    assert(objects[MAIN_USER][0x900]==0x35&&objects[INFO_USER][0x500]==0x17);
    ready();assert(fv_manual_tick(objects[MAIN],&view));fv_manual_restore();assert(restores==3);
    ready();assert(fv_manual_tick(objects[INFO],&view));fv_manual_restore();assert(restores==3);
    ready();*(void**)(objects[CATEGORY]+0xe0)=objects[OTHER];refused();
    ready();*(void**)(objects[OWNER]+0x370)=objects[OTHER];refused();
    ready();*(void**)(objects[CATEGORY]+0x290)=0;refused();
    ready();*(void**)(objects[CATEGORY]+0x2b8)=objects[OTHER];refused();
    ready();*(void**)(objects[CATEGORY]+0x2c0)=objects[OTHER];refused();
    ready();*(void**)(objects[MAIN]+0xe0)=objects[OTHER];refused();
    ready();*(void**)(objects[MAIN_ROOT]+0xa0)=objects[OTHER];refused();
    ready();*(void**)(objects[MAIN_WIDGET]+0x20)=objects[OTHER];refused();
    ready();*(void**)(objects[INFO_WIDGET]+0xa0)=objects[OTHER];refused();
    ready();*(void**)(objects[MAIN_WIDGET]+0x498)=objects[INFO_USER];refused();
    ready();*(unsigned*)(classes[INFO_USER_CLASS]+0x18)=(unsigned)name_id("WrongWidget_C");refused();
    ready();*(int*)(properties[0]+0x4c)=0x398;refused();
    ready();*(int*)(properties[3]+0x34)=2;refused();
    ready();*(int*)(properties[8]+0x38)=4;refused();
    ready();*(unsigned*)(objects[OWNER]+0x58)=0x20;refused();
    ready();*(unsigned*)(objects[INFO]+0x58)=0x20;assert(fv_manual_tick(objects[CATEGORY],&view)&&transforms==2);fv_manual_restore();assert(restores==2);
    ready();*(unsigned short*)(objects[INFO_WIDGET]+0x14c)=0;assert(fv_manual_tick(objects[CATEGORY],&view)&&transforms==2);fv_manual_restore();assert(restores==2);
    ready();*(int*)(objects[CATEGORY]+0x240)=513;refused();
    ready();assert(fv_manual_tick(objects[CATEGORY],&view));*(void**)(objects[MAIN_ROOT]+0xc0)=objects[FOREIGN];
    struct reload_tf foreign=world(objects[MAIN_ROOT]);fv_manual_restore();assert(restores==2);close_transform(foreign,world(objects[MAIN_ROOT]));
    ready();assert(fv_manual_tick(objects[CATEGORY],&view));serials[MAIN_ROOT]++;fv_manual_restore();assert(restores==2);
    ready();assert(fv_manual_tick(objects[CATEGORY],&view));serials[OWNER]++;fv_manual_restore();assert(!restores);
    ready();assert(fv_manual_tick(objects[CATEGORY],&view));*(void**)(objects[MAIN]+0xe0)=objects[OTHER];fv_manual_restore();assert(restores==2);
    ready();assert(fv_manual_tick(objects[CATEGORY],&view));serials[MAIN_PARENT]++;fv_manual_restore();assert(restores==2);
    ready();assert(fv_manual_tick(objects[CATEGORY],&view));struct reload_tf parent=world(objects[MAIN_PARENT]);parent.p[1]+=25;
    store_world(objects[MAIN_PARENT],&parent);fv_manual_restore();assert(restores==3);close_transform(world(objects[MAIN_ROOT]),parent);
    ready();*(void**)(objects[GENERIC]+0x130)=objects[GENERIC_ROOT];*(void**)(objects[GENERIC_ROOT]+0xa0)=objects[GENERIC];
    *(void**)(objects[GENERIC_ROOT]+0xc0)=objects[FOREIGN];*(void**)(objects[GENERIC_BUTTON]+0xa0)=objects[GENERIC];
    *(void**)(objects[GENERIC_BUTTON]+0x308)=objects[GENERIC];objects[GENERIC_BUTTON][0x264]=1;
    static void* generic_parts[1];generic_parts[0]=objects[GENERIC_BUTTON];controls(GENERIC,generic_parts,1);children(GENERIC_ROOT,generic_parts,1);
    struct reload_tf generic={{0,0,1,0},{200,0,15171},{1,1,1},{0,0}};store_world(objects[GENERIC_ROOT],&generic);store_world(objects[GENERIC_BUTTON],&generic);
    float dims[2]={90,50};memcpy(objects[GENERIC_BUTTON]+0x318,dims,8);
    assert(fv_frame_screen(objects[GENERIC],&view)&&cu_weak_get(&fv_framed.screen)==objects[GENERIC]);
    assert(fv_manual_tick(objects[CATEGORY],&view)&&!cu_weak_get(&fv_framed.screen));close_transform(generic,world(objects[GENERIC_ROOT]));
    assert(!fv_manual_tick(objects[OTHER],&view)&&restores==3);assert(!cu_weak_get(&fv_manual_owner));
    geometry_cases();
    puts("PASS: actual Manual C binds exact original sibling actors/widgets, includes widget-only article/text, uses current draw sizes and logical Scroll bounds, fits all native corners into three disjoint regions, is stable on repeated frames/resize/live content, and restores only weak owned roots with unchanged parents");
}
