"""Native enum/float-cvar contracts, actual guarded adapters and Android settings compilation."""
import os
from pathlib import Path
import hashlib,re,struct,subprocess,tempfile

root=Path(__file__).resolve().parents[2]
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
rela={a:c for a,t,c in struct.iter_unpack('<QQq',native[0x2e6c028:0x2e6c028+0x1146540]) if t==1027}
def string(address):return native[address:native.index(b'\0',address)].decode()
def enum(address,count):return [(string(rela[address+i*16]),struct.unpack_from('<q',native,address+i*16+8)[0]) for i in range(count)]
assert enum(0x9d66ff0,5)==[('ETurnMode::Degrees15',15),('ETurnMode::Degrees30',30),
    ('ETurnMode::Degrees45',45),('ETurnMode::Degrees90',90),('ETurnMode::SmoothTurn',0)]
assert enum(0x9d62ca0,4)==[('EPlayerMovementMode::Continuous',0),('EPlayerMovementMode::Teleport',1),
    ('EPlayerMovementMode::Hybrid',2),('EPlayerMovementMode::Count',3)]
assert enum(rela[0x9d4f580+32],3)==[('EActionTransitionsMode::FirstPerson',0),
    ('EActionTransitionsMode::ThirdPerson',1),('EActionTransitionsMode::Count',2)]
assert enum(0x9d5e8e8,3)==[('EHandedness::Left',0),('EHandedness::Right',1),('EHandedness::Count',2)]
assert enum(0x9d65b30,4)==[('ESteeringMethod::Head',0),('ESteeringMethod::Hand',1),
    ('ESteeringMethod::Fixed',2),('ESteeringMethod::Count',3)]
assert enum(0x9d58690,4)==[('EBoatControlMode::Immersive',0),('EBoatControlMode::Limited',1),
    ('EBoatControlMode::MotionSensitive',2),('EBoatControlMode::Count',3)]
assert enum(0x9d5a2b0,3)==[('ECombatStyle::RunAndGun',0),('ECombatStyle::Classic',1),('ECombatStyle::Count',2)]
assert enum(0x9d5dbf8,3)==[('EFirstPersonFacingMode::MaintainCurrent',0),('EFirstPersonFacingMode::MatchCharacter',1),('EFirstPersonFacingMode::Count',2)]
for address,offset,name in ((0x9db6000,0,'AvatarTurnMode'),(0x9db5808,0x48,'MovementMode'),
                           (0x9db5748,0x4a,'ActionsTransitionMode'),(0x9db5c58,0x33,'MovementStick'),
                           (0x9db55e0,0x4f,'SteeringMethod'),(0x9db56e8,0x4b,'BoatControlMode'),
                           (0x9db5950,0x3e,'CombatStyle'),(0x9db57a8,0x49,'FirstPersonFacing')):
    assert struct.unpack_from('<I',native,address+36)[0]==offset and string(rela[address])==name
for lo,hi,digest in (
    (0x65e7b0c,0x65e7b60,'050cf6bf16f1e82ed7208076ada740fbd85e910f617816a9058f043af171f2da'),
    (0x65eec38,0x65eec94,'002464346e5ea29bf45b61c4c6f1a9105effa68bc8e8b0efcca8c22672099b91'),
    (0x8c0d498,0x8c0d560,'14a980e256d6f52fa6f12637bb4edb6331fdda800576c5e690899f94688d3813'),
    (0x6347098,0x63475d8,'7f76a587ccabc5ae35255d53497d3be0c22f43f1806092bf8d3eac1167938c87'),
    (0x62165bc,0x6216a48,'89490e35b176958268fee22d55c1a8b78020458ecc0070614ca7c5bf1a9ebd88'),
    (0x643925c,0x6439268,'cc3771ced5b3a50081e6a2eb79027da7a89dcf208239684616a4d5110e43691b'),
    (0x6439178,0x6439184,'0a4198bada9681406f071f8c9a4a11da36069ae84701311fb19dd466f5f4944b'),
    (0x62c7dd4,0x62c7de4,'ac4d57ceb6825b6d03c83340397e7d97bfbb62f48bd9de989336689194852493'),
    (0x5fe1978,0x5fe1ae8,'dbab58af8895374efc928765f5cb0b312c76da32846b5a6adaf31de79267ebf7'),
    (0x62c80d4,0x62c838c,'4cd60f068fa43984f55be3342a314d97f25d11fcb2008ee590cf2bb7de68f824'),
    (0x62c7fd0,0x62c80d4,'18a2f876237cf83647f18162b550d19e95cd369524f2f21ce89af473945d9efc'),
    (0x62aa4fc,0x62aa9d4,'a9c8443e38fcc7b756abb0f58793408bc818c9afe218c1ea4580c4f24cb43532'),
    (0x621baac,0x621bbbc,'a1e614353c219a1b9981652298b5eb6b858113bc1fd3e6373ed499e982ea7f75'),
    (0x62faf3c,0x62faf5c,'d41ef5f3d74071dbb1fa57ae5d75900228e56c9c7fe0d88e2d413aa81d885475'),
    (0x62fabc0,0x62fae24,'957b7538760dff24527c3eb8ec3757a589f9878a66349d52dab63a3f2114a7cb'),
    (0x62c7d94,0x62c7db4,'f29ae071a1a61d400d39c76fe9c4df62f290e6507a2b75a516f851fa6862b021'),
    (0x621a120,0x621a174,'61542ba8d1c46fc5e7369932ef19dcb2047afb07643953bc7f25ac112a7849e4'),
    (0x621a174,0x621a294,'bd573d8175872fccb82623c18c72954440694bcad3a317f67619e5639e094083'),
    (0x621a378,0x621a4a8,'4ba45584eac0b4665b62eaccfa792282ec0cf8f927ec7c28eef7d5d7b6527e06'),
):assert hashlib.sha256(native[lo:hi]).hexdigest()==digest,hex(lo)
assert rela[0x9e20df8+0x88]==0x65eec38 and rela[0x9e20df8+0xa0]==0x65e7b0c
assert struct.unpack_from('<I',native,0x65eecb0)[0]==0xb9401816 # original float cvar flags +18
print('PASS:19 native ranges, named flat-control enums and reflected offsets; original float Set/Get ABI and deep Settings ownership',flush=True)

clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
core=(root/'analysis/re4fix_cinema.h').read_text(encoding='utf8')
guard=core[core.index('static int cinema_object('):core.index('static int cinema_on_game_thread(')]
with tempfile.TemporaryDirectory(prefix='re4-mobile-settings-') as temporary:
    work=Path(temporary)
    for kind,file in (('settings','mobile_settings'),('flat','flat_controls')):
        header=(root/f'analysis/re4fix_{file}.h').read_text(encoding='utf8')
        checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',header)
        assert len(checks)==(4 if kind=='settings' else 20)
        for a,x,y in checks:assert struct.unpack_from('<II',native,int(a,16))==(int(x,16),int(y,16)),a
        prefix='#include <string.h>\n#include <stdint.h>\n'+(guard if kind=='flat' else '')
        prefix+=f'unsigned long fixture_{kind}_code[]={{'+','.join(a for a,_,_ in checks)+'};\n'
        prefix+=f'unsigned fixture_{kind}_count={len(checks)};\n'
        prefix+=f'void fixture_{kind}_image(unsigned char* p){{\n'
        for a,x,y in checks:prefix+=f'*(unsigned*)(p+{a})={x};*(unsigned*)(p+{a}+4)={y};\n'
        if kind=='flat':
            for a,v in ((0x9db6000,0),(0x9db5808,0x48),(0x9db5748,0x4a),(0x9db5c58,0x33),(0x9db55e0,0x4f),(0x9db56e8,0x4b),(0x9db5950,0x3e),(0x9db57a8,0x49)):
                prefix+=f'*(unsigned*)(p+{a}+36)={v};\n'
            for a,n in ((0x9d5e8e8,3),(0x9d65b30,4),(0x9d58690,4),(0x9d5a2b0,3),(0x9d5dbf8,3)):
                for i in range(n):prefix+=f'*(int64_t*)(p+{a}+{i*16+8})={i};\n'
        prefix+='}\n'
        source=work/f'{kind}.c';source.write_text(prefix+(root/f'analysis/research/test_{file}.c').read_text(),encoding='utf8')
        subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-I',str(root/'analysis'),'-fsyntax-only',str(source)],check=True)
        obj,exe=work/f'{kind}.obj',work/f'{kind}.exe'
        subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-I',str(root/'analysis'),
            '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
            '-isystem',str(sdk/'Include/10.0.26100.0/um'),'-isystem',str(sdk/'Include/10.0.26100.0/shared'),
            '-c','-o',str(obj),str(source)],check=True)
        subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
            'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
            f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
        subprocess.run([str(exe)],check=True)
    java=list((root/'analysis/android').rglob('*.java'))
    subprocess.run(['javac','--release','8','-encoding','UTF-8','-classpath',
        os.path.join(os.environ['ANDROID_SDK_ROOT'], 'platforms/android-36/android.jar'),
        '-d',str(work/'java'),*map(str,java)],check=True)
print('PASS:actual settings/flat adapters ARM64 Werror and host execution; standalone Android settings compiles with original Files integration',flush=True)
