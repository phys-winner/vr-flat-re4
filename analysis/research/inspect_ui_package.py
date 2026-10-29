"""Extract selected immutable cooked UI packages and verify their bounded UE4 package maps."""
from pathlib import Path
import hashlib
import json
import struct
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'analysis'))
from pak9 import Pak9

def package(asset,body):
    assert asset[:4]==bytes.fromhex('c1832a9e') and struct.unpack_from('<i',asset,4)[0]==-7
    def string(at):
        n=struct.unpack_from('<i',asset,at)[0];size=n if n>=0 else -n*2
        assert 0<abs(n)<65536 and at+4+size<=len(asset)
        raw=asset[at+4:at+4+size];unit=1 if n>=0 else 2
        assert raw[-unit:]==bytes(unit)
        return raw[:-unit].decode('utf8' if n>=0 else 'utf-16-le'),at+4+size
    custom=struct.unpack_from('<i',asset,20)[0];assert custom==0
    header_size=struct.unpack_from('<I',asset,24)[0];assert header_size==len(asset)
    _,at=string(28)
    flags,names_count,names_offset,_,_,exports_count,exports_offset,imports_count,imports_offset=struct.unpack_from('<9I',asset,at)
    assert names_count<10000 and exports_count<10000 and imports_count<10000
    names=[];at=names_offset
    for _ in range(names_count):
        name,at=string(at);names.append(name);at+=4
    assert at<=imports_offset
    def fname(at):
        index,number=struct.unpack_from('<II',asset,at);assert index<len(names) and number<100000
        return names[index]+('_'+str(number-1) if number else '')
    imports=[]
    for i in range(imports_count):
        at=imports_offset+i*28
        imports.append(dict(index=-i-1,class_package=fname(at),**{'class':fname(at+8)},
                            outer=struct.unpack_from('<i',asset,at+16)[0],name=fname(at+20)))
    exports=[]
    for i in range(exports_count):
        at=exports_offset+i*104
        cls,super_,template,outer=struct.unpack_from('<4i',asset,at)
        size,offset=struct.unpack_from('<qq',asset,at+28);offset-=header_size
        assert size>=0 and 0<=offset<=len(body) and offset+size<=len(body)
        exports.append(dict(index=i+1,class_index=cls,super=super_,template=template,outer=outer,
                            name=fname(at+16),serial_size=size,uexp_offset=offset,
                            sha256=hashlib.sha256(body[offset:offset+size]).hexdigest()))
    def path(index,depth=0):
        assert depth<32 and index and -imports_count<=index<=exports_count
        item=imports[-index-1] if index<0 else exports[index-1]
        return (path(item['outer'],depth+1)+'.' if item['outer'] else '')+item['name']
    for item in imports:item['path']=path(item['index'])
    for item in exports:item['class_path']=path(item['class_index'])
    return dict(asset_sha256=hashlib.sha256(asset).hexdigest(),body_sha256=hashlib.sha256(body).hexdigest(),
                package_flags=hex(flags),names=names,imports=imports,exports=exports)

if __name__=='__main__':
    directory=ROOT/'analysis/research/flat_ui_graph'
    # The retained fixture independently checks the parser before another source is extracted.
    original=json.loads((directory/'OptionsConfirmScreen_BP.package.json').read_text())
    parsed=package((directory/'OptionsConfirmScreen_BP.uasset').read_bytes(),(directory/'OptionsConfirmScreen_BP.uexp').read_bytes())
    assert parsed==original
    pak=Pak9(ROOT/'VRLOC.com.Armature.VR4/patch.201.com.Armature.VR4.obb','VR4/Content/Paks/pakchunk0-Android_ETC2.pak')
    for stem in ('VR4CameraCover_BP','UI_PauseMenu_Content_BP','UI_PauseMenu_Content_Trifold_Base_BP'):
        name=next(name for name in pak.entries if name.endswith('/'+stem+'.uasset'))
        asset=pak.read_entry(name);body=pak.read_entry(name[:-7]+'.uexp');parsed=package(asset,body)
        (directory/(stem+'.uasset')).write_bytes(asset);(directory/(stem+'.uexp')).write_bytes(body)
        (directory/(stem+'.package.json')).write_text(json.dumps(parsed,indent=2)+'\n')
        print(stem,len(parsed['names']),len(parsed['imports']),len(parsed['exports']),parsed['body_sha256'])
