#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

static char prefs_file[2048];
static int io_fail, settings_published, original_gets, original_applies, original_resets, refreshes, native_sets;
static long fixture_now;
static int game_thread=1;
static int cinema_on_game_thread(void){return game_thread;}
struct cinema_ui_weak{int index,serial;};
static void* weak_component;
static int weak_serial=1;
static void weak_assign(struct cinema_ui_weak* weak,const void* object){
    weak_component=(void*)object;*weak=(struct cinema_ui_weak){object?1:-1,weak_serial};
}
static void* weak_get(const struct cinema_ui_weak* weak){return weak->index==1&&weak->serial==weak_serial?weak_component:0;}
static void (*cu_weak_assign)(struct cinema_ui_weak*,const void*)=weak_assign;
static void* (*cu_weak_get)(const struct cinema_ui_weak*)=weak_get;
static int fixture_open(const char* path, int flags, ...) {
    char out[2056]; snprintf(out,sizeof out,"%s%s",prefs_file,strstr(path,".tmp")?".tmp":"");
    return _open(out,flags|_O_BINARY,_S_IREAD|_S_IWRITE);
}
static int fixture_write(int fd,const void* bytes,unsigned count){return io_fail?(int)count-1:_write(fd,bytes,count);}
static int fixture_rename(const char* from,const char* to){(void)from;(void)to;remove(prefs_file);char temp[2056];snprintf(temp,sizeof temp,"%s.tmp",prefs_file);return rename(temp,prefs_file);}
static int fixture_unlink(const char* path){(void)path;char temp[2056];snprintf(temp,sizeof temp,"%s.tmp",prefs_file);return _unlink(temp);}
#define O_CLOEXEC 0
#define open fixture_open
#define read _read
#define write fixture_write
#define fsync _commit
#define close _close
#define rename fixture_rename
#define unlink fixture_unlink
#define LOGF(...) ((void)0)
static int touch_mobile_settings_apply(int a,int b,int c,int d,int e){(void)a;(void)b;(void)c;(void)d;(void)e;settings_published++;return 1;}
static const char* fixture_language="en";
static const char* mobile_android_language(void){return fixture_language;}
static volatile int g_touch_density_milli=2750;
static int confirm_requests,confirm_kind,confirm_reply,unlock_calls;
static int mobile_android_confirm_request(int kind,int ru,long now){assert(kind>=1&&kind<=6&&!ru&&now>=0);confirm_requests++;confirm_kind=kind;return 1;}
static int mobile_android_confirm_take(int kind,long now){(void)now;if(kind!=confirm_kind)return 0;int reply=confirm_reply;confirm_reply=0;return reply;}
static int touch_mobile_extras_request(int kind,long now){assert(kind>=1&&kind<=5&&now>=0);unlock_calls++;return 1;}
static int gate_opt_get(void* self,const void* desc){(void)self;(void)desc;original_gets++;return 73;}
static void gate_opt_apply(void* self,const void* desc,int value){(void)self;(void)desc;(void)value;original_applies++;}
static int gate_opt_get_slider(void* self,const void* desc){return gate_opt_get(self,desc);}
static void gate_opt_apply_slider(void* self,const void* desc,int value){gate_opt_apply(self,desc,value);}
static float slider_value;
static int slider_int(const void* desc,float value){assert(desc);slider_value=value;return 37;}
static float slider_float(const void* desc,int value){assert(desc&&value==37);return slider_value;}
static int (*mm_slider_int)(const void*,float)=slider_int;
static float (*mm_slider_float)(const void*,int)=slider_float;
static void (*native_reset_replay)(void*);
static void gate_opt_reset(void* self,uint64_t menu){(void)menu;original_resets++;if(native_reset_replay)native_reset_replay(self);}
static int file_requests, language_requests, menu_changes, native_add_calls;
static uint64_t last_menu;
static void gate_menu_set_current(void* self,uint64_t menu){assert(self);last_menu=menu;menu_changes++;}
static long ptr_now_ms(void){return fixture_now;}
static void* dead_object;
static int pv_alive(void* value){return value&&value!=dead_object;}
static const unsigned char* (*qs_settings)(void);
static void (*qs_copy)(void*,const void*);
static void (*qs_set)(const void*);
static void (*qs_destroy)(void*);
static void files(void){file_requests++;}
static void language(void){language_requests++;}
static int refresh_readback,displayed_extras[5];
static void fixture_refresh_display(void*);
static void refresh(void* component,uint64_t menu){assert(component&&menu);refreshes++;if(refresh_readback)fixture_refresh_display(component);}
static void (*mm_show_files)(void)=files;
static void (*mm_show_language)(void)=language;
static void (*mm_refresh_menu)(void*,uint64_t)=refresh;
static unsigned mm_logs;
static int mm_ready=1,phone_inspect_ready;
static void mm_names(void){}
static void obj_name(const void* object,char* out,int size){snprintf(out,size,"%s",object==(void*)1?"OptionsMenu_DT":"Other_DT");}
static void phone_inspect_fname(const void* name,char* out,int size){(void)name;(void)size;*out=0;}
static void* options_component;
static int mm_owner_is_options(void* component){return component==options_component;}
static void* current_menu;
static unsigned char menu_name_property[0x58];
static int name_property_valid=1;
static int name_property_lowercase=1;
static void* fv_object_property(const void* object,const char* name){assert(object&&!strcmp(name,"CurrentMenu"));return current_menu;}
static const char* fv_scalar_property(const void* object,const char* name,const char* kind,int size){
    assert((!strcmp(name,"MenuName")||!strcmp(name,"menuName"))&&!strcmp(kind,"NameProperty")&&size==8);
    return name_property_valid&&!strcmp(name,name_property_lowercase?"menuName":"MenuName")&&object==current_menu&&pv_alive((void*)object)?(const char*)menu_name_property:0;
}
struct touch_mobile_extras{unsigned available,unlocked;int menu_ready,last_action,status;unsigned pending,failed;};
static int extras_ready=1;
static unsigned extras_unlocked;
static int extras_last_action,extras_status;
static unsigned extras_pending,extras_failed;
static struct touch_mobile_extras touch_mobile_extras_snapshot(void){return (struct touch_mobile_extras){31,extras_unlocked,extras_ready,extras_last_action,extras_status,extras_pending,extras_failed};}
static void gate_menu_add_entries(void*,void*);
static void gate_menu_add_entry(void*,uint64_t,const void*);
static void capture_placement(const char*,int,uint64_t,uint64_t);
struct row_capture {int widget,count;uint64_t owning,link,prop;float lo,hi,snap;const char* label;const char* values[7];const char* help;};
static struct row_capture rows[128];static int row_count;
static void mm_row(void* component,const char* row,int widget,uint64_t owning,uint64_t link,uint64_t prop,
    float lo,float hi,float snap,uint64_t get,uint64_t apply,uint64_t display,const char* label,const char* help,
    const char* const* toggles,int count) {
    (void)row;(void)get;(void)apply;(void)display;
    assert(component&&label&&help&&row_count<128);
    rows[row_count]=(struct row_capture){widget,count,owning,link,prop,lo,hi,snap,label};
    rows[row_count].help=help;
    for(int i=0;i<count;i++){assert(i<7);rows[row_count].values[i]=toggles[i];}
    ++row_count;
    capture_placement(row,widget,owning,prop);
    for(int i=0;i<count;i++)assert(toggles[i]&&*toggles[i]);
}

#include "graphics_fixture.h"

static void fixture_refresh_display(void* component){
    assert(component==options_component);
    void* owner=*(void**)((char*)component+0xa0);
    for(int i=0;i<5;i++){
        unsigned char desc[256]={0};mm_put(desc+0x48,mn.p_x[i]);
        displayed_extras[i]=hook_opt_get(owner,desc);
        if(refresh_readback)hook_opt_apply(owner,desc,displayed_extras[i]);
    }
}

// Native AddEntry emits per-menu Num-1. The cooked Blueprint resets one shared ValidIndex at index zero.
static struct {uint64_t id;int count;} native_menus[8];
static struct {uint64_t menu,prop;int slot;} native_rows[128];
static int native_menu_count,native_row_count,valid_index,placement_active;
static void native_add(uint64_t row,int widget,uint64_t menu,uint64_t prop){
    if(!placement_active)return;
    if(!widget){assert(native_menu_count<8);native_menus[native_menu_count++]=(typeof(*native_menus)){row,0};return;}
    int index=-1;for(int i=0;i<native_menu_count;i++)if(native_menus[i].id==menu){index=native_menus[i].count++;break;}
    assert(index>=0&&native_row_count<128);if(!index)valid_index=0;
    native_rows[native_row_count++]=(typeof(*native_rows)){menu,prop,valid_index++};
}
static void capture_placement(const char* row,int widget,uint64_t owning,uint64_t prop){
    uint64_t id=!strcmp(row,"MobileGraphics")?mn.graphics:!strcmp(row,"MobileExtras")?mn.extras:500;
    native_add(id,widget,owning,prop);
}
static void gate_menu_add_entry(void* component,uint64_t row,const void* value){
    ++native_add_calls;
    (void)component;const unsigned char* desc=value;
    native_add(row,desc[0x28],mm_u64(desc+0x20),mm_u64(desc+0x48));
}
static void authored_row(void* component,uint64_t row,int widget,uint64_t owning,uint64_t prop){
    unsigned char desc[256]={0};desc[0x28]=(unsigned char)widget;mm_put(desc+0x20,owning);mm_put(desc+0x48,prop);
    hook_menu_add_entry(component,row,desc);
}
static void gate_menu_add_entries(void* component,void* table){
    (void)table;authored_row(component,mn.audio,0,0,0);authored_row(component,mn.comfort,0,0,0);
    for(int i=0;i<6;i++)authored_row(component,700+i,3,mn.audio,800+i);
    for(int i=0;i<3;i++)authored_row(component,900+i,3,mn.comfort,1000+i);
}
static void menu_placement_case(void* component,void* table,int owner,int extras){
    options_component=owner?component:0;extras_ready=extras;native_menu_count=native_row_count=valid_index=row_count=0;placement_active=1;
    hook_menu_add_entries(component,table);placement_active=0;
    int audio_count=0,graphics_count=0,fov_count=0;
    for(int i=0;i<native_row_count;i++){
        if(native_rows[i].menu==mn.audio){
            assert(native_rows[i].slot==audio_count);
            audio_count++;
        }
        if(native_rows[i].menu==mn.graphics){assert(native_rows[i].slot==graphics_count++);}
        if(native_rows[i].prop==mn.p_fov){assert(native_rows[i].menu==mn.graphics);fov_count++;}
    }
    assert(audio_count==6+(owner&&table==(void*)1));
    assert(graphics_count==((owner&&table==(void*)1)?MG_COUNT+2:0));
    assert(fov_count==(owner&&table==(void*)1));
    if(owner&&table==(void*)1)assert(native_rows[0].prop==mn.p_graphics_open);
}

struct fake_var {uintptr_t vtable;unsigned char pad[16];unsigned flags;float value;};
static struct fake_var variables[MG_COUNT];
static int refuse_write;
static void* find_var(void* manager,const unsigned short* wide,int tracking){
    assert(manager==(void*)1&&!tracking);char name[64];int i=0;
    for(;wide[i]&&i<63;i++)name[i]=(char)wide[i];name[i]=0;
    for(i=0;i<MG_COUNT;i++)if(!strcmp(name,mobile_graphics_options[i].cvar))return &variables[i];
    return 0;
}
static int get_int(const void* p){return (int)lroundf(((const struct fake_var*)p)->value);}
static float get_float(const void* p){return ((const struct fake_var*)p)->value;}
static void set_var(void* p,const unsigned short* wide,unsigned flags){
    assert(flags==0x08000000u);char ascii[32];int i=0;
    for(;wide[i]&&i<31;i++)ascii[i]=(char)wide[i];ascii[i]=0;
    native_sets++;if(!refuse_write){((struct fake_var*)p)->value=(float)atof(ascii);((struct fake_var*)p)->flags=flags|0x60;}
}
static void configure(unsigned char* image){
    fixture_image(image);mobile_graphics_checked=0;mobile_graphics_base=0;
    memset(mobile_graphics_state,0,sizeof mobile_graphics_state);
    assert(mobile_graphics_init((uintptr_t)image));mobile_graphics_find=find_var;mobile_graphics_aniso_limit=16;
    for(unsigned i=0;i<sizeof mobile_graphics_classes/sizeof*mobile_graphics_classes;i++){
        mobile_graphics_classes[i].get_int=get_int;mobile_graphics_classes[i].get_float=get_float;mobile_graphics_classes[i].set=set_var;
    }
    const int kinds[MG_COUNT]={0,1,3,1,2,0};const float values[MG_COUNT]={4,0,1,1,0,1024};
    for(int i=0;i<MG_COUNT;i++){variables[i].vtable=(uintptr_t)image+mobile_graphics_classes[kinds[i]].vtable;variables[i].flags=0x01000060;variables[i].value=values[i];}
    g_mprefs=mobile_prefs_default;native_sets=0;refuse_write=0;
}
static void descriptor(unsigned char* desc,uint64_t prop){memset(desc,0,256);memcpy(desc+0x48,&prop,8);}
static void replay_actions(void* self){
    unsigned char desc[256];uint64_t actions[]={mn.p_files,mn.p_language,mn.p_extras_open,mn.p_graphics_open};
    for(unsigned i=0;i<sizeof actions/sizeof*actions;i++){
        descriptor(desc,actions[i]);hook_opt_apply(self,desc,hook_opt_get(self,desc));
    }
    for(int i=0;i<5;i++){descriptor(desc,mn.p_x[i]);hook_opt_apply(self,desc,0);}
}
int main(int argc,char** argv){
    assert(argc==2);snprintf(prefs_file,sizeof prefs_file,"%s",argv[1]);
    assert(sizeof(uintptr_t)==8&&offsetof(struct fake_var,flags)==0x18);
    unsigned char* image=calloc(1,0xb000000);assert(image);
    for(unsigned i=0;i<fixture_guard_count;i++){
        fixture_image(image);image[fixture_guards[i]]^=1;mobile_graphics_checked=0;mobile_graphics_base=0;
        assert(!mobile_graphics_init((uintptr_t)image)&&!mobile_graphics_base);
    }
    configure(image);apply_touch_graphics((uintptr_t)image);
    assert(variables[MG_ANISO].value==8&&mobile_graphics_state[MG_ANISO].original==8&&native_sets==1);
    for(int i=0;i<MG_COUNT;i++)assert(mobile_graphics_available(i)&&mobile_graphics_current_choice(i,0)==0);
    for(int i=0;i<MG_COUNT;i++){
        int last=mobile_graphics_choice_count(i)-1;
        assert(mobile_graphics_select(i,last)&&fabsf(variables[i].value-mobile_graphics_options[i].choices[last])<.0001f);
        assert(mobile_graphics_current_choice(i,last)==last);
        assert(mobile_graphics_select(i,0)&&fabsf(variables[i].value-mobile_graphics_state[i].original)<.0001f);
        assert(!mobile_graphics_select(i,-1)&&!mobile_graphics_select(i,mobile_graphics_choice_count(i)));
        assert(!mobile_graphics_write(i,NAN)&&!mobile_graphics_write(i,INFINITY));
        for(unsigned flags=4;flags<=8;flags+=4){variables[i].flags=flags;assert(!mobile_graphics_available(i)&&!mobile_graphics_select(i,1));}
        variables[i].flags=0x09000060;assert(!mobile_graphics_select(i,1));variables[i].flags=0x08000060;
    }
    mobile_graphics_aniso_limit=4;assert(mobile_graphics_choice_count(MG_ANISO)==4&&!mobile_graphics_select(MG_ANISO,4));
    mobile_graphics_aniso_limit=0;assert(!mobile_graphics_available(MG_ANISO));mobile_graphics_aniso_limit=16;
    variables[MG_MIPBIAS].vtable++;assert(!mobile_graphics_available(MG_MIPBIAS));variables[MG_MIPBIAS].vtable--;
    variables[MG_VIEWDIST].value=NAN;assert(!mobile_graphics_select(MG_VIEWDIST,1));variables[MG_VIEWDIST].value=1;
    assert(mobile_graphics_select(MG_MIPBIAS,1)&&variables[MG_MIPBIAS].value==-1);assert(mobile_graphics_select(MG_MIPBIAS,0));

    mn.ready=1;mn.audio=100;mn.graphics=101;mn.comfort=102;mn.extras=103;mn.p_graphics_open=104;mn.p_extras_open=105;
    mn.p_files=106;mn.p_sens=107;mn.p_aim=108;mn.p_fov=109;mn.p_weapon_fov=110;
    mn.p_aim_walk=111;mn.p_intro_skip=112;mn.p_start_confirm=113;mn.p_hide_pad=114;mn.p_tutorial=115;
    mn.p_aggression=116;mn.language=117;mn.p_language=118;
    mn.f_language_get=130;mn.f_language_apply=131;mn.f_get=132;mn.f_apply=133;
    for(int i=0;i<5;i++)mn.p_x[i]=120+i;for(int i=0;i<MG_COUNT;i++)mn.p_graphics[i]=200+i;
    unsigned char activity[0x300]={0},component[0x200]={0},desc[256];*(void**)(activity+0x258)=component;
    descriptor(desc,mn.p_graphics[MG_VIEWDIST]);hook_opt_apply(activity,desc,6);
    assert(g_mprefs.graphics[MG_VIEWDIST]==6&&variables[MG_VIEWDIST].value==2&&hook_opt_get(activity,desc)==6);
    refuse_write=1;hook_opt_apply(activity,desc,1);assert(g_mprefs.graphics[MG_VIEWDIST]==6&&hook_opt_get(activity,desc)==6);refuse_write=0;
    hook_opt_apply(activity,desc,999);assert(g_mprefs.graphics[MG_VIEWDIST]==6);
    descriptor(desc,999);assert(hook_opt_get(activity,desc)==73);hook_opt_apply(activity,desc,1);assert(original_applies==1&&original_gets==1);
    descriptor(desc,mn.p_graphics_open);fixture_now=100;hook_opt_apply(activity,desc,1);touch_mobile_menu_tick();assert(!menu_changes);
    fixture_now=151;touch_mobile_menu_tick();assert(menu_changes==1&&last_menu==mn.graphics);
    descriptor(desc,mn.p_extras_open);fixture_now=200;hook_opt_apply(activity,desc,1);fixture_now=251;touch_mobile_menu_tick();assert(menu_changes==2&&last_menu==mn.extras);
    row_count=0;mm_inject_graphics_open(component);mm_inject_graphics(component);assert(row_count==MG_COUNT+4);
    assert(rows[0].owning==mn.audio&&rows[0].prop==mn.p_graphics_open&&rows[0].widget==3);
    assert(rows[1].widget==0&&rows[1].link==mn.audio);
    assert(rows[2].owning==mn.graphics&&rows[2].prop==mn.p_fov&&rows[2].widget==4&&rows[2].lo==70&&rows[2].hi==110&&rows[2].snap==5);
    assert(rows[3].owning==mn.graphics&&rows[3].prop==mn.p_weapon_fov&&rows[3].widget==4&&rows[3].lo==60&&rows[3].hi==110);
    for(int i=0;i<MG_COUNT;i++)assert(rows[i+4].owning==mn.graphics&&rows[i+4].prop==mn.p_graphics[i]&&rows[i+4].count==mobile_graphics_choice_count(i));
    variables[MG_MIPBIAS].flags=4;row_count=0;mm_inject_graphics_open(component);mm_inject_graphics(component);assert(row_count==MG_COUNT+3);variables[MG_MIPBIAS].flags=0x60;
    for(int i=0;i<MG_COUNT;i++)variables[i].flags=4;
    row_count=0;mm_inject_graphics_open(component);mm_inject_graphics(component);assert(row_count==4&&rows[2].prop==mn.p_fov&&rows[3].prop==mn.p_weapon_fov);
    for(int i=0;i<MG_COUNT;i++)variables[i].flags=0x60;
    menu_placement_case(component,(void*)1,1,1);menu_placement_case(component,(void*)1,1,0);
    menu_placement_case(component,(void*)2,1,1);menu_placement_case(component,(void*)1,0,1);
    const char* languages[]={"en","fr-FR","es-419","de-DE","ja-JP","ru","ru-RU"};
    const char* autonyms[]={"English","Français","Español","Deutsch","日本語","Русский","Русский"};
    for(unsigned i=0;i<sizeof languages/sizeof *languages;i++){
        fixture_language=languages[i];options_component=component;mm_current_table_kind=1;mm_graphics_open_injected=mm_injected_this_table=1;
        descriptor(desc,mn.none);desc[0x28]=3;mm_put(desc+0x20,mn.audio);
        mm_put(desc+0x5c,mn.f_language_get);mm_put(desc+0x64,mn.f_language_apply);
        unsigned char before[256];memcpy(before,desc,sizeof before);row_count=native_add_calls=0;
        hook_menu_add_entry(component,mn.language,desc);
        assert(row_count==1&&!native_add_calls&&!memcmp(before,desc,sizeof before));
        assert(rows[0].owning==mn.audio&&rows[0].prop==mn.p_language&&rows[0].widget==3&&rows[0].count==2);
        assert(!strcmp(rows[0].values[0],autonyms[i])&&!strcmp(rows[0].values[1],autonyms[i]));
        assert(strstr(rows[0].help,i>=5?"следующем запуске":"next game launch"));
        for(int wrong=0;wrong<8;wrong++){
            memcpy(desc,before,sizeof before);uint64_t row=mn.language;
            options_component=component;mm_current_table_kind=1;
            if(wrong==0)row++;if(wrong==1)desc[0x28]=4;if(wrong==2)mm_put(desc+0x20,mn.comfort);
            if(wrong==3)mm_put(desc+0x48,mn.p_files);if(wrong==4)mm_put(desc+0x5c,mn.f_get);
            if(wrong==5)mm_put(desc+0x64,mn.f_apply);if(wrong==6)options_component=0;if(wrong==7)mm_current_table_kind=2;
            row_count=native_add_calls=0;hook_menu_add_entry(component,row,desc);assert(!row_count&&native_add_calls==1);
        }
    }
    assert(!strcmp(mm_language_autonym(0),"English")&&!strcmp(mm_language_autonym(""),"English"));
    assert(!strcmp(mm_language_autonym("rubbish"),"English")&&!strcmp(mm_language_autonym("ru_RU"),"Русский"));
    fixture_language="en";options_component=component;mm_current_table_kind=0;
    descriptor(desc,mn.p_language);assert(hook_opt_get(activity,desc)==0);
    hook_opt_apply(activity,desc,1);assert(language_requests==1&&!file_requests);
    mm_reset_replay=1;hook_opt_apply(activity,desc,0);assert(language_requests==1);mm_reset_replay=0;
    language_requests=0;
    puts("PASS: original native Language row only, seven native culture/autonym cases, unchanged source descriptor and eight foreign identity/owner/table guards; direct picker action and reset suppression");
    for(int i=0;i<L_COUNT;i++)assert(mm_text_en[i]&&*mm_text_en[i]&&mm_text_ru[i]&&*mm_text_ru[i]);
    assert(!strcmp(mm_text_en[L_X_WEAPONS],"Bonus weapons")&&!strcmp(mm_text_en[L_X_MODES],"Special modes"));
    assert(!strcmp(mm_text_ru[L_X_WEAPONS],"Бонусное оружие")&&!strcmp(mm_text_ru[L_X_MODES],"Особые режимы"));
    assert(strstr(mm_text_en[L_D_X_WEAPONS],"Chicago Typewriter")&&strstr(mm_text_en[L_D_X_WEAPONS],"Hand Cannon"));
    assert(strstr(mm_text_ru[L_D_X_WEAPONS],"Chicago Typewriter")&&strstr(mm_text_ru[L_D_X_WEAPONS],"Hand Cannon"));
    const char* modes[]={"Horror","Big Head","High Speed","Golden Guns"};
    for(unsigned i=0;i<sizeof modes/sizeof*modes;i++){
        assert(strstr(mm_text_en[L_D_X_MODES],modes[i])&&strstr(mm_text_ru[L_D_X_MODES],modes[i]));
    }

    descriptor(desc,mn.p_fov);assert(hook_opt_get_slider(activity,desc)==37&&slider_value==90);
    slider_value=105;hook_opt_apply_slider(activity,desc,37);assert(g_mprefs.fov==105&&mobile_prefs_fov()==105);
    slider_value=999;hook_opt_apply_slider(activity,desc,37);assert(g_mprefs.fov==110);
    slider_value=-9;hook_opt_apply_slider(activity,desc,37);assert(g_mprefs.fov==70);
    g_mprefs.sens=150;g_mprefs.fov=105;g_mprefs.graphics[MG_ANISO]=5;g_mprefs.graphics[MG_MIPBIAS]=1;mobile_prefs_save();
    g_mprefs=mobile_prefs_default;mobile_prefs_load();assert(g_mprefs.sens==150&&g_mprefs.fov==105&&g_mprefs.graphics[MG_ANISO]==5&&g_mprefs.graphics[MG_MIPBIAS]==1);
    io_fail=1;g_mprefs.sens=175;mobile_prefs_save();io_fail=0;mobile_prefs_load();assert(g_mprefs.sens==150);
    g_mprefs.graphics[MG_MIPBIAS]=-99;g_mprefs.graphics[MG_ANISO]=999;mobile_prefs_sanitize(&g_mprefs);
    assert(g_mprefs.graphics[MG_MIPBIAS]==0&&g_mprefs.graphics[MG_ANISO]==5);
    native_reset_replay=replay_actions;
    g_mprefs.graphics[MG_MIPBIAS]=1;hook_opt_reset(activity,mn.comfort);
    assert(g_mprefs.sens==100&&g_mprefs.fov==105&&g_mprefs.graphics[MG_ANISO]==5&&g_mprefs.graphics[MG_MIPBIAS]==1&&original_resets==1);
    assert(!file_requests&&!language_requests&&!mm_pending_menu_component&&!mm_reset_replay);
    assert(!confirm_requests&&!unlock_calls);
    g_mprefs.sens=150;hook_opt_reset(activity,mn.graphics);assert(g_mprefs.sens==150&&g_mprefs.fov==90&&original_resets==1&&refreshes==1);
    for(int i=0;i<MG_COUNT;i++)assert(g_mprefs.graphics[i]==0&&fabsf(variables[i].value-mobile_graphics_state[i].original)<.0001f);
    g_mprefs.fov=110;g_mprefs.graphics[MG_VIEWDIST]=6;hook_opt_reset(activity,mn.audio);assert(g_mprefs.sens==150&&g_mprefs.fov==90&&g_mprefs.graphics[MG_VIEWDIST]==0&&original_resets==2);
    assert(!file_requests&&!language_requests&&!mm_pending_menu_component&&!mm_reset_replay);
    descriptor(desc,mn.p_files);hook_opt_apply(activity,desc,1);assert(file_requests==1);
    descriptor(desc,mn.p_graphics_open);hook_opt_apply(activity,desc,1);assert(mm_pending_menu_component==component);
    mm_pending_menu_component=0;native_reset_replay=0;
    assert(!mobile_prefs_default.aim_toggle&&!mobile_prefs_default.qturn&&mobile_prefs_default.aim_walk);
    assert(!mobile_prefs_default.intro_skip&&!mobile_prefs_default.start_confirm&&!mobile_prefs_default.hide_gamepad_hints);
    assert(mobile_prefs_default.weapon_fov==90);
    assert(mobile_prefs_default.gyro==1&&!mobile_prefs_default.aggression);
    descriptor(desc,mn.p_aggression);hook_opt_apply(activity,desc,1);
    assert(g_mprefs.aggression==1&&hook_opt_get(activity,desc)==1);
    g_mprefs.aggression=0;mobile_prefs_load();assert(g_mprefs.aggression==1);
    hook_opt_apply(activity,desc,0);assert(!g_mprefs.aggression);

    for(int i=0;i<5;i++){
        descriptor(desc,mn.p_x[i]);int before=confirm_requests,unlocks=unlock_calls;
        hook_opt_apply(activity,desc,1);assert(confirm_requests==before+1&&confirm_kind==i+1&&unlock_calls==unlocks);
        confirm_reply=-1;touch_mobile_menu_tick();assert(unlock_calls==unlocks);
        hook_opt_apply(activity,desc,1);confirm_reply=1;touch_mobile_menu_tick();assert(unlock_calls==unlocks+1);
        touch_mobile_menu_tick();assert(unlock_calls==unlocks+1);
    }
    row_count=0;mm_inject_extras(component);assert(row_count==6&&rows[0].widget==0);
    for(int i=1;i<=5;i++){
        assert(rows[i].widget==3&&rows[i].owning==mn.extras&&rows[i].prop==mn.p_x[i-1]&&rows[i].count==4&&!rows[i].link);
        assert(!strcmp(rows[i].label,mm_text_en[L_X_MERC+i-1]));
        assert(!strcmp(rows[i].values[0],"Open")&&!strcmp(rows[i].values[1],"Available"));
        assert(!strcmp(rows[i].values[2],"Saving...")&&!strcmp(rows[i].values[3],"Retry save"));
    }
    unsigned char visible_menu[0x240]={0};current_menu=visible_menu;
    *(void**)(component+0xa0)=activity;options_component=component;
    *(int*)(menu_name_property+0x4c)=0x220;mm_put(visible_menu+0x220,mn.extras);
    extras_unlocked=0;extras_last_action=extras_status=0;mm_extras_display_state=mm_extras_values(touch_mobile_extras_snapshot());
    refresh_readback=1;int refreshed=refreshes,unlocks=unlock_calls,confirms=confirm_requests,resets=original_resets,applies=original_applies,changed=menu_changes;
    touch_mobile_menu_tick();assert(refreshes==refreshed);
    confirm_reply=-1;touch_mobile_menu_tick();assert(refreshes==refreshed); /* Cancel changes no native state. */
    extras_unlocked=2;extras_last_action=2;extras_status=6;extras_pending=2;touch_mobile_menu_tick();
    assert(refreshes==++refreshed&&displayed_extras[1]==2); /* Never show saved availability while pending. */
    touch_mobile_menu_tick();assert(refreshes==refreshed);
    extras_status=1;extras_pending=0;touch_mobile_menu_tick();assert(refreshes==++refreshed&&displayed_extras[1]==1);
    extras_status=2;touch_mobile_menu_tick();assert(refreshes==refreshed); /* Same displayed value needs no refresh. */
    extras_status=7;extras_failed=2;touch_mobile_menu_tick();assert(refreshes==++refreshed&&displayed_extras[1]==3);
    extras_last_action=3;extras_status=2;touch_mobile_menu_tick();assert(refreshes==refreshed&&displayed_extras[1]==3);
    extras_status=6;extras_failed=0;extras_pending=2;touch_mobile_menu_tick();assert(refreshes==++refreshed&&displayed_extras[1]==2);
    extras_status=1;extras_pending=0;game_thread=0;touch_mobile_menu_tick();assert(refreshes==refreshed);game_thread=1;
    touch_mobile_menu_tick();assert(refreshes==++refreshed&&displayed_extras[1]==1);
    mm_put(visible_menu+0x220,mn.graphics);extras_unlocked=6;touch_mobile_menu_tick();assert(refreshes==refreshed);
    mm_put(visible_menu+0x220,mn.extras);refresh_readback=0;fixture_refresh_display(component);refresh_readback=1;touch_mobile_menu_tick();
    assert(refreshes==refreshed&&displayed_extras[2]==1); /* Native navigation queries fresh values itself. */
    extras_unlocked=14;name_property_valid=0;touch_mobile_menu_tick();assert(refreshes==refreshed);name_property_valid=1;
    touch_mobile_menu_tick();assert(refreshes==++refreshed&&displayed_extras[3]==1);
    name_property_lowercase=0;extras_unlocked=15;touch_mobile_menu_tick();assert(refreshes==++refreshed&&displayed_extras[0]==1);
    name_property_lowercase=1;
    extras_unlocked=30;dead_object=activity;touch_mobile_menu_tick();assert(refreshes==refreshed);dead_object=0;
    touch_mobile_menu_tick();assert(refreshes==++refreshed&&displayed_extras[4]==1); /* Temporary owner gap never loses binding. */
    weak_assign(&mm_extras_component,component);weak_serial++;touch_mobile_menu_tick();assert(refreshes==refreshed);
    weak_assign(&mm_extras_component,component);*(void**)(activity+0x258)=0;touch_mobile_menu_tick();assert(refreshes==refreshed);
    *(void**)(activity+0x258)=component;refresh_readback=0;
    descriptor(desc,mn.p_x[1]);hook_opt_get(activity,desc);extras_failed=2;
    refresh_readback=1;touch_mobile_menu_tick();assert(refreshes==++refreshed&&displayed_extras[1]==3);refresh_readback=0;
    assert(unlock_calls==unlocks&&confirm_requests==confirms&&original_resets==resets&&original_applies==applies&&menu_changes==changed);
    descriptor(desc,mn.p_weapon_fov);slider_value=55;hook_opt_apply_slider(activity,desc,37);assert(mobile_prefs_weapon_fov()==60);
    slider_value=120;hook_opt_apply_slider(activity,desc,37);assert(mobile_prefs_weapon_fov()==110);
    g_mprefs.aim_walk=0;g_mprefs.intro_skip=1;g_mprefs.start_confirm=1;g_mprefs.hide_gamepad_hints=1;mobile_prefs_save();
    g_mprefs=mobile_prefs_default;mobile_prefs_load();
    assert(!mobile_prefs_aim_walk()&&mobile_prefs_intro_skip()&&mobile_prefs_start_confirm()&&mobile_prefs_hide_gamepad_hints()&&mobile_prefs_weapon_fov()==110);
    configure(image);g_mprefs.graphics[MG_MIPBIAS]=1;apply_touch_graphics((uintptr_t)image);assert(variables[MG_MIPBIAS].value==-1);
    configure(image);variables[MG_STATICLOD].vtable=0;variables[MG_SHADOWRES].value=0;apply_touch_graphics((uintptr_t)image);
    assert(!mobile_graphics_available(MG_STATICLOD)&&!mobile_graphics_available(MG_SHADOWRES));
    int before=native_sets;apply_touch_graphics((uintptr_t)image);assert(native_sets==before);
    free(image);puts("PASS: guarded graphics/menu adapter; original Audio placement and scoped reset; live Extras values and one material-change refresh only on same weak-owned visible list; pending/retry/cancel/stale/thread/ownership guards with no actions on refresh");return 0;
}
