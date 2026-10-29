"""Root-only final identity audit and exact pre-session save restoration.

Examples (only the exclusive device operator runs commands without --offline):
  python -B analysis/research/finalize_mobile_release.py audit --tag p372
  python -B analysis/research/finalize_mobile_release.py restore --tag p372
Offline audit never opens ADB. Neither command launches the application.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import subprocess
import time
import zipfile

ROOT = Path(__file__).resolve().parents[2]
PKG = "com.Armature.VR4"
SERIAL = os.environ.get("ANDROID_SERIAL", "")
ADB = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'platform-tools/adb.exe'))
JAVA = Path(os.path.join(os.environ['JAVA_HOME'], 'bin/java.exe'))
SIGNER = Path(os.path.join(os.environ['RE4_TOOLS_DIR'], 'apksigner.jar'))
ZIPALIGN = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'build-tools/36.0.0/zipalign.exe'))
CERT = "a40da80a59d170caa950cf15c18c454d47a39b26989d8b640ecd745ba71bf5dc"
FILES = f"/sdcard/Android/data/{PKG}/files"
BASELINE = ROOT / "analysis/build/qa365/saves_before_gameplay"
MAINTENANCE_BASELINE = ROOT / "analysis/build/qa374/saves_before"
TARGETS = {
    "savegame00.sav": (f"{FILES}/savegame00.sav", 2079352,
                       "33a488865638a0bc01a8f3cfe7b29b35a80bcf8e6015db36c763ad043aa343b8"),
    "System.sav": (f"{FILES}/UE4Game/VR4/VR4/Saved/SaveGames/System.sav", 2065,
                   "5c94c47da61b45788f35c27367f92e1468af63c332bb8a8fffa784a29fb534b7"),
}
TEMP_FILES = ("/data/local/tmp/re4cmd.txt", "/data/local/tmp/re4-TouchProbe.jar",
              "/data/local/tmp/re4-qa370-savegame00.sav", "/data/local/tmp/re4-qa370-System.sav")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def maintenance_release(tag):
    require(re.fullmatch(r"p[1-9][0-9]{2,}", tag), "Invalid release tag")
    return int(tag[1:]) >= 374


def release_counts(tag):
    if maintenance_release(tag):
        return 80, 3303, 57 if int(tag[1:]) >= 381 else 56
    return 78, 3302, 50


def digest(data):
    return hashlib.sha256(data).hexdigest()


def file_digest(path):
    with Path(path).open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def utc():
    return datetime.now(timezone.utc).isoformat()


def local(relative):
    path = (ROOT / str(relative).replace("\\", "/")).resolve()
    require(path.is_relative_to(ROOT.resolve()), f"Path outside workspace: {relative}")
    return path


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def durable_json(path, data):
    with Path(path).open("w", encoding="utf-8", newline="\n") as target:
        target.write(json.dumps(data, indent=2, ensure_ascii=False) + "\n")
        target.flush()
        os.fsync(target.fileno())


def run(command):
    result = subprocess.run([str(part) for part in command], capture_output=True, timeout=55)
    require(result.returncode == 0, f"Command failed ({result.returncode}): {command}\n"
            + result.stderr.decode("utf-8", errors="replace"))
    return result.stdout.decode("utf-8", errors="replace")


class Device:
    def __init__(self, serial):
        require(bool(SERIAL) and serial == SERIAL, "Set ANDROID_SERIAL to the explicitly selected device")
        self.prefix = [str(ADB), "-s", serial]
        require(self.call("get-state").strip() == "device", "Device is not ready")

    def call(self, *args, binary=False, check=True):
        result = subprocess.run(self.prefix + list(args), capture_output=True, timeout=55)
        if check:
            require(result.returncode == 0, f"ADB failed: {args}\n"
                    + result.stderr.decode("utf-8", errors="replace"))
        return result.stdout if binary else result.stdout.decode("utf-8", errors="strict")

    def pid(self):
        value = self.call("shell", "pidof", PKG, check=False).strip()
        require(not value or re.fullmatch(r"\d+(?: \d+)*", value), f"Invalid PID: {value}")
        return value

    def stop(self):
        self.call("shell", "am", "force-stop", PKG)
        for _ in range(40):
            if not self.pid():
                return
            time.sleep(.25)
        raise RuntimeError("Application did not stop; no save may be written")

    def read(self, path):
        return self.call("exec-out", "cat", path, binary=True)

    def prefs(self):
        return self.call("exec-out", "run-as", PKG, "cat", "re4_mobile_prefs_v2", binary=True)

    def metadata(self, path):
        # Check the existing file's ownership/mode before and after push, without chmod/chown.
        value = self.call("shell", "stat", "-c", "%a:%u:%g", path).strip()
        require(re.fullmatch(r"[0-7]{3,4}:\d+:\d+", value), f"Invalid file metadata: {value}")
        return value

    def remote_digest(self, path):
        value = self.call("exec-out", "su", "-c", "sha256sum " + shlex.quote(path)).strip()
        require(re.fullmatch(r"[a-f0-9]{64}\s+" + re.escape(path), value),
                f"Invalid remote hash response: {value}")
        return value.split()[0]

    def push(self, source, target):
        self.call("push", str(source), target)

    def save_paths(self):
        paths = self.call("shell", "find", FILES, "-type", "f").splitlines()
        require(all(path.startswith(FILES + "/") and ".." not in PurePosixPath(path).parts
                    for path in paths), "Unexpected external-files path")
        return sorted(path for path in paths if path.lower().endswith(".sav"))

    def cleanup(self):
        # These are the complete four task-owned temporary paths, never a directory/glob.
        self.call("shell", "rm", "-f", *TEMP_FILES)
        for path in TEMP_FILES:
            result = self.call("shell", "sh", "-c", shlex.quote(
                "if [ -e " + shlex.quote(path) + " ]; then echo exists; fi")).strip()
            require(not result, f"Temporary file remains: {path}")


def installed_identity(device, manifest):
    paths = device.call("shell", "pm", "path", PKG).strip()
    match = re.fullmatch(r"package:(/data/app/[A-Za-z0-9_+=.~/-]+/base\.apk)", paths)
    require(match is not None, "Expected one installed package base APK")
    base = match[1]
    helper = base.rsplit("/", 1)[0] + "/lib/arm64/libre4fix.so"
    apk_hash = device.remote_digest(base)
    helper_hash = device.remote_digest(helper)
    require(apk_hash == manifest["apk"]["sha256"], "Installed APK differs from frozen candidate")
    require(helper_hash == manifest["library"]["sha256"], "Installed helper differs from candidate")
    return dict(base=base, apk_sha256=apk_hash, helper=helper, helper_sha256=helper_hash,
                certificate="Identical complete APK to locally verified signer certificate")


def source_audit(tag):
    native_count, android_count, check_count = release_counts(tag)
    manifest_path = local(f"analysis/research/{tag}_source_build_manifest.json")
    manifest = read_json(manifest_path)
    require(manifest["candidate"] == tag, "Manifest tag mismatch")
    frozen = local(f"analysis/build/{tag}_source")
    inputs = manifest["inputs"]
    require(len(inputs) == native_count, f"Expected this release's {native_count} native inputs")
    names = set()
    for entry in inputs:
        source = local(entry["path"])
        name = source.relative_to(ROOT / "analysis").as_posix()
        require(name not in names, f"Duplicate native input: {name}")
        names.add(name)
        snapshot = frozen / name
        for path in (source, snapshot):
            require(path.stat().st_size == entry["bytes"] and file_digest(path) == entry["sha256"],
                    f"Native input mismatch: {path}")
    closure, pending = set(), ["re4fix.c", "re4fix_overlay.c"]
    while pending:
        name = pending.pop()
        if name in closure:
            continue
        closure.add(name)
        path = frozen / name
        require(path.is_file(), f"Missing frozen include: {name}")
        text = path.read_text(encoding="utf-8")
        require("RE4_QA_GRENADE_FIXTURE" not in text and Path(name).name != "qa_grenade_fixture.h",
                f"Production source contains the QA grenade fixture: {name}")
        for include in re.findall(r'^\s*#\s*include\s*"([^"]+)"', text, re.M):
            child = (path.parent / include).resolve()
            require(child.is_relative_to(frozen.resolve()), "Include escapes frozen tree")
            pending.append(child.relative_to(frozen).as_posix())
    require(closure == names, f"Native include closure mismatch: {closure ^ names}")
    android_path = local("analysis/research/mobile_android_build.json")
    android = read_json(android_path)
    require(len(android["inputs"]) == android_count, f"Expected this release's {android_count} Android inputs")
    for name, expected in android["inputs"].items():
        require(file_digest(local(name)) == expected, f"Android input mismatch: {name}")
    base = local(manifest["base"]["path"])
    require(file_digest(base) == manifest["base"]["sha256"] == android["apk_sha256"],
            "Android base identity mismatch")
    helper = frozen / "libre4fix.so"
    require(helper.stat().st_size == manifest["library"]["bytes"]
            and file_digest(helper) == manifest["library"]["sha256"], "Frozen helper mismatch")
    apk = local(manifest["apk"]["path"])
    require(apk.stat().st_size == manifest["apk"]["bytes"]
            and file_digest(apk) == manifest["apk"]["sha256"], "APK identity mismatch")
    with zipfile.ZipFile(apk) as package, zipfile.ZipFile(base) as original:
        require(digest(package.read("lib/arm64-v8a/libre4fix.so")) == manifest["library"]["sha256"],
                "Packaged helper mismatch")
        base_names = {name for name in original.namelist() if not name.startswith("META-INF/")}
        apk_names = {name for name in package.namelist() if not name.startswith("META-INF/")}
        require(apk_names == base_names | {"lib/arm64-v8a/libre4fix.so"}, "Unexpected APK entries")
        for name in base_names - {"lib/arm64-v8a/libre4fix.so"}:
            require(package.read(name) == original.read(name), f"APK/base content mismatch: {name}")
    check_path = local(f"analysis/build/qa{tag[1:]}/source_checks.json")
    checks = read_json(check_path)["checks"]
    require(len(checks) == check_count and len({row["name"] for row in checks}) == check_count,
            f"Expected {check_count} unique source check receipts")
    for row in checks:
        output = check_path.parent / f"check_{row['name']}.log"
        require(row["code"] == 0 and file_digest(output) == row["output_sha256"],
                f"Source check receipt/output mismatch: {row['name']}")
    signed = run([JAVA, "-jar", SIGNER, "verify", "--verbose", "--print-certs", apk])
    for version in (1, 2, 3):
        require(re.search(rf"Verified using v{version} scheme[^\n]*: true", signed),
                f"APK signature scheme v{version} failed")
    require(f"Signer #1 certificate SHA-256 digest: {CERT}" in signed, "Wrong signing certificate")
    aligned = run([ZIPALIGN, "-c", "-v", "4", apk])
    require("Verification successful" in aligned, "ZIP alignment failed")
    return manifest, dict(schema=1, utc=utc(), candidate=tag, native_inputs=native_count, android_inputs=android_count,
                         native_frozen_canonical_and_include_closure=True,
                         manifest_sha256=file_digest(manifest_path), android_receipt_sha256=file_digest(android_path),
                         source_checks_sha256=file_digest(check_path), source_checks=check_count,
                         tests_rerun=False, apk=manifest["apk"], library=manifest["library"],
                         certificate_sha256=CERT, signatures=[1, 2, 3], zipalign=4,
                         apksigner_output_sha256=digest(signed.encode()),
                         zipalign_output_sha256=digest(aligned.encode()),
                         hardware_acceptance="Not inferred; historical evidence retains its original version label")


def restore(device, tag, out, manifest, baseline=None):
    family = "qa374" if maintenance_release(tag) else "qa365"
    if baseline is None:
        baseline = MAINTENANCE_BASELINE if family == "qa374" else BASELINE
    # Both retained baselines have the same exact two authorized original save identities.
    for name, (_, size, expected) in TARGETS.items():
        path = baseline / name
        require(path.stat().st_size == size and file_digest(path) == expected,
                f"Wrong {family} baseline: {name}")
    identity = installed_identity(device, manifest)
    archive = out / "saves_before_final_restore"
    require(not archive.exists(), "Existing final restore archive must never be overwritten")
    require(not (out / "save_restore_receipt.json").exists(), "Existing final restore receipt must never be overwritten")
    prior_pid = device.pid()
    device.stop()
    archive.mkdir()
    receipt_path = out / "save_restore_receipt.json"
    receipt = dict(schema=1, utc=utc(), candidate=tag, device=SERIAL, prior_pid=prior_pid,
                   installed=identity, status="ARCHIVING", baseline=("qa374/saves_before" if family == "qa374"
                                                                    else "qa365/saves_before_gameplay"),
                   files=[], extra_save_files_preserved=[], removed_task_helpers=[], app_left_stopped=False)
    durable_json(receipt_path, receipt)
    try:
        prefs = device.prefs()
        values = dict(line.split("=", 1) for line in prefs.decode().splitlines() if line.strip())
        if not maintenance_release(tag):
            require(values.get("gyro", "0") == "0", "Restore QA gyro setting in game UI before finalization")
        (archive / "re4_mobile_prefs_v2.before").write_bytes(prefs)
        saves = device.save_paths()
        expected_targets = {row[0] for row in TARGETS.values()}
        require(expected_targets.issubset(saves), "Missing an existing baseline target; no save was written")
        receipt["extra_save_files_preserved"] = sorted(set(saves) - expected_targets)
        # Archive every current native .sav before the first device write, including any extra QA slot.
        for index, target in enumerate(saves):
            name = next((name for name, row in TARGETS.items() if row[0] == target), f"extra_{index:03d}.sav")
            current = device.read(target)
            require(0 < len(current) <= 16 * 1024 * 1024, f"Unexpected save size: {target}")
            archived = archive / name
            with archived.open("xb") as file:
                file.write(current)
                file.flush()
                os.fsync(file.fileno())
            require(file_digest(archived) == digest(current), "Host archive verification failed")
            receipt["files"].append(dict(name=name, device_path=target, archived_bytes=len(current),
                                         archived_sha256=digest(current), mode_uid_gid=device.metadata(target)))
        receipt["prefs_sha256"] = digest(prefs)
        receipt["status"] = "ARCHIVED_BEFORE_ANY_DEVICE_WRITE"
        durable_json(receipt_path, receipt)
        for name, (target, size, expected) in TARGETS.items():
            require(not device.pid(), "Game resumed; refusing to write save")
            row = next(row for row in receipt["files"] if row["device_path"] == target)
            device.push(baseline / name, target)
            restored = device.read(target)
            require(len(restored) == size and digest(restored) == expected, f"Restored save mismatch: {name}")
            require(device.metadata(target) == row["mode_uid_gid"], f"Save ownership/mode changed: {name}")
            row.update(restored_bytes=size, restored_sha256=expected, reread_verified=True)
            durable_json(receipt_path, receipt)
        require(device.prefs() == prefs, "Preferences changed during restoration")
        require(not device.pid(), "Game resumed during restoration")
        device.cleanup()
        receipt.update(status="PASS", removed_task_helpers=list(TEMP_FILES), app_left_stopped=True,
                       preferences_byte_identical=True, target_writes=2, save_files_deleted=[])
    except Exception as error:
        receipt.update(status="FAILED", error=str(error))
        raise
    finally:
        try:
            device.stop()
            receipt["app_left_stopped"] = not bool(device.pid())
        except Exception as error:
            receipt.update(status="FAILED", final_stop_error=str(error), app_left_stopped=False)
            durable_json(receipt_path, receipt)
            raise
        durable_json(receipt_path, receipt)
    return receipt


@contextmanager
def device_lock():
    path = ROOT / f"analysis/build/final-device-{SERIAL}.lock"
    descriptor = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
    try:
        os.write(descriptor, f"pid={os.getpid()} utc={utc()}\n".encode())
        yield
    finally:
        os.close(descriptor)
        path.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("audit", "restore"))
    parser.add_argument("--tag", required=True, help="Exact frozen candidate, e.g. p372 or p373")
    parser.add_argument("--serial", default=SERIAL, choices=(SERIAL,))
    parser.add_argument("--offline", action="store_true", help="Audit host artifacts only; never opens ADB")
    args = parser.parse_args()
    require(re.fullmatch(r"p[1-9][0-9]{2,}", args.tag), "Invalid release tag")
    require(args.command == "audit" or not args.offline, "Restore requires the exclusive device operator")
    out = local(f"analysis/build/qa{args.tag[1:]}")
    require(out.is_dir(), "Candidate QA directory is missing")
    if args.command == "audit":
        manifest, receipt = source_audit(args.tag)
        if args.offline:
            receipt["device_access"] = False
            output = out / "final_source_audit.json"
        else:
            with device_lock():
                receipt["installed"] = installed_identity(Device(args.serial), manifest)
            receipt["device_access"] = True
            output = out / "release_audit.json"
        require(not output.exists(), f"Preserve the existing receipt: {output}")
        receipt["result"] = "PASS"
        durable_json(output, receipt)
    else:
        audit_path = out / "release_audit.json"
        audit = read_json(audit_path)
        require(audit.get("result") == "PASS" and audit.get("device_access") is True
                and audit.get("candidate") == args.tag, "A successful device release audit is required")
        manifest_path = local(f"analysis/research/{args.tag}_source_build_manifest.json")
        require(file_digest(manifest_path) == audit["manifest_sha256"], "Manifest changed since audit")
        with device_lock():
            receipt = restore(Device(args.serial), args.tag, out, read_json(manifest_path))
        output = out / "save_restore_receipt.json"
    print(json.dumps(dict(receipt=str(output), result=receipt.get("status", receipt.get("result"))), indent=2))


if __name__ == "__main__":
    main()
