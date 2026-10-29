// GameThread-only mono tracking; accept only the native primary FOculusHMD vtable.
#ifndef RE4FIX_TRACKING_H
#define RE4FIX_TRACKING_H
#include <stdint.h>
static void (*tracking_enforce)(void*, int);
static int (*tracking_enforced)(const void*), (*tracking_allowed)(const void*);
static const void* const* tracking_wrapper_slot;
static void* const* tracking_engine_slot;
static const void* tracking_hmd_vtable;
static const unsigned long long* g_pose_frame_counter;
static unsigned tracking_logs, tracking_state_logs, tracking_capture_logs;
struct tracking_quat { float x,y,z,w; };
struct tracking_vec { float x,y,z; };
_Static_assert(sizeof(struct tracking_quat)==16 && sizeof(struct tracking_vec)==12,"native tracking output ABI");
static int (*tracking_pose)(void*,int,struct tracking_quat*,struct tracking_vec*);
static void* (*tracking_active_pawn)(void);
static void* (*tracking_pawn_class)(void);
static void* (*tracking_camera_class)(void);
static volatile int g_tracking_diag_request;
static unsigned tracking_diag_samples;
int h5_ovrp_GetNodePoseState3(long step,long frame,long node,long out);

static int tracking_object_is_a(const void* object,const void* target,int minimum_size) {
    if (!object || !target) return 0;
    const char* cls=*(const char* const*)((const char*)object+0x10);
    if (!cls) return 0;
    int size=*(const int*)(cls+0x58), depth=*(const int*)((const char*)target+0x38);
    int child_depth=*(const int*)(cls+0x38);
    if (size<minimum_size || size>0x10000 || depth<0 || depth>128
        || child_depth<depth || child_depth>256) return 0;
    const void* const* chain=*(const void* const* const*)(cls+0x30);
    return chain && chain[depth]==(const char*)target+0x30;
}

static void tracking_read_diag(void* hmd) {
    if (!tracking_pose || !tracking_active_pawn || !tracking_pawn_class || !tracking_camera_class) return;
    unsigned request=__atomic_exchange_n(&g_tracking_diag_request,0,__ATOMIC_ACQ_REL);
    if (!request && tracking_diag_samples>=2) return;
    if (!request) ++tracking_diag_samples;
    const char* wrapper=(const char*)*tracking_wrapper_slot;
    int (*initialized)(void)=0;
    if (wrapper) memcpy(&initialized,wrapper+0x18,sizeof initialized);
    int ready=initialized ? initialized() : 0;
    struct tracking_quat q={0,0,0,1};struct tracking_vec p={0,0,0};
    int result=ready==1 ? tracking_pose(hmd,0,&q,&p) : -1;
    void* frame=*(void* const*)((const char*)hmd+0x270);
    const char* settings=*(const char* const*)((const char*)hmd+0x248);
    struct tracking_quat base_q={0,0,0,0};struct tracking_vec base_p={0,0,0};
    unsigned flags=0, number=0;float scale=0;
    if (settings) {
        memcpy(&base_q,settings+0x30,sizeof base_q);memcpy(&base_p,settings+0x20,sizeof base_p);
        memcpy(&flags,settings+0x18,sizeof flags);
    }
    if (frame) { memcpy(&number,(const char*)frame+0x10,sizeof number);memcpy(&scale,(const char*)frame+0x14,sizeof scale); }
    int (*raw_pose)(intptr_t,intptr_t,intptr_t,intptr_t)=0;
    if (wrapper) memcpy(&raw_pose,wrapper+0x208,sizeof raw_pose);
    _Alignas(8) unsigned raw_state[22];unsigned raw_before[22];
    for (unsigned i=0;i<22;i++) raw_state[i]=0x7fc5a5a5;
    memcpy(raw_before,raw_state,sizeof raw_state);
    int raw_result=ready==1 && frame && raw_pose ? raw_pose(-1,(intptr_t)number,9,(intptr_t)raw_state) : -1;
    struct tracking_quat raw_q;struct tracking_vec raw_p;
    memcpy(&raw_q,raw_state,sizeof raw_q);memcpy(&raw_p,(const char*)raw_state+16,sizeof raw_p);
    unsigned changed=0;for (unsigned i=0;i<sizeof raw_state;i++)
        changed+=((const unsigned char*)raw_state)[i]!=((const unsigned char*)raw_before)[i];
    void* pawn=tracking_active_pawn(), *camera=0;
    int lock=-1, camera_valid=0;
    if (tracking_object_is_a(pawn,tracking_pawn_class(),0x2e0)) {
        camera=*(void* const*)((const char*)pawn+0x2d8);
        camera_valid=tracking_object_is_a(camera,tracking_camera_class(),0x205);
        if (camera_valid) lock=(*(const unsigned char*)((const char*)camera+0x204)>>2)&1;
    }
    LOGF("TRACKPOSE hmd=%p frame=%p initialized=%d result=%d q=(%.5f,%.5f,%.5f,%.5f) p=(%.3f,%.3f,%.3f) pawn=%p camera=%p valid=%d lockHMD=%d",
         hmd,frame,ready,result,q.x,q.y,q.z,q.w,p.x,p.y,p.z,pawn,camera,camera_valid,lock);
    LOGF("TRACKBASE lookDeg=(%.3f,%.3f) flags=%08x frameNumber=%u scale=%.3f baseQ=(%.5f,%.5f,%.5f,%.5f) baseP=(%.3f,%.3f,%.3f) raw208=%p h5=%p same=%d rawResult=%d changed=%u rawQ=(%.5f,%.5f,%.5f,%.5f) rawP=(%.3f,%.3f,%.3f)",
         g_look_yaw*57.29578f,g_look_pitch*57.29578f,flags,number,scale,base_q.x,base_q.y,base_q.z,base_q.w,base_p.x,base_p.y,base_p.z,
         (void*)raw_pose,(void*)&h5_ovrp_GetNodePoseState3,(void*)raw_pose==(void*)&h5_ovrp_GetNodePoseState3,raw_result,changed,
         raw_q.x,raw_q.y,raw_q.z,raw_q.w,raw_p.x,raw_p.y,raw_p.z);
}

static int tracking_is_primary_hmd(const void* hmd) {
    if (!hmd || !tracking_hmd_vtable || !tracking_allowed || !tracking_enforced) return 0;
    const char* vt=*(const char* const*)hmd;
    if (vt!=tracking_hmd_vtable) return 0;
    int (*allowed)(const void*)=0, (*enforced)(const void*)=0;
    memcpy(&allowed,vt+0x128,sizeof allowed);
    memcpy(&enforced,vt+0x130,sizeof enforced);
    return allowed==tracking_allowed && enforced==tracking_enforced;
}

static void* tracking_find_hmd(void) {
    void* hmd=__atomic_load_n(&g_hmd_instance,__ATOMIC_ACQUIRE);
    if (!hmd) {
        void* engine=tracking_engine_slot ? *tracking_engine_slot : 0;
        void* candidate=engine ? *(void* const*)((const char*)engine+0xb00) : 0;
        if (!tracking_is_primary_hmd(candidate)) {
            if (tracking_capture_logs<8) {
                ++tracking_capture_logs;
                LOGF("TRACKING capture deferred: engine=%p xr=%p vtable=%p",engine,candidate,
                     candidate ? *(const void* const*)candidate : 0);
            }
            return 0;
        }
        void* empty=0;
        if (__atomic_compare_exchange_n(&g_hmd_instance,&empty,candidate,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
            hmd=candidate;
            if (tracking_capture_logs<8) {
                ++tracking_capture_logs;
                LOGF("TRACKING captured native primary HMD from GEngine.XRSystem hmd=%p",hmd);
            }
        } else hmd=empty;
    }
    if (!tracking_is_primary_hmd(hmd)) {
        if (tracking_capture_logs<8) {
            ++tracking_capture_logs;
            LOGF("TRACKING rejected cached HMD: hmd=%p vtable=%p",hmd,hmd ? *(const void* const*)hmd : 0);
        }
        return 0;
    }
    return hmd;
}

static void touch_tracking_tick(void) {
    void (*enforce)(void*,int)=__atomic_load_n(&tracking_enforce,__ATOMIC_ACQUIRE);
    if (!enforce || !tracking_wrapper_slot) return;
    void* hmd=tracking_find_hmd();
    if (!hmd) return;
    if (!*(const void* const*)((const char*)hmd+0x248)) {
        if (tracking_logs<8) { ++tracking_logs; LOGF("TRACKING deferred: native Settings unavailable"); }
        return;
    }
    if (tracking_enforced(hmd)) {
        if (!tracking_state_logs) {
            ++tracking_state_logs;
            LOGF("TRACKING native mono already enforced hmd=%p allowed=%d",hmd,tracking_allowed(hmd));
        }
        tracking_read_diag(hmd);
        return;
    }
    const char* wrapper=(const char*)*tracking_wrapper_slot;
    int (*initialized)(void)=0;
    if (wrapper) memcpy(&initialized,wrapper+0x18,sizeof initialized);
    if (!initialized || initialized()!=1) {
        if (tracking_logs<8) { ++tracking_logs; LOGF("TRACKING deferred: native OVR not initialized"); }
        return;
    }
    int before=tracking_allowed(hmd);
    // Existing compatibility InitDevice succeeds; stock code would also early-return.
    enforce(hmd,1);
    int after=tracking_allowed(hmd), enabled=tracking_enforced(hmd);
    if (tracking_state_logs<4) {
        ++tracking_state_logs;
        LOGF("TRACKING native mono hmd=%p enforced=%d allowed=%d->%d",hmd,enabled,before,after);
    }
    if (enabled) tracking_read_diag(hmd);
}

static void install_touch_tracking(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[2]; } check[] = {
        {0x5c1329c,{0xf9412409,0x1200002a}}, {0x5c13230,{0xf81e0ff3,0xa9017bfd}},
        {0x5c13268,{0xf9412668,0xb4000108}}, {0x5c135a8,{0xf81e0ff3,0xa9017bfd}},
        {0x5c132b4,{0x36000061,0x37200068}}, {0x5c133e8,{0xf94282d6,0xf9400ec8}},
        {0x5c133f0,{0xd63f0100,0x340000e0}}, {0x5c133f4,{0x340000e0,0x52800034}},
        {0x8300d58,{0xb0010295,0xf945fab5}}, {0x8300da0,{0xb40039e8,0xf9458108}},
        {0x8300dc8,{0xf9458115,0xf94002a8}}, {0x8300df0,{0xf9409508,0xd63f0100}},
        {0x5c19d64,{0xf942d508,0x9104c260}}, {0x5c19d70,{0x91004109,0x910f410a}},
        {0x5c19d78,{0xf9000269,0x9e670140}},
        {0x5c1260c,{0xd10383ff,0xa90967fa}}, {0x5c1278c,{0xf94126d8,0xf9413ad9}},
        {0x63380dc,{0xd0020168,0xf9431508}}, {0x64b7218,{0xd10183ff,0xf90023f3}},
        {0x8cf28c0,{0xd10183ff,0xf90023f3}}, {0x6340354,{0xf81d0ffc,0xa9014ff4}},
        {0x83013f8,{0x39481288,0x36100368}}, {0x63403c0,{0xf9416e60,0x1e2703e0}},
        {0x5c12710,{0xf9410748,0x3d801be0}}, {0x5c12718,{0xb9401321,0x910023e3}},
        {0x5c12730,{0xbd401727,0x2d460700}}, {0x5c12738,{0x2d470f02,0x2d441704}},
    };
    if (!base || __atomic_load_n(&tracking_enforce,__ATOMIC_ACQUIRE)) return;
    for (unsigned i=0;i<sizeof check/sizeof check[0];i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,sizeof check[i].first)) {
            LOGF("TRACKING disabled: opcode mismatch +%lx",check[i].addr); return;
        }
    if (*(const void* const*)(base+0xa351bf0)!=(const void*)(base+0xaa75db0)
        || *(const void* const*)(base+0xa35f5a8)!=(const void*)(base+0x9cb5458)
        || *(const void* const*)(base+0x9cb5590)!=(const void*)(base+0x5c135a8)
        || *(const void* const*)(base+0x9cb5598)!=(const void*)(base+0x5c13230)) {
        LOGF("TRACKING disabled: engine or primary HMD relocation mismatch"); return;
    }
    tracking_wrapper_slot=(const void* const*)(base+0xa35f500);
    tracking_engine_slot=(void* const*)(base+0xaa75db0);
    tracking_hmd_vtable=(const void*)(base+0x9cb5468);
    tracking_enforced=(int (*)(const void*))(base+0x5c13230);
    tracking_allowed=(int (*)(const void*))(base+0x5c135a8);
    tracking_pose=(int (*)(void*,int,struct tracking_quat*,struct tracking_vec*))(base+0x5c1260c);
    tracking_active_pawn=(void* (*)(void))(base+0x63380dc);
    tracking_pawn_class=(void* (*)(void))(base+0x64b7218);
    tracking_camera_class=(void* (*)(void))(base+0x8cf28c0);
    g_pose_frame_counter=(const unsigned long long*)(base+0xa6eb040); // Exported uint64 GFrameCounter in this guarded native build.
    __atomic_store_n(&tracking_enforce,(void (*)(void*,int))(base+0x5c1329c),__ATOMIC_RELEASE);
    LOGF("TRACKING installed: native enforced tracking without stereo or camera writes");
}
#endif
