#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef struct {float x,y,z,w;} qf_t;
struct __attribute__((aligned(16))) reload_tf {qf_t q;float p[3],s[3],padding[2];};
static qf_t q_mul(qf_t a,qf_t b) {
    return (qf_t){a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
                  a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
static void q_rot(qf_t q,const float v[3],float o[3]) {
    float d=q.x*v[0]+q.y*v[1]+q.z*v[2],u=q.x*q.x+q.y*q.y+q.z*q.z,k=q.w*q.w-u;
    o[0]=2*d*q.x+k*v[0]+2*q.w*(q.y*v[2]-q.z*v[1]);
    o[1]=2*d*q.y+k*v[1]+2*q.w*(q.z*v[0]-q.x*v[2]);
    o[2]=2*d*q.z+k*v[2]+2*q.w*(q.x*v[1]-q.y*v[0]);
}
static int reload_marker_valid(const struct reload_tf* t) {
    float n=t->q.x*t->q.x+t->q.y*t->q.y+t->q.z*t->q.z+t->q.w*t->q.w;
    if(!isfinite(n)||fabsf(n-1)>.01f)return 0;
    for(unsigned i=0;i<3;i++)if(!isfinite(t->p[i])||!isfinite(t->s[i])||t->s[i]<=0)return 0;
    return 1;
}
static int reload_tf_valid(const struct reload_tf* t) {return reload_marker_valid(t)&&t->s[0]==1&&t->s[1]==1&&t->s[2]==1;}
#include "../re4fix_ui_frame.h"
struct cinema_ui_weak {int index,serial;};
static unsigned char objects[13][0x600],classes[13][0x100],properties[8][0x60];
static const char* names[13];
static void* (*fv_button_class)(void);
static int (*fv_ui_enabled)(const void*);
static void (*cu_weak_assign)(struct cinema_ui_weak*,const void*);
static void* (*cu_weak_get)(const struct cinema_ui_weak*);
static unsigned (*fv_widget_planes)(void*,struct fv_plane*,unsigned);
static int index_of(const void* p) {for(int i=0;i<13;i++)if(p==objects[i])return i;return -1;}
static int pv_alive(const void* p) {int i=index_of(p);return i>=0&&*(int*)(objects[i]+8)==1;}
static int menu_visual_layout(const void* p,const void* cls,int minimum) {(void)minimum;return pv_alive(p)&&*(const int*)((const char*)p+4)==(int)(size_t)cls;}
static int cinema_ui_named(const void* p,const char* name,int minimum) {
    int i=index_of(p);return pv_alive(p)&&names[i]&&!strcmp(name,names[i])&&*(int*)(classes[i]+0x58)>=minimum;
}
static const char* cinema_field_property(const void* p,const char* name,const char* kind) {
    static const char* fields[]={"TrifoldBaseBP","UIScreen","DoorRightScreen","CurrentMenu","Scroll","ButtonList","EntrySpacing","Header Widget"};
    static const int owners[]={0,1,1,0,3,0,0,2};
    for(unsigned i=0;i<8;i++)if(index_of(p)==owners[i]&&!strcmp(name,fields[i])) {
        assert(!strcmp(kind,i==5?"ArrayProperty":i==6?"FloatProperty":"ObjectProperty"));return (const char*)properties[i];
    }
    return 0;
}
static int fv_frame_owned(void* root,void* screen) {
    return pv_alive(root)&&pv_alive(screen)&&*(void**)((char*)root+0xa0)==screen&&*(void**)((char*)screen+0x130)==root;
}
static int fv_descends_from(void* part,const void* root) {return pv_alive(part)&&*(void**)((char*)part+0xc0)==root;}
static void obj_name(const void* p,char* out,int size) {
    const char* value=p==objects[8]?"VR4UIButton_Confirm":"ButtonButton";
    assert(size>(int)strlen(value));memcpy(out,value,strlen(value)+1);
}
#define LOGF(...) ((void)0)
#include "../re4fix_ui_footer.h"
static unsigned attaches,unattaches,restores,finds,updates;
static int attached,scroll_member_count;
static void* scroll_members[3];
static struct reload_tf original={{0,0,0,1},{0,0,0},{1,1,1},{0,0}},last_relative;
static void weak_assign(struct cinema_ui_weak* ref,const void* object) {ref->index=index_of(object);ref->serial=7;assert(ref->index>=0);}
static void* weak_get(const struct cinema_ui_weak* ref) {return ref->index>=0&&ref->index<13&&ref->serial==7&&pv_alive(objects[ref->index])?objects[ref->index]:0;}
static void* button_class(void) {return (void*)2;}
static void* scroll_class(void) {return (void*)4;}
static int enabled(const void* part) {
    void* owner=*(void* const*)((const char*)part+0xa0);
    return *(const unsigned char*)((const char*)part+0x264) && owner && pv_alive(owner)
        && (*(const unsigned short*)((const char*)part+0x14c)&0x20) && !(*(const unsigned*)((const char*)owner+0x58)&0x20);
}
static void update(void* part) {assert(part==objects[4]);updates++;}
static void attach(void* scroll,void* actor,const struct reload_tf* tf) {
    assert(scroll==objects[4]&&actor==objects[2]&&!attached);attaches++;attached=1;last_relative=*tf;
}
static void unattach(void* scroll,void* actor) {assert(scroll==objects[4]&&actor==objects[2]&&attached);unattaches++;attached=0;}
static int find(void* scroll,int* index,void* actor) {
    assert(scroll==objects[4]);finds++;
    for(int i=0;i<scroll_member_count;i++)if(scroll_members[i]==actor){*index=i;return 1;}
    return 0;
}
static struct reload_tf scroll_world(const void* scroll) {assert(scroll==objects[4]);return (struct reload_tf){{0,0,0,1},{200,0,160},{1,1,1},{0,0}};}
static struct reload_tf relative_transform(const void* root) {assert(root==objects[7]);return original;}
static void set_relative(void* root,const struct reload_tf* tf,int sweep,void* hit,int teleport) {
    assert(root==objects[7]&&!memcmp(tf,&original,sizeof original)&&!sweep&&!hit&&!teleport);restores++;
}
static void ready(void) {
    memset(objects,0,sizeof objects);memset(classes,0,sizeof classes);memset(properties,0,sizeof properties);
    memset(names,0,sizeof names);memset(&fv_footer,0,sizeof fv_footer);
    fv_footer.screen=fv_footer.footer=fv_footer.scroll=fv_footer.root=fv_footer.parent=(struct cinema_ui_weak){-1,0};
    for(int i=0;i<13;i++){*(int*)(objects[i]+8)=1;*(void**)(objects[i]+0x10)=classes[i];*(int*)(classes[i]+0x58)=0x600;}
    names[0]="OptionsUIScreen_BP_C";names[1]="UI_PauseMenu_Content_Options_BP_C";
    names[2]="OptionsConfirmScreen_BP_C";names[3]="OptionsScreen_Menu_C";
    names[5]=names[10]=names[11]="OptionsScreen_Button_C";
    static const int offsets[]={0x448,0x370,0x398,0x2e8,0x250,0x450,0x2f0,0x258};
    for(unsigned i=0;i<8;i++){*(int*)(properties[i]+0x34)=1;*(int*)(properties[i]+0x38)=i==5?16:i==6?4:8;*(int*)(properties[i]+0x4c)=offsets[i];}
    *(void**)(objects[2]+0x258)=objects[11];
    *(void**)(objects[0]+0x448)=objects[1];*(void**)(objects[1]+0x370)=objects[0];*(void**)(objects[1]+0x398)=objects[2];
    *(void**)(objects[0]+0x2e8)=objects[3];*(void**)(objects[3]+0x250)=objects[4];*(void**)(objects[4]+0xa0)=objects[3];*(int*)(objects[4]+4)=4;
    static void* rows[3];rows[0]=objects[5];rows[1]=objects[10];rows[2]=objects[11];
    struct {void** data;int count,max;} list={rows,3,3};memcpy(objects[0]+0x450,&list,sizeof list);*(float*)(objects[0]+0x2f0)=11;
    static void* row_controls[1],*footer_controls[1];row_controls[0]=objects[6];footer_controls[0]=objects[8];
    list=(__typeof__(list)){row_controls,1,1};memcpy(objects[0]+0x238,&list,sizeof list);
    list=(__typeof__(list)){footer_controls,1,1};memcpy(objects[2]+0x238,&list,sizeof list);
    *(int*)(objects[6]+4)=*(int*)(objects[8]+4)=2;objects[6][0x264]=objects[8][0x264]=1;
    *(unsigned short*)(objects[4]+0x14c)=*(unsigned short*)(objects[6]+0x14c)=*(unsigned short*)(objects[8]+0x14c)=0x20;
    *(void**)(objects[6]+0x308)=objects[0];*(void**)(objects[6]+0xa0)=objects[5];
    *(void**)(objects[8]+0x308)=objects[2];*(void**)(objects[8]+0xa0)=objects[2];*(void**)(objects[8]+0xc0)=objects[7];
    *(void**)(objects[7]+0xa0)=objects[2];*(void**)(objects[2]+0x130)=objects[7];*(void**)(objects[7]+0xc0)=objects[9];
    struct reload_tf row={{0,0,0,1},{200,0,160},{1,1,1},{0,0}},footer={{0,0,0,1},{200,100,160},{1,1,1},{0,0}};
    memcpy(objects[6]+0x1c0,&row,sizeof row);memcpy(objects[7]+0x1c0,&footer,sizeof footer);memcpy(objects[8]+0x1c0,&footer,sizeof footer);
    float dims[2]={100,12};memcpy(objects[6]+0x318,dims,8);memcpy(objects[8]+0x318,dims,8);
    fv_button_class=button_class;fv_ui_enabled=enabled;cu_weak_assign=weak_assign;cu_weak_get=weak_get;
    fv_scroll_class=scroll_class;fv_scroll_attach=attach;fv_scroll_unattach=unattach;fv_scroll_find=find;
    fv_scroll_world=scroll_world;fv_relative_transform=relative_transform;fv_set_relative=set_relative;
    attaches=unattaches=restores=finds=updates=0;attached=0;fv_scroll_update=update;fv_widget_planes=0;
    scroll_members[0]=objects[5];scroll_members[1]=objects[10];scroll_members[2]=0;scroll_member_count=2;
}
static void hidden_leading_row(void) {
    ready();scroll_members[0]=objects[11];scroll_members[1]=objects[5];scroll_members[2]=objects[10];scroll_member_count=3;
    *(unsigned*)(objects[11]+0x58)=0x20;
    *(int*)(objects[12]+4)=2;objects[12][0x264]=1;*(unsigned short*)(objects[12]+0x14c)=0x20;
    *(void**)(objects[12]+0xa0)=objects[11];*(void**)(objects[12]+0x308)=objects[0];
    assert(objects[12][0x264]==1&&!enabled(objects[12]));
    static void* controls[2];controls[0]=objects[12];controls[1]=objects[6];
    struct {void** data;int count,max;} list={controls,2,2};memcpy(objects[0]+0x238,&list,sizeof list);
    struct reload_tf row=scroll_world(objects[4]);row.p[2]-=11;memcpy(objects[6]+0x1c0,&row,sizeof row);
    unsigned char before[sizeof objects];memcpy(before,objects,sizeof objects);
    fv_footer_tick(objects[0]);assert(attaches==1&&fv_footer.rows==3&&cu_weak_get(&fv_footer.row)==objects[5]);
    assert(last_relative.p[2]==-33&&!memcmp(before,objects,sizeof objects));
    fv_footer_tick(0);assert(unattaches==1&&restores==1&&!memcmp(before,objects,sizeof objects));
    *(unsigned*)(objects[10]+0x58)=0x20;memcpy(before,objects,sizeof objects);
    fv_footer_tick(objects[0]);assert(attaches==2&&fv_footer.rows==3&&last_relative.p[2]==-33&&!memcmp(before,objects,sizeof objects));
    fv_footer_tick(0);assert(unattaches==2&&restores==2&&!memcmp(before,objects,sizeof objects));
    ready();*(unsigned*)(objects[5]+0x58)=*(unsigned*)(objects[10]+0x58)=0x20;
    memcpy(before,objects,sizeof objects);fv_footer_tick(objects[0]);assert(!attaches&&!memcmp(before,objects,sizeof objects));
    ready();objects[6][0x264]=0;*(int*)(objects[12]+4)=2;objects[12][0x264]=1;
    *(unsigned short*)(objects[12]+0x14c)=0x20;*(void**)(objects[12]+0xa0)=objects[10];*(void**)(objects[12]+0x308)=objects[0];
    controls[0]=objects[12];controls[1]=objects[6];memcpy(objects[0]+0x238,&list,sizeof list);
    assert(enabled(objects[12]));fv_footer_tick(objects[0]);assert(!attaches);
    puts("PASS: hidden leading Scroll row with raw Enabled=1/native IsEnabled=0 is retained in the count; earliest visible actor anchors footer exactly after the final native index including hidden tail rows, with no extra gap; all-hidden and visible-without-eligible-control remain refused without native visibility writes");
}
static void visible_window_checks(void) {
    ready();struct fv_plane plane;
    objects[4][0x264]=1;float dims[2]={150,56};memcpy(objects[4]+0x318,dims,8);
    struct reload_tf content=scroll_world(objects[4]);memcpy(objects[4]+0x1c0,&content,sizeof content);
    *(void**)(objects[6]+0x310)=objects[4];
    assert(!fv_scroll_plane(objects[6],&plane));
    assert(fv_scroll_plane(objects[4],&plane)&&plane.width==150&&plane.height==56&&!memcmp(&plane.transform,&content,sizeof content));
    assert(fv_scroll_plane(objects[8],&plane));
    assert(fv_footer_control_visible(objects[6],objects[4]));
    struct reload_tf outside=content;outside.p[2]+=29;
    memcpy(objects[6]+0x1c0,&outside,sizeof outside);assert(!fv_footer_control_visible(objects[6],objects[4]));
    outside.p[2]=content.p[2];outside.p[1]+=76;
    memcpy(objects[6]+0x1c0,&outside,sizeof outside);assert(!fv_footer_control_visible(objects[6],objects[4]));
    memcpy(objects[6]+0x1c0,&content,sizeof content);
    *(void**)(objects[6]+0x310)=objects[7];assert(!fv_footer_control_visible(objects[6],objects[4]));
    *(void**)(objects[6]+0x310)=objects[4];objects[6][0x264]=0;
    assert(!fv_footer_control_visible(objects[6],objects[4]));objects[6][0x264]=1;
    objects[4][0x264]=0;assert(!fv_scroll_plane(objects[4],&plane));objects[4][0x264]=1;
    *(void**)(objects[0]+0x130)=objects[9];*(void**)(objects[9]+0xa0)=objects[0];
    *(void**)(objects[4]+0xc0)=objects[9];*(void**)(objects[4]+0x308)=objects[0];
    *(int*)(objects[11]+4)=4;*(void**)(objects[11]+0xc0)=objects[9];*(void**)(objects[11]+0x308)=objects[2];
    static void* controls[3];controls[0]=objects[4];controls[1]=objects[6];controls[2]=objects[11];
    struct {void** data;int count,max;} list={controls,3,3};memcpy(objects[0]+0x238,&list,sizeof list);
    fv_update_scrollers(objects[0],objects[9]);assert(updates==1);
    *(void**)(objects[4]+0xc0)=objects[7];fv_update_scrollers(objects[0],objects[9]);assert(updates==1);
    *(void**)(objects[4]+0xc0)=objects[9];list.count=list.max=513;memcpy(objects[0]+0x238,&list,sizeof list);
    fv_update_scrollers(objects[0],objects[9]);assert(updates==1);
    puts("PASS: actual native Scroll window replaces logical/offscreen row bounds; immediate attached-actor refresh validates screen registration, scene ancestry, root owner and array bounds");
}
int main(void) {
    ready();fv_footer_tick(objects[0]);assert(attaches==1&&fv_footer.rows==2&&last_relative.p[2]==-22&&finds==3);
    fv_footer_tick(objects[0]);assert(attaches==1&&finds==3);
    fv_footer_tick(0);assert(unattaches==1&&restores==1&&!attached);
    fv_footer_tick(0);assert(unattaches==1&&restores==1);
    ready();names[1]="UI_PauseMenu_Content_NewGame_BP_C";names[2]="NewGameConfirmScreen_BP_C";fv_footer_tick(objects[0]);assert(attaches==1);
    ready();names[1]="UI_PauseMenu_Content_GameOptions_BP_C";fv_footer_tick(objects[0]);assert(attaches==1);
    ready();*(void**)(objects[1]+0x370)=objects[3];fv_footer_tick(objects[0]);assert(!attaches);
    ready();*(void**)(objects[4]+0xa0)=objects[1];fv_footer_tick(objects[0]);assert(!attaches);
    ready();*(int*)(properties[5]+0x38)=8;fv_footer_tick(objects[0]);assert(!attaches);
    ready();*(void**)(objects[8]+0x308)=objects[0];fv_footer_tick(objects[0]);assert(!attaches);
    ready();*(void**)(objects[8]+0x310)=objects[1];fv_footer_tick(objects[0]);assert(!attaches&&!unattaches);
    ready();fv_footer_tick(objects[0]);*(int*)(objects[4]+8)=0;fv_footer_tick(0);assert(!unattaches&&restores==1);
    ready();fv_footer_tick(objects[0]);*(int*)(objects[7]+8)=0;fv_footer_tick(0);assert(unattaches==1&&!restores);
    ready();fv_footer_tick(objects[0]);*(void**)(objects[7]+0xc0)=objects[10];fv_footer_tick(0);assert(unattaches==1&&!restores);
    ready();fv_footer_tick(objects[0]);struct reload_tf before=last_relative;
    objects[2][0x298]=1;objects[11][0x14c]=0x20;fv_footer_tick(objects[0]);
    assert(attaches==1&&!unattaches&&!memcmp(&last_relative,&before,sizeof before));
    objects[2][0x298]=0;objects[11][0x14c]=0;fv_footer_tick(objects[0]);
    assert(attaches==1&&!unattaches&&!memcmp(&last_relative,&before,sizeof before));
    visible_window_checks();
    hidden_leading_row();
    puts("PASS: actual footer C uses native Scroll membership, excludes other menus, appends existing authored actor once, accepts exact Options/NewGame owners, validates property/owner/registration and weak cleanup, restores relative transform only to unchanged parent");
}
