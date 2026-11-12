#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RE4_QA_USER_TRANSITIONS
static uintptr_t g_ue4_base_cached;
static unsigned char *image,ita[16+27*0xd0],actor[0x4e0],native_class[8];
static int context,alive,typed,found,counts[3],collections,finds,count_calls,refuse;
static unsigned active_item,active_id;
static unsigned char receptacle[0x420],slots[2][0x4f8],key_menu[0x2a0],world_pawn[0x1608];
static unsigned char roots[3][0x1f0],world[8],wrong_world[8],scene_class[8],receptacle_class[8],slot_class[8],key_menu_class[8],pawn_class[8];
static unsigned char cached_inventory[0x100],cached_inventory_class[8],bio4_pawn_class[8],fixture_items[8*14];
static int fixture_get_calls,fixture_erases,fixture_refuse_at,fixture_prop_bad,fixture_recipe_bad;
static unsigned char room_groups[160],room_rows[2][5*24],room_saves[8*216];
static int fixture_room_index,fixture_room_bad,save_requests,save_refuse;
static const void *slot_data[2],*button_data[2],*dead_object,*untyped_object,*wrong_world_actor;
static unsigned char allowed_items[2],menu_items[2];
static int name_exists,find_receptacle;
static const unsigned char* qa_treasure_context(int mutation,unsigned room){
    assert(room==0x104);return context && (!mutation || context==1)?image+0xa501bd8:0;
}
static unsigned type_get(void){return (unsigned)context;}
static unsigned (*cinema_type)(void)=type_get;
static int pv_alive(const void* p){
    if(!p || p==dead_object)return 0;
    if(p==actor)return alive;
    return p==receptacle || p==slots[0] || p==slots[1] || p==key_menu || p==world_pawn || p==world || p==wrong_world || p==cached_inventory
        || p==roots[0] || p==roots[1] || p==roots[2];
}
static int menu_visual_layout(const void* p,const void* cls,int bytes){
    if(p==untyped_object)return 0;
    if(p==actor){assert(cls==native_class && bytes==0x4e0);return typed;}
    if(p==receptacle)return cls==receptacle_class && bytes==0x41c;
    if(p==slots[0] || p==slots[1])return cls==slot_class && bytes==0x4f8;
    if(p==key_menu)return cls==key_menu_class && bytes==0x2a0;
    if(p==world_pawn)return (cls==pawn_class && bytes==0x570) || (cls==bio4_pawn_class && bytes==0x1608);
    if(p==cached_inventory)return cls==cached_inventory_class && bytes==0xf8;
    return (p==roots[0] || p==roots[1] || p==roots[2]) && cls==scene_class && bytes==0x1f0;
}
static void log_message(const char* format,...){(void)format;}
#define LOGF(...) log_message(__VA_ARGS__)
static int native_count(uintptr_t base,unsigned item){assert(base==(uintptr_t)image && item>=0xa4 && item<=0xa6);count_calls++;return counts[item-0xa4];}
static void* native_find(uintptr_t base,unsigned id){assert(base==(uintptr_t)image && id==active_id);finds++;return found?actor:0;}
static void native_collect(uintptr_t base,void* object){assert(base==(uintptr_t)image && object==actor);collections++;if(!refuse)counts[active_item-0xa4]++;}
#define RE4_QA_KEY_COUNT(base,item) native_count(base,item)
#define RE4_QA_TREASURE_FIND(base,id) native_find(base,id)
#define RE4_QA_TREASURE_CLASS(base) ((void)base,native_class)
#define RE4_QA_TREASURE_COLLECT(base,object) native_collect(base,object)
static void find_name(uintptr_t base,uint64_t* name){assert(base==(uintptr_t)image);*name=name_exists?0x42:0;}
static void* native_receptacle(uintptr_t base,uint64_t name){assert(base==(uintptr_t)image && name==0x42);return find_receptacle?receptacle:0;}
static void* native_world(uintptr_t base,const void* object){assert(base==(uintptr_t)image);return object==wrong_world_actor?wrong_world:world;}
#define RE4_QA_KEY_FIND_NAME(base,name) find_name(base,name)
#define RE4_QA_KEY_RECEPTACLE(base,name) native_receptacle(base,name)
#define RE4_QA_KEY_RECEPTACLE_CLASS(base) ((void)base,receptacle_class)
#define RE4_QA_KEY_SLOT_CLASS(base) ((void)base,slot_class)
#define RE4_QA_KEY_MENU_CLASS(base) ((void)base,key_menu_class)
#define RE4_QA_KEY_SCENE_CLASS(base) ((void)base,scene_class)
#define RE4_QA_KEY_PAWN_CLASS(base) ((void)base,pawn_class)
#define RE4_QA_KEY_PAWN(base) ((void)base,world_pawn)
#define RE4_QA_KEY_WORLD(base,object) native_world(base,object)
static int native_fixture_get(uintptr_t base,unsigned item){
    assert(base==(uintptr_t)image && item>=0xa4 && item<=0xa5);
    if(fixture_get_calls++==fixture_refuse_at)return 0;
    for(unsigned i=0;i<8;i++)if(!(fixture_items[i*14+4]&1)){
        unsigned char* p=fixture_items+i*14;unsigned short id=(unsigned short)item,quantity=1;
        memcpy(p,&id,2);memcpy(p+2,&quantity,2);p[4]=1;p[5]=0;
        const void* pointer_value=p;memcpy(image+0xa500780+0x28,&pointer_value,8);counts[item-0xa4]++;return 1;
    }
    return 0;
}
static void native_fixture_erase(uintptr_t base,void* p){
    assert(base==(uintptr_t)image && (unsigned char*)p>=fixture_items && (unsigned char*)p<fixture_items+sizeof fixture_items);
    unsigned short item;memcpy(&item,p,2);assert(item==0xa4 || item==0xa5);
    ((unsigned char*)p)[4]=0;counts[item-0xa4]--;fixture_erases++;
}
static void native_fixture_prop(uintptr_t base,unsigned item,void* out){
    assert(base==(uintptr_t)image && item>=0xa4 && item<=0xa6);
    const unsigned char prop[8]={0,0,7,1,1,0,0,0};memcpy(out,prop,8);if(fixture_prop_bad)((unsigned char*)out)[2]=9;
}
static int native_fixture_recipe(uintptr_t base,unsigned first,unsigned second,unsigned short* out){
    assert(base==(uintptr_t)image && ((first==0xa4 && second==0xa5) || (first==0xa5 && second==0xa4)));
    *out=fixture_recipe_bad?0xa5:0xa6;return 1;
}
#define RE4_QA_KEY_FIXTURE_GET(base,item) native_fixture_get(base,item)
#define RE4_QA_KEY_FIXTURE_ERASE(base,item) native_fixture_erase(base,item)
#define RE4_QA_KEY_FIXTURE_PROP(base,item,out) native_fixture_prop(base,item,out)
#define RE4_QA_KEY_FIXTURE_RECIPE(base,first,second,out) native_fixture_recipe(base,first,second,out)
#define RE4_QA_KEY_FIXTURE_INVENTORY(base) ((void)base,cached_inventory)
#define RE4_QA_KEY_FIXTURE_INVENTORY_CLASS(base) ((void)base,cached_inventory_class)
#define RE4_QA_KEY_FIXTURE_PAWN_CLASS(base) ((void)base,bio4_pawn_class)
static const unsigned char* native_room_save(uintptr_t base,const void* manager){
    assert(base==(uintptr_t)image && manager==image+0xa540ce8);return room_saves+fixture_room_index*216+(fixture_room_bad?4:0);
}
static void native_save_open(uintptr_t base){
    assert(base==(uintptr_t)image);save_requests++;
    if(!save_refuse){uintptr_t work=0x4000;memcpy(image+0xa4ff390,&work,8);}
}
#define RE4_QA_KEY_ROOM_SAVE(base,manager) native_room_save(base,manager)
#define RE4_QA_KEY_SAVE_OPEN(base) native_save_open(base)
#define RE4_QA_KEY_STORY_SOURCE "qa_story_source.sav"
#define RE4_QA_KEY_STORY_COPY "qa_story_copy.sav"
#include "qa_key_item_use.h"
static void word(void* p,unsigned value){memcpy(p,&value,4);}
static void half(void* p,unsigned short value){memcpy(p,&value,2);}
static void pointer(void* p,const void* value){memcpy(p,&value,8);}
static void world_setup(void){
    memset(receptacle,0,sizeof receptacle);memset(slots,0,sizeof slots);memset(key_menu,0,sizeof key_menu);
    memset(world_pawn,0,sizeof world_pawn);memset(roots,0,sizeof roots);
    memset(cached_inventory,0,sizeof cached_inventory);cached_inventory[0xf0]=1;pointer(world_pawn+0x1600,cached_inventory);
    memset(fixture_items,0,sizeof fixture_items);fixture_get_calls=fixture_erases=fixture_prop_bad=fixture_recipe_bad=0;fixture_refuse_at=-1;
    const unsigned char* native_manager=image+0xa500780;
    pointer(image+0xa361da0,native_manager);pointer((void*)(native_manager+0x20),fixture_items);word((void*)(native_manager+0x30),8);
    ((unsigned char*)native_manager)[0x1b]=0;
    memset(room_groups,0,sizeof room_groups);memset(room_rows,0,sizeof room_rows);memset(room_saves,0,sizeof room_saves);
    pointer(image+0xa361d50,image+0xa540ce8);pointer(image+0xa540ce8+0x30,room_saves);pointer(image+0xa364288,room_groups);
    pointer(image+0xa361d30,image+0xa3f5e48);pointer(image+0xa3f5e48,image+0xa501bd8);word(image+0xa501bd8+0x5378,0);
    pointer(room_groups+16,room_rows[1]);half(room_groups+24,5);room_rows[1][4*24]=1;
    fixture_room_index=fixture_room_bad=save_requests=save_refuse=0;
    pointer(image+0xa4ff390,0);word(image+0xa501bd8+4,0);
    name_exists=find_receptacle=1;dead_object=untyped_object=wrong_world_actor=0;
    unsigned char* objects[3]={receptacle,world_pawn,key_menu};
    for(unsigned i=0;i<3;i++){
        pointer(objects[i]+0x130,roots[i]);pointer(roots[i]+0x20,objects[i]);pointer(roots[i]+0xa0,objects[i]);
    }
    uint64_t name=0x42;memcpy(receptacle+0x244,&name,8);
    slot_data[0]=slots[0];slot_data[1]=slots[1];pointer(receptacle+0x3c0,slot_data);word(receptacle+0x3c8,2);word(receptacle+0x3cc,2);
    pointer(receptacle+0x3d8,key_menu);pointer(key_menu+0x298,receptacle);
    for(unsigned i=0;i<2;i++){
        unsigned char* slot=slots[i];pointer(slot+0x20,receptacle);pointer(slot+0xa0,receptacle);pointer(slot+0x4f0,receptacle);
        allowed_items[i]=(unsigned char)(0xa4+i);pointer(slot+0x4b0,allowed_items+i);word(slot+0x4b8,1);word(slot+0x4bc,1);
        slot[0x4c0]=(unsigned char)(0xa4+i);slot[0x4c2]=1;menu_items[i]=(unsigned char)(0xa4+i);
    }
    pointer(key_menu+0x250,menu_items);word(key_menu+0x258,2);word(key_menu+0x25c,2);
    pointer(key_menu+0x268,button_data);word(key_menu+0x270,2);word(key_menu+0x274,2);
}
static unsigned char* setup(unsigned item){
    memset(ita,0,sizeof ita);memset(actor,0,sizeof actor);memset(counts,0,sizeof counts);
    context=alive=typed=found=1;collections=finds=count_calls=refuse=0;active_item=item;active_id=item==0xa4?0x8f:0x90;
    static const struct{uintptr_t at;unsigned a,b;} code[]={
        {0x62ec678,0xd10103ff,0xa90157f6},{0x62e9b5c,0xd10283ff,0xa90757f6},
        {0x62e9b7c,0x97f9a8d1,0xb4000a00},{0x645ace8,0xd10183ff,0xf90023f3},
        {0x6153ec0,0xf0021f68,0xf9402d08},
        {0x6150fc0,0x3940e90a,0x5219014a},{0x6151008,0x39400109,0x52190129},
        {0x62f27dc,0xd10283ff,0xa90757f6},{0x67c7644,0xf81d0ff5,0xa9014ff4},
        {0x645d984,0xd10183ff,0xf90023f3},{0x645f684,0xd10183ff,0xf90023f3},
        {0x6460494,0xd10183ff,0xf90023f3},{0x8702bc4,0xf81e0ff3,0xa9017bfd},
        {0x5f2886c,0xd10183ff,0xf9000bf9},{0x5f33a1c,0xd10183ff,0xa90167fa},
        {0x5f32148,0x12003c08,0x7104451f},{0x5f34198,0x90ff1e88,0xaa1f03e9},
        {0x62199c8,0xf81e0ff3,0xa9017bfd},{0x645023c,0xd10183ff,0xf90023f3},{0x63c8a5c,0xd10183ff,0xf90023f3},
        {0x614cdb0,0x53083c28,0x7100251f},{0x5f1fe34,0xd0022209,0xf9469929},
        {0x5f1fcf0,0xa9bd57f6,0xa9014ff4},{0x5f177c8,0xa9bd57f6,0xa9014ff4},
        {0x61e1f70,0xa9be4ff4,0xa9017bfd},{0x62f2678,0xf81a0ffb,0xa90167fa}};
    for(unsigned i=0;i<sizeof code/sizeof *code;i++){word(image+code[i].at,code[i].a);word(image+code[i].at+4,code[i].b);}
    unsigned char* manager=image+0xa541b38;pointer(image+0xa542058,manager);pointer(manager+0x10,ita);pointer(manager+0x18,ita+16);
    memcpy(ita,"ITA",4);half(ita+4,0x105);half(ita+6,27);
    unsigned char* entry=ita+16+(item==0xa4?15:16)*0xd0;
    entry[0x38]=3;entry[0x39]=3;entry[0x3a]=(unsigned char)active_id;half(entry+0xa4,(unsigned short)item);
    half(entry+0xaa,item==0xa4?16:17);pointer(entry+0x90,(void*)0x4000);
    pointer(actor+0x3f8,entry);half(actor+0x4c0,(unsigned short)item);actor[0x4c2]=actor[0x4c3]=1;
    world_setup();
    return entry;
}
int main(void){
    image=calloc(1,0xa600000);assert(image);g_ue4_base_cached=(uintptr_t)image;
    for(unsigned item=0xa4;item<=0xa5;item++){
        setup(item);assert(qa_authored_emblem_pickup(item));assert(collections==1 && counts[item-0xa4]==1 && counts[2]==0);
        assert(!qa_authored_emblem_pickup(item));assert(collections==1);
        for(unsigned bad=0;bad<25;bad++){
            unsigned char* e=setup(item);
            switch(bad){
                case 0:context=0;break;case 1:context=2;break;case 2:word(image+0x62ec678,0);break;
                case 3:pointer(image+0xa542058,0);break;case 4:pointer(image+0xa541b38+0x10,0);break;
                case 5:pointer(image+0xa541b38+0x18,ita+17);break;case 6:ita[0]='X';break;
                case 7:half(ita+4,0x104);break;case 8:half(ita+6,28);break;case 9:e[0x38]=0;break;
                case 10:e[0x39]=1;break;case 11:e[0x3a]++;break;case 12:half(e+0xa4,0xa6);break;
                case 13:half(e+0xaa,18);break;case 14:half(e+0xa8,2);break;case 15:pointer(e+0x90,0);break;
                case 16:found=0;break;case 17:alive=0;break;case 18:typed=0;break;
                case 19:pointer(actor+0x3f8,ita+16);break;case 20:half(actor+0x4c0,0xa6);break;
                case 21:actor[0x4c2]=0;break;case 22:actor[0x4c3]=0;break;case 23:actor[0x4dc]=1;break;
                case 24:e[0x3a]^=0x80;break;
            }
            assert(!qa_authored_emblem_pickup(item));assert(collections==0 && counts[0]==0 && counts[1]==0 && counts[2]==0);
        }
        setup(item);counts[item-0xa4]=-1;assert(!qa_authored_emblem_pickup(item) && collections==0);
        setup(item);refuse=1;assert(!qa_authored_emblem_pickup(item) && collections==1 && counts[2]==0);
    }
    setup(0xa4);assert(!qa_authored_emblem_pickup(0xa6) && collections==0 && finds==0 && count_calls==0);
    setup(0xa4);context=2;counts[0]=counts[1]=1;assert(qa_key_item_snapshot() && collections==0 && count_calls==3);
    setup(0xa4);counts[2]=-1;assert(!qa_key_item_snapshot() && collections==0);
    setup(0xa4);context=0;assert(!qa_key_item_snapshot() && count_calls==0);
    setup(0xa4);unsigned char before_receptacle[sizeof receptacle],before_slots[sizeof slots],before_menu[sizeof key_menu];
    memcpy(before_receptacle,receptacle,sizeof receptacle);memcpy(before_slots,slots,sizeof slots);memcpy(before_menu,key_menu,sizeof key_menu);
    assert(qa_key_receptacle_snapshot(g_ue4_base_cached));
    assert(!memcmp(before_receptacle,receptacle,sizeof receptacle) && !memcmp(before_slots,slots,sizeof slots) && !memcmp(before_menu,key_menu,sizeof key_menu));
    for(unsigned bad=0;bad<27;bad++){
        setup(0xa4);
        switch(bad){
            case 0:context=2;break;case 1:word(image+0x8702bc4,0);break;case 2:dead_object=world_pawn;break;
            case 3:untyped_object=world_pawn;break;case 4:dead_object=world;break;case 5:name_exists=0;break;
            case 6:find_receptacle=0;break;case 7:dead_object=receptacle;break;case 8:untyped_object=receptacle;break;
            case 9:wrong_world_actor=receptacle;break;case 10:receptacle[0x244]=0;break;case 11:pointer(roots[0]+0x20,0);break;
            case 12:word(roots[1]+0x1d0,0x7f800000);break;case 13:word(receptacle+0x3c8,3);break;
            case 14:word(receptacle+0x3cc,1);break;case 15:dead_object=slots[0];break;case 16:untyped_object=slots[0];break;
            case 17:pointer(slots[0]+0x4f0,0);break;case 18:word(slots[0]+0x4b8,17);break;
            case 19:slots[0][0x4c0]=0xa6;break;case 20:slots[1][0x4c0]=0xa4;break;
            case 21:word(slots[0]+0x1d0,0x7fc00000);break;case 22:dead_object=key_menu;break;
            case 23:wrong_world_actor=key_menu;break;case 24:pointer(key_menu+0x298,0);break;
            case 25:pointer(roots[2]+0xa0,0);break;case 26:word(key_menu+0x258,17);break;
        }
        assert(!qa_key_receptacle_snapshot(g_ue4_base_cached) && collections==0 && count_calls==0);
    }
    setup(0xa4);pointer(receptacle+0x3d8,0);assert(qa_key_receptacle_snapshot(g_ue4_base_cached));
    puts("PASS: read-only same-world receptacle/slot/menu snapshots, ownership and bounds; 27 independently invalid world/layout/name/array cases refuse");
    setup(0xa4);assert(qa_key_inventory_fixture() && fixture_get_calls==2 && fixture_erases==0 && counts[0]==1 && counts[1]==1 && counts[2]==0 && collections==0);
    assert(!qa_key_inventory_fixture() && fixture_get_calls==2);
    for(unsigned bad=0;bad<17;bad++){
        setup(0xa4);
        switch(bad){
            case 0:context=2;break;case 1:word(image+0x5f2886c,0);break;case 2:dead_object=cached_inventory;break;
            case 3:untyped_object=cached_inventory;break;case 4:pointer(world_pawn+0x1600,0);break;
            case 5:cached_inventory[0xf0]=0;break;case 6:counts[2]=1;break;case 7:fixture_prop_bad=1;break;
            case 8:fixture_recipe_bad=1;break;case 9:word(image+0xa500780+0x30,1);break;
            case 10:image[0xa500780+0x1b]=1;break;case 11:for(unsigned i=0;i<8;i++)fixture_items[i*14+4]=1;break;
            case 12:find_receptacle=0;break;
            case 13:word(image+0xa501bd8+0x5378,0x00400000);break;
            case 14:pointer(image+0xa361d30,0);break;
            case 15:pointer(image+0xa3f5e48,image+0xa501bd8+8);break;
            case 16:word(image+0x61e1f70,0);break;
        }
        assert(!qa_key_inventory_fixture() && fixture_get_calls==0 && fixture_erases==0 && collections==0);
    }
    for(int bad=0;bad<2;bad++){
        setup(0xa4);fixture_refuse_at=bad;
        assert(!qa_key_inventory_fixture() && fixture_get_calls==bad+1 && fixture_erases==bad && !counts[0] && !counts[1] && !counts[2]);
    }
    puts("PASS: explicit QA ownership fixture, fresh-only/typed/recipe/capacity gates, first/second native-get refusal and native rollback");
    setup(0xa4);int flag9=-1,key9=-1;assert(qa_key_room_save_flag(g_ue4_base_cached,&flag9) && flag9==0);
    assert(qa_key_global_key_flag(g_ue4_base_cached,&key9) && key9==0);
    word(image+0xa501bd8+0x5378,0x00400002);assert(qa_key_global_key_flag(g_ue4_base_cached,&key9) && key9==1);
    assert(qa_key_room_save_flag(g_ue4_base_cached,&flag9) && flag9==0); // Distinct namespaces after actual native gate use.
    assert(!qa_key_inventory_fixture() && fixture_get_calls==0 && !counts[0] && !counts[1]);
    word(image+0xa501bd8+0x5378,2);word(room_saves+4,0x00400000);
    assert(qa_key_room_save_flag(g_ue4_base_cached,&flag9) && flag9==1);
    assert(qa_key_global_key_flag(g_ue4_base_cached,&key9) && key9==0);
    assert(qa_key_inventory_fixture()); // Unrelated RoomScenario9 does not impersonate global Key9.
    setup(0xa4);pointer(room_groups,room_rows[0]);half(room_groups+8,1);room_rows[0][0]=1;room_rows[1][0]=1;fixture_room_index=2;
    word(room_saves+2*216+4,0x00400000);assert(qa_key_room_save_flag(g_ue4_base_cached,&flag9) && flag9==1);
    setup(0xa4);assert(qa_key_save_fixture() && save_requests==1 && counts[0]==0 && counts[1]==0 && counts[2]==0);
    assert(!qa_key_save_fixture() && save_requests==1);
    setup(0xa4);counts[2]=1;assert(qa_key_save_fixture() && save_requests==1 && counts[2]==1);
    setup(0xa4);counts[0]=1;word(room_saves+4,0x00400000);assert(qa_key_save_fixture() && save_requests==1 && counts[0]==1);
    setup(0xa4);word(image+0xa501bd8+0x5378,0x00400002);assert(qa_key_save_fixture() && save_requests==1); // Saving real unlocked state is allowed.
    for(unsigned bad=0;bad<12;bad++){
        setup(0xa4);
        switch(bad){
            case 0:context=2;break;case 1:word(image+0x5f1fe34,0);break;case 2:word(image+0xa501bd8+4,0x20);break;
            case 3:pointer(image+0xa4ff390,(void*)0x4000);break;case 4:pointer(world_pawn+0x1600,0);break;
            case 5:counts[0]=2;break;case 6:counts[2]=-1;break;case 7:half(room_groups+24,256);break;
            case 8:pointer(room_groups+16,0);break;case 9:room_rows[1][4*24]=0;break;
            case 10:fixture_room_bad=1;break;case 11:pointer(image+0xa540ce8+0x30,0);break;
        }
        assert(!qa_key_save_fixture() && save_requests==0 && fixture_get_calls==0 && collections==0);
    }
    setup(0xa4);save_refuse=1;assert(!qa_key_save_fixture() && save_requests==1);
    puts("PASS: separate global Key9 / RoomScenario9, dynamic RoomSave index, fresh fixture refuses global completion, native saves allow full/partial/unlocked configurations");
    setup(0xa4);assert(!qa_key_story_snapshot());
    unsigned char content[8193],readback[8193];for(unsigned i=0;i<sizeof content;i++)content[i]=(unsigned char)i;
    FILE* f=fopen(RE4_QA_KEY_STORY_SOURCE,"wb");assert(f);assert(fwrite(content,1,sizeof content,f)==sizeof content);assert(!fclose(f));
    context=0;assert(!qa_key_story_snapshot());context=1;assert(qa_key_story_snapshot());
    f=fopen(RE4_QA_KEY_STORY_COPY,"rb");assert(f);assert(fread(readback,1,sizeof readback,f)==sizeof readback);assert(fgetc(f)==EOF && !ferror(f));assert(!fclose(f));assert(!memcmp(content,readback,sizeof content));
    assert(!qa_key_story_snapshot());assert(!remove(RE4_QA_KEY_STORY_COPY));
    f=fopen(RE4_QA_KEY_STORY_SOURCE,"wb");assert(f);assert(!fclose(f));assert(!qa_key_story_snapshot());assert(!remove(RE4_QA_KEY_STORY_SOURCE));
    free(image);puts("PASS: both real halves, duplicate/full-item refusal, 50 independent input/identity guards including disk/runtime id distinction, native refusal and read-only snapshot");
    return 0;
}
