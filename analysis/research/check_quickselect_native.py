"""Read-only reproduction of QuickSelect/reload/owned Settings native evidence."""
from pathlib import Path
import hashlib
import struct

root=Path(__file__).resolve().parents[2]
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
checks={
    0x62c7e04:(0xa9be4ff4,0xa9017bfd),
    0x5fe1978:(0xf81c0ff7,0xa90157f6),
    0x62c80d4:(0xd105c3ff,0xf90093fc),
    0x62c7fd0:(0xa9bc5ff8,0xa90157f6),
    0x6326bfc:(0xd10403ff,0xa90c5ff8),
    0x630f27c:(0xa9bd57f6,0xa9014ff4),
    0x649a1d8:(0xd10403ff,0xa90c5ff8),
    0x62ab7fc:(0xa9bd57f6,0xa9014ff4),
    0x62a7ad4:(0x12001c28,0x7100251f),
    0x62bc710:(0xaa0003e8,0xf941e800),
    0x63c767c:(0xd100c3ff,0xa9014ff4),
    0x621c528:(0xf81e0ff3,0xa9017bfd),
    0x62bbb1c:(0xd101c3ff,0xa90457f6),
    0x62e145c:(0xf81d0ff5,0xa9014ff4),
    0x62e12e4:(0xd10103ff,0xa90157f6),
    0x628e25c:(0xa9be4ff4,0xa9017bfd),
    0x62b60f8:(0xf81e0ff3,0xa9017bfd),
    0x62ca018:(0xa9be4ff4,0xa9017bfd),
    0x6273574:(0xd107c3ff,0xa91c57fc),
    0x6277ffc:(0xd10243ff,0xfd0023e8),
    0x6278384:(0xd10183ff,0xa9025ff8),
    0x62e15c0:(0xd10203ff,0xf90023f7),
    0x62e78e8:(0xd10243ff,0xf90023f9),
    0x62791fc:(0xf902d001,0xd65f03c0),
    0x5fb76f4:(0xd0021d48,0xf9474508),
}
for address,words in checks.items():
    assert struct.unpack_from('<II',native,address)==words,hex(address)
assert struct.unpack_from('<I',native,0x64390b0)[0]==0x52801703  # Settings size w3=0xb8.

# Resolve original generated enum tables through ELF RELA, not zero raw pointers.
qword=lambda address:struct.unpack_from('<Q',native,address)[0]
relocations={a:(info,addend) for a,info,addend in struct.iter_unpack(
    '<QQq',native[0x2e6c028:0x2e6c028+0x1146540])}
def pointer(address):
    if address not in relocations:
        return qword(address)
    info,addend=relocations[address]
    kind=info&0xffffffff
    if kind==1027:
        return addend
    assert kind in (257,1025,1026)
    return addend+qword(0x330+(info>>32)*24+8)
def cstring(address):
    return native[address:native.index(0,address)].decode('ascii')
def enum(params):
    count=struct.unpack_from('<I',native,params+40)[0]
    entries=pointer(params+32)
    return [(cstring(pointer(entries+i*16)),struct.unpack_from('<q',native,entries+i*16+8)[0])
            for i in range(count)]
assert enum(0x9d636f8)==[(f'EPropSlot::{name}',value) for value,name in enumerate(
    ('Gun1','Gun2','Grenade','Knife','Ammo1','Ammo2','Consumable','Flashlight','ScriptedGun','ScriptedAmmo','Count'))]
assert enum(0x9e1a8c0)==[(f'EWeaponSelectMode::{name}',value) for value,name in enumerate(('BodySelect','QuickSelect','Count'))]
assert enum(0x9d5a2e0)==[(f'ECombatStyle::{name}',value) for value,name in enumerate(('RunAndGun','Classic','Count'))]
assert enum(0x9d63968)==[(f'EReloadStyle::{name}',value) for value,name in enumerate(('Default','Manual','Count'))]

# Verify actual stock callers for the transaction, equip and inventory consumption.
def branch(address):
    word=struct.unpack_from('<I',native,address)[0]
    assert word&0x7c000000==0x14000000
    displacement=word&0x3ffffff
    if displacement&0x2000000:
        displacement-=0x4000000
    return address+4*displacement
calls={
    0x6326c38:0x5fe1978,0x6326c58:0x630f27c,0x6326c64:0x62c80d4,0x6326c74:0x649a1d8,
    0x62ab870:0x62bc710,0x62ab904:0x62a7b34,0x62ab914:0x62bbb1c,
    0x62a7874:0x62ab7fc,0x62a7960:0x62ab7fc,
    0x62b59e8:0x628fed0,0x62b59f8:0x6290128,0x62b5a04:0x628e1c4,
    0x62c9098:0x6290128,0x62c90b0:0x628e1c4,
    0x62780f8:0x6278384,0x627842c:0x5f334c4,0x62e796c:0x5f334c4,
}
for address,target in calls.items():
    assert branch(address)==target,(hex(address),hex(target))
slot_calls=[]
for address in range(0x61b0000,0x6500000,4):
    word=struct.unpack_from('<I',native,address)[0]
    if (word&0xffc00000)==0xf9400000 and ((word>>10)&0xfff)==0xff:
        register=word&31
        following=struct.unpack_from('<8I',native,address+4)
        if any((candidate&0xfffffc1f)==0xd63f0000 and ((candidate>>5)&31)==register for candidate in following):
            slot_calls.append(address)
assert slot_calls==[0x6272798,0x6273608,0x629b1d0,0x629b908,0x64b469c]
print(f'PASS: original native hash, {len(checks)} preimages, Settings size0xb8, four enum tables and {len(calls)} actual callers')
print('PASS: register-independent virtual slot7f8 scan: two Ammo->Gun completion calls and three Pawn hand-transform calls; no other call claimed')
