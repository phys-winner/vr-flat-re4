"""Read-only original native MenuHand action/key/capture proof."""
from pathlib import Path
import hashlib
import struct

root=Path(__file__).resolve().parents[2]
b=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
for lo,hi,digest in (
    (0x6310f50,0x6311258,'58b0f236ddf6f52b9decf4828b5c1751a78ec8b9902df2c33781b7c444d887c1'),
    (0x6310630,0x631067c,'53a15d359687bf0342527d0541b64f7a63b06137dd7f9c8b6a8e52dc0107597c'),
    (0x633c4a8,0x633c560,'4447a5f2c030aa4ec0b8b311684de732294ae83dcd591113bceff2a21af8e92d'),
    (0x6375538,0x6375f20,'2f90f417e6222c526d142db2ea376896ae9e30221764b22c1c0c8ad1814d9959'),
    (0x6376b74,0x6376d44,'6462a8fd89129be40e5f111deb57b8260b0f11619a39c777dbcbef06dcc08f23'),
    (0x63113c0,0x63113c8,'2b59db05426e0f313d72b30f3398c65721de2562a59dc46ceeba224f98d0b876'),
    (0x629c7a0,0x629c84c,'c78839eae456a1075b780c8aa5dad2f14cd12c47f3a7c4a9ba1208397efc44a6'),
    (0x63769bc,0x6376b74,'ee535968114d9dadd9b62b6a6d03aaca57fcc31c3d5ec7a0671ba76c09a1c90e'),
):
    assert hashlib.sha256(b[lo:hi]).hexdigest()==digest,hex(lo)
for caller,target in ((0x6310f64,0x6375538),(0x6310f74,0x6310f78),
                      (0x6311070,0x6376b74),(0x6311080,0x6375538),
                      (0x63110c0,0x6375f20),(0x63110d0,0x63110d4),
                      (0x63111cc,0x6375f20),(0x6311208,0x629c7a0),
                      (0x631121c,0x6375538),(0x6311230,0x629c84c),
                      (0x6311244,0x6375f20),(0x631124c,0x6375538),
                      (0x6311254,0x6375f20),(0x6310640,0x6298468),
                      (0x6310650,0x6310320),(0x629c848,0x629acec)):
    op=struct.unpack_from('<I',b,caller)[0]
    assert op&0x7c000000==0x14000000,hex(caller)
    imm=op&0x3ffffff
    if imm&0x2000000:imm-=0x4000000
    assert caller+4*imm==target,hex(caller)
for address,words in (
    (0x6310f5c,(0x52800081,0xaa0003f3)),
    (0x6311060,(0x2a1f03e3,0xa9047fff)),
    (0x6311210,(0x52800041,0xaa1303e0)),
    (0x6311238,(0x52800041,0xaa1303e0)),
    (0x6311248,(0x52800021,0x140190bb)),
    (0x6376d0c,(0x3944212a,0x6b35015f)),
    (0x647766c,(0x52800028,0x3914d008)),
    (0x63769bc,(0xf81b0ff9,0xa9015ff8)),
    (0x64da960,(0xd10183ff,0xf90023f3)),
    (0x64d8eec,(0xd10183ff,0xf90023f3)),
    (0x6376b58,(0x9104e129,0xf9000a88)),
):
    assert struct.unpack_from('<II',b,address)==words,hex(address)
for metadata,offset in ((0x9e129d8,0xb0),(0x9e0ffd8,8),(0x9e0ffa8,0x10),
                        (0x9e0ff78,0x28),(0x9e0ff48,0x40),(0x9e10228,0),
                        (0x9ddce08,0x510),(0x9e10370,0),(0x9e10340,8),(0x9e10310,0x10),
                        (0x9e10098,0),(0x9e10070,0xc),(0x9e10ed8,0x308),
                        (0x9e10ea8,0x310),(0x9e0fb28,0x320)):
    assert struct.unpack_from('<I',b,metadata+0x24)[0]==offset,hex(metadata)
assert struct.unpack_from('<I',b,0x9e129d8+0x20)[0]==2

shoff=struct.unpack_from('<Q',b,40)[0]
shsize,shnum=struct.unpack_from('<HH',b,58)
keys={0xa360620:0xa725bd0,0xa35ff28:0xa7276b8,0xa35ff30:0xa727508,
      0xa35ff38:0xa727688,0xa35ff40:0xa7274d8}
rel={}
for i in range(shnum):
    s=struct.unpack_from('<IIQQQQIIQQ',b,shoff+i*shsize)
    if s[1]!=4:continue
    for a,info,addend in struct.iter_unpack('<QQq',memoryview(b)[s[4]:s[4]+s[5]]):
        if a in keys or 0x9e0fec0<=a<=0x9e0ff30:
            assert info&0xffffffff==1027
            rel[a]=addend
assert rel[0x9e0ff10+32]==0x9e0fec0
assert struct.unpack_from('<I',b,0x9e0ff10+40)[0]==5
for value,name in enumerate(('Click','Cancel','Grip','Face1','Trigger')):
    address=0x9e0fec0+value*16
    start=rel[address]
    assert b[start:b.index(b'\0',start)]==f'EVR4UIAction::{name}'.encode()
    assert struct.unpack_from('<q',b,address+8)[0]==value
for a,target in keys.items():assert rel[a]==target,hex(a)
symbols=(root/'analysis/ue4_syms.txt').read_text().splitlines()
for address,name in (
    (0xa725bd0,'EKeys::LeftMouseButton'),
    (0xa7276b8,'EKeys::OculusTouch_Right_Trigger_Click'),
    (0xa727508,'EKeys::OculusTouch_Left_Trigger_Click'),
    (0xa727688,'EKeys::OculusTouch_Right_Grip_Click'),
    (0xa7274d8,'EKeys::OculusTouch_Left_Grip_Click'),
):
    assert f'{address:016x} B {name}' in symbols,name
print('PASS: 8 original native ranges, 16 exact normal calls/branches, 11 opcode pairs, 15 fields and hand-state dimension2')
print('PASS: original EVR4UIAction five names/values, widget MouseLeft and left/right Trigger/Grip FKey GOT identities')
print('PASS: single per-hand PressingObject action108 check and 24/48-byte native output-copy body')
print('PASS: HoveringObject 24-byte object/16-byte hit, native matching-owner/hover Actor gates and typed UIButton/UIComponent field proof')
print('LIMIT: read-only mapping/capture proof; no production change or diagnosis from a held-mask snapshot alone')
