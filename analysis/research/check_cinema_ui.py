"""Original subtitle viewport/weak-pointer ABI proof and actual helper C tests."""
import os
from pathlib import Path
import hashlib,re,struct,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
header=(root/'analysis/re4fix_cinema_ui.h').read_text()
for lo,hi,digest in (
    (0x69defcc,0x69df01c,'5263854ad7474485db542937af9e1a65927596cca4b4633190c64b57614c9f67'),
    (0x69df1cc,0x69df24c,'2144371488f7df4eaa292697317803db88ca4ffa31e731786a20f9ecd6f85344'),
    (0x7dd5c58,0x7dd5c6c,'dc561575093e6cc9d54dbba119785c333747fecbc993a170ca0c24e22758d3c3'),
    (0x7dd70f4,0x7dd743c,'b137241a03510ec9efebe54c1b613fa72fbf39269852d65df57583aea9b9accf'),
    (0x7dd7468,0x7dd7494,'e40504e95936574988f394e441bbaa723e6018c1d23e22a8f82029e715684461'),
    (0x7db1a28,0x7db1a30,'3306a9fc3bb6052ef5dbee5d5a3407f4e4f737c6ed4025ce1d03224d0c1e3f86'),
    (0x7e37bec,0x7e37cac,'172e88cd88646ef0101627ef377d4e9821544ebadcec13d3ef9c918c0683f9de'),
    (0x7da30fc,0x7da3104,'0c212c7473b7fd0a59731f2321adfe362e7b43ceb76319298f3c41842a018c77'),
    (0x7da3274,0x7da34b4,'ac8a2b121a58d920fa740a334391dbb6cd9fbc3ce333c34c3e1e4a358b119193'),
    (0x7dd762c,0x7dd7678,'90e234cae491208f09aebc35a64a8bc585284d6f8fe12f2bdb1fcbf568851ab9'),
    (0x7dd7678,0x7dd7688,'6ab7d97918af0206b07933d7771ad64a87d851dd4b69d0f9c2f63efd5c1c1e09'),
    (0x7dd7688,0x7dd769c,'c5c88e575e61d7f4d64f736ef37afae43492f6da73b0e6f7d460c30c6582fc80'),
    (0x7dd769c,0x7dd76a8,'b3adb66bf13ffe95efdc43483d5f947f906191f43dbc1cd1fc3773053eea87bd'),
    (0x7db1fb4,0x7db1fc8,'fc78490c9c96bb78cf558c17d0c79c0bad7231aef87825192bd416026e09001c'),
    (0x7de6f14,0x7de70b8,'6ea8ba9ebbbc6402fe16180e0627d8ee25b361b85e48a5f64180ab013bd196d3'),
    (0x7e296b4,0x7e29770,'a2d7c6e5320ee8ead524ba4d7752a49bb9150562501bb736507725093dd22069'),
    (0x7e52bf4,0x7e52ca8,'22d6e23a5e1794a5306076ae80627bf514b04d0e07812aa5e1126146529ce687'),
    (0x7dd5818,0x7dd5828,'dd4ad9287fddf5429c92c11fe247d9ffecedba3c175b0884243593be1be82da1'),
    (0x7da4524,0x7da46d4,'f163ed4c06558806a3a8b5d2d8b0257c1bf21cbc131798dd168d464a9c89d13a'),
):
    assert hashlib.sha256(native[lo:hi]).hexdigest()==digest,hex(lo)
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',header)
assert len(checks)==23
for address,first,second in checks:
    assert struct.unpack_from('<II',native,int(address,16))==(int(first,16),int(second,16)),address
assert struct.unpack_from('<I',native,0x9f77968+36)[0]==0x498
for address,value in ((0x9f701c0+36,0x90),(0x9f70190+36,0xac),(0x9f7bd18+36,8),(0x9f7bd98+32,28)):
    assert struct.unpack_from('<I',native,address)[0]==value,hex(address)
for address,value in ((0x9f6b718+36,0x1a8),(0x9f64bd0+36,0x188),(0x9f64c30+36,0x150),
    (0x9e51020+32,0x50),(0x9e50f98+36,8),(0x9e50f68+36,0x10),(0x9e51098+36,8),
    (0x9e50f40+36,0x40),(0x9e50f18+36,0x48),(0x9e503b8+32,0x28),(0x9e50348+36,0x10)):
    assert struct.unpack_from('<I',native,address)[0]==value,hex(address)
# Native ABI/storage, including position's authoritative DPI conversion.
assert struct.unpack_from('<II',native,0x7da30fc)==(0x2d130400,0x17fffeb4)
assert struct.unpack_from('<II',native,0x7dd7654)==(0x1e201929,0x1e201908)
assert struct.unpack_from('<II',native,0x7dd765c)==(0xbd01f269,0xbd01f668)
assert struct.unpack_from('<II',native,0x7dd7664)==(0xa91e7e7f,0xa9427bfd)
# Exact original weak Get rejects serial changes and PendingKill/Unreachable before loading object.
assert struct.unpack_from('<II',native,0x69df220)==(0x6b08019f,0x540000c1)
assert struct.unpack_from('<II',native,0x69df234)==(0x721c051f,0x54000060)
# Exact script fields come from the saved original runtime reflection, not guessed offsets.
runtime=(root/'analysis/research/p252_cinema_probe_runtime.log').read_text(encoding='utf-8-sig')
for name,offset in (('TheatreBox','1198'),('WidgetSubtitles','2b0'),('SubtitleWidget','338')):
    assert re.search(r"name='"+name+r"' type='ObjectProperty' offset="+offset+r" element=8 dim=1",runtime)
assert "class='CutsceneTheatreBox_C' size=528" in runtime
assert "'Subtitles_C_" in runtime and "class='Subtitles_C'" in runtime
main=(root/'analysis/re4fix_cinema.h').read_text()
assert main.index('gate_cinema_view(camera,dt,view);')<main.index('touch_cinema_ui_tick();')<main.index('cinema_flat_view(camera,view);')
print('PASS: 19 native complete ranges and 23 preimages; exact read-only owned TextBlock/WidgetTree, font80/color40/opacity ABI; native viewport/scale/pivot HFA and DPI; Widget498/getter, weak serial/GC refusal, runtime BP fields and original-first integration',flush=True)
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
source=root/'analysis/research/test_cinema_ui.c'
subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(source)],check=True)
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-cinema-ui-') as td:
    obj,exe=Path(td)/'ui.obj',Path(td)/'ui.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc / "lib/x64"}',f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: Android ARM64 -Werror and actual C host execution',flush=True)
