"""Verify actual world pickup/door/save route evidence without invoking the game."""
from pathlib import Path
import hashlib
import struct

root=Path(__file__).resolve().parents[2]
b=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
ranges=(
    (0x625dc20,0x625dfc4,'0c1b0324d2fe93e7c317a625d5dc30718d784658ee9a362558ff742a72110a26'),
    (0x625e428,0x625e4f8,'865b0d6b8335321ca6dd9ce672638ea0cfbb2c05d61aefee08d8c54151a60285'),
    (0x625e4f8,0x625e6b4,'ff14cd2fe675d9130d2d1f1ebc22717e6c03dd44a5ed6082a71f5e86a1cb84f3'),
    (0x62eb7dc,0x62ebc38,'1ac7aa4f2d5c79604b6fb357c74df5de8ff4d34afe63bf72dbeffe0605007219'),
    (0x62ebeb4,0x62ec504,'79bf8d09355758dd53e176a90b9ec91472e0c81fb018542c1d831ac7b3298255'),
    (0x62ec678,0x62ec894,'e78c8cce86dfb7231b3d96b75b2a897b1ce9a38d21427ab0b48ffe03d5142d37'),
    (0x62ec8f0,0x62ec9a8,'40f2edaf7aa9ff213228ee94e22a41dce3e554d7f26b1df9ab02bae940ed7def'),
    (0x62ec9c8,0x62ecc20,'ce313311469882c4d78bf4dab0068bc107d904d786deb2a82dc40d320f3df543'),
    (0x6458c6c,0x6458ce4,'90322095f102d59ff439148a5de2cd255aedc0aec7309c619d0fcfa16013de6a'),
    (0x640bddc,0x640be54,'9d2293ad1fd7aacdf66a903769b8eee2496f95a64f87c25cd8f0c1849377859e'),
    (0x5f1fc9c,0x5f1fed0,'e22f08b3168585de7daf0de4ebc8e21cac29b3c82fe56e4f1aff45bc8fa79540'),
    (0x62f0060,0x62f0130,'b15c2fa34c30bdc657b6b782e520da366004afbd6a3c707c0030f5347ea29440'),
    (0x623ba4c,0x623bab4,'d7c26d57e136308ec1c961b1e475518b61f51dd114253a845f56124055f51ad9'),
    (0x615bf14,0x615bf7c,'89e6d287e66a61b3094987d483cfc4eaef889ebd992384572077ca3a588e0ee4'),
    (0x625ea50,0x625ea58,'27c77efd4e49dfee759126d4fe6e6a8d6338af8827fb7bef944aaa7f8939d31b'),
)
for lo,hi,digest in ranges:assert hashlib.sha256(b[lo:hi]).hexdigest()==digest,hex(lo)
def branch(a,target):
    w=struct.unpack_from('<I',b,a)[0];assert w&0x7c000000==0x14000000,hex(a)
    d=w&0x3ffffff
    if d&0x2000000:d-=0x4000000
    assert a+4*d==target,(hex(a),hex(target))
for a,target in (
    (0x625dc54,0x61d401c),(0x625dd70,0x63380dc),(0x625e4a8,0x5e9b4c0),
    (0x625e5b4,0x5e9c1d4),(0x625ea54,0x625e4f8),
    (0x62eb800,0x62cdcf0),(0x62eb984,0x6283240),(0x62eb9e8,0x6283438),
    (0x62ebf50,0x6458c6c),(0x62ec444,0x62ec678),
    (0x62ec978,0x62ec9c8),(0x62ecbbc,0x62ec678),
    (0x62ec78c,0x6153664),(0x62ec888,0x5f2886c),
    (0x62f00dc,0x62e3198),(0x62f0100,0x5ffcdc8),
    (0x615bf4c,0x5f1fe34),(0x5f1fe78,0x5f1fcf0),(0x5f1fd50,0x6358d10),
):branch(a,target)
def words(a,*expected):
    assert struct.unpack_from('<'+'I'*len(expected),b,a)==expected,hex(a)
words(0x62ebf38,0x39532408)  # native already-collected/state gate4c9
words(0x62ebf44,0x39532a68)  # held state gate4ca
words(0x62ec3ec,0x394a4a68)  # authored can-add flag292
words(0x62ec698,0x39537008)  # native duplicate inventory-add guard4dc
# Original grip release invokes Release(collect=true) only for its own active,
# held, not-yet-added pickup; Release then calls the original AddToInventory.
words(0x62ec944,0x39532688,0x340001a8)  # primary-grabbed flag4c9
words(0x62ec94c,0x39532a88,0x34000168)  # actual held flag4ca
words(0x62ec95c,0x39537288,0x7100011f,0x1a9f17e1) # collect iff duplicate guard4dc==0
words(0x62ecbb4,0x36000075,0xaa1303e0,0x97fffeaf) # native bool gate -> AddToInventory
words(0x6458cac,0xf9410d08,0xd63f0100)  # BP CanPickupItem stays real ProcessEvent
words(0x640be1c,0xf9410d08,0xd63f0100)  # CallSaveGame is likewise BP wrapper
words(0x615bf44,0xb9408000,0x52800021)  # actual scene save args, not generic UI confirmation
words(0x625df68,0x97fd9bcc,0x1e2b2000,0x54fffc41)  # native door-side rejection
for a,name in ((0x40ef9a4,'PrimaryGripGrabbed'),(0x3fe42b2,'PrimaryGripReleased')):
    encoded=(name+'\0').encode('utf-16-le')
    assert b[a:a+len(encoded)]==encoded,hex(a)
for a,offset in ((0x9dcb308,0x260),(0x9dcb218,0x288),(0x9dcaa48,0x3d8),(0x9dcaa18,0x3e8)):
    assert struct.unpack_from('<I',b,a+0x24)[0]==offset,hex(a)
relocs={a:(info,addend) for a,info,addend in struct.iter_unpack('<QQq',b[0x2e6c028:0x2e6c028+0x1146540])}
for a,target in ((0x9dca418,0x643eecc),(0x9d2cc08,0x623ba4c),(0x9d2ccc8,0x623bb9c),
                 (0x9d2cc40,0x62ebeb4),(0x9d2cc68,0x62eff60)):
    assert relocs[a]==(1027,target),(hex(a),hex(target))
assert '439.582 [16170] INTERACT no eligible nearby world grip' in (root/'analysis/research/p245_world_runtime.log').read_text()
print('PASS: 15 original native body ranges; 19 ordinary caller branches; BP/state/capacity/door-side gates and native release-to-inventory lifecycle')
print('PASS: real pickup grab/release UTF16 bindings, four metadata offsets and five parent/interface/callback RELAs')
print('PASS: original scene CardSave/menu prelude plus actual p245 no-eligible-world-grip evidence')
print('LIMIT: no direct world/save calls; physical typewriter trigger and hardware door/pickup/save acceptance remain unproven')
