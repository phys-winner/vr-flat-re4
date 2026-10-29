"""Offline original-ABI/preimage proof and actual cinema-helper C execution."""
import os
from pathlib import Path
import hashlib
import re
import struct
import subprocess
import tempfile
from inspect_flat_bp import read_script,DIRECTORY

root=Path(__file__).resolve().parents[2]
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
header=(root/'analysis/re4fix_cinema.h').read_text(encoding='utf8')
for lo,hi,digest in (
    (0x63e5b50,0x63e5bd4,'deb2e26f7a2edb1aa3cfad2e833b7055c9f000e8f82f42206901ce28ecbac8c1'),
    (0x6245174,0x6245288,'073e756b7ea71e9cd529f33bd845fd3d9d8672aa9ec422b6d6d6dd7a30cdbb7f'),
    (0x6244928,0x6244a64,'a117ddac2ceeb1fd443986d8cc6d0576f5579834a2b4f9e1b6201f8ceca081ee'),
    (0x8300d20,0x8301e48,'c969bcbea9f6f6d5f4f4e6514d0911ad14f1981ee8e81c695e6f4648c205b10c'),
    (0x5d36518,0x5d365b0,'0266549209ac14f095e09b74869f5b16ce68bd52836fb62e5bc84522a4ceb2eb'),
    (0x5d4f174,0x5d4f1f0,'6b9988b60dda2da7ebd49a797e1d4b5dea98709805d607b13e47f05d7285c0eb'),
    (0x8cf840c,0x8cf841c,'62db9f178eca88f2381249a8c3f1667187d85dbb0b924fe4d123e2ce7065851a'),
    (0x622d80c,0x622e1c8,'6478c72bd876457791180c9c604ece3f8cfc8b8ef053d6147be32c6d280f21ad'),
    (0x63e2824,0x63e28fc,'a607404d4fe28eeece37998afdbe0f9a25e1aa1e7533ac310d34c2fc5ba99f31'),
):
    assert hashlib.sha256(native[lo:hi]).hexdigest()==digest,hex(lo)
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',header)
assert len(checks)==25
for address,first,second in checks:
    assert struct.unpack_from('<II',native,int(address,16))==(int(first,16),int(second,16)),address
fields=re.findall(r'\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}',header.split('} fields[] = {')[1].split('};')[0])
assert len(fields)==9
for address,offset in fields:
    assert struct.unpack_from('<I',native,int(address,16)+36)[0]==int(offset,16),address
rela={a:c for a,b,c in struct.iter_unpack('<QQq',native[0x2e6c028:0x2e6c028+0x1146540]) if b==1027}
for address,name in ((0x9dff840,'Camera'),(0x9d80360,'fovY'),(0x9d80388,'transform'),
                     (0xa0f9448,'Location'),(0xa0f9418,'Rotation'),(0xa0f93f0,'FOV'),
                     (0xa0f9328,'AspectRatio'),(0xa0f9288,'ProjectionMode'),(0x9d77400,'ViewProxy')):
    value=rela[address]
    assert native[value:native.index(b'\0',value)].decode()==name,(hex(address),name)
assert rela[0x9d04e38+16+0x550]==0x622d80c
# The actual VR4 override writes its own view prefix without calling the base GetCameraView.
for address in range(0x622d80c,0x622e1c8,4):
    op=struct.unpack_from('<I',native,address)[0]
    if op&0xfc000000!=0x94000000:continue
    imm=op&0x3ffffff
    if imm&0x2000000:imm-=0x4000000
    assert address+4*imm!=0x8300d20
for caller,target in ((0x624523c,0x6244928),(0x6245254,0x63e5b50),(0x5d4f1ec,0x5d36518)):
    op=struct.unpack_from('<I',native,caller)[0]
    assert op&0xfc000000==0x94000000
    imm=op&0x3ffffff
    if imm&0x2000000:imm-=0x4000000
    assert caller+4*imm==target,hex(caller)
# C_MTXPerspective fov degree conversion is exactly float pi/180.
assert struct.unpack('<f',struct.pack('<I',0x3c8efa35))[0]==0.01745329238474369
# Original native camera authored aspect immediate float4/3 and native pawn class1170.
assert struct.unpack_from('<II',native,0x5d4f1e0)==(0x52955568,0x72a7f548)
assert struct.unpack_from('<I',native,0x63e6150)[0]==0x52822e04
# FMinimalViewInfo constrain-aspect setter explicitly sets byte30 bit0.
assert struct.unpack_from('<III',native,0x8cf840c)==(0x3940c008,0x32000108,0x3900c008)
assert header.index('gate_cinema_update(pawn,transform,fov_y,screen_on);')<header.index('cinema_capture(pawn,transform,fov_y,screen_on);')
assert header.index('gate_cinema_view(camera,dt,view);')<header.index('cinema_flat_view(camera,view);')
assert header.index('cinema_flat_view(camera,view);')<header.index('if (cinema_view_extra) cinema_view_extra(camera,view);')
assert header.index('gate_cinema_subtitle(manager,key,seconds);')<header.index('cinema_subtitle_diagnostic(key,seconds);')
assert struct.unpack_from('<I',native,0x9d7ea08+36)[0]==0
assert struct.unpack_from('<I',native,0x9d7e9e0+36)[0]==16
print('PASS: 9 original native ranges, 25 installer preimages, 9 named reflected fields, 3 exact native calls; actual VR4 vtable/no-base-call, degree/4:3 and output constrain-bit, original subtitle ABI proof',flush=True)
for extension,digest in (('.uasset','1565d70c1fa8e363512f908f84818aa09b262ae3ba89b66d07fc00db3035e2f4'),
                         ('.uexp','c8ed269a8a5a18f457f4de26b8a498fa7e86fe9f287814dd0df97c8794bc78dd')):
    assert hashlib.sha256((DIRECTORY/('CutsceneTheatreBox'+extension)).read_bytes()).hexdigest()==digest
complete=read_script('CutsceneTheatreBox','VideoComplete',32)
assert complete['calls'][0]['arguments']==[{'int':17129}]
graph=read_script('CutsceneTheatreBox','ExecuteUbergraph_CutsceneTheatreBox',17681)
graph={n['runtime']:n['expression'] for n in graph['tree']}
assert graph[17129]['call']=='ResetMedia' and graph[17148]['let_bool'][0]['field']==['VideoCompleted']
assert graph[17159]=={'jump':3854} and graph[3883]['jump_if_not']==3902
assert graph[3817]['call']['call']=='VideoComplete' and graph[3817]['context']['field']==['VideoManager']
assert graph[3902]['let_object'][1]['call'].endswith('.GetCutscenePlayerPawnIfValid')
assert graph[3977]['call']['call'].endswith('.VR4CutscenePlayerPawn.VideoComplete')
print('PASS: exact original FMV completion resets Media then routes through live VideoManager, or the original native cutscene pawn when no manager exists; no direct completion/progress writes',flush=True)
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
source=root/'analysis/research/test_cinema.c'
subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(source)],check=True)
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-cinema-') as td:
    obj,exe=Path(td)/'cinema.obj',Path(td)/'cinema.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc / "lib/x64"}',f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: Android ARM64 -Werror syntax and actual host execution',flush=True)
