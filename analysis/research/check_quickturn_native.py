"""Read-only proof of the original analog quick-turn lifecycle; no game calls."""
from pathlib import Path
import hashlib
import re
import struct

root=Path(__file__).resolve().parents[2]
b=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
ranges=(
    (0x6346b18,0x6346ff0,'c459e0841a698f97ac32525995e25ba99033746cf89ede74d9efe9740c713e6b'),
    (0x6346ff0,0x6347098,'1a70dd4677a4dda8f8fe8321a0f12ad96c2ddf3e974a0b71ed8467a7c91cb731'),
    (0x6347098,0x63475d8,'7f76a587ccabc5ae35255d53497d3be0c22f43f1806092bf8d3eac1167938c87'),
    (0x6345444,0x6345494,'b0060fb32437fd6c9854bd6cf0df0a6b548a74117cc7be3a4410f468ba53d684'),
    (0x6345bd0,0x6345bd8,'d2ffb971d1ca67a57c6b45e714eeee8d5fa4fa72eeb8f4fff89f89f4a94d890b'),
    (0x621ae8c,0x621af28,'b9f33e819e43fec83a8307928621d17ede371bd40422f7a8b443bc0a12f58260'),
    (0x621b7bc,0x621b848,'280cdeaa98aee68da4f1722f6234bbdfc518b99c09c3d01d23cc5b697da793be'),
    (0x63469fc,0x6346ae8,'dd5bfce49afa6ad3c819ad6ee949f0cafb38471125c05c0bd3ccf4943564a556'),
    (0x6346ae8,0x6346b18,'9e2db8250eb379be7b41233bcfe3d9a549eeeaf3d9f5edef04f5b66b7b82ee80'),
    (0x633fc48,0x633fce0,'4a0cf8632dbb40421a6f3bc2d1442081238ed1fb33188d1cadd7098c52101f29'),
    (0x6345494,0x6345bd0,'1b1cd81bf44d1e7a2b376baf5e46e8884e38796aeb0fd299634283bde511cd2e'),
    (0x621af28,0x621b7bc,'fe18b4d710fd660ced501084df587f84ffdc85636ddb462ee6014f21bfd8b96a'),
)
for lo,hi,digest in ranges:
    assert hashlib.sha256(b[lo:hi]).hexdigest()==digest,hex(lo)
def words(a,*expected):
    assert struct.unpack_from('<'+'I'*len(expected),b,a)==expected,hex(a)
def branch(a,target):
    word=struct.unpack_from('<I',b,a)[0]
    assert word&0x7c000000==0x14000000,hex(a)
    delta=word&0x3ffffff
    if delta&0x2000000:delta-=0x4000000
    assert a+4*delta==target,(hex(a),hex(target))
for a,target in (
    (0x621af54,0x62ace2c),(0x62ace3c,0x6346ff0),(0x6347018,0x6346b18),
    (0x621b844,0x6347098),(0x634724c,0x6265844),(0x6347274,0x64b5b50),
    (0x634546c,0x61c4030),(0x6345474,0x6345494),(0x6345484,0x64b5ad8),
    (0x6345bd4,0x64b5b14),(0x6345b80,0x63411cc),
    (0x621ae9c,0x63469fc),(0x621aea4,0x61d401c),(0x621aeac,0x61d0934),
):branch(a,target)
words(0x63470d0,0x394a5268)  # pawn AllowQuickTurn +294
words(0x63470dc,0x39402288)  # setting EnableQuickTurn +8
words(0x63470e4,0xbd428a60,0xbd4002a1,0x1e214000,0x1e202020,0x54000cc5)
words(0x6347108,0xbd455260,0xbd429a61,0x1e282800,0x1e212000,0xbd055260,0x54001eeb)
words(0x6347270,0x790aaa68)
words(0x634728c,0x52800028,0xb905527f,0x39155268)
words(0x6345464,0x52a86688,0x1e270100)  # float180 before FromDegrees
words(0x6346e40,0x3940ce88)  # MovementStick +33
words(0x6346fc0,0x4ea81d00,0x4ea91d21)  # actual output HFA Y,X
words(0x634704c,0xf9440d08,0xd63f0100)  # virtual CanUpdateViewRotation818
words(0x6347068,0xf9441908,0xd63f0100)  # virtual UpdateViewRotation830
words(0x6345458,0xf9440908,0xd63f0100)  # real permission rechecked at fade-rise
words(0x6345b90,0xf9441d08,0xd63f0100)  # authored rotation consumed838
words(0x633fc98,0x390a527f)  # base AllowQuickTurn defaults false; BP may override
words(0x643925c,0x52800028,0x39002008)  # generated settings boolean metadata setter
words(0x64b720c,0x52800028,0x390a5008)  # generated pawn boolean metadata setter
words(0x62c7cf4,0x52802028,0x79001268)  # native setting constructor true
assert abs(struct.unpack_from('<f',b,0x3fbf050+12)[0]-.7)<1e-6
assert all(abs(a-z)<1e-6 for a,z in zip(struct.unpack_from('<4f',b,0x3fb8e40),(.25,.10,.05,.10)))
assert struct.unpack_from('<I',b,0x9dffae8+0x24)[0]==0x298
assert struct.unpack_from('<I',b,0x9db5c58+0x24)[0]==0x33

relocs={a:(info,addend) for a,info,addend in struct.iter_unpack('<QQq',b[0x2e6c028:0x2e6c028+0x1146540])}
def rela(a,target):assert relocs[a]==(1027,target),(hex(a),hex(target))
for a,target in ((0xa35ff18,0xa727718),(0xa35ff20,0xa727568),(0xa35ff08,0xa727700),(0xa35ff10,0xa727550),
                 (0x9d3cfe8+0x810,0x63469fc),(0x9cffa58+0x810,0x621ae8c),
                 (0x9cffa58+0x818,0x6346ae0),(0x9cffa58+0x830,0x621b7bc),(0x9cffa58+0x838,0x621b848)):
    rela(a,target)
symbols={}
for line in (root/'analysis/ue4_syms.txt').read_text().splitlines():
    match=re.match(r'([0-9a-f]+) \w (.*)',line)
    if match:symbols.setdefault(int(match[1],16),set()).add(match[2])
for address,name in ((0xa727718,'EKeys::OculusTouch_Right_Thumbstick_Y'),(0xa727568,'EKeys::OculusTouch_Left_Thumbstick_Y'),
                     (0xa727700,'EKeys::OculusTouch_Right_Thumbstick_X'),(0xa727550,'EKeys::OculusTouch_Left_Thumbstick_X')):
    assert name in symbols[address]
source=(root/'analysis/re4fix_input.h').read_text()
assert 'f[10] = g_cmd_rstick[0]; f[11] = g_cmd_rstick[1];' in source
assert 'g_cmd_lstick_until = g_cmd_rstick_until = 0;' in source
print('PASS: 12 complete native ranges, 14 stock caller branches and actual analog Y,X HFA/quick-turn gates')
print('PASS: four actual EKeys RELA targets; five permission/rotation virtual slots; 180-degree fade/anchor lifecycle')
print('PASS: native defaults/metadata, EnableQuickTurn constructor/setter and existing right-stick/focus-reset publication')
print('LIMIT: read-only proof only; runtime BP AllowQuickTurn and actual turn acceptance require root hardware measurement')
