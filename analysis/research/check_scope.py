"""Execute the actual scope-eye geometry helper; original native scope keeps its lifecycle."""
import os
from pathlib import Path
import hashlib,re,struct,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
header=root/'analysis/re4fix_scope.h';source=header.read_text(encoding='utf8')
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',source);assert len(checks)==11
fields=re.findall(r'\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}',source[source.index('fields[]='):source.index('as_ready=0;')]);assert len(fields)==5
for a,x,y in checks:assert struct.unpack_from('<II',native,int(a,16))==(int(x,16),int(y,16)),a
for a,x in fields:assert struct.unpack_from('<I',native,int(a,16)+36)[0]==int(x,16),a
for a,x in ((0x9db40a0,0),(0x9db4070,4),(0x9db4040,8),(0x9db4010,12)):
    assert struct.unpack_from('<I',native,a+36)[0]==x
assert struct.unpack_from('<II',native,0x62c6024)==(0xf9011260,0xf9009a60) # original ScopeRoot==ActorRoot.
assert struct.unpack_from('<II',native,0x61c404c)==(0xbd400000,0xd65f03c0) # FAngle is literal degrees.
def branch(a):
    w=struct.unpack_from('<I',native,a)[0];assert w&0x7c000000==0x14000000
    d=w&0x3ffffff;d=d-0x4000000 if d&0x2000000 else d;return a+d*4
for a,b in ((0x62c5d04,0x61c4ab0),(0x62c5d10,0x6341aec),(0x62c6248,0x61ca820),
            (0x62c6260,0x83faeb8),(0x62c627c,0x62c5cb8),(0x62c6320,0x6437530),(0x62c6398,0x643756c)):
    assert branch(a)==b
assert not re.search(r'\(base\+0x(?:62c646c|6437614|62c68fc)\)',source)
main=(root/'analysis/re4fix.c').read_text(encoding='utf8');reload=(root/'analysis/re4fix_reload.h').read_text(encoding='utf8');aim=(root/'analysis/re4fix_aim.h').read_text(encoding='utf8')
math=main[main.index('static qf_t q_mul('):main.index('static float g_eye_height')]
for text,sigs in ((reload,('static int reload_tf_valid(','static int reload_marker_valid(','static int reload_is_a(','static int reload_layout(')),
                  (aim,('static struct reload_tf aim_relative_marker(','static struct reload_tf aim_world_marker('))):
    for sig in sigs:
        a=text.index(sig);math+=text[a:text.index('\n}',a)+2]+'\n'
test=(root/'analysis/research/test_scope.c').read_text(encoding='utf8').replace('/* SHARED_MATH */',math).replace('SCOPE_HEADER',header.as_posix())
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231');sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-scope-') as tmp:
    d=Path(tmp);c=d/'test.c';c.write_text(test,encoding='utf8')
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(c)],check=True)
    obj,exe=d/'test.obj',d/'test.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-c','-o',str(obj),str(c)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: original ELF hash/11 opcode pairs/nine reflected fields/scalar transform/native FAngle degrees/seven stock scope callers; Android aarch64 Werror',flush=True)
