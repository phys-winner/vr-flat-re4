#!/usr/bin/env python3
"""Send real UHID gamepad reports through Android's `hid -` command (no root).

Adapted from the existing BL2 QA pad. Each invocation owns one HID process and
releases all controls before closing it; it never kills another HID session.
Steps: wait:MS, tap:BUTTON[:MS], hold:BUTTON, up:BUTTON, hat:DIR[:MS],
dpad:DIR[:MS], stick:LX,LY[,RX,RY] (-127..127), trig:L,R (0..255),
shot:FILENAME.png, dump:LABEL, disconnect. --output holds the reports, Android
input-device evidence, process diagnostics and screenshots.
"""
import argparse
import json
from pathlib import Path
import subprocess
import time
import uuid

BUTTONS = ["A", "B", "C", "X", "Y", "Z", "L1", "R1", "L2", "R2", "SELECT", "START", "MODE", "THUMBL", "THUMBR"]
HAT = {"up": 0, "upright": 1, "right": 2, "downright": 3, "down": 4, "downleft": 5, "left": 6, "upleft": 7}
DPAD = {"up": 0, "down": 1, "right": 2, "left": 3}


def descriptor():
    return [
        0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0x85, 0x01,
        0x05, 0x09, 0x19, 0x01, 0x29, 0x0F, 0x15, 0x00, 0x25, 0x01,
        0x75, 0x01, 0x95, 0x0F, 0x81, 0x02,
        0x75, 0x01, 0x95, 0x01, 0x81, 0x03,
        0x05, 0x01, 0x09, 0x39, 0x15, 0x00, 0x25, 0x07,
        0x35, 0x00, 0x46, 0x3B, 0x01, 0x65, 0x14,
        0x75, 0x04, 0x95, 0x01, 0x81, 0x42, 0x65, 0x00,
        0x75, 0x04, 0x95, 0x01, 0x81, 0x03,
        0x09, 0x90, 0x09, 0x91, 0x09, 0x92, 0x09, 0x93,
        0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x04, 0x81, 0x02,
        0x75, 0x04, 0x95, 0x01, 0x81, 0x03,
        0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35,
        0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x04, 0x81, 0x02,
        0x05, 0x02, 0x09, 0xC5, 0x09, 0xC4,
        0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02, 0xC0]


def parse_step(step):
    kind, separator, argument = step.partition(":")
    parts = argument.split(":")
    if kind == "wait":
        value = int(argument)
        assert 0 <= value <= 60000, "wait must be between 0 and 60000 ms"
        return kind, value
    if kind in ("tap", "hold", "up", "hat", "dpad"):
        name = parts[0].upper() if kind in ("tap", "hold", "up") else parts[0].lower()
        names = BUTTONS if kind in ("tap", "hold", "up") else HAT if kind == "hat" else DPAD
        assert name in names, f"unknown {kind} control: {name}"
        assert len(parts) <= (2 if kind in ("tap", "hat", "dpad") else 1)
        duration = int(parts[1]) if len(parts) == 2 else 120
        assert 1 <= duration <= 60000, "hold must be between 1 and 60000 ms"
        return kind, (name, duration)
    if kind in ("stick", "trig"):
        values = [int(value) for value in argument.split(",")]
        assert len(values) in ((2, 4) if kind == "stick" else (2,))
        low, high = (-127, 127) if kind == "stick" else (0, 255)
        assert all(low <= value <= high for value in values), f"{kind} out of range"
        return kind, values
    if kind in ("shot", "dump"):
        assert separator and argument and Path(argument).name == argument and argument not in (".", "..")
        return kind, argument
    assert kind == "disconnect" and not separator, f"unknown step: {step!r}"
    return kind, None


class Pad:
    def __init__(self, adb, serial, output, settle):
        self.adb, self.serial, self.output = adb, serial, output
        self.buttons, self.hat, self.dpad = 0, 15, 0
        self.sticks, self.triggers = [0, 0, 0, 0], [0, 0]
        self.proc = None
        self.pid = None
        self.pidfile = "/data/local/tmp/re4-hid-" + uuid.uuid4().hex + ".pid"
        self.events = (output / "reports.jsonl").open("w", encoding="utf8")
        self.diagnostics = (output / "hid_process.log").open("wb")
        self.started = time.monotonic()
        try:
            assert self.command("get-state").strip() == b"device", "selected ADB device unavailable"
            self.dump("before")
            self.proc = subprocess.Popen([adb, "-s", serial, "shell", "echo $$ > " + self.pidfile + "; exec hid -"],
                                         stdin=subprocess.PIPE, stdout=self.diagnostics, stderr=self.diagnostics)
            self.send({"id": 1, "command": "register", "name": "RE4 QA Gamepad", "vid": 0xFEED,
                       "pid": 0xC0DE, "bus": "usb", "descriptor": descriptor()})
            time.sleep(settle)
            self.pid = self.command("shell", "cat", self.pidfile).decode().strip()
            assert self.pid.isdecimal(), "could not identify owned HID process"
            assert self.proc.poll() is None, "Android hid tool exited; see hid_process.log"
            self.report()
            dump = self.dump("registered")
            assert b"RE4 QA Gamepad" in dump, "virtual HID was not registered as an Android input device"
        except BaseException:
            self.close()
            raise

    def command(self, *arguments):
        return subprocess.run([self.adb, "-s", self.serial, *arguments], check=True,
                              capture_output=True, timeout=20).stdout

    def dump(self, label):
        data = self.command("shell", "dumpsys", "input")
        (self.output / (label + "_input.txt")).write_bytes(data)
        return data

    def send(self, event):
        self.events.write(json.dumps({"elapsed": round(time.monotonic() - self.started, 4), **event}) + "\n")
        self.events.flush()
        self.proc.stdin.write((json.dumps(event) + "\n").encode())
        self.proc.stdin.flush()

    def report(self):
        data = [1, self.buttons & 255, (self.buttons >> 8) & 255, self.hat & 15, self.dpad & 15]
        data += [value & 255 for value in self.sticks] + self.triggers
        assert len(data) == 11
        self.send({"id": 1, "command": "report", "report": data})

    def close(self):
        if self.proc is not None:
            try:
                if self.proc.poll() is None and not self.proc.stdin.closed:
                    self.buttons = self.dpad = 0; self.hat = 15
                    self.sticks, self.triggers = [0, 0, 0, 0], [0, 0]
                    self.report()
                    self.proc.stdin.close()
                self.proc.wait(timeout=5)
            except (OSError, subprocess.TimeoutExpired):
                try:
                    if self.pid and self.pid.isdecimal():
                        cmdline = self.command("shell", "cat", "/proc/" + self.pid + "/cmdline")
                        if b"com.android.commands.hid" in cmdline:
                            self.command("shell", "kill", self.pid)
                except (OSError, subprocess.SubprocessError):
                    pass
                if self.proc.poll() is None:
                    self.proc.kill()
                    self.proc.wait(timeout=5)
            self.proc = None
        try:
            self.command("shell", "rm", "-f", self.pidfile)
            self.dump("after")
        finally:
            self.events.close()
            self.diagnostics.close()

    def run(self, step):
        kind, value = step
        if kind == "wait":
            time.sleep(value / 1000)
        elif kind in ("tap", "hold", "up"):
            name, duration = value
            bit = 1 << BUTTONS.index(name)
            if kind != "up":
                self.buttons |= bit; self.report()
            if kind == "tap":
                time.sleep(duration / 1000)
            if kind != "hold":
                self.buttons &= ~bit; self.report()
        elif kind in ("hat", "dpad"):
            name, duration = value
            if kind == "hat": self.hat = HAT[name]
            else: self.dpad = 1 << DPAD[name]
            self.report(); time.sleep(duration / 1000)
            self.hat = 15; self.dpad = 0; self.report()
        elif kind == "stick":
            self.sticks[:len(value)] = value; self.report()
        elif kind == "trig":
            self.triggers = value; self.report()
        elif kind == "shot":
            (self.output / value).write_bytes(self.command("exec-out", "screencap", "-p"))
        elif kind == "dump":
            self.dump(value)
        else:
            # Deliberately detach without a neutral report to check device-removal release.
            self.proc.stdin.close()
            self.proc.wait(timeout=5)
            self.proc = None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--adb", required=True, help="absolute verified adb executable")
    parser.add_argument("--serial", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--settle", type=float, default=7)
    parser.add_argument("--gap", type=int, default=250)
    parser.add_argument("steps", nargs="*")
    args = parser.parse_args()
    assert Path(args.adb).is_absolute() and Path(args.adb).is_file()
    assert 0 <= args.settle <= 60 and 0 <= args.gap <= 60000
    steps = [parse_step(step) for step in args.steps]
    assert all(step[0] != "disconnect" for step in steps[:-1]), "disconnect must be the final step"
    args.output.mkdir(parents=True, exist_ok=False)
    pad = Pad(args.adb, args.serial, args.output, args.settle)
    try:
        for source, step in zip(args.steps, steps):
            pad.run(step)
            print("done", source, flush=True)
            time.sleep(args.gap / 1000)
    finally:
        pad.close()


if __name__ == "__main__":
    main()
