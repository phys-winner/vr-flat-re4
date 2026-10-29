"""Verify original mode/equip contracts and execute the actual QuickSelect adapter."""
import os
from pathlib import Path
import hashlib,re,struct,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
header=root/'analysis/re4fix_quickselect.h'
source=header.read_text(encoding='utf8')
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',source)
assert len(checks)==14
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
for a,w0,w1 in checks:assert struct.unpack_from('<II',native,int(a,16))==(int(w0,16),int(w1,16)),a
for a,o in ((0x9db59b0,0x3d),(0x9db0e60,0x270)):assert struct.unpack_from('<I',native,a+36)[0]==o
spans=(
 (0x5fe1978,0x5fe1ae8,'dbab58af8895374efc928765f5cb0b312c76da32846b5a6adaf31de79267ebf7'),
 (0x62c7e04,0x62c7fd0,'633fbaf676541869268d361cb736b2c700a53eb1bd7951a9cfd7050685291b90'),
 (0x62c7fd0,0x62c80d4,'18a2f876237cf83647f18162b550d19e95cd369524f2f21ce89af473945d9efc'),
 (0x62c80d4,0x62c838c,'4cd60f068fa43984f55be3342a314d97f25d11fcb2008ee590cf2bb7de68f824'),
 (0x62a7ad4,0x62a7b34,'3f51e4ff0cd1fe6355b83dd478a0a0bd2f42604aa4f9b23d8c23ad8264080832'),
 (0x62a7b34,0x62a7c08,'b20066f5caa4a30fcbecfb96cdef6219d87b17b2b8e91d565427a721aff7616c'),
 (0x62ab7fc,0x62ab934,'a8748b21a82c14bd904817b9308f5bdacc204254a1d48959fb08518f1d22b64e'),
 (0x63c767c,0x63c7764,'d64736471920ed549942c2936642fb8d4d00c28b69db00828c266146606ebce5'),
 (0x621c528,0x621c5e0,'b53706b2893e93d009459db5d28e5526d9730701e8da49c4ea002912944d6afa'),
 (0x62bc710,0x62bc7b0,'b620d1b30163bf61943e055c7589d10ab7778286aba241c7a42c42d2eedd8e9d'),
 (0x5f2b67c,0x5f2b6b4,'6907a5ebdb014c0db4e0866217164ed929c944553af0f779f44059b4c7fa49be'),
)
for a,b,digest in spans:assert hashlib.sha256(native[a:b]).hexdigest()==digest,hex(a)
# Native instruction callers proving the ABI, rather than inferred symbol names.
def branch(a):
    w=struct.unpack_from('<I',native,a)[0];assert w&0x7c000000==0x14000000
    d=w&0x3ffffff;d=d-0x4000000 if d&0x2000000 else d;return a+d*4
for a,b in ((0x6326c38,0x5fe1978),(0x6326c64,0x62c80d4),
            (0x62ab870,0x62bc710),(0x62ab904,0x62a7b34),(0x62ab914,0x62bbb1c)):
    assert branch(a)==b
# A primary Grip release calls the native Prop gate, whose IsPresentOnBody
# permits release only in WeaponSelectMode0; mode1 is the existing native retention.
for a,w in ((0x629c984,0xf9401508),(0x629c988,0xd63f0100),(0x629c98c,0x3607fb20),
            (0x62bc030,0xf9413a68),(0x62bc068,0xf9434101),
            (0x62bb860,0x3940f408),(0x62bb864,0x7100011f),(0x62bb868,0x1a9f17e0)):
    assert struct.unpack_from('<I',native,a)[0]==w,hex(a)
assert branch(0x629c998)==0x629b4f0 and branch(0x62bb85c)==0x62c7e04
assert 'TryGrabGrip' not in source and 'ReleaseCurrentGrip' not in source and 'InstantReload' not in source
assert 'copy.bytes[0x3d]=1;qs_set(&copy)' in source and 'qs_destroy(&copy)' in source
assert source.count('qs_equip(pawn,')==1 and 'qs_allowed(pawn)' in source
binding=source[source.index('static int qs_binding('):source.index('static int qs_can_equip(')]
assert 'qs_context' not in binding
assert source.count('qs_context(objects[1])')==3
assert 'if(allowed && qs_context(objects[1]) && qs_can_equip(objects[1]))' in source
print('PASS:14 native preimages, Settings0xb8/WeaponSelectMode3d/PrimaryGrip270 and five original callsites',flush=True)
weapon=(root/'analysis/re4fix_weapon.h').read_text(encoding='utf8')
start=weapon.index('static int tw_typed(');typed=weapon[start:weapon.index('\n}',start)+2]
input_source=(root/'analysis/re4fix_input.h').read_text(encoding='utf8')
start=input_source.index('static unsigned ctl_bit(')
fire_binding=input_source[start:input_source.index('\n}',start)+2]
assert 'state.selected!=g_fire_slot || state.fire_side!=g_fire_side' in fire_binding
world=(root/'analysis/re4fix_world_touch.h').read_text(encoding='utf8')
world_grab=world[world.index('static struct { void* hand; void* grip; long off_at; } wt_grab;'):world.index('static int wt_range_grab(')]
assert 'if (now < wt_grab.off_at) {touch_quickselect_world_retention(hand);return;}' in world_grab
test=(root/'analysis/research/test_quickselect.c').read_text(encoding='utf8').replace('/* SHARED_TYPED */',typed).replace('/* FIRE_BINDING */',fire_binding).replace('/* WORLD_GRAB_TICK */',world_grab).replace('QUICKSELECT_HEADER',header.as_posix())
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-quickselect-') as tmp:
    d=Path(tmp);c=d/'test.c';c.write_text(test,encoding='utf8')
    fixture=d/'native_fixture.h'
    fixture.write_text(r'''
__declspec(dllimport) void* __stdcall VirtualAlloc(void*,size_t,unsigned long,unsigned long);
__declspec(dllimport) int __stdcall VirtualFree(void*,size_t,unsigned long);
static void verify_quickselect_install_guard(void){
 unsigned char* image=VirtualAlloc((void*)0x20000000,0xa000000,0x3000,4);assert(image);
 static const struct{unsigned a,w[2];} code[]={
'''+''.join('{%s,{%s,%s}},\n'%row for row in checks)+r'''};
 for(unsigned i=0;i<sizeof code/sizeof *code;i++)memcpy(image+code[i].a,code[i].w,8);
 *(unsigned*)(image+0x9db59b0+36)=0x3d;*(unsigned*)(image+0x9db0e60+36)=0x270;
 unsigned long base=(unsigned long)(uintptr_t)image;
 tw_hand_class=(void* (*)(void))(base+0x6424b5c);tw_pawn_class=(void* (*)(void))(base+0x642c718);
 tw_grip_class=(void* (*)(void))(base+0x64192f4);ti_prop_class=(void* (*)(void))(base+0x6432600);
 ti_parent=(void* (*)(const void*))(base+0x6280170);phone_player=(void* (*)(const void*))(base+0x633d030);
 phone_active=(void* (*)(void))(base+0x63380dc);phone_type=(unsigned (*)(void))(base+0x6338aec);
 rl_primary=(void* (*)(const void*))(base+0x62bbc30);f_GetCurrentGrip=(void* (*)(const void*))(base+0x629a774);
 rl_paused=(int (*)(void))(base+0x61d0934);rl_player_valid=(int (*)(void))(base+0x61d401c);
 install_touch_quickselect(base);assert(qs_ready);
 for(unsigned i=0;i<sizeof code/sizeof *code;i++)for(unsigned j=0;j<2;j++){
  *(unsigned*)(image+code[i].a+j*4)^=1;install_touch_quickselect(base);assert(!qs_ready);
  *(unsigned*)(image+code[i].a+j*4)^=1;
 }
 *(unsigned*)(image+0x9db59b0+36)^=1;install_touch_quickselect(base);assert(!qs_ready);*(unsigned*)(image+0x9db59b0+36)^=1;
 *(unsigned*)(image+0x9db0e60+36)^=1;install_touch_quickselect(base);assert(!qs_ready);*(unsigned*)(image+0x9db0e60+36)^=1;
 phone_type=0;install_touch_quickselect(base);assert(!qs_ready);phone_type=(unsigned (*)(void))(base+0x6338aec);
 ti_parent=0;install_touch_quickselect(base);assert(!qs_ready);ti_parent=(void* (*)(const void*))(base+0x6280170);
 install_touch_quickselect(base);assert(qs_ready);assert(VirtualFree(image,0,0x8000));
 puts("PASS: actual installer28 changed instruction words/two metadata/dependency corruption refusals");
}
''',encoding='utf8')
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(c)],check=True)
    obj,exe=d/'test.obj',d/'test.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-DQUICKSELECT_NATIVE_FIXTURE="'+fixture.as_posix()+'"','-c','-o',str(obj),str(c)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: Android aarch64 Werror and executed actual helper with shared typed ancestry',flush=True)
