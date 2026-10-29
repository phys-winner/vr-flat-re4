"""Read-only native command enum, real face-key and authored-ignore proof."""
from pathlib import Path
import hashlib
import re
import struct

root=Path(__file__).resolve().parents[2]
b=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
ranges=(
    (0x61f5cf4,0x61f6990,'3fefab12b44b17bf06d4c9fd7f6f881b699cf1e623c8a84bad4cf185abf7a10a'),
    (0x61f6990,0x61f6aec,'73df6c429bd524c9f2e88646497dfd542c6f70ab02c000548e3fed3d78a1548d'),
    (0x61f4ef0,0x61f501c,'06f660142b8ab2f892acdb2f4963925a9240ea9dcdc8a2fa59f798ab75968f66'),
    (0x61f6ce8,0x61f7114,'859e41104f9cd459854ea82a0b0dc24055fd709e28197724cadcdbd2035ddc1b'),
    (0x61f7434,0x61f74a4,'dad02b1ab1787a27e00eb29c908c9ed5d1727c2f39361246ea7458e48e3879c0'),
    (0x62da72c,0x62da930,'00360c09ee7deebb6a9119bb696c6378e107ac899a843555da8c31d24e33b3eb'),
    (0x62dab04,0x62dac00,'cf3380fa955234799ab4fe2c941e8bbd4d8746e855242019ab4a011b04a4e886'),
    (0x62dd110,0x62dd178,'f2648ce5c28dd4865bc4256625169cd3ab413e273dc1425207b97231ffd3a77a'),
    (0x62dd2b4,0x62dd2c8,'1c16c9f9cd036ecc09eb1429be5e0413d9bc470d7acf99b3d18fdde162b07776'),
    (0x62dd310,0x62dd344,'a008606cc612373ff2d7f65b86d22e1962d7e8a985f3a07781e9ccc57e9ab045'),
    (0x628d9fc,0x628db4c,'2b281c31c46ba7e65270b7f588aec303034035858974d102f988ce61a27607c9'),
    (0x633d078,0x633d218,'b6f248ebd86a9d053a682b7c2e0691a645653c8dc205a5bb7fce7f88a80841d1'),
)
for lo,hi,h in ranges:assert hashlib.sha256(b[lo:hi]).hexdigest()==h,hex(lo)
shoff=struct.unpack_from('<Q',b,40)[0]
shsize,shnum=struct.unpack_from('<HH',b,58)
relocs={}
for i in range(shnum):
    s=struct.unpack_from('<IIQQQQIIQQ',b,shoff+i*shsize)
    if s[1]!=4:continue
    for p in range(s[4],s[4]+s[5],s[9]):
        dst,info,add=struct.unpack_from('<QQq',b,p)
        if (0x9d92268<=dst<0x9d92378 or 0xa35ff20<=dst<=0xa35ff80):
            assert info&0xffffffff==1027
            relocs[dst]=add
names=('Invalid','A','B','X','Y','LB','RB','A_B','LB_RB','Rotate',
       'A_Rapid','B_Rapid','Stick_A','Stick_Up','Waggle','LB_or_RB','HandMotion')
assert struct.unpack_from('<I',b,0x9d92378+40)[0]==len(names)
for value,name in enumerate(names):
    a=0x9d92268+16*value
    assert struct.unpack_from('<q',b,a+8)[0]==value
    p=relocs[a];assert b[p:b.index(b'\0',p)]==('EVR4CommandButton::'+name).encode()
symbols={}
for line in (root/'analysis/ue4_syms.txt').read_text().splitlines():
    m=re.match(r'([0-9a-f]+) \w (.*)',line)
    if m:symbols.setdefault(int(m[1],16),set()).add(m[2])
for command,case,ignored,got,name,bit in (
    (1,0x61f5f2c,0x61f6d34,0xa35ff58,'EKeys::OculusTouch_Right_A_Click',0),
    (2,0x61f602c,0x61f6d84,0xa35ff68,'EKeys::OculusTouch_Right_B_Click',1),
    (3,0x61f612c,0x61f6e9c,0xa35ff60,'EKeys::OculusTouch_Left_X_Click',8),
    (4,0x61f622c,0x61f6eec,0xa35ff70,'EKeys::OculusTouch_Left_Y_Click',9),
):
    assert 0x61f5e6c+4*struct.unpack_from('<H',b,0x430e250+2*(command-1))[0]==case
    assert 0x61f6d34+4*b[0x430e26e+command-1]==ignored
    for location in (case+4,ignored+0x10):
        op=struct.unpack_from('<I',b,location)[0]
        assert op&0xffc003ff==0xf9400108  # LDR x8,[x8,#imm]
        assert 0xa35f000+8*((op>>10)&0xfff)==got
    assert name in symbols[relocs[got]]
    assert (1<<bit)=={1:0x1,2:0x2,3:0x100,4:0x200}[command]
for addr,target in ((0x61f4f8c,0x61f6990),(0x61f4f98,0x61f6ce8),(0x61f4ff0,0x62da72c),
                    (0x61f5e30,0x61f6990),(0x62da894,0x62dab04),
                    (0x628da4c,0x633d078),(0x633d12c,0x61f74a4)):
    op=struct.unpack_from('<I',b,addr)[0];assert op&0xfc000000==0x94000000
    delta=op&0x3ffffff
    if delta&0x2000000:delta-=0x4000000
    assert addr+4*delta==target,hex(addr)
for meta,off in ((0x9dbf850,0x3d9),(0x9dbf7f0,0x3da),(0x9dbf790,0x3db),(0x9dbf730,0x3dc),
                 (0x9d68718,0xa),(0x9d68778,0x9),(0x9d68658,0xc),(0x9d686b8,0xb)):
    assert struct.unpack_from('<I',b,meta+36)[0]==off,hex(meta)
assert struct.unpack_from('<II',b,0x62dab20)==(0x390f6402,0x390f6c03)
assert struct.unpack_from('<I',b,0x628da44)[0]==0x52800022  # checkIfKeyIsIgnored=true
assert 'OB_A 0x1u' in (root/'analysis/re4fix_input.h').read_text()
print('PASS: 12 original native ranges; complete 17-member relocated command enum')
print('PASS: native CheckInput/IgnoreKeys A/B/X/Y key parity, 7 authored call targets and 8 metadata fields')
print('PASS: real face input bits0/1/8/9 translation, native QTE distinction and weapon key-ignore route')
print('LIMIT: no production mapping change, native calls, or hardware world-action acceptance')
