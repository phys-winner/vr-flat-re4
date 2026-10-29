"""Verify original mirror dependencies and execute the actual flat paired-grip adapter."""
import os
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
native=(ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
for lo,hi,digest in (
    (0x6354eec,0x63554bc,'12a3989c900cabe8853d74bf44b1552db9d9326f744be81935f70874c0dd8fae'),
    (0x6354b14,0x6354ebc,'d466dfa3c3c59311ffabca438e1530ebe9025a530a9922d0cd8470521234df5a'),
    (0x62bc1e4,0x62bc3d4,'5c7fe68095b632af9a9e9b59a5e3d35a2748f622dc82b5fae7b4644e2238721b'),
):assert hashlib.sha256(native[lo:hi]).hexdigest()==digest,hex(lo)
header=ROOT/'analysis/re4fix_mirror_touch.h';source=header.read_text()
checks=[tuple(int(x,16) for x in row) for row in re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',source)]
assert len(checks)==8
for address,*expected in checks:assert list(struct.unpack_from('<II',native,address))==expected,hex(address)
for address,offset in ((0x9e035f0,0x248),(0x9e03560,0x260),(0x9d6fa80,0x218),(0x9d6f3c0,0x10)):
    assert struct.unpack_from('<I',native,address+0x24)[0]==offset
# Neither a direct completion call nor a second hand-position writer belongs to this adapter.
assert 'patch_code(' not in source and 'MirrorAMove(' not in source and 'MirrorBMove(' not in source and 'PuzzleComplete(' not in source
assert 'SetJointAngle(' not in source and 'SetJawReachedTarget(' not in source and 'FinishDogTrap(' not in source
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
test=ROOT/'analysis/research/test_mirror_touch.c'
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-mirror-check-') as temporary:
    temp=Path(temporary);obj=temp/'mirror.obj';exe=temp/'mirror.exe';fixture=temp/'native_fixture.h';math_fixture=temp/'pose_math.h'
    def function(text,signature):
        start=text.index(signature);at=text.index('{',start)+1;depth=1
        while depth:depth+=(text[at]=='{')-(text[at]=='}');at+=1
        return text[start:at]
    core=(ROOT/'analysis/re4fix.c').read_text();reload=(ROOT/'analysis/re4fix_reload.h').read_text()
    render=function(source,'static int touch_mirror_pose(')
    assert not any(token in render for token in ('tm_hold.','pv_alive(','phone_','tm_grab_location(','tm_release_distance(','interact_'))
    assert core.count('(void)touch_mirror_pose(hi,ptr_now_ms(),pos);')==1
    math_fixture.write_text('\n'.join((function(core,'static qf_t q_mul('),function(core,'static void q_rot('),
        function(reload,'static int reload_tf_valid('),function(reload,'static int reload_marker_valid('),function(reload,'static struct reload_tf reload_hand_pose('))))
    math_arg='-DMIRROR_POSE_MATH="'+math_fixture.as_posix()+'"'
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror',math_arg,'-fsyntax-only',str(test)],check=True)
    print('PASS: original mirror/holster bodies, native class/field guards and Android aarch64 adapter syntax',flush=True)
    fixture.write_text(r'''
__declspec(dllimport) void* __stdcall VirtualAlloc(void*,size_t,unsigned long,unsigned long);
__declspec(dllimport) int __stdcall VirtualFree(void*,size_t,unsigned long);
static void verify_mirror_install_guard(void) {
    unsigned char* memory=VirtualAlloc((void*)0x20000000,0xa000000,0x3000,4);
    assert(memory && (uintptr_t)memory<0x100000000ULL);
    static const struct {unsigned addr,words[2];} original[]={
'''+'\n'.join('        {0x%x,{0x%x,0x%x}},'%row for row in checks)+r'''
    };
    for(unsigned i=0;i<sizeof original/sizeof *original;i++)memcpy(memory+original[i].addr,original[i].words,8);
    *(unsigned*)(memory+0x9e035f0+0x24)=0x248;*(unsigned*)(memory+0x9e03560+0x24)=0x260;
    *(unsigned*)(memory+0x9d6fa80+0x24)=0x218;*(unsigned*)(memory+0x9d6f3c0+0x24)=0x10;
    unsigned long base=(unsigned long)(uintptr_t)memory;
    ti_ready=qs_ready=1;install_touch_mirror(base);assert(interact_mirror_hook && interact_mirror_tick);
    for(unsigned i=0;i<sizeof original/sizeof *original;i++)for(int word=0;word<2;word++) {
        ((unsigned*)(memory+original[i].addr))[word]^=1;install_touch_mirror(base);
        assert(!interact_mirror_hook && !interact_mirror_tick);
        ((unsigned*)(memory+original[i].addr))[word]^=1;
    }
    const unsigned fields[]={0x9e035f0,0x9e03560,0x9d6fa80,0x9d6f3c0};
    for(unsigned i=0;i<4;i++) {
        *(unsigned*)(memory+fields[i]+0x24)^=4;install_touch_mirror(base);assert(!interact_mirror_hook);
        *(unsigned*)(memory+fields[i]+0x24)^=4;
    }
    ti_ready=0;install_touch_mirror(base);assert(!interact_mirror_hook);ti_ready=1;
    qs_ready=0;install_touch_mirror(base);assert(!interact_mirror_hook);qs_ready=1;
    install_touch_mirror(base);assert(interact_mirror_hook && interact_mirror_tick);
    assert(VirtualFree(memory,0,0x8000));
    puts("PASS: actual mirror installer accepts original preimages and rejects each word/field/dependency corruption");
}
''')
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-DMIRROR_NATIVE_FIXTURE="'+fixture.as_posix()+'"',math_arg,'-c','-o',str(obj),str(test)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
    result=subprocess.run([str(exe),'--pose-samples'],check=True,capture_output=True,text=True)
    samples=[json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
    from verify_pair_pose_native import verify
    verify(samples)
print('LIMIT: both-direction native solver input is covered separately; physical mirror traversal and puzzle completion need gameplay QA',flush=True)
