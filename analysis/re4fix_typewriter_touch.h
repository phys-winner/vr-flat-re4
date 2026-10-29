// The original typewriter uses physical finger collisions, not menu-hand UI rays.
// A still touch presses only a key registered by the active pawn's own TheatreBox.
#ifndef RE4FIX_TYPEWRITER_TOUCH_H
#define RE4FIX_TYPEWRITER_TOUCH_H
#include <stdint.h>
#include <pthread.h>
static int tt_ready,tt_bound;
static void* tt_event_address;
static struct reload_tf (*tt_socket)(const void*,unsigned long long,int);
static struct cinema_ui_weak tt_pawn={-1,0},tt_theatre={-1,0},tt_camera={-1,0};
static pthread_mutex_t tt_mutex=PTHREAD_MUTEX_INITIALIZER;
static struct {
    unsigned generation,down_generation,pending_generation;
    int down,handle,moved,pending;
    float x,y,width,height,u,v;
    long started,stamp,queued;
} tt_input;
static unsigned tt_logs;
static struct cinema_ui_weak tt_animated_key={-1,0},tt_animated_theatre={-1,0};
static long tt_animation_started;
static float tt_animation_amount;
static struct {struct cinema_ui_weak theatre;int remaining;long started;} tt_scroll;

static void touch_typewriter_cancel(void) {
    pthread_mutex_lock(&tt_mutex);
    unsigned generation=tt_input.generation+1;if(!generation)generation=1;
    memset(&tt_input,0,sizeof tt_input);tt_input.generation=generation;
    pthread_mutex_unlock(&tt_mutex);
}
// Input thread. The caller already validated the UE TouchInput event. Never reads a UObject.
static int touch_typewriter_event(int handle,int phase,float x,float y,float width,float height,long now) {
    if(__atomic_load_n(&g_touch_context,__ATOMIC_ACQUIRE)!=6)return 0;
    pthread_mutex_lock(&tt_mutex);
    now=ptr_now_ms();
    int valid=tt_ready && __atomic_load_n(&g_app_input_focus,__ATOMIC_ACQUIRE)
        && tt_input.stamp>0 && now>=tt_input.stamp && now-tt_input.stamp<=300
        && isfinite(x)&&isfinite(y)&&isfinite(width)&&isfinite(height)
        && width>=64&&height>=64&&width<=16384&&height<=16384
        && x>=0&&y>=0&&x<=width&&y<=height;
    if(!valid) {tt_input.down=tt_input.pending=0;pthread_mutex_unlock(&tt_mutex);return 1;}
    if(phase==0 && !tt_input.down) {
        tt_input.down=1;tt_input.handle=handle;tt_input.moved=0;
        tt_input.x=x;tt_input.y=y;tt_input.width=width;tt_input.height=height;
        tt_input.started=now;tt_input.down_generation=tt_input.generation;
    } else if(tt_input.down && tt_input.handle==handle && (phase==1||phase==2)) {
        float dx=x-tt_input.x,dy=y-tt_input.y;
        if(dx*dx+dy*dy>30*30 || width!=tt_input.width || height!=tt_input.height)tt_input.moved=1;
        if(phase==2) {
            if(!tt_input.moved && !tt_input.pending && tt_input.down_generation==tt_input.generation
                && now>=tt_input.started&&now-tt_input.started<1500) {
                tt_input.pending=1;tt_input.pending_generation=tt_input.generation;
                tt_input.u=x/width;tt_input.v=y/height;tt_input.queued=now;
            }
            tt_input.down=0;
        }
    }
    pthread_mutex_unlock(&tt_mutex);return 1;
}
static int tt_class(const void* object,const char* wanted,int minimum) {
    if(!pv_alive(object))return 0;
    const char* cls=*(const char* const*)((const char*)object+0x10);
    int bytes=cls?*(const int*)(cls+0x58):0;
    if(bytes<minimum||bytes>0x10000)return 0;
    for(unsigned depth=0;cls&&depth<16;depth++) {
        char name[96];phone_inspect_fname(cls+0x18,name,sizeof name);
        if(!strcmp(name,wanted))return 1;
        const char* next=*(const char* const*)(cls+0x40);if(next==cls)break;cls=next;
    }
    return 0;
}
static const char* tt_property(const void* object,const char* name,const char* kind,int bytes) {
    if(!pv_alive(object))return 0;
    const char* cls=*(const char* const*)((const char*)object+0x10);
    int size=cls?*(const int*)(cls+0x58):0;
    const char* field=cinema_field_property(object,name,kind);
    int offset=field?*(const int*)(field+0x4c):-1;
    return field&&size>=0x220&&size<=0x10000&&offset>=0x28&&offset<=size-bytes
        &&*(const int*)(field+0x34)==1&&*(const int*)(field+0x38)==bytes?field:0;
}
static void* tt_object(const void* object,const char* name) {
    const char* field=tt_property(object,name,"ObjectProperty",8);
    return field?*(void* const*)((const char*)object+*(const int*)(field+0x4c)):0;
}
static int tt_child(void* actor,void* owner) {
    // AActor::ParentComponent is a native FWeakObjectPtr, even when the cooked
    // asset's object-reference tag is serialized as ObjectProperty. Preserve its serial.
    const char* field=tt_property(actor,"ParentComponent","WeakObjectProperty",8);
    struct cinema_ui_weak weak={-1,0};
    if(field)memcpy(&weak,(const char*)actor+*(const int*)(field+0x4c),sizeof weak);
    void* parent=field&&cu_weak_get?cu_weak_get(&weak):0;
    return tt_class(parent,"ChildActorComponent",0x200)
        &&*(void* const*)((const char*)parent+0x20)==owner&&tt_object(parent,"ChildActor")==actor;
}
static void* tt_overwrite_key(void* theatre) {
    // RegisterKeys deliberately excludes this separate original key from AllKeys.
    void* key=tt_object(theatre,"OverwriteKey");
    return pv_alive(key)&&cinema_ui_named(key,"TypewriterKeyOverwrite_C",0x220)&&tt_child(key,theatre)?key:0;
}
static void* tt_function(void* object,const char* wanted,const char* parameter,int bytes,int output) {
    int attachment=parameter&&!strcmp(parameter,"KeyAttach");
    const char* cls=*(const char* const*)((const char*)object+0x10);
    for(unsigned depth=0;cls&&depth<16;depth++) {
        const char* fn=*(const char* const*)(cls+0x48);
        for(unsigned i=0;fn&&i<256;i++) {
            char name[160];phone_inspect_fname(fn+0x18,name,sizeof name);
            if(!strcmp(name,wanted)) {
                // These Blueprint getters have named OUT parameters, not C++ return values.
                unsigned short result=0xffff;
                if(!pv_alive(fn)||!cinema_ui_named(fn,"Function",0xc0)
                    ||*(const void* const*)(fn+0x20)!=cls
                    ||*(const unsigned char*)(fn+0xb4)!=(attachment?2:bytes?1:0)
                    ||*(const unsigned short*)(fn+0xb6)!=bytes||*(const unsigned short*)(fn+0xb8)!=result)return 0;
                if(!bytes)return (void*)fn;
                const char* field=*(const char* const*)(fn+0x50);unsigned parms=0;
                for(unsigned k=0;field&&k<32;k++) {
                    uint64_t flags=*(const uint64_t*)(field+0x40);
                    if(flags&0x80) {
                        char kind[48];phone_inspect_fname(*(const void* const*)(field+8),kind,sizeof kind);
                        char pn[48];phone_inspect_fname(field+0x28,pn,sizeof pn);++parms;
                        int offset=attachment?(parms==1?0:8):0,size=attachment?8:bytes;
                        const char* expected=attachment?(parms==1?"ObjectProperty":"NameProperty"):parameter;
                        if(parms>(attachment?2u:1u)||strcmp(kind,expected)||*(const int*)(field+0x34)!=1
                            ||*(const int*)(field+0x38)!=size||*(const int*)(field+0x4c)!=offset
                            ||(attachment&&strcmp(pn,parms==1?"ParentComponent":"ParentSocket"))
                            ||(flags&0x400)||!!(flags&0x100)!=!!output)return 0;
                        if(!strcmp(parameter,"BoolProperty") && (*(const unsigned char*)(field+0x79)
                            ||!(*(const unsigned char*)(field+0x7b)&1)))return 0;
                    }
                    const char* next=*(const char* const*)(field+0x20);if(next==field)break;field=next;
                }
                return parms==(attachment?2u:1u)?(void*)fn:0;
            }
            const char* next=*(const char* const*)(fn+0x28);if(next==fn)break;fn=next;
        }
        const char* next=*(const char* const*)(cls+0x40);if(next==cls)break;cls=next;
    }
    return 0;
}
static int tt_call(void* object,void* function,void* params) {
    void** table=object?*(void***)object:0;
    if(!function||!table||table[0x218/sizeof(void*)]!=tt_event_address)return 0;
    ((void(*)(void*,void*,void*))tt_event_address)(object,function,params);return 1;
}
// Use the original AnimBP key writer without triggering its input delegate a second time.
static int tt_key_visual(void* key,float amount) {
    const char* method=tt_class(key,"TypeWriterKeyBase_Translating_C",0x220)?"SetAnimBPKeyTranslation"
        :tt_class(key,"TypeWriterKeyBase_Rotating_C",0x220)?"SetAnimBPKeyAngle":0;
    return method&&tt_call(key,tt_function(key,method,"FloatProperty",4,0),&amount);
}
static void tt_key_animation_clear(void) {
    if(tt_animation_started) {
        void* key=cu_weak_get(&tt_animated_key),*theatre=cu_weak_get(&tt_animated_theatre);
        if(key&&theatre&&tt_class(key,"TypewriterKeyBase_C",0x220)&&tt_child(key,theatre))tt_key_visual(key,0);
    }
    tt_animation_started=0;
}
static void tt_key_animation(long now) {
    if(!tt_animation_started)return;
    void* key=cu_weak_get(&tt_animated_key),*theatre=cu_weak_get(&tt_animated_theatre);
    if(!key||theatre!=cu_weak_get(&tt_theatre)||!tt_class(key,"TypewriterKeyBase_C",0x220)||!tt_child(key,theatre)) {
        tt_animation_started=0;return;
    }
    float phase=(float)(now-tt_animation_started)/180.0f;
    if(phase<0)phase=1;if(phase>1)phase=1;
    float amount=tt_animation_amount*(1-phase*phase*(3-2*phase));
    tt_key_visual(key,amount);if(phase==1)tt_animation_started=0;
}
static void tt_key_animation_begin(void* key,void* theatre,long now) {
    const char* property=tt_property(key,tt_class(key,"TypeWriterKeyBase_Rotating_C",0x220)
        ?"KeyPressThresholdDegrees":"KeyPressThreshold","FloatProperty",4);
    float amount=0;if(property)memcpy(&amount,(const char*)key+*(const int*)(property+0x4c),4);
    if(!isfinite(amount)||amount<=0||amount>100)return;
    tt_key_animation_clear();
    if(tt_key_visual(key,amount)) {
        cu_weak_assign(&tt_animated_key,key);cu_weak_assign(&tt_animated_theatre,theatre);
        tt_animation_amount=amount;tt_animation_started=now;
    }
}
// UE final FMinimalViewInfo: Location0, RotationC, horizontal FOV18, AspectRatio2C.
static int tt_view_ray(const void* view,float u,float v,float width,float height,float eye[3],float ray[3]) {
    if(!view||!isfinite(u)||!isfinite(v)||u<0||u>1||v<0||v>1||width<64||height<64)return 0;
    float rotation[3],fov,aspect;memcpy(eye,view,12);memcpy(rotation,(const char*)view+0xc,12);
    memcpy(&fov,(const char*)view+0x18,4);memcpy(&aspect,(const char*)view+0x2c,4);
    for(unsigned k=0;k<3;k++)if(!isfinite(eye[k])||!isfinite(rotation[k]))return 0;
    if(!isfinite(fov)||fov<1||fov>130||*(const unsigned char*)((const char*)view+0x31))return 0;
    float w=width,h=height,x=u*width,y=v*height;
    if(*(const unsigned char*)((const char*)view+0x30)&1) {
        if(!isfinite(aspect)||aspect<1||aspect>4)return 0;
        if(width/height>aspect){w=height*aspect;x-=(width-w)*.5f;}
        else{h=width/aspect;y-=(height-h)*.5f;}
        if(x<0||x>w||y<0||y>h)return 0;
    }
    float pitch=rotation[0]*.0174532925199433f,yaw=rotation[1]*.0174532925199433f,roll=rotation[2]*.0174532925199433f;
    float cp=cosf(pitch),sp=sinf(pitch),cy=cosf(yaw),sy=sinf(yaw),cr=cosf(roll),sr=sinf(roll);
    float f[3]={cp*cy,cp*sy,sp},r[3]={sr*sp*cy-cr*sy,sr*sp*sy+cr*cy,-sr*cp};
    float top[3]={-cr*sp*cy-sr*sy,cy*sr-cr*sp*sy,cr*cp};
    float horizontal=(2*x/w-1)*tanf(fov*.00872664625997165f),vertical=(1-2*y/h)*tanf(fov*.00872664625997165f)*h/w,norm=0;
    for(unsigned k=0;k<3;k++){ray[k]=f[k]+r[k]*horizontal+top[k]*vertical;norm+=ray[k]*ray[k];}
    if(!isfinite(norm)||norm<.01f)return 0;norm=sqrtf(norm);for(unsigned k=0;k<3;k++)ray[k]/=norm;return 1;
}
static float tt_box(const float o[3],const float d[3],const float extent[3]) {
    float near=0,far=400;
    for(unsigned k=0;k<3;k++) {
        if(!isfinite(extent[k])||extent[k]<=0||extent[k]>1000)return -1;
        if(fabsf(d[k])<1e-7f){if(fabsf(o[k])>extent[k])return -1;continue;}
        float a=(-extent[k]-o[k])/d[k],b=(extent[k]-o[k])/d[k];if(a>b){float t=a;a=b;b=t;}
        near=fmaxf(near,a);far=fminf(far,b);if(near>far)return -1;
    }
    return near;
}
static float tt_capsule(const float o[3],const float d[3],float radius,float half) {
    if(!isfinite(radius)||!isfinite(half)||radius<=0||half<radius||half>1000)return -1;
    float h=half-radius,best=401,a=d[0]*d[0]+d[1]*d[1],b=o[0]*d[0]+o[1]*d[1],c=o[0]*o[0]+o[1]*o[1]-radius*radius;
    if(a>1e-10f) {
        float discriminant=b*b-a*c;
        if(discriminant>=0)for(unsigned sign=0;sign<2;sign++) {
            float t=(-b+(sign?1:-1)*sqrtf(discriminant))/a,z=o[2]+t*d[2];
            if(t>=0&&t<best&&fabsf(z)<=h)best=t;
        }
    }
    for(unsigned side=0;side<2;side++) {
        float z=o[2]+(side?h:-h),aa=a+d[2]*d[2],bb=b+z*d[2],cc=c+z*z,disc=bb*bb-aa*cc;
        if(aa>1e-10f&&disc>=0)for(unsigned sign=0;sign<2;sign++) {
            float t=(-bb+(sign?1:-1)*sqrtf(disc))/aa;if(t>=0&&t<best)best=t;
        }
    }
    return best<=400?best:-1;
}
static int tt_local(void* component,void* owner,const float eye[3],const float ray[3],float o[3],float d[3]) {
    if(!pv_alive(component)||!mv_scene_class||!menu_visual_layout(component,mv_scene_class(),0x1f0)
        ||*(void* const*)((const char*)component+0x20)!=owner||*(void* const*)((const char*)component+0xa0)!=owner)return 0;
    struct reload_tf tf;memcpy(&tf,(const char*)component+0x1c0,sizeof tf);if(!reload_marker_valid(&tf))return 0;
    qf_t inverse={-tf.q.x,-tf.q.y,-tf.q.z,tf.q.w};float delta[3],turned[3];
    for(unsigned k=0;k<3;k++)delta[k]=eye[k]-tf.p[k];q_rot(inverse,delta,turned);
    for(unsigned k=0;k<3;k++)o[k]=turned[k]/tf.s[k];q_rot(inverse,ray,turned);
    for(unsigned k=0;k<3;k++)d[k]=turned[k]/tf.s[k];return 1;
}
static float tt_shape(void* component,void* owner,const float eye[3],const float ray[3]) {
    float o[3],d[3];if(!tt_local(component,owner,eye,ray,o,d))return -1;
    if(tt_class(component,"BoxComponent",0x430)) {
        const char* field=tt_property(component,"BoxExtent","StructProperty",12);float extent[3];if(!field)return -1;
        memcpy(extent,(const char*)component+*(const int*)(field+0x4c),12);return tt_box(o,d,extent);
    }
    if(tt_class(component,"CapsuleComponent",0x430)) {
        const char* r=tt_property(component,"CapsuleRadius","FloatProperty",4),*h=tt_property(component,"CapsuleHalfHeight","FloatProperty",4);
        if(!r||!h)return -1;float radius,half;memcpy(&radius,(const char*)component+*(const int*)(r+0x4c),4);memcpy(&half,(const char*)component+*(const int*)(h+0x4c),4);
        return tt_capsule(o,d,radius,half);
    }
    return -1;
}
// Finger acquisition capsules sit above the key caps. The original hierarchy's
// SurfaceSocket follows the rendered key bone, including the keyboard opening.
static float tt_key_surface(void* key,void* theatre,void* collision,const float eye[3],const float ray[3],float* score) {
    float checked_o[3],checked_d[3];if(!tt_local(collision,key,eye,ray,checked_o,checked_d))return -1;
    struct {void* component;unsigned long long socket;} attach={0};
    void* fn=tt_function(key,"GetAttachPoint","KeyAttach",16,1);
    if(!tt_socket||!tt_call(key,fn,&attach)||!attach.socket)return -1;
    void* mesh=tt_object(theatre,"TypewriterMesh");
    if(attach.component!=mesh||!pv_alive(mesh)||!mv_skeletal_class
        ||!menu_visual_layout(mesh,mv_skeletal_class(),0x520)
        ||*(void* const*)((const char*)mesh+0x20)!=theatre
        ||*(void* const*)((const char*)mesh+0xa0)!=theatre
        ||*(struct reload_tf (**)(const void*,unsigned long long,int))((const char*)*(const void* const*)mesh+0x430)!=tt_socket)return -1;
    struct reload_tf surface=tt_socket(mesh,attach.socket,0);
    if(!reload_marker_valid(&surface))return -1;
    const char* normal_field=tt_property(key,"LocalKeySurfaceNormal","StructProperty",12);float normal[3];
    if(!normal_field)return -1;memcpy(normal,(const char*)key+*(const int*)(normal_field+0x4c),12);
    float norm=0;for(unsigned k=0;k<3;k++)norm+=normal[k]*normal[k];
    if(!isfinite(norm)||fabsf(norm-1)>.01f||fabsf(normal[2])<.99f)return -1;
    qf_t inverse={-surface.q.x,-surface.q.y,-surface.q.z,surface.q.w};float delta[3],o[3],d[3];
    for(unsigned k=0;k<3;k++)delta[k]=eye[k]-surface.p[k];q_rot(inverse,delta,o);q_rot(inverse,ray,d);
    for(unsigned k=0;k<3;k++){o[k]/=surface.s[k];d[k]/=surface.s[k];}
    if(fabsf(d[2])<1e-7f)return -1;float t=-o[2]/d[2];if(!isfinite(t)||t<0||t>400)return -1;
    float x=o[0]+t*d[0],y=o[1]+t*d[1],distance;
    if(tt_class(collision,"CapsuleComponent",0x430)) {
        const char* field=tt_property(collision,"CapsuleHalfHeight","FloatProperty",4);float reach=0;
        if(field)memcpy(&reach,(const char*)collision+*(const int*)(field+0x4c),4);
        if(!isfinite(reach)||reach<=0||reach>10)return -1;
        // Keep the original finger target's full reach on the flat cap plane.
        // Adjacent targets are resolved by distance to their own surface centre.
        distance=(x*x+y*y)/(reach*reach);
    } else if(tt_class(collision,"BoxComponent",0x430)) {
        const char* field=tt_property(collision,"BoxExtent","StructProperty",12);float extent[3];
        if(!field)return -1;memcpy(extent,(const char*)collision+*(const int*)(field+0x4c),12);
        if(!isfinite(extent[0])||!isfinite(extent[1])||extent[0]<=0||extent[1]<=0||extent[0]>30||extent[1]>30)return -1;
        distance=fmaxf(x*x/(extent[0]*extent[0]),y*y/(extent[1]*extent[1]));
    } else return -1;
    if(!isfinite(distance)||distance>1)return -1;*score=distance;return t;
}
static int tt_state(void* theatre) {
    const char* field=tt_property(theatre,"currentState","ByteProperty",1);
    return field?*((const unsigned char*)theatre+*(const int*)(field+0x4c)):-1;
}
static int tt_scroll_active(void* theatre) {
    const char* field=tt_property(theatre,"ScrollActive","BoolProperty",1);
    return !field||!*(const unsigned char*)(field+0x7b)
        ||(*((const unsigned char*)theatre+*(const int*)(field+0x4c))&*(const unsigned char*)(field+0x7b));
}
static int tt_list_state(void* theatre) {int state=tt_state(theatre);return state==6||state==7||state==12||state==13;}
static void* tt_dial_hand(void* theatre) {
    void* state=tt_object(theatre,"HandState_LH");
    if(!tt_class(state,"TypewriterHandState_C",0x28)||*(void* const*)((const char*)state+0x20)!=theatre)return 0;
    const char* cls=*(const char* const*)((const char*)state+0x10);
    int bytes=*(const int*)(cls+0x58);const char* field=cinema_field_property(state,"Hand","ObjectProperty");
    int offset=field?*(const int*)(field+0x4c):-1;
    if(!field||bytes>0x100||offset<0x28||offset>bytes-8||*(const int*)(field+0x34)!=1||*(const int*)(field+0x38)!=8)return 0;
    void* hand=*(void* const*)((const char*)state+offset);
    if(!tt_class(hand,"VR4GamePlayerHand",0x510)||!rl_player||rl_player(hand)!=cu_weak_get(&tt_pawn))return 0;
    void* components[]={tt_object(hand,"HandRoot"),tt_object(theatre,"DummyDial"),tt_object(theatre,"IkRotatorDial")};
    for(unsigned i=0;i<3;i++) {
        void* component=components[i],*owner=i?theatre:hand;struct reload_tf transform;
        if(!pv_alive(component)||!mv_scene_class||!menu_visual_layout(component,mv_scene_class(),0x1f0)
            ||*(void* const*)((const char*)component+0x20)!=owner||*(void* const*)((const char*)component+0xa0)!=owner)return 0;
        memcpy(&transform,(const char*)component+0x1c0,sizeof transform);if(!reload_marker_valid(&transform))return 0;
    }
    return hand;
}
static void tt_scroll_tick(void* theatre,long now) {
    if(!tt_scroll.remaining)return;
    if(!__atomic_load_n(&g_app_input_focus,__ATOMIC_ACQUIRE)
        ||cu_weak_get(&tt_scroll.theatre)!=theatre||(!tt_list_state(theatre)&&tt_state(theatre)!=0)
        ||now<tt_scroll.started||now-tt_scroll.started>2000){tt_scroll.remaining=0;return;}
    if(tt_scroll_active(theatre)||tt_state(theatre)==0)return;
    void* hand=tt_dial_hand(theatre);
    void* grab=tt_function(theatre,"BndEvt__DialPlayerGrip_K2Node_ComponentBoundEvent_5_GripHandEventMulticastDelegate__DelegateSignature","ObjectProperty",8,0);
    void* release=tt_function(theatre,"BndEvt__DialPlayerGrip_K2Node_ComponentBoundEvent_6_GripHandEventMulticastDelegate__DelegateSignature","ObjectProperty",8,0);
    void* scroll=tt_function(theatre,"ScrollList","BoolProperty",1,0);
    if(!hand||!grab||!release||!scroll){tt_scroll.remaining=0;return;}
    unsigned char forward=tt_scroll.remaining>0;
    // Replay the original dial grab/scroll/release. Release sets native state 0;
    // the original tick waits for ScrollActive then chooses empty/existing slot.
    int moved=tt_call(theatre,grab,&hand)&&cu_weak_get(&tt_scroll.theatre)==theatre&&tt_state(theatre)==5
        &&tt_call(theatre,scroll,&forward);
    if(cu_weak_get(&tt_scroll.theatre)==theatre)tt_call(theatre,release,&hand);
    if(moved)tt_scroll.remaining+=forward?-1:1;else tt_scroll.remaining=0;
}
// The original list renders five 32-unit rows, with the selected slot in row 2.
// ScrollList performs the native animation, wrapping and state transition.
static int tt_list_target(void* theatre,const float eye[3],const float ray[3],long now) {
    if(!tt_list_state(theatre))return 0;
    void* widget=tt_object(theatre,"WidgetList");float o[3],d[3];
    if(!pv_alive(widget)||!cu_component_class||!menu_visual_layout(widget,cu_component_class(),0x519)
        ||(*(const unsigned short*)((const char*)widget+0x14c)&0x420)!=0x20
        ||*(const unsigned char*)((const char*)widget+0x429)||*(const unsigned char*)((const char*)widget+0x518)
        ||!tt_local(widget,theatre,eye,ray,o,d)||!fv_current_draw_size||fabsf(d[0])<1e-7f)return 0;
    struct cinema_ui_vec2 size=fv_current_draw_size(widget);float pivot[2];memcpy(pivot,(const char*)widget+0x45c,8);
    if(!isfinite(size.x)||size.x<=0||size.x>16384||size.y!=160
        ||!isfinite(pivot[0])||!isfinite(pivot[1])||pivot[0]<0||pivot[0]>1||pivot[1]<0||pivot[1]>1)return 0;
    float t=-o[0]/d[0],x=pivot[0]*size.x-(o[1]+t*d[1]),y=pivot[1]*size.y-(o[2]+t*d[2]);
    if(!isfinite(t)||t<0||t>400||!isfinite(x)||!isfinite(y)||x<0||x>=size.x||y<0||y>=size.y)return 0;
    if(tt_scroll.remaining||tt_scroll_active(theatre))return 1;
    tt_scroll.remaining=(int)(y/32)-2;tt_scroll.started=now;cu_weak_assign(&tt_scroll.theatre,theatre);
    tt_scroll_tick(theatre,now);return 1;
}
static float tt_cancel_target(void* theatre,const float eye[3],const float ray[3]) {
    void* widget=tt_object(theatre,"CancelWidget");float o[3],d[3];
    if(pv_alive(widget)&&cu_component_class&&menu_visual_layout(widget,cu_component_class(),0x519)
        &&(*(const unsigned short*)((const char*)widget+0x14c)&0x420)==0x20
        &&tt_local(widget,theatre,eye,ray,o,d)&&fv_current_draw_size) {
        struct cinema_ui_vec2 size=fv_current_draw_size(widget);float pivot[2];memcpy(pivot,(const char*)widget+0x45c,8);
        if(isfinite(size.x)&&isfinite(size.y)&&size.x>0&&size.y>0&&size.x<=16384&&size.y<=16384
            &&isfinite(pivot[0])&&isfinite(pivot[1])&&pivot[0]>=0&&pivot[0]<=1&&pivot[1]>=0&&pivot[1]<=1) {
            o[1]-=(pivot[0]-.5f)*size.x;o[2]-=(pivot[1]-.5f)*size.y;
            float extent[3]={.5f,size.x*.5f,size.y*.5f},hit=tt_box(o,d,extent);if(hit>=0)return hit;
        }
    }
    void* bell=tt_object(theatre,"BellActor"),*mesh=tt_object(bell,"Type_CancelBell_01");
    if(!tt_class(bell,"Typewriter_Bell_C",0x220)||!pv_alive(mesh)||!mv_scene_class
        ||!menu_visual_layout(mesh,mv_scene_class(),0x1f0)||*(void* const*)((const char*)mesh+0x20)!=bell
        ||(*(const unsigned short*)((const char*)mesh+0x14c)&0x420)!=0x20)return -1;
    return tt_shape(tt_object(bell,"PressCollision"),bell,eye,ray);
}
enum {TT_FRAME_POINTS=(64+1+4)*8};
struct tt_frame_bounds {float lo[3],hi[3],points[TT_FRAME_POINTS][3],facing[3];unsigned count;int aligned;};
static int tt_frame_add(struct tt_frame_bounds* bounds,const struct reload_tf* transform,
                        const float origin[3],const float extent[3]) {
    if(!bounds||!reload_marker_valid(transform)||bounds->count>TT_FRAME_POINTS-8||bounds->count%8)return 0;
    for(unsigned k=0;k<3;k++)if(!isfinite(origin[k])||!isfinite(extent[k])||fabsf(origin[k])>16384
        ||extent[k]<0||extent[k]>16384||transform->s[k]>1000)return 0;
    for(unsigned c=0;c<8;c++) {
        float local[3],world[3];
        for(unsigned k=0;k<3;k++)local[k]=(origin[k]+(c&(1u<<k)?extent[k]:-extent[k]))*transform->s[k];
        q_rot(transform->q,local,world);
        for(unsigned k=0;k<3;k++) {
            world[k]+=transform->p[k];if(!isfinite(world[k])||fabsf(world[k])>10000000)return 0;
            bounds->points[bounds->count][k]=world[k];
            if(!bounds->count)bounds->lo[k]=bounds->hi[k]=world[k];
            else{bounds->lo[k]=fminf(bounds->lo[k],world[k]);bounds->hi[k]=fmaxf(bounds->hi[k],world[k]);}
        }
        bounds->count++;
    }
    return 1;
}
static int tt_frame_owned(void* component,void* owner,struct reload_tf* transform) {
    if(!pv_alive(component)||!mv_scene_class||!menu_visual_layout(component,mv_scene_class(),0x1f0)
        ||*(void* const*)((const char*)component+0x20)!=owner||*(void* const*)((const char*)component+0xa0)!=owner)return 0;
    memcpy(transform,(const char*)component+0x1c0,sizeof *transform);return reload_marker_valid(transform);
}
static int tt_frame_shape(struct tt_frame_bounds* bounds,void* component,void* owner) {
    struct reload_tf transform;float extent[3],origin[3]={0};
    if(!tt_frame_owned(component,owner,&transform))return 0;
    if(tt_class(component,"BoxComponent",0x430)) {
        const char* field=tt_property(component,"BoxExtent","StructProperty",12);if(!field)return 0;
        memcpy(extent,(const char*)component+*(const int*)(field+0x4c),12);
        for(unsigned k=0;k<3;k++)if(!isfinite(extent[k])||extent[k]<=0||extent[k]>1000)return 0;
    } else if(tt_class(component,"CapsuleComponent",0x430)) {
        const char* r=tt_property(component,"CapsuleRadius","FloatProperty",4),*h=tt_property(component,"CapsuleHalfHeight","FloatProperty",4);
        if(!r||!h)return 0;
        memcpy(extent,(const char*)component+*(const int*)(r+0x4c),4);
        memcpy(extent+2,(const char*)component+*(const int*)(h+0x4c),4);extent[1]=extent[0];
        if(!isfinite(extent[0])||!isfinite(extent[2])||extent[0]<=0||extent[2]<extent[0]||extent[2]>1000)return 0;
    } else return 0;
    return tt_frame_add(bounds,&transform,origin,extent);
}
// Use the rendered original quad: DrawAtDesiredSize changes it independently of requested DrawSize.
static int tt_frame_widget(struct tt_frame_bounds* bounds,void* component,void* owner) {
    if(!component)return 0;
    struct reload_tf transform;
    if(!cu_component_class||!fv_current_draw_size||!tt_frame_owned(component,owner,&transform)
        ||!menu_visual_layout(component,cu_component_class(),0x519))return -1;
    if((*(const unsigned short*)((const char*)component+0x14c)&0x420)!=0x20)return 0;
    if(*(const unsigned char*)((const char*)component+0x429)||*(const unsigned char*)((const char*)component+0x518))return -1;
    struct cinema_ui_vec2 size=fv_current_draw_size(component);float pivot[2];memcpy(pivot,(const char*)component+0x45c,8);
    if(!isfinite(size.x)||!isfinite(size.y)||size.x<0||size.y<0||size.x>16384||size.y>16384
        ||!isfinite(pivot[0])||!isfinite(pivot[1])||pivot[0]<0||pivot[0]>1||pivot[1]<0||pivot[1]>1)return -1;
    if(!size.x||!size.y)return 0;
    float origin[3]={0,(pivot[0]-.5f)*size.x,(pivot[1]-.5f)*size.y},extent[3]={0,size.x*.5f,size.y*.5f};
    return tt_frame_add(bounds,&transform,origin,extent)?1:-1;
}
static struct {
    struct cinema_ui_weak widget,theatre,parent;
    struct reload_tf relative;
    int active;
} tt_typing_widgets[3];
static void tt_typing_widget_restore(unsigned slot) {
    if(!tt_typing_widgets[slot].active)return;
    void* widget=cu_weak_get(&tt_typing_widgets[slot].widget),*theatre=cu_weak_get(&tt_typing_widgets[slot].theatre);
    void* parent=cu_weak_get(&tt_typing_widgets[slot].parent);struct reload_tf current;
    if(parent&&tt_frame_owned(widget,theatre,&current)&&*(void* const*)((const char*)widget+0xc0)==parent&&fv_set_relative)
        fv_set_relative(widget,&tt_typing_widgets[slot].relative,0,0,0);
    tt_typing_widgets[slot].active=0;
}
static void tt_typing_widgets_restore(void) {for(unsigned i=0;i<3;i++)tt_typing_widget_restore(i);}
// Keep the original text and Cancel controls readable without letting the remote VR paper shrink the keyboard.
static int tt_typing_widgets_place(void* theatre,const void* view) {
    if(!fv_relative_transform||!fv_set_relative||!rl_world_transform||!fv_current_draw_size||!cu_component_class)return 0;
    const char* names[]={"WidgetTitle","WidgetList","CancelWidget"};
    static const float frame[3][4]={{.5f,.055f,.55f,.055f},{.5f,.22f,.55f,.23f},{.90f,.87f,.12f,.10f}};
    void* widgets[3],*parents[3];struct reload_tf targets[3],originals[3];int capture[3];
    float rotation[3],eye[3],forward[3],right[3],up[3],aspect,fov;
    if(!tt_view_ray(view,.5f,.5f,g_comp_w,g_comp_h,eye,forward))return 0;
    memcpy(rotation,(const char*)view+0xc,12);memcpy(&fov,(const char*)view+0x18,4);
    aspect=(float)g_comp_w/(float)g_comp_h;
    if(*(const unsigned char*)((const char*)view+0x30)&1)memcpy(&aspect,(const char*)view+0x2c,4);
    float yaw=rotation[1]*.00872664625997165f,pitch=rotation[0]*.00872664625997165f;
    qf_t camera=q_mul((qf_t){0,0,sinf(yaw),cosf(yaw)},(qf_t){0,-sinf(pitch),0,cosf(pitch)});
    qf_t facing=q_mul(camera,(qf_t){0,0,1,0});
    q_rot(camera,(float[3]){0,1,0},right);q_rot(camera,(float[3]){0,0,1},up);
    float tx=tanf(fov*.00872664625997165f),ty=tx/aspect;
    for(unsigned i=0;i<3;i++) {
        widgets[i]=tt_object(theatre,names[i]);struct reload_tf current;
        if(!tt_frame_owned(widgets[i],theatre,&current)||!menu_visual_layout(widgets[i],cu_component_class(),0x519)
            ||*(const unsigned char*)((const char*)widgets[i]+0x429)||*(const unsigned char*)((const char*)widgets[i]+0x518))return 0;
        parents[i]=*(void* const*)((const char*)widgets[i]+0xc0);
        if(!pv_alive(parents[i])||!menu_visual_layout(parents[i],mv_scene_class(),0x1f0))return 0;
        struct cinema_ui_vec2 size=fv_current_draw_size(widgets[i]);float pivot[2];memcpy(pivot,(const char*)widgets[i]+0x45c,8);
        if(!isfinite(size.x)||!isfinite(size.y)||size.x<=0||size.y<=0||size.x>16384||size.y>16384
            ||!isfinite(pivot[0])||!isfinite(pivot[1])||pivot[0]<0||pivot[0]>1||pivot[1]<0||pivot[1]>1)return 0;
        float scale=fminf(80*tx*frame[i][2]/size.x,80*ty*frame[i][3]/size.y);
        targets[i]=current;targets[i].q=facing;
        for(unsigned k=0;k<3;k++)targets[i].s[k]=scale;
        float origin[3]={0,(pivot[0]-.5f)*size.x*scale,(pivot[1]-.5f)*size.y*scale},offset[3];q_rot(facing,origin,offset);
        for(unsigned k=0;k<3;k++)targets[i].p[k]=eye[k]+forward[k]*40+right[k]*(2*frame[i][0]-1)*40*tx
            +up[k]*(1-2*frame[i][1])*40*ty-offset[k];
        if(!reload_marker_valid(&targets[i]))return 0;
        capture[i]=!tt_typing_widgets[i].active||cu_weak_get(&tt_typing_widgets[i].widget)!=widgets[i]
            ||cu_weak_get(&tt_typing_widgets[i].theatre)!=theatre||cu_weak_get(&tt_typing_widgets[i].parent)!=parents[i];
        if(capture[i]){originals[i]=fv_relative_transform(widgets[i]);if(!reload_marker_valid(&originals[i]))return 0;}
    }
    for(unsigned i=0;i<3;i++) {
        if(capture[i]) {
            tt_typing_widget_restore(i);
            tt_typing_widgets[i].relative=originals[i];tt_typing_widgets[i].active=1;
            cu_weak_assign(&tt_typing_widgets[i].widget,widgets[i]);cu_weak_assign(&tt_typing_widgets[i].theatre,theatre);
            cu_weak_assign(&tt_typing_widgets[i].parent,parents[i]);
        }
        rl_world_transform(widgets[i],targets+i,0,0,0);
    }
    return 1;
}
// Look down onto the keyboard and fit the original paper/keys with the final eye only.
static int tt_frame_view(void* view,const struct tt_frame_bounds* bounds,float width,float height,int keyboard) {
    if(!view||!bounds||!bounds->count||bounds->count>TT_FRAME_POINTS||!isfinite(width)||!isfinite(height)||width<64||height<64||width>16384||height>16384)return 0;
    float eye[3],ray[3];if(!tt_view_ray(view,.5f,.5f,width,height,eye,ray))return 0;
    float aspect=width/height,fov;memcpy(&fov,(const char*)view+0x18,4);
    if(*(const unsigned char*)((const char*)view+0x30)&1)memcpy(&aspect,(const char*)view+0x2c,4);
    if(!isfinite(aspect)||aspect<1||aspect>4)return 0;
    float centre[3],delta[3];
    for(unsigned k=0;k<3;k++) {
        if(!isfinite(bounds->lo[k])||!isfinite(bounds->hi[k])||bounds->hi[k]<bounds->lo[k]||bounds->hi[k]-bounds->lo[k]>400)return 0;
        centre[k]=(bounds->lo[k]+bounds->hi[k])*.5f;delta[k]=centre[k]-eye[k];
    }
    float horizontal=hypotf(delta[0],delta[1]),distance=hypotf(horizontal,delta[2]);
    if(!isfinite(distance)||horizontal<.01f||distance<1||distance>400)return 0;
    float yaw=atan2f(delta[1],delta[0]),pitch=keyboard?-.872664625997165f:atan2f(delta[2],horizontal);
    if(bounds->aligned) {
        if(!isfinite(bounds->facing[0])||!isfinite(bounds->facing[1])||hypotf(bounds->facing[0],bounds->facing[1])<.5f)return 0;
        yaw=atan2f(bounds->facing[1],bounds->facing[0]);
    }
    float forward[3]={cosf(pitch)*cosf(yaw),cosf(pitch)*sinf(yaw),sinf(pitch)};
    for(unsigned k=0;k<3;k++)eye[k]=centre[k]-forward[k]*distance;
    float right[3]={-sinf(yaw),cosf(yaw),0},up[3]={-sinf(pitch)*cosf(yaw),-sinf(pitch)*sinf(yaw),cosf(pitch)};
    if(keyboard) {
        float tx=tanf(fov*.00872664625997165f),ty=tx/aspect,depth=4;
        for(unsigned c=0;c<bounds->count;c++) {
            float local[3],z=0,x=0,y=0;
            for(unsigned k=0;k<3;k++){local[k]=bounds->points[c][k]-centre[k];if(!isfinite(local[k]))return 0;
                z+=local[k]*forward[k];x+=local[k]*right[k];y+=local[k]*up[k];}
            depth=fmaxf(depth,fmaxf(4-z,fmaxf(fabsf(x)/(.55f*tx)-z,
                fmaxf((y/ty-.28f*z)/.58f,(-y/ty-.88f*z)/.58f))));
        }
        if(!isfinite(depth)||depth>300)return 0;
        for(unsigned k=0;k<3;k++)eye[k]=centre[k]-forward[k]*depth+up[k]*.30f*depth*ty;
        for(unsigned c=0;c<bounds->count;c++) {
            float squared=0;for(unsigned k=0;k<3;k++){float d=bounds->points[c][k]-eye[k];squared+=d*d;}
            if(!isfinite(squared)||squared>399*399)return 0;
        }
        float rotation[3]={pitch*57.2957795130823f,yaw*57.2957795130823f,0};
        memcpy(view,eye,12);memcpy((char*)view+0xc,rotation,12);return 1;
    }
    float tangent=tanf(fov*.00872664625997165f)*.90f,retreat=-200;
    for(unsigned c=0;c<bounds->count;c++) {
        float depth=0,x=0,y=0;
        for(unsigned k=0;k<3;k++) {
            if(!isfinite(bounds->points[c][k]))return 0;
            float d=bounds->points[c][k]-eye[k];
            depth+=d*forward[k];x+=d*right[k];y+=d*up[k];
        }
        retreat=fmaxf(retreat,fmaxf(4-depth,fmaxf(fabsf(x)/tangent,fabsf(y)*aspect/tangent)-depth));
    }
    if(!isfinite(retreat)||fabsf(retreat)>200)return 0;
    float location[3],rotation[3]={pitch*57.2957795130823f,yaw*57.2957795130823f,0};
    for(unsigned k=0;k<3;k++){location[k]=eye[k]-forward[k]*retreat;if(!isfinite(location[k])||fabsf(location[k])>10000000)return 0;}
    // Native key hit tests have a 400cm range; never frame a control outside that same range.
    for(unsigned c=0;c<bounds->count;c++) {
        float squared=0;
        for(unsigned k=0;k<3;k++){float d=bounds->points[c][k]-location[k];squared+=d*d;}
        if(!isfinite(squared)||squared>399*399)return 0;
    }
    memcpy(view,location,12);memcpy((char*)view+0xc,rotation,12);return 1;
}
static int tt_fit_theatre(void* theatre,void** keys,int count,void* view) {
    if(!tt_class(theatre,"TypewriterTheatreBox_C",0x220)||!keys||count<1||count>64)return 0;
    struct tt_frame_bounds bounds={0};int paper=0;
    struct reload_tf keyboard_transform;
    if(tt_frame_owned(tt_object(theatre,"TypewriterRoot"),theatre,&keyboard_transform)) {
        const float forward[3]={-1,0,0};q_rot(keyboard_transform.q,forward,bounds.facing);bounds.aligned=1;
    }
    const char* state=tt_property(theatre,"currentState","ByteProperty",1);
    int current=state?*((const unsigned char*)theatre+*(const int*)(state+0x4c)):-1;
    int keyboard=current==7||current==8||current==9||current==14; // original IsTypingAllowed states
    if(!keyboard)tt_typing_widgets_restore();
    const char* widgets[]={"WidgetTitle","WidgetList","WidgetOverwrite","CancelWidget"};
    for(unsigned i=0;i<sizeof widgets/sizeof *widgets;i++) {
        if(keyboard)continue;
        int included=tt_frame_widget(&bounds,tt_object(theatre,widgets[i]),theatre);if(included<0)return 0;
        if(i<2)paper+=included;
    }
    if(!keyboard&&!paper)return 0;
    for(int i=0;i<count;i++) {
        void* key=keys[i];
        // Every shipped derived getter returns this authored component, including Space and Enter.
        if(!tt_class(key,"TypewriterKeyBase_C",0x220)||!tt_child(key,theatre)
            ||!tt_frame_shape(&bounds,tt_object(key,"FingerMagnetCollision"),key))return 0;
    }
    void* overwrite=keyboard?0:tt_overwrite_key(theatre);
    if(overwrite&&!tt_frame_shape(&bounds,tt_object(overwrite,"FingerMagnetCollision"),overwrite))return 0;
    unsigned char framed_view[64];memcpy(framed_view,view,sizeof framed_view);
    int framed=tt_frame_view(framed_view,&bounds,g_comp_w,g_comp_h,keyboard);
    if(framed&&keyboard)framed=tt_typing_widgets_place(theatre,framed_view);
    if(!framed&&keyboard)tt_typing_widgets_restore();
    if(framed)memcpy(view,framed_view,sizeof framed_view);
    static unsigned logs;static int last_keyboard=-1;
    if(framed&&logs<8&&last_keyboard!=keyboard){logs++;last_keyboard=keyboard;float location[3],rotation[3];memcpy(location,view,12);memcpy(rotation,(const char*)view+0xc,12);
        LOGF("TYPEWRITER framed keyboard=%d keys=%d camera=(%.2f,%.2f,%.2f) rotation=(%.2f,%.2f,%.2f) native FOV retained",keyboard,count,
             location[0],location[1],location[2],rotation[0],rotation[1],rotation[2]);}
    return framed;
}
static void tt_hide_hands(void* pawn) {
    if(!mv_visible||!mv_skeletal_class||!mv_scene_class)return;
    void* meshset=tt_object(pawn,"MeshSet"),*pawn_root=tt_object(pawn,"RootComponent");
    if(!tt_class(meshset,"VR4GamePlayerMeshSet",0x238)||!pv_alive(pawn_root))return;
    void* root=tt_object(meshset,"MeshesRoot");
    struct reload_tf transform;
    if(!tt_frame_owned(root,meshset,&transform)||*(void* const*)((const char*)root+0xc0)!=pawn_root
        ||!tt_frame_owned(pawn_root,pawn,&transform))return;
    const char* names[]={"Mesh","SecondaryMesh"};
    for(unsigned i=0;i<2;i++) {
        void* mesh=tt_object(meshset,names[i]);
        if(!pv_alive(mesh)||!menu_visual_layout(mesh,mv_skeletal_class(),0x438)
            ||*(void* const*)((const char*)mesh+0x20)!=meshset||*(void* const*)((const char*)mesh+0xa0)!=meshset
            ||*(void* const*)((const char*)mesh+0xc0)!=root)continue;
        // Native PreBio4Tick restores this leaf visibility on return to gameplay.
        if(*(const unsigned short*)((const char*)mesh+0x14c)&0x20)mv_visible(mesh,0,0);
    }
}
static void touch_typewriter_view(void* camera,void* view) {
    if(!tt_ready||!cinema_on_game_thread())return;
    void* pawn=phone_active();
    if(phone_type()!=6||!tt_class(pawn,"VR4PlayerPawn_Typewriter_C",0x2e0)
        ||!pv_alive(camera)||!cinema_object(camera,cinema_camera_class(),0x7b8)
        ||*(void* const*)((const char*)pawn+0x2d8)!=camera||*(void* const*)((const char*)camera+0xa0)!=pawn) {
        if(tt_bound){tt_scroll.remaining=0;tt_typing_widgets_restore();tt_key_animation_clear();touch_typewriter_cancel();tt_bound=0;}return;
    }
    void* theatre=tt_object(pawn,"TheatreBox");
    const char* field=tt_class(theatre,"TypewriterTheatreBox_C",0x220)?tt_property(theatre,"AllKeys","ArrayProperty",16):0;
    struct {void** data;int count,max;} keys={0};
    if(field)memcpy(&keys,(const char*)theatre+*(const int*)(field+0x4c),sizeof keys);
    if(!field||keys.count<1||keys.count>64||keys.max<keys.count||keys.max>256||!keys.data) {
        if(tt_bound){tt_scroll.remaining=0;tt_typing_widgets_restore();tt_key_animation_clear();touch_typewriter_cancel();tt_bound=0;}return;
    }
    if(!tt_bound||cu_weak_get(&tt_pawn)!=pawn||cu_weak_get(&tt_theatre)!=theatre||cu_weak_get(&tt_camera)!=camera) {
        tt_scroll.remaining=0;tt_typing_widgets_restore();tt_key_animation_clear();touch_typewriter_cancel();cu_weak_assign(&tt_pawn,pawn);cu_weak_assign(&tt_theatre,theatre);cu_weak_assign(&tt_camera,camera);tt_bound=1;
    }
    tt_hide_hands(pawn);
    tt_scroll_tick(theatre,ptr_now_ms());
    tt_fit_theatre(theatre,keys.data,keys.count,view);
    tt_key_animation(ptr_now_ms());
    long now=ptr_now_ms();float u=0,v=0;int pending=0;
    pthread_mutex_lock(&tt_mutex);tt_input.stamp=now;
    if(tt_input.pending&&tt_input.pending_generation==tt_input.generation
        &&now>=tt_input.queued&&now-tt_input.queued<=500) {pending=1;u=tt_input.u;v=tt_input.v;}
    tt_input.pending=0;pthread_mutex_unlock(&tt_mutex);
    if(!pending||!__atomic_load_n(&g_app_input_focus,__ATOMIC_ACQUIRE))return;
    float eye[3],ray[3];if(!tt_view_ray(view,u,v,g_comp_w,g_comp_h,eye,ray))return;
    if(tt_list_target(theatre,eye,ray,now))return;
    void* selected=0;float best=tt_cancel_target(theatre,eye,ray);int cancel=best>=0;
    if(best<0)best=401;
    void* surface_selected=0;float surface_score=2,cancel_depth=best;
    for(int i=0;i<=keys.count;i++) {
        void* key=i<keys.count?keys.data[i]:tt_overwrite_key(theatre);
        if(!tt_class(key,"TypewriterKeyBase_C",0x220)||!tt_child(key,theatre))continue;
        void* enabled=tt_function(key,"IsKeyEnabled","BoolProperty",1,1);unsigned char allowed=0;
        if(!tt_call(key,enabled,&allowed)||!allowed)continue;
        void* collision=0,*get=tt_function(key,"GetFingerMagnetCollision","ObjectProperty",8,1);
        if(!tt_call(key,get,&collision))continue;
        float hit=tt_shape(collision,key,eye,ray);
        if(i<keys.count) {
            float score=2,surface=tt_key_surface(key,theatre,collision,eye,ray,&score);
            if(surface>=0&&surface<cancel_depth&&score<surface_score){surface_score=score;surface_selected=key;}
        }
        if(i==keys.count) {
            // The authored button is below its finger magnet. A flat tap can hit
            // its visible face while missing the VR finger acquisition volume.
            void* widget=tt_object(theatre,"WidgetOverwrite"),*button=tt_object(theatre,"OvewriteCollision");
            struct reload_tf transform;
            if(cu_component_class&&tt_frame_owned(widget,theatre,&transform)
                &&menu_visual_layout(widget,cu_component_class(),0x519)
                &&(*(const unsigned short*)((const char*)widget+0x14c)&0x420)==0x20
                &&tt_class(button,"BoxComponent",0x430)) {
                float face=tt_shape(button,theatre,eye,ray);
                if(face>=0&&(hit<0||face<hit))hit=face;
            }
        }
        if(hit>=0&&hit<best){best=hit;selected=key;cancel=0;}
    }
    if(surface_selected){selected=surface_selected;cancel=0;}
    if(cancel) {
        int accepted=tt_call(theatre,tt_function(theatre,"OnRing",0,0,0),0);
        if(tt_logs<40){tt_logs++;LOGF("TYPEWRITER native Cancel accepted=%d",accepted);}return;
    }
    if(selected) {
        char name[96];obj_name(selected,name,sizeof name);
        void* function=tt_function(selected,"SetKeyActivated","BoolProperty",1,0);
        struct cinema_ui_weak key={-1,0};cu_weak_assign(&key,selected);unsigned char pressed=1;
        int accepted=tt_call(selected,function,&pressed);
        // The original press may change pawn or destroy the keyboard. Resolve its weak identity again.
        void* live=cu_weak_get(&key);
        if(accepted&&live==selected&&cu_weak_get(&tt_theatre)==theatre
            &&tt_class(live,"TypewriterKeyBase_C",0x220)&&tt_child(live,theatre)) {
            pressed=0;tt_call(live,tt_function(live,"SetKeyActivated","BoolProperty",1,0),&pressed);
            tt_key_animation_begin(live,theatre,ptr_now_ms());
        }
        if(tt_logs<40){tt_logs++;LOGF("TYPEWRITER native key '%s' accepted=%d",name,accepted);}
    } else if(tt_logs<40){tt_logs++;LOGF("TYPEWRITER tap missed owned key u=%.3f v=%.3f",u,v);}
}
static void install_touch_typewriter(uintptr_t base) {
    static const struct{unsigned long at;unsigned words[2];} checks[]={
        {0x87037cc,{0xa9bd57f6,0xa9014ff4}},
        {0x63380dc,{0xd0020168,0xf9431508}},
        {0x6338aec,{0xd0020168,0xf9431508}},
        {0x83c6310,{0xd10643ff,0xfd0083ee}},
    };
    tt_ready=0;
    if(!base||!phone_active||!phone_type||!phone_inspect_ready||!cu_weak_assign||!cu_weak_get
        ||!cinema_camera_class||!mv_scene_class)return;
    for(unsigned i=0;i<sizeof checks/sizeof *checks;i++)
        if(memcmp((const void*)(base+checks[i].at),checks[i].words,8))return;
    if(phone_active!=(void*(*)(void))(base+0x63380dc)||phone_type!=(unsigned(*)(void))(base+0x6338aec))return;
    tt_event_address=(void*)(base+0x87037cc);tt_ready=1;
    tt_socket=(struct reload_tf (*)(const void*,unsigned long long,int))(base+0x83c6310);
    cinema_typewriter_view=touch_typewriter_view;
    LOGF("TYPEWRITER touch ready: active owned keys through original press/release and native Cancel");
}
#endif
