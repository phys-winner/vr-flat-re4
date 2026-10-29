"""Read-only original native config -> backbuffer proof; no renderer/device mutations."""
from pathlib import Path
import hashlib,re,struct
root=Path(__file__).resolve().parents[2]
b=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
for lo,hi,digest in (
    (0x758a0c8,0x758a118,'f981226054b04a67e56769a9ed8da699a4728b1743fa65ba882e7f6bf1c317cb'),
    (0x8a8834c,0x8a8836c,'23452125fccf9d86b9e0246e6e8a2ca2c19605056c043d0ad1a9890cb6187dfd'),
    (0x8a8838c,0x8a883ac,'6a4b50e210b1d213e456e1321bdbd669d9d58f405b1b999137607850269b375f'),
    (0x671b774,0x671b8ac,'60cc27f81c13821ee39ff902a22bf08171e2672f13c4bf41d6ff54f0e1453a51'),
    (0x65ee9f0,0x65eea54,'6bf77bfee40b159da2c45e7802b70d40963749d1c3eb617e3a0a8cc02316ecd0'),
    (0x65e7d18,0x65e7e8c,'38dc6d4302f12bd27fefb170af275e7b9343efe208f088948cf917f4addd8ee3'),
    (0x655c8f8,0x655ca4c,'c0bf5234f318bfbc9351c6dfa8bb9fd4901795522055914ac215ac6359b677ed'),
    (0x652b198,0x652b2cc,'abf2149017d3b8dbaaf9a5d82b4688bcd4bf60d675dca6c06269451b3866a75e'),
    (0x54c2480,0x54c54a0,'9251b4d08c04b0f1294e385d570b0c7360de2ba7478398fd62a1ea12e8c20e41'),
    (0x54c5ae0,0x54c61e0,'f222addef6af7ff50f76550be8f674e46e25aa0f8419e3d9a51de478aeed3db8'),
    (0x64f61a4,0x64f71c0,'c355fbc877d363d32fedc37e8b24b0706d1470bf417a29c59cf9855b5f5eb796'),
    (0x6542f44,0x65438e4,'a4866f112d97c4ced48c2714ad4117f82ac7b7933f0791bc7858cfebcc13813d'),
    (0x655a130,0x655a680,'7079140c6e294648b2271e0e69d9daf60a368e6d401db376b1d5b80cf20dbe81'),
):
    assert hashlib.sha256(b[lo:hi]).hexdigest()==digest,hex(lo)
def word(address): return struct.unpack_from('<I',b,address)[0]
def call(address,target):
    w=word(address);assert w&0xfc000000==0x94000000
    imm=w&0x3ffffff;imm=imm-0x4000000 if imm&0x2000000 else imm
    assert address+imm*4==target,hex(address)
def utf16(address,value):
    encoded=value.encode('utf-16-le')+b'\0\0';assert b[address:address+len(encoded)]==encoded
utf16(0x4039f78,'r.DefaultBackBufferPixelFormat')
utf16(0x414062a,'/Script/Engine.RendererSettings')
assert (word(0x758a0f0),word(0x758a0f4))==(0x52800082,0x52800084) # default4/flags4
assert struct.unpack_from('<5I',b,0x44aeb54)==(0,0,3,3,4) # FromInt
assert struct.unpack_from('<5I',b,0x44aeb40)==(2,2,10,10,18) # Convert2PixelFormat
utf16(0x41be69c,'B8G8R8A8');utf16(0x4167e5a,'A2B10G10R10');utf16(0x419c572,'FloatRGBA')
assert word(0xa413290+2*40+36)==2 and word(0xa413290+18*40+36)==18
call(0x54c3fcc,0x54c5ae0);call(0x54c5c34,0x67168bc) # AppInit initializes config
call(0x54c4094,0x671993c);call(0x54c4c28,0x6ed4148) # project settings before RHIInit
assert word(0x54c407c)==0x52a06002 # project ini priority03000000
assert word(0x671b828)==0x360002f6 # config tests cheat bit0, not low readonly bit2
assert word(0x65e7d3c)==0x12081c39 and word(0x65e7d48)==0x12081ed8 # priority mask only
call(0x655c97c,0x8a8838c);call(0x655c980,0x8a8834c)
assert word(0x6533d74)==0x2a0103e0 # Vulkan inherited pixel-format hint preserves requested PF
assert word(0x64f6244)==0x5280058a and word(0x64f625c)==0xb9006e88 # PF2 platform44
assert word(0x64f6988)==0x531a6508 and word(0x64f6994)==0xb902ee88 # supported PF18 platform64
assert 0x652b1d4+b[0x430f237+44-9]*4==0x652b20c and word(0x652b20c)==0x52800640 #44 sRGB->50
assert 0x652b1d4+b[0x430f237+64-9]*4==0x652b2c8 and word(0x652b2c8)==0xd65f03c0 #64 unchanged
assert word(0x6543188)==0x6b09019f and word(0x654318c)==0x54000bc0 # platform44 exact match ->3304
assert word(0x6543248)==0xb900033f and word(0x65432ac)==0xb900032d # fallback clears/updates caller PF ref
assert word(0x655a4fc)==0x52800129 and word(0x655a520)==0xa90023e9 # window flags9, no SRGB bit10
call(0x655a524,0x6549c90)
original=(root/'analysis/research/graphics-evidence/VR4/Config/DefaultEngine.ini').read_bytes()
assert hashlib.sha256(original).hexdigest()=='8d8f154078f8781d800ee428ef58d038130dbc6b8604ccd97748f0163beedbae'
assert b'r.DefaultBackBufferPixelFormat=' not in original
font=(root/'analysis/research/p256_cinema_font_runtime.log').read_text(encoding='utf-8-sig')
assert "field='SubtitleText'" in font and "material=0x0 '' class='' outline=0x0" in font
assert 'rgba=(1,1,1,1) rule=0' in font and 'opacity=1 rootOpacity=1' in font
runtime=(root/'analysis/logs/p256_probe.log').read_text(encoding='utf-8-sig')
mismatch=re.findall(r'FBSW[^\n]*att\[0\]=swapchain#([0-3]) viewfmt=50[^\n]*rpfmt\[0\]=64 MISMATCH',runtime)
assert set(mismatch)==set('0123')
p257=(root/'analysis/logs/p257_accept.log').read_text(encoding='utf-8-sig')
fallback=re.findall(r'FBSW[^\n]*att\[0\]=swapchain#([0-3]) viewfmt=50[^\n]*rpfmt\[0\]=37 MISMATCH',p257)
assert set(fallback)==set('0123')
p258=(root/'analysis/logs/p258_accept.log').read_text(encoding='utf-8-sig')
linear=re.findall(r'FBSW[^\n]*att\[0\]=swapchain#([0-3]) viewfmt=44[^\n]*rpfmt\[0\]=44',p258)
scene=re.findall(r'FBSW[^\n]*att\[1\]=swapchain#([0-3]) viewfmt=50[^\n]*rpfmt\[1\]=50',p258)
assert set(linear)==set(scene)==set('0123')
assert 'req=44 real=43' in p258 and 'MISMATCH' not in p258
assert not re.search(r'comp on=\d+ dis=\d+[^\n]*fail=[1-9]',p258)
print('PASS: 13 original native ranges; project config before RHI and readonly-compatible priority; CVar0->PF2 BGRA8, native surface platform44 matching/fallback PF update, window flags9/noSRGB; p256/p257 incompatible and p258 legal Slate44/scene50 attachments on all4 proxies (p258 visual color regression remains unaccepted)')
