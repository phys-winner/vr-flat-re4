"""Original native pistol shot direction proof; no device or native execution."""
from pathlib import Path
import hashlib
import math
import random
import struct


def check_native_fire_axis(native):
    # Complete native bodies preserve the scalar register/data flow used below.
    ranges = (
        (0x6290d8c,0x62912f0,'0ebcdf61b71817f8548ca72bc165ff3efea5d3d42c9994b23362929a0804d3d5'),
        (0x61cc6b0,0x61cc7c4,'63f4737a220b199e18bfc0a4c0992c756cd03a32b72ab48c1d4c28bfb559b9a1'),
        (0x5fbe48c,0x5fbe53c,'a0c2f69467767a4edaa4050088fb860bd76efb953905bd5c1437df63292f2fdb'),
        (0x5d35494,0x5d354fc,'8de116c20f1273c8c73dc04b8051c13dbc8f5668e78524ccbe9c54af42ae0df8'),
    )
    for lo,hi,expected in ranges:
        assert hashlib.sha256(native[lo:hi]).hexdigest()==expected,hex(lo)
    for metadata,offset in ((0x9da41f8,0x620),(0x9da4220,0x618),(0x9da41d0,0x628),(0x9da4248,0x610)):
        assert struct.unpack_from('<I',native,metadata+36)[0]==offset,hex(metadata)
    assert struct.unpack_from('<6I',native,0x6380de8)==(
        0xad408042,0x3dc00041,0x3900c001,0xad008002,0x3d800001,0xd65f03c0)
    assert struct.unpack_from('<3I',native,0x61cb920)==(0x2d408402,0xbd400000,0xd65f03c0)
    assert struct.unpack_from('<II',native,0x83c6310)==(0xd10643ff,0xfd0083ee)
    assert struct.unpack_from('<I',native,0x83c6344)[0]==0xaa0803f3  # FTransform sret x8.
    assert struct.unpack_from('<I',native,0x83c634c)[0]==0xaa0103f7  # FName64 in x1.
    shoff=struct.unpack_from('<Q',native,40)[0];stride,count=struct.unpack_from('<HH',native,58)
    slots={0xa241b98+16+0x430,0xa2497c8+16+0x430};found=set()
    for i in range(count):
        sh=struct.unpack_from('<IIQQQQIIQQ',native,shoff+i*stride)
        if sh[1]!=4:continue
        for pos in range(sh[4],sh[4]+sh[5],sh[9]):
            offset,kind,target=struct.unpack_from('<QQq',native,pos)
            if offset in slots:
                assert kind&0xffffffff==1027 and target==0x83c6310
                found.add(offset)
    assert found==slots  # SkeletalMesh/SkinnedMesh native virtual socket getter.
    for caller,target in ((0x5fbe494,0x6290d8c),(0x5fbe4a0,0x61cc6b0),(0x5fbe4f4,0x5d35494),(0x5fbe538,0x5fdf884)):
        op=struct.unpack_from('<I',native,caller)[0]
        assert op&0xfc000000==0x94000000
        immediate=op&0x03ffffff
        if immediate&0x02000000:immediate-=0x04000000
        assert caller+4*immediate==target,hex(caller)
    # MOV/MOVK builds 0xc7435000, stored as the legacy shot X before MTXMultVecSR.
    assert struct.unpack_from('<II',native,0x5fbe4c0)==(0x528a0008,0x72b8e868)
    assert struct.unpack('<f',struct.pack('<I',0xc7435000))[0]==-50000
    assert struct.unpack_from('<I',native,0x5fbe4cc)[0]==0xb90003e8
    rng=random.Random(6290)
    for _ in range(256):
        q=[rng.uniform(-1,1) for _ in range(4)]
        norm=math.sqrt(sum(v*v for v in q));x,y,z,w=(v/norm for v in q)
        sy=rng.uniform(.2,4)
        # BoneTransformToMtx output column0 from 61cc748/774/778/784/788/794/798.
        column0=(-sy*(2*x*y-2*z*w),-sy*(2*y*z+2*x*w),-sy*(1-2*x*x-2*z*z))
        legacy=tuple(-50000*v for v in column0)
        ue=(legacy[0],legacy[2],legacy[1])  # DirToVector swaps legacy Y/Z.
        actual=tuple(v/(50000*sy) for v in ue)
        # Independent quaternion-vector rotation for FireBone local +Y.
        uv=(-z,0,x);uuv=(y*x,-z*z-x*x,y*z)
        expected=(2*w*uv[0]+2*uuv[0],1+2*w*uv[1]+2*uuv[1],2*w*uv[2]+2*uuv[2])
        assert max(abs(a-b) for a,b in zip(actual,expected))<1e-12
    return 256


if __name__=='__main__':
    root=Path(__file__).resolve().parents[2]
    count=check_native_fire_axis((root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes())
    print(f'PASS: native FireBone+620, wrapper TF48/type48, raw socket FName64/sret/RELA, actual handgun call chain and {count} independent +Y shot direction checks')
