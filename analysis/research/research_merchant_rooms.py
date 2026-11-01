"""Read only bounded original room ITA/AEV sections for the touch-sale fixture."""
from pathlib import Path
import hashlib
import json
import struct

ROOT=Path(__file__).resolve().parents[2]
loader=ROOT/'analysis/research/host_oodle_decode.py'
scope={'__file__':str(loader),'__name__':'merchant_room_decoder'}
exec(compile(loader.read_text().split("fixture = ROOT / 'analysis/research/flat_ui_graph'",1)[0],str(loader),'exec'),scope)
u,buffers,call=(scope[key] for key in ('u','buffers','call'))
from pak9 import Pak9
pak=Pak9(ROOT/'com.Armature.VR4/patch.201.com.Armature.VR4.obb','VR4/Content/Paks/pakchunk0-Android_ETC2.pak')
results=[]
for room in (0x100,0x101,0x102,0x104):
    name=f'VR4/Content/BIO4/St1/r{room:03x}.das';entry=pak.entries[name]
    assert entry['method']==1 and not entry['encrypted']
    pak.f.seek(entry['off']);local,_=pak.rentry(pak.f.read(entry['header_size']),0)
    assert all(local[key]==entry[key] for key in ('size','unc','method','hash','blocks','encrypted','cbs'))
    raw=pak.f.read(entry['size']);assert hashlib.sha1(raw).digest()==entry['hash'];cache={}
    def block(index):
        if index not in cache:
            begin,end=entry['blocks'][index];compressed=raw[begin-entry['header_size']:end-entry['header_size']]
            length=min(entry['cbs'],entry['unc']-index*entry['cbs']);trials=[]
            for _ in range(2):
                u.mem_write(buffers,compressed);kind=call(0x96b6f6c,[buffers,len(compressed),0]);needed=call(0x96b4b88,[kind,length])
                decoded=call(0x96b64e0,[buffers,len(compressed),buffers+0x400000,length,1,1,0,0,0,0,0,buffers+0x800000,needed,3])
                assert decoded==length;trials.append(bytes(u.mem_read(buffers+0x400000,decoded)))
            assert trials[0]==trials[1];cache[index]=trials[0]
        return cache[index]
    header=block(0);archive=struct.unpack_from('<I',header,0x2c)[0];count=struct.unpack_from('<I',header,archive)[0]
    offsets=list(struct.unpack_from('<'+'I'*count,header,archive+16));offsets.append(struct.unpack_from('<I',header,0x24)[0])
    tags=[header[archive+16+count*4+i*4:archive+20+count*4+i*4] for i in range(count)]
    item={'room':f'{room:03x}','source':name,'compressed_sha1':entry['hash'].hex(),'sections':[]}
    for tag in (b'ITA\0',b'AEV\0'):
        index=tags.index(tag);begin,end=archive+offsets[index],archive+offsets[index+1]
        decoded=b''.join(block(i) for i in range(begin//entry['cbs'],(end-1)//entry['cbs']+1))
        data=decoded[begin%entry['cbs']:begin%entry['cbs']+end-begin]
        version,records=struct.unpack_from('<HH',data,4)
        section={'tag':tag[:-1].decode(),'offset':hex(begin),'sha256':hashlib.sha256(data).hexdigest(),
                 'version':hex(version),'count':records,'bytes':len(data),'header':data[:16].hex(),
                 'tail':data[16+records*0x9c:].hex(),'records':[]}
        if tag==b'ITA\0':
            assert len(data)>=16+records*0x9c
            u.mem_write(buffers+0x1000000,data)
            u.mem_write(buffers+0x1100000,bytes(16+records*0xd0))
            call(0x5d3b024,[buffers+0x1100000,16+records*0xd0,buffers+0x1000000,len(data)])
            for i in range(records):
                record=data[16+i*0x9c:16+(i+1)*0x9c]
                converted=bytes(u.mem_read(buffers+0x1100000+16+i*0xd0,0xd0))
                section['records'].append({'index':i,'bytes':record.hex(),'converted':converted.hex()})
        else:
            assert len(data)>=16+records*0x9c
            for i in range(records):
                record=data[16+i*0x9c:16+(i+1)*0x9c]
                section['records'].append({'index':i,'bytes':record.hex()})
        item['sections'].append(section)
    item['decoded_block_count']=len(cache);results.append(item);print(f'R{room:03x}: bounded original sections decoded twice, {len(cache)} blocks',flush=True)
pak.f.close();pak.z.close()
out=ROOT/'analysis/research/merchant_originals/room_records.json'
out.write_text(json.dumps(results,indent=2)+'\n')
print(out)
