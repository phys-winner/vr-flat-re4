"""Execute the production camera target query with counted native seams."""
import os
from pathlib import Path
import re, struct, subprocess, tempfile

root = Path(__file__).resolve().parents[2]
header = (root/'analysis/re4fix_hud_extra.h').read_text()
start = header.index('static void touch_aim_assist_tick(')
end = header.index('\n}', start) + 2
function = header[start:end]
start = header.index('static void touch_hud_ammo_tick(')
caller = header[start:header.index('\n}',start)+2]
native = (root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
for address, values in ((0x5ec7190,(0xd10403ff,0x6d0733ed,0x6d082beb,0x6d0923e9)),
                        (0x61cba30,(0x2d400800,0xbd400801)),
                        (0x628c8c4,(0x2a1f03e2,0x97f0ea32))):
    assert struct.unpack_from('<'+'I'*len(values),native,address)==values
main = (root/'analysis/re4fix.c').read_text()
math = main[main.index('static qf_t q_mul('):main.index('static float g_eye_height')]
source = r'''
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
typedef struct {float x,y,z,w;} qf_t;
struct reload_tf {qf_t q;float p[3],s[3];};
MATH
static unsigned char gun[0xd40],pawn[32],enemy[32];
static int enabled=1,paused,valid=1,g_touch_context=1,g_touch_cancel,calls,result=2;
static volatile long g_aim_painted_ms;
static struct reload_tf head={{0,0,0,1},{10,20,30},{1,1,1}};
static int mobile_prefs_assist(void){return enabled;}
static int paused_fn(void){return paused;}static int valid_fn(void){return valid;}
static int (*rl_paused)(void)=paused_fn,(*rl_player_valid)(void)=valid_fn;
static struct reload_tf get_head(const void* p){assert(p==pawn);return head;}
static struct reload_tf (*aim_head)(const void*)=get_head;
static int reload_tf_valid(const struct reload_tf* t){return isfinite(t->p[0])&&isfinite(t->q.w);}
static qf_t pose_camera_delta_ue(void){return(qf_t){0,0,0,1};}
static int query(float* start,float* end,unsigned mode,unsigned weapon,void** out,unsigned* hit){
 assert(mode==0&&weapon==7&&hit);calls++;
 assert(start[0]==100&&start[1]==300&&start[2]==200);
 assert(end[0]==10100&&end[1]==300&&end[2]==200);
 *out=enemy;return result;
}
static int (*hs_aim_query)(float*,float*,unsigned,unsigned,void**,unsigned*)=query;
FUNCTION
static uintptr_t hs_ammo_base=1;static volatile int g_touch_gt_tid=1;
static uint64_t hs_ammo;static long hs_reload_ready_ms;static int equipped_calls;
#define SYS_gettid 0
#define syscall(x) 1
static void* fixture_active=pawn;
static void* player(const void* h){(void)h;return pawn;}
static void* active(void){return fixture_active;}
static void* (*rl_player)(const void*)=player,*(*rl_active)(void)=active;
static void* touch_equipped_gun(void* h){(void)h;equipped_calls++;return 0;}
static int hs_ammo_known(void* f,const unsigned long* a,unsigned n){(void)f;(void)a;(void)n;return 0;}
static int (*hs_em_alive)(const void*);
static int hs_reload_possible(void* h,void* g){(void)h;(void)g;return 0;}
CALLER
int main(void){
 *(float*)(gun+0x60c)=1000;gun[0xd20]=7;*(unsigned*)(enemy+8)=1;
 touch_aim_assist_tick(gun,pawn,1000);assert(calls==1&&g_aim_painted_ms==1000);
 result=1;touch_aim_assist_tick(gun,pawn,1001);assert(calls==2&&g_aim_painted_ms==1000);
 result=2;*(unsigned*)(enemy+8)=0x201;touch_aim_assist_tick(gun,pawn,1002);assert(g_aim_painted_ms==1000);
 enabled=0;touch_aim_assist_tick(gun,pawn,1003);enabled=1;
 paused=1;touch_aim_assist_tick(gun,pawn,1003);paused=0;
 valid=0;touch_aim_assist_tick(gun,pawn,1003);valid=1;
 g_touch_context=2;touch_aim_assist_tick(gun,pawn,1003);g_touch_context=1;
 g_touch_cancel=1;touch_aim_assist_tick(gun,pawn,1003);g_touch_cancel=0;
 *(float*)(gun+0x60c)=NAN;touch_aim_assist_tick(gun,pawn,1003);
 touch_aim_assist_tick(0,pawn,1003);touch_aim_assist_tick(gun,0,1003);assert(calls==3);
 fixture_active=enemy;touch_hud_ammo_tick(pawn,2000);assert(!equipped_calls);
 fixture_active=pawn;touch_hud_ammo_tick(pawn,2000);assert(equipped_calls==1);
 return 0;
}
'''.replace('MATH',math).replace('FUNCTION',function).replace('CALLER',caller)
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='aim-assist-') as temporary:
    work=Path(temporary);test=work/'test.c';test.write_text(source)
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-Werror','-fsyntax-only',str(test)],check=True)
    obj,exe=work/'test.obj',work/'test.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-c',str(test),'-o',str(obj)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: production native target query, camera centre/axis/scale, enemy/occlusion distinction, setting/context/ownership/invalid-data gates')
