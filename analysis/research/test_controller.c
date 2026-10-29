#include <assert.h>
#include <stdio.h>
#include <string.h>

static void* (*phone_player)(const void*);
static void* (*phone_active)(void);
static unsigned (*phone_type)(void);
static int phone_inspect_ready;
static void phone_inspect_fname(const void*,char*,int);
static void obj_name(const void*,char*,int);
#define LOGF(...) ((void)0)
#define CONTROLLER_TEST
static int patch_code(void* target,void* hook) { (void)target; (void)hook; return 1; }
#include "../re4fix_controller.h"
static void gate_tutorial_hand_begin(void* self) { (void)self; }

static struct {
    unsigned char hand[0x600], pawn[0x1300], other[0x1300], controller[0x600], mesh[0x100];
    unsigned char gun[0x600], hand_proxy[0x600], component_class[0x100], hand_class[0x100];
    unsigned char mesh_class[0x100], wrong_class[0x100], prop[0x70];
    void* chains[4][2];
} f __attribute__((aligned(16)));
static void* active;
static unsigned type;
static int hide_calls;
static const char* asset_name = "CMN_Controller_Quest2_Right_Base";
static const char* property_name = "Controller";
static const char* property_type = "ObjectProperty";
static void* get_player(const void* h) { assert(h == f.hand); return f.pawn; }
static void* get_active(void) { return active; }
static unsigned get_type(void) { return type; }
static void* hand_class(void) { return f.hand_class; }
static void* component_class(void) { return f.component_class; }
static void* mesh_class(void) { return f.mesh_class; }
static void phone_inspect_fname(const void* name,char* out,int size) {
    const char* text = name == f.prop+0x28 ? property_name : name == f.mesh ? property_type : "Unknown";
    snprintf(out,(size_t)size,"%s",text);
}
static void obj_name(const void* object,char* out,int size) {
    assert(object == f.mesh); snprintf(out,(size_t)size,"%s",asset_name);
}
static void hide(void* component,int hidden,int propagation) {
    assert(component == f.controller && hidden == 1 && propagation == 0);
    hide_calls++; *(unsigned short*)(f.controller+0x14c) |= 0x400;
}
static void set_class(unsigned char* cls,int slot,int size) {
    f.chains[slot][0] = cls+0x30;
    *(void**)(cls+0x30) = f.chains[slot]; *(int*)(cls+0x38) = 0; *(int*)(cls+0x58) = size;
}
static void ready(void) {
    memset(&f,0,sizeof f); memset(tc_seen_component,0,sizeof tc_seen_component); memset(tc_seen_mesh,0,sizeof tc_seen_mesh);
    memset(tc_check_count,0,sizeof tc_check_count);
    set_class(f.hand_class,0,sizeof f.hand); set_class(f.component_class,1,sizeof f.controller);
    set_class(f.mesh_class,2,sizeof f.mesh); set_class(f.wrong_class,3,sizeof f.hand);
    *(void**)(f.hand+0x10) = f.hand_class; *(void**)(f.hand_class+0x70) = f.prop;
    *(void**)(f.hand+0x548) = f.controller; *(void**)(f.hand+0x530) = f.hand_proxy;
    *(void**)(f.hand+0x538) = f.gun;
    *(void**)(f.prop+0x8) = f.mesh; *(int*)(f.prop+0x34) = 1; *(int*)(f.prop+0x38) = 8;
    *(int*)(f.prop+0x4c) = 0x548;
    *(void**)(f.controller+0x10) = f.component_class; *(void**)(f.controller+0x20) = f.hand;
    *(void**)(f.controller+0x440) = f.mesh; *(void**)(f.mesh+0x10) = f.mesh_class;
    active = f.pawn; type = 1; phone_inspect_ready = 1; hide_calls = 0;
    asset_name = "CMN_Controller_Quest2_Right_Base"; property_name = "Controller"; property_type = "ObjectProperty";
    phone_player = get_player; phone_active = get_active; phone_type = get_type;
    tc_hand_class = hand_class; tc_component_class = component_class; tc_mesh_class = mesh_class; tc_hide = hide;
}
static void rejected(void) {
    unsigned char before[sizeof f]; memcpy(before,&f,sizeof f); touch_controller_tick(f.hand);
    assert(!hide_calls && !memcmp(before,&f,sizeof f));
}
static void allowed(void) {
    unsigned char before[sizeof f]; memcpy(before,&f,sizeof f); touch_controller_tick(f.hand);
    assert(hide_calls == 1);
    *(unsigned short*)(f.controller+0x14c) &= (unsigned short)~0x400;
    assert(!memcmp(before,&f,sizeof f)); // Gun, HandProxy, transforms and input are untouched.
}
int main(int argc,char** argv) {
    (void)argv; if (argc == 999) install_touch_controller(0);
    ready(); allowed(); touch_controller_tick(f.hand); assert(hide_calls == 2);
    touch_controller_tick(f.hand); assert(hide_calls == 2); // no duplicate engine calls while hidden
    ready(); f.hand[0x314] = 1; allowed();
    ready(); asset_name = "CMN_Controller_Quest2_Left_Base"; allowed();
    ready(); active = f.other; rejected(); ready(); type = 5; rejected();
    ready(); phone_inspect_ready = 0; rejected(); ready(); tc_hide = 0; rejected();
    ready(); phone_player = 0; rejected(); ready(); phone_active = 0; rejected();
    ready(); f.hand[0x314] = 2; rejected();
    ready(); *(void**)(f.hand+0x10) = f.wrong_class; rejected();
    ready(); *(int*)(f.hand_class+0x58) = 0x54f; rejected();
    ready(); *(int*)(f.hand_class+0x58) = 0x10001; rejected();
    ready(); *(void**)(f.hand_class+0x70) = 0; rejected();
    ready(); property_name = "Gun"; rejected(); ready(); property_type = "StructProperty"; rejected();
    ready(); *(int*)(f.prop+0x4c) = 0x538; rejected();
    ready(); *(int*)(f.prop+0x34) = 2; rejected(); ready(); *(int*)(f.prop+0x38) = 16; rejected();
    ready(); *(void**)(f.hand+0x548) = 0; rejected();
    ready(); *(void**)(f.controller+0x20) = f.other; rejected();
    ready(); *(void**)(f.controller+0x10) = f.wrong_class; rejected();
    ready(); *(int*)(f.component_class+0x58) = 0x447; rejected();
    ready(); *(void**)(f.controller+0x440) = 0; rejected();
    ready(); *(void**)(f.mesh+0x10) = f.wrong_class; rejected();
    ready(); asset_name = "Wep02"; rejected(); ready(); asset_name = "Leon_Hand"; rejected();
    ready(); asset_name = "Controller"; rejected(); ready(); asset_name = "CMN_Controller_Quest1_Right_Base"; rejected();
    ready(); *(void**)(f.prop+0x58) = f.prop; property_name = "Other"; rejected();
    ready(); *(void**)(f.controller+0x20) = f.other;
    for (int i = 0; i < 20; i++) touch_controller_tick(f.hand);
    assert(tc_check_count[0] == 4 && !tc_check_count[1] && !hide_calls);
    f.hand[0x314] = 1;
    for (int i = 0; i < 20; i++) touch_controller_tick(f.hand);
    assert(tc_check_count[0] == 4 && tc_check_count[1] == 4 && !hide_calls);
    ready(); phone_inspect_ready = 0;
    for (int i = 0; i < 20; i++) touch_controller_tick(f.hand);
    assert(tc_check_count[0] == 4 && !hide_calls);
    assert(!controller_art_name(0) && !controller_art_name("") && !controller_art_name("Wep02"));
    puts("PASS: actual controller C, active/side/reflection/type/size/Outer/mesh guards, Quest2 leaf only, gun/handproxy/input/transforms immutable, propagation 0 and visibility reapply");
    return 0;
}
