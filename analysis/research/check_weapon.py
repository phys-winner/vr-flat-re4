"""Check native layout dependencies and run the actual pointer-only weapon helper."""
import os
from pathlib import Path
import mmap
import hashlib
import re
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
header = root / "analysis/re4fix_weapon.h"
clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
checks = re.findall(r"\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}", header.read_text(encoding="utf-8"))
assert len(checks) == 20
with (root / "apk_decoded/lib/arm64-v8a/libUE4.so").open("rb") as stream:
    with mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as native:
        assert native[:6] == b"\x7fELF\x02\x01"
        offset = struct.unpack_from("<Q",native,0x20)[0]
        stride,count = struct.unpack_from("<HH",native,0x36)
        segments = [struct.unpack_from("<IIQQQQQQ",native,offset+i*stride) for i in range(count)]
        for address,word0,word1 in checks:
            address = int(address,16)
            segment = next(s for s in segments if s[0] == 1 and s[3] <= address and address+8 <= s[3]+s[5])
            actual = struct.unpack_from("<II",native,segment[2]+address-segment[3])
            assert actual == (int(word0,16),int(word1,16)), hex(address)
        for a,off in ((0x9db0e60,0x270),(0x9db1dd8,0x350),(0x9db1e38,0x348),(0x9d9e298,0x300)):
            assert struct.unpack_from('<I',native,a+36)[0]==off
        for lo,hi,digest in (
            (0x6424b5c,0x6424c1c,'54a5ae25155cfb34f9a3e212ec907b8ac9072f8f6f422088ae057ce68e95a85e'),
            (0x642c718,0x642c7d8,'3a4c955e3886ff8f6eea64c9c226525c765f542228f1518be77b65e77f111353'),
            (0x64192f4,0x64193b4,'52bdf762b2f6bd84f413bf968cca1348bb4590163b75b88ca525c6be82bf6dbc'),
            (0x629a930,0x629ac74,'5f8c4fa8c485d485eddc6fb829a934a93251afa6e0b98bde9614e7e78ef95ec6'),
            (0x629c7a0,0x629c84c,'c78839eae456a1075b780c8aa5dad2f14cd12c47f3a7c4a9ba1208397efc44a6'),
            (0x62ac400,0x62ac4a0,'b31cbd2a4a795174dcbc0dfce4dfa15e12e5e61af56794f235bec532bcb60cb6'),
            (0x6277ffc,0x6278340,'e9d6fa63f0ee857ec403305327c637494cce8be2ee8768b8fe41e7201587c157'),
            (0x6278a38,0x6278b38,'9b01eca533087f372085c7cfd4406b0f1cefed50696358abb31dcde0fc805fc3'),
        ):assert hashlib.sha256(native[lo:hi]).hexdigest()==digest,hex(lo)
        assert struct.unpack_from('<I',native,0x62ac40c)[0]==0xf9476c13 # GetConsumable: pawn+ed8 = GetProp(6).
        assert 0xea8+6*8==0xed8
        assert struct.unpack_from('<I',native,0x6278a6c)[0]==0xf9427268 # real consumable mesh+4e0.
        assert struct.unpack_from('<I',native,0x6278128)[0]==0x940313c6 # held item uses normal hand trigger.
print("PASS: 20 native knife/typed Hand/Pawn/Grip/layout/owner preimages",flush=True)
print("PASS: original consumable6 actor, native visible mesh and trigger-owned use; no item consumption on selection",flush=True)

with tempfile.TemporaryDirectory(prefix="re4-weapon-syntax-") as temporary:
    stub = Path(temporary) / "weapon.c"
    stub.write_text("#include <string.h>\n#include <unistd.h>\n"
        "static void* (*phone_player)(const void*);\n"
        "static void* (*phone_active)(void);\n"
        "static void* (*rl_gun1)(const void*);\n"
        "static unsigned (*phone_type)(void);\n"
        "static void* (*f_GetCurrentGrip)(const void*);\n"
        "static void* g_mobile_holster,*g_mobile_holster_hand;\n"
        "static long g_mobile_holster_until;\n"
        "static volatile int g_touch_gt_tid,g_touch_cancel,g_touch_weapon_request,g_touch_weapon;\n"
        "static volatile int g_touch_reload_request,g_reload_phase;\n"
        "static void* (*ti_prop_class)(void),*(*ti_holster_class)(void),*(*ti_parent)(const void*),*(*ti_best)(const void*);\n"
        "static int (*ti_down)(const void*),(*ti_free)(const void*,int),(*rl_paused)(void),(*rl_player_valid)(void);\n"
        "static void (*ti_press)(void*),(*ti_release)(void*);\n"
        "static void* (*rl_primary)(const void*);\nstatic int ti_ready;\n"
        "static long ptr_now_ms(void){return 1000;}\n"
        "static int reload_grip_registered(void* p){return p!=0;}\n"
        "#define LOGF(...) ((void)0)\n"
        f'#include "{header.as_posix()}"\n'
        "void* check(void* h,int w,unsigned long b){install_touch_weapon(b);return touch_weapon_grip(h,w);}\n",
        encoding="utf-8")
    subprocess.run([str(clang),"--target=aarch64-linux-android23","-O2","-Werror","-fsyntax-only",str(stub)],check=True)
print("PASS: Android aarch64 weapon helper syntax",flush=True)
subprocess.run([str(clang),"--target=aarch64-linux-android23","-O2","-Werror","-fsyntax-only",
                str(root / "analysis/research/test_weapon.c")],check=True)

vc = Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231")
sdk = Path(r"C:\Program Files (x86)\Windows Kits\10")
with tempfile.TemporaryDirectory(prefix="re4-weapon-check-") as temporary:
    fixture=Path(temporary)/'native_fixture.h'
    fixture.write_text(r'''
__declspec(dllimport) void* __stdcall VirtualAlloc(void*,size_t,unsigned long,unsigned long);
__declspec(dllimport) int __stdcall VirtualFree(void*,size_t,unsigned long);
static void verify_weapon_install_guard(void) {
    unsigned char* image=VirtualAlloc((void*)0x20000000,0xa000000,0x3000,4);assert(image);
    static const struct {unsigned addr,words[2];} code[]={
'''+''.join('        {%s,{%s,%s}},\n'%row for row in checks)+r'''
    };
    static const struct {unsigned addr,offset;} fields[]={{0x9db0e60,0x270},{0x9db1dd8,0x350},{0x9db1e38,0x348},{0x9d9e298,0x300}};
    for(unsigned i=0;i<sizeof code/sizeof *code;i++)memcpy(image+code[i].addr,code[i].words,8);
    for(unsigned i=0;i<sizeof fields/sizeof *fields;i++)*(unsigned*)(image+fields[i].addr+36)=fields[i].offset;
    unsigned long base=(unsigned long)(uintptr_t)image;
    ti_ready=1;ti_best=(void* (*)(const void*))(base+0x629a930);ti_press=(void (*)(void*))(base+0x633c54c);
    ti_release=(void (*)(void*))(base+0x633c558);ti_down=(int (*)(const void*))(base+0x633d048);
    ti_free=(int (*)(const void*,int))(base+0x627feb8);ti_parent=(void* (*)(const void*))(base+0x6280170);
    ti_prop_class=(void* (*)(void))(base+0x6432600);ti_holster_class=(void* (*)(void))(base+0x6434184);
    rl_primary=(void* (*)(const void*))(base+0x62bbc30);rl_paused=(int (*)(void))(base+0x61d0934);
    rl_player_valid=(int (*)(void))(base+0x61d401c);f_GetCurrentGrip=(void* (*)(const void*))(base+0x629a774);
    install_touch_weapon(base);assert(tw_hand_class && tw_pawn_class && tw_grip_class);
    for(unsigned i=0;i<sizeof code/sizeof *code;i++){
        *(unsigned*)(image+code[i].addr)^=1;install_touch_weapon(base);assert(!tw_hand_class);
        *(unsigned*)(image+code[i].addr)^=1;
    }
    for(unsigned i=0;i<sizeof fields/sizeof *fields;i++){
        *(unsigned*)(image+fields[i].addr+36)^=1;install_touch_weapon(base);assert(!tw_hand_class);
        *(unsigned*)(image+fields[i].addr+36)^=1;
    }
    ti_ready=0;install_touch_weapon(base);assert(!tw_hand_class);ti_ready=1;
    ti_press=0;install_touch_weapon(base);assert(!tw_hand_class);ti_press=(void (*)(void*))(base+0x633c54c);
    ti_parent=0;install_touch_weapon(base);assert(!tw_hand_class);ti_parent=(void* (*)(const void*))(base+0x6280170);
    f_GetCurrentGrip=0;install_touch_weapon(base);assert(!tw_hand_class);f_GetCurrentGrip=(void* (*)(const void*))(base+0x629a774);
    install_touch_weapon(base);assert(tw_hand_class && tw_grip_class);assert(VirtualFree(image,0,0x8000));
    puts("PASS: actual weapon installer rejects17 code/four field corruptions and missing input/getter dependencies before typed publication");
}
''',encoding='utf8')
    obj,exe = Path(temporary) / "weapon.obj",Path(temporary) / "weapon.exe"
    subprocess.run([str(clang),"--target=x86_64-pc-windows-msvc","-O2","-Werror",
        "-Wno-int-to-pointer-cast","-Wno-int-to-void-pointer-cast",
        "-isystem",str(vc / "include"),"-isystem",str(sdk / "Include/10.0.26100.0/ucrt"),
        '-DWEAPON_NATIVE_FIXTURE="'+fixture.as_posix()+'"',
        "-c","-o",str(obj),str(root / "analysis/research/test_weapon.c")],check=True)
    subprocess.run([str(vc / "bin/Hostx64/x64/link.exe"),"/NOLOGO",f"/OUT:{exe}",str(obj),
        "libcmt.lib","libucrt.lib","libvcruntime.lib","oldnames.lib","kernel32.lib",
        f"/LIBPATH:{vc / 'lib/x64'}",f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/ucrt/x64'}",
        f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/um/x64'}"],check=True)
    subprocess.run([str(exe)],check=True)
