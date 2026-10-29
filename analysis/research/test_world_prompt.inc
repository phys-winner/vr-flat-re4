/* Host-only dependencies for the actual world prompt callbacks and traversal. */
#include <math.h>
typedef struct {float x,y,z,w;} qf_t;
struct reload_tf {qf_t q;float p[3],s[3],padding[2];};
static void q_rot(qf_t q,const float v[3],float o[3]) {
    float d=q.x*v[0]+q.y*v[1]+q.z*v[2],k=q.w*q.w-q.x*q.x-q.y*q.y-q.z*q.z;
    o[0]=2*d*q.x+k*v[0]+2*q.w*(q.y*v[2]-q.z*v[1]);
    o[1]=2*d*q.y+k*v[1]+2*q.w*(q.z*v[0]-q.x*v[2]);
    o[2]=2*d*q.z+k*v[2]+2*q.w*(q.x*v[1]-q.y*v[0]);
}
static qf_t q_mul(qf_t a,qf_t b){assert(a.w==1&&b.w==1);return a;}
static qf_t pose_camera_delta_ue(void){return(qf_t){0,0,0,1};}
static struct reload_tf head(void* object){assert(object==pawn);return(struct reload_tf){{0,0,0,1},{0,0,0},{1,1,1},{0,0}};}
static struct reload_tf(*aim_head)(void*)=head;
static int reload_tf_valid(const struct reload_tf* tf){return tf&&tf->q.w==1;}
static unsigned g_comp_w=2400,g_comp_h=1080;
static float g_fov_h_deg=90,g_prompt_sx,g_prompt_sy;
static long g_prompt_seen_ms;
static int g_touch_context=1,wt_mx,wt_button_count;
static void* wt_buttons[64];
static unsigned wt_logs,world_visibility_calls,command_calls,level_calls;
static unsigned long long g_world_tap;
static int shown(const void* actor){assert(actor==floater);return 1;}
static int(*wt_floater_displaying)(const void*)=shown;
static int(*wt_can_press)(const void*);
static void(*wt_activate)(void*);
static int pthread_mutex_lock(int* mutex){(void)mutex;return 0;}
static int pthread_mutex_unlock(int* mutex){(void)mutex;return 0;}
static int interact_is_a(const void* object,const void* cls){return cinema_object(object,cls,0x28);}
static void obj_name(const void* object,char* out,int capacity) {
    const char* name=object==prompt_image ? "ButtonIcon_ControllerGlyph":object==prompt_caption ? "Caption":"Widget";
    assert((int)strlen(name)<capacity);memcpy(out,name,strlen(name)+1);
}
static void widget_visibility(void* object,unsigned char visibility){assert(object==prompt_image);*(unsigned char*)((char*)object+0xc3)=visibility;++world_visibility_calls;}
static void gate_floater_cmd(void* self,const void* text,int command,int action,int qte,int size) {
    assert(self==floater&&text==prompt_caption&&command==2&&action==7&&qte==1&&size==3);++command_calls;
}
static void gate_floater_level(void* self,const void* text,int command) {
    assert(self==floater&&text==prompt_caption&&command==4);++level_calls;
}
static void wt_grab_tick(void* hand,long now){(void)hand;(void)now;}
static int touch_sce_tap(float x,float y,long now){(void)x;(void)y;(void)now;return 0;}
static int touch_throw_tap(void* hand,const struct reload_tf* camera,float x,float y,long now){(void)hand;(void)camera;(void)x;(void)y;(void)now;return 0;}
static int wt_range_grab(void* hand,const struct reload_tf* camera,float x,float y,long now){(void)hand;(void)camera;(void)x;(void)y;(void)now;return 0;}
static int wt_owner_name(const void* component,char* name,int capacity){(void)component;(void)capacity;name[0]=0;return 0;}
/* WORLD_FUNCTIONS */
static void* prompt_user_class(void){return prompt_classes[0];}
static void* prompt_panel_class(void){return prompt_classes[1];}
static void check_world_prompt_artwork(void) {
    setup();
    for(unsigned i=0;i<3;i++) {
        memset(prompt_classes[i],0,0x80);prompt_chains[i][0]=prompt_classes[i]+0x30;
        *(void**)(prompt_classes[i]+0x30)=prompt_chains[i];*(int*)(prompt_classes[i]+0x58)=0x230;
    }
    *(void**)(prompt_component+0x10)=classes[8];*(uint64_t*)(classes[8]+0x18)=77;
    link_component(prompt_component,floater,root);children[2]=prompt_component;*(int*)(root+0xd8)=*(int*)(root+0xdc)=3;
    struct reload_tf widget={{0,0,0,1},{100,0,0},{1,1,1},{0,0}};memcpy(prompt_component+0x1c0,&widget,sizeof widget);
    *(void**)(prompt_component+0x498)=prompt_user;*(void**)(prompt_user+0x10)=prompt_classes[0];
    *(void**)(prompt_user+0x1a8)=prompt_tree;*(void**)(prompt_tree+0x28)=prompt_panel;
    *(void**)(prompt_panel+0x10)=prompt_classes[1];*(void**)(prompt_image+0x10)=prompt_classes[2];*(void**)(prompt_caption+0x10)=prompt_classes[2];
    void* slots[2]={prompt_slots[0],prompt_slots[1]};struct{void**data;int count,max;} list={slots,2,2};memcpy(prompt_panel+0x108,&list,sizeof list);
    *(void**)(prompt_slots[0]+0x30)=prompt_image;*(void**)(prompt_slots[1]+0x30)=prompt_caption;
    prompt_image[0xc3]=3;prompt_caption[0xc3]=0;wt_set_visibility=widget_visibility;
    wt_user_class=prompt_user_class;wt_panel_class=prompt_panel_class;wt_tree_dumps=0;
    unsigned char before_image[sizeof prompt_image],before_caption[sizeof prompt_caption];
    memcpy(before_image,prompt_image,sizeof before_image);memcpy(before_caption,prompt_caption,sizeof before_caption);
    world_visibility_calls=command_calls=level_calls=0;
    for(unsigned step=0;step<3;step++) {
        hide_hints=step==1;
        hook_floater_cmd(floater,prompt_caption,2,7,1,3);
        hook_floater_level(floater,prompt_caption,4);
        for(unsigned frame=0;frame<6;frame++) {
            touch_world_tick(pawn,0,1000+step*100+frame);
            touch_prompt_visual_tick();
            assert(!memcmp(before_image,prompt_image,sizeof before_image)&&!memcmp(before_caption,prompt_caption,sizeof before_caption));
            if(hide_hints)assert_masked();else assert_original();
        }
    }
    assert(command_calls==3&&level_calls==3&&wt_tree_dumps==3&&!world_visibility_calls);
    assert(g_prompt_seen_ms==1205&&g_prompt_sx==1200&&g_prompt_sy==540);
    puts("PASS: actual world prompt callbacks/tick + optional native-mesh masking Off/On/Off,18 authored frames/three read-only UMG dumps; named button glyph/caption bytes and command arguments unchanged, prompt tap projection retained");
}
