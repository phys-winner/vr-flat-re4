#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned char wrist[0x1000],before[sizeof wrist];
static int game_thread=1;
static int cinema_on_game_thread(void){return game_thread;}
static void* cls(void){return (void*)1;}
static void* (*mv_scene_class)(void)=cls,*(*mv_static_class)(void)=cls,*(*cu_component_class)(void)=cls;
static int pv_alive(const void* value){return value!=0;}
static int menu_visual_layout(const void* value,const void* type,int size){(void)value;(void)type;(void)size;assert(0);return 0;}
static int pv_component(const void* value,const void* owner,int size){(void)value;(void)owner;(void)size;assert(0);return 0;}
static int cinema_ui_named(const void* value,const char* name,int size){(void)value;(void)name;(void)size;assert(0);return 0;}
static void* fv_object_property(const void* value,const char* name){(void)value;(void)name;assert(0);return 0;}
static int fv_descends_from(void* value,const void* root){(void)value;(void)root;assert(0);return 0;}
static void* touch_equipped_gun(void* hand){(void)hand;return 0;}
static void set_visible(void* value,int visible,int propagate){(void)value;(void)visible;(void)propagate;assert(0);}
static void (*mv_visible)(void*,int,int)=set_visible;
#include "../re4fix_weapon_ui.h"
int main(void){
 for(int visibility=0;visibility<=4;visibility++){
  memset(wrist,visibility,sizeof wrist);memcpy(before,wrist,sizeof wrist);
  for(int frame=0;frame<120;frame++){game_thread=frame%2;touch_weapon_ui_tick(wrist);}
  assert(!memcmp(wrist,before,sizeof wrist));
 }
 puts("PASS: native wrist visibility, opacity and contents remain byte-identical across ticks; weapon UI never resolves or hides the watch");
}
