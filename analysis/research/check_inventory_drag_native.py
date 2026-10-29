"""Read-only proof of authored inventory drag actions and placement gates."""
from pathlib import Path
import hashlib
import importlib.util
import mmap
import struct

root = Path(__file__).resolve().parents[2]
ranges = [
    (0x620cf98, 0x620cff4, "b04ba564f6d611d6007b3e2e7f37c71d41c803bbac7e0e085cebba6bba1a4f87"),
    (0x62e4e30, 0x62e6370, "40ebd324f53303ed59dbe84d9ff28421214a05121e1116a5ce7bda69a163bbd8"),
    (0x62e72e4, 0x62e7524, "82c64d49471743da56f250de10f2219a025dd9e73f3791c84f220fcca3902a07"),
    (0x62dfce4, 0x62e007c, "58650ba75b5168979d048464a3be84aa5409f4d5e786cc1d246b1d0ff7b2ce6a"),
    (0x620d90c, 0x620db10, "1ad122e03bbddb80ebf8a54be5c45801f8cc8fa2e1f29c7b73349ec31cc170ee"),
    (0x620d754, 0x620d90c, "601499e95aa466064568083c1e24928e596030dca027d386e7c40f4d83681c2f"),
    (0x6310f50, 0x6310f78, "93ee737157723ee4fdd7ca01a64e2ac69d556294b4403dd61ade21c960fca42d"),
    (0x63111f8, 0x6311220, "141ea95934e813adafb25b595d84a41f175a3615738d5a6f47d2f7b7cc3df838"),
    (0x6311220, 0x6311248, "bbdd93b6695f71d1edfd37c48dd46d110b3d0fa5918e0a245efdcf2a51bede59"),
    (0x6298468, 0x629865c, "84b0472e211b0017d5e71f9db626caa63e1b0cba4200269e5e3b14ae15d7e9a8"),
]
fields = [
    (0x9dc4c20, "UIState", 0x270),
    (0x9dc4a48, "GrabState", 0x2f0),
    (0x9dc4b38, "ActiveItem", 0x2c0),
    (0x9dc4b08, "TargetItem", 0x2c8),
    (0x9dc4ce0, "AttacheCase", 0x258),
    (0x9dc4cb0, "SecondaryAttacheCase", 0x260),
    (0x9dc3ba8, "Hand", 0x40),
    (0x9dc3b78, "ProjectedGridBounds", 0x48),
    (0x9dc3bd8, "ItemRotationOffset", 0x30),
    (0x9dc3c08, "ItemPositionOffset", 0x18),
    (0x9dc3c38, "GunVice", 0x10),
    (0x9dc3c68, "AttacheCase", 8),
    (0x9dc3c98, "InitialAttacheCase", 0),
    (0x9d6d598, "GrabbedHand", 0x248),
    (0x9d6d628, "ItemState", 0x240),
    (0x9d6d718, "Button", 0x220),
    (0x9d6d6b8, "AttacheCase", 0x230),
    (0x9e0fb28, "AllowedAction", 0x320),
]
calls = {
    0x620cfc8: 0x63bf700, 0x620cfe4: 0x63bf504,
    0x62e4f00: 0x62df1b8, 0x62e4f10: 0x633e52c,
    0x62e5018: 0x62e4118, 0x62e5054: 0x629a774,
    0x62e5eec: 0x620dfe8, 0x62e6118: 0x620e1f8,
    0x62e7414: 0x620d90c, 0x62e74c4: 0x64537a0,
    0x620d950: 0x620d754, 0x620dad4: 0x62e0230,
    0x620d7a8: 0x62df1b8, 0x620d888: 0x62e313c,
    0x62dfd14: 0x62199c8, 0x62dfda8: 0x619e894,
    0x6298580: 0x633d048, 0x629862c: 0x629b4f0,
    0x633c554: 0x64b07a8, 0x633c55c: 0x64b07e4,
}
with (root / "apk_decoded/lib/arm64-v8a/libUE4.so").open("rb") as stream:
    with mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as native:
        phoff = struct.unpack_from("<Q", native, 0x20)[0]
        stride, count = struct.unpack_from("<HH", native, 0x36)
        segments = [struct.unpack_from("<IIQQQQQQ", native, phoff+i*stride) for i in range(count)]
        def off(address, size=1):
            segment = next(s for s in segments if s[0] == 1 and s[3] <= address and address+size <= s[3]+s[5])
            return segment[2]+address-segment[3]
        def word(address):
            return struct.unpack_from("<I", native, off(address, 4))[0]
        rela = {a: c for a, b, c in struct.iter_unpack("<QQq", native[0x2e6c028:0x2e6c028+0x1146540]) if b == 1027}
        def string(address):
            start = off(address)
            return bytes(native[start:native.find(b"\0", start)]).decode("utf8")
        for start, end, expected in ranges:
            assert hashlib.sha256(native[off(start):off(start)+end-start]).hexdigest() == expected, hex(start)
        for address, name, offset in fields:
            assert string(rela[address]) == name, (hex(address), name)
            assert word(address+0x24) == offset, (name, hex(offset))
        assert rela[0x9d6d718+0x28] == 0x64d8e2c  # Button is typed UVR4UIButtonComponent.
        for params, table, names in [
            (0x9dc3ac8, 0x9dc3a68, ["Normal", "MoveItem", "DumpingOverflowItems", "UsingItem", "CombiningItem", "EquippingItem"]),
            (0x9dc0528, 0x9dc04a8, ["Equip", "Reload", "Combine", "Remove", "Use", "Examine", "Dump", "Move"]),
            (0x9e0ff10, 0x9e0fec0, ["Click", "Cancel", "Grip", "Face1", "Trigger"]),
        ]:
            assert rela[params+0x20] == table and word(params+0x28) == len(names)
            for index, name in enumerate(names):
                entry = table+index*16
                assert string(rela[entry]).split("::")[-1] == name
                assert struct.unpack_from("<q", native, off(entry+8, 8))[0] == index
        for address, target in calls.items():
            instruction = word(address)
            assert instruction & 0x7c000000 == 0x14000000, hex(address)
            immediate = instruction & 0x03ffffff
            if immediate & 0x02000000:
                immediate -= 0x04000000
            assert address+immediate*4 == target, (hex(address), hex(target))
        # Item button accepts exactly its authored action, rejecting other actions.
        assert [word(a) for a in range(0x637119c, 0x63711b8, 4)] == [
            0x39400028, 0x394c8009, 0x6b09011f, 0x54000041, 0x14000232, 0x2a1f03e0, 0xd65f03c0]
        # Move7 is a real appended action after item validity/type checks.
        assert word(0x62dfdd8) == 0x528000e9 and word(0x62dfde0) == 0x38356909
        # Do not confuse pure menu UI action handling with accepting physical props.
        assert (word(0x63113c0), word(0x63113c4)) == (0x2a1f03e0, 0xd65f03c0)
        # Pooled GameHand grip-release tail must not be assumed to be menu-owned.
        assert [word(a) for a in [0x6298584, 0x62985fc, 0x6298600]] == [0x37000560, 0x39734288, 0x35000188]
        assert (word(0x633d048), word(0x633d04c)) == (0x394cf400, 0xd65f03c0)
        # Call the normal input entry, not only the implementation which lacks this state update.
        assert [word(a) for a in [0x633c54c, 0x633c550, 0x633c558]] == [0x52800028, 0x390cf408, 0x390cf41f]
print("PASS: ten native body hashes, 18 typed fields, 19 enum names/values, 20 call targets and exact action filter/Move7/CanAcceptProps/pooled grip-release/normal InputGrip state guards")

spec = importlib.util.spec_from_file_location("inventory_locres_read", root / "analysis/research/locres_read.py")
locres = importlib.util.module_from_spec(spec)
spec.loader.exec_module(locres)
original = locres.read_locres((root / "analysis/research/VR4_patch_en_original.locres").read_bytes())
manual = next(e["text"] for e in original["entries"] if e["namespace"] == "Tutorials" and e["key"] == "HelpInfoInventory")
assert "Перемещение" in manual and "Кнопки Захвата" in manual and "Курок" in manual
print("PASS: authored original inventory manual distinguishes Grip movement from Trigger attach/use")
