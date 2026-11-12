"""Read-only engine research: execute native full-key conversion/rollback APIs.

All game functions and callbacks execute their original ARM64 bytes. The retained
loader substitutes only libc memcpy/memmove/memset; fixture writes are to emulator
memory, never to the APK, engine, save files or a device. This is ABI evidence,
not a replacement for live UObject/cache/door acceptance.
"""
from pathlib import Path
import struct
from unicorn import UC_HOOK_CODE
from unicorn.arm64_const import UC_ARM64_REG_X0

ROOT=Path(__file__).resolve().parents[2]
loader=ROOT/'analysis/research/host_oodle_decode.py'
environment={'__file__':str(loader),'__name__':'native_key_transaction_research'}
exec(compile(loader.read_text().split("fixture = ROOT / 'analysis/research/flat_ui_graph'",1)[0],str(loader),'exec'),environment)
u,base,buffers,call=(environment[name] for name in ('u','base','buffers','call'))
# IDA proves the stock itemCombine scans exactly75 six-byte rows at0x4304e56.
# Classify every row with the complete original metadata function, not item names.
recipes=[struct.unpack_from('<HHH',environment['native'],0x4304e56+i*6) for i in range(75)]
types={}
for item in {item for recipe in recipes for item in recipe}:
    u.mem_write(buffers,bytes(16));call(0x5f32148,[item,buffers])
    types[item]=bytes(u.mem_read(buffers,8))[2]
key_recipes=[recipe for recipe in recipes if any(types[item]==7 for item in recipe)]
assert key_recipes==[(0xa4,0xa5,0xa6),(0x3a,0x69,0x7a)]
assert all(all(types[item]==7 for item in recipe) for recipe in key_recipes)
print('PASS: all75 native combine recipes inspected; the two key-type7 recipes are a4+a5=>a6 and3a+69=>7a')
manager=base+0xa500780
flags,items=buffers+0x10000,buffers+0x11000
events=[]
callbacks={base+0x62e1ee4:'ItemAdded',base+0x62e2020:'ItemReceived',base+0x62e219c:'ItemRemoved'}

def observe(uc,address,size,user):
    if address in callbacks:
        pointer=uc.reg_read(UC_ARM64_REG_X0)
        events.append((callbacks[address],pointer,struct.unpack('<H',bytes(uc.mem_read(pointer,2)))[0]))

u.hook_add(UC_HOOK_CODE,observe,begin=min(callbacks),end=max(callbacks))

def read64(address):
    return struct.unpack('<Q',bytes(u.mem_read(address,8)))[0]

def count(item):
    return call(0x5f33f14,[manager,item])

def counts():
    return tuple(count(item) for item in active_recipe)

def get(item):
    result=call(0x5f2886c,[manager,item,1])
    pointer=read64(manager+0x28)
    if result:
        assert items<=pointer<items+14*capacity and (pointer-items)%14==0
        assert struct.unpack('<HHBB',bytes(u.mem_read(pointer,6)))==(item,1,1,0)
    return result,pointer

def setup(slots):
    global capacity
    capacity=slots
    u.mem_write(manager,bytes(0x80))
    u.mem_write(flags,bytes(256))
    u.mem_write(items-16,b'\xa5'*16+bytes(14*8)+b'\x5a'*16)
    u.mem_write(manager,struct.pack('<QI',flags,64))
    u.mem_write(manager+0x20,struct.pack('<Q',items))
    u.mem_write(manager+0x30,struct.pack('<I',capacity))
    assert read64(base+0xa361da0)==manager
    assert get(active_recipe[2])[0]==1 and counts()==(0,0,1)
    source=read64(manager+0x28)
    events.clear()
    return source,bytes(u.mem_read(source,14))

def rollback(pointers):
    for pointer in reversed(pointers):
        call(0x5f33a1c,[manager,pointer])

def check_canaries():
    assert bytes(u.mem_read(items-16,16))==b'\xa5'*16
    assert bytes(u.mem_read(items+14*8,16))==b'\x5a'*16

for active_recipe in key_recipes:
    right_item,left_item,full_item=active_recipe
    # Success: both outputs are first acquired natively; only then consume source.
    source,before=setup(8)
    right_ok,right=get(right_item);left_ok,left=get(left_item)
    assert right_ok==left_ok==1 and counts()==(1,1,1)
    assert bytes(u.mem_read(source,14))==before
    assert events==[('ItemAdded',right,right_item),('ItemReceived',right,right_item),('ItemAdded',left,left_item),('ItemReceived',left,left_item)]
    call(0x5f34f64,[manager,full_item])
    assert call(0x5f33468,[manager,full_item])==1 and counts()==(1,1,0)
    assert events[-1]==('ItemRemoved',source,full_item)
    assert bytes(u.mem_read(source,4))==struct.pack('<HH',full_item,0)
    assert bytes(u.mem_read(source+4,1))==b'\0'
    check_canaries()
    print(f'PASS: original get/available/use consumes exactly one0x{full_item:02x} for two real halves; stock Added/Received/Removed callbacks execute')

    # No free output slot: preserve source without invoking its availability.
    source,before=setup(1)
    assert get(right_item)[0]==0 and counts()==(0,0,1)
    assert bytes(u.mem_read(source,14))==before and events==[]
    check_canaries()

    # One free slot: undo only the successfully acquired half through native erase.
    source,before=setup(2)
    right_ok,right=get(right_item);left_ok,_=get(left_item)
    assert right_ok==1 and left_ok==0 and counts()==(1,0,1)
    rollback([right])
    assert counts()==(0,0,1) and bytes(u.mem_read(source,14))==before
    assert events==[('ItemAdded',right,right_item),('ItemReceived',right,right_item),('ItemRemoved',right,right_item)]
    assert bytes(u.mem_read(flags,256))==bytes(256)
    check_canaries()
    print(f'PASS: original full0x{full_item:02x} survives zero/one-free-slot refusal; only newly acquired half is rolled back')

    # Native use refusal is represented by its actual unmet availability predicate.
    source,before=setup(8)
    _,right=get(right_item);_,left=get(left_item)
    assert call(0x5f33468,[manager,full_item])==0
    rollback([right,left])
    assert counts()==(0,0,1) and bytes(u.mem_read(source,14))==before
    assert events[-2:]==[('ItemRemoved',left,left_item),('ItemRemoved',right,right_item)]
    assert bytes(u.mem_read(flags,256))==bytes(256)
    check_canaries()
    print(f'PASS: original use refusal allows both outputs rollback, with full0x{full_item:02x} bytes/count preserved')
assert set(environment['used'])<= {'memcpy','memmove','memset'}
print('LIMIT: no device writes or game-function substitutions; active VR inventory/cache and physical receptacle remain hardware gates')
