// Include after reload/phone helpers. The caller declares g_touch_interact_request.
#include <math.h>
#include <string.h>
static void* (*ti_best)(const void*);
static void* (*ti_parent)(const void*);
static int (*ti_free)(const void*,int);
static int (*ti_down)(const void*);
static void (*ti_press)(void*), (*ti_release)(void*);
static void* (*ti_holster_class)(void);
static void* (*ti_ammo_class)(void);
static void* (*ti_prop_class)(void);
static void* (*ti_prop_pawn)(const void*);
static int (*ti_action_active)(void), (*ti_can_action)(void);
static void* (*ti_manager)(void);
static void* (*ti_manager_class)(void);
static void* (*ti_get_hand)(const void*,unsigned char);
static void* (*ti_hand_class)(void);
static void* ti_hand, *ti_pawn, *ti_grip;
static int ti_side = -1;
static void* ti_face_hand, *ti_face_pawn;
static int ti_ready, ti_cancel_hold, ti_face_bit = -1;
static int (*interact_sce_hook)(void* pawn, long now);   // re4fix_sce_touch.h: levers/cranks
static int (*interact_mirror_hook)(void* pawn,long now,int release_only);
static void (*interact_mirror_tick)(void* hand,int gameplay,int cancelled);
static long ti_face_until, ti_prior_face_until;
static void* (*ti_pickup_class)(void),*(*ti_pickup_grip_class)(void),*(*ti_scene_class)(void);
static int (*ti_collectable)(const void*);
static volatile long ti_pickup_available_ms;
static long ti_pickup_sample_ms;
struct ti_vec {float x,y,z;};
static struct ti_vec (*ti_grab_center)(const void*);
static struct ti_vec (*ti_ray_origin)(const void*);
static void* (*ti_game_pawn_class)(void);
static int (*ti_in_valid)(const void*),(*ti_line_of_sight)(const void*,const float*,const float*);
static void* (*ti_door_class)(void);
static int (*ti_door_can_open)(const void*);
static void (*ti_door_open)(void*,float);


static int interact_is_a(const void* object, const void* cls) {
    const char* actual = object ? *(const char* const*)((const char*)object+0x10) : 0;
    if (!actual || !cls) return 0;
    int depth = *(const int*)((const char*)cls+0x38), actual_depth = *(const int*)(actual+0x38);
    const void* const* chain = *(const void* const**)(actual+0x30);
    return depth >= 0 && actual_depth <= 1024 && depth <= actual_depth && chain
        && chain[depth] == (const char*)cls+0x30;
}
static int interact_world_grip(void* grip, void* pawn) {
    if (!reload_grip_registered(grip)) return 0;
    const char* cls = *(const char* const*)((const char*)grip+0x10);
    int size = cls ? *(const int*)(cls+0x58) : 0;
    if (size < 0x308 || size > 0x10000 || !ti_parent(grip)) return 0;
    // GetGrabbableParent returns an interface; +0x300 is its validity-checked UObject.
    void* parent = *(void* const*)((const char*)grip+0x300);
    cls = parent ? *(const char* const*)((const char*)parent+0x10) : 0;
    size = cls ? *(const int*)(cls+0x58) : 0;
    if (size < 0x28 || size > 0x10000) return 0;
    void* holster = ti_holster_class(), *ammo = ti_ammo_class(), *prop = ti_prop_class();
    if (!holster || !ammo || !prop || interact_is_a(parent,holster) || interact_is_a(parent,ammo)) return 0;
    if (interact_is_a(parent,prop) && (size < 0x3c8 || ti_prop_pawn(parent) == pawn)) return 0;
    return 1;
}
static int interact_layout(const void* p,int minimum) {
    const char* cls=p ? *(const char* const*)((const char*)p+0x10) : 0;
    int size=cls ? *(const int*)(cls+0x58) : 0;
    return size>=minimum && size<=0x10000;
}
static int interact_body_origin(void* pawn,float body[3]) {
    if(!ti_ready || !cinema_on_game_thread() || !ti_ray_origin || !ti_game_pawn_class
        || !phone_active || !phone_type || phone_type()!=1 || phone_active()!=pawn
        || !interact_layout(pawn,0xb90) || !interact_is_a(pawn,ti_game_pawn_class()))return 0;
    void* const* vtable=*(void* const* const*)pawn;
    if(!vtable || vtable[0x888/sizeof(void*)]!=(void*)ti_ray_origin)return 0;
    struct ti_vec origin=ti_ray_origin(pawn);
    body[0]=origin.x;body[1]=origin.y;body[2]=origin.z;
    return isfinite(body[0]) && isfinite(body[1]) && isfinite(body[2]);
}
static int interact_line_of_sight(void* grip,void* pawn,const float eye[3]) {
    float body[3];
    if(!ti_line_of_sight || !reload_grip_registered(grip) || !interact_layout(grip,0x460)
        || !isfinite(eye[0]) || !isfinite(eye[1]) || !isfinite(eye[2]) || !interact_body_origin(pawn,body))return 0;
    // Original GetBestGrip -> FindBestGrip supplies (head, pawn.GetHandRayCastOrigin()).
    return ti_line_of_sight(grip,eye,body);
}
static int interact_pickup_available(long now) {
    long sampled=__atomic_load_n(&ti_pickup_available_ms,__ATOMIC_ACQUIRE);
    return sampled>0 && now>=sampled && now-sampled<=300;
}
static int interact_pickup_score(const float eye[3],const float forward[3],const float point[3],int on_floor,float* score) {
    float d[3],horizontal=0;
    for(int i=0;i<3;i++){if(!isfinite(eye[i])||!isfinite(point[i])||!isfinite(forward[i]))return 0;d[i]=point[i]-eye[i];}
    horizontal=d[0]*d[0]+d[1]*d[1];
    float length=forward[0]*forward[0]+forward[1]*forward[1];
    if(!isfinite(horizontal)||horizontal>130.0f*130.0f||d[2]<-190.0f||d[2]>65.0f)return 0;
    float facing=d[0]*forward[0]+d[1]*forward[1];
    if(!(on_floor && horizontal<=65.0f*65.0f)
        &&(length<.01f || facing<=0 || facing*facing<.32898993f*horizontal*length))return 0; // 55 degrees either side.
    *score=horizontal+.08f*d[2]*d[2];return 1;
}
static int interact_pickup_owned(void* grip,void** item_out) {
    if(!ti_pickup_class||!ti_pickup_grip_class||!ti_collectable||!reload_grip_registered(grip)
        ||!interact_layout(grip,0x460)||!interact_is_a(grip,ti_pickup_grip_class()))return 0;
    void* item=*(void* const*)((const char*)grip+0x20);
    if(!interact_layout(item,0x530)||!interact_is_a(item,ti_pickup_class())
        ||*(void* const*)((const char*)item+0x260)!=grip
        ||*(void* const*)((const char*)grip+0x300)!=item||!ti_parent(grip)||!ti_collectable(item)
        ||*(const unsigned char*)((const char*)item+0x4c9)
        ||*(const unsigned char*)((const char*)item+0x4ca)
        ||*(const unsigned char*)((const char*)item+0x4dc))return 0;
    *item_out=item;return 1;
}
static void* interact_near_pickup(void* hand,void* pawn,int side) {
    if(!ti_scene_class||!ti_pickup_class||!ti_collectable||!interact_layout(pawn,0x2e0)
        ||!interact_layout(hand,0x470))return 0;
    if(*(const int*)((const char*)hand+0x468))return 0; // A foreign forced hover keeps its owner.
    void* head=*(void* const*)((const char*)pawn+0x2d8);
    if(!interact_layout(head,0x1f0)||!interact_is_a(head,ti_scene_class())
        ||*(void* const*)((const char*)head+0x20)!=pawn)return 0;
    float q[4],eye[3],forward[3];memcpy(q,(const char*)head+0x1c0,sizeof q);memcpy(eye,(const char*)head+0x1d0,sizeof eye);
    float norm=0;for(int i=0;i<4;i++)norm+=q[i]*q[i];if(!isfinite(norm)||fabsf(norm-1)>.01f)return 0;
    forward[0]=1-2*(q[1]*q[1]+q[2]*q[2]);forward[1]=2*(q[0]*q[1]+q[3]*q[2]);forward[2]=2*(q[0]*q[2]-q[3]*q[1]);
    void* candidates[512];int count=0;
    pthread_mutex_lock(&g_gripmx);
    for(int i=0;i<g_ngrips && count<512;i++)candidates[count++]=g_grips[i].g;
    pthread_mutex_unlock(&g_gripmx);
    void* best=0;float best_score=1e9f;
    for(int i=0;i<count;i++){
        void* item=0,*grip=candidates[i];if(!interact_pickup_owned(grip,&item)||!ti_free(grip,side))continue;
        void* root=*(void* const*)((const char*)item+0x130);
        if(!interact_layout(root,0x1f0)||!interact_is_a(root,ti_scene_class())
            ||*(void* const*)((const char*)root+0x20)!=item)continue;
        float point[3],score;memcpy(point,(const char*)root+0x1d0,sizeof point);
        if(interact_pickup_score(eye,forward,point,*(const unsigned char*)((const char*)item+0x4de)==2,&score)
            &&score<best_score){best=grip;best_score=score;}
    }
    return best;
}
static void* interact_near_world(void* hand,void* pawn,int side) {
    if(!ti_grab_center || !ti_in_valid || !ti_line_of_sight || !ti_scene_class
        || !interact_layout(pawn,0x2e0) || !interact_layout(hand,0x470)
        || *(const int*)((const char*)hand+0x468))return 0;
    void* head=*(void* const*)((const char*)pawn+0x2d8);
    if(!interact_layout(head,0x1f0) || !interact_is_a(head,ti_scene_class())
        || *(void* const*)((const char*)head+0x20)!=pawn)return 0;
    float q[4],eye[3],forward[3];memcpy(q,(const char*)head+0x1c0,16);memcpy(eye,(const char*)head+0x1d0,12);
    float norm=0;for(int i=0;i<4;i++)norm+=q[i]*q[i];if(!isfinite(norm)||fabsf(norm-1)>.01f)return 0;
    forward[0]=1-2*(q[1]*q[1]+q[2]*q[2]);forward[1]=2*(q[0]*q[1]+q[3]*q[2]);forward[2]=2*(q[0]*q[2]-q[3]*q[1]);
    void* candidates[512];int count=0;
    pthread_mutex_lock(&g_gripmx);
    for(int i=0;i<g_ngrips && count<512;i++)candidates[count++]=g_grips[i].g;
    pthread_mutex_unlock(&g_gripmx);
    void* best=0;float best_score=1e9f;
    for(int i=0;i<count;i++) {
        void* grip=candidates[i];if(!interact_world_grip(grip,pawn) || !ti_free(grip,side))continue;
        struct ti_vec center=ti_grab_center(grip);float point[3]={center.x,center.y,center.z},score;
        if(!interact_pickup_score(eye,forward,point,0,&score) || score>=best_score
            || !ti_in_valid(grip) || !interact_line_of_sight(grip,pawn,eye))continue;
        best=grip;best_score=score;
    }
    return best;
}
static int interact_face_bit(unsigned command) {
    // Ordinary authored commands use real face-key edges, never QTE completion.
    return command == 1 ? 0 : command == 2 ? 1 : command == 3 ? 8 : command == 4 ? 9 : -1;
}
static unsigned interact_prompt(unsigned* action) {
    if (!ti_ready || !ti_manager || !ti_manager_class || !ti_action_active || !ti_can_action
        || !ti_action_active() || !ti_can_action()) return 0;
    void* manager = ti_manager(), *expected = ti_manager_class();
    const char* cls = manager ? *(const char* const*)((const char*)manager+0x10) : 0;
    int size = cls ? *(const int*)(cls+0x58) : 0;
    if (!expected || *(const int*)((const char*)expected+0x58)!=0x410
        || size<0x410 || size>0x10000 || !interact_is_a(manager,expected)) return 0;
    unsigned command = *(const unsigned char*)((const char*)manager+0x3d9);
    unsigned kind = *(const unsigned char*)((const char*)manager+0x3da);
    if (interact_face_bit(command)<0 || kind>=72 || kind==48) return 0; // Authored NoDisp has no mobile prompt.
    if (action) *action = kind;
    return command;
}
static void interact_clear_face(void) {
    if (ti_face_bit >= 0 && ti_face_until > 0 && g_cmd_until[ti_face_bit] == ti_face_until)
        g_cmd_until[ti_face_bit] = ti_prior_face_until;
    ti_face_bit = -1; ti_face_until = 0; ti_face_hand = 0; ti_face_pawn = 0;
}
static void interact_clear_hold(void* hand, int release) {
    if (release && ti_hand == hand && ti_down(hand)) {
        void* current=f_GetCurrentGrip(hand);
        if(!current || (current==ti_grip && interact_world_grip(current,ti_pawn))) ti_release(hand);
    }
    ti_hand = 0; ti_pawn = 0; ti_grip = 0; ti_cancel_hold = 0; ti_side = -1;
}
static int interact_bound_hand(void* hand,void* pawn,int side) {
    const char* cls=hand ? *(const char* const*)((const char*)hand+0x10):0;
    int size=cls ? *(const int*)(cls+0x58):0;
    return ti_hand_class && size>=0x510 && size<=0x10000 && interact_is_a(hand,ti_hand_class())
        && *(const unsigned char*)((const char*)hand+0x314)==side && phone_player(hand)==pawn;
}
static int interact_hand_busy(void* hand,int side,long now) {
    unsigned bit=side ? 27u:29u;
    return f_GetCurrentGrip(hand) || ti_down(hand) || g_cmd_until[bit]>now || (g_grip_latched&(1u<<bit));
}
// Explicit inspect only: query the same native predicates without publishing input.
static void touch_interact_inspect_object(void* grip) {
    if(!ti_ready || !reload_grip_registered(grip) || !interact_layout(grip,0x460)
        || !ti_grab_center || !ti_free || !ti_in_valid || !ti_parent)return;
    struct ti_vec center=ti_grab_center(grip);
    int free0=ti_free(grip,0),free1=ti_free(grip,1),in_valid=ti_in_valid(grip);
    void* parent=*(void* const*)((const char*)grip+0x300);
    int parent_ok=ti_parent(grip) && interact_layout(parent,0x28);
    char pn[96]="",pc[96]="";
    if(parent_ok){obj_name(parent,pn,sizeof pn);obj_name(*(void* const*)((const char*)parent+0x10),pc,sizeof pc);}
    LOGF("INTERACTINSPECT grip=%p center=%.3f,%.3f,%.3f free=%d,%d in_valid=%d parent=%p layout=%d '%s' class='%s'",
         grip,center.x,center.y,center.z,free0,free1,in_valid,parent,parent_ok,pn,pc);
    if(!phone_active || !phone_type || phone_type()!=1 || !ti_get_hand || !ti_hand_class
        || !phone_player || !ti_scene_class || !ti_line_of_sight || !f_GetCurrentGrip)return;
    void* pawn=phone_active();
    if(!interact_layout(pawn,0x2e0))return;
    void* head=*(void* const*)((const char*)pawn+0x2d8);
    int head_ok=interact_layout(head,0x1f0) && interact_is_a(head,ti_scene_class())
        && *(void* const*)((const char*)head+0x20)==pawn;
    LOGF("INTERACTEYE pawn=%p head=%p layout=%d",pawn,head,head_ok);
    if(!head_ok)return;
    float q[4],eye[3],forward[3],point[3]={center.x,center.y,center.z},norm=0,score=-1;
    memcpy(q,(const char*)head+0x1c0,sizeof q);memcpy(eye,(const char*)head+0x1d0,sizeof eye);
    for(int i=0;i<4;i++)norm+=q[i]*q[i];
    if(!isfinite(norm) || fabsf(norm-1)>.01f)return;
    forward[0]=1-2*(q[1]*q[1]+q[2]*q[2]);forward[1]=2*(q[0]*q[1]+q[3]*q[2]);forward[2]=2*(q[0]*q[2]-q[3]*q[1]);
    int finite=1;for(int i=0;i<3;i++)if(!isfinite(eye[i]) || !isfinite(point[i]))finite=0;
    int score_ok=interact_pickup_score(eye,forward,point,0,&score);
    int world=interact_world_grip(grip,pawn),los=finite ? interact_line_of_sight(grip,pawn,eye):-1;
    float body[3]={0};int body_ok=interact_body_origin(pawn,body);
    LOGF("INTERACTBODY pawn=%p valid=%d ray_origin=%.3f,%.3f,%.3f",pawn,body_ok,body[0],body[1],body[2]);
    LOGF("INTERACTEYE eye=%.3f,%.3f,%.3f forward=%.4f,%.4f,%.4f delta=%.3f,%.3f,%.3f world=%d score_ok=%d score=%.3f los=%d",
         eye[0],eye[1],eye[2],forward[0],forward[1],forward[2],point[0]-eye[0],point[1]-eye[1],point[2]-eye[2],world,score_ok,score,los);
    for(int side=0;side<2;side++){
        void* hand=ti_get_hand(pawn,(unsigned char)side);int bound=interact_bound_hand(hand,pawn,side);
        int forced=bound ? *(const int*)((const char*)hand+0x468):-1;
        void* current=bound ? f_GetCurrentGrip(hand):0;
        LOGF("INTERACTSIDE side=%d hand=%p bound=%d forced=%d current=%p free=%d in_valid=%d world=%d score_ok=%d los=%d",
             side,hand,bound,forced,current,side ? free1:free0,in_valid,world,score_ok,los);
    }
}
static void touch_interact_tick(void* hand, int side, long now) {
    if (!ti_ready || !phone_player || !phone_active || !phone_type || !rl_paused || !rl_player_valid
        || !f_GetCurrentGrip || !hand || side<0 || side>1
        || *(const unsigned char*)((const char*)hand+0x314) != side) return;
    void* pawn = phone_player(hand), *active = phone_active();
    int cancelled = (__atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE) & 1) != 0;
    int gameplay = pawn && pawn == active && phone_type() == 1 && !rl_paused() && rl_player_valid();
    if(interact_mirror_tick)interact_mirror_tick(hand,gameplay,cancelled);
    if (ti_face_until && (now >= ti_face_until || cancelled || active != ti_face_pawn
        || (ti_face_hand == hand && !gameplay))) interact_clear_face();
    if (ti_hand && (active != ti_pawn || cancelled)) ti_cancel_hold = 1;
    // Pooled inactive hands may Tick first; only the owning hand may release its cached input.
    if (ti_hand == hand && (ti_cancel_hold || !gameplay)) interact_clear_hold(hand,1);
    if (!gameplay || cancelled) return;
    if(side==0 && (!ti_pickup_sample_ms || now<ti_pickup_sample_ms || now-ti_pickup_sample_ms>=120)){
        ti_pickup_sample_ms=now;
        void* candidate=0;
        if(!g_reload_phase && !ti_hand && !interact_hand_busy(hand,0,now)) {
            candidate=interact_near_pickup(hand,pawn,0);
            if(!candidate)candidate=interact_near_world(hand,pawn,0);
        }
        __atomic_store_n(&ti_pickup_available_ms,candidate ? now : 0,__ATOMIC_RELEASE);
    }
    if (ti_hand == hand) {
        void* held = f_GetCurrentGrip(hand);
        if (held != ti_grip || !reload_grip_registered(ti_grip) || !ti_down(hand))
            interact_clear_hold(hand,!held);
    }
    // Only active-left Tick consumes a new request. Right Tick cleans up its owned hold only.
    if(side!=0 || !__atomic_exchange_n(&g_touch_interact_request,0,__ATOMIC_ACQ_REL)) return;
    if(interact_mirror_hook && interact_mirror_hook(pawn,now,1))return;
    if (ti_action_active() && ti_can_action()) {
        unsigned action = 71, command = interact_prompt(&action);
        int bit = interact_face_bit(command);
        if (bit >= 0 && !ti_face_until && g_cmd_until[bit] <= now) {
            ti_face_bit = bit; ti_prior_face_until = g_cmd_until[bit]; ti_face_until = now+180;
            ti_face_hand = hand; ti_face_pawn = pawn;
            g_cmd_until[bit] = ti_face_until;
            LOGF("INTERACT native simple-face prompt action=%u command=%u bit=%d",action,command,bit); return;
        }
        LOGF("INTERACT prompt unavailable (unsupported or already held command)"); return;
    }
    // VR-only levers and cranks in front of the camera (re4fix_sce_touch.h) before free-hand world grips.
    if (interact_sce_hook && interact_sce_hook(pawn, now)) return;
    if(interact_mirror_hook && interact_mirror_hook(pawn,now,0))return;
    void* selected=hand;int selected_side=0;
    if(ti_hand) {
        if(!ti_get_hand || ti_side<0 || ti_side>1 || ti_get_hand(pawn,(unsigned char)ti_side)!=ti_hand
            || !interact_bound_hand(ti_hand,pawn,ti_side)) {
            LOGF("INTERACT unavailable (owned world hand changed)");return;
        }
        selected=ti_hand;selected_side=ti_side;
    } else {
        void* left_current=f_GetCurrentGrip(hand);
        if(!interact_world_grip(left_current,pawn) && interact_hand_busy(hand,0,now)) {
            selected=ti_get_hand ? ti_get_hand(pawn,1):0;selected_side=1;
            if(!interact_bound_hand(selected,pawn,1)) {
                LOGF("INTERACT unavailable (no bound free hand)");return;
            }
        }
    }
    void* current = f_GetCurrentGrip(selected);
    if (!g_reload_phase && !__atomic_load_n(&g_touch_reload_request,__ATOMIC_ACQUIRE)
        && current && interact_world_grip(current,pawn)) {
        ti_release(selected);
        if (ti_hand == selected) interact_clear_hold(selected,0);
        LOGF("INTERACT normal world grip release"); return;
    }
    if (ti_hand || g_reload_phase || __atomic_load_n(&g_touch_reload_request,__ATOMIC_ACQUIRE)
        || interact_hand_busy(selected,selected_side,now)) {
        LOGF("INTERACT unavailable (hands occupied)"); return;
    }
    void** vtable = *(void***)selected;
    if (!vtable || !vtable[0x740/sizeof(void*)]
        || !((int (*)(const void*))vtable[0x740/sizeof(void*)])(selected)) return;
    void* grip = ti_best(selected);
    void* pickup=0;
    int forced_hover=0;
    if((!interact_world_grip(grip,pawn)||!ti_free(grip,selected_side)) && f_SetGripForcedHover){
        grip=interact_near_pickup(selected,pawn,selected_side);
        int nearby_pickup=interact_pickup_owned(grip,&pickup);
        if(!nearby_pickup)grip=interact_near_world(selected,pawn,selected_side);
        if(nearby_pickup || interact_world_grip(grip,pawn)){
            f_SetGripForcedHover(selected,grip,1);
            forced_hover=1;
            if(ti_best(selected)!=grip || !ti_free(grip,selected_side) || phone_active()!=pawn
                || (nearby_pickup ? !interact_pickup_owned(grip,&pickup)
                    : !interact_world_grip(grip,pawn) || !ti_in_valid(grip))){
                if(reload_grip_registered(grip))f_SetGripForcedHover(selected,grip,0);
                LOGF("INTERACT nearby pickup rejected by native grip route");return;
            }
        }
    }
    if (!interact_world_grip(grip,pawn) || !ti_free(grip,selected_side)) {
        LOGF("INTERACT no eligible nearby world grip"); return;
    }
    // Native hover and the nearby fallback use the same collectible lifecycle.
    // Classification must precede press: the original grab changes pickup flags.
    if(!pickup)interact_pickup_owned(grip,&pickup);
    // This normal input callback owns IsGripDown; an additional OVR LG edge would grab twice.
    ti_press(selected);
    if(forced_hover && reload_grip_registered(grip))f_SetGripForcedHover(selected,grip,0);
    void* held = f_GetCurrentGrip(selected);
    if(pickup && held==grip && reload_grip_registered(grip) && interact_layout(pickup,0x530)
        && interact_is_a(pickup,ti_pickup_class())
        && *(void* const*)((const char*)grip+0x20)==pickup
        && *(void* const*)((const char*)pickup+0x260)==grip){
        ti_release(selected);__atomic_store_n(&ti_pickup_available_ms,0,__ATOMIC_RELEASE);
        LOGF("INTERACT native nearby pickup grab/release item=%p",pickup);return;
    }
    if (!interact_world_grip(held,pawn)) {
        if(!held) ti_release(selected);
        LOGF("INTERACT attempt completed without a world hold"); return;
    }
    void* parent=*(void* const*)((const char*)held+0x300);
    if(ti_door_class && ti_door_can_open && ti_door_open && interact_layout(parent,0x320)
        && interact_is_a(parent,ti_door_class())) {
        // Native grab selects the real knob side; its lock/story predicate owns opening eligibility.
        int allowed=ti_door_can_open(parent);
        if(allowed && phone_active()==pawn && !g_touch_cancel && f_GetCurrentGrip(selected)==held)
            ti_door_open(parent,.65f);
        if(f_GetCurrentGrip(selected)==held && ti_down(selected))ti_release(selected);
        LOGF("INTERACT native door gesture allowed=%d door=%p",allowed,parent);return;
    }
    ti_hand = selected; ti_side=selected_side; ti_pawn = pawn; ti_grip = held;
    LOGF("INTERACT native world hold=%p hand=%p",held,selected);
}
static void install_touch_interact(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[2]; } check[] = {
        {0x629a930,{0xd10403ff,0x6d082beb}}, {0x6280170,{0xf81e0ff3,0xa9017bfd}},
        {0x627feb8,{0x3947c008,0x340003c8}}, {0x633c54c,{0x52800028,0x390cf408}},
        {0x633c558,{0x390cf41f,0x1405d0a2}}, {0x633d048,{0x394cf400,0xd65f03c0}},
        {0x6434184,{0xd10183ff,0xf90023f3}}, {0x640dea8,{0xd10183ff,0xf90023f3}},
        {0x6432600,{0xd10183ff,0xf90023f3}}, {0x62bbd24,{0xf941e008,0xb4000268}},
        {0x61f7434,{0xa9bf7bfd,0x910003fd}}, {0x62da0c8,{0x90020468,0xf9415508}},
        {0x62dd110,{0xb0020448,0xf9415508}}, {0x629ad20,{0xf9400008,0xf943a108}},
        {0x628017c,{0xf9418013,0xb40002f3}}, {0x62dd160,{0x394a8108,0x7100111f}},
        {0x629857c,{0xaa1303e0,0x940292b2}}, {0x69b6500,{0xaa0003e8,0xf9401000}},
        {0x687d798,{0x294b2408,0x2a0203f6}},
        {0x62dab20,{0x390f6402,0x390f6c03}},
        {0x61f6990,{0xa9bd57f6,0xa9014ff4}}, {0x61f6ce8,{0xa9bc5ff8,0xa90157f6}},
        {0x61f4f8c,{0x94000681,0x2a0003f5}}, {0x61f4f98,{0x94000754,0x910013e0}},
        {0x61f5e30,{0x940002d8,0x12001c08}},
        {0x61f5f2c,{0xd0020b48,0xf947ad08}}, {0x61f602c,{0xb0020b48,0xf947b508}},
        {0x61f612c,{0xb0020b48,0xf947b108}}, {0x61f622c,{0xb0020b48,0xf947b908}},
        {0x61f6d44,{0xf947ad08,0xaa0003f6}}, {0x61f6d94,{0xf947b508,0xaa0003f6}},
        {0x61f6eac,{0xf947b108,0xaa0003f6}}, {0x61f6efc,{0xf947b908,0xaa0003f6}},
        {0x628da44,{0x52800022,0xaa1403e0}}, {0x628da4c,{0x9402bd8b,0x36000160}},
        {0x644cb84,{0xd10183ff,0xf90023f3}}, {0x644cc0c,{0x52808204,0x52800105}},
        {0x62da84c,{0x390f6a77,0x540001a0}},
        {0x62a8fc0,{0xf81e0ff3,0xa9017bfd}},
        {0x6424b5c,{0xd10183ff,0xf90023f3}},
        {0x627ff18,{0x8b210008,0x3947c508}},
        {0x6281494,{0xd101c3ff,0xf90023f5}}, {0x628220c,{0xd103c3ff,0xf9005bf7}},
        {0x6280060,{0xd10143ff,0xf90013f5}}, {0x63fa1cc,{0xd10183ff,0xf90023f3}},
        {0x625e428,{0xf81d0ff5,0xa9014ff4}}, {0x625e4f8,{0xd10143ff,0xfd000be8}},
        {0x621cb6c,{0xd10183ff,0xa9044ff4}}, {0x621cb8c,{0x396e2408,0x34000168}},
        {0x63c8a5c,{0xd10183ff,0xf90023f3}},
    };
    ti_ready = 0;ti_grab_center=0;ti_in_valid=0;ti_line_of_sight=0;ti_ray_origin=0;ti_game_pawn_class=0;
    ti_door_class=0;ti_door_can_open=0;ti_door_open=0;
    ti_pickup_class=ti_pickup_grip_class=ti_scene_class=0;ti_collectable=0;
    for (unsigned i = 0; i < sizeof check/sizeof check[0]; i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,sizeof check[i].first)) {
            LOGF("INTERACT disabled: opcode mismatch +%lx",check[i].addr); return;
        }
    static const struct { unsigned command, bit; char letter; } faces[] = {
        {1,0,'A'}, {2,1,'B'}, {3,8,'X'}, {4,9,'Y'},
    };
    for (unsigned i = 0; i < sizeof faces/sizeof faces[0]; i++) {
        const char* entry = (const char*)(base+0x9d92268+16*faces[i].command);
        const char* name = *(const char* const*)entry;
        if (*(const long long*)(entry+8) != faces[i].command || !name
            || memcmp(name,"EVR4CommandButton::",19) || name[19] != faces[i].letter || name[20]
            || interact_face_bit(faces[i].command) != (int)faces[i].bit) {
            LOGF("INTERACT disabled: native face enum mismatch"); return;
        }
    }
    for (unsigned command = 5; command <= 16; command++)
        if (interact_face_bit(command) != -1) return;
    static const struct { unsigned value; const char* name; } actions[] = {
        {0,"Talk"}, {1,"Check"}, {5,"JumpOver"}, {7,"Kick"}, {8,"GoUp"}, {9,"GetDown"},
        {16,"Open"}, {25,"Climb"}, {40,"PickUp"}, {44,"Suplex"}, {47,"Save"},
        {48,"NoDisp"}, {54,"Answer"}, {57,"BackKick"}, {71,"Any"},
    };
    if (*(const unsigned*)(base+0x9d4f170+40)!=72) return;
    for (unsigned i=0;i<sizeof actions/sizeof *actions;i++) {
        const char* entry=(const char*)(base+0x9d4ecf0+16*actions[i].value);
        const char* name=*(const char* const*)entry;
        if (*(const long long*)(entry+8)!=actions[i].value || !name
            || memcmp(name,"EActionButtonType::",19) || strcmp(name+19,actions[i].name)) {
            LOGF("INTERACT disabled: native semantic action enum mismatch"); return;
        }
    }
    if (interact_face_bit(0) != -1
        || *(const unsigned*)(base+0x9dbf850+0x24) != 0x3d9
        || *(const unsigned*)(base+0x9dbf7f0+0x24) != 0x3da
        || *(const unsigned*)(base+0x9d9e298+0x24) != 0x300) {
        LOGF("INTERACT disabled: native field metadata mismatch"); return;
    }
    ti_best = (void* (*)(const void*))(base+0x629a930);
    ti_parent = (void* (*)(const void*))(base+0x6280170);
    ti_free = (int (*)(const void*,int))(base+0x627feb8);
    ti_press = (void (*)(void*))(base+0x633c54c);
    ti_release = (void (*)(void*))(base+0x633c558);
    ti_down = (int (*)(const void*))(base+0x633d048);
    ti_holster_class = (void* (*)(void))(base+0x6434184);
    ti_ammo_class = (void* (*)(void))(base+0x640dea8);
    ti_prop_class = (void* (*)(void))(base+0x6432600);
    ti_prop_pawn = (void* (*)(const void*))(base+0x62bbd24);
    ti_action_active = (int (*)(void))(base+0x61f7434);
    ti_can_action = (int (*)(void))(base+0x62dd110);
    ti_manager = (void* (*)(void))(base+0x62da0c8);
    ti_manager_class = (void* (*)(void))(base+0x644cb84);
    ti_get_hand = (void* (*)(const void*,unsigned char))(base+0x62a8fc0);
    ti_hand_class = (void* (*)(void))(base+0x6424b5c);
    static const struct {unsigned long address;unsigned words[2];} pickup_checks[]={
        {0x645ace8,{0xd10183ff,0xf90023f3}}, {0x645ad70,{0x5280a604,0x52800205}},
        {0x64192f4,{0xd10183ff,0xf90023f3}}, {0x8f82500,{0xd10183ff,0xf90023f3}},
        {0x62ec9a8,{0x39530808,0x340000a8}}, {0x62ed97c,{0x39537808,0x7100091f}},
        {0x62ebf38,{0x39532408,0xaa0003f3}}, {0x62ec698,{0x39537008,0x34000148}},
    };
    int pickup_ok=*(const unsigned*)(base+0x9dcb308+0x24)==0x260;
    for(unsigned i=0;i<sizeof pickup_checks/sizeof *pickup_checks;i++)
        if(memcmp((const void*)(base+pickup_checks[i].address),pickup_checks[i].words,8))pickup_ok=0;
    if(pickup_ok){
        ti_pickup_class=(void* (*)(void))(base+0x645ace8);ti_pickup_grip_class=(void* (*)(void))(base+0x64192f4);
        ti_scene_class=(void* (*)(void))(base+0x8f82500);ti_collectable=(int (*)(const void*))(base+0x62ec9a8);
    }
    ti_grab_center=(struct ti_vec (*)(const void*))(base+0x6281494);
    ti_in_valid=(int (*)(const void*))(base+0x6280060);
    ti_line_of_sight=(int (*)(const void*,const float*,const float*))(base+0x628220c);
    ti_ray_origin=(struct ti_vec (*)(const void*))(base+0x621cb6c);
    ti_game_pawn_class=(void* (*)(void))(base+0x63c8a5c);
    ti_door_class=(void* (*)(void))(base+0x63fa1cc);
    ti_door_can_open=(int (*)(const void*))(base+0x625e428);
    ti_door_open=(void (*)(void*,float))(base+0x625e4f8);
    ti_ready = 1;
    LOGF("INTERACT native world-grip and authored A/B/X/Y prompt helper ready nearbyPickup=%d",pickup_ok);
}
