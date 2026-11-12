#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#define RE4_QA_USER_TRANSITIONS
static uintptr_t g_ue4_base_cached;
static unsigned char *image,ita[16+41*0xd0],aev[16+29*0x100],player[0x500],pawn[16],pickup[0x4e0],inventory[14*4],enemies[0x111c*2],native_class[16];
static unsigned char merchant_component[0xc0],merchant_actor[16],merchant_class[16];
static unsigned char scene_entry[0x100],room_saves[512*0xd8];
static unsigned type;static int gt,valid,paused,alive,layout,collectable,finds,collections,opens,logs,item_count,count_calls,wrong_actor;
static int merchant_present,merchant_alive,merchant_layout,merchant_owner,merchant_probes;
static int scene_ready,scene_save_case,scene_entry_case,scene_schedule,scenes;
struct inventory_drag_ray {float origin[3],direction[3];};
struct inventory_drag_plane {float normal[3],distance;};
struct mg_ui_object {void* actor;void* component;void* screen;};
struct mg_ui_hit {float position[3],distance;};
static struct {
    unsigned char hand[0x570],manager[16],merchant[0x320],screen[0x3b0],cases[2][0x2a8],roots[2][0x1f0],item[0x320],button[0x330],actor[16];
} diag;
static unsigned char diag_classes[7][16];
static int diag_bindings,diag_current,diag_foreign,diag_hovered,diag_top_inventory,diag_bad_ray;
static int diag_hover_calls,diag_ray_calls,diag_plane_calls,diag_inventory_calls,diag_enabled_calls,diag_state_calls;
static const void *diag_dead,*diag_bad_layout;
static void* mg_seen_hand;
static char log_text[8192];static unsigned log_used;
static int cinema_on_game_thread(void){return gt;}
static unsigned type_get(void){return type;}
static void* active_get(void){return pawn;}
static int valid_get(void){return valid;}
static int paused_get(void){return paused;}
static int pv_alive(const void* p){
    if(!p || p==diag_dead)return 0;
    if(p==diag.hand || p==diag.manager || p==diag.merchant || p==diag.screen || p==diag.cases[0] || p==diag.cases[1]
        || p==diag.roots[0] || p==diag.roots[1] || p==diag.item || p==diag.button || p==diag.actor)return 1;
    return p==merchant_component || p==merchant_actor ? merchant_alive:alive && (p==pawn || p==pickup);
}
static int menu_visual_layout(const void* p,const void* cls,int size){
    if(p==diag_bad_layout)return 0;
    for(unsigned i=0;i<7;i++)if(cls==diag_classes[i]){
        if(i==0){assert(size==0x320);return p==diag.merchant;}
        if(i==1){assert(size==0x3b0);return p==diag.screen;}
        if(i==2){assert(size==0x320);return p==diag.item;}
        if(i==3){assert(size==0x2a8);return p==diag.cases[0] || p==diag.cases[1];}
        if(i==4){assert(size==0x330);return p==diag.button;}
        if(i==5){assert(size==0x248);return p==diag.merchant || p==diag.screen;}
        assert(size==0x1f0);return p==diag.roots[0] || p==diag.roots[1];
    }
    if(cls==merchant_class){assert(size==0xc0);return p==merchant_component && merchant_layout;}
    assert(cls==native_class && size==0x4e0);return p==pickup && layout;
}
static unsigned (*cinema_type)(void)=type_get;
static void* (*cinema_active)(void)=active_get;
static int (*rl_player_valid)(void)=valid_get,(*rl_paused)(void)=paused_get;
static void log_message(const char* fmt,...){
    logs++;if(log_used+2>=sizeof log_text)return;va_list args;va_start(args,fmt);
    int written=vsnprintf(log_text+log_used,sizeof log_text-log_used,fmt,args);va_end(args);
    if(written>0){unsigned available=(unsigned)sizeof log_text-log_used-1;log_used+=(unsigned)written>available?available:(unsigned)written;}
    if(log_used+2<sizeof log_text){log_text[log_used++]='\n';log_text[log_used]=0;}
}
#define LOGF(...) log_message(__VA_ARGS__)
static void ptr(void* p,const void* value){memcpy(p,&value,8);}
static void word(void* p,unsigned value){memcpy(p,&value,4);}
static void half(void* p,unsigned short value){memcpy(p,&value,2);}
static int mg_current(void* hand,void** out,unsigned* context){if(!gt || !diag_current || hand!=diag.hand)return 0;*out=diag_foreign?diag.actor:pawn;*context=type;return 1;}
static int diag_hover(struct mg_ui_object* object,struct mg_ui_hit* hit,void* hand){
    assert(gt && hand==diag.hand);diag_hover_calls++;
    if(diag_hovered){object->actor=diag.actor;object->component=diag.button;object->screen=diag.merchant;hit->distance=350;hit->position[0]=180;hit->position[2]=15100;}
    return diag_hovered;
}
static int diag_enabled(const void* object){assert(object==diag.button);diag_enabled_calls++;return 1;}
static struct inventory_drag_ray diag_ray(const void* hand){assert(gt && hand==diag.hand);diag_ray_calls++;struct inventory_drag_ray value={{-100,0,15171},{.8f,0,-.6f}};if(diag_bad_ray)value.direction[0]=NAN;return value;}
static void* diag_class0(void){return diag_classes[0];}static void* diag_class1(void){return diag_classes[1];}
static void* diag_class2(void){return diag_classes[2];}static void* diag_class3(void){return diag_classes[3];}
static void* diag_class4(void){return diag_classes[4];}static void* diag_class5(void){return diag_classes[5];}static void* diag_class6(void){return diag_classes[6];}
static void* diag_manager(void){return diag.manager;}static void* diag_top(void* manager){assert(manager==diag.manager);return diag_top_inventory?diag.screen:diag.merchant;}
static void* diag_inventory(uintptr_t base,const void* merchant){assert(base==(uintptr_t)image && merchant==diag.merchant);diag_inventory_calls++;return diag.screen;}
static unsigned char diag_state(uintptr_t base,const void* merchant){assert(base==(uintptr_t)image && gt && merchant==diag.merchant);diag_state_calls++;return diag.merchant[0x314];}
static struct inventory_drag_plane diag_plane(uintptr_t base,const void* object){
    assert(base==(uintptr_t)image && gt && (object==diag.cases[0] || object==diag.cases[1]));diag_plane_calls++;
    struct inventory_drag_plane plane={{0,0,1},15082};return plane;
}
static int (*mg_hover)(struct mg_ui_object*,struct mg_ui_hit*,void*)=diag_hover,(*mg_enabled)(const void*)=diag_enabled;
static void *(*inventory_drag_screen_class)(void)=diag_class1,*(*inventory_drag_item_class)(void)=diag_class2,*(*inventory_drag_case_class)(void)=diag_class3;
static void *(*mg_button_class)(void)=diag_class4,*(*mg_screen_class)(void)=diag_class5,*(*mv_scene_class)(void)=diag_class6;
static void *(*fv_ui_manager)(void)=diag_manager,*(*fv_peek_screen)(void*)=diag_top;
static struct inventory_drag_ray (*inventory_drag_ray)(const void*)=diag_ray;
static int native_count(uintptr_t base){assert(base==(uintptr_t)image && gt);count_calls++;return item_count;}
static void* native_find(uintptr_t base,unsigned id){assert(base==(uintptr_t)image && id==0x89 && gt);finds++;return !collectable?0:wrong_actor?pawn:pickup;}
static void native_collect(uintptr_t base,void* actor){assert(base==(uintptr_t)image && actor==pickup && gt);collections++;item_count++;}
static int native_open(uintptr_t base){assert(base==(uintptr_t)image && gt);opens++;return 1;}
static void* native_merchant_component(uintptr_t base,const void* model){assert(base==(uintptr_t)image && gt);merchant_probes++;return model==enemies && merchant_present?merchant_component:0;}
static void* native_merchant_owner(uintptr_t base,const void* model){assert(base==(uintptr_t)image && gt && model==enemies);return merchant_owner?merchant_actor:0;}
static const unsigned char* native_room_save(uintptr_t base,unsigned room){
    assert(base==(uintptr_t)image && gt && (room==0x101 || room==0x104));
    return scene_save_case==1?0:scene_save_case==2?room_saves+512*0xd8:scene_save_case==3?room_saves+1:room_saves+0xd8;
}
static const unsigned char* native_scene_ptr(uintptr_t base,unsigned id){
    assert(base==(uintptr_t)image && gt && (id==8 || id==18));
    return scene_entry_case==1?0:scene_entry_case==2?scene_entry+1:scene_entry_case==3?(const unsigned char*)(UINTPTR_MAX-3):scene_entry;
}
static int native_scene_execute(uintptr_t base,unsigned id){
    assert(base==(uintptr_t)image && gt && (id==8 || id==18));scenes++;
    if(scene_schedule){unsigned char* task=image+0xa5436e8+11*0x40;task[0]=1;task[1]=14;task[3]=scene_entry[0x51];memcpy(task+0x10,scene_entry+0x40,16);}
    return 1;
}
#define RE4_QA_TREASURE_COUNT(base) native_count(base)
#define RE4_QA_TREASURE_FIND(base,id) native_find(base,id)
#define RE4_QA_TREASURE_CLASS(base) ((void)base,native_class)
#define RE4_QA_TREASURE_COLLECT(base,actor) native_collect(base,actor)
#define RE4_QA_MERCHANT_OPEN(base) native_open(base)
#define RE4_QA_MERCHANT_COMPONENT(base,model) native_merchant_component(base,model)
#define RE4_QA_MERCHANT_CLASS(base) ((void)base,merchant_class)
#define RE4_QA_MERCHANT_OWNER(base,model) native_merchant_owner(base,model)
#define RE4_QA_SCENE_ROOMSAVE(base,room) native_room_save(base,room)
#define RE4_QA_SCENE_PTR(base,id) native_scene_ptr(base,id)
#define RE4_QA_SCENE_EXECUTE(base,id) native_scene_execute(base,id)
#define RE4_QA_SCENE_READY(base) ((void)base,scene_ready)
#define RE4_QA_DIAG_BINDINGS(base) ((void)base,diag_bindings)
#define RE4_QA_DIAG_MERCHANT_CLASS(base) ((void)base,diag_class0())
#define RE4_QA_DIAG_MERCHANT_INVENTORY(base,screen) diag_inventory(base,screen)
#define RE4_QA_DIAG_MERCHANT_STATE(base,screen) diag_state(base,screen)
#define RE4_QA_DIAG_CASE_PLANE(base,object) diag_plane(base,object)
#include "qa_treasure_touch.h"
static void setup(unsigned room){
    memset(image,0,0xa600000);memset(ita,0,sizeof ita);memset(aev,0,sizeof aev);memset(player,0,sizeof player);memset(pickup,0,sizeof pickup);memset(enemies,0,sizeof enemies);
    gt=valid=alive=layout=collectable=1;paused=finds=collections=opens=logs=item_count=count_calls=wrong_actor=0;type=1;qa_treasure_logs=0;
    diag_bindings=diag_current=diag_foreign=diag_hovered=diag_top_inventory=diag_bad_ray=0;diag_hover_calls=diag_ray_calls=diag_plane_calls=diag_inventory_calls=diag_enabled_calls=diag_state_calls=0;
    diag_dead=diag_bad_layout=mg_seen_hand=0;log_used=0;log_text[0]=0;memset(&diag,0,sizeof diag);
    merchant_present=merchant_alive=merchant_layout=merchant_owner=1;merchant_probes=0;memset(merchant_component,0,sizeof merchant_component);ptr(merchant_component+0xa0,merchant_actor);
    scene_ready=scene_schedule=1;scene_save_case=scene_entry_case=scenes=0;memset(scene_entry,0,sizeof scene_entry);memset(room_saves,0,sizeof room_saves);
    g_ue4_base_cached=(uintptr_t)image;cinema_type=type_get;cinema_active=active_get;rl_player_valid=valid_get;rl_paused=paused_get;
    unsigned char* game=image+0xa501bd8;word(game+0x24,3);word(game+0x5034,200);half(game+0x5038,(unsigned short)room);
    ptr(image+0xa3f5e48,game);ptr(image+0xa361da0,image+0xa500780);ptr(image+0xa500780+0x20,inventory);word(image+0xa500780+0x30,4);
    ptr(image+0xa4e9b40,player);player[0x3dd]=1;
    static const struct{uintptr_t at;unsigned a,b;} code[]={
        {0x62ec678,0xd10103ff,0xa90157f6},{0x6153664,0xf81d0ff5,0xa9014ff4},
        {0x615bce8,0xa9bf7bfd,0x910003fd},{0x61538d4,0xd10143ff,0xa9015ff8},
        {0x62e9b5c,0xd10283ff,0xa90757f6},{0x645ace8,0xd10183ff,0xf90023f3},
        {0x6153ec0,0xf0021f68,0xf9402d08},{0x62e9b7c,0x97f9a8d1,0xb4000a00},
        {0x5f33f14,0xf81c0ff9,0xa9015ff8},{0x617d214,0xd100c3ff,0xf9000bf3},
        {0x631493c,0xf81e0ff3,0xa9017bfd},{0x6485860,0xd10183ff,0xf90023f3},
        {0x69df1cc,0xb9400408,0x34000368},{0x83289ec,0xf9405008,0xb4000068},
        {0x614cdb0,0x53083c28,0x7100251f},{0x6155664,0xb0021f68,0xf9402d08},
        {0x615bc6c,0xf81e0ff3,0xa9017bfd},{0x616107c,0xf81b0ff9,0xa9015ff8},
        {0x61627bc,0xb0021f09,0x2a0003e8},{0x60093c0,0xd10143ff,0xf90013f5},
        {0x6003ed8,0xd10243ff,0xa90467fa},{0x61d4050,0xb0020c68,0xf946bd08},
        {0x61d4760,0xb0020c88,0xf9446508},{0x62dd310,0xb0020448,0xf9415508},{0x621a8f8,0xa9bf7bfd,0x910003fd},
        {0x6488adc,0xd10183ff,0xf90023f3},{0x63191f4,0xf9418c00,0xd65f03c0},
        {0x63168dc,0x394c5000,0xd65f03c0},
        {0x620dc24,0xd101c3ff,0xf9002bf3},{0x633e52c,0xf81d0ff5,0xa9014ff4},
        {0x63769bc,0xf81b0ff9,0xa9015ff8},{0x6371fdc,0xf81e0ff3,0xa9017bfd}};
    for(unsigned i=0;i<sizeof code/sizeof *code;i++){word(image+code[i].at,code[i].a);word(image+code[i].at+4,code[i].b);}
    ptr(image+0x9cee050,image+0x615bce8);
    ptr(image+0x9cee040,image+0x615bc6c);ptr(image+0xa361d50,image+0xa540ce8);ptr(image+0xa540ce8+0x30,room_saves);
    scene_entry[0x38]=3;scene_entry[0x39]=2;scene_entry[0x3a]=room==0x101?8:18;scene_entry[0x3e]=0x14;
    ptr(scene_entry+0x48,image+(room==0x101?0x6003ed8:0x60093c0));scene_entry[0x50]=2;scene_entry[0x51]=room==0x101?2:1;
    unsigned char* manager=image+0xa541b38;ptr(image+0xa542058,manager);ptr(manager+0x10,ita);ptr(manager+0x18,ita+16);ptr(manager,aev);ptr(manager+8,aev+16);
    memcpy(ita,"ITA",4);half(ita+4,0x105);half(ita+6,41);memcpy(aev,"AEV",4);half(aev+4,0x104);half(aev+6,29);
    unsigned char* entry=ita+16+9*0xd0;ptr(manager+0xd8,aev+16);ptr(aev+16,entry);ptr(entry,(void*)(uintptr_t)UINT32_MAX);
    entry[0x38]=3;entry[0x39]=3;entry[0x3a]=0x89;half(entry+0xa4,0x57);half(entry+0xaa,32);ptr(entry+0x90,(void*)0x4000);
    ptr(pickup+0x3f8,entry);half(pickup+0x4c0,0x57);pickup[0x4c2]=pickup[0x4c3]=1;
    ptr(image+0xa4e9b68+8,enemies);word(image+0xa4e9b68+0x10,2);word(image+0xa4e9b68+0x14,0x111c);
    word(enemies+8,1);enemies[0x118]=0x18;ptr(enemies,image+0x9ce01c0);
}
static void setup_diag(void){
    setup(0x104);type=8;diag_bindings=diag_current=diag_hovered=1;mg_seen_hand=diag.hand;
    diag.hand[0x33d]=1;diag.screen[0x270]=1;diag.item[0x240]=1;ptr(diag.item+0x248,diag.hand);
    ptr(diag.screen+0x2c0,diag.item);ptr(diag.screen+0x258,diag.cases[0]);ptr(diag.screen+0x260,diag.cases[1]);
    ptr(diag.screen+0x2f0,diag.cases[0]);ptr(diag.screen+0x2f8,diag.cases[1]);ptr(diag.screen+0x330,diag.hand);
    ptr(diag.button+0xa0,diag.actor);ptr(diag.button+0x308,diag.merchant);diag.button[0x320]=2;
    float button_position[3]={180,0,15100};memcpy(diag.button+0x1d0,button_position,sizeof button_position);
    for(unsigned i=0;i<2;i++){
        ptr(diag.cases[i]+0x130,diag.roots[i]);ptr(diag.roots[i]+0x20,diag.cases[i]);ptr(diag.roots[i]+0xa0,diag.cases[i]);
        float transform[12]={0,0,0,1,i?110.547f:180,0,i?15082:15167,0,1,1,1,0};
        memcpy(diag.roots[i]+0x1c0,transform,sizeof transform);
    }
    diag.cases[0][0x58]=0x20;
}
int main(void){
    image=calloc(1,0xa600000);assert(image);
    setup(0x101);unsigned char* original=malloc(0xa600000);assert(original);memcpy(original,image,0xa600000);
    unsigned char old_ita[sizeof ita],old_aev[sizeof aev];memcpy(old_ita,ita,sizeof ita);memcpy(old_aev,aev,sizeof aev);
    assert(qa_authored_spinel_pickup()==1 && collections==1 && item_count==1 && finds==1);
    assert(!memcmp(image,original,0xa600000) && !memcmp(ita,old_ita,sizeof ita) && !memcmp(aev,old_aev,sizeof aev));
    assert(!qa_authored_spinel_pickup() && collections==1); // No repeated fixture after it is owned.
    for(unsigned fail=0;fail<54;fail++){
        setup(0x101);unsigned char* entry=ita+16+9*0xd0;unsigned char* manager=image+0xa541b38;
        if(fail==0)gt=0;if(fail==1)type=8;if(fail==2)valid=0;if(fail==3)paused=1;if(fail==4)alive=0;
        if(fail==5)g_ue4_base_cached=0;if(fail==6)cinema_type=0;if(fail==7)cinema_active=0;if(fail==8)rl_player_valid=0;if(fail==9)rl_paused=0;
        if(fail==10)ptr(image+0xa3f5e48,player);if(fail==11)half(image+0xa501bd8+0x5038,0x100);if(fail==12)word(image+0xa501bd8+0x24,4);
        if(fail==13)word(image+0x5f33f14,0);if(fail==14)ptr(image+0xa361da0,0);if(fail==15)ptr(image+0xa500780+0x20,0);
        if(fail==16)word(image+0xa500780+0x30,4097);if(fail==17)ptr(image+0xa4e9b40,0);if(fail==18)player[0x3dd]=0;
        if(fail==19)word(image+0x62ec678,0);if(fail==20)word(image+0x615bce8,0);if(fail==21)word(image+0x61538d4,0);if(fail==22)word(image+0x62e9b5c,0);
        if(fail==23)ptr(image+0x9cee050,0);if(fail==24)ptr(image+0xa542058,0);if(fail==25)ptr(manager+0x10,0);if(fail==26)ptr(manager+0x18,ita+20);
        if(fail==27)ita[0]='X';if(fail==28)half(ita+4,0x104);if(fail==29)half(ita+6,40);
        if(fail==30)entry[0x38]=2;if(fail==31)entry[0x39]=2;if(fail==32)entry[0x3a]=0x88;if(fail==33)half(entry+0xa4,0x58);
        if(fail==34)half(entry+0xaa,31);if(fail==35)half(entry+0xa8,2);if(fail==36)ptr(entry+0x90,0);
        if(fail==37)word(image+0x6153ec0,0);if(fail==38)word(image+0x62e9b7c,0);if(fail==39)wrong_actor=1;
        if(fail==40)collectable=0;if(fail==41)layout=0;if(fail==42)pickup[0x4c2]=0;if(fail==43)pickup[0x4dc]=1;
        if(fail==44)word(image+0x645ace8,0);if(fail==45)ptr(pickup+0x3f8,entry+0xd0);if(fail==46)half(pickup+0x4c0,0x58);
        if(fail==47)pickup[0x4c3]=0;if(fail==48)item_count=1;if(fail==49)item_count=-1;
        if(fail==50)ptr(manager+0x10,(void*)(UINTPTR_MAX-3));if(fail==51)word(image+0xa500780+0x30,0);
        if(fail==52)ptr(image+0xa500780+0x20,(void*)(UINTPTR_MAX-1));
        if(fail==53)word(image+0x6153664,0);
        memcpy(original,image,0xa600000);memcpy(old_ita,ita,sizeof ita);memcpy(old_aev,aev,sizeof aev);
        assert(!qa_authored_spinel_pickup() && !collections && !opens);
        assert(!memcmp(image,original,0xa600000) && !memcmp(ita,old_ita,sizeof ita) && !memcmp(aev,old_aev,sizeof aev));
    }
    for(unsigned legacy=0;legacy<2;legacy++){
        setup(0x104);enemies[0x118]=legacy?0x18:0x10;enemies[0x119]=6;
        memcpy(original,image,0xa600000);assert(qa_original_merchant_entry() && opens==1 && merchant_probes==1 && !memcmp(image,original,0xa600000));
    }
    for(unsigned fail=0;fail<17;fail++){
        setup(0x104);
        if(fail==0)word(image+0x617d214,0);if(fail==1)ptr(image+0xa4e9b68+8,0);if(fail==2)word(image+0xa4e9b68+0x10,4097);
        if(fail==3)word(image+0xa4e9b68+0x14,0x1100);if(fail==4)word(enemies+8,0x201);if(fail==5)enemies[0x118]=0x10;
        if(fail==6)merchant_present=0;if(fail==7)half(image+0xa501bd8+0x5038,0x101);if(fail==8)paused=1;
        if(fail==9)merchant_alive=0;if(fail==10)merchant_layout=0;if(fail==11)merchant_owner=0;if(fail==12)ptr(merchant_component+0xa0,pawn);
        if(fail==13)word(image+0x631493c,0);if(fail==14)word(image+0x6485860,0);if(fail==15)word(image+0x69df1cc,0);if(fail==16)word(image+0x83289ec,0);
        if(fail==5){enemies[0x118]=0x10;enemies[0x119]=5;}
        memcpy(original,image,0xa600000);assert(!qa_original_merchant_entry() && !opens && !memcmp(image,original,0xa600000));
    }
    for(unsigned context=0;context<=9;context++){
        setup(0x104);type=context;item_count=1;memcpy(original,image,0xa600000);
        assert(qa_treasure_snapshot()==(context==1 || context==2 || context==8));
        assert(count_calls==(context==1 || context==2 || context==8) && !collections && !opens && !memcmp(image,original,0xa600000));
    }
    for(unsigned house=0;house<2;house++){
        unsigned room=house?0x101:0x104;int (*scene)(void)=house?qa_authored_house_entry:qa_authored_merchant_reveal;
        for(unsigned idle=0;idle<3;idle++){
            setup(room);ptr(image+0xa543150,idle==0?0:idle==1?image+0xa40a0f8:(void*)UINTPTR_MAX);
            memcpy(original,image,0xa600000);unsigned char old_save[sizeof room_saves],old_entry[sizeof scene_entry];memcpy(old_save,room_saves,sizeof room_saves);memcpy(old_entry,scene_entry,sizeof scene_entry);
            assert(scene() && scenes==1 && !collections && !opens);
            // Only the simulated original scheduler writes its own task, never the wrapper.
            memcpy(original+0xa5436e8+11*0x40,image+0xa5436e8+11*0x40,0x40);
            assert(!memcmp(image,original,0xa600000) && !memcmp(old_save,room_saves,sizeof room_saves) && !memcmp(old_entry,scene_entry,sizeof scene_entry));
            assert(!scene() && scenes==1);
        }
        static const uintptr_t preimages[]={0x614cdb0,0x6155664,0x6153ec0,0x615bc6c,0x616107c,0x61627bc,0x60093c0,0x6003ed8,0x61d4050,0x61d4760,0x62dd310,0x621a8f8};
        for(unsigned fail=0;fail<45;fail++){
            setup(room);
            if(fail<12)word(image+preimages[fail],0);
            if(fail==12)half(image+0xa501bd8+0x5038,0x100);if(fail==13)paused=1;if(fail==14)scene_ready=0;if(fail==15)image[0xa52d8d0]=1;
            if(fail==16)ptr(image+0xa361d50,0);if(fail==17)ptr(image+0xa540ce8+0x30,0);if(fail==18)ptr(image+0xa540ce8+0x30,(void*)UINTPTR_MAX);
            if(fail==19)scene_save_case=1;if(fail==20)scene_save_case=2;if(fail==21)scene_save_case=3;
            if(fail==22)room_saves[0xd8+(house?6:5)]=house?0x80:2;
            if(fail==23){if(house)room_saves[0xd8+7]=1;else scene_ready=0;}
            if(fail==24)scene_entry[0x38]=2;if(fail==25)scene_entry[0x39]=1;if(fail==26)scene_entry[0x3a]=9;if(fail==27)scene_entry[0x3e]=0;
            if(fail==28)ptr(scene_entry+0x48,0);if(fail==29)ptr(scene_entry+0x40,pawn);if(fail==30)scene_entry[0x50]=3;if(fail==31)scene_entry[0x51]=0;
            if(fail==32)ptr(image+0xa543150,image+0xa5436e8);
            if(fail==33)for(unsigned i=0;i<12;i++)image[0xa5436e8+i*0x40]=1;
            if(fail==34){unsigned char* task=image+0xa5436e8;task[0]=1;memcpy(task+0x18,scene_entry+0x48,8);}
            if(fail==35)ptr(image+0x9cee040,0);if(fail==36)ptr(image+0xa542058,0);if(fail==37)type=2;
            if(fail==38)scene_schedule=0;if(fail==39)ptr(image+0xa540ce8+0x30,room_saves+1);
            if(fail==40)scene_entry[0x51]=house?1:2;if(fail==41)word(image+0xa501bd8+0x24,4);
            if(fail>=42)scene_entry_case=(int)fail-41;
            memcpy(original,image,0xa600000);assert(!scene() && scenes==(fail==38) && !collections && !opens);
            assert(!memcmp(image,original,0xa600000));
        }
    }
    setup_diag();unsigned char* old_diag=malloc(sizeof diag);assert(old_diag);memcpy(old_diag,&diag,sizeof diag);memcpy(original,image,0xa600000);
    assert(qa_treasure_snapshot() && diag_hover_calls==1 && diag_ray_calls==1 && diag_plane_calls==2 && diag_inventory_calls==1 && diag_enabled_calls==1 && diag_state_calls==1);
    assert(strstr(log_text,"typed_owned_button=1 action=2 enabled=1") && strstr(log_text,"UIState=1")
        && strstr(log_text,"case=primary") && strstr(log_text,"case=secondary") && strstr(log_text,"interaction_plane=") && strstr(log_text,"ray_dot="));
    assert(!memcmp(old_diag,&diag,sizeof diag) && !memcmp(original,image,0xa600000));
    for(unsigned i=1;i<24;i++)assert(qa_treasure_snapshot());
    assert(qa_treasure_logs==24 && diag_hover_calls==24 && diag_ray_calls==24 && diag_plane_calls==48 && diag_state_calls==24);
    assert(qa_treasure_snapshot() && diag_hover_calls==24 && diag_plane_calls==48 && diag_state_calls==24); // Existing cap also caps all extra native reads.
    for(unsigned fail=0;fail<26;fail++){
        setup_diag();
        static const uintptr_t preimages[]={0x6488adc,0x63191f4,0x620dc24,0x633e52c,0x63769bc,0x6371fdc};
        if(fail<6)word(image+preimages[fail],0);
        if(fail==6)diag_bindings=0;if(fail==7)mg_seen_hand=0;if(fail==8)diag_dead=diag.hand;if(fail==9)diag_current=0;if(fail==10)diag_foreign=1;
        if(fail==11)diag_bad_ray=1;if(fail==12)diag_dead=diag.manager;if(fail==13)diag_bad_layout=diag.merchant;
        if(fail==14)diag_dead=diag.screen;if(fail==15)diag_bad_layout=diag.screen;
        if(fail==16)diag_dead=diag.cases[0];if(fail==17)diag_dead=diag.roots[0];if(fail==18)ptr(diag.roots[0]+0xa0,pawn);
        if(fail==19)diag_bad_layout=diag.roots[0];if(fail==20){float bad=NAN;memcpy(diag.roots[0]+0x1d0,&bad,4);}
        if(fail==21)diag_dead=diag.actor;if(fail==22)ptr(diag.button+0xa0,pawn);if(fail==23)diag_hovered=0;
        if(fail==24)diag_dead=diag.item;if(fail==25)diag_bad_layout=diag.item;
        memcpy(old_diag,&diag,sizeof diag);memcpy(original,image,0xa600000);assert(qa_treasure_snapshot());
        if(fail<=10)assert(!diag_hover_calls && !diag_ray_calls && !diag_plane_calls);
        else if(fail<=15)assert(diag_hover_calls==1 && diag_ray_calls==1 && !diag_plane_calls);
        else if(fail<=20)assert(diag_plane_calls==1);
        else {assert(diag_plane_calls==2);if(fail<=23)assert(!diag_enabled_calls);}
        assert(!memcmp(old_diag,&diag,sizeof diag) && !memcmp(original,image,0xa600000));
        if(fail<=13)assert(!diag_state_calls);
    }
    setup_diag();word(image+0x63168dc,0);assert(qa_treasure_snapshot() && !diag_state_calls && !diag_hover_calls && strstr(log_text,"rejected=preimage"));
    for(unsigned state=0;state<18;state++) {
        setup_diag();diag.merchant[0x314]=(unsigned char)state;memcpy(old_diag,&diag,sizeof diag);memcpy(original,image,0xa600000);
        assert(qa_treasure_snapshot() && diag_state_calls==1);
        char expected[48];snprintf(expected,sizeof expected,"state=%u read_only",state);assert(strstr(log_text,expected));
        assert(!memcmp(old_diag,&diag,sizeof diag) && !memcmp(original,image,0xa600000));
    }
    setup_diag();diag_top_inventory=1;assert(qa_treasure_snapshot() && !diag_inventory_calls && !diag_state_calls && diag_plane_calls==2);
    for(unsigned context=1;context<=2;context++){
        setup_diag();type=context;assert(qa_treasure_snapshot() && !diag_hover_calls && !diag_ray_calls && !diag_plane_calls && !diag_state_calls);
    }
    free(old_diag);
    free(original);free(image);
    puts("PASS: actual QA treasure C: pickup54/merchant17/scene45 guards, original scheduler kinds1/2, snapshot1/2/8; bounded type8 hover/ray/inventory/owned-case-plane diagnostics,26 refusal branches,18 merchant states plus getter preimage refusal,24 cap, byte-identical fixture/game data; no wrapper player/inventory/plot/money writes");
    return 0;
}
