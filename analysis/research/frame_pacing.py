"""Offline cadence analysis, or an explicitly requested bounded Android capture. Never launches the game."""
import argparse
from collections import Counter
import datetime
import json
import math
from pathlib import Path
import re
import shlex
import statistics
import subprocess
import time
import uuid

PACKAGE = "com.Armature.VR4"
PENDING = (1 << 63) - 1


def stats(values):
    values = sorted(values)
    if not values:
        return None
    return {"count": len(values), "min": values[0], "p50": statistics.median(values),
            "p95": values[max(0, math.ceil(len(values) * .95) - 1)], "max": values[-1]}


def latency_packet(text):
    lines = text.splitlines()
    period = int(lines[0]) if lines and re.fullmatch(r"\s*\d+\s*", lines[0]) else 0
    if not 4000000 <= period <= 50000000:
        period = 0
    rows, rejected = {}, 0
    for line in lines[1:]:
        if not line.strip():
            continue
        if not re.fullmatch(r"\s*-?\d+\s+-?\d+\s+-?\d+\s*", line):
            rejected += 1
            continue
        desired, actual, ready = map(int, line.split())
        if 0 < actual < PENDING:
            rows[actual] = (desired, actual, ready)
        else:
            rejected += 1
    return period, rows, rejected


def analyze_latency(packets, fps):
    baseline = None
    periods, presents, pairs = set(), set(), set()
    duplicates, rejected, gaps, failures = 0, 0, 0, 0
    previous = set()
    for index, packet in enumerate(packets):
        if packet.get("returncode", 0):
            failures += 1
            continue
        period, rows, bad = latency_packet(packet.get("stdout", ""))
        rejected += bad
        if period:
            periods.add(period)
        times = sorted(rows)
        if baseline is None and times:
            baseline = max(times)
        if previous and times and not previous.intersection(times):
            gaps += 1
        if times:
            previous = set(times)
        fresh = [t for t in times if baseline is not None and t > baseline]
        duplicates += sum(t in presents for t in fresh)
        presents.update(fresh)
        # Only intervals witnessed inside the same ring snapshot count. Missing polls cannot create fake hitches.
        pairs.update(zip(fresh, fresh[1:]))
    intervals = [(b - a) / 1e6 for a, b in sorted(pairs)]
    result = {"status": "observed" if intervals else "no_valid_fresh_timestamps", "frames": len(presents),
              "observed_intervals_ms": stats(intervals), "refresh_periods_ns": sorted(periods),
              "overlapping_rows_deduplicated": duplicates, "invalid_or_pending_rows": rejected,
              "nonoverlapping_ring_snapshots": gaps, "failed_queries": failures,
              "note": "Actual SurfaceFlinger column2; no FPS/jank verdict from absent data or sparse native present logs."}
    if intervals:
        result["observed_pair_rate_hz"] = len(intervals) * 1000 / sum(intervals)
    if intervals and len(periods) == 1:
        period = next(iter(periods))
        ratio = 1e9 / period / fps
        largest_legal = max(1, math.ceil(ratio - .01))
        result.update({"display_hz": 1e9 / period, "requested_fps": fps,
            "display_periods_per_target_frame": ratio, "integer_cadence": abs(ratio - round(ratio)) < .01,
            "intervals_in_display_periods": dict(sorted(Counter(round(t * 1e6 / period) for t in intervals).items())),
            "intervals_beyond_target_quantization": sum(t * 1e6 > (largest_legal + .25) * period for t in intervals)})
    return result


def timestamp(line):
    date = re.match(r"(\d\d)-(\d\d) (\d\d):(\d\d):(\d\d)\.(\d+)", line)
    if date:
        month, day, hour, minute, second, fraction = map(int, date.groups())
        return (datetime.datetime(2000, month, day, hour, minute, second).timestamp()
                + fraction / 10 ** len(date[6]))
    relative = re.match(r"\s*(\d+\.\d+)\s", line)
    return float(relative[1]) if relative else None


def analyze_log(text):
    previous = None
    rates, blocks, camera, mismatch, ovr_frames = [], [], {}, 0, Counter()
    for line_number, line in enumerate(text.splitlines(), 1):
        found = re.search(r"vkQueuePresentKHR #(\d+) -> (-?\d+)", line)
        now = timestamp(line)
        if found and now is not None:
            count, code = map(int, found.groups())
            if code == 0:
                if previous and count > previous[0] and now > previous[1]:
                    delta, seconds = count - previous[0], now - previous[1]
                    rates.append(delta / seconds)
                    blocks.append({"frames": delta, "seconds": seconds, "rate_hz": delta / seconds,
                                   "start_count": previous[0], "end_count": count,
                                   "start_line": previous[2], "end_line": line_number})
                previous = count, now, line_number
            else:
                previous = None
        if "CAMTRACE " in line:
            fields = dict(re.findall(r"([A-Za-z]+)=(-?[0-9.]+)", line))
            if "nativeFrame" in fields and "latchFrame" in fields:
                frame = int(fields["nativeFrame"])
                mismatch += fields["nativeFrame"] != fields["latchFrame"]
                camera.setdefault(frame, []).append(fields)
        if "POSETRACE " in line:
            found = re.search(r"frame=(-?\d+)", line)
            if found:
                ovr_frames[int(found[1])] += 1
    return {"coarse_present_block_rates_hz": stats(rates), "slowest_present_blocks": sorted(blocks, key=lambda b: b["rate_hz"])[:10],
            "camera_reference_frames": len(camera), "camera_reference_rows": sum(map(len, camera.values())),
            "camera_native_latch_mismatches": mismatch, "ovr_frame_arguments": dict(ovr_frames),
            "limits": ["Present-call return cadence is not display presentation; sparse blocks hide individual gaps.",
                       "CAMTRACE is the corrected GT weapon/camera reference, not a capture of the final render view.",
                       "OVR frame argument0 is not the native GFrameCounter and must not group pose samples."]}


def perfetto_config(seconds):
    if not 5 <= seconds <= 30:
        raise ValueError("duration must be 5..30 seconds")
    return f'''duration_ms: {int(seconds * 1000)}
buffers {{ size_kb: 32768 fill_policy: RING_BUFFER }}
data_sources {{ config {{ name: "android.surfaceflinger.frametimeline" }} }}
data_sources {{ config {{ name: "linux.ftrace" ftrace_config {{
  ftrace_events: "sched/sched_switch"
  ftrace_events: "sched/sched_waking"
  ftrace_events: "power/cpu_frequency"
  ftrace_events: "power/cpu_idle"
  atrace_categories: "gfx"
  atrace_categories: "view"
  atrace_categories: "input"
  atrace_apps: "{PACKAGE}"
}} }} }}
data_sources {{ config {{ name: "linux.process_stats" process_stats_config {{ scan_all_processes_on_start: true }} }} }}
'''


def shell_command(*parts):
    if any("\0" in str(p) or "\n" in str(p) for p in parts):
        raise ValueError("invalid shell argument")
    return " ".join(shlex.quote(str(p)) for p in parts)


def listed_layer_name(line):
    """Decode the exact AOSP debug wrapper; ordinary --list names remain byte-for-byte unchanged."""
    if not line or "\0" in line or "\n" in line or "\r" in line:
        return None
    if not line.startswith("RequestedLayerState{"):
        return line
    # AOSP listLayers prints getDebugString(), while dumpStats matches Layer::getName().
    # The original name ends in #sequence; then only this ordered debug metadata can follow.
    # Split once so a failed metadata suffix cannot be reinterpreted as part of a guessed name.
    head = re.match(r"RequestedLayerState\{(.+?#[0-9]+)(?= |\})", line)
    if not head:
        return None
    suffix = (r"(?: parentId=[0-9]+)?(?: relativeParentId=[0-9]+)?"
              r"(?: mirrorId=\{(?:[0-9]+,)+\})?(?: !handle)?"
              r"(?: z=-?[0-9]+)?(?: layerStack=[0-9]+)?\}")
    return head[1] if re.fullmatch(suffix, line[head.end():]) else None


def select_layer(lines, requested=None):
    records = [(raw, listed_layer_name(raw)) for raw in lines]
    choices = [(raw, name) for raw, name in records if name and PACKAGE in name and "SurfaceView" in name]
    if requested is not None:
        choices = [(raw, name) for raw, name in choices if requested in (raw, name)]
    else:
        blast = [(raw, name) for raw, name in choices if "BLAST" in name]
        choices = blast or choices
    if len(choices) != 1:
        raise ValueError("choose one exact current game SurfaceView --layer from layers.json; candidates="
                         + repr(choices))
    raw, name = choices[0]
    return name, raw


class Device:
    def __init__(self, adb, serial):
        if not adb.is_absolute() or not adb.is_file() or not re.fullmatch(r"[A-Za-z0-9_.:-]+", serial):
            raise ValueError("explicit existing absolute adb path and valid serial required")
        self.prefix = [str(adb), "-s", serial]

    def call(self, *parts, data=None, timeout=10):
        command = self.prefix + list(parts)
        r = subprocess.run(command, input=data, capture_output=True, timeout=timeout)
        return {"command": command, "returncode": r.returncode, "stdout": r.stdout.decode("utf8", "replace"),
                "stderr": r.stderr.decode("utf8", "replace")}

    def shell(self, *parts, **options):
        return self.call("shell", shell_command(*parts), **options)


def capture(args):
    device = Device(args.adb, args.serial)
    config = perfetto_config(args.seconds)
    if not .25 <= args.interval <= 1 or not 24 <= args.fps <= 240:
        raise ValueError("poll interval must be .25..1 second; FPS must be 24..240")
    args.output.mkdir(parents=True, exist_ok=False)
    metadata = {"serial": args.serial, "package": PACKAGE, "seconds": args.seconds, "fps": args.fps,
                "note": "Read-only game measurement; no start/stop/input/preferences or global latency reset."}
    trace_path = "/data/misc/perfetto-traces/re4-pacing-" + uuid.uuid4().hex + ".perfetto-trace"
    trace_pid = None
    trace_done = False
    def save(label, result):
        (args.output / (label + ".json")).write_text(json.dumps(result, indent=2) + "\n", encoding="utf8")
        return result
    def finish_trace():
        if not trace_pid:
            return False
        for attempt in range(5):
            live = device.shell("cat", "/proc/" + trace_pid + "/cmdline", timeout=2)
            if live["returncode"] != 0:
                # A denied cmdline read is not proof that the process exited.
                return device.shell("test", "-d", "/proc/" + trace_pid, timeout=2)["returncode"] == 1
            # A recycled PID belongs to somebody else; the original trace helper has exited.
            if not ("perfetto" in live["stdout"] and trace_path in live["stdout"]):
                metadata["trace_pid_recycled"] = True
                return True
            if attempt == 0:
                save("trace_stop", device.shell("kill", "-INT", trace_pid, timeout=2))
            time.sleep(.2)
        return False
    try:
        identity = save("serial", device.call("get-serialno"))
        if identity["returncode"] or identity["stdout"].strip() != args.serial:
            raise RuntimeError("selected serial identity mismatch")
        pid_result = save("pid", device.shell("pidof", PACKAGE))
        pid = pid_result["stdout"].strip()
        if pid_result["returncode"] or not pid.isdecimal():
            raise RuntimeError("exactly one running game PID required")
        metadata["pid"] = int(pid)
        layers = save("layers", device.shell("dumpsys", "SurfaceFlinger", "--list"))["stdout"].splitlines()
        layer, raw_layer = select_layer(layers, args.layer)
        metadata["layer"] = layer
        metadata["layer_list_entry"] = raw_layer
        metadata["layer_name_format"] = "RequestedLayerState.getDebugString" if raw_layer != layer else "plain"
        for label, command in (("display_before", ("dumpsys", "display")),
                               ("thermal_before", ("dumpsys", "thermalservice")),
                               ("threads_before", ("ps", "-T", "-p", pid)),
                               ("process_stat", ("cat", "/proc/" + pid + "/stat")),
                               ("uptime", ("cat", "/proc/uptime")),
                               ("clock_ticks", ("getconf", "CLK_TCK"))):
            save(label, device.shell(*command))
        (args.output / "trace_config.pbtxt").write_text(config, encoding="utf8")
        if args.perfetto:
            save("perfetto_query", device.shell("perfetto", "--query"))
            started = save("perfetto_start", device.shell("perfetto", "--txt", "-c", "-", "--background", "-o", trace_path,
                                                          data=config.encode(), timeout=15))
            if started["returncode"]:
                raise RuntimeError("Perfetto start failed; see perfetto_start.json")
            pids = [s.strip() for s in (started["stdout"] + "\n" + started["stderr"]).splitlines() if s.strip().isdecimal()]
            trace_pid = pids[-1] if len(pids) == 1 else None
            metadata["trace_pid"] = trace_pid
        started_ns = time.monotonic_ns()
        metadata["host_start_monotonic_ns"] = started_ns
        with (args.output / "latency.jsonl").open("w", encoding="utf8") as stream:
            index = 0
            while True:
                start_ns = time.monotonic_ns()
                result = device.shell("dumpsys", "SurfaceFlinger", "--latency", layer)
                result.update(index=index, host_start_ns=start_ns, host_end_ns=time.monotonic_ns())
                stream.write(json.dumps(result) + "\n"); stream.flush()
                index += 1
                elapsed = (time.monotonic_ns() - started_ns) / 1e9
                if elapsed >= args.seconds:
                    break
                time.sleep(max(0, min(args.seconds - elapsed, index * args.interval - elapsed)))
        save("display_after", device.shell("dumpsys", "display"))
        save("thermal_after", device.shell("dumpsys", "thermalservice"))
        save("native_log", device.shell("logcat", "-b", "main", "-d", "-v", "threadtime", "-v", "monotonic",
                                        "--pid", pid, "-t", "10000", "RE4FIX-N:I", "*:S"))
        metadata["pid_after"] = save("pid_after", device.shell("pidof", PACKAGE))["stdout"].strip()
        if args.perfetto:
            time.sleep(1)
            trace_done = finish_trace()
            metadata["trace_completion"] = "helper_exited" if trace_done else "duration_elapsed_completion_unconfirmed"
            pulled = save("trace_pull", device.call("pull", trace_path, str(args.output / "trace.perfetto-trace"), timeout=20))
            metadata["trace_pulled"] = pulled["returncode"] == 0
        metadata["status"] = "captured"
    except BaseException as error:
        metadata["status"] = "error"
        metadata["error"] = str(error)
        raise
    finally:
        try:
            if args.perfetto and trace_pid and not trace_done:
                trace_done = finish_trace()
            if args.perfetto and trace_done and metadata.get("trace_pulled"):
                removed = save("trace_cleanup", device.shell("rm", "-f", trace_path))
                if removed["returncode"]:
                    metadata["retained_remote_trace"] = trace_path
            elif args.perfetto:
                metadata["retained_remote_trace"] = trace_path
        except (OSError, subprocess.SubprocessError) as error:
            metadata["cleanup_error"] = str(error)
            metadata["retained_remote_trace"] = trace_path
        metadata["trace_done"] = trace_done if args.perfetto else None
        (args.output / "capture.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf8")
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="mode", required=True)
    cap = sub.add_parser("capture", help="device operation; run only when device testing is authorized")
    cap.add_argument("--adb", type=Path, required=True); cap.add_argument("--serial", required=True)
    cap.add_argument("--output", type=Path, required=True); cap.add_argument("--layer")
    cap.add_argument("--seconds", type=float, default=20); cap.add_argument("--interval", type=float, default=.5)
    cap.add_argument("--fps", type=float, required=True); cap.add_argument("--perfetto", action="store_true")
    ana = sub.add_parser("analyze", help="offline only")
    ana.add_argument("--capture", type=Path); ana.add_argument("--log", action="append", type=Path, default=[])
    ana.add_argument("--fps", type=float, default=60); ana.add_argument("--output", type=Path)
    cfg = sub.add_parser("config", help="offline only")
    cfg.add_argument("--seconds", type=float, default=20)
    args = parser.parse_args()
    if args.mode == "capture":
        result = capture(args)
    elif args.mode == "config":
        print(perfetto_config(args.seconds)); return
    else:
        if not math.isfinite(args.fps) or not 24 <= args.fps <= 240:
            parser.error("FPS must be 24..240")
        result = {"logs": {str(p): analyze_log(p.read_text(encoding="utf8", errors="replace")) for p in args.log}}
        if args.capture:
            packets = [json.loads(s) for s in (args.capture / "latency.jsonl").read_text(encoding="utf8").splitlines() if s]
            result["surface"] = analyze_latency(packets, args.fps)
            native_log = args.capture / "native_log.json"
            if native_log.is_file():
                result["captured_log"] = analyze_log(json.loads(native_log.read_text(encoding="utf8"))["stdout"])
        if args.output:
            args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
