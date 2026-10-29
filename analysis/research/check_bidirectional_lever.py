"""Execute actual SCE dispatch and the original r11c two-ended grip Blueprint."""
import os
from pathlib import Path
import hashlib
import math
import re
import struct
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'analysis'),str(ROOT/'analysis/research')]
from inspect_ui_package import package
from inspect_flat_bp import Script
native=(ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
for addr in (0x64d205c,0x64c55c4):assert struct.unpack_from('<II',native,addr)==(0xd10183ff,0xf90023f3)

source=(ROOT/'analysis/re4fix_sce_touch.h').read_text()
def c_function(name):
    match=re.search(r'(?:static )?(?:int|void) '+name+r'\([^;]*?\)\s*\{',source);assert match,name
    start=match.start();at=match.end();depth=1
    while depth:
        if source[at]=='{':depth+=1
        elif source[at]=='}':depth-=1
        at+=1
    return source[start:at]
actual='\n'.join(c_function(n) for n in ('sce_bidirectional','sce_kind','sce_crank_stop','sce_operate','touch_sce_interact','touch_sce_tap'))
assert 'if (!kind || !sce_location(c, world)) continue;' in source
assert 'sce_bidirectional' in c_function('sce_kind')
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-bidirectional-') as temporary:
    folder=Path(temporary);helpers=folder/'helpers.h';obj=folder/'lever.obj';exe=folder/'lever.exe'
    for label,text in (('fixed',actual),('original_shortcut',actual.replace('if(kind==1 && sce_bidirectional(c))return 0;',''))):
        helpers.write_text(text)
        if label=='fixed':subprocess.run([str(clang),'--target=aarch64-linux-android23','-Werror','-fsyntax-only',
            '-DBIDIRECTIONAL_C_HELPERS="'+helpers.as_posix()+'"',str(ROOT/'analysis/research/test_bidirectional_lever.c')],check=True)
        subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
            '-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
            '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
            '-DBIDIRECTIONAL_C_HELPERS="'+helpers.as_posix()+'"','-c','-o',str(obj),str(ROOT/'analysis/research/test_bidirectional_lever.c')],check=True)
        subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
            'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
            f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',
            f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
        result=subprocess.run([str(exe)],capture_output=True,text=True)
        if label=='fixed':assert result.returncode==0,result.stdout+result.stderr;print(result.stdout.strip(),flush=True)
        else:assert result.returncode!=0 and 'Assertion failed:' in result.stdout+result.stderr;print('PASS: the identical actual-C fixture fails with the original SCE shortcut restored (no physical grip press)',flush=True)

fixture=ROOT/'analysis/research/interaction_originals'
hashes={
    'BidirectionalLever_StaticModel_BP1.uasset':'38f8f5efb33cff4b4cba0dd6541fc9f15c1e52f8873a748944cab04751ebf3a8',
    'BidirectionalLever_StaticModel_BP1.uexp':'eebaad28d07e10c6df90136ec88ba0da8dace84c2e0cbbaa75ba923bce8c4d24',
    'r11c_soft_lever_54_interaction.uasset':'3b148277d42d816999f15f63567d0d9a0e07badc0c07eb52248d7d79e922dbf4',
    'r11c_soft_lever_54_interaction.uexp':'15b0d40a74c70b23c0754be2155d8aec92d3b9e01844d335eb96225618201dd7',
}
raw={name:(fixture/name).read_bytes() for name in hashes}
for name,digest in hashes.items():assert hashlib.sha256(raw[name]).hexdigest()==digest
stem='BidirectionalLever_StaticModel_BP1';pkg=package(raw[stem+'.uasset'],raw[stem+'.uexp']);body=raw[stem+'.uexp'];names=pkg['names']
def script(name):
    entry=next(e for e in pkg['exports'] if e['name']==name);data=body[entry['uexp_offset']:entry['uexp_offset']+entry['serial_size']]
    for at in range(4,len(data)-16):
        count=struct.unpack_from('<I',data,at)[0]
        if at+4+count+12!=len(data) or data[-13]!=0x53:continue
        parser=Script(data[at+4:at+4+count],names,pkg['imports'],pkg['exports']);tree={}
        try:
            while parser.at<len(parser.data):runtime=parser.at+parser.runtime_delta;tree[runtime]=parser.expression()
            assert parser.at+parser.runtime_delta==struct.unpack_from('<I',data,at-4)[0]
        except (AssertionError,IndexError,KeyError,struct.error):continue
        return entry,tree,parser.calls
    raise AssertionError(name)
class_id=next(e['index'] for e in pkg['exports'] if e['class_path']=='/Script/Engine.BlueprintGeneratedClass')
programs={name:script(name) for name in ('Calculate Starting Grip','InitializeGrab','UpdateTowedHandPosition','UpdateGrab')}
_,main,_=script('ExecuteUbergraph_BidirectionalLever_StaticModel_BP1')
_,_,activated=script('OnInteractionActivated')
assert activated[0]['arguments']==[{'int':1990}]
receivers=[]
for expression in main.values():
    if expression.get('bind_delegate')!='OnInteractionActivated':continue
    variable=expression['delegate']['field']
    addition=next(x for x in main.values() if x.get('delegate_operation')==92 and x.get('value',{}).get('field')==variable)
    assert addition['target']['call']['field']==['OnInteractionActivatedDelegate']
    receivers.append(addition['target']['context']['field'][0])
assert sorted(receivers)==['SecondarySceInteraction','VR4Lever']
assert main[1990]['let_bool'][1]=={'literal_token':39}
assert main[2001]['let_bool'][1]=={'literal_token':40}
assert main[2034]['call']['call']=='/Script/VR4.VR4GamePlayerGrip.Release'
assert main[2235]['arguments'][1]=={'float':.75}
assert not any('GetLocalEarlyGrabTransformDelta' in c['call'] for _,_,calls in programs.values() for c in calls)
assert not any(name in str(script('SetLastAlpha')[1]) for name in ('PercentageComplete','GrabStartingPosition','GrabTargetPosition'))

def property_value(metadata,body,export,name,kind,size,structure=None):
    entry=next(e for e in metadata['exports'] if e['name']==export);data=body[entry['uexp_offset']:entry['uexp_offset']+entry['serial_size']]
    ns=metadata['names'];mark=struct.pack('<II',ns.index(name),0);assert data.count(mark)==1;at=data.index(mark)
    assert struct.unpack_from('<II',data,at+8)==(ns.index(kind),0) and struct.unpack_from('<II',data,at+16)==(size,0)
    header=49 if structure else 25
    if structure:assert struct.unpack_from('<II',data,at+24)==(ns.index(structure),0)
    assert data[at+header-1]==0;return data[at+header:at+header+size]
child='r11c_soft_lever_54_interaction';cm=package(raw[child+'.uasset'],raw[child+'.uexp']);cb=raw[child+'.uexp']
start=struct.unpack('<3f',property_value(cm,cb,'VR4GamePlayerGrip_GEN_VARIABLE','RelativeLocation','StructProperty',12,'Vector'))
target_rotation=struct.unpack('<3f',property_value(cm,cb,'VR4Lever_GEN_VARIABLE','TargetDeltaAngle','StructProperty',12,'Rotator'))
below=struct.unpack('<f',property_value(cm,cb,'Default__'+child+'_C','PercentBelowToOpen','FloatProperty',4))[0]
above=struct.unpack('<f',property_value(pkg,body,'VR4Lever_GEN_VARIABLE','PercentToAutoOpen','FloatProperty',4))[0]
neutral=struct.unpack('<f',property_value(pkg,body,'Default__'+stem+'_C','StartOpenPercent','FloatProperty',4))[0]
assert target_rotation==(0,0,110) and (below,above,neutral)==(30,70,50)
def add(a,b):return tuple(x+y for x,y in zip(a,b))
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def mul(a,b):return tuple(x*b for x in a)
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def normal(a):return mul(a,1/math.sqrt(dot(a,a))) if dot(a,a)>1e-8 else (0,0,0)
def rotate_z(a,yaw):c,s=math.cos(yaw),math.sin(yaw);return (c*a[0]-s*a[1],s*a[0]+c*a[1],a[2])
def rotate_roll(a,degrees):c,s=math.cos(math.radians(degrees)),math.sin(math.radians(degrees));return (a[0],c*a[1]-s*a[2],s*a[1]+c*a[2])

# Bounded interpreter of these parsed original Blueprint expressions. Pure math
# follows Kismet semantics; scene reads/writes/haptics are controlled runtime data.
# Endpoint decisions and native receiver calls come from the original script.
class VM:
    def __init__(self,yaw):
        self.yaw=yaw;self.local={};self.function=0;self.events=[]
        self.owner={'position':(300,-200,0)};self.hand={'position':(30,-20,130)}
        self.lever={'TargetDeltaAngle':target_rotation,'TargetDeltaPosition':(0,0,0),'PercentToAutoOpen':above}
        self.secondary={};self.grip={'position':add(self.owner['position'],rotate_z(start,yaw))}
        self.state={'VR4Lever':self.lever,'SecondarySceInteraction':self.secondary,'VR4GamePlayerGrip':self.grip,
            'OverrideDeltaDirection':False,'OverrideDeltaRotationTarget':(0,0,90),'UseHandGripOffsetDistance':False,
            'StartOpenPercent':neutral,'PercentBelowToOpen':below,'VirtualHandOffset':(0,0,0)}
    def field(self,x,receiver=None):
        key=x['field'][0];storage=receiver if receiver is not None else self.state if x['owner']==class_id else self.local
        return storage.get(key,0)
    def set(self,x,value,receiver=None):
        if 'context' in x:return self.set(x['call'],value,self.eval(x['context']))
        storage=receiver if receiver is not None else self.state if x['owner']==class_id else self.local;storage[x['field'][0]]=value
    def eval(self,x,receiver=None):
        if 'field' in x:return self.field(x,receiver)
        for key in ('float','int','byte_int','byte'):
            if key in x:return x[key]
        if 'vector_token' in x:return x['values']
        if 'literal_token' in x:return {0x27:True,0x28:False,0x17:self.owner,0x2a:None,0x0b:None}.get(x['literal_token'])
        if 'object' in x:return x['object']
        if 'context' in x:return self.eval(x['call'],self.eval(x['context']))
        if 'switch_value' in x:
            value=self.eval(x['switch_value'])
            return self.eval(next((case['result'] for case in x['cases'] if self.eval(case['match'])==value),x['default']))
        if 'let' in x:self.set(x['let'][1],self.eval(x['let'][2]));return None
        if 'let_bool' in x:self.set(x['let_bool'][0],self.eval(x['let_bool'][1]));return None
        if 'call' in x:return self.call(x,receiver)
        raise AssertionError(x)
    def call(self,x,receiver):
        name=x['call'];args=x['arguments'];values=[self.eval(a) for a in args];short=name.rsplit('.',1)[-1]
        if short=='K2_GetActorLocation':return (receiver or self.owner)['position']
        if short=='GetTransform':return {'position':self.owner['position'],'yaw':self.yaw}
        if short=='K2_GetComponentLocation':return receiver['position']
        if short=='InverseTransformLocation':return rotate_z(sub(values[1],values[0]['position']),-values[0]['yaw'])
        if short=='GreaterGreater_VectorRotator':return rotate_roll(values[0],values[1][2])
        if short in ('Add_VectorVector','Subtract_VectorVector'):return (add if short.startswith('Add') else sub)(*values)
        if short=='Multiply_VectorFloat':return mul(*values)
        if short=='Dot_VectorVector':return dot(*values)
        if short=='Normal':return normal(values[0])
        if short=='Vector_Distance':return math.dist(*values)
        if short=='FClamp':return max(values[1],min(values[2],values[0]))
        if short=='Multiply_FloatFloat':return values[0]*values[1]
        if short=='Subtract_FloatFloat':return values[0]-values[1]
        if short=='Add_FloatFloat':return values[0]+values[1]
        if short=='Divide_FloatFloat':return values[0]/values[1] if values[1] else 0
        if short=='Abs':return abs(values[0])
        if short=='Greater_FloatFloat':return values[0]>values[1]
        if short=='Less_FloatFloat':return values[0]<values[1]
        if short=='ModifyTargetGripPosition':self.set(args[1],values[0]);return None
        if short=='UpdateTowedHandPosition':return self.run(short,{'Hand':values[0],'TowDistance':values[1]})
        if short=='SetLastAlpha':return None # audio/movement notification only; endpoint logic recomputes alpha
        if short in ('GetStartingRotation','GetStartingPosition','GetTargetPosition'):return (0,0,0)
        if short=='GetTargetRotation':return target_rotation
        if short=='Quat_Rotator':return values[0]
        if short in ('RLerp','VLerp'):return add(values[0],mul(sub(values[1],values[0]),values[2]))
        if short in ('K2_SetActorRotation','K2_SetActorLocation','PlayHaptics','SetAngleByPercent'):return True
        if short=='ActivateSwitch':assert receiver is self.lever;self.events.append('R11C_Lever_RightGateInteraction');return None
        if short=='ActivateEvent':assert receiver is self.secondary;self.events.append('R11C_Lever_LeftGateInteraction');return None
        raise AssertionError((name,values))
    def run(self,name,arguments=None):
        saved=self.local;self.local=dict(arguments or {});_,tree,_=programs[name];positions=sorted(tree);index=0
        for _ in range(1000):
            expression=tree[positions[index]];target=None
            if 'return' in expression:break
            if 'jump' in expression:target=expression['jump']
            elif 'jump_if_not' in expression:
                if not self.eval(expression['condition']):target=expression['jump_if_not']
            else:self.eval(expression)
            index=positions.index(target) if target is not None else index+1
        else:raise AssertionError('script step budget')
        self.local=saved

producer=(ROOT/'analysis/re4fix.c').read_text()
assert 'q_rot(position_q, off, w); pos[0] += w[0]; pos[1] += w[1]; pos[2] += w[2];' in producer
assert '{-0.20f, -0.30f, -0.30f}' in producer or '{-0.20f,-0.30f,-0.30f}' in producer
results=[]
for yaw in (0,math.pi/2):
    for direction in (-1,1):
        vm=VM(yaw);vm.run('Calculate Starting Grip')
        # Actual left phone hand: head(0,0,160)+UE offset(30,-20,-30), with no roll input.
        vm.hand['position']=add((0,0,160),rotate_z((30,-20,-30),yaw));vm.run('InitializeGrab',{'Hand':vm.hand})
        vm.hand['position']=add(vm.hand['position'],rotate_z((0,direction*80,0),yaw))
        vm.run('UpdateGrab',{'Hand':vm.hand})
        assert vm.events==(['R11C_Lever_RightGateInteraction'] if direction<0 else ['R11C_Lever_LeftGateInteraction']),vm.events
        results.append((round(math.degrees(yaw)),direction*80,round(vm.state['PercentageComplete'],5),vm.events[0]))
print('PASS: executed original CalculateStartingGrip/InitializeGrab/UpdateTowedHandPosition/UpdateGrab; original phone-hand translation with 80cm left/right walking selects BOTH authored gate receivers, with no roll control:',results,flush=True)
print('LIMIT: controlled Blueprint/native scene services and no physical r11c collision/IK/door traversal acceptance; verify Interact then strafe left/right in the authored room',flush=True)
