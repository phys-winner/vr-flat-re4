#include <assert.h>
#include <stdio.h>
#include <string.h>

static void* (*phone_player)(const void*), *(*phone_active)(void);
static unsigned (*phone_type)(void);
#define LOGF(...) ((void)0)
#include "../re4fix_menu_visual.h"

static struct {
    unsigned char hand[0x600], pawn[0x1200], other[0x1200], meshset[0x300];
    unsigned char line[0x600], dot[0x600], primary[0x600], secondary[0x600];
    unsigned char root[0x600], pawn_root[0x600], widget[0x600], gun[0x600], foreign[0x600];
    unsigned char classes[7][0x100];
    void* chains[7][1];
} f __attribute__((aligned(16)));
static void* active, *player, *mesh_result;
static unsigned type;
static int hidden_calls, visible_calls, mesh_calls;
enum { HAND, PAWN, MESHSET, SCENE, STATIC, SKELETAL, WRONG };
static void* hand_class(void) { return f.classes[HAND]; }
static void* pawn_class(void) { return f.classes[PAWN]; }
static void* meshset_class(void) { return f.classes[MESHSET]; }
static void* scene_class(void) { return f.classes[SCENE]; }
static void* static_class(void) { return f.classes[STATIC]; }
static void* skeletal_class(void) { return f.classes[SKELETAL]; }
static void* get_player(const void* h) { assert(h==f.hand); return player; }
static void* get_active(void) { return active; }
static unsigned get_type(void) { return type; }
static void* get_mesh(const void* p,unsigned char side) {
    assert(p==f.pawn && side<2); mesh_calls++;
    return mesh_result ? mesh_result : side ? f.primary : f.secondary;
}
static void hidden(void* component,int value,int propagation) {
    assert((component==f.line || component==f.dot) && value==1 && propagation==0);
    hidden_calls++; *(unsigned short*)((char*)component+0x14c)|=0x400;
}
static void visible(void* component,int value,int propagation) {
    assert((component==f.primary || component==f.secondary) && value==0 && propagation==0);
    visible_calls++; *(unsigned short*)((char*)component+0x14c)&=(unsigned short)~0x20;
}
static void object(unsigned char* p,int cls,void* owner) {
    *(void**)(p+0x10)=f.classes[cls]; *(void**)(p+0x20)=owner;
    *(unsigned short*)(p+0x14c)=0x20;
}
static void ready(void) {
    memset(&f,0,sizeof f); memset(mv_log_count,0,sizeof mv_log_count);
    for (int i=0;i<7;i++) {
        f.chains[i][0]=f.classes[i]+0x30;
        *(void**)(f.classes[i]+0x30)=f.chains[i];
        *(int*)(f.classes[i]+0x58)=i==PAWN ? sizeof f.pawn : i==MESHSET ? sizeof f.meshset : 0x600;
    }
    object(f.hand,HAND,0); object(f.pawn,PAWN,0); object(f.meshset,MESHSET,0);
    object(f.line,STATIC,f.hand); object(f.dot,STATIC,f.hand);
    object(f.root,SCENE,f.meshset); object(f.pawn_root,SCENE,f.pawn);
    object(f.primary,SKELETAL,f.meshset); object(f.secondary,SKELETAL,f.meshset);
    object(f.foreign,SKELETAL,f.other);
    *(void**)(f.hand+0x510)=f.widget; *(void**)(f.hand+0x518)=f.line; *(void**)(f.hand+0x520)=f.dot;
    f.hand[0x314]=1; f.hand[0x53c]=1; f.hand[0x53d]=1; f.hand[0x53e]=1;
    *(void**)(f.pawn+0x130)=f.pawn_root; *(void**)(f.pawn+0xba8)=f.meshset;
    *(void**)(f.meshset+0x130)=f.root; *(void**)(f.meshset+0x220)=f.root;
    *(void**)(f.meshset+0x228)=f.primary; *(void**)(f.meshset+0x230)=f.secondary;
    *(void**)(f.root+0xc0)=f.pawn_root;
    *(void**)(f.primary+0xc0)=f.root; *(void**)(f.secondary+0xc0)=f.root;
    memset(f.widget,0x5a,sizeof f.widget); memset(f.gun,0xa5,sizeof f.gun);
    active=player=f.pawn; type=2; mesh_result=0; hidden_calls=visible_calls=mesh_calls=0;
    phone_player=get_player; phone_active=get_active; phone_type=get_type;
    mv_hand_class=hand_class; mv_pawn_class=pawn_class; mv_meshset_class=meshset_class;
    mv_scene_class=scene_class; mv_static_class=static_class; mv_skeletal_class=skeletal_class;
    mv_get_mesh=get_mesh; mv_visible=visible; mv_hidden=hidden;
}
static void rejected(void) {
    unsigned char before[sizeof f]; memcpy(before,&f,sizeof f);
    touch_menu_visual_tick(f.hand);
    assert(!hidden_calls && !visible_calls && !mesh_calls && !memcmp(before,&f,sizeof f));
}
static void allowed(void) {
    unsigned char before[sizeof f]; memcpy(before,&f,sizeof f);
    touch_menu_visual_tick(f.hand); assert(hidden_calls==2 && visible_calls==1 && mesh_calls==1);
    // Only the three exact native leaf render flags may differ from the fixture.
    *(unsigned short*)(f.line+0x14c)&=(unsigned short)~0x400;
    *(unsigned short*)(f.dot+0x14c)&=(unsigned short)~0x400;
    unsigned char* mesh=f.hand[0x314] ? f.primary : f.secondary;
    *(unsigned short*)(mesh+0x14c)|=0x20;
    assert(!memcmp(before,&f,sizeof f));
}
static void avatar_rejected(void) {
    touch_menu_visual_tick(f.hand); assert(hidden_calls==2 && !visible_calls);
    assert(*(unsigned short*)(f.primary+0x14c)==0x20 && *(unsigned short*)(f.secondary+0x14c)==0x20);
    assert(f.hand[0x53c]==1 && f.hand[0x53d]==1 && f.hand[0x53e]==1);
}
int main(int argc,char** argv) {
    (void)argv; if (argc==999) install_touch_menu_visual(0);
    ready(); allowed(); ready(); type=4; allowed(); ready(); type=7; allowed();
    ready(); type=7; f.hand[0x314]=0; allowed(); ready(); f.hand[0x314]=0; allowed();
    ready(); touch_menu_visual_tick(f.hand); touch_menu_visual_tick(f.hand);
    assert(hidden_calls==2 && visible_calls==1 && mesh_calls==2);
    // A later native authored visibility update is not blocked by a hidden bit.
    *(unsigned short*)(f.primary+0x14c)|=0x20;
    touch_menu_visual_tick(f.hand); assert(visible_calls==2 && hidden_calls==2);
    type=1; *(unsigned short*)(f.primary+0x14c)|=0x20;
    touch_menu_visual_tick(f.hand); assert(visible_calls==2 && hidden_calls==2 && mesh_calls==3);
    assert(*(unsigned short*)(f.primary+0x14c)==0x20);
    ready(); type=7; touch_menu_visual_tick(f.hand);
    *(unsigned short*)(f.primary+0x14c)|=0x20; // Native author writer can re-show the avatar.
    touch_menu_visual_tick(f.hand); assert(visible_calls==2 && hidden_calls==2);
    type=1; *(unsigned short*)(f.primary+0x14c)|=0x20;
    touch_menu_visual_tick(f.hand); assert(visible_calls==2 && hidden_calls==2 && mesh_calls==2);
    assert(*(unsigned short*)(f.primary+0x14c)==0x20);
    ready(); type=1; rejected(); ready(); type=3; rejected(); ready(); type=5; rejected();
    ready(); type=1; *(void**)(f.pawn+0x10)=f.classes[WRONG];
    touch_menu_visual_tick(f.hand);assert(hidden_calls==2 && !visible_calls && !mesh_calls);
    assert(*(unsigned short*)(f.primary+0x14c)==0x20 && *(unsigned short*)(f.secondary+0x14c)==0x20);
    ready(); type=7; active=f.other; rejected(); // Pooled game/menu hands cannot affect the active offer.
    ready(); type=7; player=f.other; rejected();
    ready(); type=7; *(void**)(f.hand+0x10)=f.classes[WRONG]; rejected();
    ready(); type=7; *(void**)(f.pawn+0x10)=f.classes[WRONG]; rejected();
    ready(); type=7; *(void**)(f.primary+0x20)=f.other; avatar_rejected();
    ready(); active=f.other; rejected(); ready(); player=f.other; rejected();
    ready(); player=0; rejected(); ready(); mv_hidden=0; rejected();
    ready(); phone_player=0; rejected(); ready(); phone_active=0; rejected(); ready(); phone_type=0; rejected();
    ready(); f.hand[0x314]=2; rejected();
    ready(); *(void**)(f.hand+0x10)=f.classes[WRONG]; rejected();
    ready(); *(int*)(f.classes[HAND]+0x58)=0x527; rejected();
    ready(); *(int*)(f.classes[HAND]+0x58)=0x10001; rejected();
    ready(); *(void**)(f.pawn+0x10)=f.classes[WRONG]; rejected();
    ready(); *(int*)(f.classes[PAWN]+0x58)=0xbaf; rejected();
    ready(); *(void**)(f.line+0x20)=f.other; touch_menu_visual_tick(f.hand);
    assert(hidden_calls==1 && visible_calls==1 && *(unsigned short*)(f.line+0x14c)==0x20);
    ready(); *(void**)(f.dot+0x10)=f.classes[WRONG]; touch_menu_visual_tick(f.hand);
    assert(hidden_calls==1 && visible_calls==1 && *(unsigned short*)(f.dot+0x14c)==0x20);
    ready(); *(void**)(f.pawn+0xba8)=0; avatar_rejected();
    ready(); *(void**)(f.meshset+0x10)=f.classes[WRONG]; avatar_rejected();
    ready(); *(void**)(f.meshset+0x130)=f.pawn_root; avatar_rejected();
    ready(); *(void**)(f.root+0x20)=f.other; avatar_rejected();
    ready(); *(void**)(f.pawn_root+0x20)=f.other; avatar_rejected();
    ready(); *(void**)(f.root+0xc0)=f.other; avatar_rejected();
    ready(); mesh_result=f.foreign; avatar_rejected();
    ready(); *(void**)(f.primary+0x20)=f.other; avatar_rejected();
    ready(); *(void**)(f.primary+0xc0)=f.pawn_root; avatar_rejected();
    ready(); *(void**)(f.primary+0x10)=f.classes[WRONG]; avatar_rejected();
    assert(!mesh_calls); // Native getter must not inspect a malformed field.
    ready(); *(void**)(f.secondary+0x10)=f.classes[WRONG]; avatar_rejected(); assert(!mesh_calls);
    ready(); *(int*)(f.classes[SKELETAL]+0x58)=0x437; avatar_rejected(); assert(!mesh_calls);
    ready(); *(void**)(f.secondary+0x20)=f.other; avatar_rejected(); assert(!mesh_calls);
    ready(); for (int i=0;i<20;i++) touch_menu_visual_tick(f.hand);
    assert(mv_log_count[1]==4 && !mv_log_count[0]);
    f.hand[0x314]=0; for (int i=0;i<20;i++) touch_menu_visual_tick(f.hand);
    assert(mv_log_count[0]==4 && mv_log_count[1]==4 && hidden_calls==2 && visible_calls==2);
    puts("PASS: actual menu visual C; active Pause/Menu/InWorldMenu only, typed owned exact ray/avatar leaves, pooled/type7 rejection, propagation0, pointer/widget/game gun immutable, native avatar visibility recovery and eight-log bound");
    return 0;
}
