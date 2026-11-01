"""Offline checks for the real-HID QA harness; never contacts an Android device."""
import importlib.util
from pathlib import Path

path = Path(__file__).with_name("re4_virtual_gamepad.py")
spec = importlib.util.spec_from_file_location("re4_virtual_gamepad", path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
assert module.descriptor()[:8] == [5, 1, 9, 5, 0xA1, 1, 0x85, 1]
assert module.descriptor()[-1] == 0xC0
assert module.parse_step("stick:-127,127,0,0") == ("stick", [-127, 127, 0, 0])
assert module.parse_step("trig:255,0") == ("trig", [255, 0])
assert module.parse_step("tap:START:30") == ("tap", ("START", 30))
assert module.parse_step("hat:down") == ("hat", ("down", 120))
assert module.parse_step("dpad:left:300") == ("dpad", ("left", 300))
assert module.parse_step("disconnect") == ("disconnect", None)
for invalid in ("tap:NO", "up:A:1", "wait:60001", "wait:-1", "trig:1", "trig:1,2,3", "trig:256,0",
                "stick:0", "stick:0,0,0", "stick:-128,0", "shot:../x.png", "dump:..", "disconnect:x", "unknown"):
    try:
        module.parse_step(invalid)
    except (AssertionError, ValueError):
        pass
    else:
        raise AssertionError(invalid)

pad = module.Pad.__new__(module.Pad)
pad.buttons = (1 << 0) | (1 << 14); pad.hat = 15; pad.dpad = 0
pad.sticks = [-127, 0, 127, -1]; pad.triggers = [0, 255]
events = []
pad.send = events.append
pad.report()
assert events == [{"id": 1, "command": "report", "report": [1, 1, 64, 15, 0, 129, 0, 127, 255, 0, 255]}]
print("PASS: HID descriptor/report framing, signed stick bytes, trigger/button state and bounded/rejected scenario inputs; no device contact")
