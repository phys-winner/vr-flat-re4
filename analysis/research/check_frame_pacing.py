"""Offline synthetic timestamp, cancellation ownership and original ARM64 native-pacer checks. No ADB."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import struct
import tempfile

import frame_pacing as pacing

period = 16666667
origin = 1000000000
def packet(indices, refresh=period):
    return {"stdout": str(refresh) + "\n" + "\n".join(f"0 {origin + i * period} 0" for i in indices)}

stable = pacing.analyze_latency([packet([0]), packet([0, 1, 2]), packet([1, 2, 3, 4])], 60)
assert stable["frames"] == 4 and stable["observed_intervals_ms"]["count"] == 3
assert abs(stable["observed_pair_rate_hz"] - 60) < .001
assert stable["intervals_beyond_target_quantization"] == 0 and stable["overlapping_rows_deduplicated"] == 2
late = pacing.analyze_latency([packet([0]), packet([0, 1, 2, 4])], 60)
assert late["intervals_beyond_target_quantization"] == 1 and late["observed_intervals_ms"]["max"] > 33
fractional = pacing.analyze_latency([packet([0]), packet([0, 1, 2, 4, 5])], 45)
assert not fractional["integer_cadence"] and fractional["intervals_beyond_target_quantization"] == 0
gap = pacing.analyze_latency([packet([0]), packet([0, 1, 2]), packet([200, 201, 202])], 60)
assert gap["nonoverlapping_ring_snapshots"] == 1 and gap["observed_intervals_ms"]["max"] < 17
mixed = pacing.analyze_latency([packet([0]), packet([0, 1, 2], 8333333)], 60)
assert "integer_cadence" not in mixed and len(mixed["refresh_periods_ns"]) == 2
invalid = pacing.analyze_latency([{"stdout": "Permission denied\n"}, {"stdout": "16666667\n0 0 0\n0 9223372036854775807 0\nbad data\n"}], 60)
assert invalid["status"] == "no_valid_fresh_timestamps" and invalid["invalid_or_pending_rows"] == 3
initial_failure = pacing.analyze_latency([{"returncode": 1, **packet([0])}, packet([200, 201]), packet([200, 201, 202, 203])], 60)
assert initial_failure["failed_queries"] == 1 and initial_failure["frames"] == 2
log = """10-31 03:03:11.759 1 2 I RE4FIX-N: vkQueuePresentKHR #100 -> 0
10-31 03:03:13.427 1 2 I RE4FIX-N: vkQueuePresentKHR #200 -> 0
10-31 03:03:15.427 1 2 I RE4FIX-N: vkQueuePresentKHR #300 -> -1
10-31 03:03:17.427 1 2 I RE4FIX-N: vkQueuePresentKHR #400 -> 0
  23.123 [1] CAMTRACE nativeFrame=5 latchFrame=5 camYaw=2.0
  23.124 [1] CAMTRACE nativeFrame=5 latchFrame=4 camYaw=2.0
  23.125 [1] POSETRACE tid=1 step=1 frame=0 node=9 yaw=2 look=2
"""
analyzed = pacing.analyze_log(log)
assert analyzed["coarse_present_block_rates_hz"]["count"] == 1
assert abs(analyzed["coarse_present_block_rates_hz"]["p50"] - 100 / 1.668) < .001
assert analyzed["camera_reference_frames"] == 1 and analyzed["camera_native_latch_mismatches"] == 1
assert analyzed["ovr_frame_arguments"] == {0: 1}
parts = ("dumpsys", "SurfaceFlinger", "--latency", "SurfaceView[com.Armature.VR4](BLAST)';echo no#3")
assert tuple(shlex.split(pacing.shell_command(*parts))) == parts
plain_layer = "SurfaceView[com.Armature.VR4/com.epicgames.ue4.GameActivity](BLAST)#19841"
wrapped_layer = "RequestedLayerState{" + plain_layer + " parentId=19840}"
assert pacing.listed_layer_name(plain_layer) == plain_layer
assert pacing.listed_layer_name(wrapped_layer) == plain_layer
assert pacing.listed_layer_name("RequestedLayerState{" + plain_layer + "}") == plain_layer
assert pacing.listed_layer_name("RequestedLayerState{" + plain_layer
    + " parentId=19840 relativeParentId=19836 mirrorId={1,2,} !handle z=-2147483648 layerStack=2}") == plain_layer
for bad in (wrapped_layer[:-1], wrapped_layer + " extra", wrapped_layer.replace("parentId", "ownerId"),
            "RequestedLayerState{" + plain_layer + " unknown=bad#42}",
            "RequestedLayerState{" + plain_layer + " z=1 parentId=2}",
            "RequestedLayerState{" + plain_layer + " parentId=1 parentId=2}"):
    assert pacing.listed_layer_name(bad) is None
for listed in (plain_layer, wrapped_layer):
    for requested in (None, plain_layer, listed):
        assert pacing.select_layer([listed], requested) == (plain_layer, listed)
for ambiguous, requested in (([plain_layer, plain_layer + "1"], None), ([wrapped_layer], "not listed"),
                             (["RequestedLayerState{" + plain_layer + " unknown=1}"], None)):
    try:
        pacing.select_layer(ambiguous, requested)
        assert False
    except ValueError:
        pass
retained_layers = Path(__file__).resolve().parents[2] / "analysis/build/qa412_motion_font_push_watch/resume/p412_pacing_idle/layers.json"
if retained_layers.is_file():
    retained = json.loads(retained_layers.read_text(encoding="utf8"))
    assert pacing.select_layer(retained["stdout"].splitlines()) == (plain_layer, wrapped_layer)
print("PASS: exact AOSP RequestedLayerState wrapper decoding, ordinary Android names unchanged, retained p412 layer identity, unknown/ambiguous names rejected", flush=True)
try:
    pacing.shell_command("bad\nargument")
    assert False
except ValueError:
    pass
for invalid_duration in (0, 4, 31, float("nan")):
    try:
        pacing.perfetto_config(invalid_duration)
        assert False
    except ValueError:
        pass
assert "duration_ms: 20000" in pacing.perfetto_config(20)
print("PASS: fresh/pending/zero timestamp parsing, overlapping ring dedup, missing-poll edges, mixed refresh refusal, legal45-on60 cadence, sparse native/pose limitations and shell quoting", flush=True)

# Exercise capture control flow with a fake device and clock. No subprocess or device invocation can occur.
class FakeTime:
    now = 1000000000
    def monotonic_ns(self): return self.now
    def sleep(self, seconds): self.now += int(seconds * 1e9)

class FakeDevice:
    calls = []
    stopped = False
    pulled = False
    trace_path = ""
    serial = "offline-test-device"
    layer = "SurfaceView[com.Armature.VR4/com.epicgames.ue4.GameActivity](BLAST)#1"
    def __init__(self, adb, serial): assert serial == self.serial
    def result(self, parts, stdout="", code=0):
        self.calls.append(parts)
        return {"command": list(parts), "stdout": stdout, "stderr": "", "returncode": code}
    def call(self, *parts, **options):
        if parts[0] == "get-serialno": return self.result(parts, self.serial + "\n")
        assert parts[0] == "pull" and parts[1] == self.trace_path
        Path(parts[2]).write_bytes(b"offline-test-trace")
        self.pulled = True
        return self.result(parts)
    def shell(self, *parts, **options):
        if parts[0] == "pidof": return self.result(parts, "1234\n")
        if parts[:3] == ("dumpsys", "SurfaceFlinger", "--list"):
            return self.result(parts, getattr(self, "listed_layer", self.layer) + "\n")
        if parts[:3] == ("dumpsys", "SurfaceFlinger", "--latency"):
            assert parts == ("dumpsys", "SurfaceFlinger", "--latency", self.layer)
            i = (fake_clock.now - 1000000000) // 100000000
            return self.result(parts, packet([i, i + 1])["stdout"])
        if parts[0] == "perfetto" and "--background" in parts:
            self.trace_path = parts[-1]
            return self.result(parts, "420\n")
        if parts[:2] == ("cat", "/proc/420/cmdline"):
            return self.result(parts, "perfetto\0-o\0" + self.trace_path if not self.stopped else "", int(self.stopped))
        if parts[:2] == ("test", "-d"):
            assert parts[2] == "/proc/420"
            return self.result(parts, code=int(self.stopped))
        if parts[0] == "kill":
            assert parts == ("kill", "-INT", "420")
            self.stopped = True
        if parts[0] == "rm":
            assert parts == ("rm", "-f", self.trace_path) and self.pulled and self.stopped
        return self.result(parts)

old_device, old_time = pacing.Device, pacing.time
fake_clock = FakeTime()
pacing.Device, pacing.time = FakeDevice, fake_clock
try:
    with tempfile.TemporaryDirectory(prefix="re4-frame-capture-offline-") as temp:
        output = Path(temp) / "capture"
        pacing.capture(argparse.Namespace(adb=Path("unused"), serial=FakeDevice.serial, output=output,
            seconds=5, interval=.5, fps=60, layer=None, perfetto=True))
        receipt = json.loads((output / "capture.json").read_text())
        assert receipt["status"] == "captured" and receipt["trace_done"] and receipt["trace_pulled"]
        assert receipt["pid"] == 1234 and receipt["pid_after"] == "1234"
        assert len((output / "latency.jsonl").read_text().splitlines()) >= 10
        assert receipt["layer"] == FakeDevice.layer and receipt["layer_name_format"] == "plain"
        fake_clock.now = 1000000000
        FakeDevice.listed_layer = "RequestedLayerState{" + FakeDevice.layer + " parentId=9}"
        wrapped_output = Path(temp) / "capture_wrapped"
        pacing.capture(argparse.Namespace(adb=Path("unused"), serial=FakeDevice.serial, output=wrapped_output,
            seconds=5, interval=.5, fps=60, layer=FakeDevice.listed_layer, perfetto=False))
        wrapped_receipt = json.loads((wrapped_output / "capture.json").read_text())
        assert wrapped_receipt["layer"] == FakeDevice.layer
        assert wrapped_receipt["layer_list_entry"] == FakeDevice.listed_layer
        assert wrapped_receipt["layer_name_format"] == "RequestedLayerState.getDebugString"
finally:
    pacing.Device, pacing.time = old_device, old_time
assert not any(call[0] in ("am", "pm", "input", "settings") for call in FakeDevice.calls)
print("PASS: offline-only full capture lifecycle, selected serial/layer/PID, bounded duration and exact owned Perfetto stop/pull/delete; no game mutation commands", flush=True)

# Execute the original native Vulkan CPU pacer's interval arithmetic; activation on the installed APK remains unknown.
root = Path(__file__).resolve().parents[2]
native = (root / "apk_decoded/lib/arm64-v8a/libUE4.so").read_bytes()
assert hashlib.sha256(native).hexdigest() == "6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1"
instructions = native[0x65445ac:0x65445d0]
assert struct.unpack("<9I", instructions) == (0xb200e3e8, 0x1e612800, 0xf2e7f228, 0x1e623808,
                                              0x1e68386a, 0x1e6202a0, 0x9e670101, 0x1e602148, 0x1e610809)
assert struct.unpack_from("<I", native, 0x654460c)[0] == 0x9402622f
for at in (0xa40d410, 0xa40d414, 0xa40d418):
    assert struct.unpack_from("<I", native, at)[0] == 1
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM
from unicorn.arm64_const import UC_ARM64_REG_W21, UC_ARM64_REG_D9
machine = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
machine.mem_map(0x100000, 4096); machine.mem_write(0x100000, instructions)
for interval in (0, 1, 2, 3):
    machine.reg_write(UC_ARM64_REG_W21, interval)
    machine.emu_start(0x100000, 0x100024, count=9)
    seconds = struct.unpack("<d", struct.pack("<Q", machine.reg_read(UC_ARM64_REG_D9)))[0]
    assert abs(seconds - interval / 60) < 1e-14
print("PASS: original native Vulkan CPU pacing arithmetic is syncInterval/60; its VSync/force-pacing activation is a separate hardware-only question")
