#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static volatile int g_touch_gt_tid,g_touch_context,g_touch_cancel,g_reload_phase,g_touch_reload_request;
static int fixture_tid=41,ti_ready=1;
#define gettid() fixture_tid
static void* (*tw_hand_class)(void),*(*tw_pawn_class)(void),*(*tw_grip_class)(void),*(*ti_prop_class)(void);
static void* (*ti_parent)(const void*),*(*phone_player)(const void*),*(*phone_active)(void);
static unsigned (*phone_type)(void);
static void* (*rl_primary)(const void*),*(*f_GetCurrentGrip)(const void*);
static int (*rl_paused)(void),(*rl_player_valid)(void);
static int tw_grab_paused(void){return rl_paused();}   // weapon tutorial pause reasons are covered by test_weapon.c
static int reload_grip_registered(void*);
/* SHARED_TYPED */
#define LOGF(...) ((void)0)
#include "QUICKSELECT_HEADER"
enum {C_FIRE,C_BACK,C_CONFIRM};
static int mobile_prefs_start_confirm(void){return 0;}
enum {OB_RINDEX=0x04000000u,OB_LINDEX=0x10000000u};
static struct {unsigned bit;} g_ctl[2];
static int g_fire_slot=-1,g_fire_side=-1;
static long fixture_now;
static long now_ms(void){return fixture_now;}
static int touch_motion_pose_busy(void){return 0;}
static int touch_throw_mode(long now){(void)now;return 0;}   // no spear/lantern in this fixture
/* FIRE_BINDING */
#ifdef QUICKSELECT_NATIVE_FIXTURE
#include QUICKSELECT_NATIVE_FIXTURE
#endif
static struct {
    unsigned char hand[2][0x570],pool[0x570],pawn[0x1300],other[0x1300],item[6][0xe00],grip[6][0x460];
    unsigned char cls[5][0x100],settings[0xb8];void* chains[5][2];void* vtable[0x680/8];
} f __attribute__((aligned(16)));
static void* active,*assigned[10],*current[2],*primary[6];
static unsigned registered,context_valid,equip_valid;static int wheel,paused,valid,mode_set,equip_calls,copy_calls,destroy_calls,set_calls;
static int accept_equip,cancel_in_mode,changed_name_in_mode,base_wheel,requires_mode,context_in_equip;
static unsigned char native_prop_data[8];static const unsigned char* prop_data(void){return native_prop_data;}
static void* cls_hand(void){return f.cls[0];}static void* cls_pawn(void){return f.cls[1];}
static void* cls_grip(void){return f.cls[2];}static void* cls_prop(void){return f.cls[3];}
static void* cls_gun(void){return f.cls[4];}
static void* player(const void* hand){return hand==f.hand[0] || hand==f.hand[1] || hand==f.pool ? f.pawn : 0;}
static void* native_hand_override[2];
static void* native_hand(const void* pawn,unsigned char side){assert(pawn==f.pawn && side<2);return native_hand_override[side] ? native_hand_override[side] : f.hand[side];}
static void* active_pawn(void){return active;}static unsigned native_type=1;
static unsigned type(void){return native_type;}static int is_paused(void){return paused;}
static int player_valid(void){return valid;}
static int index_item(const void* item){for(int i=0;i<6;i++)if(item==f.item[i])return i;return -1;}
static int slot_index(int slot){const int slots[]={0,1,2,3,6};for(int i=0;i<5;i++)if(slots[i]==slot)return i;return -1;}
static int reload_grip_registered(void* grip){for(int i=0;i<6;i++)if(grip==f.grip[i])return (registered>>i)&1;return 0;}
static void* parent(const void* grip){return *(void* const*)((const char*)grip+0x300);}
static void* get_prop(const void* pawn,unsigned char slot){assert(pawn==f.pawn && slot<10);return assigned[slot];}
static int left_slot=-1;
static void* get_hand(const void* pawn,unsigned char slot){assert(pawn==f.pawn);return f.hand[slot==left_slot ? 0 : 1];}
static void* get_primary(const void* item){int i=index_item(item);assert(i>=0);return primary[i];}
static void* get_current(const void* hand){assert(hand==f.hand[0] || hand==f.hand[1] || hand==f.pool);return hand==f.pool ? 0 : current[hand==f.hand[1]];}
static int allowed(const void* pawn){assert(pawn==f.pawn);return wheel && (!requires_mode || f.settings[0x3d]==1);}
static int base_allowed(const void* pawn){assert(pawn==f.pawn);return base_wheel;}
static int context(const void* item){int i=index_item(item);assert(i>=0);return (context_valid>>i)&1;}
static int can_equip(const void* item){int i=index_item(item);assert(i>=0);return (equip_valid>>i)&1;}
static const void* get_settings(void){return f.settings;}
static void copy_settings(void* dst,const void* src){assert(src==f.settings && !((uintptr_t)dst&15));copy_calls++;memcpy(dst,src,0xb8);}
static void destroy_settings(void* dst){assert(dst!=f.settings);destroy_calls++;}
static void set_settings(const void* src){
    assert(src!=f.settings);unsigned char expected[0xb8];memcpy(expected,f.settings,sizeof expected);expected[0x3d]=1;
    expected[0x3d]=((const unsigned char*)src)[0x3d];assert(expected[0x3d]<=1);
    assert(!memcmp(src,expected,sizeof expected));set_calls++;if(mode_set)memcpy(f.settings,src,0xb8);
    if(cancel_in_mode)g_touch_cancel=2;if(changed_name_in_mode)(*(uint64_t*)(f.pawn+0x18))++;
}
static void* equip(void* pawn,unsigned char slot){
    assert(pawn==f.pawn && f.settings[0x3d]==1 && wheel && context(assigned[slot]) && can_equip(assigned[slot]));
    equip_calls++;int i=slot_index(slot);assert(i>=0);
    if(accept_equip){if(slot!=6)native_prop_data[6]=slot;memset(primary,0,sizeof primary);current[0]=current[1]=0;primary[i]=get_hand(pawn,slot);current[primary[i]==f.hand[1]]=f.grip[i];}
    if(context_in_equip)context_valid&=~(1u<<i);
    return assigned[slot];
}
static void ready(void){
    memset(&f,0,sizeof f);memset(&qs_pending,0,sizeof qs_pending);qs_ui=qs_request=0;qs_pawn=0;qs_pawn_name=0;qs_generation=0;qs_logs=0;qs_busy=0;qs_pose_generation=0;
    for(int i=0;i<5;i++){
        *(int*)(f.cls[i]+0x58)=i==0 ? sizeof f.hand[0] : i==1 ? sizeof f.pawn : i==2 ? sizeof f.grip[0] : sizeof f.item[0];
        *(int*)(f.cls[i]+0x38)=i==4 ? 1 : 0;f.chains[i][0]=f.cls[i==4 ? 3 : i]+0x30;
        f.chains[i][1]=f.cls[4]+0x30;*(void***)(f.cls[i]+0x30)=f.chains[i];
    }
    const int slots[]={0,1,2,3,6};memset(assigned,0,sizeof assigned);memset(primary,0,sizeof primary);current[0]=current[1]=0;
    for(int i=0;i<6;i++){
        if(i<5)assigned[slots[i]]=f.item[i];*(void**)(f.item[i]+0x10)=f.cls[i<2 || i==5 ? 4 : 3];*(void***)(f.item[i])=f.vtable;
        *(uint64_t*)(f.item[i]+0x18)=100+i;*(void**)(f.item[i]+0x3c0)=f.pawn;f.item[i][0x2dc]=i<5 ? slots[i] : 255;
        *(void**)(f.item[i]+0x270)=f.grip[i];*(void**)(f.grip[i]+0x10)=f.cls[2];
        *(uint64_t*)(f.grip[i]+0x18)=200+i;*(void**)(f.grip[i]+0x20)=*(void**)(f.grip[i]+0x300)=f.item[i];
    }
    for(int i=0;i<2;i++){*(void**)(f.hand[i]+0x10)=f.cls[0];f.hand[i][0x314]=i;*(uint64_t*)(f.hand[i]+0x18)=300+i;}
    *(void**)(f.pool+0x10)=f.cls[0];f.pool[0x314]=1;*(uint64_t*)(f.pool+0x18)=310;native_hand_override[0]=native_hand_override[1]=0;
    *(void**)(f.pawn+0x10)=f.cls[1];*(uint64_t*)(f.pawn+0x18)=400;f.vtable[0x678/8]=(void*)can_equip;
    for(unsigned i=0;i<sizeof f.settings;i++)f.settings[i]=(unsigned char)(i*3);f.settings[0x3d]=0;
    active=f.pawn;registered=context_valid=equip_valid=63;base_wheel=wheel=valid=mode_set=accept_equip=1;requires_mode=0;
    paused=cancel_in_mode=changed_name_in_mode=context_in_equip=0;native_type=1;left_slot=-1;fixture_tid=41;
    fixture_now=1000;g_fire_slot=g_fire_side=-1;
    g_touch_gt_tid=41;g_touch_context=1;g_touch_cancel=g_reload_phase=g_touch_reload_request=0;
    equip_calls=copy_calls=destroy_calls=set_calls=0;qs_ready=1;
    memset(native_prop_data,0,sizeof native_prop_data);native_prop_data[6]=255;qs_prop_data=prop_data;
    tw_hand_class=cls_hand;tw_pawn_class=cls_pawn;tw_grip_class=cls_grip;ti_prop_class=cls_prop;ti_parent=parent;
    phone_player=player;phone_active=active_pawn;phone_type=type;rl_primary=get_primary;f_GetCurrentGrip=get_current;
    rl_paused=is_paused;rl_player_valid=player_valid;qs_settings=get_settings;qs_copy=copy_settings;qs_set=set_settings;qs_destroy=destroy_settings;
    qs_native_hand=native_hand;qs_prop=get_prop;qs_hand=get_hand;qs_equip=equip;qs_allowed=allowed;qs_base_allowed=base_allowed;qs_context=context;qs_gun_class=cls_gun;
}
static void request_tick(int slot,long now){assert(touch_quickselect_request(slot,now));assert(touch_quickselect_busy());touch_quickselect_tick(f.hand[1],now+1);}
static int world_hover_clears;
static void f_SetGripForcedHover(void* hand,void* grip,int on){assert(hand==f.hand[1] && grip==f.grip[4] && !on);world_hover_clears++;}
/* WORLD_GRAB_TICK */
int main(void){
#ifdef QUICKSELECT_NATIVE_FIXTURE
    verify_quickselect_install_guard();
#endif
    ready();unsigned char before[sizeof f];memcpy(before,&f,sizeof f);touch_quickselect_tick(f.hand[1],1000);
    assert(!memcmp(before,&f,sizeof f));struct touch_quickselect_state s=touch_quickselect_state(1000);
    assert(s.available==0x4f && s.selected==-1 && s.fire_side==-1 && !s.mode);
    request_tick(1,1001);s=touch_quickselect_state(1002);
    assert(equip_calls==1 && copy_calls==1 && set_calls==1 && destroy_calls==1 && s.mode && s.selected==1 && s.fire_side==1);
    assert(!touch_quickselect_busy() && touch_quickselect_pose_generation()>=1);
    assert(touch_equipped_gun(f.hand[1])==f.item[1] && !touch_equipped_gun(f.hand[0]));
    request_tick(3,1003);assert(equip_calls==2 && copy_calls==1 && !touch_equipped_gun(f.hand[1]));
    request_tick(2,1005);assert(touch_quickselect_state(1006).selected==2);
    equip_valid&=~4u;touch_quickselect_tick(f.hand[1],1010);s=touch_quickselect_state(1010);
    assert(s.selected==2 && s.fire_side==1 && !(s.available&4));
    request_tick(6,1011);s=touch_quickselect_state(1012);assert(s.selected==6 && s.fire_side==1 && native_prop_data[6]==2);
    equip_valid&=~16u;touch_quickselect_tick(f.hand[1],1015);assert(touch_quickselect_state(1015).selected==6);
    ready();left_slot=1;touch_quickselect_tick(f.hand[1],1000);request_tick(1,1001);
    assert(touch_quickselect_state(1002).fire_side==0 && touch_equipped_gun(f.hand[0])==f.item[1]);
    assert(!touch_quickselect_request(4,1003) && !touch_quickselect_request(9,1003));
    ready();touch_quickselect_tick(f.hand[1],1000);assert(!touch_quickselect_request(0,1501));
    assert(!touch_quickselect_request(0,999));assert(!touch_quickselect_state(1501).available);
    for(int why=0;why<10;why++){
        ready();touch_quickselect_tick(f.hand[1],1000);assert(touch_quickselect_request(0,1001));
        if(why==0)wheel=base_wheel=0;if(why==1)context_valid&=~1u;if(why==2)equip_valid&=~1u;
        if(why==3)registered&=~1u;if(why==4)*(void**)(f.item[0]+0x3c0)=f.other;
        if(why==5)*(void**)(f.grip[0]+0x300)=f.item[1];if(why==6)f.item[0][0x2dc]=1;
        if(why==7)g_reload_phase=1;if(why==8)g_touch_cancel=2;if(why==9)active=f.other;
        touch_quickselect_tick(f.hand[1],1002);assert(!equip_calls && !set_calls);
    }
    ready();touch_quickselect_tick(f.hand[1],1000);assert(touch_quickselect_request(0,1001));
    (*(uint64_t*)(f.pawn+0x18))++;touch_quickselect_tick(f.hand[1],1002);assert(!equip_calls && !set_calls);
    ready();touch_quickselect_tick(f.hand[1],1000);assert(touch_quickselect_request(0,1001));
    fixture_tid=42;touch_quickselect_tick(f.hand[1],1002);assert(!equip_calls && !set_calls && !touch_equipped_gun(f.hand[1]));
    fixture_tid=41;touch_quickselect_cancel();touch_quickselect_tick(f.hand[1],1003);assert(!equip_calls);
    for(int why=0;why<3;why++){
        ready();touch_quickselect_tick(f.hand[1],1000);mode_set=why!=0;cancel_in_mode=why==1;changed_name_in_mode=why==2;
        request_tick(0,1001);assert(set_calls==2 && destroy_calls==1 && !equip_calls && f.settings[0x3d]==0);
    }
    ready();requires_mode=1;touch_quickselect_tick(f.hand[1],1000);request_tick(0,1001);assert(equip_calls==1);
    ready();wheel=0;touch_quickselect_tick(f.hand[1],1000);request_tick(0,1001);
    assert(equip_calls==0 && set_calls==2 && destroy_calls==1 && f.settings[0x3d]==0);
    ready();accept_equip=0;touch_quickselect_tick(f.hand[1],1000);request_tick(0,1001);
    assert(equip_calls==1 && qs_pending.until);touch_quickselect_tick(f.hand[1],2802);assert(!qs_pending.until && equip_calls==1);
    assert(!touch_quickselect_busy());
    current[1]=f.grip[0];primary[0]=f.hand[1];touch_quickselect_tick(f.hand[1],3000);assert(equip_calls==1);
    // Execute the production input predicate against actual QuickSelect snapshots.
    for(int slot=0;slot<2;slot++)for(int side=0;side<2;side++){
        ready();left_slot=side ? -1 : slot;
        touch_quickselect_tick(f.hand[1],1000);request_tick(slot,1001);
        context_valid&=~(1u<<slot);equip_valid&=~(1u<<slot);wheel=base_wheel=0;
        touch_quickselect_tick(f.hand[1],1010);fixture_now=1010;
        s=touch_quickselect_state(fixture_now);
        assert(s.selected==slot && s.fire_side==side && !(s.available&(1u<<slot)));
        assert(touch_equipped_gun(f.hand[side])==f.item[slot]);
        assert(!touch_quickselect_request(slot,fixture_now));
        g_fire_slot=s.selected;g_fire_side=s.fire_side;
        assert(ctl_bit(C_FIRE)==(side ? OB_RINDEX : OB_LINDEX));
        context_valid|=1u<<slot;touch_quickselect_tick(f.hand[1],1020);fixture_now=1020;
        assert(ctl_bit(C_FIRE)==(side ? OB_RINDEX : OB_LINDEX));
        fixture_now=1521;assert(!ctl_bit(C_FIRE));
        fixture_now=1020;current[side]=0;touch_quickselect_tick(f.hand[1],1030);
        assert(touch_quickselect_state(1030).selected==-1 && !ctl_bit(C_FIRE));
    }
    // A previously advertised request must be checked again against native context.
    ready();touch_quickselect_tick(f.hand[1],1000);request_tick(0,1001);
    assert(touch_quickselect_request(0,1003));context_valid&=~1u;
    touch_quickselect_tick(f.hand[1],1004);assert(equip_calls==1 && !qs_pending.until);
    assert(touch_quickselect_state(1004).selected==0 && !touch_quickselect_busy());
    // An accepted original transaction can close its holster gate before acknowledgement.
    ready();touch_quickselect_tick(f.hand[1],1000);context_in_equip=1;request_tick(1,1001);
    s=touch_quickselect_state(1002);assert(s.selected==1 && s.fire_side==1 && !qs_pending.until && !touch_quickselect_busy());
    touch_quickselect_tick(f.hand[1],1003);assert(!(touch_quickselect_state(1003).available&2u));
    fixture_now=1003;g_fire_slot=1;g_fire_side=1;assert(ctl_bit(C_FIRE)==OB_RINDEX);
    // Fresh physical Fire never follows another item or survives a hard context reset.
    context_in_equip=0;context_valid=31;wheel=base_wheel=1;request_tick(0,1004);
    fixture_now=1005;assert(!ctl_bit(C_FIRE));
    g_fire_slot=0;g_fire_side=1;assert(ctl_bit(C_FIRE)==OB_RINDEX);
    g_touch_context=2;touch_quickselect_tick(f.hand[1],1006);assert(!ctl_bit(C_FIRE));
    ready();touch_quickselect_tick(f.hand[1],1000);request_tick(0,1001);
    context_valid&=~1u;*(void**)(f.item[0]+0x3c0)=f.other;touch_quickselect_tick(f.hand[1],1003);
    assert(touch_quickselect_state(1003).selected==-1);
    // An inactive pooled right hand never retires the publisher of the actual current pawn hand.
    ready();touch_quickselect_tick(f.hand[1],1000);request_tick(1,1001);s=touch_quickselect_state(1002);
    assert(s.selected==1 && s.fire_side==1);
    touch_quickselect_tick(f.pool,1003);s=touch_quickselect_state(1003);
    assert(s.selected==1 && s.fire_side==1 && qs_pawn==f.pawn && touch_quickselect_request(0,1003));
    touch_quickselect_tick(f.hand[1],1004);assert(touch_quickselect_state(1004).selected==0 && equip_calls==2);
    // Once the native current hand really is the other object, only that hand publishes.
    native_hand_override[1]=f.pool;touch_quickselect_tick(f.hand[1],1005);
    assert(touch_quickselect_state(1005).selected==0 && qs_pawn==f.pawn);
    touch_quickselect_tick(f.pool,1600);s=touch_quickselect_state(1600);
    assert(s.selected==0 && s.fire_side==1 && qs_pawn==f.pawn);
    // A pooled caller cannot hide a paused/foreign/cancelled state.
    for(int why=0;why<3;why++){
        ready();touch_quickselect_tick(f.hand[1],1000);request_tick(1,1001);
        if(why==0)paused=1;if(why==1)active=f.other;if(why==2)g_touch_cancel=2;
        touch_quickselect_tick(f.pool,1003);assert(touch_quickselect_state(1003).selected==-1);
    }
    puts("PASS: inactive pooled right hand preserves actual current-hand publication; real native current-hand change, pause, foreign pawn and cancel still retire it");
    // Real range/counter guns are held through their PrimaryGrip without ever
    // occupying a pawn slot. The old publisher hid Fire when native_slot was stale.
    for(int side=0;side<2;side++)for(int stale=0;stale<3;stale++){
        ready();native_prop_data[6]=stale==0 ? 3 : stale==1 ? 255 : 0;
        current[side]=f.grip[5];primary[5]=f.hand[side];
        assert(touch_equipped_gun(f.hand[side])==f.item[5]);
        touch_quickselect_tick(f.hand[1],1000);s=touch_quickselect_state(1000);
        assert(s.selected==QS_HELD_WORLD_GUN && s.fire_side==side && !(s.available&(1u<<QS_HELD_WORLD_GUN)));
        fixture_now=1000;g_fire_slot=s.selected;g_fire_side=side;
        assert(ctl_bit(C_FIRE)==(side ? OB_RINDEX : OB_LINDEX));
        assert(!touch_quickselect_request(QS_HELD_WORLD_GUN,1001) && !equip_calls && !set_calls);
        current[side]=0;touch_quickselect_tick(f.hand[1],1010);assert(touch_quickselect_state(1010).selected==-1 && !ctl_bit(C_FIRE));
    }
    for(int invalid=0;invalid<6;invalid++){
        ready();native_prop_data[6]=255;current[1]=f.grip[5];primary[5]=f.hand[1];
        if(invalid==0)registered&=~(1u<<5);
        if(invalid==1)*(void**)(f.grip[5]+0x300)=f.item[0];
        if(invalid==2)*(void**)(f.grip[5]+0x20)=f.item[0];
        if(invalid==3)*(void**)(f.item[5]+0x270)=f.grip[0];
        if(invalid==4)primary[5]=f.hand[0];
        if(invalid==5)*(void**)(f.item[5]+0x10)=f.cls[3];
        touch_quickselect_tick(f.hand[1],1000);
        assert(!touch_equipped_gun(f.hand[1]) && touch_quickselect_state(1000).selected==-1);
    }
    // A touch counter grab in mode0 must acquire native retention before its 350ms grip pulse ends.
    ready();current[1]=f.grip[5];primary[5]=f.hand[1];world_hover_clears=0;
    wt_grab.hand=f.hand[1];wt_grab.grip=f.grip[4];wt_grab.off_at=1700;
    wt_grab_tick(f.pool,1100);assert(!set_calls && !world_hover_clears);
    wt_grab_tick(f.hand[1],1100);assert(f.settings[0x3d]==1 && copy_calls==1 && set_calls==1 && destroy_calls==1 && !equip_calls);
    wt_grab_tick(f.hand[1],1349);assert(set_calls==1 && !world_hover_clears);
    wt_grab_tick(f.hand[1],1700);assert(world_hover_clears==1 && !wt_grab.grip && current[1]==f.grip[5] && !equip_calls);
    for(int side=0;side<2;side++){
        ready();current[side]=f.grip[5];primary[5]=f.hand[side];
        assert(touch_quickselect_world_retention(f.hand[side]));
        assert(f.settings[0x3d]==1 && set_calls==1 && copy_calls==1 && destroy_calls==1 && !equip_calls);
        assert(current[side]==f.grip[5] && !current[1-side]);
        assert(touch_quickselect_world_retention(f.hand[side]) && set_calls==1);
    }
    for(int why=0;why<10;why++){
        ready();current[1]=f.grip[5];primary[5]=f.hand[1];
        if(why==0)g_touch_cancel=1;if(why==1)g_touch_context=2;if(why==2)g_reload_phase=1;
        if(why==3)g_touch_reload_request=1;if(why==4)qs_busy=1;if(why==5)fixture_tid=42;
        if(why==6)active=f.other;if(why==7)current[1]=0;
        if(why==8)*(void**)(f.item[5]+0x3c0)=f.other;if(why==9)primary[5]=f.hand[0];
        assert(!touch_quickselect_world_retention(f.hand[1]) && !copy_calls && !set_calls && !equip_calls);
    }
    ready();current[1]=f.grip[0];primary[0]=f.hand[1];native_prop_data[6]=0;
    assert(!touch_quickselect_world_retention(f.hand[1]) && !set_calls); // assigned icons retain their original transaction
    for(int why=0;why<3;why++){
        ready();current[1]=f.grip[5];primary[5]=f.hand[1];
        mode_set=why!=0;cancel_in_mode=why==1;changed_name_in_mode=why==2;
        assert(!touch_quickselect_world_retention(f.hand[1]));
        assert(f.settings[0x3d]==0 && set_calls==2 && copy_calls==1 && destroy_calls==1 && !equip_calls);
    }
    puts("PASS: actual counter-grab tick changes only native retention before350ms; both-hand world guns, no equip/grip/slot writes, once-only copy/destructor and rollback; busy/foreign/pool/thread/context/cancel refusals");
    puts("PASS: actual held range/world gun publishes Fire side for either hand despite stale native slot; no assignment/equip, and six invalid grip/class refusals");
    puts("PASS: native mode copy/Set/destructor, existing five slots, real Gun2/left-hand/retained consumable, no inventory writes");
    puts("PASS: held Gun1/Gun2 both hands retain actual Fire binding while selection context/eligibility closes; requests still refuse; pending acknowledgement, expiry/owner/grip/item/context retirement");
    puts("PASS: expired/future snapshots, owner/name/parent/registry/context/eligibility/GameThread/cancel/timeout refusal; one equip per intent");
    return 0;
}
