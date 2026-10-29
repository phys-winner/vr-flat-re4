#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
static struct object {unsigned char raw[0x600];int kind,alive;} objects[8];
#define fixture_gun objects[0].raw
#define gunroot objects[1].raw
#define counter objects[2].raw
#define cube objects[3].raw
#define fixture_widget objects[4].raw
#define sibling objects[5].raw
#define foreign objects[6].raw
#define indicator objects[7].raw
static void* children[3],*counter_children[1];
static int visible_calls,game_thread=1;
static struct object* object(const void* value){for(unsigned i=0;i<8;i++)if(objects[i].raw==value)return &objects[i];return 0;}
static int pv_alive(const void* value){struct object* o=object(value);return o&&o->alive;}
static int cinema_on_game_thread(void){return game_thread;}
static void* scene(void){return (void*)1;}static void* mesh(void){return (void*)2;}static void* widget_cls(void){return (void*)3;}
static void* (*mv_scene_class)(void)=scene,*(*mv_static_class)(void)=mesh,*(*cu_component_class)(void)=widget_cls;
static int menu_visual_layout(const void* value,const void* type,int minimum){(void)minimum;struct object* o=object(value);return o&&o->alive&&((type==(void*)1&&((o->kind>=1&&o->kind<=3)||o->kind==5))||(uintptr_t)type==(unsigned)o->kind);}
static int pv_component(const void* value,const void* owner,int minimum){return pv_alive(value)&&menu_visual_layout(value,scene(),minimum)&&*(void* const*)((const char*)value+0x20)==owner&&*(void* const*)((const char*)value+0xa0)==owner;}
static int cinema_ui_named(const void* value,const char* expected,int minimum){(void)minimum;if(!strcmp(expected,"VR4GamePlayerGripIndicator"))return value==indicator&&objects[7].kind==5;assert(!strcmp(expected,"UI_AmmoCounter_Pistol_C"));return value==counter&&objects[2].kind==4;}
static void* fv_object_property(const void* value,const char* property){assert(value==counter);return !strcmp(property,"Widget")?fixture_widget:!strcmp(property,"Cube")?cube:0;}
static int fv_descends_from(void* value,const void* root){for(int i=0;i<24&&pv_alive(value);i++){if(value==root)return 1;value=*(void**)((char*)value+0xc0);}return 0;}
static void* touch_equipped_gun(void* hand){return hand==foreign?fixture_gun:0;}
static void set_visible(void* value,int shown,int propagation){assert((value==cube||value==fixture_widget||value==indicator)&&!shown&&!propagation);visible_calls++;*(unsigned short*)((char*)value+0x14c)&=~0x20u;}
static void (*mv_visible)(void*,int,int)=set_visible;
static void* qs_player(void* hand){(void)hand;return 0;}
static void* get_widget(const void* component){(void)component;return 0;}
static void* (*cu_get_widget)(const void*)=get_widget,*(*cu_find_widget)(const void*,const void*);
static void* (*cu_user_class)(void),*(*cu_tree_class)(void);
static int cinema_object(const void* value,const void* cls,int minimum){return menu_visual_layout(value,cls,minimum);}
static void (*wt_set_visibility)(void*,unsigned char);
static unsigned long g_ue4_base_cached;
#include "../re4fix_weapon_ui.h"
static void ready(void){memset(objects,0,sizeof objects);visible_calls=0;game_thread=1;for(int i=0;i<8;i++)objects[i].alive=1;
 objects[1].kind=1;objects[2].kind=4;objects[3].kind=2;objects[4].kind=3;objects[5].kind=2;
 *(void**)(fixture_gun+0x130)=gunroot;*(void**)(gunroot+0x20)=*(void**)(gunroot+0xa0)=fixture_gun;
 *(void**)(cube+0x20)=*(void**)(cube+0xa0)=*(void**)(fixture_widget+0x20)=*(void**)(fixture_widget+0xa0)=counter;
 *(void**)(cube+0xc0)=gunroot;*(void**)(fixture_widget+0xc0)=cube;
 *(void***)(gunroot+0xd0)=children;*(int*)(gunroot+0xd8)=*(int*)(gunroot+0xdc)=3;children[0]=cube;children[1]=sibling;children[2]=indicator;
 *(void***)(cube+0xd0)=counter_children;*(int*)(cube+0xd8)=*(int*)(cube+0xdc)=1;counter_children[0]=fixture_widget;
 *(void**)(sibling+0xc0)=gunroot;*(void**)(sibling+0x20)=*(void**)(sibling+0xa0)=fixture_gun;
 objects[7].kind=5;*(void**)(indicator+0xc0)=gunroot;*(void**)(indicator+0x20)=*(void**)(indicator+0xa0)=fixture_gun;
 *(unsigned short*)(indicator+0x14c)=0x20;
 *(unsigned short*)(cube+0x14c)=*(unsigned short*)(fixture_widget+0x14c)=*(unsigned short*)(sibling+0x14c)=0x20;
}
int main(void){ready();touch_weapon_ui_tick(foreign);assert(visible_calls==3&&*(unsigned short*)(sibling+0x14c)==0x20);touch_weapon_ui_tick(foreign);assert(visible_calls==3);
 ready();objects[2].kind=5;touch_weapon_ui_tick(foreign);assert(visible_calls==1);
 ready();*(void**)(cube+0xc0)=sibling;touch_weapon_ui_tick(foreign);assert(visible_calls==1);
 ready();*(void**)(fixture_widget+0x20)=fixture_gun;touch_weapon_ui_tick(foreign);assert(visible_calls==2&&*(unsigned short*)(fixture_widget+0x14c)==0x20);
 ready();objects[4].alive=0;touch_weapon_ui_tick(foreign);assert(visible_calls==2);
 ready();game_thread=0;touch_weapon_ui_tick(foreign);assert(!visible_calls);
 ready();touch_weapon_ui_tick(gunroot);assert(!visible_calls);
 ready();children[1]=gunroot;touch_weapon_ui_tick(foreign);assert(visible_calls==3);
 ready();*(void**)(indicator+0xa0)=foreign;touch_weapon_ui_tick(foreign);assert(visible_calls==2&&*(unsigned short*)(indicator+0x14c)==0x20);
 ready();objects[7].kind=3;touch_weapon_ui_tick(foreign);assert(visible_calls==2&&*(unsigned short*)(indicator+0x14c)==0x20);
 ready();*(void**)(indicator+0xc0)=foreign;touch_weapon_ui_tick(foreign);assert(visible_calls==2&&*(unsigned short*)(indicator+0x14c)==0x20);
 puts("PASS: owned held-gun VR grip dots and pistol ammo leaves hidden without propagation; other meshes, unrelated widgets, foreign owner/parent, dead component, cycle and thread guards");return 0;}
