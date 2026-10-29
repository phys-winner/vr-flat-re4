"""Host checks for the native touch reload helper; does not build/install an APK."""
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import hashlib

root = Path(__file__).resolve().parents[2]
clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
header = (root / "analysis/re4fix_reload.h").read_text(encoding="utf-8")
native = (root / "apk_decoded/lib/arm64-v8a/libUE4.so").read_bytes()
checks = re.findall(r"\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}", header)
assert len(checks) == 54, f"expected 54 native preconditions, got {len(checks)}"
for address, word0, word1 in checks:
    actual = struct.unpack_from("<II", native, int(address, 16))
    assert actual == (int(word0, 16), int(word1, 16)), f"native opcode mismatch at {address}: {actual}"
# SetWorldTransform reads scalar Scale3D.Z at +0x24 and X/Y at +0x1c.
assert struct.unpack_from('<II', native, 0x83a92ec) == (0xb94026e8, 0xf841c2e9)
assert struct.unpack_from('<12f', native, 0x4315110) == (0,0,0,1,0,0,0,1,1,1,0,0)
assert struct.unpack_from('<III', native, 0x62bbea8)==(0xf9406129,0xeb08013f,0x54000360)
assert struct.unpack_from('<I', native, 0x62c09a8)[0]==0x17ff6947
assert struct.unpack_from('<II',native,0x62729d0)==(0xf81e0ff3,0xa9017bfd)
assert struct.unpack_from('<II',native,0x629b2fc)==(0xf9021674,0xb9400e89)
for params,offset in ((0x9da3ca0,0x8e0),(0x9d96840,0x578),(0x9db0b18,0x3c0),
                      (0x9db0e60,0x270),(0x9db1e38,0x348),(0x9db1dd8,0x350),(0x9db2570,0x248),
                      (0x9daded8,0xd38),(0x9d9e298,0x300)):
    assert struct.unpack_from('<I',native,params+36)[0]==offset,hex(params)
# FCachedSceneComponent returns a scalar FTransform through x8, with no
# FVR4UnifiedScaleTransform tag. Positive-scale path also writes only q/p/s.
assert struct.unpack_from('<II',native,0x61e4184)==(0x2d038e64,0x4ea008e3)
assert struct.unpack_from('<I',native,0x61e4190)[0]==0xbd002662
assert struct.unpack_from('<II',native,0x61e426c)==(0x2d020660,0xbd001a62)
for lo,hi,digest in (
    (0x62b6034,0x62b6078,'46557c00416297869525f1187d7f52dfb6dab01b8f5a5b025f8aedae03f74491'),
    (0x62b60f8,0x62b6138,'fbcf4bfefc1e62465fb4ee22c3164875b5dc89a26683a4a9dca694a28edf70a7'),
    (0x62b6138,0x62b6150,'9eb3cd1e10ff3c4ea8b7bd060c5964f7deefe5cd39d83d7fc068d911289b2e19'),
    (0x62b6174,0x62b61bc,'6546eb84ff50dfdec1e11dfb1e679fb9673e69d9be9f2ae4d37c3cd74400a97f'),
    (0x62b58f4,0x62b5ed8,'f8598e3b37e2f83b5ba81cef0df1488071b580dd462b2a146243e9bae1a64885'),
    (0x62825b0,0x6282608,'a2ee0ecb64834b472f241f0388e9e07d52359d7eefaa6f99e21f9fe4298c4d20'),
    (0x628c464,0x628c5bc,'afe31ec6c0b2a1d78f0037f789dc370c8d8238a4e417a6378d0eea10e16814d3'),
    (0x62bb880,0x62bb8c8,'e557686b66c8c1f443f8c737d46e112e69b76c10e4fed702d0c05f449eaad5bd'),
    (0x62bb584,0x62bb5cc,'9c624ca291742e086ca6abdfc29191913b54569226d1f48a979bb90bbaf772a1'),
    (0x62729d0,0x6272a08,'f6deb7e90e1873c653d4d886768189735b30412cbbe84afb1e4774ad233a94ad'),
    (0x6272a08,0x6272b9c,'668b2b6d15cc729342e9fffe6fef56ef3292b2695356ad43ad334a93d5740d60'),
    (0x62c08c4,0x62c09ac,'afffbb6e0d4e3b1405f0484e66cf074b96450be4780be8327cc2840e4638127e'),
):
    assert hashlib.sha256(native[lo:hi]).hexdigest()==digest,hex(lo)
# Exact original holster iteration: first eligible through virtual +670 wins.
assert struct.unpack_from('<II',native,0x62c0980)==(0xf9433908,0xd63f0100)
assert struct.unpack_from('<III',native,0x62c0988)==(0x3607fd60,0xf9413aa1,0xaa1403e0)
# Ammo's original predicate accepts its own gun only when exactly one gun is
# held; another campaign pouch is not a reason to reject the entire holster.
assert struct.unpack_from('<II',native,0x6272a8c)==(0xb94013e8,0x7100051f)
assert struct.unpack_from('<III',native,0x6272a9c)==(0xf942be68,0xf9400009,0xeb08013f)
# The actual native Pistol ready/SetSlideCocked slots, rather than base Gun ready.
targets={0x9d1fa20+16+0x778:0x62b6034,0x9d1fa20+16+0x840:0x62b6138}
sections=struct.unpack_from('<Q',native,0x28)[0]
stride,count=struct.unpack_from('<HH',native,0x3a)
found={}
for i in range(count):
    section=struct.unpack_from('<IIQQQQIIQQ',native,sections+i*stride)
    if section[1]!=4: continue
    for offset in range(section[4],section[4]+section[5],section[9]):
        addr,info,value=struct.unpack_from('<QQq',native,offset)
        if addr in targets:
            assert info==0x403
            found[addr]=value
assert found==targets
assert struct.unpack_from('<I',native,0x642e5d0)[0]==0x5281bc04 # native Pistol size de0
assert not re.search(r'\(base\s*\+\s*0x(?:628dc70|62b6138)\)',header) # no Fire/cock setter
print("PASS: 54 native preconditions, scalar FTransform48 ABI, 9 property offsets, complete pistol cocking branch and actual virtual ready contract", flush=True)
subprocess.run([str(clang), "--target=aarch64-linux-android23", "-O2", "-fsyntax-only",
                str(root / "analysis/re4fix.c")], cwd=root, check=True)
print("PASS: integrated header compiled for Android aarch64", flush=True)
for test in ('test_reload_pose.c','test_reload_cock.c'):
    subprocess.run([str(clang),"--target=aarch64-linux-android23","-O2","-Werror","-fsyntax-only",
                    str(root/'analysis/research'/test)],check=True)
print("PASS: owned reload helper/test compiled for Android aarch64 with -Werror",flush=True)

vc = Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231")
sdk = Path(r"C:\Program Files (x86)\Windows Kits\10")
with tempfile.TemporaryDirectory(prefix="re4-reload-check-") as temporary:
  for test in ('test_reload_pose.c','test_reload_cock.c'):
    obj = Path(temporary) / (test+'.obj')
    exe = Path(temporary) / (test+'.exe')
    subprocess.run([str(clang), "--target=x86_64-pc-windows-msvc", "-O2",
        "-Wno-int-to-pointer-cast", "-Wno-int-to-void-pointer-cast",
        "-isystem", str(vc / "include"), "-isystem", str(sdk / "Include/10.0.26100.0/ucrt"),
        "-c", "-o", str(obj), str(root/'analysis/research'/test)], check=True)
    subprocess.run([str(vc / "bin/Hostx64/x64/link.exe"), "/NOLOGO", f"/OUT:{exe}", str(obj),
        "libcmt.lib", "libucrt.lib", "libvcruntime.lib", "oldnames.lib", "kernel32.lib",
        f"/LIBPATH:{vc / 'lib/x64'}", f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/ucrt/x64'}",
        f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/um/x64'}"], check=True)
    args=[str(exe)]
    if test=='test_reload_cock.c': args.append(str(root/'apk_decoded/lib/arm64-v8a/libUE4.so'))
    subprocess.run(args, check=True)
  # Reinsert the former sole-item restriction into the production helper. The
  # SAME actual C lifecycle fixture must fail at the shared campaign request.
  restriction='void* prop=(void*)props[i];entries[i]=prop;if(!prop)continue;'
  assert header.count(restriction)==1
  baseline=Path(temporary)/'sole_item_reload.h'
  baseline.write_text(header.replace(restriction,restriction+'if(prop!=ammo)return 0;'),encoding='utf8')
  baseline_test=Path(temporary)/'sole_item_fixture.c'
  fixture=(root/'analysis/research/test_reload_pose.c').read_text(encoding='utf8')
  baseline_test.write_text(fixture.replace('../re4fix_reload.h',baseline.as_posix()),encoding='utf8')
  subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
      '-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
      '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
      '-c','-o',str(obj),str(baseline_test)],check=True)
  subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
      'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
      f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',
      f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
  rejected=subprocess.run([str(exe)],capture_output=True,text=True)
  assert rejected.returncode and 'g_reload_phase==1 && g_cmd_until[1]==1180 && !eligibility_calls' in rejected.stderr
  print('PASS: unchanged campaign shared-holster C fixture rejects the former sole-item guard',flush=True)
print("PASS: actual C transform helper, owned Ammo/Holster route, bounded magazine + native cock acknowledgement and cancellation")
