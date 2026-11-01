"""Execute the original R106 action dispatcher and scheduler without an Android device."""
from pathlib import Path
import hashlib
import json
import struct

ROOT = Path(__file__).resolve().parents[2]
loader = ROOT / 'analysis/research/host_oodle_decode.py'
prefix = loader.read_text().split("fixture = ROOT / 'analysis/research/flat_ui_graph'", 1)[0]
environment = {'__file__': str(loader), '__name__': 'original_native_loader'}
exec(compile(prefix, str(loader), 'exec'), environment)
u, buffers, call, base = (environment[name] for name in ('u', 'buffers', 'call', 'base'))
from unicorn import UC_HOOK_CODE
from unicorn.arm64_const import UC_ARM64_REG_PC, UC_ARM64_REG_LR, UC_ARM64_REG_X0

evidence = ROOT / 'analysis/research/luis_route_originals'
receipt = json.loads((evidence / 'receipt.json').read_text())
for item in receipt:
    data = (evidence / item['output']).read_bytes()
    assert len(data) == item['bytes'] and hashlib.sha256(data).hexdigest() == item['sha256']
    assert item['repeats'] == 2
    assert item['obb'] in ('main.201.com.Armature.VR4.obb', 'patch.201.com.Armature.VR4.obb')
from locres_read import read_locres
localization = read_locres((ROOT / 'analysis/research/localization_originals/en_VR4.locres').read_bytes())
rescue_line = next(item for item in localization['entries'] if item['key'] == 'r106s00_00')
assert rescue_line['text'] == "Oh! A little rough don't you think?"
room0 = (evidence / 'r106.das.block0').read_bytes()
room9 = (evidence / 'r106.das.block9').read_bytes()
assert struct.unpack_from('<I', room0, 0x2c)[0] == 0x400
assert struct.unpack_from('<I', room0, 0x400)[0] == 55
assert room0[0x528:0x52c] == b'AEV\0'
assert struct.unpack_from('<I', room0, 0x410 + 15 * 4)[0] == 0x9e2e40
original = room9[0xe3240:0xe3cc0]
assert original[:8] == b'AEV\0\x04\x01\x11\0'
disk_entry = original[16 + 2 * 0x9c:16 + 3 * 0x9c]
assert disk_entry[0x34:0x3c] == bytes.fromhex('03 02 02 01 88 01 00 00')

manager = base + 0xa541b38
header, entries = buffers + 0x100000, buffers + 0x100010
node = entries + 2 * 0x100
u.mem_write(header, original[:16] + bytes(17 * 0x100))
u.mem_write(buffers, disk_entry)
call(0x5d3a7f0, [node, buffers])  # Original ARM32-disk -> ARM64 common action conversion.
assert u.mem_read(node + 0x38, 4) == bytes.fromhex('03 02 02 01')
u.mem_write(node, struct.pack('<Q', 0xffffffff))
u.mem_write(manager, struct.pack('<QQ', header, entries))
u.mem_write(manager + 0xd8, struct.pack('<Q', node))
u.mem_write(base + 0xa542058, struct.pack('<Q', manager))

# The original API optionally hands the event to a physical VR override. This
# room has no such override in the fixture; stop at the original Find API only.
def no_override(uc, address, length, unused):
    uc.reg_write(UC_ARM64_REG_X0, 0)
    uc.reg_write(UC_ARM64_REG_PC, uc.reg_read(UC_ARM64_REG_LR))

find_hook = u.hook_add(UC_HOOK_CODE, no_override, begin=base + 0x635951c, end=base + 0x635951c)
call(0x61553e8, [2, 0x14, 0, base + 0x600e294, 0, 1])
u.hook_del(find_hook)
registered = bytes(u.mem_read(node, 0x100))
assert registered[0x38:0x3c] == bytes.fromhex('03 02 02 01')
assert registered[0x3e] == 0x14 and registered[0x50:0x52] == bytes((2, 1))
assert struct.unpack_from('<QQ', registered, 0x40) == (0, base + 0x600e294)

work = base + 0xa542bb8

def forbid_scene_or_sleep(uc, address, length, unused):
    raise AssertionError(('yielding scene ran synchronously', hex(address - base)))

forbidden = [u.hook_add(UC_HOOK_CODE, forbid_scene_or_sleep, begin=base + address, end=base + address)
             for address in (0x600e294, 0x6161358, 0x61629d4)]
for current_task in (base + 0xa40a0f8, 0, 0xffffffffffffffff):
    u.mem_write(work + 0x38 + 2 * 8, struct.pack('<Q', 0xffffffff))
    u.mem_write(base + 0xa543150, struct.pack('<Q', current_task))
    for index in range(8, 20):
        u.mem_write(base + 0xa5434e8 + index * 0x40, bytes(0x40))
    assert call(0x6155664, [2]) == 1
    tasks = [bytes(u.mem_read(base + 0xa5434e8 + index * 0x40, 0x40)) for index in range(8, 20)]
    active = [(index + 8, task) for index, task in enumerate(tasks) if task[0]]
    assert len(active) == 1 and active[0][0] == 19
    task = active[0][1]
    assert task[0] == 1 and task[1] == 14 and task[3] == 1
    assert struct.unpack_from('<QQ', task, 0x10) == (0, base + 0x600e294)
    assert struct.unpack('<Q', u.mem_read(base + 0xa543150, 8))[0] == current_task
    assert bytes(u.mem_read(node, 0x100)) == registered
for hook in forbidden:
    u.hook_del(hook)
print('PASS: original ConvertSceAtUnit -> SceAtDataSet_exec(2,20) -> SceAtExecute(2) -> SceExec/TaskExec')
print('PASS: native task19 owns actual R106S00 callback; no scene callback/SceSleep ran on caller; action record and current task unchanged')
print('PASS: explicit native kind1 bypasses TaskGetTaskNo for Main/null/idle(-1); no scheduler/current-task mutation by the QA caller')
(ROOT / 'analysis/research/user_luis_route_evidence.json').write_text(json.dumps({
    'native_sha256': environment['native_hash'],
    'room': '0x106', 'scene': 'r106s00', 'original_action_id': 2,
    'room_originals_receipt': 'luis_route_originals/receipt.json',
    'english_locres_sha256': localization['sha256'],
    'subtitle_key': rescue_line['key'], 'subtitle': rescue_line['text'],
    'original_registration': {'caller': '0x600d83c', 'api': 'SceAtDataSet_exec',
                              'arguments': [2, 20, 0, '0x600e294', 0, 1]},
    'original_event': {'data': 'evd/r106s00.evd', 'set_evt_call': '0x600e38c',
                       'wait_alive_call': '0x600e3d4'},
    'original_chapter_end': {'call': '0x600e424', 'api': 'SceSetChapterEnd', 'arguments': [0, 3],
                             'chapter_summary_call': '0x615e6b0', 'advance_summary_call': '0x615e9f4'},
    'qa_command': 'qatrans room106 then qatrans luis',
    'scheduler_executed_original_code': True, 'scheduled_task': 19,
    'no_synchronous_scene_or_sleep': True, 'current_task_unchanged': True,
    'current_task_cases': ['CTASK_MAIN', 'null', 'idle(-1)'], 'no_current_task_lookup_for_explicit_kind1': True,
    'hardware_route_verified': False,
    'limits': 'Original dispatch/scheduling runs in Unicorn with the optional physical Find override returning none; full native event/scene hardware traversal belongs to the coordinator.'
}, indent=2) + '\n')
