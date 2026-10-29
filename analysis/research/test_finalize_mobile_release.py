"""Execute the actual restore function against a recording fake device, never ADB."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location("release", Path(__file__).with_name("finalize_mobile_release.py"))
release = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(release)


class FakeDevice:
    def __init__(self, out, manifest, failure=None, extra=False):
        self.out, self.manifest, self.failure = out, manifest, failure
        self.files = {row[0]: b"current QA data " + name.encode() for name, row in release.TARGETS.items()}
        if extra:
            self.files[release.FILES + "/savegame01.sav"] = b"extra QA slot"
        self.events, self.running = [], True
        self.preferences = b"sens=117\ngyro=0\nsize=96\n"
        self.writes, self.removed = [], []

    def call(self, *args):
        if args[:3] == ("shell", "pm", "path"):
            return "package:/data/app/known/app/base.apk\n"
        raise AssertionError(args)

    def remote_digest(self, path):
        return self.manifest["apk" if path.endswith(".apk") else "library"]["sha256"]

    def pid(self):
        return "17090" if self.running else ""

    def stop(self):
        self.events.append("stop")
        self.running = False

    def prefs(self):
        return self.preferences

    def save_paths(self):
        return sorted(self.files)

    def metadata(self, path):
        return "660:1023:1023" if self.failure != "metadata" or not self.writes else "644:1023:1023"

    def read(self, path):
        self.events.append(("read", path))
        if self.failure == "archive" and path.endswith("savegame00.sav"):
            raise RuntimeError("Injected archive read failure")
        if self.failure == "reread" and self.writes:
            return b"corrupt"
        return self.files[path]

    def push(self, source, target):
        self.events.append(("push", target))
        if self.failure == "push":
            raise RuntimeError("Injected push failure")
        archive = self.out / "saves_before_final_restore"
        # Both originals and a durable ARCHIVED checkpoint must exist before either write.
        for name in release.TARGETS:
            assert (archive / name).read_bytes().startswith(b"current QA data ")
        receipt = json.loads((self.out / "save_restore_receipt.json").read_text())
        assert receipt["status"] == "ARCHIVED_BEFORE_ANY_DEVICE_WRITE"
        assert not self.running
        self.files[target] = source.read_bytes()
        self.writes.append(target)

    def cleanup(self):
        assert len(self.writes) == 2
        assert not self.running
        for name, (target, size, expected) in release.TARGETS.items():
            assert len(self.files[target]) == size
            assert hashlib.sha256(self.files[target]).hexdigest() == expected
        if self.failure == "cleanup":
            raise RuntimeError("Injected cleanup failure")
        self.removed = list(release.TEMP_FILES)


class RestoreTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.out = Path(self.temporary.name)
        self.manifest = {"apk": {"sha256": "a" * 64}, "library": {"sha256": "b" * 64}}

    def test_exact_two_targets_original_baseline_preserved_preferences_and_extra_slot(self):
        device = FakeDevice(self.out, self.manifest, extra=True)
        receipt = release.restore(device, "p373", self.out, self.manifest)
        self.assertEqual(receipt["status"], "PASS")
        self.assertEqual(device.writes, [row[0] for row in release.TARGETS.values()])
        self.assertEqual(device.removed, list(release.TEMP_FILES))
        self.assertEqual(device.files[release.FILES + "/savegame01.sav"], b"extra QA slot")
        self.assertEqual(device.preferences, b"sens=117\ngyro=0\nsize=96\n")
        self.assertFalse(device.running)
        self.assertEqual(receipt["files"][0]["mode_uid_gid"], "660:1023:1023")
        self.assertEqual(receipt["save_files_deleted"], [])
        self.assertEqual(receipt["target_writes"], 2)

    def test_maintenance_default_uses_exact_qa374_baseline(self):
        device = FakeDevice(self.out, self.manifest, extra=True)
        receipt = release.restore(device, "p379", self.out, self.manifest)
        self.assertEqual(receipt["status"], "PASS")
        self.assertEqual(receipt["baseline"], "qa374/saves_before")
        self.assertEqual(receipt["target_writes"], 2)
        self.assertEqual(device.files[release.FILES + "/savegame01.sav"], b"extra QA slot")
        self.assertEqual(device.preferences, b"sens=117\ngyro=0\nsize=96\n")
        self.assertFalse(device.running)

    def test_wrong_maintenance_baseline_refused_before_device_access(self):
        device = FakeDevice(self.out, self.manifest)
        with mock.patch.object(release, "MAINTENANCE_BASELINE", self.out):
            for name in release.TARGETS:
                (self.out / name).write_bytes(b"wrong maintenance baseline")
            with self.assertRaisesRegex(RuntimeError, "Wrong qa374 baseline"):
                release.restore(device, "p379", self.out, self.manifest)
        self.assertEqual(device.events, [])
        self.assertEqual(device.writes, [])

    def test_maintenance_preserves_requested_enabled_gyro(self):
        device = FakeDevice(self.out, self.manifest)
        device.preferences = b"sens=117\ngyro=1\naggression=0\n"
        receipt = release.restore(device, "p381", self.out, self.manifest)
        self.assertEqual(receipt["status"], "PASS")
        self.assertEqual(device.preferences, b"sens=117\ngyro=1\naggression=0\n")
        self.assertEqual(receipt["target_writes"], 2)
        self.assertFalse(device.running)

    def test_second_archive_failure_prevents_all_writes_and_cleanup(self):
        device = FakeDevice(self.out, self.manifest, failure="archive")
        with self.assertRaisesRegex(RuntimeError, "archive read failure"):
            release.restore(device, "p372", self.out, self.manifest)
        self.assertEqual(device.writes, [])
        self.assertEqual(device.removed, [])
        self.assertFalse(device.running)
        self.assertEqual(json.loads((self.out / "save_restore_receipt.json").read_text())["status"], "FAILED")

    def test_write_and_reread_and_metadata_failure_leave_archive_no_cleanup_and_app_stopped(self):
        for failure in ("push", "reread", "metadata", "cleanup"):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as temporary:
                out = Path(temporary)
                device = FakeDevice(out, self.manifest, failure=failure)
                with self.assertRaises(RuntimeError):
                    release.restore(device, "p372", out, self.manifest)
                self.assertFalse(device.running)
                self.assertEqual(device.removed, [])
                self.assertTrue((out / "saves_before_final_restore/savegame00.sav").is_file())
                self.assertTrue((out / "saves_before_final_restore/System.sav").is_file())

    def test_wrong_baseline_refused_before_any_device_access(self):
        device = FakeDevice(self.out, self.manifest)
        with tempfile.TemporaryDirectory() as temporary:
            bad = Path(temporary)
            for name in release.TARGETS:
                (bad / name).write_bytes(b"not qa365 baseline")
            with self.assertRaisesRegex(RuntimeError, "Wrong qa365 baseline"):
                release.restore(device, "p372", self.out, self.manifest, baseline=bad)
        self.assertEqual(device.events, [])
        self.assertEqual(device.writes, [])

    def test_existing_archive_and_wrong_installed_apk_refused_without_write(self):
        device = FakeDevice(self.out, self.manifest)
        (self.out / "saves_before_final_restore").mkdir()
        with self.assertRaisesRegex(RuntimeError, "never be overwritten"):
            release.restore(device, "p372", self.out, self.manifest)
        self.assertEqual(device.writes, [])
        device.remote_digest = lambda path: "c" * 64
        with self.assertRaisesRegex(RuntimeError, "Installed APK differs"):
            release.restore(device, "p372", self.out, self.manifest)
        self.assertEqual(device.writes, [])

    def test_qa_gyro_must_be_restored_via_ui_no_preference_write(self):
        device = FakeDevice(self.out, self.manifest)
        device.preferences = b"sens=117\ngyro=1\n"
        with self.assertRaisesRegex(RuntimeError, "game UI"):
            release.restore(device, "p372", self.out, self.manifest)
        self.assertEqual(device.writes, [])
        self.assertFalse(device.running)
        self.assertEqual(device.preferences, b"sens=117\ngyro=1\n")

    def test_missing_target_prevents_all_writes_and_keeps_app_stopped(self):
        device = FakeDevice(self.out, self.manifest)
        del device.files[release.TARGETS["System.sav"][0]]
        with self.assertRaisesRegex(RuntimeError, "Missing an existing baseline target"):
            release.restore(device, "p373", self.out, self.manifest)
        self.assertEqual(device.writes, [])
        self.assertEqual(device.removed, [])
        self.assertFalse(device.running)

    def test_prefs_change_is_reported_and_prevents_cleanup(self):
        device = FakeDevice(self.out, self.manifest)
        original_push = device.push
        def change_after_push(source, target):
            original_push(source, target)
            device.preferences = b"sens=118\ngyro=0\nsize=96\n"
        device.push = change_after_push
        with self.assertRaisesRegex(RuntimeError, "Preferences changed"):
            release.restore(device, "p373", self.out, self.manifest)
        self.assertEqual(device.removed, [])
        self.assertFalse(device.running)


class DeviceSelectionTests(unittest.TestCase):
    def test_missing_or_mismatched_selection_never_contacts_adb(self):
        with mock.patch.object(release.Device, "call") as call:
            for configured, requested in (("", ""), ("", "test-device"), ("test-device", "other-device")):
                with self.subTest(configured=configured, requested=requested):
                    with mock.patch.object(release, "SERIAL", configured):
                        with self.assertRaises(RuntimeError):
                            release.Device(requested)
            call.assert_not_called()

    def test_explicit_selection_is_forwarded_to_adb(self):
        with mock.patch.object(release, "SERIAL", "test-device"):
            with mock.patch.object(release.Device, "call", return_value="device\n") as call:
                device = release.Device("test-device")
        self.assertEqual(device.prefix, [str(release.ADB), "-s", "test-device"])
        call.assert_called_once_with("get-state")


class SourceAuditTests(unittest.TestCase):
    def test_maintenance_boundary_and_tag_validation(self):
        self.assertFalse(release.maintenance_release("p373"))
        self.assertTrue(release.maintenance_release("p374"))
        self.assertTrue(release.maintenance_release("p379"))
        with self.assertRaisesRegex(RuntimeError, "Invalid release tag"):
            release.maintenance_release("p379/../../other")

    def test_exact_check_count_profiles_preserve_historical_releases(self):
        for tag, expected in (("p373", (78, 3302, 50)), ("p374", (80, 3303, 56)),
                              ("p380", (80, 3303, 56)), ("p381", (80, 3303, 57)),
                              ("p500", (80, 3303, 57))):
            self.assertEqual(release.release_counts(tag), expected)

    def test_production_audit_rejects_qa_macro_with_valid_native_hashes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            analysis = root / "analysis"
            frozen = analysis / "build/p379_source"
            frozen.mkdir(parents=True)
            (analysis / "research").mkdir()
            files = {"re4fix.c": '#define RE4_QA_GRENADE_FIXTURE 1\n', "re4fix_overlay.c": ""}
            files.update({f"header_{index}.h": "" for index in range(78)})
            entries = []
            for name, body in files.items():
                data = body.encode()
                (analysis / name).write_bytes(data)
                (frozen / name).write_bytes(data)
                entries.append(dict(path="analysis/" + name, bytes=len(data),
                                    sha256=hashlib.sha256(data).hexdigest()))
            (analysis / "research/p379_source_build_manifest.json").write_text(json.dumps(
                dict(candidate="p379", inputs=entries)))
            with mock.patch.object(release, "ROOT", root):
                with self.assertRaisesRegex(RuntimeError, "Production source contains the QA grenade fixture"):
                    release.source_audit("p379")


if __name__ == "__main__":
    unittest.main(verbosity=2)
