"""Verify original native HP/prompt contracts and execute the actual HUD helper C."""
import os
from pathlib import Path
import hashlib
import re
import struct
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
header=root/'analysis/re4fix_hud_state.h'
b=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
for lo,hi,digest in (
    (0x621a8f8,0x621a968,'582617811b6f4860443d739b85e3654c539e4ae23baa85a5bc15b0850a0e13ea'),
    (0x63c6e58,0x63c6ec0,'eb8339090418cfbbfd39a83cb74a60660d12a4f6023640a65b4b0ee50a85d2f4'),
    (0x621984c,0x62198b0,'e58cd6917c5cb46a4bd9e4301a8385d4aa79466d3a7c3e4b3b3020a243d68388'),
    (0x644cb84,0x644cc44,'819fa767dc7e292dc6d8336b239951565f6c7e4ee3d4a7ca75f410acd6edb58a'),
    (0x6424b5c,0x6424c18,'3aafcb1185beb13ecda61b321875b0a2d6407481bbc4caa0096a8cb1a37c1146'),
    (0x62da068,0x62da130,'7d90c8a5807bd8be9a5512fff8cb89dfc0fc22c035ac649c4bba03e2c294ac03'),
): assert hashlib.sha256(b[lo:hi]).hexdigest()==digest,hex(lo)
def branch(addr,target):
    w=struct.unpack_from('<I',b,addr)[0];assert w&0xfc000000==0x94000000
    d=w&0x3ffffff
    if d&0x2000000:d-=0x4000000
    assert addr+d*4==target,hex(addr)
for addr,target in ((0x621a900,0x61d401c),(0x621a938,0x61d401c),
                    (0x63c6eac,0x621a8f8),(0x63c6e78,0x621a930),
                    (0x621985c,0x6338760),(0x6219868,0x63c8a5c)):
    branch(addr,target)
# GetPlayerCurrent/MaxHealth are static int getters, not this/HFA methods.
for addr,value in ((0x621a910,0x5040),(0x621a948,0x5042)):
    w=struct.unpack_from('<I',b,addr)[0]
    assert w&0xff80001f==0x52800009 and (w>>5)&0xffff==value
for addr in (0x621a918,0x621a950):
    assert struct.unpack_from('<I',b,addr)[0]==0x78e96900  # LDRSH w0,[x8,x9]
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',header.read_text())
assert len(checks)==13
original=[]
for addr,w1,w2 in checks:
    row=int(addr,16),int(w1,16),int(w2,16)
    assert struct.unpack_from('<II',b,row[0])==row[1:]
    original.append(row)
# Check the entire authored semantic enum; production additionally checks names
# used by the mobile semantic categories in the shared interact installer.
rel={a:add for a,info,add in struct.iter_unpack('<QQq',b[0x2e6c028:0x2e6c028+0x1146540]) if info==1027}
assert rel[0x9d4f170+32]==0x9d4ecf0
assert struct.unpack_from('<I',b,0x9d4f170+40)[0]==72
expected=('Talk Check JumpOut JumpIn JumpDown JumpOver Push Kick GoUp GetDown KnockDown Stand JumpAt Look Peep Sniping Open Swim JumpBack Stoop Operation Rescue RideShoulder SearchAttack Sprint Climb Jump SlideDown Catch PullUp WaitOther SeparateOther HideOther FollowOther HelpOther Ride GetOff Guard Hide Through PickUp QuickStick Rotate Resist Suplex HeliOrder Shooting Save NoDisp Accelerate Accele ThroughOther SitDown ThrowDown Answer Sneak Senpuu BackKick PoisonNeedle Execute PalmShock Nerichagi JumpMove PushSwitch GetDown1M Fire Operation2 Wire TryAgain GoAshley Hint Any').split()
assert len(expected)==72
for value,name in enumerate(expected):
    addr=0x9d4ecf0+16*value;p=rel[addr]
    assert struct.unpack_from('<q',b,addr+8)[0]==value
    assert b[p:b.index(b'\0',p)]==('EActionButtonType::'+name).encode()
for meta,off in ((0x9dbf850,0x3d9),(0x9dbf7f0,0x3da)):
    assert struct.unpack_from('<I',b,meta+36)[0]==off
print('PASS: six complete native ranges,13 HUD preimages, static signed-short->int HP getters/native BP wrappers, typed Bio4/Hand510 ownership',flush=True)
print('PASS: entire relocated72-entry semantic enum (Kick7/Suplex44), CurrentCommand3d9/Action3da native metadata',flush=True)

clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-hud-check-') as temporary:
    t=Path(temporary);fixture=t/'native_fixture.h'
    fixture.write_text(r'''
__declspec(dllimport) void* __stdcall VirtualAlloc(void*,size_t,unsigned long,unsigned long);
__declspec(dllimport) int __stdcall VirtualFree(void*,size_t,unsigned long);
static void verify_hud_install_guard(void) {
    unsigned char* memory=VirtualAlloc((void*)0x20000000,0xa000000,0x3000,4);
    assert(memory && (uintptr_t)memory<0x100000000ULL);
    static const struct { unsigned addr,words[2]; } original[]={
'''+'\n'.join('        {0x%x,{0x%x,0x%x}},'%row for row in original)+r'''
    };
    for(unsigned i=0;i<sizeof original/sizeof *original;i++) memcpy(memory+original[i].addr,original[i].words,8);
    unsigned long base=(unsigned long)(uintptr_t)memory;
    ti_ready=1; phone_player=(void* (*)(const void*))(base+0x633d030);
    phone_active=(void* (*)(void))(base+0x63380dc); phone_type=(unsigned (*)(void))(base+0x6338aec);
    rl_paused=(int (*)(void))(base+0x61d0934); rl_player_valid=(int (*)(void))(base+0x61d401c);
    hs_current=0;install_touch_hud_state(base);assert(hs_current && hs_max && hs_hand_class && hs_bio4);
    for(unsigned i=0;i<sizeof original/sizeof *original;i++) {
        hs_current=0;*(unsigned*)(memory+original[i].addr)^=1;
        install_touch_hud_state(base);assert(!hs_current);
        *(unsigned*)(memory+original[i].addr)^=1;
    }
    ti_ready=0;install_touch_hud_state(base);assert(!hs_current);ti_ready=1;
    phone_player=0;install_touch_hud_state(base);assert(!hs_current);phone_player=(void* (*)(const void*))(base+0x633d030);
    phone_active=0;install_touch_hud_state(base);assert(!hs_current);phone_active=(void* (*)(void))(base+0x63380dc);
    phone_type=0;install_touch_hud_state(base);assert(!hs_current);phone_type=(unsigned (*)(void))(base+0x6338aec);
    rl_paused=0;install_touch_hud_state(base);assert(!hs_current);rl_paused=(int (*)(void))(base+0x61d0934);
    rl_player_valid=0;install_touch_hud_state(base);assert(!hs_current);rl_player_valid=(int (*)(void))(base+0x61d401c);
    install_touch_hud_state(base);assert(hs_current);
    assert(VirtualFree(memory,0,0x8000));
    puts("PASS: actual HUD installer, every13 preimage corruption and six dependency corruptions decline");
}
''',encoding='utf8')
    source=root/'analysis/research/test_hud_state.c'
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(source)],check=True)
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast','-isystem',str(vc/'include'),
        '-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-DHUD_NATIVE_FIXTURE="'+fixture.as_posix()+'"',
        '-c','-o',str(t/'hud.obj'),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{t/"hud.exe"}',str(t/'hud.obj'),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(t/'hud.exe')],check=True)
