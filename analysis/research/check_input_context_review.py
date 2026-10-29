"""Run actual test_touch.c/header with a deterministic serial host clock.

No APK, device or production file is changed. The host mutex and sleep shims
exercise input-state transitions without adding real threaded scheduling claims.
"""
import os
from pathlib import Path
import hashlib
import math
import re
import struct
import subprocess
import sys
import tempfile

root=Path(__file__).resolve().parents[2]
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
test=root/'analysis/test_touch.c'
main_source=(root/'analysis/re4fix.c').read_text(encoding='utf8')
phone_source=(root/'analysis/re4fix_phone.h').read_text(encoding='utf8')
input_source=(root/'analysis/re4fix_input.h').read_text(encoding='utf8')
def extract_function(text,signature):
    match=re.search(re.escape(signature)+r'\s*\{',text)
    assert match,signature
    start=match.start();end=text.index('{',start)+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
menu_owner=extract_function(phone_source,'static int touch_menu_active(void* hand)')
menu_hook=extract_function(main_source,'void hook_mhtick(void* self, float dt)')
game_hook=extract_function(main_source,'void hook_ghtick(void* self, float dt)')
pointer=extract_function(main_source,'static int ptr_active(void)')
grenade_pointer=extract_function(main_source,'static int touch_menu_pointer_allowed(int node)')
grenade_side=extract_function(main_source,'static int touch_grenade_selected_side(long now)')
pose=extract_function(main_source,'int h5_ovrp_GetNodePoseState3(long step, long frame, long node, long out)')
assert 'touch_menu_pointer_allowed((int)node)' in pose
assert pose.index('touch_qte_pose(hi,ptr_now_ms(),off);')<pose.index('touch_grenade_pose(hi,ptr_now_ms(),off)')<pose.index('q_rot(position_q, off, w)')
assert 'hi==touch_grenade_selected_side(ptr_now_ms())' in pose
grenade_rest=re.search(r'if\(g_touch_context==1 && hi==touch_grenade_selected_side\(ptr_now_ms\(\)\)\)\s*\{([^{}]*)\}',pose)
assert grenade_rest, 'Grenade rest pose must retain game-context and selected-side guards'
assert 'float aspect=g_comp_w && g_comp_h ? (float)g_comp_w/(float)g_comp_h:2400.0f/1080.0f;' in grenade_rest[1]
assert 'grenade_held_pose(hi,g_fov_h_deg,aspect,off);' in grenade_rest[1]
assert game_hook.index('touch_grenade_cancel_tick(')<game_hook.index('gate_ghtick(self, dt);')<game_hook.index('touch_grenade_tick(')
assert game_hook.index('touch_grenade_cancel_tick(')<game_hook.index('touch_pause_tick(self);')
assert 'if(!touch_grenade_owned_grip_mask())touch_pause_tick(self);' in game_hook
assert 'if(!touch_grenade_owned_grip_mask())touch_inventory_tick(self);' in game_hook
assert 'touch_grenade_cancel_tick(0,0,ptr_now_ms());' in menu_hook
assert main_source.index('install_touch_quickselect(base);')<main_source.index('install_touch_grenade(base);')
assert '#include "re4fix_grenade.h"' in main_source and 'touch_grenade_busy(now)' in main_source
assert menu_hook.index('touch_phone_tick(self)')<menu_hook.index('if (touch_menu_active(self))')
assert menu_hook.count('g_menu_tick_ms =')==1 and menu_hook.count('g_menu_ticks++')==1
assert re.search(r'if \(touch_menu_active\(self\)\)\s*\{[^}]*g_menu_tick_ms =[^}]*g_menu_ticks\+\+',menu_hook)
assert main_source.index('install_touch_phone(base);')<main_source.index('patch_code((void*)(base + 0x6310630)')
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
for addr,words in ((0x6310630,(0xf81e0ff3,0xa9017bfd,0x910043fd,0xaa0003f3)),
                   (0x633d030,(0xf9417800,0xd65f03c0)),(0x63380dc,(0xd0020168,0xf9431508))):
    assert struct.unpack_from('<'+'I'*len(words),native,addr)==words,hex(addr)
print('PASS: original menu Tick/player/active native guards, owner-only timestamp block and install ordering',flush=True)
# UObjectBase constructor passes its 8-byte FName value from x5 to AddObject x1.
# AddObject performs an actual STR x1 at object+18, not a guessed layout field.
for addr,words in ((0x69afecc,(0xaa0503e1,0x2a0303e2)),
                   (0x69afedc,(0x14000001,)),
                   (0x69afee0,(0xa9be4ff4,0xa9017bfd,0x910043fd,0xf9000c01))):
    assert struct.unpack_from('<'+'I'*len(words),native,addr)==words,hex(addr)
new_pawn=extract_function(input_source,'static void touch_new_game_pawn(void* pawn)')
assert 'if (!pawn) return;' in new_pawn
assert 'memcpy(&name, (const char*)pawn+0x18, sizeof name);' in new_pawn
assert 'pawn != g_touch_last_game_pawn || name != g_touch_last_game_name' in new_pawn
assert new_pawn.count('touch_reset();')==1
assert new_pawn.index('touch_reset();')<new_pawn.rindex('g_touch_last_game_pawn = pawn;')
assert 'g_touch_last_game_name = name;' in new_pawn
assert 'f_FName_ToString((const char*)obj + 0x18, &fs);' in main_source
print('PASS: native UObject full8-byte FName+18 constructor/store proof and actual new-game-pawn identity/reset ordering',flush=True)
type_body=native[0x6338aec:0x6338b5c]
assert hashlib.sha256(type_body).hexdigest()=='cb490b801cb285821cfac6dfad99d6a4878ab8d92a8460df540a5fb625d0b042'
assert struct.unpack_from('<IIII',type_body)==(0xd0020168,0xf9431508,0xf9400108,0xb40002e8)
assert struct.unpack_from('<IIII',native,0x6338b4c)==(0x39578100,0xd65f03c0,0x2a1f03e0,0xd65f03c0)
assert not any(word[0]&0xfc000000==0x94000000 or word[0]&0xfffffc1f==0xd63f0000
               for word in struct.iter_unpack('<I',type_body)) # leaf, no BL/BLR/mutex/render call
refresh=extract_function(input_source,'static void touch_refresh_context_gt(void)')
assert 'gt > 0 && gettid() == gt && phone_type' in refresh
assert refresh.count('phone_type()')==1
assert '__atomic_load_n(&g_touch_gt_tid, __ATOMIC_ACQUIRE)' in refresh
fill=extract_function(input_source,'void fill_controller_state(unsigned char* p, unsigned mask)')
reset=extract_function(input_source,'static void touch_reset(void)')
assert reset.index('__atomic_store_n(&g_touch_cancel,3,__ATOMIC_RELEASE);')<reset.index('touch_grenade_input_reset()')<reset.index('pthread_mutex_lock(&g_imx);')
assert 'if(!(grenade_preserve&(1u<<bit)))' in reset
assert 'touch_grenade_fire_up(now_ms());' in reset
assert 'btn&=~menu_excluded;' in fill
assert '(menu_excluded & OB_LHAND) ? 0' in fill and '(menu_excluded & OB_RHAND) ? 0' in fill
assert fill.index('pthread_mutex_lock(&g_imx);')<fill.index('touch_refresh_context_gt();')<fill.index('recompute();')
assert fill.index('recompute();')<fill.index('g_menu_pointer.down && g_menu_pointer.action==MP_TRIGGER && nm >= g_menu_pointer.press_ms')
assert 'phone_type=(unsigned (*)(void))(base+0x6338aec);' in phone_source
mark='__atomic_store_n(&g_touch_gt_tid,(int)syscall(SYS_gettid),__ATOMIC_RELEASE);'
assert menu_hook.count(mark)==1 and game_hook.count(mark)==1
assert menu_hook.index(mark)<menu_hook.index('gate_mhtick(self, dt);')
assert game_hook.index(mark)<game_hook.index('gate_ghtick(self, dt);')
assert re.search(r'void hook_[mg]htick\([^\n]*\)\s*\{\s*'+re.escape(mark),menu_hook)
assert re.search(r'void hook_[mg]htick\([^\n]*\)\s*\{\s*'+re.escape(mark),game_hook)
assert 'void hook_touch(const void* arr){ g_tq_calls++; touch_process(arr); }' in input_source
print('PASS: complete native active-type leaf body/preimages, recorded-GT-only getter and context refresh before controller payload',flush=True)
print('PASS: root RELEASE entry marks in both actual hooks; consume-only touch hook unchanged',flush=True)
controller=extract_function(input_source,'void fill_controller_state(unsigned char* p, unsigned mask)')
assert re.search(r'selected\.selected==QS_HELD_WORLD_GUN\s*\?\s*2\s*:\s*3',controller)
overlay=extract_function(input_source,'int touch_overlay_quads(struct ovl_quad* q, int max)')
assert 'touch_hud_ammo_read(' not in overlay and 'OVL_DIGIT' not in overlay
assert 'else if (g_anim_kind != 2) g_anim_kind = 0;' in pose
print('PASS: production controller maps real held world gun to gun mode; no ammo HUD draw/read, full knife cycle survives finger release',flush=True)
subprocess.run([str(clang),'--target=aarch64-linux-android23','-std=c11','-D_GNU_SOURCE',
    '-O2','-Werror','-fsyntax-only',str(test)],check=True)
print('PASS: actual input/header test Android aarch64 syntax',flush=True)

host_shims=r'''
#include <time.h>
#define RE4_INPUT_DETERMINISTIC_CLOCK 1
typedef int review_mutex_t;
#define pthread_mutex_t review_mutex_t
#define PTHREAD_MUTEX_INITIALIZER 0
static int pthread_mutex_lock(pthread_mutex_t* mutex) { (void)mutex;return 0; }
static int pthread_mutex_unlock(pthread_mutex_t* mutex) { (void)mutex;return 0; }
static long review_clock_ms=10000;
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif
static int review_clock_gettime(int clock,struct timespec* stamp) {
    (void)clock;stamp->tv_sec=review_clock_ms/1000;
    stamp->tv_nsec=(review_clock_ms%1000)*1000000;return 0;
}
#define clock_gettime review_clock_gettime
static int usleep(unsigned long microseconds) { review_clock_ms+=(long)(microseconds/1000);return 0; }
'''
source=test.read_text(encoding='utf8').replace('#include <pthread.h>',host_shims).replace('#include <unistd.h>','')
ownership_harness=host_shims+r'''
#include <assert.h>
#include <stdio.h>
static int menu_hand,pooled_hand,pawn_menu,pawn_old,desired_context=7;
static void* player_result=&pawn_menu;
static void* active_result=&pawn_menu;
static int player_reads,active_reads,phone_ticks,gates,visual_ticks;
static void* player(const void* hand) {assert(hand);player_reads++;return hand==&pooled_hand ? &pawn_old : player_result;}
static void* active(void) {active_reads++;return active_result;}
static void* (*phone_player)(const void*);
static void* (*phone_active)(void);
static long g_menu_tick_ms=77,g_menu_ticks,g_ptr_until_ms;
static int g_touch_context,g_touch_weapon=2,g_ptr_down=1;
static unsigned review_grenade_mask;
static unsigned touch_grenade_owned_grip_mask(void) {return review_grenade_mask;}
static void touch_grenade_cancel(void) {}
static void touch_grenade_cancel_tick(void* hand,int side,long now){assert(!hand && side==0 && now>0);}
struct touch_quickselect_state {unsigned available;int selected,fire_side,mode;};
static struct touch_quickselect_state review_qs={0,-1,-1,0};
static struct touch_quickselect_state touch_quickselect_state(long now){(void)now;return review_qs;}
static volatile int g_touch_gt_tid;
#define SYS_gettid 178
static long syscall(long number) {assert(number==SYS_gettid);return 41;}
static unsigned long g_ue4_base_cached=0x11223344;
static long ptr_now_ms(void) {return review_clock_ms;}
static void touch_tracking_tick(void) {}
static void apply_touch_graphics(unsigned long base) {assert(base==g_ue4_base_cached);}
static void touch_phone_tick(void* hand) {assert(hand && g_touch_gt_tid==41);phone_ticks++;g_touch_context=desired_context;}
static void touch_heal_progress_tick(void* hand,long now) {(void)hand;(void)now;}
static void touch_pause_tick(void* hand) {assert(hand);}
static void touch_unpause_tick(void* hand) {assert(hand);}
static void touch_mobile_settings_tick(void) {}
static void touch_flat_controls_tick(void) {}
static void touch_mobile_android_tick(void) {}
static void touch_mobile_extras_tick(long now) {(void)now;}
static void touch_extras_save_tick(long now) {(void)now;}
static void touch_prompt_visual_tick(void) {}
static long g_gt_alive_ms;
static void touch_menu_view_tick(long now) {(void)now;}
static void touch_ui_diag_tick(void) {}
static void touch_inspect_tick(void) {}
static void touch_ui_targets_tick(long now) {(void)now;}
static int g_loading_title_wait;
static unsigned loading_phone_type(void) {return (unsigned)g_touch_context;}
static unsigned (*phone_type)(void)=loading_phone_type;
static int fv_title_can_continue(long now,unsigned type) {(void)now;(void)type;return 0;}
static void loading_title_ready(void) {assert(0);}
static void touch_comfort_wizard_tick(long now) {(void)now;}
static void touch_mobile_menu_tick(void) {}
static void touch_sce_idle_tick(void) {}
static void touch_throw_idle_tick(void) {}
static void touch_mirror_idle_tick(void) {}
static void touch_map_zoom_tick(long now) {(void)now;}
static void gate_mhtick(void* hand,float dt) {assert(hand && dt==.01f);gates++;}
static void touch_menu_grip_tick(void* hand) {assert(hand && gates==visual_ticks+1);}
static void touch_menu_visual_tick(void* hand) {assert(hand && gates==visual_ticks+1);visual_ticks++;}
'''+menu_owner+'\n'+pointer+'\n'+grenade_pointer+'\n'+grenade_side+'\n'+menu_hook+r'''
int main(void) {
    assert(!touch_menu_active(&menu_hand));
    phone_player=player;assert(!touch_menu_active(&menu_hand));phone_active=active;
    assert(!touch_menu_active(0) && !player_reads && !active_reads);
    player_result=active_result=0;assert(!touch_menu_active(&menu_hand));
    assert(g_menu_tick_ms==77 && !g_menu_ticks);
    player_result=&pawn_menu;active_result=&pawn_old;assert(!touch_menu_active(&menu_hand));
    hook_mhtick(&menu_hand,.01f);assert(g_menu_tick_ms==77 && !g_menu_ticks && gates==1);
    active_result=&pawn_menu;assert(touch_menu_active(&menu_hand));
    g_touch_context=0;hook_mhtick(&menu_hand,.01f);
    assert(g_touch_context==7 && g_menu_tick_ms==review_clock_ms && g_menu_ticks==1 && gates==2);
    assert(ptr_active()); // Original initial menu still gets a pointer.
    assert(touch_menu_pointer_allowed(4) && !touch_menu_pointer_allowed(3));
    review_grenade_mask=0x20000000u;assert(touch_menu_pointer_allowed(4));
    review_grenade_mask=0x08000000u;assert(!touch_menu_pointer_allowed(4));
    review_grenade_mask=0;assert(touch_menu_pointer_allowed(4));
    review_qs.selected=2;review_qs.fire_side=0;assert(touch_grenade_selected_side(review_clock_ms)==0);
    review_qs.fire_side=1;assert(touch_grenade_selected_side(review_clock_ms)==1);
    review_qs.selected=0;assert(touch_grenade_selected_side(review_clock_ms)==-1);
    long owned_time=g_menu_tick_ms;review_clock_ms+=500;desired_context=1;
    hook_mhtick(&pooled_hand,.01f);
    assert(g_touch_context==1 && g_menu_tick_ms==owned_time && g_menu_ticks==1 && gates==3);
    assert(!ptr_active());
    g_menu_tick_ms=review_clock_ms;assert(ptr_active()); // An in-world menu ticking during gameplay owns the pointer.
    review_clock_ms+=301;assert(!ptr_active());g_menu_tick_ms=review_clock_ms;
    g_touch_context=3;assert(!ptr_active());
    g_touch_context=5;assert(ptr_active());
    g_touch_context=2;assert(ptr_active());
    review_clock_ms+=301;assert(!ptr_active());
    assert(phone_ticks==3 && visual_ticks==gates && visual_ticks==3);
    puts("PASS: actual owner/helper/hook C; null and pooled hands, initial menu, native passthrough, timestamp ownership and context-authoritative pointer");
    return 0;
}
'''
pose_source=(root/'analysis/re4fix_weapon_pose.h').read_text(encoding='utf8')
knife_times=re.search(r'static int g_knife_t\[4\] = \{[^}]+\};',pose_source).group(0)
hold_harness=r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
static volatile long g_watch_since_ms,g_watch_until_ms,g_anim_t0,g_anim_len;
static volatile int g_touch_context=1,g_anim_kind;
static int g_watch_held,swings;
static long g_knife_swing_until;
#define LOGF(...) (++swings)
'''+knife_times+'\n'+extract_function(pose_source,'static long knife_slash_length(void)')+'\n' \
    +extract_function(input_source,'static void touch_knife_swing(long now)')+'\n' \
    +extract_function(input_source,'static void touch_watch_hold(int held, long now)')+'\n' \
    +extract_function(main_source,'static float watch_pose_weight(long now)')+r'''
int main(void){
    long start=1000,length=knife_slash_length();assert(length>150);
    touch_knife_swing(start);assert(swings==1 && g_anim_kind==2 && g_anim_t0==start && g_anim_len==length);
    for(long now=start+1;now<start+length;now++){
        touch_knife_swing(now);assert(swings==1 && g_anim_t0==start && g_knife_swing_until==start+length);
    }
    touch_knife_swing(start+length-1);assert(swings==1);
    touch_knife_swing(start+length);assert(swings==2 && g_anim_t0==start+length);
    touch_knife_swing(start+length);assert(swings==2);
    touch_knife_swing(start+2*length);assert(swings==3 && g_anim_t0==start+2*length);
    // A single tap releases its input, not the animation deadline; no further
    // swing request occurs when its minimum Fire press has expired.
    long completed=g_knife_swing_until;assert(completed==start+3*length);
    assert(g_anim_t0==start+2*length && swings==3);
    assert(!watch_pose_weight(1000));touch_watch_hold(1,1000);
    assert(g_watch_held && g_watch_since_ms==1000 && g_watch_until_ms==1500 && !watch_pose_weight(999) && !watch_pose_weight(1000));
    assert(fabsf(watch_pose_weight(1110)-.5f)<.00001f && watch_pose_weight(1220)==1);
    for(long now=1001;now<=5000;now++){
        touch_watch_hold(1,now);assert(g_watch_since_ms==1000 && g_watch_until_ms==now+500);
        if(now>=1220)assert(watch_pose_weight(now)==1);
    }
    touch_watch_hold(0,5000);assert(!g_watch_held && g_watch_until_ms==5000 && watch_pose_weight(5000)==1);
    assert(watch_pose_weight(5001)<1 && fabsf(watch_pose_weight(5110)-.5f)<.00001f && !watch_pose_weight(5220));
    float previous=1;
    for(long now=5000;now<=5300;now++){float w=watch_pose_weight(now);assert(w>=0 && w<=previous);previous=w;}
    // Releasing a short hold must start lowering immediately, including while
    // the wrist is still on its way up; it must never rise after release.
    touch_watch_hold(1,6000);touch_watch_hold(0,6055);previous=watch_pose_weight(6055);
    assert(fabsf(previous-.15625f)<.00001f);
    for(long now=6056;now<=6300;now++){float w=watch_pose_weight(now);assert(w>=0 && w<previous || w==0 && previous==0);previous=w;}
    assert(!watch_pose_weight(6275));
    touch_watch_hold(1,7000);assert(g_watch_since_ms==7000 && !watch_pose_weight(7000));
    g_touch_context=0;assert(!watch_pose_weight(7110));g_touch_context=2;assert(!watch_pose_weight(7110));
    g_watch_since_ms=g_watch_until_ms=0;g_watch_held=0;g_touch_context=1;assert(!watch_pose_weight(99999));
    puts("PASS: actual swing helper exact native-length boundaries/non-restart/hold cycles and watch pose 4s hold/220ms fade/short-release monotonic/context/cancel state");
    return 0;
}
'''
if '--probe-menu-history' in sys.argv[1:]:
    probe=r'''
    touch_reset();g_touch_context=1;g_menu_tick_ms=now_ms();fill_controller_state(state,3);
    assert(!menu_mode());
    send_touch(0,4,1400,600);send_touch(2,4,1400,600);
    usleep(130000);fill_controller_state(state,3);
    printf("MENU HISTORY PROBE context=%d menu=%d RT=%.1f pulse=%ld..%ld now=%ld\n",
        g_touch_context,menu_mode(),((float*)state)[5],g_tap_press_ms,g_tap_until_ms,now_ms());
'''
    source=source.replace('    test_context_lifetime();',probe+'\n    test_context_lifetime();')
with tempfile.TemporaryDirectory(prefix='re4-input-context-review-') as temporary:
    path=Path(temporary)
    copied=path/'touch.c';obj=path/'touch.obj';exe=path/'touch.exe'
    copied.write_text(source,encoding='utf8')
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-std=c11','-O2','-Werror',
        '-D_CRT_SECURE_NO_WARNINGS','-DRE4_RELOAD_ARROW_PROOF="'+(path/'arrow.txt').as_posix()+'"',
        '-I',str(root/'analysis'),'-isystem',str(vc/'include'),
        '-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-c','-o',str(obj),str(copied)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc / "lib/x64"}',f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
    # Execute the actual 48-quad output against the shipped atlas pixels, including its alpha.
    assets=(root/'analysis/overlay/overlay_assets.h').read_text(encoding='utf8')
    encoded=re.search(r'ovl_atlas_rle\[\d+\] = \{(.*?)\n\};',assets,re.S)
    assert encoded
    packed=bytes(map(int,re.findall(r'\d+',encoded[1])));pixels=bytearray();offset=0
    while offset<len(packed):
        run,count=struct.unpack_from('<HH',packed,offset);offset+=4
        pixels.extend(bytes(run*4));pixels.extend(packed[offset:offset+count*4]);offset+=count*4
    assert len(pixels)==896*896*4
    def solid_alpha(x,y):return pixels[(((256+y)*896)+(128+x))*4+3]
    assert all(solid_alpha(x,y)==(255 if 4<=x<124 and 4<=y<124 else 0) for y in range(128) for x in range(128))
    records=(path/'arrow.txt').read_text().splitlines();width,height,count=map(float,records[0].split())
    assert count<=48 and len(records)==5
    mask=bytearray(int(width)*int(height))
    area=0
    for record in records[1:]:
        x0,y0,x1,y1,alpha=map(float,record.split())
        box=tuple(round((v+1)*dimension*.5) for v,dimension in ((x0,width),(y0,height),(x1,width),(y1,height)))
        for y in range(box[1],box[3]):
            sy=127*(y+.5-box[1])/(box[3]-box[1]);iy=math.floor(sy);fy=sy-iy
            for x in range(box[0],box[2]):
                sx=127*(x+.5-box[0])/(box[2]-box[0]);ix=math.floor(sx);fx=sx-ix
                a=(solid_alpha(ix,iy)*(1-fx)*(1-fy)+solid_alpha(ix+1,iy)*fx*(1-fy)
                    +solid_alpha(ix,iy+1)*(1-fx)*fy+solid_alpha(ix+1,iy+1)*fx*fy)
                mask[y*int(width)+x]=round(a*alpha)
        area+=(box[2]-box[0])*(box[3]-box[1])
    visible=sum(value>180 for value in mask)
    assert visible>200 and visible>area*.70 and max(mask)>220
    print(f'PASS: actual48-quad arrow samples opaque OVL_SOLID15 atlas; {visible} visible high-contrast pixels, {visible/area:.1%} of its geometry',flush=True)
    # Execute the default device-fixture branch as well. The serial host clock
    # is still controlled, but its tests now use far deadlines rather than
    # exact real-usleep boundary assumptions; Android gets this default branch.
    copied.write_text(source.replace('#define RE4_INPUT_DETERMINISTIC_CLOCK 1',''),encoding='utf8')
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-std=c11','-O2','-Werror',
        '-I',str(root/'analysis'),'-isystem',str(vc/'include'),
        '-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-c','-o',str(obj),str(copied)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc / "lib/x64"}',f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
    print('PASS: exact deterministic boundary branch and default far-deadline device-fixture branch executed',flush=True)
    for name,program in (('owned_menu',ownership_harness),('knife_watch_hold',hold_harness)):
        owned=path/(name+'.c');owned.write_text(program,encoding='utf8')
        subprocess.run([str(clang),'--target=aarch64-linux-android23','-std=c11','-O2','-Werror','-fsyntax-only',str(owned)],check=True)
        subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-std=c11','-O2','-Werror',
            '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
            '-c','-o',str(obj),str(owned)],check=True)
        subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
            'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
            f'/LIBPATH:{vc / "lib/x64"}',f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
            f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'],check=True)
        subprocess.run([str(exe)],check=True)
print('PASS: actual input/header host execution with deterministic time and serial mutex shims',flush=True)
