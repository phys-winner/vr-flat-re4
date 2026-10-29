// Include after grip registry helpers; call touch_reload_tick after the native hand Tick.
#include <stddef.h>
static void* touch_equipped_gun(void* hand);
static int (*ti_down)(const void*);
static int touch_quickselect_busy(void);
static int reload_family_info(void* gun,int* angle_offset);
static int reload_family_post_begin(void* primary,void* gun,long now);
static int reload_family_shells_pending(void* gun,int* loaded);
static int reload_family_post_tick(void* hand,int side,long now);
static void reload_family_post_cancel(void);
// This Android cook uses scalar FVector: Scale3D follows the 12-byte translation.
struct __attribute__((aligned(16))) reload_tf { qf_t q; float p[3], s[3], padding[2]; };
_Static_assert(sizeof(struct reload_tf) == 48 && offsetof(struct reload_tf,p) == 16
               && offsetof(struct reload_tf,s) == 28, "native scalar FTransform ABI");
static void* (*rl_player)(const void*);
static void* (*rl_active)(void);
static void* (*rl_gun1)(const void*);
static void* (*rl_left)(const void*);
static void* (*rl_right)(const void*);
static void* (*rl_primary)(const void*);
static void* (*rl_pistol_class)(void);
static void* (*rl_ammo_class)(void);
static void* (*rl_holster_class)(void);
static void* (*rl_get_pawn)(const void*);
static void* (*rl_get_holster)(const void*);
static int (*rl_can_grab_holstered_ammo)(const void*);
static int (*rl_paused)(void), (*rl_player_valid)(void);
static int (*rl_interactivity)(const void*), (*rl_ammo_available)(const void*);
static int (*rl_reloading)(void*), (*rl_ready)(const void*, float), (*rl_overlap)(const void*, float);
static void (*rl_begin)(void*);
static struct reload_tf (*rl_cached_transform)(const void*);
static void (*rl_world_transform)(void*, const struct reload_tf*, int, void*, int);
static void* rl_pawn, *rl_weapon, *rl_hover_hand, *rl_hover_grip;
static void* rl_mobile_holster, *rl_mobile_hand;
static long rl_mobile_until;
static long rl_deadline, rl_eject_end, rl_grip_until, rl_prior_grip_until;
static long rl_eject_until, rl_prior_eject_until;
static int rl_grip_bit=29,rl_eject_bit=1,rl_family,rl_angle_offset=0xd50;
static void* rl_main_hand,*rl_other_hand;
static int rl_main_side=1,rl_shell_count;
static struct reload_tf rl_hold_pose, rl_hold_target, rl_approach_from, rl_approach_target;
static long rl_approach_t0;
static int g_reload_approach_ms = 280;
// Position lerp + normalized quaternion lerp (shortest arc); scale from b.
static struct reload_tf reload_tf_blend(const struct reload_tf* a, const struct reload_tf* b, float t) {
    struct reload_tf r = *b;
    float d = a->q.x*b->q.x + a->q.y*b->q.y + a->q.z*b->q.z + a->q.w*b->q.w, k = d < 0 ? -1.0f : 1.0f;
    r.q.x = a->q.x*k + (b->q.x - a->q.x*k)*t; r.q.y = a->q.y*k + (b->q.y - a->q.y*k)*t;
    r.q.z = a->q.z*k + (b->q.z - a->q.z*k)*t; r.q.w = a->q.w*k + (b->q.w - a->q.w*k)*t;
    float n = sqrtf(r.q.x*r.q.x + r.q.y*r.q.y + r.q.z*r.q.z + r.q.w*r.q.w);
    if (n > 1e-6f) { r.q.x /= n; r.q.y /= n; r.q.z /= n; r.q.w /= n; }
    for (int i = 0; i < 3; i++) r.p[i] = a->p[i] + (b->p[i] - a->p[i])*t;
    return r;
}
static int rl_hold_pose_valid, rl_hold_target_offset;
static unsigned rl_transform_failure_logs;
static void* (*rl_hand_class)(void), *(*rl_pawn_class)(void), *(*rl_grip_class)(void);
static int (*rl_type)(void), (*rl_base_ready)(const void*), (*rl_pistol_ready)(const void*);
static void* (*rl_slide_hand)(const void*);
static int (*rl_held_ammo_ready)(const void*,float), (*rl_trigger)(const void*);
static int (*rl_cock_blending)(const void*), (*rl_cock_flourishing)(const void*);
struct rl_post_binding { void* object[8]; unsigned long long name[8]; };
static struct { struct rl_post_binding binding; long up_since,pulse_until; int valid,trigger_bit; } rl_post;
static struct {void* object[7];unsigned long long name[7];int valid;} rl_run;

static int reload_grip_registered(void* grip) {
    int found = 0;
    pthread_mutex_lock(&g_gripmx);
    for (int i = 0; i < g_ngrips; i++) if (g_grips[i].g == grip) { found = 1; break; }
    pthread_mutex_unlock(&g_gripmx);
    return found;
}
static int reload_marker_valid(const struct reload_tf* t) {
    float norm = t->q.x*t->q.x + t->q.y*t->q.y + t->q.z*t->q.z + t->q.w*t->q.w;
    if (!isfinite(norm) || fabsf(norm - 1.0f) > 0.01f) return 0;
    for (int i = 0; i < 3; i++)
        if (!isfinite(t->p[i]) || !isfinite(t->s[i]) || t->s[i]<=0.0f) return 0;
    return 1;
}
static int reload_tf_valid(const struct reload_tf* t) {
    float norm = t->q.x*t->q.x + t->q.y*t->q.y + t->q.z*t->q.z + t->q.w*t->q.w;
    if (!isfinite(norm) || fabsf(norm-1.0f)>0.01f) return 0;
    for (int i=0;i<3;i++)
        if (!isfinite(t->p[i]) || !isfinite(t->s[i]) || fabsf(t->s[i]-1.0f)>0.01f) return 0;
    return 1;
}
static void reload_transform_failure(const char* reason,const void* hand,const void* ammo,
                                     const void* gun,int target_offset,
                                     const struct reload_tf* current,const struct reload_tf* capsule,
                                     const struct reload_tf* target) {
    if (rl_transform_failure_logs>=3) return;
    ++rl_transform_failure_logs;
    LOGF("RELOADTF fail=%s hand=%p ammo=%p gun=%p cache=+%x ok=H%dC%dT%d "
         "Hq=(%.5g,%.5g,%.5g,%.5g) Hp=(%.6g,%.6g,%.6g) Hs=(%.6g,%.6g,%.6g) "
         "Cq=(%.5g,%.5g,%.5g,%.5g) Cp=(%.6g,%.6g,%.6g) Cs=(%.6g,%.6g,%.6g) "
         "Tq=(%.5g,%.5g,%.5g,%.5g) Tp=(%.6g,%.6g,%.6g) Ts=(%.6g,%.6g,%.6g)",
         reason,hand,ammo,gun,target_offset,reload_tf_valid(current),reload_marker_valid(capsule),reload_marker_valid(target),
         current->q.x,current->q.y,current->q.z,current->q.w,current->p[0],current->p[1],current->p[2],current->s[0],current->s[1],current->s[2],
         capsule->q.x,capsule->q.y,capsule->q.z,capsule->q.w,capsule->p[0],capsule->p[1],capsule->p[2],capsule->s[0],capsule->s[1],capsule->s[2],
         target->q.x,target->q.y,target->q.z,target->q.w,target->p[0],target->p[1],target->p[2],target->s[0],target->s[1],target->s[2]);
}
static struct reload_tf reload_hand_pose(struct reload_tf hand, struct reload_tf capsule,
                                          struct reload_tf target) {
    qf_t inv = {-capsule.q.x, -capsule.q.y, -capsule.q.z, capsule.q.w};
    qf_t delta = q_mul(target.q, inv);
    float from[3] = {hand.p[0]-capsule.p[0], hand.p[1]-capsule.p[1], hand.p[2]-capsule.p[2]}, to[3];
    q_rot(delta, from, to);
    for (int i = 0; i < 3; i++) hand.p[i] = target.p[i] + to[i];
    hand.q = q_mul(delta, hand.q);
    return hand;
}
static struct reload_tf reload_approach_pose(struct reload_tf from,struct reload_tf anchor,
                                             struct reload_tf target,struct reload_tf finish,float t) {
    t=fminf(1.0f,fmaxf(0.0f,t));
    // Keep the feed relative to the moving magazine well when the player looks around.
    from=reload_hand_pose(from,anchor,target);
    float e=t*t*(3.0f-2.0f*t),arc=t*t*(1.0f-t)*(1.0f-t);
    struct reload_tf pose=reload_tf_blend(&from,&finish,e);
    pose.p[2]-=128.0f*arc;   // an 8 cm dip clears the grip before the final upward insertion
    return pose;
}
static int reload_pose_selfcheck(void) {
    struct reload_tf h = {{0,0,0,1}, {2,0,0}, {1,1,1}, {0,0}};
    struct reload_tf c = {{0,0,0,1}, {3,0,0}, {1,1,1}, {0,0}};
    struct reload_tf t = {{0,0,0.70710678f,0.70710678f}, {10,20,30}, {1,1,1}, {0,0}};
    struct reload_tf r = reload_hand_pose(h,c,t);
    return fabsf(r.p[0]-10)<0.0001f && fabsf(r.p[1]-19)<0.0001f && fabsf(r.p[2]-30)<0.0001f
        && fabsf(r.q.z-t.q.z)<0.0001f && fabsf(r.q.w-t.q.w)<0.0001f && reload_tf_valid(&r);
}
static int reload_is_pistol(void* gun) {
    void* cls = rl_pistol_class();
    const char* actual = *(const char**)((char*)gun+0x10);
    if (!cls || !actual) return 0;
    int depth = *(const int*)((char*)cls+0x38), actual_depth = *(const int*)(actual+0x38);
    if (depth < 0 || depth > actual_depth || depth > 1024) return 0;
    const void* const* chain = *(const void* const**)(actual+0x30);
    return chain && chain[depth] == (char*)cls+0x30;
}
static int reload_layout(const void* object, int minimum) {
    const char* cls=object ? *(const char* const*)((const char*)object+0x10) : 0;
    int size=cls ? *(const int*)(cls+0x58) : 0;
    return size>=minimum && size<=0x10000;
}
static int reload_is_a(const void* object, const void* cls) {
    const char* actual=object ? *(const char* const*)((const char*)object+0x10) : 0;
    if (!actual || !cls) return 0;
    int depth=*(const int*)((const char*)cls+0x38), child_depth=*(const int*)(actual+0x38);
    const void* const* chain=*(const void* const* const*)(actual+0x30);
    return depth>=0 && depth<=128 && child_depth>=depth && child_depth<=256
        && chain && chain[depth]==(const char*)cls+0x30;
}
static void* reload_ammo_primary(void* gun, void* pawn) {
    if (!rl_ammo_class || !rl_get_pawn || !reload_layout(gun,0xd54)) return 0;
    void* ammo=*(void* const*)((const char*)gun+0x8e0);
    if (!reload_layout(ammo,0x610) || !reload_is_a(ammo,rl_ammo_class())
        || rl_get_pawn(ammo)!=pawn || *(void* const*)((const char*)ammo+0x578)!=gun) return 0;
    void* grip=*(void* const*)((const char*)ammo+0x270);
    return reload_grip_registered(grip) && *(void* const*)((const char*)grip+0x20)==ammo ? grip : 0;
}
static void* reload_ammo_holster_selection(void* ammo, void* pawn, void** holster_out,int* ready_out) {
    if(ready_out)*ready_out=0;
    if (!rl_get_holster || !rl_get_pawn || !rl_holster_class || !rl_ammo_class
        || !rl_can_grab_holstered_ammo || !holster_out) return 0;
    void* holster=rl_get_holster(ammo);
    void* owner=reload_layout(holster,0x360) ? *(void* const*)((const char*)holster+0x348) : 0;
    // Pawn holsters, or a world ammo holster (shooting range counter) that belongs to no pawn.
    if (!reload_layout(holster,0x360) || !reload_is_a(holster,rl_holster_class()) || (owner && owner!=pawn)) return 0;
    const void* const* props=*(const void* const* const*)((const char*)holster+0x350);
    int count=*(const int*)((const char*)holster+0x358), capacity=*(const int*)((const char*)holster+0x35c);
    if (!props || count<1 || count>32 || capacity<count || capacity>128) return 0;
    // Campaign AmmoHolster holds both Gun1/Gun2 pouches. Their mere presence
    // must not reject Reload before the native eject input. Validate the
    // original list, then (only when grabbing) mirror its first eligible item.
    void* entries[32];unsigned long long names[32],holster_name;
    memcpy(&holster_name,(const char*)holster+0x18,8);
    int found=0;for (int i=0;i<count;i++) {
        void* prop=(void*)props[i];entries[i]=prop;if(!prop)continue;
        if(!reload_layout(prop,0x610) || !reload_is_a(prop,rl_ammo_class())
            || rl_get_pawn(prop)!=pawn || rl_get_holster(prop)!=holster)return 0;
        const void* const* vtable=*(const void* const* const*)prop;
        if(!vtable || vtable[0x670/sizeof(void*)]!=(const void*)rl_can_grab_holstered_ammo)return 0;
        memcpy(&names[i],(const char*)prop+0x18,8);
        if(prop==ammo)found=1;
    }
    void* grip=*(void* const*)((const char*)holster+0x248);
    if (!found || !reload_grip_registered(grip) || *(void* const*)((const char*)grip+0x20)!=holster) return 0;
    if(ready_out)for(int i=0;i<count;i++)if(entries[i]){
        int eligible=rl_can_grab_holstered_ammo(entries[i]);
        unsigned long long current_name,current_holster_name;
        memcpy(&current_name,(const char*)entries[i]+0x18,8);memcpy(&current_holster_name,(const char*)holster+0x18,8);
        if(g_touch_context!=1 || g_touch_cancel || current_name!=names[i] || current_holster_name!=holster_name
            || *(const void* const* const*)((const char*)holster+0x350)!=props
            || *(const int*)((const char*)holster+0x358)!=count || props[i]!=entries[i])return 0;
        if(eligible){*ready_out=entries[i]==ammo;break;}
    }
    *holster_out=holster;return grip;
}
static void* reload_ammo_holster_grip(void* ammo,void* pawn,void** holster_out) {
    return reload_ammo_holster_selection(ammo,pawn,holster_out,0);
}
static void reload_clear_mobile_holster(void) {
    // Only release the exact deadline published by this macro; foreign requests survive.
    if (rl_mobile_until && g_mobile_holster==rl_mobile_holster && g_mobile_holster_hand==rl_mobile_hand
        && __atomic_load_n(&g_mobile_holster_until,__ATOMIC_ACQUIRE)==rl_mobile_until)
        __atomic_store_n(&g_mobile_holster_until,0,__ATOMIC_RELEASE);
    rl_mobile_holster=0;rl_mobile_hand=0;rl_mobile_until=0;
}
static int reload_publish_holster(void* holster, void* hand, long now) {
    long until=__atomic_load_n(&g_mobile_holster_until,__ATOMIC_ACQUIRE);
    int ours=rl_mobile_until && g_mobile_holster==rl_mobile_holster && g_mobile_holster_hand==hand
        && until==rl_mobile_until;
    if (until>now && !ours) return 0;
    long deadline=now+1800<rl_deadline ? now+1800 : rl_deadline;
    __atomic_store_n(&g_mobile_holster_until,0,__ATOMIC_RELEASE);
    g_mobile_holster=holster;g_mobile_holster_hand=hand;
    rl_mobile_holster=holster;rl_mobile_hand=hand;rl_mobile_until=deadline;
    __atomic_store_n(&g_mobile_holster_until,deadline,__ATOMIC_RELEASE);
    return 1;
}
static int rl_post_typed(const void* object,const void* cls,int minimum) {
    return cls && reload_layout(object,minimum) && reload_is_a(object,cls);
}
static int rl_post_binding(void* right,struct rl_post_binding* out) {
    if (!rl_hand_class || !rl_pawn_class || !rl_grip_class || !rl_type
        || rl_type()!=1 || !rl_base_ready || !rl_pistol_ready || !rl_slide_hand
        || !rl_held_ammo_ready || !rl_trigger || !rl_cock_blending || !rl_cock_flourishing || !f_GetCurrentGrip
        || !rl_post_typed(right,rl_hand_class(),0x510)
        || *(const unsigned char*)((const char*)right+0x314)>1) return 0;
    void* pawn=rl_player(right);
    if (!pawn || pawn!=rl_active() || !rl_post_typed(pawn,rl_pawn_class(),0x10d0)) return 0;
    int side=*(const unsigned char*)((const char*)right+0x314);
    void* gun=touch_equipped_gun(right),*left=side ? rl_left(pawn) : rl_right ? rl_right(pawn) : 0;
    if (!rl_post_typed(gun,rl_pistol_class(),0xde0)
        || *(void* const*)((const char*)gun+0x3c0)!=pawn
        || !rl_post_typed(left,rl_hand_class(),0x510)
        || *(const unsigned char*)((const char*)left+0x314)==side || rl_player(left)!=pawn) return 0;
    void** vtable=*(void***)gun;
    if (!vtable || vtable[0x778/sizeof(void*)]!=(void*)rl_pistol_ready) return 0;
    void* primary=*(void* const*)((const char*)gun+0x270);
    void* slide=*(void* const*)((const char*)gun+0xd38);
    void* ammo=*(void* const*)((const char*)gun+0x8e0);
    void* ammo_grip=reload_ammo_primary(gun,pawn);
    void* grips[3]={primary,slide,ammo_grip};
    void* owners[3]={gun,gun,ammo};
    for (int i=0;i<3;i++) if (!reload_grip_registered(grips[i])
        || !rl_post_typed(grips[i],rl_grip_class(),0x460)
        || *(void* const*)((const char*)grips[i]+0x20)!=owners[i]
        || *(void* const*)((const char*)grips[i]+0x300)!=owners[i]) return 0;
    if (rl_primary(gun)!=right || f_GetCurrentGrip(right)!=primary) return 0;
    void* objects[8]={right,pawn,gun,left,primary,slide,ammo,ammo_grip};
    for (int i=0;i<8;i++) {
        out->object[i]=objects[i];
        memcpy(&out->name[i],(const char*)objects[i]+0x18,8);
    }
    return 1;
}
static int reload_run_binding(void* main,void* other,void* gun,void* pawn,void** objects,unsigned long long* names) {
    if(!main || !other || !gun || !pawn || touch_equipped_gun(main)!=gun
        || rl_player(main)!=pawn || rl_player(other)!=pawn || pawn!=rl_active())return 0;
    int side=*(const unsigned char*)((const char*)main+0x314);
    if(side>1 || *(const unsigned char*)((const char*)other+0x314)!=(1-side)
        || (side ? rl_left(pawn) : rl_right ? rl_right(pawn) : 0)!=other)return 0;
    void* primary=*(void* const*)((const char*)gun+0x270),*ammo=*(void* const*)((const char*)gun+0x8e0);
    void* ammo_primary=reload_ammo_primary(gun,pawn);
    if(!reload_grip_registered(primary) || *(void* const*)((const char*)primary+0x20)!=gun
        || f_GetCurrentGrip(main)!=primary || rl_primary(gun)!=main || !ammo_primary)return 0;
    void* values[7]={pawn,main,other,gun,primary,ammo,ammo_primary};
    for(int i=0;i<7;i++){objects[i]=values[i];memcpy(&names[i],(const char*)values[i]+0x18,8);}return 1;
}
static void reload_release_input(void) {
    reload_clear_mobile_holster();
    if (rl_grip_until && g_cmd_until[rl_grip_bit]==rl_grip_until) g_cmd_until[rl_grip_bit]=rl_prior_grip_until;
    if (rl_eject_until && g_cmd_until[rl_eject_bit]==rl_eject_until) g_cmd_until[rl_eject_bit]=rl_prior_eject_until;
    rl_grip_until=rl_eject_until=0;rl_hold_pose_valid=0;
}
static void reload_stop(const char* why) {
    reload_release_input();
    if (rl_post.pulse_until) {
        long owned=rl_post.pulse_until;
        (void)__atomic_compare_exchange_n(&g_cmd_until[rl_post.trigger_bit],&owned,0,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
    }
    if (rl_deadline) LOGF("RELOAD stop: %s phase=%d", why, g_reload_phase);
    g_reload_phase = 0; rl_deadline = 0; rl_pawn = 0; rl_weapon = 0; rl_hold_pose_valid = 0; rl_approach_t0 = 0;
    memset(&rl_post,0,sizeof rl_post);
    memset(&rl_run,0,sizeof rl_run);rl_main_hand=rl_other_hand=0;rl_family=0;
    reload_family_post_cancel();
}
static void reload_cock_tick(void* right,long now) {
    int gt=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE);
    if (gt<=0 || gettid()!=gt || !right || right!=rl_post.binding.object[0]) return;
    if (!rl_post.valid || now<=0 || now>rl_deadline || rl_deadline-now>5000
        || __atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE)
        || __atomic_load_n(&g_touch_weapon_request,__ATOMIC_ACQUIRE)
        || __atomic_load_n(&g_touch_weapon,__ATOMIC_ACQUIRE)!=2
        || __atomic_load_n(&g_touch_context,__ATOMIC_ACQUIRE)!=1
        || rl_paused() || !rl_player_valid()) { reload_stop("cocking cancelled");return; }
    struct rl_post_binding current;
    if (!rl_post_binding(right,&current) || memcmp(&current,&rl_post.binding,sizeof current)) {
        reload_stop("cocking ownership changed");return;
    }
    void* gun=current.object[2],*left=current.object[3],*ammo=current.object[6];
    int trigger_bit=*(const unsigned char*)((const char*)right+0x314)==1 ? 26 : 28;
    int cocked=*(const unsigned char*)((const char*)gun+0xdd5)!=0;
    if (cocked) {
        if (!rl_pistol_ready(gun)) { reload_stop("cocked but unavailable");return; }
        if (g_reload_phase==6) {
            if (rl_post.pulse_until) {
                long owned=rl_post.pulse_until;
                (void)__atomic_compare_exchange_n(&g_cmd_until[trigger_bit],&owned,0,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
                rl_post.pulse_until=0;
                LOGF("RELOAD native cocking confirmed; awaiting trigger up");
            }
            // Keep Reload active until the native input route sees UP too, so
            // the user's next normal Fire is a new edge rather than this hold.
            if (rl_trigger(gun) || *(const unsigned char*)((const char*)right+0x33c)
                || touch_reload_user_fire(now)) return;
        }
        reload_stop("finished cocked");return;
    }
    if (touch_reload_user_fire(now)) { reload_stop("user Fire owns trigger");return; }
    long rt=__atomic_load_n(&g_cmd_until[trigger_bit],__ATOMIC_ACQUIRE);
    if (rl_post.pulse_until) {
        if (rt!=rl_post.pulse_until || now>=rl_post.pulse_until) reload_stop("cocking edge not confirmed");
        return; // One normal edge only; a ready pistol must never receive another.
    }
    if (rt>now) { reload_stop("foreign RT owns trigger");return; }
    if (rl_reloading(gun) || !rl_base_ready(gun) || rl_pistol_ready(gun)
        || rl_cock_blending(gun) || rl_cock_flourishing(gun)
        || rl_slide_hand(gun) || rl_held_ammo_ready(gun,0.0f)
        || rl_primary(ammo) || f_GetCurrentGrip(left)
        || rl_trigger(gun) || *(const unsigned char*)((const char*)right+0x33c)
        || *(const unsigned char*)((const char*)gun+0xdb8)
        || *(const unsigned char*)((const char*)gun+0xd22)
        || *(const unsigned char*)((const char*)gun+0xd23)) { rl_post.up_since=0;return; }
    if (!rl_post.up_since) { rl_post.up_since=now;return; }
    if (now<rl_post.up_since || now-rl_post.up_since<150) return;
    long live=ptr_now_ms();
    if (live<now || live>=rl_deadline || touch_reload_user_fire(live)
        || __atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE) || !g_reload_phase
        || __atomic_load_n(&g_touch_weapon_request,__ATOMIC_ACQUIRE)
        || __atomic_load_n(&g_touch_weapon,__ATOMIC_ACQUIRE)!=2 || rl_type()!=1
        || !rl_post_binding(right,&current) || memcmp(&current,&rl_post.binding,sizeof current)
        || *(const unsigned char*)((const char*)gun+0xdd5) || rl_pistol_ready(gun)
        || rl_cock_blending(gun) || rl_cock_flourishing(gun)
        || !rl_base_ready(gun) || rl_slide_hand(gun) || rl_held_ammo_ready(gun,0.0f)
        || rl_primary(ammo) || f_GetCurrentGrip(left) || rl_trigger(gun)
        || *(const unsigned char*)((const char*)gun+0xdb8)
        || *(const unsigned char*)((const char*)gun+0xd22)
        || *(const unsigned char*)((const char*)gun+0xd23)) { reload_stop("cocking edge cancelled");return; }
    // Re-resolve after the scalar gates; no earlier native callback may leave
    // a stale selected item, pending Home or ready-to-fire pistol for this edge.
    if (!rl_post_binding(right,&current) || memcmp(&current,&rl_post.binding,sizeof current)
        || __atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE)
        || __atomic_load_n(&g_touch_weapon_request,__ATOMIC_ACQUIRE)
        || __atomic_load_n(&g_touch_weapon,__ATOMIC_ACQUIRE)!=2
        || __atomic_load_n(&g_touch_context,__ATOMIC_ACQUIRE)!=1 || g_reload_phase!=5
        || *(const unsigned char*)((const char*)gun+0xdd5)
        || *(const unsigned char*)((const char*)right+0x33c)
        || *(const unsigned char*)((const char*)gun+0xdb8)
        || *(const unsigned char*)((const char*)gun+0xd22)
        || *(const unsigned char*)((const char*)gun+0xd23)) { reload_stop("cocking edge cancelled");return; }
    live=ptr_now_ms();
    if (live<now || live>=rl_deadline || touch_reload_user_fire(live)
        || __atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE) || g_reload_phase!=5) {
        reload_stop("cocking edge cancelled");return;
    }
    long until=live+150<rl_deadline ? live+150 : rl_deadline;
    // The original pistol PreBio4Tick routes this first uncocked edge through
    // its authored SetSlideCocked branch, before its ready-only TryFire path.
    if (!__atomic_compare_exchange_n(&g_cmd_until[trigger_bit],&rt,until,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
        reload_stop("RT ownership changed");return;
    }
    rl_post.pulse_until=until;rl_post.trigger_bit=trigger_bit;
    int phase=5;
    if (!__atomic_compare_exchange_n(&g_reload_phase,&phase,6,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)
        || __atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE)
        || __atomic_load_n(&g_touch_weapon_request,__ATOMIC_ACQUIRE)
        || __atomic_load_n(&g_touch_weapon,__ATOMIC_ACQUIRE)!=2
        || __atomic_load_n(&g_touch_context,__ATOMIC_ACQUIRE)!=1 || touch_reload_user_fire(live)) {
        reload_stop("cocking edge cancelled");return;
    }
    LOGF("RELOAD normal cocking edge gun=%p until=%ld",gun,until);
}
static void touch_reload_tick(void* hand, int side, long now) {
    int gt=__atomic_load_n(&g_touch_gt_tid,__ATOMIC_ACQUIRE);
    if (gt<=0 || gettid()!=gt) return;
    if (!rl_player || !rl_active || !rl_left || !hand || side < 0 || side > 1
        || !reload_layout(hand,0x430) || *(const unsigned char*)((const char*)hand+0x314)!=side) return;
    void* active = rl_active();
    void* pawn = rl_player(hand);
    if ((!g_reload_phase || !rl_deadline) && rl_hover_hand == hand) {
        if (reload_grip_registered(rl_hover_grip)) f_SetGripForcedHover(hand,rl_hover_grip,0);
        rl_hover_hand = 0; rl_hover_grip = 0;
    }
    if (!g_reload_phase && rl_deadline) reload_stop("cancelled");
    if (g_reload_phase && (active != rl_pawn || now > rl_deadline))
        reload_stop("active pawn changed or timed out");
    if(g_reload_phase && (g_touch_context!=1 || g_touch_cancel || g_touch_weapon_request || g_touch_weapon!=2
        || g_touch_inventory_request || g_touch_pause || g_touch_interact_request || touch_quickselect_busy()))
        reload_stop("input context changed");
    if (__atomic_load_n(&g_touch_reload_request,__ATOMIC_ACQUIRE)) {
        if (!pawn || pawn != active) return;
        void* gun=touch_equipped_gun(hand);
        if(!gun){
            void* other=side ? rl_left(pawn) : rl_right ? rl_right(pawn) : 0;
            // Preserve the queue for the other actual primary Tick, but never replay
            // an unavailable Reload after a later grenade/knife/weapon transition.
            if(!other || !touch_equipped_gun(other))__atomic_store_n(&g_touch_reload_request,0,__ATOMIC_RELEASE);
            return;
        }
        if (!__atomic_exchange_n(&g_touch_reload_request,0,__ATOMIC_ACQ_REL)) return;
        if (g_reload_phase) return;
        if(g_touch_context!=1 || g_touch_cancel || g_touch_inventory_request || g_touch_pause
            || g_touch_interact_request || touch_quickselect_busy())return;
        void* left = side ? rl_left(pawn) : rl_right ? rl_right(pawn) : 0;
        int angle=0xd50,family=reload_is_pistol(gun) ? 0 : reload_family_info(gun,&angle);
        if (!reload_layout(gun,0xd54) || !reload_layout(left,0x430)
            || *(const unsigned char*)((const char*)left+0x314)!=(1-side) || rl_player(left)!=pawn
            || rl_paused() || !rl_player_valid() || rl_primary(gun) != hand
            || rl_interactivity(gun) != 2 || (!reload_is_pistol(gun) && !family) || !rl_ammo_available(gun)
            || angle<0xd38 || angle>0x1300 || !reload_layout(gun,angle+4)
            || !isfinite(*(const float*)((const char*)gun+angle))) {
            LOGF("RELOAD unavailable (owned supported gun, ammo and free off-hand required)"); return;
        }
        void* ammo = *(void**)((char*)gun+0x8e0);
        void* grip = reload_ammo_primary(gun,pawn);
        void* held = f_GetCurrentGrip(left);
        void* holster=0;
        if (!grip || (held && held != grip) || rl_reloading(gun)
            || (!held && !reload_ammo_holster_grip(ammo,pawn,&holster))) {
            LOGF("RELOAD unavailable (off-hand occupied or already reloading)"); return;
        }
        memset(&rl_run,0,sizeof rl_run);
        if(!reload_run_binding(hand,left,gun,pawn,rl_run.object,rl_run.name))return;
        rl_run.valid=1;rl_main_hand=hand;rl_other_hand=left;rl_main_side=side;rl_family=family;rl_angle_offset=angle;
        rl_shell_count=-1;
        if(family==5) {
            int pending=reload_family_shells_pending(gun,&rl_shell_count);
            if(pending<=0){reload_stop(pending<0 ? "invalid shotgun ammo getters" : "shotgun full or reserves empty");return;}
        }
        rl_grip_bit=side ? 29 : 27;rl_eject_bit=side ? 1 : 9;
        rl_pawn = pawn; rl_weapon = gun; rl_deadline = now+(family ? 12000 : 5000); rl_eject_end = now+180;
        memset(&rl_post,0,sizeof rl_post);
        rl_post.valid=rl_post_binding(hand,&rl_post.binding);
        rl_prior_grip_until = g_cmd_until[rl_grip_bit]; rl_grip_until = 0;
        rl_prior_eject_until = g_cmd_until[rl_eject_bit]; rl_eject_until = 0;
        if (g_cmd_until[rl_eject_bit] < rl_eject_end) {
            g_cmd_until[rl_eject_bit] = rl_eject_end; rl_eject_until = rl_eject_end;
        }
        g_reload_phase = 1;
        LOGF("RELOAD request gun=%p ammo=%p", gun, ammo);
    }
    if (!g_reload_phase) return;
    if(g_reload_phase==7){
        int state=reload_family_post_tick(hand,side,now);
        if(state==2)reload_stop("finished native family cycle");else if(state==3)reload_stop("native family cycle refused");
        return;
    }
    if (g_reload_phase>=5 && g_reload_phase!=8) {
        if (hand==rl_post.binding.object[0]) reload_cock_tick(hand,now);
        return;
    }
    // Other pooled gameplay hands can keep ticking; they do not own this macro.
    if (pawn != rl_pawn || (rl_run.valid ? hand!=rl_main_hand && hand!=rl_other_hand : side==0 && rl_left(pawn)!=hand)) return;
    void* gun = rl_run.valid ? touch_equipped_gun(rl_main_hand) : rl_gun1(pawn);
    if(rl_run.valid){
        void* objects[7];unsigned long long names[7];
        if(!reload_run_binding(rl_main_hand,rl_other_hand,gun,pawn,objects,names)
            || memcmp(objects,rl_run.object,sizeof objects) || memcmp(names,rl_run.name,sizeof names)){
            reload_stop("reload ownership changed");return;
        }
    }
    if (gun != rl_weapon || !gun || now > rl_deadline || rl_paused() || !rl_player_valid()
        || rl_interactivity(gun) != 2 || (side == rl_main_side && rl_primary(gun) != hand)) {
        reload_stop("state changed or timed out"); return;
    }
    if (side == rl_main_side) return;
    void* ammo = *(void**)((char*)gun+0x8e0);
    void* grip = reload_ammo_primary(gun,pawn);
    if (!grip) { reload_stop("ammo removed or ownership changed"); return; }
    if(g_reload_phase==8) {
        if(now<rl_eject_end || f_GetCurrentGrip(hand) || rl_primary(ammo) || ti_down(hand))return;
        g_reload_phase=1;rl_approach_t0=0;
    }
    if (g_reload_phase == 4) {
        if (!rl_reloading(gun)) {
            reload_release_input();
            if(rl_family==5) {
                int loaded=-1,pending=reload_family_shells_pending(gun,&loaded);
                if(pending<0 || loaded<=rl_shell_count){reload_stop("shotgun shell did not advance");return;}
                rl_shell_count=loaded;
                if(pending) {
                    // Each native insertion consumes one real shell; wait for ammo-hand UP before the next.
                    rl_deadline=now+12000;rl_eject_end=now+180;g_reload_phase=8;return;
                }
            }
            if(rl_family){
                g_reload_phase=7;
                if(!reload_family_post_begin(rl_main_hand,gun,now))reload_stop("native family binding unavailable");
                return;
            }
            if (!rl_post.valid) { reload_stop("cocking binding unavailable");return; }
            int phase=4;
            if (__atomic_load_n(&g_touch_cancel,__ATOMIC_ACQUIRE)
                || !__atomic_compare_exchange_n(&g_reload_phase,&phase,5,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
                reload_stop("cocking cancelled");return;
            }
            rl_post.up_since=0;
            LOGF("RELOAD magazine loaded; awaiting native cocking");return;
        }
        rl_grip_until = now+600 < rl_deadline ? now+600 : rl_deadline;
        g_cmd_until[rl_grip_bit] = rl_grip_until;
        if (rl_hold_pose_valid) {
            void* root = *(void**)((char*)hand+0x130);
            const char* cache = (char*)gun+rl_hold_target_offset;
            if (!root || !*(void* const*)cache) { reload_stop("reload target removed"); return; }
            struct reload_tf target = rl_cached_transform(cache);
            if (!reload_marker_valid(&target)) {
                if (rl_transform_failure_logs<3) {
                    struct reload_tf current,capsule={0};
                    memcpy(&current,(const char*)root+0x1c0,sizeof current);
                    if (*(void* const*)((const char*)ammo+0x5b0)) capsule=rl_cached_transform((const char*)ammo+0x5b0);
                    reload_transform_failure("animated",hand,ammo,gun,rl_hold_target_offset,&current,&capsule,&target);
                }
                reload_stop("invalid animated target"); return;
            }
            struct reload_tf pose = reload_hand_pose(rl_hold_pose,rl_hold_target,target);
            if (!reload_tf_valid(&pose)) { reload_stop("invalid animated hand pose"); return; }
            rl_world_transform(root,&pose,0,0,0);
        }
        return;
    }
    if (now < rl_eject_end) return;
    void* held = f_GetCurrentGrip(hand);
    if (held && held != grip) { reload_stop("off-hand occupied"); return; }
    if (rl_reloading(gun)) { g_reload_phase = 4; return; }
    if (!held) {
        // A holstered Prop has no valid PrimaryGrip. Its authored HolsterGrip
        // handler checks CanBeGrabbedFromHolster, then TryGrabGrip(Ammo.Primary).
        void* holster=0;int ready=0;void* holster_grip=reload_ammo_holster_selection(ammo,pawn,&holster,&ready);
        if (!holster_grip) { reload_stop("ammo holster unavailable"); return; }
        if(!ready){
            // Retire only our previous attempt if the native selection changed
            // before its grip callback. A later eligible frame gets a new edge.
            reload_release_input();
            if(rl_hover_hand==hand){
                if(reload_grip_registered(rl_hover_grip))f_SetGripForcedHover(hand,rl_hover_grip,0);
                rl_hover_hand=rl_hover_grip=0;
            }
            g_reload_phase=1;return;
        }
        if (!reload_publish_holster(holster,hand,now)) return;
        if (rl_hover_hand!=hand || rl_hover_grip!=holster_grip) {
            if (rl_hover_hand==hand && reload_grip_registered(rl_hover_grip))
                f_SetGripForcedHover(hand,rl_hover_grip,0);
            f_SetGripForcedHover(hand,holster_grip,1);rl_hover_hand=hand;rl_hover_grip=holster_grip;
            LOGF("RELOAD normal ammo holster=%p grip=%p left=%p",holster,holster_grip,hand);
        }
        rl_grip_until = now+600; g_cmd_until[rl_grip_bit] = rl_grip_until; g_reload_phase = 2;
        return;
    }
    reload_clear_mobile_holster();
    if (rl_hover_hand==hand) {
        if (reload_grip_registered(rl_hover_grip)) f_SetGripForcedHover(hand,rl_hover_grip,0);
        rl_hover_hand=0;rl_hover_grip=0;
    }
    rl_grip_until = now+600; g_cmd_until[rl_grip_bit] = rl_grip_until;
    void* root = *(void**)((char*)hand+0x130);
    const char* target_cache = (char*)gun + (*(const unsigned char*)((char*)hand+0x36c) ? 0xcd0 : 0xc80);
    if (!root || !*(void**)((char*)ammo+0x5b0) || !*(void* const*)target_cache) {
        reload_stop("missing reload target"); return;
    }
    struct reload_tf current = *(const struct reload_tf*)((char*)root+0x1c0);
    struct reload_tf capsule = rl_cached_transform((char*)ammo+0x5b0);
    struct reload_tf target = rl_cached_transform(target_cache);
    if (!reload_tf_valid(&current) || !reload_marker_valid(&capsule) || !reload_marker_valid(&target)) {
        reload_transform_failure("insert",hand,ammo,gun,(int)(target_cache-(const char*)gun),&current,&capsule,&target);
        reload_stop("invalid reload transform"); return;
    }
    struct reload_tf pose = reload_hand_pose(current,capsule,target);
    if (!reload_tf_valid(&pose)) { reload_stop("invalid composed hand pose"); return; }
    float dx = pose.p[0]-current.p[0], dy = pose.p[1]-current.p[1], dz = pose.p[2]-current.p[2];
    if (dx*dx+dy*dy+dz*dz > 150.0f*150.0f) { reload_stop("target outside arm reach"); return; }
    // Draw the magazine around the grip, then feed it upward into the native well.
    if (g_reload_phase != 3 || !rl_approach_t0 || now < rl_approach_t0) {
        rl_approach_t0 = now; rl_approach_from = current; rl_approach_target=target;
        if(rl_family)rl_approach_from.p[2]-=14.0f;
    }
    int approach_ms=rl_family ? 170 : g_reload_approach_ms>0 ? g_reload_approach_ms : 1;
    float approach = (float)(now - rl_approach_t0) / (float)approach_ms;
    if (approach < 1.0f) {
        struct reload_tf mid=rl_family
            ? reload_tf_blend(&rl_approach_from,&pose,approach*approach*(3.0f-2.0f*approach))
            : reload_approach_pose(rl_approach_from,rl_approach_target,target,pose,approach);
        if (reload_tf_valid(&mid)) { rl_world_transform(root,&mid,0,0,0); g_reload_phase = 3; return; }
    }
    rl_world_transform(root,&pose,0,0,0); g_reload_phase = 3;
    rl_hold_pose = pose; rl_hold_target = target; rl_hold_pose_valid = 1;
    rl_hold_target_offset = (int)(target_cache-(char*)gun);
    if (rl_ready(gun,0.0f) && rl_overlap(gun,*(const float*)((char*)gun+rl_angle_offset))) {
        rl_begin(gun); g_reload_phase = 4;
        LOGF("RELOAD native BeginReload (ready+overlap)");
    }
}
static void install_touch_reload(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[2]; } check[] = {
        {0x633d030,{0xf9417800,0xd65f03c0}}, {0x62ac2c8,{0xf81e0ff3,0xa9017bfd}},
        {0x63380dc,{0xd0020168,0xf9431508}},
        {0x62a8db4,{0xf81e0ff3,0xa9017bfd}}, {0x62bbc30,{0xf9413800,0x17ff1a5f}},
        {0x62a8e50,{0xf81e0ff3,0xa9017bfd}}, {0x6272bbc,{0x52800020,0xd65f03c0}},
        {0x642e548,{0xd10183ff,0xf90023f3}}, {0x61d0934,{0xb0020ca8,0xf9446508}},
        {0x61d401c,{0xb0020c68,0xf946bd08}}, {0x628cf08,{0xf81e0ff3,0xa9017bfd}},
        {0x628d2e8,{0xa9be4ff4,0xa9017bfd}}, {0x628c410,{0xf9447000,0xb4000240}},
        {0x628fed0,{0xfc1d0fe8,0xa9014ff4}}, {0x6290128,{0xfc1d0fe8,0xf90007f5}},
        {0x628e1c4,{0xf81e0ff3,0xa9017bfd}}, {0x61e3c88,{0xd103c3ff,0x6d0933ed}},
        {0x83a926c,{0xd10303ff,0xa9085ff8}},
        {0x62bbd24,{0xf941e008,0xb4000268}}, {0x62bbd7c,{0xf941e808,0xb4000268}},
        {0x640dea8,{0xd10183ff,0xf90023f3}}, {0x6434184,{0xd10183ff,0xf90023f3}},
        {0x6272bd8,{0xd10143ff,0xa90257f6}}, {0x62bbea0,{0xf9412a69,0xf9413508}},
        {0x62c08c4,{0xa9bb67fa,0xa9015ff8}}, {0x62c098c,{0xf9413aa1,0xaa1403e0}},
        {0x6271e30,{0xf902bc01,0xd65f03c0}}, {0x62c0908,{0xb9835808,0xf94186d6}},
        {0x61e3cb0,{0xaa0003f4,0xaa0803f3}}, {0x61e3cdc,{0xad4e8001,0x3dc07002}},
        {0x61e4120,{0x2d03aa69,0xbd00266b}},
        {0x6424b5c,{0xd10183ff,0xf90023f3}}, {0x642c718,{0xd10183ff,0xf90023f3}},
        {0x64192f4,{0xd10183ff,0xf90023f3}}, {0x6338aec,{0xd0020168,0xf9431508}},
        {0x628d268,{0xf81e0ff3,0xa9017bfd}}, {0x62b6034,{0xf81e0ff3,0xa9017bfd}},
        {0x62b5ed8,{0xf9469c00,0x17ff31b5}}, {0x628e090,{0xfc1e0fe8,0xf90007f3}},
        {0x628d978,{0xf81e0ff3,0xa9017bfd}}, {0x62b59ac,{0xf9400268,0xaa1303e0}},
        {0x62b59bc,{0xb5000294,0x36000260}}, {0x62b5a30,{0x7100041f,0x5400006b}},
        {0x62b5e98,{0x1e2703e0,0xaa1303e0}}, {0x62b5ea4,{0xb5ffdcf4,0x3707dcc0}},
        {0x62b5eb4,{0xaa1303e0,0xf9442108}}, {0x62b6138,{0x12000028,0x39375408}},
        {0x642e5d0,{0x5281bc04,0x52800205}}, {0x62825b0,{0xf9417c08,0xb4000268}},
        {0x62bb880,{0xf81e0ff3,0xa9017bfd}}, {0x62bb584,{0xf81e0ff3,0xa9017bfd}},
        {0x62729d0,{0xf81e0ff3,0xa9017bfd}},
        {0x62c0978,{0xf94002a8,0xaa1503e0}}, {0x62c0980,{0xf9433908,0xd63f0100}},
    };
    for (unsigned i = 0; i < sizeof check/sizeof check[0]; i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,sizeof check[i].first)) {
            LOGF("RELOAD disabled: opcode mismatch +%lx",check[i].addr); return;
        }
    static const struct { unsigned long addr; unsigned value; } fields[] = {
        {0x9daded8+0x24,0xd38},{0x9d9e298+0x24,0x300},
    };
    for (unsigned i=0;i<sizeof fields/sizeof fields[0];i++)
        if (*(const unsigned*)(base+fields[i].addr)!=fields[i].value) {
            LOGF("RELOAD disabled: cocking field mismatch +%lx",fields[i].addr);return;
        }
    if (!reload_pose_selfcheck()) { LOGF("RELOAD disabled: pose self-check failed"); return; }
    rl_player = (void* (*)(const void*))(base+0x633d030);
    rl_active = (void* (*)(void))(base+0x63380dc);
    rl_gun1 = (void* (*)(const void*))(base+0x62ac2c8);
    rl_left = (void* (*)(const void*))(base+0x62a8db4);
    rl_right = (void* (*)(const void*))(base+0x62a8e50);
    rl_primary = (void* (*)(const void*))(base+0x62bbc30);
    rl_pistol_class = (void* (*)(void))(base+0x642e548);
    rl_ammo_class=(void* (*)(void))(base+0x640dea8);
    rl_holster_class=(void* (*)(void))(base+0x6434184);
    rl_get_pawn=(void* (*)(const void*))(base+0x62bbd24);
    rl_get_holster=(void* (*)(const void*))(base+0x62bbd7c);
    rl_can_grab_holstered_ammo=(int (*)(const void*))(base+0x62729d0);
    rl_paused = (int (*)(void))(base+0x61d0934);
    rl_player_valid = (int (*)(void))(base+0x61d401c);
    rl_interactivity = (int (*)(const void*))(base+0x628cf08);
    rl_ammo_available = (int (*)(const void*))(base+0x628d2e8);
    rl_reloading = (int (*)(void*))(base+0x628c410);
    rl_ready = (int (*)(const void*,float))(base+0x628fed0);
    rl_overlap = (int (*)(const void*,float))(base+0x6290128);
    rl_begin = (void (*)(void*))(base+0x628e1c4);
    rl_cached_transform = (struct reload_tf (*)(const void*))(base+0x61e3c88);
    rl_world_transform = (void (*)(void*,const struct reload_tf*,int,void*,int))(base+0x83a926c);
    rl_hand_class=(void* (*)(void))(base+0x6424b5c);
    rl_pawn_class=(void* (*)(void))(base+0x642c718);
    rl_grip_class=(void* (*)(void))(base+0x64192f4);
    rl_type=(int (*)(void))(base+0x6338aec);
    rl_base_ready=(int (*)(const void*))(base+0x628d268);
    rl_pistol_ready=(int (*)(const void*))(base+0x62b6034);
    rl_slide_hand=(void* (*)(const void*))(base+0x62b5ed8);
    rl_held_ammo_ready=(int (*)(const void*,float))(base+0x628e090);
    rl_trigger=(int (*)(const void*))(base+0x628d978);
    rl_cock_blending=(int (*)(const void*))(base+0x62bb880);
    rl_cock_flourishing=(int (*)(const void*))(base+0x62bb584);
    LOGF("RELOAD native helper installed (owned selected gun, original ammo animation, per-family completion)");
}
