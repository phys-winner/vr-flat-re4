"""Original native lock/caller proof and execution of the actual consume-only hook; no device."""
import os
from pathlib import Path
import hashlib
import re
import struct
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
for lo,hi,expected in (
    (0x6a15440,0x6a16820,'0d21889c3a3060e8d10bfaa8dbc6f357cba4458dc44706788dc443aec1c95267'),
    (0x6a169ac,0x6a16a34,'e78d6af91f3915e8a9a7df7da4abf83a5d28ce9ae5f81e399a23c9ab8bda9ac8'),
):
    assert hashlib.sha256(native[lo:hi]).hexdigest()==expected,hex(lo)
words={
    0x6a1547c:0xf001cb00,0x6a15480:0xf9414000,0x6a15484:0x94c77687,
    0x6a167c4:0xf9400d08,0x6a167c8:0xd63f0100,0x6a167dc:0xf9414000,0x6a167e0:0x94c771b4,
    0x6a169c0:0xd001cb00,0x6a169c4:0xf9414000,0x6a169c8:0x94c77136,0x6a16a30:0x14c77120,
    0x54bf02c:0x94555e60,0x54d3ed4:0xd63f0100,0x54d3ee4:0x951c7d37,
    0x5c4f830:0x97ff13b2,0x54d3680:0x951c7f38,0x914ed4c:0xf9400c00,0x914ed50:0x942a9054,
    0x54bd1dc:0xb00274a0,0x54bd1e0:0xf9400c00,0x54bd1e4:0x951cd72f,
    0x54bd20c:0xd63f0100,0x54bd23c:0xf9400c00,0x54bd240:0x951cd71c,
    0x54bf030:0x2a1f03f8,0x54bee20:0x7100031f,0x54bee24:0x1a9303e0,
}
for address,expected in words.items():
    assert struct.unpack_from('<I',native,address)[0]==expected,hex(address)
shoff=struct.unpack_from('<Q',native,40)[0];stride,count=struct.unpack_from('<HH',native,58)
sections=[struct.unpack_from('<IIQQQQIIQQ',native,shoff+i*stride) for i in range(count)]
expected_rela={0xa378280:0xa728580,0xa352018:0xa42b1d4,0x9cbbc70:0x5c4f714};found={}
callers=[]
for sh in sections:
    if sh[1]==4:
        for pos in range(sh[4],sh[4]+sh[5],sh[9]):
            off,kind,target=struct.unpack_from('<QQq',native,pos)
            if off in expected_rela:
                assert kind&0xffffffff==1027;found[off]=target
    if sh[1]==1 and sh[2]&4:
        for i,op in enumerate(memoryview(native)[sh[4]:sh[4]+sh[5]].cast('I')):
            if op&0xfc000000!=0x94000000:continue
            immediate=op&0x03ffffff
            if immediate&0x02000000:immediate-=0x04000000
            address=sh[3]+4*i
            if address+4*immediate==0x6a169ac:callers.append(address)
assert found==expected_rela
assert callers==[0x54bf02c]

# Verify the observed four-thread wait chain in the supplied hardware evidence.
dropbox=(root/'analysis/research/p244_anr_dropbox.txt').read_text(encoding='utf8')
for text in ('sysTid=13785','sysTid=13786','sysTid=13893','054d3680','06a169c8','06a167c8','05c4f830','05c14924','05c1c4d8','06f679b0','0914ed50','action=CANCEL'):
    assert text in dropbox,text

header=(root/'analysis/re4fix_input.h').read_text(encoding='utf8')
def function(source,name):
    match=re.search(r'\b'+re.escape(name)+r'\s*\([^;{}]*\)\s*\{',source)
    assert match,name
    start=source.rfind('\n',0,match.start())+1
    brace=source.index('{',match.start());depth=1;end=brace+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
hook=re.search(r'void hook_touch\(const void\* arr\)\s*\{[^}]*\}',header).group(0)
assert hook.count('touch_process(arr)')==1 and hook.count('g_tq_calls++')==1
# Every current g_imx section ends before any rendering/native callback or sleep.
sections_locked=re.findall(r'pthread_mutex_lock\(&g_imx\);(.*?)pthread_mutex_unlock\(&g_imx\);',header,re.S)
assert len(sections_locked)==9
for section in sections_locked:
    assert not re.search(r'\b(?:gate_\w+|f_\w+|rl_\w+|aim_\w+|mg_\w+|phone_\w+|FlushRenderingCommands|nanosleep|sleep|pthread_cond_wait)\s*\(',section)
snapshot=function(header,'touch_menu_pointer_snapshot')
choose=function(header,'touch_menu_pointer_choose')
fire_snapshot=function(header,'touch_reload_user_fire')
assert 'g_cmd_until' not in fire_snapshot
for seam in (snapshot,choose,fire_snapshot):
    assert seam.count('pthread_mutex_lock(&g_imx)')==seam.count('pthread_mutex_unlock(&g_imx)')==1
    # No path returns to the native caller while retaining the local lock.
    before_unlock=seam.split('pthread_mutex_unlock(&g_imx)',1)[0]
    assert not re.search(r'\b(?:return|goto|mg_\w+|phone_\w+)\b',before_unlock)
menu=(root/'analysis/re4fix_menu_grip.h').read_text(encoding='utf8')
assert not re.search(r'\b(?:pthread_mutex_\w+|pthread_cond_wait|gate_touch|FlushRenderingCommands)\s*\(',menu)
assert '&g_imx' not in menu
# The only input helpers invoked by this native bridge are the two returning seams.
assert set(re.findall(r'\b(touch_menu_pointer_\w+)\s*\(',menu))=={
    'touch_menu_pointer_snapshot','touch_menu_pointer_choose'}
for name in ('mg_hover','mg_enabled','mg_press','mg_release'):
    assert re.search(r'\b'+name+r'\s*\(',menu),name
current=function(menu,'mg_current')
assert 'gt<=0 || gettid()!=gt' in current[:current.index('unsigned type=phone_type();')]
for name in ('touch_menu_grip_tick','touch_menu_grip_cancel'):
    assert 'if (!mg_current(hand,&pawn,&type)) return;' in function(menu,name)
refresh=function(header,'touch_refresh_context_gt')
assert 'gt > 0 && gettid() == gt && phone_type' in refresh
# The existing under-lock context getter is the proved native leaf exception.
for op in memoryview(native)[0x6338aec:0x6338b5c].cast('I'):
    assert op&0xfc000000!=0x94000000 and op&0xfffffc1f!=0xd63f0000
main=(root/'analysis/re4fix.c').read_text(encoding='utf8')
assert 'patch_code((void*)(base + 0x6a169ac), (void*)&hook_touch);' in main
menu_tick=function(main,'hook_mhtick')
assert menu_tick.index('gate_mhtick(self, dt);')<menu_tick.index('touch_menu_grip_tick(self);')
unpause=function((root/'analysis/re4fix_unpause.h').read_text(encoding='utf8'),'touch_unpause_tick')
assert unpause.index('touch_menu_grip_cancel(hand);')<unpause.index('try_unpause(0);')
print('PASS: original native input lock/caller/ANR proof, nine bounded g_imx sections, returning menu/fire/back snapshots and native grip bridge outside the local lock; existing GT-only leaf exception',flush=True)

source=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned g_tq_calls,owned_calls,native_queue_calls,native_lock_attempts,finished;
static int native_input_lock_held=1,own_lock_held;
static const void* expected_array;
static void touch_process(const void* arr) {
    assert(arr==expected_array && !own_lock_held);own_lock_held=1;owned_calls++;own_lock_held=0;
}
static void gate_touch(const void* arr) {
    (void)arr;native_queue_calls++;native_lock_attempts++;assert(!native_input_lock_held);
}
ACTUAL_HOOK
MENU_SEAM_FIXTURE
static int original_android_callback(const void* arr) {
    hook_touch(arr);assert(!own_lock_held);finished++;return 0;
}
int main(void) {
    test_menu_seams();
    const unsigned char touch_bytes[28]={0};
    struct {const void* data;int count,capacity;} array={touch_bytes,1,1},before=array;
    expected_array=&array;
    for(int i=0;i<100;i++)assert(original_android_callback(&array)==0);
    assert(g_tq_calls==100 && owned_calls==100 && finished==100);
    assert(!native_queue_calls && !native_lock_attempts && native_input_lock_held==1);
    assert(!memcmp(&array,&before,sizeof array));
    expected_array=0;hook_touch(0);assert(g_tq_calls==101 && owned_calls==101 && !native_queue_calls);
    puts("PASS: actual hook executes own consumer once, never enters native queue/lock while held, preserves caller completion/return and array bytes");
    return 0;
}
'''.replace('ACTUAL_HOOK',hook)
seam_fixture=r'''
static int g_imx;
static int pthread_mutex_lock(int* mutex) {
    assert(mutex==&g_imx && !own_lock_held);own_lock_held=1;return 0;
}
static int pthread_mutex_unlock(int* mutex) {
    assert(mutex==&g_imx && own_lock_held);own_lock_held=0;return 0;
}
ACTUAL_POINTER_ACTIONS
struct menu_pointer_request { unsigned serial; int active,pressed,action; long press_ms; };
static struct { int down,h,pressed,action,tap; unsigned serial; long press_ms,release_ms; } g_menu_pointer;
static long seam_now=1000;
static long now_ms(void) {return seam_now;}
static int menu_mode(void) {return 1;}
static int menu_grip_context(void) {return 1;}
static void sync_touch_context(int menu) {assert(menu && own_lock_held);}
static int menu_pointer_active(long now) {
    return g_menu_pointer.serial && (g_menu_pointer.down || (g_menu_pointer.tap && now<g_menu_pointer.release_ms));
}
static void menu_pointer_cancel_locked(void) {
    assert(own_lock_held);memset(&g_menu_pointer,0,sizeof g_menu_pointer);
}
ACTUAL_SNAPSHOT
ACTUAL_CHOOSE
static unsigned seam_native_calls;
static void native_bridge_continuation(void) {
    assert(!own_lock_held);seam_native_calls++;
}
static void test_menu_seams(void) {
    struct menu_pointer_request request;
    for(unsigned serial=1;serial<=100;serial++) {
        g_menu_pointer.serial=serial;g_menu_pointer.down=1;g_menu_pointer.pressed=0;
        g_menu_pointer.action=MP_PENDING;g_menu_pointer.press_ms=seam_now;
        touch_menu_pointer_snapshot(&request);native_bridge_continuation();
        assert(request.active && request.serial==serial && !request.pressed);
        assert(touch_menu_pointer_choose(serial,MP_GRIP));native_bridge_continuation();
        assert(g_menu_pointer.pressed && g_menu_pointer.action==MP_GRIP);
        assert(!touch_menu_pointer_choose(serial+100,MP_CANCEL));native_bridge_continuation();
        assert(g_menu_pointer.serial==serial);
        assert(!touch_menu_pointer_choose(serial,MP_CANCEL));native_bridge_continuation();
        assert(!g_menu_pointer.serial);
    }
    assert(seam_native_calls==400 && !own_lock_held);
    puts("PASS: actual menu snapshot/choose C releases g_imx before 400 native-bridge continuations; claim, stale serial and cancellation paths");
}
'''.replace('ACTUAL_SNAPSHOT',snapshot).replace('ACTUAL_CHOOSE',choose)
seam_fixture=seam_fixture.replace('ACTUAL_POINTER_ACTIONS',
    re.search(r'enum\s*\{\s*MP_CANCEL[^}]*\};',header).group(0))
source=source.replace('MENU_SEAM_FIXTURE',seam_fixture)
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-touch-anr-check-') as tmp:
    directory=Path(tmp);test=directory/'hook.c';test.write_text(source,encoding='utf8')
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(test)],check=True)
    obj,exe=directory/'hook.obj',directory/'hook.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-c','-o',str(obj),str(test)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
