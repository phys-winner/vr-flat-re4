"""Bounded original body/input proof and supplied p245 free-space displacement."""
from decimal import Decimal
from pathlib import Path
import hashlib
import math
import re
import struct

root=Path(__file__).resolve().parents[2]
b=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
for lo,hi,digest in (
    (0x62aa4fc,0x62aa9d4,'a9c8443e38fcc7b756abb0f58793408bc818c9afe218c1ea4580c4f24cb43532'),
    (0x62ace2c,0x62ace78,'86462fb0d8cfb8f18ed45ee08f16fbfb6abd570b32e7a28ab399033ead60e251'),
    (0x6340474,0x63404d8,'db8d2c4021f9662ebd4462bdcd940c66c0617a1e3e41fac0ccda75616d223688'),
    (0x6346ff0,0x6347098,'1a70dd4677a4dda8f8fe8321a0f12ad96c2ddf3e974a0b71ed8467a7c91cb731'),
):
    assert hashlib.sha256(b[lo:hi]).hexdigest()==digest,hex(lo)
for caller,target in ((0x62ace3c,0x6346ff0),(0x62ace44,0x62aa4fc),
                      (0x6340494,0x634189c),(0x63404a4,0x66df968),(0x63404ac,0x61c4030)):
    op=struct.unpack_from('<I',b,caller)[0];assert op&0xfc000000==0x94000000
    immediate=op&0x3ffffff
    if immediate&0x2000000:immediate-=0x4000000
    assert caller+4*immediate==target,hex(caller)
assert struct.unpack_from('<I',b,0x63404b0)[0]==0xbd04b260
log=(root/'analysis/research/p245_clearspace_motion.log').read_text()
samples=[]
for time in ('904.804','906.304'):
    m=re.search(re.escape(time)+r'[^\n]*AIM head p=([-0-9.,]+) q=([-0-9.,]+)',log)
    assert m,time
    position=tuple(Decimal(v) for v in m[1].split(','))
    quaternion=tuple(float(v) for v in m[2].split(','))
    assert len(position)==3 and quaternion==(0.0,0.0,0.0,1.0)
    samples.append(position)
delta=tuple(q-p for p,q in zip(*samples))
assert delta==(Decimal('419.261'),Decimal('0.000'),Decimal('13.726'))
heading=math.degrees(math.atan2(float(delta[1]),float(delta[0])))
assert heading==0.0
print('PASS: 4 original native body/input ranges, 5 call targets and native target-local-yaw store')
print('PASS: supplied p245 free-space native head identity at both endpoints; dx419.261/dy0, horizontal heading0')
print('LIMIT: one clear-space heading, reported coordinate precision; no production change or new device execution')
