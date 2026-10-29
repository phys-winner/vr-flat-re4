"""Verify mono native tracking prerequisites and execute the actual C helper."""
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
header=(root/'analysis/re4fix_tracking.h').read_text(encoding='utf8')
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',header)
assert len(checks)==27
for address,first,second in checks:
    assert struct.unpack_from('<II',native,int(address,16))==(int(first,16),int(second,16)),address
# The GOT address used by native InitDevice and tracking-allowed code is a35f500.
assert struct.unpack_from('<II',native,0x5c133dc)==(0x90023a76,0xaa0003f3)
pc=0x5c133dc
adrp=struct.unpack_from('<I',native,pc)[0]
imm=(((adrp>>5)&0x7ffff)<<2)|((adrp>>29)&3)
if imm&(1<<20): imm-=1<<21
ldr=struct.unpack_from('<I',native,0x5c133e8)[0]
assert (adrp&31)==22 and ((ldr>>5)&31)==22 and (ldr&31)==22
assert (pc&~4095)+(imm<<12)+(((ldr>>10)&0xfff)<<3)==0xa35f500
relocations={a:(info,addend) for a,info,addend in struct.iter_unpack(
    '<QQq',native[0x2e6c028:0x2e6c028+0x1146540])}
for address,target in ((0xa351bf0,0xaa75db0),(0xa35f5a8,0x9cb5458),
                       (0x9cb5590,0x5c135a8),(0x9cb5598,0x5c13230)):
    assert relocations[address]==(1027,target),(hex(address),hex(target))
assert 0x9cb5458+16+0x128==0x9cb5590
assert 0x9cb5458+16+0x130==0x9cb5598
print('PASS: 27 native tracking/camera/pose/conversion/OVR208 preimages and four exact engine/primary-vtable RELA entries',flush=True)
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
test=root/'analysis/research/test_tracking.c'
subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(test)],check=True)
with tempfile.TemporaryDirectory(prefix='re4-tracking-check-') as temporary:
    path=Path(temporary)
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2',
        '-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-c','-o',str(path/'tracking.obj'),str(test)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{path / "tracking.exe"}',str(path/'tracking.obj'),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc / "lib/x64"}',f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(path/'tracking.exe')],check=True)
print('PASS: Android aarch64 syntax and actual host execution',flush=True)

# The exported game-frame counter is an uint64 BSS object, rather than OVR's constant-zero frame argument.
section_offset=struct.unpack_from('<Q',native,40)[0]
section_size,section_count=struct.unpack_from('<HH',native,58)
sections=[struct.unpack_from('<IIQQQQIIQQ',native,section_offset+i*section_size) for i in range(section_count)]
symbols=next(s for s in sections if s[1]==11)
strings=sections[symbols[6]]
names=native[strings[4]:strings[4]+strings[5]]
counter=[]
for at in range(symbols[4],symbols[4]+symbols[5],symbols[9]):
    name,kind,other,owner,address,size=struct.unpack_from('<IBBHQQ',native,at)
    if names[name:names.find(b'\0',name)]==b'GFrameCounter':counter.append((kind,owner,address,size))
assert len(counter)==1 and counter[0][2:]==(0xa6eb040,8) and counter[0][0]&15==1
assert 'g_pose_frame_counter=(const unsigned long long*)(base+0xa6eb040);' in header
main=(root/'analysis/re4fix.c').read_text(encoding='utf8')
latch=main[main.index('static pthread_mutex_t g_pose_latch_lock'):main.index('// The pause case is authored')]
program=r'''
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 0
#endif
static long fake_ns=1000000;
static int fake_clock(int kind,struct timespec* ts){assert(kind==CLOCK_MONOTONIC);ts->tv_sec=0;ts->tv_nsec=fake_ns;return 0;}
#define clock_gettime fake_clock
static unsigned long long counter;
static const unsigned long long* g_pose_frame_counter=&counter;
static float look_yaw,look_pitch;
static int reads;
static void head_angles_now(float* y,float* p){*y=look_yaw;*p=look_pitch;reads++;}
'''+latch+r'''
int main(void){
 float y,p;look_yaw=1;look_pitch=2;counter=1;pose_angles(3,&y,&p);assert(y==1&&p==2&&reads==1);
 look_yaw=3;look_pitch=4;fake_ns+=30000000;
 const int nodes[]={3,4,9,9,0,1,2,9};
 for(unsigned i=0;i<sizeof nodes/sizeof *nodes;i++){pose_angles(nodes[i],&y,&p);assert(y==1&&p==2&&reads==1);}
 assert(g_pose_prev_yaw==0&&g_pose_prev_pitch==0);
 counter++;pose_angles(3,&y,&p);assert(y==3&&p==4&&reads==2&&g_pose_prev_yaw==1&&g_pose_prev_pitch==2);
 look_yaw=10;look_pitch=-20;fake_ns+=40000000;pose_angles(9,&y,&p);pose_angles(9,&y,&p);
 assert(y==3&&p==4&&reads==2&&g_pose_prev_yaw==1&&g_pose_prev_pitch==2);
 counter++;pose_angles(4,&y,&p);assert(y==10&&p==-20&&reads==3&&g_pose_prev_yaw==3&&g_pose_prev_pitch==4);
 counter=~0ULL;pose_angles(4,&y,&p);counter=0;pose_angles(4,&y,&p);assert(reads==5);
 g_pose_latch_on=0;look_yaw=11;pose_angles(3,&y,&p);assert(y==11&&reads==6);g_pose_latch_on=1;
 g_pose_frame_counter=0;g_pose_latch_ns=0;pose_angles(3,&y,&p);assert(reads==7);
 look_yaw=12;pose_angles(4,&y,&p);assert(y==11&&reads==7);pose_angles(4,&y,&p);assert(y==12&&reads==8);
 puts("PASS: actual look latch samples once per native frame across repeated hand/head probes, 30/40ms stalls, counter wrap, previous-frame delta, diagnostic bypass and pre-binding fallback");
}
'''
with tempfile.TemporaryDirectory(prefix='re4-pose-frame-') as temporary:
    path=Path(temporary)
    (path/'pthread.h').write_text('''#pragma once
typedef struct{int locked;}pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER {0}
static int pthread_mutex_lock(pthread_mutex_t* m){assert(!m->locked);m->locked=1;return 0;}
static int pthread_mutex_unlock(pthread_mutex_t* m){assert(m->locked);m->locked=0;return 0;}
''')
    source=path/'pose.c';source.write_text(program,encoding='utf8')
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-std=c11','-O2','-Werror','-fsyntax-only',str(source)],check=True)
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-std=c11','-O2','-Werror','-I',str(path),
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-c','-o',str(path/'pose.obj'),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{path/"pose.exe"}',str(path/'pose.obj'),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(path/'pose.exe')],check=True)
