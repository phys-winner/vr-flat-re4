"""Execute original widget cylinder vertex generation; keep the renderer and game immutable."""
import hashlib
import math
import struct
from pathlib import Path
from inspect_ui_package import package
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn import arm64_const as a64


def original_curved_vertices(native):
    directory=Path(__file__).resolve().parent/'user_render_originals'
    asset=(directory/'UI_ChapterSummary_BP.uasset').read_bytes();body=(directory/'UI_ChapterSummary_BP.uexp').read_bytes()
    assert hashlib.sha256(body).hexdigest()=='0539253be00e9a5de0d324994ccfd00b1b3a3aac6e5c541255a93e1b606d0fe3'
    metadata=package(asset,body);names=metadata['names']
    widget=next(export for export in metadata['exports'] if export['name']=='Widget_Unlocks_GEN_VARIABLE')
    assert widget['class_path']=='/Script/UMG.WidgetComponent'
    data=body[widget['uexp_offset']:widget['uexp_offset']+widget['serial_size']]
    for name,kind,expected,offset in (
        ('GeometryMode','EnumProperty',struct.pack('<II',names.index('EWidgetGeometryMode::Cylinder'),0),33),
        ('CylinderArcAngle','FloatProperty',struct.pack('<f',64.0761947631836),25),
        ('DrawSize','StructProperty',struct.pack('<2i',1920,1080),49),
        ('RelativeScale3D','StructProperty',struct.pack('<3f',.5,.5,.5),49),
    ):
        marker=struct.pack('<II',names.index(name),0);assert data.count(marker)==1
        at=data.index(marker);assert struct.unpack_from('<II',data,at+8)==(names.index(kind),0)
        assert struct.unpack_from('<I',data,at+16)[0]==len(expected) and data[at+offset:at+offset+len(expected)]==expected
    for stem,field,expected in (
        ('UI_ChapterSummary_NextButton_BP','DrawSize',struct.pack('<2i',540,220)),
        ('UI_ChapterSummary_NextButton_BP','CylinderArcAngle',struct.pack('<f',22)),
        ('UI_Button_BP','RelativeScale3D',struct.pack('<3f',.1,.1,.1)),
    ):
        source=directory if stem!='UI_Button_BP' else directory.parent/'action_ui_originals'
        meta=package((source/(stem+'.uasset')).read_bytes(),(source/(stem+'.uexp')).read_bytes())
        values=(source/(stem+'.uexp')).read_bytes();export=next(e for e in meta['exports'] if e['class_path']=='/Script/UMG.WidgetComponent')
        payload=values[export['uexp_offset']:export['uexp_offset']+export['serial_size']]
        marker=struct.pack('<II',meta['names'].index(field),0);assert payload.count(marker)==1
        at=payload.index(marker);offset=25 if field=='CylinderArcAngle' else 49
        assert struct.unpack_from('<I',payload,at+16)[0]==len(expected) and payload[at+offset:at+offset+len(expected)]==expected
    for start,size,digest in (
        (0x7db23d4,2076,'47fa053e78cad5527c9b905d1f22ee2ea5ba10d343b482b8dec28af584d06d67'),
        (0x7dad2dc,300,'95d79979b15b06ed0cf7bae857ea5d6b44ea4f1aa1466168c495096af9b748c1'),
        (0x7e4a544,28,'08defd8b34ab07a2fb8cca27d0c7199f4b1f018a52f37ec8f7d81fe96779b57e'),
    ):
        assert hashlib.sha256(native[start:start+size]).hexdigest()==digest
    assert struct.unpack_from('<I',native,0x9f77670+36)[0]==0x51c
    assert struct.unpack_from('<4I',native,0x7dad248)==(0xbd451e80,0x529f46a8,0x72a791c8,0x1e270101)
    u=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
    calls=(0x8858808,0x8858924,0x8858970,0x8858dd8,0x88592a0,0x9bf3440)
    for page in sorted({0x7db2000,0x7dad000,0xa355000}|{address&~4095 for address in calls}):
        u.mem_map(page,4096);u.mem_write(page,native[page:page+4096])
    memory,stop=0x11000000,0x70000000
    u.mem_map(memory,0x50000);u.mem_map(stop,0x2000)
    proxy,render,material,uniform,views,array,view,collector,table1,table2,component=[memory+i*4096 for i in range(11)]
    sine=lambda value:struct.pack('<f',math.sin(value))
    cosine=lambda value:struct.pack('<f',math.cos(value))
    points=[];triangles=[]

    def callback(uc,address,size,user):
        if address==0x9bf3440:
            angle=struct.unpack('<f',struct.pack('<I',uc.reg_read(a64.UC_ARM64_REG_S0)))[0]
            uc.mem_write(uc.reg_read(a64.UC_ARM64_REG_X0),sine(angle))
            uc.mem_write(uc.reg_read(a64.UC_ARM64_REG_X1),cosine(angle))
        elif address==0x8858970:
            points.append(struct.unpack('<3f',uc.mem_read(uc.reg_read(a64.UC_ARM64_REG_X1),12)))
            uc.reg_write(a64.UC_ARM64_REG_X0,len(points)-1)
        elif address==0x8858dd8:
            triangle=tuple(uc.reg_read(register) for register in (a64.UC_ARM64_REG_W1,a64.UC_ARM64_REG_W2,a64.UC_ARM64_REG_W3))
            assert all(0<=index<len(points) for index in triangle);triangles.append(triangle)
        elif address in (stop+0x1000,stop+0x1100):
            uc.reg_write(a64.UC_ARM64_REG_X0,0)
        elif address not in calls:
            return
        uc.reg_write(a64.UC_ARM64_REG_PC,uc.reg_read(a64.UC_ARM64_REG_X30))

    u.hook_add(UC_HOOK_CODE,callback)
    result=[]
    for degrees in (1,20,22,64.0761947631836,90,180,270,360):
        for pivot in ((0,.15),(.5,.5),(1,.85)):
            u.mem_write(memory,bytes(0x50000));points.clear();triangles.clear()
            u.mem_write(component+0x51c,struct.pack('<f',degrees));u.mem_write(component+0x45c,struct.pack('<2f',*pivot))
            u.mem_write(component+0x518,b'\1')
            for register,value in ((a64.UC_ARM64_REG_X19,proxy),(a64.UC_ARM64_REG_X20,component),
                (a64.UC_ARM64_REG_X21,render),(a64.UC_ARM64_REG_X22,component+0x45c)):
                u.reg_write(register,value)
            # Original CreateSceneProxy copies degrees->radians, pivot and the cylinder enum.
            u.emu_start(0x7dad248,0x7dad284,count=30)
            arc=struct.unpack('<f',u.mem_read(proxy+0x14c,4))[0]
            assert abs(arc-degrees*math.pi/180)<.000001 and bytes(u.mem_read(proxy+0x179,1))==b'\1'
            u.mem_write(proxy+0x160,struct.pack('<QQ',render,material));u.mem_write(proxy+0xf8,struct.pack('<Q',uniform))
            u.mem_write(render+0x78,struct.pack('<Q',1));u.mem_write(render+0xc0,struct.pack('<2i',1920,1080))
            u.mem_write(material,struct.pack('<Q',table1));u.mem_write(table1+0x2b0,struct.pack('<Q',stop+0x1000))
            u.mem_write(uniform,struct.pack('<Q',table2));u.mem_write(table2+0x48,struct.pack('<Q',stop+0x1100))
            u.mem_write(views,struct.pack('<Qii',array,1,1));u.mem_write(array,struct.pack('<Q',view))
            u.mem_write(0xa355350,struct.pack('<Q',memory+0x20000))
            before=bytes(u.mem_read(memory,0xb000))
            u.reg_write(a64.UC_ARM64_REG_TPIDR_EL0,memory+0x30000)
            for register,value in ((a64.UC_ARM64_REG_X0,proxy),(a64.UC_ARM64_REG_X1,views),
                (a64.UC_ARM64_REG_X2,view),(a64.UC_ARM64_REG_X3,1),(a64.UC_ARM64_REG_X4,collector),
                (a64.UC_ARM64_REG_X30,stop),(a64.UC_ARM64_REG_SP,memory+0x4f000)):
                u.reg_write(register,value)
            # Only allocation/material/view/mesh sinks and libc sincos are supplied. Vertex math is original.
            u.emu_start(0x7db23d4,stop,count=100000)
            assert u.reg_read(a64.UC_ARM64_REG_PC)==stop and len(points)>=16 and len(triangles)*2==len(points)
            assert bytes(u.mem_read(memory,0xb000))==before
            assert all(math.isfinite(value) for point in points for value in point)
            result.append(dict(arc=degrees,pivot=pivot,size=(1920,1080),points=list(points)))
    print('PASS: original ARM64 CreateSceneProxy degree/pivot/cylinder copy and24 actual curved mesh vertex streams, including live22degree/original64.076194; allocation/material/collector sinks only, native geometry/input objects unchanged',flush=True)
    return result


def capture_header(cases):
    text=['static const float native_curve_vertices[][3]={']
    number=lambda value:float(value).hex()+'f'
    for case in cases:
        text.extend('{'+','.join(number(value) for value in point)+'},' for point in case['points'])
    text+=['};','static const struct{float arc,pivot[2],size[2];unsigned first,count;} native_curves[]={']
    first=0
    for case in cases:
        text.append('{'+number(case['arc'])+',{'+','.join(map(number,case['pivot']))+'},{'+','.join(map(number,case['size']))+'},'+str(first)+','+str(len(case['points']))+'},')
        first+=len(case['points'])
    return '\n'.join(text+['};',''])
