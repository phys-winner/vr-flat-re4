"""Execute original Android file-handle constructor/seek/read/cleanup at >4GB ZIP offsets."""
from pathlib import Path
import mmap
import struct
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2, UC_ARM64_REG_X3, UC_ARM64_REG_LR, UC_ARM64_REG_PC, UC_ARM64_REG_SP

root = Path(__file__).resolve().parents[2]
with (root / 'apk_decoded/lib/arm64-v8a/libUE4.so').open('rb') as source, mmap.mmap(source.fileno(), 0, access=mmap.ACCESS_READ) as image:
    phoff = struct.unpack_from('<Q', image, 32)[0]
    phsize, count = struct.unpack_from('<HH', image, 54)
    segments = [struct.unpack_from('<IIQQQQQQ', image, phoff + i * phsize) for i in range(count)]
    def read(address, size):
        segment = next(p for p in segments if p[0] == 1 and p[3] <= address and address + size <= p[3] + p[5])
        offset = segment[2] + address - segment[3]
        return image[offset:offset + size]
    vm = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
    for start, size in ((0x6572000, 0x4000), (0xa372000, 0x1000), (0x6389000, 0x1000),
                        (0x9bf2000, 0x3000), (0x6615000, 0x1000), (0x10000000, 0x20000)):
        vm.mem_map(start, size)
    vm.mem_write(0x6572000, read(0x6572000, 0x4000))
    vm.mem_write(0xa372898, struct.pack('<QQ', 0x9e1dac0, 0x9e1db20))
    heap = 0x10002000
    descriptor = 17
    reads, closed, assets = [], [], []
    def result(value):
        vm.reg_write(UC_ARM64_REG_X0, value & 0xffffffffffffffff)
        vm.reg_write(UC_ARM64_REG_PC, vm.reg_read(UC_ARM64_REG_LR))
    def hook(emulator, address, size, _):
        global heap
        x0, x1, x2, x3 = [vm.reg_read(register) for register in (UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2, UC_ARM64_REG_X3)]
        if address == 0x63892b4:
            allocation = heap; heap += (x0 + 15) & ~15
            result(allocation)
        elif address == 0x9bf4330: result(15000000000 if x2 == 2 else x1)
        elif address == 0x9bf4340:
            assert x0 == descriptor and x2 <= 0x100000
            reads.append((x3, x2)); vm.mem_write(x1, bytes((x3 + i) & 255 for i in range(x2))); result(x2)
        elif address == 0x9bf3370: closed.append(x0); result(0)
        elif address == 0x9bf3040: assets.append(x0); result(0)
        elif address in (0x6615fd0, 0x63892d4): result(0)
    vm.hook_add(UC_HOOK_CODE, hook)
    stop = 0x1001f000
    def call(address, *arguments):
        vm.reg_write(UC_ARM64_REG_SP, 0x1001e000)
        vm.reg_write(UC_ARM64_REG_LR, stop)
        for register, value in zip((UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2, UC_ARM64_REG_X3), arguments): vm.reg_write(register, value)
        vm.emu_start(address, stop, count=10000)
        assert vm.reg_read(UC_ARM64_REG_PC) == stop
        return vm.reg_read(UC_ARM64_REG_X0)
    handle, string, output = 0x10000000, 0x10000100, 0x10001000
    for offset, length in ((5000000000, 4278831240), (9500000000, 4237413580)):
        call(0x657201c, handle, string, descriptor)
        owner, controller = struct.unpack('<QQ', vm.mem_read(handle + 8, 16))
        assert struct.unpack('<Q', vm.mem_read(owner + 0x10, 8))[0] == 0
        assert struct.unpack('<I', vm.mem_read(owner + 0x18, 4))[0] == descriptor
        # These are the same three fields set by the production ZIP hook after the original constructor.
        vm.mem_write(handle + 0x18, struct.pack('<QQQ', offset, length, offset))
        assert call(0x6573118, handle) == 0 and call(0x6573384, handle) == length
        assert call(0x657315c, handle, output, 12) == 1 and reads[-1] == (offset, 12)
        assert bytes(vm.mem_read(output, 12)) == bytes((offset + i) & 255 for i in range(12))
        assert call(0x6573118, handle) == 12
        assert call(0x6573128, handle, 7) == 1 and call(0x6573118, handle) == 7
        assert call(0x657315c, handle, output, 9) == 1 and reads[-1] == (offset + 7, 9)
        assert call(0x6573140, handle, 0) == 1 and call(0x6573118, handle) == length
        call(0x657338c, controller)
    assert closed == [descriptor, descriptor] and not assets
print('PASS: actual ARM64 raw-FD constructor, Tell/Seek/SeekFromEnd/Size/Read at 5GB and 9.5GB offsets; exact pread64 bytes and one descriptor close without AAsset close per native owner')
