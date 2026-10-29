#define main ordinary_interact_tests
#include "test_interact.c"
#undef main

static int phone_inspect_ready=1;
static void phone_inspect_fname(const void* value,char* out,int size) {
    const char* name=*(const unsigned long long*)value==0xb1d1?"BidirectionalLever_StaticModel_BP1_C":"OtherClass";
    assert((int)strlen(name)<size);memcpy(out,name,strlen(name)+1);
}
static void* (*sce_lever_class)(void),*(*sce_crank_class)(void),*(*sce_model_class)(void),*(*sce_base_class)(void);
static int sce_ready,sce_best_kind,sce_logs;static void* sce_best,*sce_crank;
static long sce_best_ms,sce_crank_last,sce_crank_started;static float sce_best_sx,sce_best_sy;
static void (*sce_activate_switch)(void*),(*sce_crank_grab)(void*),(*sce_crank_release)(void*);
static float (*sce_crank_ratio)(const void*);
static void* dead_object;
static int pv_alive(const void* p){return p && p!=dead_object;}
static int tw_typed(const void* p,const void* cls,int minimum){return interact_layout(p,minimum)&&interact_is_a(p,cls);}
static float g_comp_h=1080;
static struct {
    unsigned char owner[0x1400],primary[0x400],secondary[0x300];
    unsigned char model_class[0x100],base_class[0x100],child_class[0x100];
    unsigned char lever_class[0x100],sce_class[0x100],crank_class[0x100],fields[2][0x60];
    void* chains[6][3];int activations;
} bd;
static const char* cinema_field_property(const void* p,const char* name,const char* kind) {
    assert(p==bd.owner && !strcmp(kind,"ObjectProperty"));
    return !strcmp(name,"VR4Lever")?(const char*)bd.fields[0]:!strcmp(name,"SecondarySceInteraction")?(const char*)bd.fields[1]:0;
}
#include BIDIRECTIONAL_C_HELPERS
static void* lever_class(void){return bd.lever_class;}static void* crank_class(void){return bd.crank_class;}
static void* model_class(void){return bd.model_class;}static void* sce_class(void){return bd.sce_class;}
static void activate(void* c){assert(c==bd.primary);bd.activations++;bd.primary[0x25a]=1;}
static void setup_class(unsigned char* cls,int depth,int bytes,void** chain) {
    *(int*)(cls+0x38)=depth;*(int*)(cls+0x58)=bytes;*(void**)(cls+0x30)=chain;
}
static void bidirectional_ready(void) {
    world_ready();ti_door_class=0;memset(&bd,0,sizeof bd);dead_object=0;
    bd.chains[0][0]=bd.model_class+0x30;
    bd.chains[1][0]=bd.model_class+0x30;bd.chains[1][1]=bd.base_class+0x30;
    memcpy(bd.chains[2],bd.chains[1],sizeof bd.chains[2]);bd.chains[2][2]=bd.child_class+0x30;
    setup_class(bd.model_class,0,0x300,bd.chains[0]);setup_class(bd.base_class,1,0x1400,bd.chains[1]);setup_class(bd.child_class,2,0x1400,bd.chains[2]);
    *(void**)(bd.base_class+0x40)=bd.model_class;*(void**)(bd.child_class+0x40)=bd.base_class;
    *(unsigned long long*)(bd.base_class+0x18)=0xb1d1;*(unsigned long long*)(bd.child_class+0x18)=0x11c;
    unsigned char* classes[]={bd.lever_class,bd.sce_class,bd.crank_class};
    for(int i=0;i<3;i++){bd.chains[i+3][0]=classes[i]+0x30;setup_class(classes[i],0,0x400,bd.chains[i+3]);}
    *(void**)(bd.owner+0x10)=bd.child_class;*(void**)(bd.primary+0x10)=bd.lever_class;*(void**)(bd.secondary+0x10)=bd.sce_class;
    *(void**)(bd.primary+0x20)=*(void**)(bd.primary+0xa0)=bd.owner;
    *(void**)(bd.secondary+0x20)=*(void**)(bd.secondary+0xa0)=bd.owner;
    for(int i=0;i<2;i++) {
        *(int*)(bd.fields[i]+0x34)=1;*(int*)(bd.fields[i]+0x38)=8;*(int*)(bd.fields[i]+0x4c)=0x600+8*i;
    }
    *(void**)(bd.owner+0x600)=bd.primary;*(void**)(bd.owner+0x608)=bd.secondary;
    *(void**)(f.parent+0x20)=*(void**)(f.parent+0xa0)=bd.owner;
    bd.primary[0x258]=1;phone_inspect_ready=1;
    sce_ready=1;sce_best=bd.primary;sce_best_kind=1;sce_best_ms=clock_now;
    sce_crank=0;sce_lever_class=lever_class;sce_crank_class=crank_class;sce_model_class=model_class;sce_base_class=sce_class;
    sce_activate_switch=activate;interact_sce_hook=touch_sce_interact;
}
int main(int argc,char** argv) {
    ordinary_interact_tests(argc,argv);
    bidirectional_ready();request();
    assert(!bd.activations && presses==1 && held==f.grip && ti_hand==f.left && get_down(f.left));
    request();assert(releases==1 && !held && !ti_hand && !bd.activations);
    bidirectional_ready();assert(!touch_sce_tap(0,0,clock_now) && !bd.activations);
    // All original face commands continue to run before the physical grip.
    bidirectional_ready();action=can_action=1;f.manager[0x3d9]=1;f.manager[0x3da]=1;request();
    assert(g_cmd_until[0]>clock_now && !bd.activations && !presses);
    // The native simple lever action is unchanged for every other owner family.
    bidirectional_ready();*(unsigned long long*)(bd.base_class+0x18)=0x9999;request();
    assert(bd.activations==1 && !presses && !ti_hand);
    // Exact family alone is insufficient: both reflected native component identities must match.
    for(int mode=0;mode<9;mode++) {
        bidirectional_ready();
        if(mode==0)*(void**)(bd.owner+0x600)=bd.secondary;else if(mode==1)*(void**)(bd.owner+0x608)=bd.primary;
        else if(mode==2)*(int*)(bd.fields[0]+0x34)=2;else if(mode==3)*(int*)(bd.fields[1]+0x38)=4;
        else if(mode==4)*(int*)(bd.fields[0]+0x4c)=0x13fc;else if(mode==5)*(void**)(bd.secondary+0xa0)=f.other;
        else if(mode==6)*(void**)(bd.secondary+0x10)=bd.lever_class;else if(mode==7)dead_object=bd.secondary;
        else *(void**)(bd.primary+0x20)=f.other;
        assert(!sce_bidirectional(bd.primary));
    }
    bidirectional_ready();native_grip_valid=0;request();assert(!bd.activations && !presses);
    bidirectional_ready();pickup_wall=1;request();assert(!bd.activations && !presses);
    puts("PASS: actual SCE filter/request/tap code yields the exact two-ended lever to original grip press/release, preserves face/normal-lever routes and native LOS/validity");
    return 0;
}
