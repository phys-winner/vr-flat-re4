"""Research only: execute the original Oodle ARM64 decoder in installed Unicorn."""
from pathlib import Path
import hashlib
import json
import struct
import sys
import contextlib
import io
import unicorn
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm64_const import *

ROOT = Path(__file__).resolve().parents[2]
assert sys.argv[1:] in ([], ['--title'], ['--actions'])
TITLE = sys.argv[1:] == ['--title']
ACTIONS = sys.argv[1:] == ['--actions']
OUTPUT = ROOT / ('analysis/research/action_ui_originals' if ACTIONS else 'analysis/research/title_ui_originals' if TITLE else 'analysis/research/health_ui_originals')
assert OUTPUT.resolve().is_relative_to((ROOT / 'analysis/research').resolve())
sys.path[:0] = [str(ROOT / 'analysis'), str(ROOT / 'analysis/research')]
from pak9 import Pak9
from inspect_ui_package import package

native = (ROOT / 'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
native_hash = hashlib.sha256(native).hexdigest()
assert native_hash == '6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
phoff = struct.unpack_from('<Q', native, 32)[0]
step, count = struct.unpack_from('<HH', native, 54)
segments = [struct.unpack_from('<IIQQQQQQ', native, phoff+i*step) for i in range(count)]
size = (max(s[3]+s[6] for s in segments if s[0] == 1)+4095) & ~4095
base = 0x10000000
loaded = bytearray(size)
for s in segments:
    if s[0] == 1:
        loaded[s[3]:s[3]+s[5]] = native[s[2]:s[2]+s[5]]
shoff = struct.unpack_from('<Q', native, 40)[0]
shsize, shcount = struct.unpack_from('<HH', native, 58)
sections = [struct.unpack_from('<IIQQQQIIQQ', native, shoff+i*shsize) for i in range(shcount)]
symsec = next(s for s in sections if s[1] == 11)
strings = sections[symsec[6]]
strdata = native[strings[4]:strings[4]+strings[5]]
symbols = []
for at in range(symsec[4], symsec[4]+symsec[5], symsec[9]):
    name, info, other, section, value, length = struct.unpack_from('<IBBHQQ', native, at)
    symbols.append((strdata[name:strdata.find(b'\0', name)].decode(), section, value))
imports = {}
stubbase = 0x71000000
for sec in sections:
    if sec[1] != 4:
        continue
    for at in range(sec[4], sec[4]+sec[5], sec[9]):
        address, info, addend = struct.unpack_from('<QQq', native, at)
        kind, symbol = info & 0xffffffff, info >> 32
        if kind == 1027:
            struct.pack_into('<Q', loaded, address, base+addend)
        elif kind in (1025, 1026, 257):
            name, defined, value = symbols[symbol]
            if defined:
                resolved = base+value+addend
            else:
                if name not in imports:
                    imports[name] = stubbase+16*len(imports)
                resolved = imports[name]+addend
            struct.pack_into('<Q', loaded, address, resolved)
u = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
u.mem_map(base, size)
u.mem_write(base, bytes(loaded))
del loaded
stack, buffers, tls, ret = 0x40000000, 0x50000000, 0x60000000, 0x70000000
for address, length in ((stack, 0x200000), (buffers, 0x2000000), (tls, 4096), (ret, 4096), (stubbase, 0x100000)):
    u.mem_map(address, length)
for address in imports.values():
    u.mem_write(address, bytes.fromhex('c0035fd6'))
names = {v:k for k,v in imports.items()}
used = {}

def external(uc, address, size, user):
    name = names[address]
    used[name] = used.get(name, 0)+1
    a, b, c = (uc.reg_read(UC_ARM64_REG_X0+i) for i in range(3))
    assert 0 <= c <= 16*1048576
    if name in ('memcpy', 'memmove'):
        uc.mem_write(a, bytes(uc.mem_read(b, c)))
    elif name == 'memset':
        uc.mem_write(a, bytes([b & 255])*c)
    else:
        raise AssertionError(('unimplemented external', name, hex(uc.reg_read(UC_ARM64_REG_LR))))
    uc.reg_write(UC_ARM64_REG_X0, a)
    uc.reg_write(UC_ARM64_REG_PC, uc.reg_read(UC_ARM64_REG_LR))

u.hook_add(UC_HOOK_CODE, external, begin=stubbase, end=stubbase+0xfffff)
u.reg_write(UC_ARM64_REG_TPIDR_EL0, tls)
u.reg_write(UC_ARM64_REG_CPACR_EL1, 3 << 20)

def call(address, args):
    sp = stack+0x1f0000
    u.reg_write(UC_ARM64_REG_SP, sp)
    u.reg_write(UC_ARM64_REG_LR, ret)
    for i,a in enumerate(args[:8]):
        u.reg_write(UC_ARM64_REG_X0+i, a)
    if len(args) > 8:
        u.mem_write(sp, struct.pack('<'+'Q'*(len(args)-8), *args[8:]))
    u.emu_start(base+address, ret, timeout=10000000, count=100000000)
    assert u.reg_read(UC_ARM64_REG_PC) == ret
    return u.reg_read(UC_ARM64_REG_X0)

fixture = ROOT / 'analysis/research/flat_ui_graph'
assert package((fixture/'OptionsConfirmScreen_BP.uasset').read_bytes(), (fixture/'OptionsConfirmScreen_BP.uexp').read_bytes()) == json.loads((fixture/'OptionsConfirmScreen_BP.package.json').read_text())
pak = Pak9(ROOT/'VRLOC.com.Armature.VR4/patch.201.com.Armature.VR4.obb', 'VR4/Content/Paks/pakchunk0-Android_ETC2.pak')
outputs, parsed, evidence = {}, {}, []
health_stems = ('VR4CameraCover_BP', 'UI_PauseMenu_Content_BP', 'UI_PauseMenu_Content_Trifold_Base_BP',
             'VR4Bio4PlayerPawn_BP', 'VR4GamePlayerPawn_BP', 'UI_HealthVignette_BP', 'UI_HealthVignette_Widget',
             'HUD_HeartbeatBG', 'HUD_HeartbeatFine', 'HUD_HeartbeatCaution', 'HUD_HeartbeatDanger')
title_stems = ('VR4MenuPawn_BP', 'VR4MenuPawnInWorld_BP', 'VR4LobbyGameMode_BP',
               'UI_MainMenu_BP', 'IntroPressButton', 'IntroWidget', 'MainMenu', 'Lobby', 'FrontendManager')
actions_stems = ('UI_PauseMenu_Content_BP','UI_PauseMenu_Content_Credits_BP','UI_PauseMenu_Content_Files_BP',
    'UI_PauseMenu_Content_NewGame_BP','UI_PauseMenu_Content_Trifold_Base_BP','UI_PauseMenu_Content_GameOptions_BP',
    'UI_PauseMenu_Content_InventoryFrame_BP','UI_PauseMenu_Content_KeysTreasures_BP','UI_PauseMenu_Content_LoadGame_BP',
    'UI_PauseMenu_Content_MapFrame_BP','UI_PauseMenu_Content_PlayingManual_BP','UI_PauseMenu_Content_ShootingRange_BP',
    'UI_PauseMenu_Content_Mercenaries_BP','UI_PauseMenu_Content_Options_BP','UI_Mercenaries_PauseMenu_Content_GameOptions_BP',
    'NewGameConfirmScreen_BP','NewGameCoverScreen_BP','NewGamePlusUIScreen_BP','ShootingRange_Panel_Button_BP',
    'ShootingRange_Configuration_Screen_BP','ShootingRange_Intro_Screen_BP','ShootingRange_Pause_Screen_BP',
    'ShootingRange_Quit_Screen_BP','ShootingRange_Score_Screen_BP','UI_Button_BP')
for stem in (actions_stems if ACTIONS else title_stems if TITLE else health_stems):
    asset_extension = '.umap' if stem == 'Lobby' else '.uasset'
    for extension in (asset_extension, '.uexp'):
        name = next(n for n in pak.entries if n.endswith('/'+stem+extension))
        e = pak.entries[name]
        assert e['method'] == 1 and pak.methods[1] == 'Oodle' and not e['encrypted'] and len(e['blocks']) == 1
        pak.f.seek(e['off'])
        local,_ = pak.rentry(pak.f.read(e['header_size']), 0)
        assert all(local[k] == e[k] for k in ('size', 'unc', 'method', 'hash', 'blocks', 'encrypted', 'cbs'))
        raw = pak.f.read(e['size'])
        assert hashlib.sha1(raw).digest() == e['hash'] and e['blocks'] == [(e['header_size'], e['header_size']+len(raw))]
        trials, native_results = [], []
        for repeat in range(2):
            u.mem_write(buffers, raw)
            compressor = call(0x96b6f6c, [buffers, len(raw), 0])
            needed = call(0x96b4b88, [compressor, e['unc']])
            assert 0 < needed < 16*1048576
            u.mem_write(buffers+0x10000, bytes(e['unc']))
            result = call(0x96b64e0, [buffers, len(raw), buffers+0x10000, e['unc'], 1, 1, 0, 0, 0, 0, 0, buffers+0x100000, needed, 3])
            assert result == e['unc'], (name, result, e['unc'])
            native_results.append(result)
            trials.append(bytes(u.mem_read(buffers+0x10000, result)))
        assert trials[0] == trials[1]
        outputs[stem+extension] = trials[0]
        evidence.append(dict(source=name, compressed_sha1=e['hash'].hex(), compressed_sha256=hashlib.sha256(raw).hexdigest(),
            bytes=len(trials[0]), sha256=hashlib.sha256(trials[0]).hexdigest(), exact_repeats=2, compressor=compressor,
            expected_raw_size=e['unc'], native_results=native_results, input_sha1_matches_index=True))
    parsed[stem] = package(outputs[stem+asset_extension], outputs[stem+'.uexp'])
pak.f.close()
pak.z.close()
assert set(used) == {'memset', 'memcpy'}
OUTPUT.mkdir(exist_ok=True)
for name,data in outputs.items():
    (OUTPUT/name).write_bytes(data)
for stem,item in parsed.items():
    (OUTPUT/(stem+'.package.json')).write_text(json.dumps(item, indent=2)+'\n')
import inspect_flat_bp as bp
bp.DIRECTORY = OUTPUT
script_fixtures = []
health_scripts = (('UI_HealthVignette_BP', 'AttachToHud', 32), ('UI_HealthVignette_BP', 'ExecuteUbergraph_UI_HealthVignette_BP', 5658), ('UI_HealthVignette_BP', 'Init', 1066), ('UI_HealthVignette_BP', 'SetInvisible', 32), ('UI_HealthVignette_BP', 'SetVisible', 85), ('UI_HealthVignette_BP', 'UpdateColors', 1403), ('UI_HealthVignette_BP', 'UpdateDamageVignette', 644), ('UI_HealthVignette_BP', 'UpdateStatus', 483), ('VR4Bio4PlayerPawn_BP', 'ExecuteUbergraph_VR4Bio4PlayerPawn_BP', 18448), ('VR4Bio4PlayerPawn_BP', 'Initialize', 281), ('VR4Bio4PlayerPawn_BP', 'SpawnUi', 446), ('VR4CameraCover_BP', 'CancelDamageVignette', 32), ('VR4CameraCover_BP', 'ExecuteUbergraph_VR4CameraCover_BP', 1119), ('VR4CameraCover_BP', 'PauseDamageVingette', 32), ('VR4CameraCover_BP', 'ResumeDamageVingette', 32), ('VR4CameraCover_BP', 'SetDamageVignetteScale', 326), ('VR4CameraCover_BP', 'UpdateDamageVignette', 181), ('VR4GamePlayerPawn_BP', 'ExecuteUbergraph_VR4GamePlayerPawn_BP', 338), ('VR4GamePlayerPawn_BP', 'Initialize', 32))
title_scripts = (('FrontendManager', 'TriggerGripPressed', 32),
                 ('FrontendManager', 'InpActEvt_Button A_K2Node_InputActionEvent_9', 83),
                 ('FrontendManager', 'LogoReady', 32),
                 ('FrontendManager', 'ExecuteUbergraph_FrontendManager', 10878))
actions_scripts = []
if ACTIONS:
    for stem,item in parsed.items():
        for export in item['exports']:
            name=export['name']
            if export['class_path']=='/Script/CoreUObject.Function' and (name in ('SubAction_A','StartNewGame','Initialize','HandleOnStartClick')
                or name.startswith('ExecuteUbergraph_') or 'OnUIComponentClicked' in name):
                payload=outputs[stem+'.uexp'][export['uexp_offset']:export['uexp_offset']+export['serial_size']]
                candidates=[(at+4,struct.unpack_from('<I',payload,at)[0]) for at in range(4,len(payload)-16)
                    if at+4+struct.unpack_from('<I',payload,at)[0]+12==len(payload) and payload[-13]==0x53]
                assert candidates,(stem,name)
                # Runtime and serialized byte counts can both satisfy the trailer arithmetic. Accept only a fully
                # parsed instruction stream whose exact relocation-expanded runtime size matches the original header.
                offset=None
                for candidate,count in sorted(candidates,key=lambda value:value[1],reverse=True):
                    try:
                        with contextlib.redirect_stdout(io.StringIO()):
                            bp.read_script(stem,name,candidate)
                    except (AssertionError,IndexError,KeyError,struct.error):
                        continue
                    offset=candidate;break
                assert offset is not None,(stem,name,candidates)
                actions_scripts.append((stem,name,offset))
for stem, name, offset in (actions_scripts if ACTIONS else title_scripts if TITLE else health_scripts):
    result = bp.read_script(stem, name, offset)
    script_fixtures.append(dict(stem=stem, name=name, disk_bytes=result['script_bytes'], runtime_bytes=result['runtime_bytes'], export_sha256=result['export']['sha256']))

receipt = dict(native_sha256=native_hash, decoder='original OodleLZ_Decompress +0x96b64e0', emulator='Unicorn '+unicorn.__version__,
    abi='original fourteen arguments: fuzzSafe=1, CRC=1, threadPhase=3; native scratch-size and chunk-ID getters',
    native_ranges={hex(start):dict(bytes=length, sha256=hashlib.sha256(native[start:start+length]).hexdigest())
        for start,length in ((0x96b64e0,0x6e0), (0x96b4b88,0x1c), (0x96b6f6c,0xdc), (0x5c02b0c,0x164))},
    package_parser_parity='exact dict equality with retained OptionsConfirmScreen_BP package JSON before decoding',
    runner_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), externals=used, files=evidence, script_fixtures=script_fixtures, hardware_tested=False)
(OUTPUT/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
print(json.dumps(dict(output=str(OUTPUT), files=len(evidence), native_sha256=native_hash, externals=used, hardware_tested=False), indent=2))
