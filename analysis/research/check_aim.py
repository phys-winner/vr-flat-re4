"""Verify native aim dependencies and execute the actual aim pose/guards with shared math."""
import os
from pathlib import Path
import mmap
import hashlib
import re
import struct
import subprocess
import tempfile
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from check_fire_axis import check_native_fire_axis

root=Path(__file__).resolve().parents[2]
header=root/'analysis/re4fix_aim.h'
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',header.read_text(encoding='utf8'))
assert len(checks)==35
with (root/'apk_decoded/lib/arm64-v8a/libUE4.so').open('rb') as stream:
    with mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as native:
        offset=struct.unpack_from('<Q',native,0x20)[0];stride,count=struct.unpack_from('<HH',native,0x36)
        segments=[struct.unpack_from('<IIQQQQQQ',native,offset+i*stride) for i in range(count)]
        for address,first,second in checks:
            address=int(address,16)
            segment=next(s for s in segments if s[0]==1 and s[3]<=address and address+8<=s[3]+s[5])
            assert struct.unpack_from('<II',native,segment[2]+address-segment[3])==(int(first,16),int(second,16)),hex(address)
        assert struct.unpack_from('<12f',native,0x4315110)==(0,0,0,1,0,0,0,1,1,1,0,0)
        assert struct.unpack_from('<II',native,0x83a92ec)==(0xb94026e8,0xf841c2e9)
        assert struct.unpack_from('<II',native,0x61e3d14)==(0xb9002668,0xf801c269)
        identity=bytes(native[0x4315110:0x4315110+48])
        assert check_native_fire_axis(native)==256
        # Independent native contracts: recoil is updated in PreBio4Tick,
        # while the shotgun's PostBio4Tick completes held/forestock poses.
        def bl(caller,target):
            op=struct.unpack_from('<I',native,caller)[0]
            assert op&0xfc000000==0x94000000
            displacement=op&0x3ffffff
            if displacement&0x2000000: displacement-=0x4000000
            assert caller+4*displacement==target,hex(caller)
        bl(0x628b4d8,0x628b83c)
        bl(0x62c9ac0,0x6292974)
        bl(0x629298c,0x628c5bc)
        bl(0x628c5d4,0x62b985c)
        bl(0x6213ba4,0x6298468) # derived Bio4 Tick calls the hooked base before its own late hand writer.
        bl(0x62b4a38,0x6333d9c) # Ordinary queued Pause keeps the authored remembered start screen.
        bl(0x62b4a54,0x633223c) # Both queued Pause and Inventory pass the same two-byte struct in w0.
        assert struct.unpack_from('<II',native,0x62b4a50)==(0x79400be0,0x9401f5fa)
        assert struct.unpack_from('<4I',native,0x633223c)==(0xf81d0ff5,0xa9014ff4,0xa9027bfd,0x910083fd)
        assert struct.unpack_from('<I',native,0x621461c)[0]==0xbd051e60 # native obstruction RaiseGunFraction stays authored.
        assert struct.unpack_from('<II',native,0x6214c24)==(0xbd451c00,0xd65f03c0)
        # The ORIGINAL no-recoil getter already saves/restores exactly this
        # mesh-relative transform with the same setter. FireBone stays +Y.
        bl(0x628ebc0,0x83a96f4)
        bl(0x628ebcc,0x628e9b4)
        bl(0x628ebd8,0x83a969c)
        bl(0x628ec00,0x83a969c)
        assert struct.unpack_from('<II',native,0x628e9d4)==(0x3963e008,0x35000328)
        assert struct.unpack_from('<I',native,0x628ea4c)[0]==0x91240260 # native default FTransform +900
        assert struct.unpack_from('<I',native,0x62939cc)[0]==0x9137c260 # exactly one hand: RecoilParameters +df0
        assert struct.unpack_from('<4I',native,0x62c9a8c)==(0xd10283ff,0xfd0033e8,0xa90757f6,0xa9084ff4)
        # Rifle and shotgun share the same FireBone matrix and legacy negative-X direction.
        for caller,target in ((0x5fd8698,0x6290d8c),(0x5fd86a4,0x61cc6b0),(0x5fd8734,0x5d35494),
                              (0x5fd8fb0,0x6290d8c),(0x5fd8fbc,0x61cc6b0),(0x5fd9050,0x5d35494)):
            op=struct.unpack_from('<I',native,caller)[0]
            assert op&0xfc000000==0x94000000
            immediate=op&0x3ffffff
            if immediate&0x2000000: immediate-=0x4000000
            assert caller+4*immediate==target,hex(caller)
        assert struct.unpack_from('<4I',native,0x5fd86c0)==(0x528f8009,0x528a000a,0x72b91249,0x72b9086a)
        assert struct.unpack('<f',struct.pack('<I',0xc8927c00))[0]==-300000
        assert struct.unpack('<f',struct.pack('<I',0xc8435000))[0]==-200000
        assert struct.unpack_from('<I',native,0x5fd900c)[0]==0x1e21400a # fneg s10,s0: authored shotgun range.
        assert struct.unpack_from('<I',native,0x5fd90c4)[0]==0xbd0013ea # str s10,[sp,#0x10]: shot X.
        # Gun control lock returns1; base Prop returns1 on GameOver and2 otherwise.
        bl(0x628cf3c,0x5fb067c) # cPlayer::ArmVrHasControl
        assert struct.unpack_from('<I',native,0x628cf78)[0]==0x52800020
        bl(0x62bb708,0x61d4050) # ABio4::IsGameOver
        assert struct.unpack_from('<II',native,0x62bb710)==(0x52800028,0x1a881500)
        for lo,hi,digest in (
            (0x628b83c,0x628c130,'4278d505f75a8fa54a74bcac00d43ff7687207b111b920d317d8e0fb73656d6c'),
            (0x628e55c,0x628e57c,'b99ba89de8b96bacd4eb03a2af8a584fdabaae18181d4b32701bd7f50ed4e255'),
            (0x6420bc0,0x6420c80,'9fc31a8c6c77a47671f12cf7fceaac3339aa67b5c2bde1328792ad9252794ded'),
            (0x62bb880,0x62bb8c8,'e557686b66c8c1f443f8c737d46e112e69b76c10e4fed702d0c05f449eaad5bd'),
            (0x62bb584,0x62bb5cc,'9c624ca291742e086ca6abdfc29191913b54569226d1f48a979bb90bbaf772a1'),
            (0x5fd8698,0x5fd877c,'6da62a74e25defd3130a414680a89415fc1c97e35229d28186051d3b06ffecc2'),
            (0x5fd8fb0,0x5fd90a8,'b6245e93ed8691e9b7e65bb06f59e1fd574392734c67823435a8398fb7f00c4a'),
            (0x628e9b4,0x628ea64,'99d477e1c9604c0fec4dce6b6541872c524620b13c0cd1c027c0918f05a8291a'),
            (0x83a969c,0x83a96f4,'27d7c204a68b6833409540454c35d6864c9dee2bc6301f971622718db8f9f7fe'),
            (0x83a96f4,0x83a984c,'d708e4541074d441f2b4a6a1daa72dc956476015bfa63b33b34a7103e46687f7'),
            (0x62c9a8c,0x62c9c78,'3e1af4740eced6c1169b5ee12ac5873ef5375da01d69199e6b5ad4d1f713848b'),
            (0x62938d8,0x62939fc,'ee8d80e8858c0e2ec759aca10c01bd55367d23f526acbd15fe7b18e441e7a6f2'),
        ):
            assert hashlib.sha256(native[lo:hi]).hexdigest()==digest,hex(lo)
print('PASS: 35 native preimages, shared queued/Inventory two-byte TryPause ABI and original gate prologue, typed Gun base size d40, animation/mesh ABI, original recoil/post-tick and derived-hand/base ordering, native obstruction getter retained, independent scalar FTransform ABI',flush=True)

main=(root/'analysis/re4fix.c').read_text(encoding='utf8')
reload=(root/'analysis/re4fix_reload.h').read_text(encoding='utf8')
math=main[main.index('static qf_t q_mul('):main.index('static float g_eye_height')]
for signature in ('static int reload_tf_valid(','static int reload_marker_valid(','static struct reload_tf reload_hand_pose(',
                  'static int reload_is_a(','static int reload_layout('):
    start=reload.index(signature)
    math+=reload[start:reload.index('\n}',start)+2]+'\n'
source=(root/'analysis/research/test_aim.c').read_text(encoding='utf8')
source=source.replace('/* SHARED_MATH */',math).replace('AIM_HEADER',header.as_posix())
source=source.replace('/* NATIVE_IDENTITY */','static const unsigned char native_identity[48]={'+','.join(str(x) for x in identity)+'};')
diag=(root/'analysis/re4fix_aim_diag.h').read_text(encoding='utf8')
source=source.replace('/* FIRE_ABI */',diag[diag.index('struct __attribute__((aligned(16))) aim_fire_tf'):diag.index('static struct aim_fire_tf (*aim_fire)')])
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-aim-align-check-') as temporary:
    directory=Path(temporary);test=directory/'aim.c'
    test.write_text(source+'\nint check_pause_gate(unsigned short start){return gate_aim_try_pause(start);}\n',encoding='utf8')
    android_object=directory/'aim-android.o'
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-c',str(test),'-o',str(android_object)],check=True)
    assembly=subprocess.check_output([str(clang.parent/'llvm-objdump.exe'),'-d',str(android_object)],text=True)
    pause_gate=assembly.split('<gate_aim_try_pause>:',1)[1]
    assert tuple(int(word,16) for word in re.findall(r'^\s*[0-9a-f]+:\s+([0-9a-f]{8})',pause_gate,re.M)[:4])==(0xf81d0ff5,0xa9014ff4,0xa9027bfd,0x910083fd)
    print('PASS: compiled AArch64 TryPause gate preserves all four original prologue instructions',flush=True)
    test.write_text(source,encoding='utf8')
    obj,exe=directory/'aim.obj',directory/'aim.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-c','-o',str(obj),str(test)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
    # The same fixture must reject unbounded/early-only recoil handling and
    # the old inventory depth captured against the outgoing ADS camera FOV.
    # Mutate only the tested source line, leaving the C fixture unchanged.
    original=header.read_text(encoding='utf8')
    for label,old,new,failure in (
        ('unbounded_recoil','int bounded=aim_shotgun_recoil(gun,mesh);','int bounded=0;','angle<=.10473f&&distance<=9.001f'),
        ('early_only','if(touch_weapon_cycle_pose_reference(primary,gun,&reference))touch_aim_tick(primary);','(void)reference;','angle<=.10473f&&distance<=9.001f'),
        ('inventory_ads_fov','aim_fps.inventory_hold<0 ? (float)mobile_prefs_fov():g_fov_h_deg','g_fov_h_deg','aim_fps.inventory_pose.p[k]-future_hip.p[k]'),
        ('ordinary_pause_unprepared','touch_aim_inventory_prepare(rl_left(pawn));','(void)pawn;','aim_fps.inventory_hold==1&&!aim_fps.blend'),
    ):
        assert original.count(old)==1
        candidate=directory/(label+'.h');candidate.write_text(original.replace(old,new),encoding='utf8')
        test.write_text(source.replace(header.as_posix(),candidate.as_posix()),encoding='utf8')
        subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
            '-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast','-I',str(header.parent),
            '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
            '-c','-o',str(obj),str(test)],check=True)
        subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
            'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
            f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',
            f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
        rejected=subprocess.run([str(exe)],capture_output=True,text=True)
        assert rejected.returncode and failure in rejected.stderr,label
        print('PASS: unchanged actual production fixture rejects '+label,flush=True)
