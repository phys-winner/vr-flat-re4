"""Research only: execute the original serializer on an isolated constructor-created cCard.

No game files or production source are changed. The original writer boundary is
captured instead of writing a file. Allocator calls are emulated, and every live
selection-job byte, source-container byte and transaction flag is compared.
"""
from pathlib import Path
import hashlib
import json
import struct

ROOT = Path(__file__).resolve().parents[2]
loader = ROOT / 'analysis/research/host_oodle_decode.py'
prefix = loader.read_text().split("fixture = ROOT / 'analysis/research/flat_ui_graph'", 1)[0]
environment = {'__file__': str(loader), '__name__': 'original_native_loader'}
exec(compile(prefix, str(loader), 'exec'), environment)
u, base, buffers, call = (environment[name] for name in ('u', 'base', 'buffers', 'call'))
UC_HOOK_CODE = environment['UC_HOOK_CODE']
X0, LR, PC = (environment[name] for name in ('UC_ARM64_REG_X0', 'UC_ARM64_REG_LR', 'UC_ARM64_REG_PC'))
native = environment['native']

ranges = ((0x5f1b0dc, 0x5f1b10c), (0x5f1b10c, 0x5f1b114),
          (0x5f13784, 0x5f1378c), (0x5d5b868, 0x5d5b87c),
          (0x5f1d2a0, 0x5f1d2e0), (0x5f202e8, 0x5f20300),
          (0x5f1f268, 0x5f1f480), (0x5f1e630, 0x5f1e6d0),
          (0x5f210e4, 0x5f21278))
assert struct.unpack_from('<I', native, 0x42f9530)[0] == 2079352
assert struct.unpack_from('<4I', native, 0x5d5b868) == (0xb900001f, 0xa900fc1f, 0xb900181f, 0xd65f03c0)
assert struct.unpack_from('<I', native, 0x5d5b878)[0] == 0xd65f03c0

source = buffers
private_file = buffers + 0x400000
private_work = buffers + 0x800000
system = buffers + 0x810000
global_data = buffers + 0x820000
live_work = buffers + 0x830000
arena = buffers + 0xc00000
file_size = 2079352
fixture = ROOT / 'analysis/research/p270_save_backup/savegame00.sav'
original = fixture.read_bytes()
assert len(original) == file_size and hashlib.sha256(original).hexdigest() == '33a488865638a0bc01a8f3cfe7b29b35a80bcf8e6015db36c763ad043aa343b8'
allocations, freed, writes, allocation_requests = [], [], [], []
fail_allocation = False

def setq(address, value):
    u.mem_write(address, struct.pack('<Q', value))

def seti(address, value):
    u.mem_write(address, struct.pack('<I', value))

def intercepted(uc, address, size, user):
    a, b, c = (uc.reg_read(X0+i) for i in range(3))
    if address == base+0x6615ef0:
        assert a == file_size and b == 0 and not allocation_requests
        allocation_requests.append((a, b))
        if fail_allocation:
            result = 0
        else:
            allocations.append(arena)
            result = arena
    elif address == base+0x6615fd0:
        assert a == arena and allocations == [arena] and not freed
        freed.append(a)
        result = 0
    else:
        assert address == base+0x5f210e4 and a == 0 and b == arena and c == file_size
        assert struct.unpack('<Q', uc.mem_read(base+0xa4ff3a0, 8))[0] == arena
        writes.append(bytes(uc.mem_read(b, c)))
        result = 0
    uc.reg_write(X0, result)
    uc.reg_write(PC, uc.reg_read(LR))

for address in (0x6615ef0, 0x6615fd0, 0x5f210e4):
    u.hook_add(UC_HOOK_CODE, intercepted, begin=base+address, end=base+address)

results = []
for flags in (0, 0x100, 0x300, 0x98):
    for action in (1, 2, 3, 4):
        allocations.clear(); freed.clear(); writes.clear(); allocation_requests.clear()
        u.mem_write(source, original)
        u.mem_write(private_file, original)
        u.mem_write(private_work, bytes(0x6f0))
        u.mem_write(live_work, bytes((i*37+11) & 255 for i in range(0x6f0)))
        u.mem_write(system, original[0x1e40:0x1f30]+bytes(0x110))
        u.mem_write(global_data, bytes(0x8700))
        seti(global_data+4, flags)
        setq(base+0xa3f5e50, system)
        setq(base+0xa3f5e48, global_data)
        setq(base+0xa4ff390, live_work)
        setq(base+0xa4ff398, source)
        seti(base+0xa4ff38c, file_size)
        setq(base+0xa4ff3a0, 0)
        assert call(0x5f202e8, []) == source
        assert call(0x5f202f4, []) == file_size
        call(0x5f1b0dc, [private_work])
        call(0x5f1d2a0, [private_work, 1])
        assert bytes(u.mem_read(private_work+0x5e8, 4)) == struct.pack('<I', file_size)
        # Adapter's sole private pointer assignment, not a live-job/state/flag write.
        # It is deliberately explicit here so review cannot mistake it for a native setter.
        setq(private_work+0x5e0, private_file)
        assert bytes(u.mem_read(private_work+0x5c8, 8)) == bytes(8)
        if action == 1:
            call(0x61d196c, [])
        elif action == 2:
            call(0x61e1b28, [1])
        else:
            for item in ((1,) if action == 3 else (2, 3)):
                call(0x61d1b34, [item])
        before_live = bytes(u.mem_read(live_work, 0x6f0))
        before_global = bytes(u.mem_read(global_data, 0x8700))
        before_system = bytes(u.mem_read(system, 0x200))
        assert call(0x5f1e630, [private_work, 0]) == 1
        call(0x5f1b10c, [private_work])
        assert allocations == freed == [arena] and len(writes) == 1
        assert bytes(u.mem_read(source, file_size)) == original
        assert bytes(u.mem_read(live_work, 0x6f0)) == before_live
        assert bytes(u.mem_read(global_data, 0x8700)) == before_global
        assert bytes(u.mem_read(system, 0x200)) == before_system
        assert struct.unpack('<Q', u.mem_read(base+0xa4ff390, 8))[0] == live_work
        assert struct.unpack('<Q', u.mem_read(base+0xa4ff3a0, 8))[0] == 0
        out = writes[0]
        expected_mask = (0x400000, 0x40000000, 0x80000, 0x30000000)[action-1]
        assert struct.unpack_from('<I', out, 0x1e44)[0] == (struct.unpack_from('<I', original, 0x1e44)[0] | expected_mask)
        assert out[:0x1e40] == original[:0x1e40]
        assert out[0x1e6c:0x1f0c] == original[0x1e6c:0x1f0c]
        assert out[0x1f30:] == original[0x1f30:]
        results.append(dict(native_flags=hex(flags), action=action, bytes=len(out),
            flags=hex(struct.unpack_from('<I', out, 0x1e44)[0]),
            changed_offsets=[hex(i) for i, (a, b) in enumerate(zip(original, out)) if a != b],
            source_and_live_job_unchanged=True, original_publication_boundary=True,
            campaign_payload_sha256=hashlib.sha256(out[0x1f30:]).hexdigest()))

# The real createSystemFile body must handle its own publication allocation failure
# without reaching the writer or changing the caller's original selection/source.
allocations.clear(); freed.clear(); writes.clear(); allocation_requests.clear()
fail_allocation = True
u.mem_write(private_work, bytes(0x6f0))
call(0x5f1b0dc, [private_work])
call(0x5f1d2a0, [private_work, 1])
setq(private_work+0x5e0, private_file)
before_live = bytes(u.mem_read(live_work, 0x6f0))
before_global = bytes(u.mem_read(global_data, 0x8700))
before_system = bytes(u.mem_read(system, 0x200))
assert call(0x5f1e630, [private_work, 0]) == 0xffffffff
call(0x5f1b10c, [private_work])
assert allocation_requests == [(file_size, 0)] and not allocations and not freed and not writes
assert bytes(u.mem_read(source, file_size)) == original
assert bytes(u.mem_read(live_work, 0x6f0)) == before_live
assert bytes(u.mem_read(global_data, 0x8700)) == before_global
assert bytes(u.mem_read(system, 0x200)) == before_system
assert struct.unpack('<Q', u.mem_read(base+0xa4ff3a0, 8))[0] == 0

report = dict(native_sha256=hashlib.sha256(native).hexdigest(),
    ranges=[dict(start=hex(a), end=hex(b), sha256=hashlib.sha256(native[a:b]).hexdigest()) for a, b in ranges],
    original_input_sha256=hashlib.sha256(original).hexdigest(),
    methods='Original ctor + getUseMemSize + unlock delegates + createSystemFile/makeSystemSaveData + dtor; original HapWriteGameData entry captured; allocation/free contract emulated',
    private_abi_assignment='Only the owned file-buffer pointer privateWork+0x5e0 is assigned by the adapter after original ctor/getUseMemSize. This explicit ABI binding is not a native setter; no live-job, progress or global transaction flag is assigned.',
    limitation='Does not execute Android filesystem publication or prove live frontend snapshot readiness. These remain runtime gates.',
    cases=results, publication_allocation_failure=dict(original_return=-1, writer_calls=0,
        source_live_job_system_and_flags_unchanged=True, publication_buffer_cleared=True), hardware_tested=False)
outdir = ROOT / 'analysis/research/lobby_save_originals'
outdir.mkdir(exist_ok=True)
(outdir/'serializer_proposal.json').write_text(json.dumps(report, indent=2)+'\n')
assert fixture.read_bytes() == original
print('PASS: 16 original-native isolated serializer cases and actual publication-allocation failure; original ctor/dtor/snapshot accessors, one captured native writer; flags/live job/source/campaign unchanged')
print('Original native serializer proof: one isolated private buffer-pointer assignment; Android filesystem publication remains a device verification')
