"""Prove original extras contracts/content and execute the actual menu-only transaction helper."""
import os
from pathlib import Path
import hashlib,re,struct,subprocess,sys,tempfile

root=Path(__file__).resolve().parents[2]
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
rela={a:c for a,t,c in struct.iter_unpack('<QQq',native[0x2e6c028:0x2e6c028+0x1146540]) if t==1027}
def string(address):return native[address:native.index(b'\0',address)].decode()
def table(a,n):return [(string(rela[a+i*16]),struct.unpack_from('<q',native,a+i*16+8)[0]) for i in range(n)]
unlock=table(0x9d55728,16)
assert unlock[1]==('EUnlockable::HardMode',1) and unlock[9]==('EUnlockable::Mercenaries',9)
assert table(0x9de3c10,8)==[(f'EMercenariesUnlockable::{name}',i) for i,name in enumerate([
    'None','MafiaCostume','ChicagoTypewriter','HandCannon','HorrorMode','BigHeadMode','HighSpeedMode','GoldenGuns'])]
assert rela[0xa361da8]==0xa3f5e50
for a,z,digest in (
 (0x61d196c,0x61d19f8,'68a2e99ce40b0d2e2e88bf15e332e349b04687106bf8bcf619b6bcb066c52f34'),
 (0x61e1b28,0x61e1b58,'f9cfa11218cdbbb1f70cbc572bfecc3fee0e7eb840321ed100477506a28aa1ca'),
 (0x61e1b94,0x61e1bc4,'1c13a15ea11112ea9f048a5381dcd26eb1b7c44da7b1453b3ae86e6199a61a33'),
 (0x61d1b34,0x61d1fac,'4cf7f7aa5ac22caa989db4f66f10a609fde804bc0e907f5b48e0ce2286cf3e49'),
 (0x61d1fac,0x61d209c,'20037af175421f23f9be082a183cb5e3d1216aad5407413b355192011dc5020d'),
 (0x626dc00,0x626dce8,'fdce4c5289e3ad24a2da16eaec51ad68924cd6a47f4d49110c64024c26aeafb6'),
 (0x626dd40,0x626de28,'2e5c4ab6c4e9d3297155563be7d3524eeff05a874a2c81d64f89426539cf281e'),
 (0x64c2538,0x64c25f4,'0c6c82a0f23f0321ff5ad09a71251ade9e1c0ce645b60e0a53f7604557c86335'),
 (0x64781dc,0x647829c,'a257833cc672bd4a660a7a6048fa8cfcea54c0f1ab4ba43a543565930db721a8'),
 (0x64b7218,0x64b72d8,'34f93265224fba0f2ce57281cf3b60249950ec886f74e4fc5edb8c9ad88c357c'),
 (0x8894238,0x88942ac,'1572dffa064cf42aa635ae7d07170ad5fe35a38bc89f40cde76598cba2b7f631'),
 (0x6615fd0,0x6616020,'d61c13d0937f23210b44100ce9c19c78f5368aa45debdd908429505124ea2b37'),
 (0x632a184,0x632a214,'637ee2556a97ce139a63741e3c0f774ac76ab8cb92f4c9b81f000c6a7c524abe'),
 (0x632a368,0x632a3f8,'fe02bd5dd48d8f79e1f5e8fafd1bb5ba78bd81286a2dda4410a1b83fe6db7204'),
 (0x632a744,0x632a7d4,'e9c8c84da6e0d02ef3f167e733645a5c7dfca180f6a5bc2c3e1f8c146242a2bf'),
 (0x632a940,0x632a9d0,'ad30951fb6f54cb3be9b8540590a9f96ee79624897cea9867718549db6da315e'),
):assert hashlib.sha256(native[a:z]).hexdigest()==digest,hex(a)
def branch(a):
    op=struct.unpack_from('<I',native,a)[0];assert op&0x7c000000==0x14000000
    immediate=op&0x3ffffff
    if immediate&0x2000000:immediate-=0x4000000
    return a+4*immediate
assert struct.unpack_from('<I',native,0x61d1998)[0]==0x37b001ea # bit22 -> skip all original resets
assert branch(0x61d19b4)==0x5f456d0 and branch(0x61d19d0)==0x5f45860
assert branch(0x626dcd4)==0x640bddc # native lifetime/type guards -> original BP metadata save
assert struct.unpack_from('<I',native,0x61d1b30)[0]==0xd65f03c0 # original character unlock is a shipping stub
assert struct.unpack_from('<I',native,0x63815b4)[0]==0xd65f03c0 # DebugGivePlayerGun is not used

sys.path.insert(0,str(root/'analysis'))
from pak9 import Pak9
pak=Pak9(root/'VRLOC.com.Armature.VR4/patch.201.com.Armature.VR4.obb','VR4/Content/Paks/pakchunk0-Android_ETC2.pak')
required=[
 'VR4/Content/Levels/MercenariesMenuMap.umap',
 'VR4/Content/UI/VR4/Menus/Mercenaries/UI_MercenariesMenu_BP.uasset',
 'VR4/Content/DataAssets/Challenges/MercenariesChallenges_DT.uasset',
 'VR4/Content/Blueprints/UI/MainMenu/NewGameOptions_DT.uasset',
 'VR4/Content/Blueprints/Player/Bio4/Leon_Special2/VR4Bio4PlayerPawn_Leon_Special2_BP.uasset',
 'VR4/Content/Blueprints/Player/Bio4/Ashley_Special2/VR4Bio4PlayerPawn_Ashley_Special2_BP.uasset',
 'VR4/Content/Props/BIO4/Weapons/pistolHandcannon/Geometry/wep1500_HandCannon_Static.uasset',
 'VR4/Content/Props/BIO4/Weapons/chicagoTypewriter/Geometry/wep12_chicagoTypewriter.uasset',
 'VR4/Content/Blueprints/Singletons/VR4HorrorMode_BP.uasset',
 'VR4/Content/UI/VR4/Unlocks/Textures/UI_Unlock_Challenge_BigHead.uasset',
 'VR4/Content/UI/VR4/Unlocks/Textures/UI_Unlock_GoldenGuns.uasset',
]
for asset in required:assert asset in pak.entries,asset
assert 'SysSave' not in required
pak.f.close();pak.z.close()
assert struct.unpack_from('<I',native,0x8894260)[0]==0xaa0803f3 # Native FString sret is x8.
assert struct.unpack_from('<I',native,0x64b72a0)[0]==0x5280ae04 # Native base-player class570.
print('PASS:16 complete native bodies,24 original enum pairs, x8 FString/native free ownership, first-unlock-only reset and11 actual cooked packages',flush=True)

header=(root/'analysis/re4fix_mobile_extras.h').read_text()
assert not any(x in header for x in ['CardSysSave','DebugGive','UnlockMercenariesCharacter','ToggleUnlockExtra','AdaGame','ResetToTitle','LoadMercenariesInventory'])
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',header)
assert len(checks)==18
for a,x,y in checks:assert struct.unpack_from('<II',native,int(a,16))==(int(x,16),int(y,16)),a
core=(root/'analysis/re4fix_cinema.h').read_text()
guard=core[core.index('static int cinema_object('):core.index('static int cinema_on_game_thread(')]
prefix='#include <string.h>\n'+guard
prefix+='unsigned long fixture_extras_code[]={'+','.join(a for a,_,_ in checks)+'};\n'
prefix+=f'unsigned fixture_extras_count={len(checks)};\nvoid fixture_extras_image(unsigned char* p){{\n'
for a,x,y in checks:prefix+=f'*(unsigned*)(p+{a})={x};*(unsigned*)(p+{a}+4)={y};\n'
prefix+='*(long long*)(p+0x9d55728+1*16+8)=1;*(long long*)(p+0x9d55728+9*16+8)=9;\n'
for i in range(1,8):prefix+=f'*(long long*)(p+0x9de3c10+{i}*16+8)={i};\n'
prefix+='*(uintptr_t*)(p+0xa361da8)=(uintptr_t)p+0xa3f5e50;\n}\n'
# uintptr_t in generated population needs this include before definitions.
prefix='#include <stdint.h>\n'+prefix
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-extras-') as temporary:
    d=Path(temporary);source=d/'extras.c';source.write_text(prefix+(root/'analysis/research/test_mobile_extras.c').read_text(),encoding='utf8')
    arm=d/'extras-arm64.o'
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-I',str(root/'analysis'),'-c','-o',str(arm),str(source)],check=True)
    assembly=subprocess.check_output([str(clang.with_name('llvm-objdump.exe')),'-d',str(arm)],text=True)
    veneer=assembly.split('<extras_current_level>:',1)[1]
    assert 'mov\tx8, x2' in veneer and 'br\tx16' in veneer
    obj,exe=d/'extras.obj',d/'extras.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-DEXTRAS_MAP_TEST','-O2','-Werror','-I',str(root/'analysis'),
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-isystem',str(sdk/'Include/10.0.26100.0/um'),'-isystem',str(sdk/'Include/10.0.26100.0/shared'),
        '-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS:actual ARM64 Werror adapter/FString ABI and executed C menu transactions with original shared ancestry guard',flush=True)
