#include <assert.h>
#include <stdio.h>
#include <string.h>
static void* (*phone_player)(const void*), *(*phone_active)(void), *(*rl_gun1)(const void*);
static unsigned (*phone_type)(void);
static void* (*rl_primary)(const void*), *(*f_GetCurrentGrip)(const void*);
static volatile int g_touch_context=3;
static int reload_grip_registered(void*);
static volatile int test_patch_result=-1;
static volatile unsigned long test_install_base;
static void* volatile test_patch_target;
static void* volatile test_patch_hook;
static int patch_code(void* target,void* hook) { test_patch_target=target;test_patch_hook=hook; return test_patch_result; }
#if !defined(__aarch64__)
static void fake_native(void*);
static void fake_avatar_native(void* p) { (void)p; }
#define RE4FIX_CUTSCENE_NATIVE_GATE fake_native
#define RE4FIX_CUTSCENE_AVATAR_NATIVE_GATE fake_avatar_native
#endif
#define LOGF(...) ((void)0)
#include "../re4fix_cutscene_weapon.h"

enum { PAWN,HAND,GUN,POSEABLE,SCENE,WRONG,CLASS_COUNT };
static struct {
    unsigned char pawn[0x1200],other[0x1200],hand[0x600],gun[0xe00],grip[0x400];
    unsigned char mesh[0x600],root[0x600],pivot[0x600],cinema[0x600],avatar[0x600];
    unsigned char classes[CLASS_COUNT][0x100],native_hand_class[0x100];
    void* chains[CLASS_COUNT][1],*native_hand_chain[1];
} f __attribute__((aligned(16)));
static unsigned type;
static void* active,*player,*slot,*primary,*current;
static int registered,native_desired,native_calls,visible_calls,order,type_calls,active_calls;
static void* pawn_class(void) { return f.classes[PAWN]; }
static void* hand_class(void) { return f.native_hand_class; }
static void* gun_class(void) { return f.classes[GUN]; }
static void* poseable_class(void) { return f.classes[POSEABLE]; }
static void* scene_class(void) { return f.classes[SCENE]; }
static unsigned get_type(void) { type_calls++; return type; }
static void* get_active(void) { active_calls++; return active; }
static void* get_player(const void* p) { assert(p==f.hand); return player; }
static void* get_gun1(const void* p) { assert(p==f.pawn); return slot; }
static void* get_primary(const void* p) { assert(p==f.gun); return primary; }
static void* get_current(const void* p) { assert(p==f.hand); return current; }
static int reload_grip_registered(void* p) { return p==f.grip && registered; }
static void visible(void* p,int value,int propagation) {
    assert(p==f.mesh && value==0 && propagation==0 && order==1);
    order=2; visible_calls++; *(unsigned short*)(f.mesh+0x14c)&=(unsigned short)~0x20;
}
#if !defined(__aarch64__)
static void fake_native(void* p) {
    assert(p==f.gun); native_calls++; order=1;
    unsigned short* flags=(unsigned short*)(f.mesh+0x14c);
    *flags=(*flags&(unsigned short)~0x20)|(native_desired ? 0x20 : 0);
}
#endif
static void object(unsigned char* p,int cls,void* owner) {
    *(void**)(p+0x10)=f.classes[cls]; *(void**)(p+0x20)=owner;
}
static void ready(void) {
    memset(&f,0,sizeof f);
    for (int i=0;i<CLASS_COUNT;i++) {
        f.chains[i][0]=f.classes[i]+0x30;
        *(void**)(f.classes[i]+0x30)=f.chains[i];
        *(int*)(f.classes[i]+0x58)=i==PAWN ? sizeof f.pawn : i==GUN ? sizeof f.gun : 0x600;
    }
    // The actual native base is smaller than its runtime Blueprint subclass.
    f.native_hand_chain[0]=f.native_hand_class+0x30;
    *(void**)(f.native_hand_class+0x30)=f.native_hand_chain;
    *(int*)(f.native_hand_class+0x58)=0x510;
    *(int*)(f.classes[HAND]+0x58)=0x598;
    f.chains[HAND][0]=f.native_hand_class+0x30;
    object(f.pawn,PAWN,0);object(f.hand,HAND,0);object(f.gun,GUN,0);
    object(f.mesh,POSEABLE,f.gun);object(f.root,SCENE,f.gun);object(f.pivot,SCENE,f.gun);object(f.grip,WRONG,f.gun);
    *(void**)(f.gun+0x3c0)=f.pawn; *(void**)(f.gun+0x270)=f.grip;
    *(void**)(f.gun+0x540)=f.mesh; *(void**)(f.gun+0x130)=f.root;
    *(void**)(f.gun+0x288)=f.pivot; *(void**)(f.pivot+0xc0)=f.root;
    *(void**)(f.mesh+0xc0)=f.pivot; *(unsigned short*)(f.mesh+0x14c)=0x220;
    f.hand[0x314]=1; memset(f.cinema,0x5a,sizeof f.cinema); memset(f.avatar,0xa5,sizeof f.avatar);
    type=3;active=player=f.pawn;slot=f.gun;primary=f.hand;current=f.grip;
    registered=native_desired=1;native_calls=visible_calls=order=type_calls=active_calls=0;
    cw_log_count=cw_reject_mask=0;g_touch_context=3;
    phone_type=get_type;phone_active=get_active;phone_player=get_player;
    rl_gun1=get_gun1;rl_primary=get_primary;f_GetCurrentGrip=get_current;
    cw_pawn_class=pawn_class;cw_hand_class=hand_class;cw_gun_class=gun_class;
    cw_poseable_class=poseable_class;cw_scene_class=scene_class;cw_visible=visible;
}
static void rejected(void) {
    unsigned char before[sizeof f];memcpy(before,&f,sizeof f);
    hook_cutscene_weapon(f.gun);
    assert(native_calls==1 && visible_calls==0 && order==1 && !memcmp(before,&f,sizeof f));
}
static void accepted(void) {
    unsigned char before[sizeof f];memcpy(before,&f,sizeof f);
    hook_cutscene_weapon(f.gun);
    assert(native_calls==1 && visible_calls==1 && order==2);
    assert(*(unsigned short*)(f.mesh+0x14c)==0x200);
    *(unsigned short*)(f.mesh+0x14c)|=0x20;
    assert(!memcmp(before,&f,sizeof f));
}
int main(int argc,char** argv) {
    (void)argv; if (argc==999) install_touch_cutscene_weapon(test_install_base);
    ready();accepted();ready();f.hand[0x314]=0;accepted();
    // Original first: each authored re-show is suppressed after that exact writer.
    ready();hook_cutscene_weapon(f.gun);hook_cutscene_weapon(f.gun);
    assert(native_calls==2 && visible_calls==2 && *(unsigned short*)(f.mesh+0x14c)==0x200);
    // Recovery belongs solely to the original method, including native false visibility.
    type=1;hook_cutscene_weapon(f.gun);
    assert(native_calls==3 && visible_calls==2 && *(unsigned short*)(f.mesh+0x14c)==0x220);
    native_desired=0;hook_cutscene_weapon(f.gun);
    assert(native_calls==4 && visible_calls==2 && *(unsigned short*)(f.mesh+0x14c)==0x200);
    for (unsigned i=0;i<10;i++) if(i!=3) { ready();type=i;rejected(); }
    ready();cw_visible=0;rejected();ready();phone_type=0;rejected();
    ready();phone_active=0;rejected();ready();phone_player=0;rejected();
    ready();rl_gun1=0;rejected();ready();rl_primary=0;rejected();ready();f_GetCurrentGrip=0;rejected();
    ready();active=0;rejected();ready();active=f.other;rejected();
    ready();*(void**)(f.pawn+0x10)=f.classes[WRONG];rejected();
    assert(!type_calls && !active_calls);
    ready();*(int*)(f.classes[PAWN]+0x58)=0xeaf;rejected();
    assert(!type_calls && !active_calls);
    ready();*(void**)(f.gun+0x10)=f.classes[WRONG];rejected();
    ready();*(int*)(f.classes[GUN]+0x58)=0x547;rejected();
    // Actual CDO/no-owner and a non-Cutscene stored owner decline before manager access.
    // The hook still ran the original method exactly once in each rejected() case.
    ready();*(void**)(f.gun+0x3c0)=0;rejected();assert(!type_calls && !active_calls);
    ready();*(void**)(f.gun+0x3c0)=f.other;rejected();assert(!type_calls && !active_calls);
    ready();slot=0;rejected();
    ready();slot=f.other;rejected();ready();registered=0;rejected();
    ready();*(void**)(f.gun+0x270)=0;rejected();ready();*(void**)(f.grip+0x20)=f.other;rejected();
    ready();primary=0;rejected();ready();*(void**)(f.hand+0x10)=f.classes[WRONG];rejected();
    ready();*(int*)(f.classes[HAND]+0x58)=0x50f;rejected();ready();f.hand[0x314]=2;rejected();
    ready();player=f.other;rejected();ready();current=0;rejected();ready();current=f.other;rejected();
    ready();*(void**)(f.gun+0x540)=0;rejected();
    ready();*(void**)(f.mesh+0x10)=f.classes[WRONG];rejected();
    ready();*(void**)(f.mesh+0x20)=f.other;rejected();ready();*(void**)(f.mesh+0xc0)=f.other;rejected();
    ready();*(void**)(f.root+0x10)=f.classes[WRONG];rejected();ready();*(void**)(f.root+0x20)=f.other;rejected();
    ready();*(void**)(f.gun+0x288)=0;rejected();
    ready();*(void**)(f.pivot+0x10)=f.classes[WRONG];rejected();
    ready();*(void**)(f.pivot+0x20)=f.other;rejected();
    ready();*(void**)(f.pivot+0xc0)=f.other;rejected();
    // The constructor's direct attachment is replaced by native Gun.BeginPlay.
    ready();*(void**)(f.mesh+0xc0)=f.root;rejected();assert(cw_reject_mask==(1u<<7));
    ready();*(int*)(f.classes[POSEABLE]+0x58)=0x14f;rejected();
    ready();*(int*)(f.classes[SCENE]+0x58)=0x10001;rejected();
    ready();for(int i=0;i<20;i++)hook_cutscene_weapon(f.gun);assert(cw_log_count==8);
    ready();*(void**)(f.mesh+0xc0)=f.other;
    for(int i=0;i<20;i++)hook_cutscene_weapon(f.gun);
    assert(cw_reject_mask==(1u<<7) && native_calls==20 && !visible_calls);
    ready();g_touch_context=1;*(void**)(f.mesh+0xc0)=f.other;rejected();assert(!cw_reject_mask);
    ready();g_touch_context=1;accepted(); // Snapshot is diagnostic-only; native context remains authoritative.
    puts("PASS: actual cutscene weapon C, native Hand base510/runtime BP598; original-first/reapply/native recovery; CDO/foreign-owner no manager calls; exact owned leaf only; flags, grip, avatar and cinema immutable; eight-log bound");
    return 0;
}
