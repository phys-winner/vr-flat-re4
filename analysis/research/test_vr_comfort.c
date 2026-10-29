#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* The checker prepends the actual shared cinema_object implementation. */
static int cinema_object(const void*,const void*,int);
static unsigned char pawn[0x600],other_pawn[0x600],player_class[0x60],other_class[0x60];
static void* class_chain[1];
static void* active;
static unsigned context;
static int on_gt,original_calls,patch_calls,patch_failure,logs;
static float original_alpha,original_scale;
static unsigned char original_scope;
static void* (*cinema_active)(void);
static unsigned (*cinema_type)(void);
static int cinema_on_game_thread(void) { return on_gt; }
static void* mock_active(void) { return active; }
static unsigned mock_type(void) { return context; }
static void* mock_class(void) { return player_class; }
static void mock_original(void* p,float* a,float* s,unsigned char* scope) {
    (void)p;original_calls++;
    if (a) *a=original_alpha;
    if (s) *s=original_scale;
    if (scope) *scope=original_scope;
}
static int patch_code(void* target,void* hook) { assert(target && hook);patch_calls++;return patch_failure; }
#define LOGF(...) ((void)++logs)
#ifndef TEST_REAL_GATE
#define RE4FIX_COMFORT_VIGNETTE_GATE mock_original
#endif
#include "re4fix_vr_comfort.h"

static void setup(void) {
    memset(pawn,0,sizeof pawn);memset(other_pawn,0,sizeof other_pawn);
    memset(player_class,0,sizeof player_class);memset(other_class,0,sizeof other_class);
    class_chain[0]=player_class+0x30;
    memcpy(player_class+0x30,&(void*){class_chain},sizeof(void*));
    *(int*)(player_class+0x38)=0;*(int*)(player_class+0x58)=0x570;
    memcpy(pawn+0x10,&(void*){player_class},sizeof(void*));
    active=pawn;context=1;on_gt=1;comfort_player_class=mock_class;
    cinema_active=mock_active;cinema_type=mock_type;
    original_alpha=.8f;original_scale=.35f;original_scope=0;
    original_calls=0;comfort_motion_logs=0;logs=0;
}
static void expect(int flattened) {
    struct { unsigned pre;float alpha,scale;unsigned char scope,tail[3];unsigned post; } out;
    memset(&out,0x91,sizeof out);out.pre=0x12345678;out.post=0x87654321;
    unsigned char before[sizeof pawn];memcpy(before,pawn,sizeof pawn);
    int calls=original_calls;
    hook_comfort_vignette(pawn,&out.alpha,&out.scale,&out.scope);
    assert(original_calls==calls+1);
    assert(out.alpha==(flattened?0:original_alpha));
    assert(out.scale==(flattened?1:original_scale));assert(out.scope==original_scope);
    assert(out.pre==0x12345678 && out.post==0x87654321);
    assert(out.tail[0]==0x91 && out.tail[1]==0x91 && out.tail[2]==0x91);
    assert(!memcmp(before,pawn,sizeof pawn));
}
int main(void) {
    setup();for (unsigned type=1;type<=7;type++) { context=type;expect(1); }
    original_scope=1;expect(1); /* Authored optical state is not overwritten. */
    setup();on_gt=0;expect(0);
    setup();active=other_pawn;expect(0);
    setup();context=0;expect(0);context=8;expect(0);
    setup();comfort_player_class=0;expect(0);
    setup();cinema_active=0;expect(0);
    setup();cinema_type=0;expect(0);
    setup();*(int*)(player_class+0x58)=0x56f;expect(0);
    setup();*(int*)(player_class+0x58)=0x10001;expect(0);
    setup();*(int*)(player_class+0x38)=-1;expect(0);
    setup();class_chain[0]=other_class+0x30;expect(0);
    setup();*(void**)(pawn+0x10)=0;expect(0);
    setup();float only=0;hook_comfort_vignette(pawn,&only,&only,0);
    assert(only==original_scale && original_calls==1);
    setup();hook_comfort_vignette(pawn,0,0,0);assert(original_calls==1);
    setup();for (int i=0;i<40;i++) expect(1);assert(comfort_motion_logs==8 && logs==8);
    setup();original_alpha=0;expect(1);assert(comfort_motion_logs==0);
    /* Native CameraCover combines alpha with its independent DamageWipeValue. */
    setup();float alpha=0,scale=0;unsigned char scope=0;
    float damage=.5f,damage_scale=.2f,none_scale=1;
    hook_comfort_vignette(pawn,&alpha,&scale,&scope);
    float motion_size=none_scale+alpha*(scale-none_scale);
    float damage_size=none_scale+damage*(damage_scale-none_scale);
    assert(motion_size==1 && damage_size>.59f && damage_size<.61f && damage==.5f);
    assert(damage_size<motion_size); /* Original damage cover remains visible. */
#ifdef _WIN32
    const unsigned long base=0x10000000UL;
    unsigned char* image=(unsigned char*)VirtualAlloc((void*)(uintptr_t)base,0xb000000,
                                                    MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(image==(void*)(uintptr_t)base);
    /* The checker supplies the native preimage arrays used by the real installer. */
    extern void fixture_image(unsigned char*);
    fixture_image(image);
    setup();comfort_player_class=0;patch_calls=0;patch_failure=0;
    install_touch_vr_comfort(base);assert(patch_calls==1 && comfort_player_class);
    assert(g_tgt_comfort_vignette==base+0x6345d1c+16);
    install_touch_vr_comfort(base);assert(patch_calls==1);
    extern unsigned long fixture_code_addresses[];extern unsigned fixture_code_count;
    for (unsigned i=0;i<fixture_code_count;i++) for (unsigned word=0;word<2;word++) {
        unsigned* p=(unsigned*)(image+fixture_code_addresses[i]+4*word);unsigned before=*p;
        *p^=1;setup();comfort_player_class=0;patch_calls=0;
        install_touch_vr_comfort(base);assert(!patch_calls && !comfort_player_class);*p=before;
    }
    extern unsigned long fixture_field_addresses[];extern unsigned fixture_field_count;
    for (unsigned i=0;i<fixture_field_count;i++) {
        unsigned* p=(unsigned*)(image+fixture_field_addresses[i]+36);unsigned before=*p;
        *p^=1;setup();comfort_player_class=0;patch_calls=0;
        install_touch_vr_comfort(base);assert(!patch_calls && !comfort_player_class);*p=before;
    }
    setup();comfort_player_class=0;patch_calls=0;patch_failure=1;
    install_touch_vr_comfort(base);assert(patch_calls==1 && !comfort_player_class);
    setup();comfort_player_class=0;cinema_active=0;patch_calls=0;patch_failure=0;
    install_touch_vr_comfort(base);assert(!patch_calls);
    assert(VirtualFree(image,0,MEM_RELEASE));
#endif
    puts("PASS: actual comfort getter preserves original calls/objects/scope/damage; GT/active/type guards, log bound and real installer refusals");
    return 0;
}
