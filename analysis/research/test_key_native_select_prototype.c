#define main qa_existing_fixture_tests
#include "test_qa_key_item_use.c"
#undef main
// The existing fixture tests cover the borrowed layout/weak guards. This file
// exercises the proposed synchronous call ordering, never a live engine world.
static unsigned char proto_hand[0x510],proto_grip[0x460];
static void* current_grip;
static unsigned proto_generation;
static int select_calls,auto_calls,hand_busy,root_moved,auto_refuse,identity_changed,no_pickup;
static int prototype_alive(const void* p){return p==proto_hand || p==proto_grip || pv_alive(p);}
static int prototype_root(uintptr_t base,const void* p,float out[3]){return qa_key_root(base,p,out);}
struct cinema_ui_weak {const void* object;unsigned generation;};
static void proto_weak_assign(struct cinema_ui_weak* out,const void* object){out->object=object;out->generation=proto_generation;}
static void* proto_weak_get(const struct cinema_ui_weak* weak){return weak->generation==proto_generation?(void*)weak->object:0;}
static int proto_slot(uintptr_t base,void* slot,unsigned item){
    assert(base==(uintptr_t)image);return ((unsigned char*)slot)[0x4c2] && !*(void**)((char*)slot+0x4e8) && ((unsigned char*)slot)[0x4c0]==item;
}
static void proto_select(uintptr_t base,void* menu,void* hand,unsigned item,const float* target){
    assert(base==(uintptr_t)image && menu==key_menu && hand==proto_hand && item==0xa4);
    select_calls++;if(identity_changed)proto_generation++;
    if(no_pickup)return;
    pointer(receptacle+0x3d0,actor);pointer(actor+0x288,hand);pointer(actor+0x260,proto_grip);actor[0x380]=3;actor[0x4ca]=1;
    pointer(proto_grip+0x20,actor);pointer(proto_grip+0xa0,actor);current_grip=proto_grip;
    pointer(actor+0x130,roots[2]);pointer(roots[2]+0x20,actor);pointer(roots[2]+0xa0,actor);
    memcpy(roots[2]+0x1d0,target,12);
    if(root_moved){float distant=1000;memcpy(roots[2]+0x1d0,&distant,4);}
}
static int proto_can_auto(uintptr_t base,void* owner,void* pickup){
    assert(base==(uintptr_t)image && owner==receptacle && pickup==actor);
    const float* root=(const float*)(roots[2]+0x1d0);const float* target=(const float*)(slots[0]+0x1d0);
    float distance=0;for(unsigned i=0;i<3;i++){float delta=root[i]-target[i];distance+=delta*delta;}
    return !auto_refuse && distance<30*30 && proto_slot(base,slots[0],0xa4);
}
static void proto_auto(uintptr_t base,void* pickup){
    assert(base==(uintptr_t)image && pickup==actor);auto_calls++;
    pointer(slots[0]+0x4e8,pickup);current_grip=0;pointer(actor+0x288,0);actor[0x4ca]=0;
}
#define RE4_KEY_NATIVE_PROTOTYPE_ENABLED
#define RE4_KEY_PROTO_SELECT(base,menu,hand,item,position) proto_select(base,menu,hand,item,position)
#define RE4_KEY_PROTO_CAN_SLOT(base,slot,item) proto_slot(base,slot,item)
#define RE4_KEY_PROTO_CAN_AUTO(base,owner,item) proto_can_auto(base,owner,item)
#define RE4_KEY_PROTO_AUTO(base,item) proto_auto(base,item)
#define RE4_KEY_PROTO_GRIP(base,hand) ((void)base,(void)hand,current_grip)
#define RE4_KEY_PROTO_HAND(base,hand,pawn) ((void)base,(void)pawn,(hand)==proto_hand && !hand_busy)
#define RE4_KEY_PROTO_GRIP_TYPE(base,grip) ((void)base,(grip)==proto_grip)
#define RE4_KEY_PROTO_WEAK_ASSIGN(out,object) proto_weak_assign(out,object)
#define RE4_KEY_PROTO_WEAK_GET(weak) proto_weak_get(weak)
#define pv_alive prototype_alive
#define qa_key_root prototype_root
#include "key_native_select_prototype.h"
#undef qa_key_root
#undef pv_alive
static void setup_prototype(void){
    setup(0xa4);counts[0]=1;proto_generation++;current_grip=0;
    select_calls=auto_calls=hand_busy=root_moved=auto_refuse=identity_changed=no_pickup=0;
    receptacle[0x3e0]=2;receptacle[0x414]=receptacle[0x416]=1;
    static const struct {uintptr_t at;unsigned a,b;} code[]={
        {0x62f8e88,0xd10203ff,0xf9001bf9},{0x62f7368,0x39530808,0x34000368},
        {0x62f564c,0xfc1b0fe8,0xa9015ff8},{0x62ee0c4,0xd10283ff,0xa90757f6},
        {0x629a774,0xf9421408,0xb4000268}};
    for(unsigned i=0;i<sizeof code/sizeof *code;i++){word(image+code[i].at,code[i].a);word(image+code[i].at+4,code[i].b);}
}
int main(void){
    image=calloc(1,0xa600000);assert(image);g_ue4_base_cached=(uintptr_t)image;
    void* selected=0;
    setup_prototype();assert(qa_key_select_at_slot(key_menu,receptacle,slots[0],proto_hand,0xa4,1,&selected)==QA_KEY_NATIVE_PLACED_PENDING_USE);
    assert(selected==actor && select_calls==1 && auto_calls==1 && counts[0]==1 && !counts[1] && !counts[2]);
    for(unsigned bad=0;bad<10;bad++){
        setup_prototype();
        switch(bad){
            case 0:context=2;break;case 1:hand_busy=1;break;case 2:current_grip=proto_grip;break;
            case 3:pointer(key_menu+0x298,0);break;case 4:receptacle[0x414]=0;break;
            case 5:receptacle[0x416]=0;break;case 6:counts[0]=0;break;case 7:slots[0][0x4c2]=0;break;
            case 8:menu_items[0]=0xa6;break;case 9:word(image+0x62ee0c4,0);break;
        }
        assert(qa_key_select_at_slot(key_menu,receptacle,slots[0],proto_hand,0xa4,1,&selected)==QA_KEY_NATIVE_REFUSED);
        assert(!selected && !select_calls && !auto_calls);
    }
    setup_prototype();assert(qa_key_select_at_slot(key_menu,receptacle,slots[0],proto_hand,0xa4,0,&selected)==QA_KEY_NATIVE_REFUSED && !select_calls);
    for(unsigned bad=0;bad<4;bad++){
        setup_prototype();
        if(bad==0)root_moved=1;else if(bad==1)auto_refuse=1;else if(bad==2)identity_changed=1;else no_pickup=1;
        assert(qa_key_select_at_slot(key_menu,receptacle,slots[0],proto_hand,0xa4,1,&selected)==QA_KEY_NATIVE_SELECTED_UNPLACED);
        assert(select_calls==1 && !auto_calls && counts[0]==1);
    }
    free(image);puts("PASS: synchronous Select-target/actual-root/native-CanAutoPlace/stock-Auto order; held/refused/moved/stale cases never force placement or consumption");
    return 0;
}
