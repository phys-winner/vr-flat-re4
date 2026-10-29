"""Compare every AI gate to the original instructions, including all live register state."""
import os
from pathlib import Path
import io
import mmap
import re
import struct
import subprocess
import tempfile
from elftools.elf.elffile import ELFFile
from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM
from capstone.arm64_const import ARM64_REG_X16, ARM64_REG_W16
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM
from unicorn import arm64_const as arm

root = Path(__file__).resolve().parents[2]
header = root / 'analysis/re4fix_enemy_aggression.h'
clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
sites = [0x5d701fc, 0x5d70228, 0x5d86798, 0x5d86858, 0x5d86928, 0x5d86bbc]
continuations = [0x5d7020c, 0x5d70238, 0x5d867a8, 0x5d86868, 0x5d8693c, 0x5d86bcc]
gate_names = ['timer7', 'timer10', 'attack_floor', 'attack_angle', 'attack_group', 'attack_cooldown']
with tempfile.TemporaryDirectory(prefix='enemy-aggression-') as temporary:
    work = Path(temporary)
    source = work / 'fixture.c'
    source.write_text('#include <string.h>\nstruct mobile_prefs {int aggression;}; static struct mobile_prefs g_mprefs;\n'
        'int patch_code(void*a,void*b);\n#define LOGI(...) ((void)0)\n#include "'
        + header.as_posix() + '"\nvoid install(unsigned long b){install_enemy_aggression(b);}\n')
    library = work / 'fixture.so'
    subprocess.run([str(clang), '--target=aarch64-linux-android23', '-O2', '-fPIC', '-shared', '-nostdlib', '-Werror',
        str(source), '-o', str(library)], check=True)
    helper = ELFFile(io.BytesIO(library.read_bytes()))
    symbols = {symbol.name: symbol['st_value'] for symbol in helper.get_section_by_name('.symtab').iter_symbols()}
    base = 0x40000000
    def load_helper(vm):
        for segment in helper.iter_segments():
            if segment['p_type'] != 'PT_LOAD': continue
            start = (base + segment['p_vaddr']) & ~4095
            end = (base + segment['p_vaddr'] + segment['p_memsz'] + 4095) & ~4095
            vm.mem_map(start, end - start)
            vm.mem_write(base + segment['p_vaddr'], segment.data())
        for section in helper.iter_sections():
            if section['sh_type'] != 'SHT_RELA': continue
            table = helper.get_section(section['sh_link'])
            for relocation in section.iter_relocations():
                target = base + relocation['r_offset']
                if relocation['r_info_type'] == 1027: value = base + relocation['r_addend']
                elif relocation['r_info_type'] in (1025, 257):
                    symbol = table.get_symbol(relocation['r_info_sym'])
                    if not symbol['st_value']: continue
                    value = base + symbol['st_value'] + relocation['r_addend']
                else: continue
                vm.mem_write(target, struct.pack('<Q', value))
    with (root / 'apk_decoded/lib/arm64-v8a/libUE4.so').open('rb') as native_file, mmap.mmap(native_file.fileno(), 0, access=mmap.ACCESS_READ) as native:
        elf = ELFFile(native_file)
        def read(address, size):
            segment = next(p for p in elf.iter_segments() if p['p_type'] == 'PT_LOAD' and p['p_vaddr'] <= address
                and address + size <= p['p_vaddr'] + p['p_filesz'])
            offset = segment['p_offset'] + address - segment['p_vaddr']
            return native[offset:offset + size]
        expected = re.search(r'original\[6\]\[4\] = \{(.*?)\n    \};', header.read_text(), re.S).group(1)
        words = [int(word, 16) for word in re.findall(r'0x[0-9a-f]+', expected)]
        for index, site in enumerate(sites): assert read(site, 16) == struct.pack('<4I', *words[index * 4:index * 4 + 4])
        disassembler = Cs(CS_ARCH_ARM64, CS_MODE_ARM); disassembler.detail = True
        for start, end in ((0x5d6fd50, 0x5d730f4), (0x5d866f0, 0x5d869a8), (0x5d869a8, 0x5d86c48)):
            for instruction in disassembler.disasm(read(start, end - start), start):
                used, written = instruction.regs_access()
                assert ARM64_REG_X16 not in used + written and ARM64_REG_W16 not in used + written, hex(instruction.address)
        vm = Uc(UC_ARCH_ARM64, UC_MODE_ARM); load_helper(vm)
        vm.mem_map(0x10000000, 0x20000)
        vm.mem_map(0x5d70000, 0x4000); vm.mem_write(0x5d70000, read(0x5d70000, 0x4000))
        vm.mem_map(0x5d86000, 0x1000); vm.mem_write(0x5d86000, read(0x5d86000, 0x1000))
        preference, work_pointer, gamework = 0x10000000, 0x10000100, 0x10002000
        vm.mem_write(work_pointer, struct.pack('<Q', gamework))
        vm.mem_write(base + symbols['g_mobile_enemy_pref'], struct.pack('<Q', preference))
        registers = [getattr(arm, 'UC_ARM64_REG_X' + str(index)) for index in range(31)]
        vectors = [getattr(arm, 'UC_ARM64_REG_Q' + str(index)) for index in range(32)]
        def setup(rank, enabled, flags, index):
            for number, register in enumerate(registers): vm.reg_write(register, 0x55550000 + number * 16)
            for number, register in enumerate(vectors): vm.reg_write(register, int.from_bytes(bytes([number + 128]) * 16, 'little'))
            vm.reg_write(arm.UC_ARM64_REG_SP, 0x1001f000)
            vm.reg_write(arm.UC_ARM64_REG_NZCV, flags << 28)
            vm.reg_write(arm.UC_ARM64_REG_FPCR, 0); vm.reg_write(arm.UC_ARM64_REG_FPSR, 0)
            vm.mem_write(preference, struct.pack('<I', enabled))
            vm.mem_write(gamework + 0x5024, bytes([rank]))
            vm.reg_write(arm.UC_ARM64_REG_X25, work_pointer)
            vm.reg_write(arm.UC_ARM64_REG_X21, work_pointer)
            vm.reg_write(arm.UC_ARM64_REG_X23, work_pointer)
            vm.reg_write(arm.UC_ARM64_REG_X8, gamework if index == 3 else work_pointer)
        def snapshot():
            return [vm.reg_read(register) for register in registers if register != arm.UC_ARM64_REG_X16] + \
                [vm.reg_read(register) for register in vectors] + \
                [vm.reg_read(register) for register in (arm.UC_ARM64_REG_SP, arm.UC_ARM64_REG_NZCV, arm.UC_ARM64_REG_FPCR, arm.UC_ARM64_REG_FPSR)]
        cases = 0
        for index, name in enumerate(gate_names):
            for rank in range(11):
                for enabled in (0, 1):
                    for flags in range(16):
                        setup(10 if enabled else rank, enabled, flags, index)
                        vm.emu_start(sites[index], continuations[index], count=30)
                        expected_state = snapshot()
                        setup(rank, enabled, flags, index)
                        vm.mem_write(base + symbols['g_mobile_enemy_continue'] + 8 * index, struct.pack('<Q', continuations[index]))
                        vm.emu_start(base + symbols['gate_enemy_' + name], continuations[index], count=60)
                        assert snapshot() == expected_state, (name, rank, enabled, flags)
                        assert bytes(vm.mem_read(gamework + 0x5024, 1)) == bytes([rank])
                        cases += 1
print('PASS:', cases, 'actual compiled AI-gate comparisons: all live GPR/FP/flags/SP preserved; option off exactly native, on scoped rank10 decisions; global adaptive rank unchanged; x16 absent from three complete native functions')
print('LIMIT: common Ganado group/approach/ranged scheduling only; unique boss AI remains authored')
