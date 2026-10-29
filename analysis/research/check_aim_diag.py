"""Check native probe layouts and execute read-only pose/guard tests; no device access."""
import os
from pathlib import Path
import re
import hashlib
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
header = root / 'analysis/re4fix_aim_diag.h'
header_text=header.read_text(encoding='utf8')
native = (root / 'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
checks = re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}', header_text)
assert len(checks) == 36
for addr, first, second in checks:
    assert struct.unpack_from('<II',native,int(addr,16)) == (int(first,16),int(second,16)), addr
field_body=header_text.split('gate_fields[] = {',1)[1].split('};',1)[0]
fields=[tuple(int(value,16) for value in entry) for entry in re.findall(r'\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}',field_body)]
assert fields==[(0x9dadefc,0xd38),(0x9d9e2bc,0x300)]
for address,value in fields:
    assert struct.unpack_from('<I',native,address)[0]==value
# GetHeadTransform copies the SceneComponent's actual world transform at +0x1c0.
assert struct.unpack_from('<IIIIII',native,0x6341aec) == (
    0xf9416c09,0xad4e8122,0x3dc07121,0xad008102,0x3d800101,0xd65f03c0)
assert struct.unpack_from('<12f',native,0x4315110)==(0,0,0,1,0,0,0,1,1,1,0,0)
assert struct.unpack_from('<II',native,0x83a92ec)==(0xb94026e8,0xf841c2e9)
for lo,hi,digest in (
    (0x628d20c,0x628d268,'00fca8160d88442f8daf5d12b44fa20380d8d354bc2e5977c72175e94ebb570f'),
    (0x628d268,0x628d2d8,'a0fb3d45c4829e846a45deecadecb535fddaf8aa4c6ff0aa1852b85ea1c56f5f'),
    (0x628d978,0x628d9fc,'516851a518c0dee106d5a6a8721c4ccc6ce2ce722dcf9c260157e1df6f5ef293'),
    (0x633d040,0x633d048,'0cf42e6293b07a4703d52928e1e77207778b40b661ba749f8ea3a76a9de74b09'),
    (0x6214c24,0x6214c2c,'8e15b199b0a8468895ec5dac862fcf8f66c74861c718031d9aa8ab2a1beb50d1'),
    (0x628d2d8,0x628d2e8,'18dfcc5d1874092807252117a9f8176288994f331152149f0043a0c680d6c9fd'),
    (0x63c56e4,0x63c57a4,'fdb92287f98ef21f5e32f8ba976f00dd5708bc5c2e63cd301d146a46cf1ce363'),
    (0x6213b60,0x621466c,'fa6180e0f9018efd30f93cc89dea6b2b013497fa39618c564631f3a4f9b64334'),
    (0x62bb880,0x62bb8c8,'e557686b66c8c1f443f8c737d46e112e69b76c10e4fed702d0c05f449eaad5bd'),
    (0x62bb584,0x62bb5cc,'9c624ca291742e086ca6abdfc29191913b54569226d1f48a979bb90bbaf772a1'),
    (0x6338b5c,0x6338b64,'2b59db05426e0f313d72b30f3398c65721de2562a59dc46ceeba224f98d0b876'),
    (0x62b6034,0x62b6078,'46557c00416297869525f1187d7f52dfb6dab01b8f5a5b025f8aedae03f74491'),
    (0x62825b0,0x6282608,'a2ee0ecb64834b472f241f0388e9e07d52359d7eefaa6f99e21f9fe4298c4d20'),
    (0x642e548,0x642e608,'8348cc40cd0eb7e53a7474210546116048d7b8d8dfa9977b14655cd01a30c129'),
    (0x64192f4,0x64193b4,'52bdf762b2f6bd84f413bf968cca1348bb4590163b75b88ca525c6be82bf6dbc'),
):
    assert hashlib.sha256(native[lo:hi]).hexdigest()==digest,hex(lo)
# Resolve the actual Bio4 hand virtual +0x730 rather than assuming a grab-age getter.
sections=struct.unpack_from('<Q',native,0x28)[0]
stride,count=struct.unpack_from('<HH',native,0x3a)
targets={0x9cff2f0+16+0x730:0x6214c24,0x9d1fa20+16+0x778:0x62b6034}
relocations=[]
metadata_relocations=[]
for i in range(count):
    section=struct.unpack_from('<IIQQQQIIQQ',native,sections+i*stride)
    if section[1]!=4:
        continue
    for offset in range(section[4],section[4]+section[5],section[9]):
        entry=struct.unpack_from('<QQq',native,offset)
        if entry[0] in targets:
            relocations.append(entry)
        if entry[0] in (0x9dadf00,0x9d9e2c0):
            metadata_relocations.append(entry)
assert sorted(relocations)==sorted((address,0x403,value) for address,value in targets.items())
assert sorted(metadata_relocations)==[(0x9d9e2c0,0x403,0x69e17e8),(0x9dadf00,0x403,0x6415990)]
assert struct.unpack_from('<I',native,0x9daded8+0x24)[0]==0xd38
assert struct.unpack_from('<I',native,0x9d9e298+0x24)[0]==0x300
print('PASS: 36 instruction pairs, two exact32 metadata fields, 15 complete native hashes, actual virtuals and adjacent live RELA proof',flush=True)

main_source = (root / 'analysis/re4fix.c').read_text(encoding='utf8')
install = main_source[main_source.index('    g_tgt_touch = base + 0x6a169ac + 16;'):]
assert install.index('    f_GetCurrentGrip =') < install.index('    install_touch_aim_diag(base);')
q_rot = main_source[main_source.index('static qf_t q_mul('):main_source.index('static float g_eye_height')]
reload_source=(root/'analysis/re4fix_reload.h').read_text(encoding='utf8')
for signature in ('static int reload_is_a(','static int reload_layout('):
    start=reload_source.index(signature)
    q_rot+='\n'+reload_source[start:reload_source.index('\n}',start)+2]+'\n'
source = r'''
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
typedef struct { float x,y,z,w; } qf_t;
struct __attribute__((aligned(16))) reload_tf { qf_t q; float p[3],s[3],padding[2]; };
_Static_assert(sizeof(struct reload_tf)==48 && _Alignof(struct reload_tf)==16 && offsetof(struct reload_tf,p)==16 && offsetof(struct reload_tf,s)==28 && offsetof(struct reload_tf,padding)==40,"native scalar FTransform ABI");
Q_ROT
static int logs,grab_logs,grip_logs,tf_calls,pistol=1,g_reload_phase,ammo_logs,ammo_calls,ammo_seen;
static int paused,valid_player=1,interactive=2,registered=1;
static int fire_calls,fire_logs;
static int gate_calls,gate_log_count,gate_hand_class_calls;
static int pistol_gate_calls,expect_pistol_ready=-1,expect_cocked=-1,slide_registered=1;
static volatile int g_aim_diag_request;
static float muzzle_yaw,muzzle_pitch;
static void probe_log(const char* fmt,...) {
    if(!strncmp(fmt,"AIMGATES gun=",13)) {
        gate_log_count++;va_list ap;va_start(ap,fmt);
        assert(va_arg(ap,void*) && va_arg(ap,void*));
        assert(va_arg(ap,int)==1 && va_arg(ap,int)==1 && va_arg(ap,int)==1 && va_arg(ap,unsigned)==1);
        assert(fabs(va_arg(ap,double)-.25)<.000001 && fabs(va_arg(ap,double)-.125)<.000001);
        assert(!va_arg(ap,int) && !va_arg(ap,int) && !va_arg(ap,int));
        assert(va_arg(ap,int)==expect_pistol_ready && va_arg(ap,int)==expect_cocked);
        va_end(ap);return;
    }
    if(!strncmp(fmt,"AIMFIRE ",8)) {fire_logs++;return;}
    if(!strncmp(fmt,"AIMGRAB ",8)) {grab_logs++;return;}
    if(!strncmp(fmt,"AIMGRIP ",8)) {grip_logs++;return;}
    if(!strncmp(fmt,"AIMAMMO ",8)) {
        ammo_logs++;va_list ap;va_start(ap,fmt);(void)va_arg(ap,void*);
        assert(va_arg(ap,int)==7 && va_arg(ap,int)==9 && va_arg(ap,int)==10 && va_arg(ap,int)==90);
        va_end(ap);return;
    }
    logs++;
    if(strncmp(fmt,"AIM %s p=",9)) return;
    va_list ap; va_start(ap,fmt);
    const char* name=va_arg(ap,const char*);
    for(int i=0;i<16;i++) (void)va_arg(ap,double);
    double yaw=va_arg(ap,double),pitch=va_arg(ap,double);
    if(!strcmp(name,"muzzle")) { muzzle_yaw=(float)yaw; muzzle_pitch=(float)pitch; }
    va_end(ap);
}
#define LOGF(...) probe_log(__VA_ARGS__)
static unsigned char pawn[0x300],other_pawn[0x300],hand[0x520],component[0x200],gun[0xde0],hand_cls[0x60];
static unsigned char bio4_hand_cls[0x60];
static const void* hand_chain[2];
static unsigned char slide[0x460],slide_cls[0x60],grip_base_cls[0x60];
static const void* slide_chain[2];
static const void* gun_vtable[0x780/sizeof(void*)];
static unsigned char gun_cls[0x60],holster[0x350],holster_cls[0x60],grip[0x30],holster_grip[0x30];
static void* owner=pawn,*active=pawn,*primary=hand,*weapon=gun;
static unsigned active_type=1;
static struct reload_tf camera={{0,0,0,1},{10,20,30},{1,1,1},{0,0}};
static struct reload_tf sight={{0,0,0,1},{20,20,30},{1,1,1},{0,0}};
static struct reload_tf muzzle={{0,0,0,1},{30,20,30},{1,1,1},{0,0}};
static void* player(const void* p) {(void)p;return owner;}
static void* current(void) {return active;}
static unsigned type(void) {return active_type;}
static void* gun1(const void* p) {(void)p;return weapon;}
static void* primary_hand(const void* p) {(void)p;return primary;}
static void* current_grip(const void* p) {(void)p;return primary==hand ? grip : 0;}
static void* get_holster(const void* p) {(void)p;return holster;}
static int interactivity(const void* p) {(void)p;return interactive;}
static int reload_grip_registered(void* p) {return registered && (p==grip || p==holster_grip || (p==slide && slide_registered));}
static int player_valid(void) {return valid_player;}
static int is_paused(void) {return paused;}
static int ammo_current(const void* p,int include_ejected) {assert(p==gun && (include_ejected==0 || include_ejected==1));ammo_calls++;ammo_seen|=1<<include_ejected;return include_ejected?9:7;}
static int ammo_capacity(const void* p) {assert(p==gun);ammo_calls++;return 10;}
static int ammo_reserves(const void* p) {assert(p==gun);ammo_calls++;return 90;}
static void obj_name(const void* p,char* out,int n) {if(n) snprintf(out,(size_t)n,"%s",p?"Mock":"");}
static int reload_is_pistol(void* p) {(void)p;return pistol;}
static void* pistol_class(void) {return gun;}
static void* grip_class(void) {return grip_base_cls;}
static void* (*rl_grip_class)(void)=grip_class;
static void* (*body_player)(const void*)=player;
static void* (*body_active)(void)=current;
static unsigned (*body_type)(void)=type;
static void* (*rl_gun1)(const void*)=gun1;
static void* (*rl_primary)(const void*)=primary_hand;
static void* (*rl_pistol_class)(void)=pistol_class;
static int (*rl_interactivity)(const void*)=interactivity;
static int (*rl_player_valid)(void)=player_valid;
static int (*rl_paused)(void)=is_paused;
static void* (*f_GetCurrentGrip)(const void*)=current_grip;
static struct reload_tf get_head(const void* p) {(void)p;tf_calls++;return camera;}
static struct reload_tf get_sights(const void* p) {(void)p;tf_calls++;return sight;}
static struct reload_tf get_muzzle(const void* p) {(void)p;tf_calls++;return muzzle;}
#include "AIM_HEADER"
static struct aim_fire_tf get_fire(const void* p) {
    assert(p==gun);fire_calls++;struct aim_fire_tf result={0};
    result.tf=muzzle;result.tf.q=(qf_t){0,0,-0.70710678f,0.70710678f};result.scale_type=1;return result;
}
static int gate_true(const void* p) {assert(p==gun);gate_calls++;return 1;}
static int gate_false(const void* p) {assert(p==gun);gate_calls++;return 0;}
static int gate_debug(void) {gate_calls++;return 0;}
static void* gate_hand_class(void) {gate_hand_class_calls++;return bio4_hand_cls;}
static int gate_pistol(const void* p) {
    assert(p==gun && *(void* const*)((const char*)p+0xd38)==slide);
    assert(*(void**)(slide+0x20)==gun && *(void**)(slide+0x300)==gun);
    assert(slide_registered && reload_layout(slide,0x460) && reload_is_a(slide,grip_base_cls));
    pistol_gate_calls++;return ((const unsigned char*)p)[0xdd5]!=0;
}
int main(void) {
    void* c=component; struct reload_tf h={{0,0,0,1},{12,23,34},{1,1,1},{0,0}};
    void* gp=grip;void* hg=holster_grip;void* gc=gun_cls;void* hc=holster_cls;void* g=gun;void* pp=pawn;
    int gs=(int)sizeof gun,hs=(int)sizeof holster;
    memcpy(gun+0x10,&gc,sizeof gc);memcpy(gun_cls+0x58,&gs,sizeof gs);
    memcpy(holster+0x10,&hc,sizeof hc);memcpy(holster_cls+0x58,&hs,sizeof hs);
    memcpy(gun+0x270,&gp,sizeof gp);memcpy(gun+0x3c0,&pp,sizeof pp);
    memcpy(holster+0x248,&hg,sizeof hg);memcpy(holster+0x348,&pp,sizeof pp);
    memcpy(grip+0x20,&g,sizeof g);memcpy(holster_grip+0x20,&g,sizeof g);
    memcpy(hand+0x130,&c,sizeof c);memcpy(pawn+0x2d8,&c,sizeof c);memcpy(gun+0x540,&c,sizeof c);
    memcpy(component+0x1c0,&h,sizeof h);
    aim_head=get_head;aim_sights=get_sights;aim_muzzle=get_muzzle;
    aim_prop_interactivity=interactivity;aim_holster=get_holster;
    hand[0x314]=1;g_aim_diag_request=1;aim_holster=0;touch_aim_diag(hand);
    assert(g_aim_diag_request==1 && !logs && !tf_calls && !grab_logs);
    aim_holster=get_holster;hand[0x314]=0;g_aim_diag_request=0;
    touch_aim_diag(hand);assert(!logs && !tf_calls);
    g_aim_diag_request=1;touch_aim_diag(0);touch_aim_diag(hand);assert(g_aim_diag_request==1 && !logs);
    hand[0x314]=1;active=other_pawn;touch_aim_diag(hand);assert(g_aim_diag_request==1 && !logs);active=pawn;
    active_type=5;touch_aim_diag(hand);assert(!g_aim_diag_request && logs==1 && !tf_calls);active_type=1;
    pistol=0;g_aim_diag_request=1;touch_aim_diag(hand);assert(!g_aim_diag_request && logs==2 && !tf_calls);pistol=1;
    primary=0;g_aim_diag_request=1;touch_aim_diag(hand);assert(!g_aim_diag_request && logs==3 && !tf_calls);primary=hand;
    unsigned char before_p[sizeof pawn],before_h[sizeof hand],before_c[sizeof component],before_g[sizeof gun];
    memcpy(before_p,pawn,sizeof pawn);memcpy(before_h,hand,sizeof hand);memcpy(before_c,component,sizeof component);memcpy(before_g,gun,sizeof gun);
    g_aim_diag_request=1;touch_aim_diag(hand);assert(!g_aim_diag_request && logs==8 && tf_calls==3);
    assert(fabsf(muzzle_yaw)<0.0001f && fabsf(muzzle_pitch)<0.0001f);
    assert(!memcmp(before_p,pawn,sizeof pawn) && !memcmp(before_h,hand,sizeof hand) && !memcmp(before_c,component,sizeof component) && !memcmp(before_g,gun,sizeof gun));
    touch_aim_diag(hand);assert(logs==8 && tf_calls==3);
    camera.q=(qf_t){0,0,0.70710678f,0.70710678f};g_aim_diag_request=1;touch_aim_diag(hand);
    assert(logs==13 && tf_calls==6 && fabsf(muzzle_yaw+90)<0.001f && fabsf(muzzle_pitch)<0.001f);
    muzzle.q=(qf_t){0,0,0,0};g_aim_diag_request=1;touch_aim_diag(hand);assert(logs==18 && tf_calls==9);
    assert(grab_logs==5 && grip_logs==15);
    weapon=0;g_aim_diag_request=1;touch_aim_diag(hand);assert(logs==19 && tf_calls==9 && grab_logs==6 && grip_logs==18);
    weapon=gun;memset(gun+0x270,0,sizeof(void*));g_aim_diag_request=1;touch_aim_diag(hand);
    assert(logs==20 && tf_calls==9 && grab_logs==7 && grip_logs==21);
    memcpy(gun+0x270,&gp,sizeof gp);muzzle.q=(qf_t){0,0,0,1};
    aim_ammo_current=ammo_current;aim_ammo_capacity=ammo_capacity;aim_ammo_reserves=ammo_reserves;
    g_aim_diag_request=1;touch_aim_diag(hand);assert(ammo_calls==4 && ammo_logs==1 && ammo_seen==3);
    touch_aim_diag(hand);assert(ammo_calls==4 && ammo_logs==1);
    int small=0xd2f;memcpy(gun_cls+0x58,&small,sizeof small);g_aim_diag_request=1;touch_aim_diag(hand);
    memcpy(gun_cls+0x58,&gs,sizeof gs);assert(ammo_calls==4);
    valid_player=0;g_aim_diag_request=1;touch_aim_diag(hand);valid_player=1;
    paused=1;g_aim_diag_request=1;touch_aim_diag(hand);paused=0;
    interactive=1;g_aim_diag_request=1;touch_aim_diag(hand);interactive=2;
    registered=0;g_aim_diag_request=1;touch_aim_diag(hand);registered=1;
    memset(gun+0x3c0,0,sizeof(void*));g_aim_diag_request=1;touch_aim_diag(hand);memcpy(gun+0x3c0,&pp,sizeof pp);
    memset(grip+0x20,0,sizeof(void*));g_aim_diag_request=1;touch_aim_diag(hand);memcpy(grip+0x20,&g,sizeof g);
    primary=0;g_aim_diag_request=1;touch_aim_diag(hand);primary=hand;assert(ammo_calls==4 && ammo_logs==1);
    memcpy(before_p,pawn,sizeof pawn);memcpy(before_h,hand,sizeof hand);memcpy(before_c,component,sizeof component);memcpy(before_g,gun,sizeof gun);
    g_aim_diag_request=1;touch_aim_diag(hand);assert(ammo_calls==8 && ammo_logs==2);
    assert(!memcmp(before_p,pawn,sizeof pawn) && !memcmp(before_h,hand,sizeof hand) && !memcmp(before_c,component,sizeof component) && !memcmp(before_g,gun,sizeof gun));
    void* hand_class=hand_cls;int hand_size=(int)sizeof hand;
    memcpy(hand+0x10,&hand_class,sizeof hand_class);memcpy(hand_cls+0x58,&hand_size,sizeof hand_size);aim_fire=get_fire;
    memcpy(before_p,pawn,sizeof pawn);memcpy(before_h,hand,sizeof hand);memcpy(before_c,component,sizeof component);memcpy(before_g,gun,sizeof gun);
    g_aim_diag_request=1;touch_aim_diag(hand);assert(fire_calls==1 && fire_logs==1);
    touch_aim_diag(hand);assert(fire_calls==1 && fire_logs==1);
    assert(!memcmp(before_p,pawn,sizeof pawn) && !memcmp(before_h,hand,sizeof hand) && !memcmp(before_c,component,sizeof component) && !memcmp(before_g,gun,sizeof gun));
    small=0x51b;memcpy(hand_cls+0x58,&small,sizeof small);g_aim_diag_request=1;touch_aim_diag(hand);assert(fire_calls==1);
    memcpy(hand_cls+0x58,&hand_size,sizeof hand_size);
    registered=0;g_aim_diag_request=1;touch_aim_diag(hand);registered=1;
    memset(grip+0x20,0,sizeof(void*));g_aim_diag_request=1;touch_aim_diag(hand);memcpy(grip+0x20,&g,sizeof g);
    paused=1;g_aim_diag_request=1;touch_aim_diag(hand);paused=0;assert(fire_calls==1);
    int depth=1;const void* chain=hand_chain;
    memcpy(hand_cls+0x30,&chain,sizeof chain);memcpy(hand_cls+0x38,&depth,sizeof depth);
    memcpy(bio4_hand_cls+0x38,&depth,sizeof depth);hand_chain[1]=bio4_hand_cls+0x30;
    float raise=.25f,timer=.125f;hand[0x33c]=1;
    memcpy(hand+0x51c,&raise,sizeof raise);memcpy(gun+0xc58,&timer,sizeof timer);
    aim_gate_trigger=gate_true;aim_gate_blocked=gate_true;aim_gate_ready=gate_true;
    aim_gate_blending=gate_false;aim_gate_flourishing=gate_false;aim_gate_debug=gate_debug;
    aim_gate_hand_class=gate_hand_class;
    void* slide_pointer=slide,*slide_class=slide_cls;const void* slide_class_chain=slide_chain,*gun_table=gun_vtable;
    int slide_size=(int)sizeof slide;
    memcpy(slide+0x10,&slide_class,sizeof slide_class);memcpy(slide_cls+0x58,&slide_size,sizeof slide_size);
    memcpy(slide_cls+0x30,&slide_class_chain,sizeof slide_class_chain);memcpy(slide_cls+0x38,&depth,sizeof depth);
    memcpy(grip_base_cls+0x38,&depth,sizeof depth);slide_chain[1]=grip_base_cls+0x30;
    memcpy(slide+0x20,&g,sizeof g);memcpy(slide+0x300,&g,sizeof g);
    memcpy(gun+0xd38,&slide_pointer,sizeof slide_pointer);memcpy(gun,&gun_table,sizeof gun_table);
    aim_gate_pistol_ready=gate_pistol;gun_vtable[0x778/sizeof(void*)]=(const void*)gate_pistol;
    gun[0xdd5]=1;expect_pistol_ready=expect_cocked=1;
    memcpy(before_p,pawn,sizeof pawn);memcpy(before_h,hand,sizeof hand);memcpy(before_c,component,sizeof component);memcpy(before_g,gun,sizeof gun);
    g_aim_diag_request=1;touch_aim_diag(hand);assert(gate_calls==6 && gate_log_count==1 && gate_hand_class_calls==1 && pistol_gate_calls==1);
    touch_aim_diag(hand);assert(gate_calls==6 && gate_log_count==1 && gate_hand_class_calls==1);
    assert(!memcmp(before_p,pawn,sizeof pawn) && !memcmp(before_h,hand,sizeof hand) && !memcmp(before_c,component,sizeof component) && !memcmp(before_g,gun,sizeof gun));
    valid_player=0;g_aim_diag_request=1;touch_aim_diag(hand);valid_player=1;
    paused=1;g_aim_diag_request=1;touch_aim_diag(hand);paused=0;
    interactive=1;g_aim_diag_request=1;touch_aim_diag(hand);interactive=2;
    registered=0;g_aim_diag_request=1;touch_aim_diag(hand);registered=1;
    pistol=0;g_aim_diag_request=1;touch_aim_diag(hand);pistol=1;
    primary=0;g_aim_diag_request=1;touch_aim_diag(hand);primary=hand;
    memset(gun+0x3c0,0,sizeof(void*));g_aim_diag_request=1;touch_aim_diag(hand);memcpy(gun+0x3c0,&pp,sizeof pp);
    memset(grip+0x20,0,sizeof(void*));g_aim_diag_request=1;touch_aim_diag(hand);memcpy(grip+0x20,&g,sizeof g);
    small=0x51f;memcpy(hand_cls+0x58,&small,sizeof small);g_aim_diag_request=1;touch_aim_diag(hand);
    small=0x10001;memcpy(hand_cls+0x58,&small,sizeof small);g_aim_diag_request=1;touch_aim_diag(hand);
    memcpy(hand_cls+0x58,&hand_size,sizeof hand_size);
    hand_chain[1]=0;g_aim_diag_request=1;touch_aim_diag(hand);hand_chain[1]=bio4_hand_cls+0x30;
    aim_gate_ready=0;g_aim_diag_request=1;touch_aim_diag(hand);aim_gate_ready=gate_true;
    assert(gate_calls==6 && gate_log_count==1);
    expect_pistol_ready=expect_cocked=-1;
    memset(gun+0xd38,0,sizeof(void*));g_aim_diag_request=1;touch_aim_diag(hand);memcpy(gun+0xd38,&slide_pointer,sizeof slide_pointer);
    slide_registered=0;g_aim_diag_request=1;touch_aim_diag(hand);slide_registered=1;
    memset(slide+0x20,0,sizeof(void*));g_aim_diag_request=1;touch_aim_diag(hand);memcpy(slide+0x20,&g,sizeof g);
    memset(slide+0x300,0,sizeof(void*));g_aim_diag_request=1;touch_aim_diag(hand);memcpy(slide+0x300,&g,sizeof g);
    small=0x45f;memcpy(slide_cls+0x58,&small,sizeof small);g_aim_diag_request=1;touch_aim_diag(hand);memcpy(slide_cls+0x58,&slide_size,sizeof slide_size);
    slide_chain[1]=0;g_aim_diag_request=1;touch_aim_diag(hand);slide_chain[1]=grip_base_cls+0x30;
    gun_vtable[0x778/sizeof(void*)]=0;g_aim_diag_request=1;touch_aim_diag(hand);gun_vtable[0x778/sizeof(void*)]=(const void*)gate_pistol;
    small=0xddf;memcpy(gun_cls+0x58,&small,sizeof small);g_aim_diag_request=1;touch_aim_diag(hand);memcpy(gun_cls+0x58,&gs,sizeof gs);
    aim_gate_pistol_ready=0;g_aim_diag_request=1;touch_aim_diag(hand);aim_gate_pistol_ready=gate_pistol;
    rl_grip_class=0;g_aim_diag_request=1;touch_aim_diag(hand);rl_grip_class=grip_class;
    assert(pistol_gate_calls==1);
    gun[0xdd5]=0;expect_pistol_ready=expect_cocked=0;g_aim_diag_request=1;touch_aim_diag(hand);assert(pistol_gate_calls==2);
    gun[0xdd5]=1;expect_pistol_ready=expect_cocked=1;
    int count_before_cap=gate_log_count;
    for(int i=0;i<80;i++){g_aim_diag_request=1;touch_aim_diag(hand);}
    assert(gate_calls==32*6 && gate_log_count==32 && aim_gate_logs==32);
    assert(pistol_gate_calls==2+32-count_before_cap);
    assert(!memcmp(before_p,pawn,sizeof pawn) && !memcmp(before_h,hand,sizeof hand) && !memcmp(before_c,component,sizeof component) && !memcmp(before_g,gun,sizeof gun));
    unsigned char* image=(unsigned char*)calloc(1,0x9dadf10UL);assert(image);
    NATIVE_INSTALL_DATA
    uintptr_t image_base=(uintptr_t)image;
    uintptr_t live_slide_class=image_base+0x6415990,live_parent_class=image_base+0x69e17e8;
    memcpy(image+0x9dadf00,&live_slide_class,sizeof live_slide_class);
    memcpy(image+0x9d9e2c0,&live_parent_class,sizeof live_parent_class);
    aim_head=0;aim_gate_pistol_ready=0;install_touch_aim_diag(image_base);
    assert(aim_head==(struct reload_tf (*)(const void*))(image_base+0x6341aec));
    assert(aim_gate_pistol_ready==(int (*)(const void*))(image_base+0x62b6034));
    const uintptr_t metadata_addresses[2]={0x9dadefc,0x9d9e2bc};
    for(int i=0;i<2;i++){
        unsigned value;memcpy(&value,image+metadata_addresses[i],sizeof value);
        unsigned corrupted=value^1;memcpy(image+metadata_addresses[i],&corrupted,sizeof corrupted);
        aim_head=0;aim_gate_pistol_ready=0;install_touch_aim_diag(image_base);
        assert(aim_head==(struct reload_tf (*)(const void*))(image_base+0x6341aec) && !aim_gate_pistol_ready);
        memcpy(image+metadata_addresses[i],&value,sizeof value);
    }
    free(image);
    puts("PASS: actual immutable gate C, owned typed SlideGrip/PistolReady guards and cap32; actual installer accepts relocated adjacent metadata pointers and rejects either corrupt32 offset preserving FPS dependencies");
    return 0;
}
'''.replace('Q_ROT',q_rot).replace('AIM_HEADER',header.as_posix())
seeds=[(int(addr,16),8) for addr,_,_ in checks]+[(address,4) for address,_ in fields]
source=source.replace('NATIVE_INSTALL_DATA','\n'.join(
    '{ const unsigned char bytes[]={'+','.join(str(value) for value in native[address:address+width])+'};'
    +'memcpy(image+'+hex(address)+',bytes,sizeof bytes);}' for address,width in seeds))
clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk = Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-aim-check-') as temporary:
    path=Path(temporary);test=path/'aim.c';test.write_text(source,encoding='utf8')
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(test)],check=True)
    # Preserve Android LP64 address widths in the Windows LLP64 installer fixture.
    host_header=path/'aim_diag_host.h';host_header.write_text(header_text.replace('unsigned long','uintptr_t'),encoding='utf8')
    test.write_text(source.replace(header.as_posix(),host_header.as_posix()),encoding='utf8')
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-c','-o',str(path/'aim.obj'),str(test)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{path / "aim.exe"}',str(path/'aim.obj'),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc / "lib/x64"}',f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(path/'aim.exe')],check=True)
