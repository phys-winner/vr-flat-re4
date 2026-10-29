#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef struct { float x, y, z, w; } qf_t;
struct __attribute__((aligned(16))) reload_tf { qf_t q; float p[3], s[3], padding[2]; };
static qf_t q_mul(qf_t a, qf_t b) {
    return (qf_t){a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y, a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
                  a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w, a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
static void q_rot(qf_t q, const float v[3], float o[3]) {
    float d=q.x*v[0]+q.y*v[1]+q.z*v[2], uu=q.x*q.x+q.y*q.y+q.z*q.z, k=q.w*q.w-uu;
    o[0]=2*d*q.x+k*v[0]+2*q.w*(q.y*v[2]-q.z*v[1]);
    o[1]=2*d*q.y+k*v[1]+2*q.w*(q.z*v[0]-q.x*v[2]);
    o[2]=2*d*q.z+k*v[2]+2*q.w*(q.x*v[1]-q.y*v[0]);
}
static int reload_marker_valid(const struct reload_tf* t) {
    float n=t->q.x*t->q.x+t->q.y*t->q.y+t->q.z*t->q.z+t->q.w*t->q.w;
    if (!isfinite(n) || fabsf(n-1)>.01f) return 0;
    for (unsigned k=0;k<3;k++) if (!isfinite(t->p[k]) || !isfinite(t->s[k]) || t->s[k]<=0) return 0;
    return 1;
}
static int reload_tf_valid(const struct reload_tf* t) {
    if (!reload_marker_valid(t)) return 0;
    for (unsigned k=0;k<3;k++) if (fabsf(t->s[k]-1)>.01f) return 0;
    return 1;
}
#include "../re4fix_ui_frame.h"
#ifdef FLAT_VIEW_FRAGMENT
struct cinema_ui_weak { int index,serial; };
static unsigned g_comp_w=2424,g_comp_h=1080;
static float g_fov_h_deg=90;
static void (*rl_world_transform)(void*,const struct reload_tf*,int,void*,int);
static void (*cu_weak_assign)(struct cinema_ui_weak*,const void*);
static void* (*cu_weak_get)(const struct cinema_ui_weak*);
static unsigned char fixture[5][0x600];
static const void* scene_class(void) {return (void*)1;}
static int pv_alive(const void* p) {return p && *(const int*)((const char*)p+8)==1;}
static int menu_visual_layout(const void* p,const void* cls,int minimum) {
    (void)minimum;return pv_alive(p) && (cls==(void*)1 || *(const int*)((const char*)p+4)==(int)(size_t)cls);
}
static void* (*mv_scene_class)(void)=(void* (*)(void))scene_class;
static int wizard_names;
static void obj_name(void* p,char* name,int bytes) {
    const char* value=wizard_names && p==fixture[2] ? "VR4UIButton_Next" : "fixture";
    assert(bytes>(int)strlen(value));memcpy(name,value,strlen(value)+1);
}
#define LOGF(...) ((void)0)
#include FLAT_VIEW_FRAGMENT
#ifdef WIDGET_FRAME_FRAGMENT
struct cinema_ui_vec2 {float x,y;};
static void* (*cu_component_class)(void);
static struct cinema_ui_vec2 (*cu_draw_size)(const void*);
#include WIDGET_FRAME_FRAGMENT
static void* mock_widget_class(void) {return (void*)3;}
static struct cinema_ui_vec2 mock_draw_size(const void* part) {
    struct cinema_ui_vec2 size;memcpy(&size,(const char*)part+0x438,sizeof size);return size;
}
static struct cinema_ui_vec2 mock_current_size(const void* part) {(void)part;return (struct cinema_ui_vec2){1288,724};}
static void widget_plane_checks(void) {
    memset(fixture,0,sizeof fixture);
    for(unsigned i=0;i<5;i++) {*(int*)(fixture[i]+8)=1;*(unsigned short*)(fixture[i]+0x14c)=0x20;}
    cu_component_class=mock_widget_class;cu_draw_size=mock_draw_size;
    static void* children[2];children[0]=fixture[2];children[1]=fixture[1];
    struct {void** data;int count,max;} list={children,2,2};memcpy(fixture[1]+0xd0,&list,sizeof list);
    *(int*)(fixture[2]+4)=3;
    struct reload_tf tf={{0,0,0,1},{200,40,180},{1,2,3},{0,0}};
    memcpy(fixture[2]+0x1c0,&tf,sizeof tf);
    float dims[2]={100,50},pivot[2]={.2f,.8f};
    memcpy(fixture[2]+0x438,dims,sizeof dims);memcpy(fixture[2]+0x45c,pivot,sizeof pivot);
    struct fv_plane plane;
    assert(fv_collect_widgets(fixture[1],&plane,1)==1);
    assert(plane.width==100 && plane.height==50 && fabsf(plane.transform.p[1]+20)<.0001f && fabsf(plane.transform.p[2]-225)<.0001f);
    fv_current_draw_size=mock_current_size;
    assert(fv_collect_widgets(fixture[1],&plane,1)==1 && plane.width==1288 && plane.height==724);
    fv_current_draw_size=0;
    fixture[2][0x429]=1;assert(fv_collect_widgets(fixture[1],&plane,1)==0);fixture[2][0x429]=0;
    fixture[2][0x518]=1;assert(fv_collect_widgets(fixture[1],&plane,1)==0);fixture[2][0x518]=0;
    *(unsigned short*)(fixture[1]+0x14c)=0x420;assert(fv_collect_widgets(fixture[1],&plane,1)==1);*(unsigned short*)(fixture[1]+0x14c)=0x20;
    assert(fv_collect_widgets(fixture[1],&plane,0)==1);
    list.count=257;memcpy(fixture[1]+0xd0,&list,sizeof list);assert(fv_collect_widgets(fixture[1],&plane,1)==2);
    puts("PASS: actual rendered WidgetComponent size including DrawAtDesiredSize, pivot/anisotropic scale, curved/screen-space/hidden leaf refusal, visible children of hidden containers, native tree cycle and bounds guards");
}
#endif
static unsigned native_transforms;
static unsigned native_refreshes;
static void mock_refresh(void* screen,void* root) {assert(screen==fixture[0]&&root==fixture[1]);native_refreshes++;}
static void mock_transform(void* root,const struct reload_tf* tf,int sweep,void* hit,int teleport) {
    assert(root==fixture[1] && sweep==0 && !hit && teleport==0);
    native_transforms++;memcpy((char*)root+0x1c0,tf,sizeof *tf);
}
static void mock_weak_assign(struct cinema_ui_weak* ref,const void* object) {
    ref->index=-1;ref->serial=1;
    for(unsigned i=0;i<5;i++)if(object==fixture[i])ref->index=(int)i;
    assert(ref->index>=0);
}
static void* mock_weak_get(const struct cinema_ui_weak* ref) {
    return ref->index>=0 && ref->index<5 && pv_alive(fixture[ref->index]) ? fixture[ref->index] : 0;
}
static void* mock_button_class(void) {return (void*)2;}
static int mock_enabled(const void* part) {return *(const unsigned char*)((const char*)part+0x264);}
#ifdef WIZARD_FRAME_FRAGMENT
static void (*fv_ui_click)(void*,const void*,const void*);
static unsigned wizard_clicks,int_sets;
static int mock_click(void* part,void* screen) {assert(part==fixture[2] && screen==fixture[0]);wizard_clicks++;return 1;}
#define fv_click_component mock_click
static int cinema_ui_named(const void* object,const char* name,int size) {
    return object==fixture[0] && !strcmp(name,"ComfortCustomizeCoverScreen_BP_C") && size==0x250
        && *(void* const*)((const char*)object+0x10)==fixture[4];
}
static const char* cinema_field_property(const void* object,const char* name,const char* kind) {
    assert(object==fixture[0] && !strcmp(name,"currentScreen") && !strcmp(kind,"IntProperty"));return (const char*)fixture[3];
}
#include WIZARD_FRAME_FRAGMENT
#undef fv_click_component
static void mock_int_set(void* object,unsigned long long name,int value) {
    assert(object==fixture[0] && name==123 && value==4);int_sets++;*(int*)((char*)object+0x500)=value;
}
static void wizard_ready(void) {
    memset(fixture,0,sizeof fixture);for(unsigned i=0;i<5;i++)*(int*)(fixture[i]+8)=1;
    *(void**)(fixture[0]+0x10)=fixture[4];*(int*)(fixture[4]+0x58)=0x600;
    static void* parts[1];parts[0]=fixture[2];struct {void** data;int count,max;} list={parts,1,1};
    memcpy(fixture[0]+0x238,&list,sizeof list);*(int*)(fixture[2]+4)=2;
    *(void**)(fixture[2]+0xa0)=fixture[0];*(void**)(fixture[2]+0x308)=fixture[0];fixture[2][0x264]=1;
    *(unsigned long long*)(fixture[3]+0x28)=123;*(int*)(fixture[3]+0x34)=1;
    *(int*)(fixture[3]+0x38)=4;*(int*)(fixture[3]+0x4c)=0x500;
    cu_weak_get=mock_weak_get;cu_weak_assign=mock_weak_assign;fv_button_class=mock_button_class;
    fv_set_int=mock_int_set;fv_ui_click=(void(*)(void*,const void*,const void*))1;
    fv_wizard_finished=(struct cinema_ui_weak){-1,0};wizard_names=1;wizard_clicks=int_sets=0;
}
static void wizard_checks(void) {
    wizard_ready();assert(fv_finish_wizard(fixture[0]) && wizard_clicks==1 && int_sets==1 && *(int*)(fixture[0]+0x500)==4);
    assert(fv_finish_wizard(fixture[0]) && wizard_clicks==1 && int_sets==1);
    wizard_ready();*(void**)(fixture[2]+0xa0)=fixture[1];assert(!fv_finish_wizard(fixture[0]) && !int_sets && !wizard_clicks);
    wizard_ready();*(void**)(fixture[2]+0x308)=fixture[1];assert(!fv_finish_wizard(fixture[0]) && !int_sets);
    wizard_ready();fixture[2][0x264]=0;assert(!fv_finish_wizard(fixture[0]) && !int_sets);
    wizard_ready();*(int*)(fixture[3]+0x38)=1;assert(!fv_finish_wizard(fixture[0]) && !int_sets);
    wizard_ready();*(int*)(fixture[3]+0x34)=2;assert(!fv_finish_wizard(fixture[0]) && !int_sets);
    wizard_ready();*(int*)(fixture[3]+0x4c)=0x600;assert(!fv_finish_wizard(fixture[0]) && !int_sets);
    wizard_ready();*(void**)(fixture[0]+0x10)=0;assert(!fv_finish_wizard(fixture[0]) && !int_sets);
    wizard_ready();fv_ui_click=0;assert(!fv_finish_wizard(fixture[0]) && !int_sets);
    wizard_names=0;
    puts("PASS: actual wizard completion performs one reflected stage4 set and one original Next; exact class/property bounds, owner/registered screen/enabled/weak-once guards");
}
#endif
static void frame_binding_checks(void) {
    memset(fixture,0,sizeof fixture);fv_framed.screen.index=fv_framed.root.index=-1;
    for(unsigned i=0;i<5;i++)*(int*)(fixture[i]+8)=1;
    static void* parts[1];parts[0]=fixture[2];
    *(void**)(fixture[0]+0x130)=fixture[1];*(void**)(fixture[1]+0xa0)=fixture[0];
    struct {void** data;int count,max;} list={parts,1,1};memcpy(fixture[0]+0x238,&list,sizeof list);
    *(int*)(fixture[2]+4)=2;fixture[2][0x264]=1;
    *(void**)(fixture[2]+0x308)=fixture[0];*(void**)(fixture[2]+0xc0)=fixture[1];
    float size[2]={100,50};memcpy(fixture[2]+0x318,size,sizeof size);
    struct reload_tf tf={{0,0,0,1},{200,40,180},{1,1,1},{0,0}},camera={{0,0,0,1},{0,0,160},{1,1,1},{0,0}};
    memcpy(fixture[1]+0x1c0,&tf,sizeof tf);memcpy(fixture[2]+0x1c0,&tf,sizeof tf);
    cu_weak_get=mock_weak_get;cu_weak_assign=mock_weak_assign;rl_world_transform=mock_transform;
    fv_button_class=mock_button_class;fv_ui_enabled=mock_enabled;native_transforms=native_refreshes=0;fv_screen_moved=mock_refresh;
    assert(fv_frame_screen(fixture[0],&camera) && native_transforms==1);
    assert(native_refreshes==1);
    assert(fv_framed.original.p[0]==200 && fv_framed.root.index==1 && fv_framed.screen.index==0);
    fv_frame_restore();assert(native_transforms==2 && native_refreshes==2 && !memcmp(fixture[1]+0x1c0,&tf,sizeof tf));
    struct reload_tf far=tf;far.p[2]+=500;memcpy(fixture[2]+0x1c0,&far,sizeof far);
    assert(!fv_frame_screen(fixture[0],&camera));memcpy(fixture[2]+0x1c0,&tf,sizeof tf);
    fv_frame_restore();assert(native_transforms==2);
    *(void**)(fixture[2]+0xc0)=fixture[2];assert(!fv_frame_screen(fixture[0],&camera));
    *(void**)(fixture[2]+0xc0)=fixture[3];assert(!fv_frame_screen(fixture[0],&camera));
    *(void**)(fixture[2]+0xc0)=fixture[1];
    *(void**)(fixture[2]+0x308)=fixture[3];assert(!fv_frame_screen(fixture[0],&camera));
    *(void**)(fixture[2]+0x308)=fixture[0];
    fixture[2][0x264]=0;assert(!fv_frame_screen(fixture[0],&camera));fixture[2][0x264]=1;
    *(void**)(fixture[1]+0xa0)=fixture[3];assert(!fv_frame_screen(fixture[0],&camera));
    *(void**)(fixture[1]+0xa0)=fixture[0];assert(native_transforms==2);
    assert(fv_frame_screen(fixture[0],&camera));
    *(int*)(fixture[1]+8)=0;fv_frame_restore();assert(native_transforms==3 && native_refreshes==3);
    fv_screen_moved=0;
    puts("PASS: production native frame binding; exact root owner, ancestry/cycle, registered-screen and enabled guards; weak lifetime and original transform restoration");
}
#endif
static qf_t rotation(float yaw,float pitch,float roll) {
    return q_mul(q_mul((qf_t){0,0,sinf(yaw*.5f),cosf(yaw*.5f)},
                       (qf_t){0,sinf(pitch*.5f),0,cosf(pitch*.5f)}),
                 (qf_t){sinf(roll*.5f),0,0,cosf(roll*.5f)});
}
static void check_fit(struct reload_tf camera,struct reload_tf root,struct fv_plane planes[4],float aspect,float fov) {
    struct reload_tf before_root=root,before_camera=camera,output;
    struct fv_plane before[4];memcpy(before,planes,sizeof before);
    assert(fv_frame_transform(&camera,&root,planes,4,aspect,fov,&output));
    assert(!memcmp(&root,&before_root,sizeof root) && !memcmp(&camera,&before_camera,sizeof camera));
    assert(!memcmp(before,planes,sizeof before));
    qf_t inv_root={-root.q.x,-root.q.y,-root.q.z,root.q.w};
    qf_t inv_camera={-camera.q.x,-camera.q.y,-camera.q.z,camera.q.w};
    float span=0,first_x=0;
    for(unsigned i=0;i<4;i++) for(unsigned c=0;c<4;c++) {
        float local[3]={0,((c&1)?.5f:-.5f)*planes[i].width*planes[i].transform.s[1],
                          ((c&2)?.5f:-.5f)*planes[i].height*planes[i].transform.s[2]};
        float old_world[3],root_local[3],world[3],view[3];
        q_rot(planes[i].transform.q,local,old_world);
        for(unsigned k=0;k<3;k++)old_world[k]+=planes[i].transform.p[k]-root.p[k];
        q_rot(inv_root,old_world,root_local);
        for(unsigned k=0;k<3;k++)root_local[k]=root_local[k]/root.s[k]*output.s[k];
        q_rot(output.q,root_local,world);
        for(unsigned k=0;k<3;k++)world[k]+=output.p[k]-camera.p[k];
        q_rot(inv_camera,world,view);
        if(i==0 && c==0)first_x=view[0];
        assert(fabsf(view[0]-100)<.002f && fabsf(view[0]-first_x)<.002f);
        float x=fabsf(view[1]/view[0]/tanf(fov*.00872664625997165f));
        float y=fabsf(view[2]/view[0]/tanf(fov*.00872664625997165f)*aspect);
        assert(x<=.8601f && y<=.8601f);
        span=fmaxf(span,fmaxf(x,y));
    }
    assert(span>.8599f);
    struct fv_plane framed[4];
    qf_t delta=q_mul(output.q,inv_root);
    for(unsigned i=0;i<4;i++) {
        framed[i]=planes[i];framed[i].transform.q=q_mul(delta,planes[i].transform.q);
        float d[3],p[3];for(unsigned k=0;k<3;k++)d[k]=(planes[i].transform.p[k]-root.p[k])*(output.s[k]/root.s[k]);
        q_rot(delta,d,p);
        for(unsigned k=0;k<3;k++) {framed[i].transform.p[k]=p[k]+output.p[k];framed[i].transform.s[k]*=output.s[k]/root.s[k];}
    }
    struct reload_tf again;
    assert(fv_frame_transform(&camera,&output,framed,4,aspect,fov,&again));
    for(unsigned k=0;k<3;k++)assert(fabsf(output.p[k]-again.p[k])<.002f && fabsf(output.s[k]-again.s[k])<.001f);
    float dot=output.q.x*again.q.x+output.q.y*again.q.y+output.q.z*again.q.z+output.q.w*again.q.w;
    assert(fabsf(fabsf(dot)-1)<.00001f);
}
static void projection_checks(void) {
    for(unsigned type=0;type<10;type++)assert(fv_world_popup_type(type)==(type==1 || type==7));
    for (unsigned i=30;i<=130;i++) {
        float fov=fv_console_fov((float)i);
        assert(fabsf(tanf(i*.00872664625997165f)/tanf(fov*.00872664625997165f)-1.5f)<.00001f);
        for(unsigned tab=0;tab<12;tab++)assert(fv_console_fov((float)i)==fov);
    }
    assert(fabsf(fv_console_fov(90)-67.380135f)<.00001f);
    assert(fv_console_fov(29)==29 && fv_console_fov(131)==131 && isnan(fv_console_fov(NAN)));
    puts("PASS: constant console magnification for every tab/scroll/hover, no dependency on rows or enabled controls; invalid authored FOV unchanged");
}
static void mesh_plane_checks(void) {
    for(unsigned axis=0;axis<3;axis++)for(unsigned n=0;n<60;n++) {
        struct reload_tf mesh={rotation(n*.07f,.3f,-.2f),{30,-40,150},{1.1f,.8f,1.3f},{0,0}};
        float origin[3]={5,-3,4},extent[3]={50,60,70};extent[axis]=0;
        struct fv_plane plane;assert(fv_mesh_plane(&mesh,origin,extent,&plane));
        unsigned width=axis==1 ? 0:1,height=axis==2 ? 0:2;
        for(unsigned corner=0;corner<4;corner++) {
            float local[3]={0,((corner&1)?.5f:-.5f)*plane.width*plane.transform.s[1],
                ((corner&2)?.5f:-.5f)*plane.height*plane.transform.s[2]},actual[3];
            q_rot(plane.transform.q,local,actual);for(unsigned i=0;i<3;i++)actual[i]+=plane.transform.p[i];
            int found=0;
            for(unsigned target=0;target<4;target++) {
                float old[3];for(unsigned i=0;i<3;i++)old[i]=origin[i];
                old[width]+=((target&1)?1:-1)*extent[width];old[height]+=((target&2)?1:-1)*extent[height];
                for(unsigned i=0;i<3;i++)old[i]*=mesh.s[i];float expected[3];q_rot(mesh.q,old,expected);
                float error=0;for(unsigned i=0;i<3;i++)error+=fabsf(actual[i]-expected[i]-mesh.p[i]);
                if(error<.001f)found=1;
            }
            assert(found);
        }
    }
    struct reload_tf tf={{0,0,0,1},{0,0,0},{1,1,1},{0,0}};float origin[3]={0},extent[3]={50,50,50};struct fv_plane plane;
    assert(!fv_mesh_plane(&tf,origin,extent,&plane));extent[0]=NAN;assert(!fv_mesh_plane(&tf,origin,extent,&plane));
    extent[0]=0;assert(fv_mesh_plane(&tf,origin,extent,&plane));origin[1]=INFINITY;assert(!fv_mesh_plane(&tf,origin,extent,&plane));
    puts("PASS: original flat mesh bounds convert to UI Y/Z plane with identical four corners across three mesh normals,180 rotations and anisotropic scales; nonflat/invalid bounds refused");
}
static void footer_geometry_checks(void) {
    {
        struct reload_tf content={{0,0,0,1},{200,0,160},{1,1,1},{0,0}},relative=content;
        struct fv_plane row={content,60,14},button={content,60,14},row_visual=row,button_visual=button;
        row_visual.transform.p[0]-=.05f;button_visual.transform.p[0]+=.397f;
        assert(fv_footer_visual_depth(&content,&row,&button,&row_visual,&button_visual,&relative));
        assert(fabsf(relative.p[0]-199.553f)<.0001f);
        button_visual.transform.p[0]+=20;assert(!fv_footer_visual_depth(&content,&row,&button,&row_visual,&button_visual,&relative));
    }
    for(unsigned n=1;n<=180;n++) {
        struct reload_tf content={rotation((float)n*.03f,.2f,-.1f),{200,10,160},{.8f,1.2f,.9f},{0,0}};
        struct reload_tf root={rotation((float)n*.07f,-.3f,.2f),{180,100,150},{1.1f,.9f,1.2f},{0,0}};
        struct fv_plane row={{q_mul(content.q,rotation(.2f,0,0)),{200,15,170},{1,1.3f,1},{0,0}},100,12};
        struct fv_plane button={{q_mul(root.q,rotation(-.1f,0,.2f)),{181,120,145},{1,.8f,1},{0,0}},120,15};
        struct reload_tf relative;
        unsigned rows=n%128+1;
        assert(fv_footer_transform(&content,&row,&root,&button,rows,11,&relative));
        qf_t desired=q_mul(content.q,relative.q), inverse={-root.q.x,-root.q.y,-root.q.z,root.q.w};
        qf_t correction=q_mul(desired,inverse), actual=q_mul(correction,button.transform.q);
        float dot=actual.x*row.transform.q.x+actual.y*row.transform.q.y+actual.z*row.transform.q.z+actual.w*row.transform.q.w;
        assert(fabsf(fabsf(dot)-1)<.00001f);
        float scale=fminf(row.width*row.transform.s[1]/(button.width*button.transform.s[1]),
                          row.height*row.transform.s[2]/(button.height*button.transform.s[2]));
        float origin_local[3],origin[3],offset[3],moved[3],tail_local[3]={0,0,-(float)rows*11*content.s[2]},tail[3];
        for(unsigned k=0;k<3;k++) {origin_local[k]=relative.p[k]*content.s[k];offset[k]=(button.transform.p[k]-root.p[k])*scale;}
        q_rot(content.q,origin_local,origin);q_rot(correction,offset,moved);q_rot(content.q,tail_local,tail);
        for(unsigned k=0;k<3;k++) {
            assert(fabsf(origin[k]+content.p[k]+moved[k]-row.transform.p[k]-tail[k])<.001f);
            assert(fabsf(relative.s[k]*content.s[k]-root.s[k]*scale)<.00001f);
        }
        assert(!fv_footer_transform(&content,&row,&root,&button,0,11,&relative));
        assert(!fv_footer_transform(&content,&row,&root,&button,129,11,&relative));
        assert(!fv_footer_transform(&content,&row,&root,&button,rows,NAN,&relative));
    }
    puts("PASS: footer first button follows actual native row scale/orientation at exact rows*spacing after the list across180 root/content rotations; invalid rows/spacing refused");
}
static void front_priority_checks(void) {
    struct reload_tf camera={{0,0,0,1},{17,-33,168},{1,1,1},{0,0}},widgets[4];
    const float positions[4][3]={{100,0,0},{99.7f,80,-40},{99.7f,-80,40},{98,0,0}};
    int priority[4];
    for (unsigned n=0;n<180;n++) {
        camera.q=rotation((float)n*.07f,.3f*sinf((float)n),.2f*cosf((float)n));
        for(unsigned i=0;i<4;i++) {
            widgets[i]=camera;float moved[3];q_rot(camera.q,positions[i],moved);
            for(unsigned k=0;k<3;k++)widgets[i].p[k]+=moved[k];
        }
        assert(fv_front_priorities(&camera,widgets,4,priority));
        assert(priority[0]==1 && priority[1]==2 && priority[2]==2 && priority[3]==4);
    }
    assert(!fv_front_priorities(&camera,widgets,0,priority));
    assert(!fv_front_priorities(&camera,widgets,65,priority));
    widgets[0].p[0]=NAN;assert(!fv_front_priorities(&camera,widgets,4,priority));
    widgets[0]=camera;assert(!fv_front_priorities(&camera,widgets,4,priority));
    puts("PASS: popup layer priorities preserve near-plane order despite farther button centres, equal planes and180 camera rotations; invalid/behind-camera inputs refused");
}
int main(void) {
    _Static_assert(sizeof(struct reload_tf)==48 && offsetof(struct reload_tf,s)==28,"native scalar FTransform");
    struct reload_tf camera={{0,0,0,1},{17,-33,168},{1,1,1},{0,0}};
    struct reload_tf root={{0,0,0,1},{100,0,100},{1,1,1},{0,0}};
    struct fv_plane planes[4];
    for(unsigned n=0;n<180;n++) {
        camera.q=rotation((float)n*.07f,.3f*sinf((float)n),.2f*cosf((float)n));
        root.q=rotation((float)n*.13f,.5f*sinf((float)n*.7f),.4f*cosf((float)n*.3f));
        root.s[0]=.25f+(float)(n%20)*.1f;
        root.s[1]=.4f+(float)(n%13)*.2f;root.s[2]=.3f+(float)(n%17)*.15f;
        for(unsigned i=0;i<4;i++) {
            planes[i]=(struct fv_plane){root,100,14};
            float at[3]={0,(i&1)?55.0f:-55.0f,(i&2)?30.0f:-30.0f},world[3];
            for(unsigned k=0;k<3;k++)at[k]*=root.s[k];
            q_rot(root.q,at,world);for(unsigned k=0;k<3;k++)planes[i].transform.p[k]+=world[k];
        }
        check_fit(camera,root,planes,1.0f+(float)(n%30)*.1f,30.0f+(float)(n%100));
    }
    struct reload_tf output;
    assert(!fv_frame_transform(&camera,&root,planes,0,2,90,&output));
    assert(!fv_frame_transform(&camera,&root,planes,65,2,90,&output));
    assert(!fv_frame_transform(&camera,&root,planes,4,NAN,90,&output));
    assert(!fv_frame_transform(&camera,&root,planes,4,2,INFINITY,&output));
    planes[1].width=NAN;assert(!fv_frame_transform(&camera,&root,planes,4,2,90,&output));planes[1].width=100;
    planes[1].transform.p[0]+=10000;assert(!fv_frame_transform(&camera,&root,planes,4,2,90,&output));
#ifdef FLAT_VIEW_FRAGMENT
    frame_binding_checks();
#ifdef WIDGET_FRAME_FRAGMENT
    widget_plane_checks();
#endif
#ifdef WIZARD_FRAME_FRAGMENT
    wizard_checks();
#endif
#endif
    projection_checks();mesh_plane_checks();
    footer_geometry_checks();
    front_priority_checks();
    puts("PASS: production UI framing, 180 camera/root rotations/scales/FOV/aspects; all corners fit, no tilt, stable repeated framing, invalid/nonplanar bounds rejected");
}
