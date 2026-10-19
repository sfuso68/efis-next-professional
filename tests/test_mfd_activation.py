import contextlib
import hashlib
import io
import json
from pathlib import Path
import tarfile
import tempfile
import types
import unittest
from unittest.mock import Mock, patch

from tools.deployment import activate_mfd_a0be9a5 as activation


class FakeOperations:
    def __init__(self, fail=None, recover_fails=False, failure_type=RuntimeError):
        self.calls = []
        self.index = -1
        self.fail = fail
        self.recover_fails = recover_fails
        self.failure_type = failure_type

    def step(self, label):
        self.calls.append(label)
        if self.fail == label:
            raise self.failure_type(label)

    def prepare(self): self.step("prepare")
    def ready(self, target):
        self.index += 1
        self.calls.append(("target", target))
        self.step(f"ready{self.index}")
    def change(self, target): self.step(f"change{self.index}")
    def verify(self, target): self.step(f"verify{self.index}")
    def complete(self): self.step("complete")
    def recover(self):
        self.calls.append("recover")
        if self.recover_fails: raise RuntimeError("recovery failed")


class MfdActivationTests(unittest.TestCase):
    def power_record(self):
        return {"schema": "efis-next.qualified-power.v1", "node": "MFD", "boot_id": "boot",
                "chain_identified": True, "representative_load_pass": True,
                "approved_by": "operator", "evidence_sha256": "a" * 64}

    def test_power_approval_requires_current_boot_physical_evidence_and_exact_fields(self):
        activation.validate_power_approval(self.power_record(), "boot")
        for key, value in [("node", "EMS"), ("boot_id", "old"), ("chain_identified", False),
                           ("representative_load_pass", "true"), ("approved_by", ""),
                           ("evidence_sha256", "placeholder"), ("extra", True)]:
            with self.subTest(key=key):
                record = self.power_record(); record[key] = value
                with self.assertRaises(RuntimeError): activation.validate_power_approval(record, "boot")
        with self.assertRaises(RuntimeError): activation.validate_power_approval([], "boot")

    def test_power_history_failure_precedes_any_approval_read(self):
        with patch.object(activation, "run", return_value="throttled=0x50000"), \
                patch.object(activation, "POWER_APPROVAL") as approval:
            with self.assertRaisesRegex(RuntimeError, "history gate"):
                activation.power_gate()
            approval.is_file.assert_not_called()

    def test_zero_history_without_physical_approval_is_still_blocked(self):
        with patch.object(activation, "run", return_value="throttled=0x0"), \
                patch.object(activation, "POWER_APPROVAL") as approval:
            approval.is_file.return_value = False
            with self.assertRaisesRegex(RuntimeError, "approval missing"):
                activation.power_gate()

    def test_dropin_only_accepts_original_and_exact_canonical_append(self):
        raw = b"[Service]\nExecStart=canonical\n"
        with patch.object(activation, "OLD_DROPIN_SHA", hashlib.sha256(raw).hexdigest()):
            self.assertTrue(activation.approved_dropin(raw))
            self.assertTrue(activation.approved_dropin(raw + activation.DROPIN_APPEND))
            self.assertFalse(activation.approved_dropin(raw + activation.DROPIN_APPEND + b"ExecStart=other\n"))
            self.assertFalse(activation.approved_dropin(raw + activation.DROPIN_APPEND * 2))
        self.assertIn(b"\n[Unit]\nConditionPathExists=\n", activation.DROPIN_APPEND)
        self.assertNotIn(b"build-rpi5", activation.DROPIN_APPEND)

    def test_change_stops_before_dropin_write_then_points_and_starts(self):
        operations = activation.Operations(); operations.original_dropin = b"old"
        calls = []
        with patch.object(activation, "run", side_effect=lambda argv: calls.append(argv[1])), \
                patch.object(operations, "install_dropin", side_effect=lambda raw: calls.append("dropin")), \
                patch.object(operations, "point", side_effect=lambda target: calls.append("point")):
            operations.change(activation.SEALED)
        self.assertEqual(calls, ["stop", "dropin", "point", "start"])

    def test_recovery_still_attempts_previous_if_dropin_restore_fails(self):
        operations = activation.Operations()
        with patch.object(activation, "run", return_value="") as run, \
                patch.object(operations, "install_dropin", side_effect=RuntimeError("restore")), \
                patch.object(operations, "point") as point, \
                patch.object(activation.time, "sleep"), \
                patch.object(activation, "runtime_identity", return_value="123"):
            with self.assertRaisesRegex(RuntimeError, "restoration failed"):
                operations.recover()
            point.assert_called_once_with(activation.PREVIOUS)
            self.assertEqual(run.call_args.args[0][1], "start")

    def transact(self, operations):
        with contextlib.redirect_stdout(io.StringIO()):
            activation.transaction(operations)

    def test_candidate_previous_candidate_and_no_recovery_on_success(self):
        operations = FakeOperations()
        self.transact(operations)
        self.assertEqual([c[1] for c in operations.calls if isinstance(c, tuple)],
                         [activation.SEALED, activation.PREVIOUS, activation.SEALED])
        self.assertEqual(operations.calls[-1], "complete")
        self.assertNotIn("recover", operations.calls)

    def test_prepare_failure_never_restarts_previous_service(self):
        operations = FakeOperations("prepare")
        with self.assertRaises(RuntimeError): self.transact(operations)
        self.assertEqual(operations.calls, ["prepare"])

    def test_first_gate_failure_never_arms_recovery(self):
        operations = FakeOperations("ready0")
        with self.assertRaises(RuntimeError): self.transact(operations)
        self.assertNotIn("recover", operations.calls)
        self.assertNotIn("change0", operations.calls)

    def test_each_mutation_or_later_gate_failure_recovers_once(self):
        for failure in ["change0", "verify0", "ready1", "change1", "verify1", "ready2", "change2", "verify2", "complete"]:
            with self.subTest(failure=failure):
                operations = FakeOperations(failure)
                with self.assertRaises(RuntimeError): self.transact(operations)
                self.assertEqual(operations.calls.count("recover"), 1)
                self.assertEqual(operations.calls[-1], "recover")

    def test_interrupt_after_mutation_recovers(self):
        operations = FakeOperations("verify0", failure_type=KeyboardInterrupt)
        with self.assertRaises(KeyboardInterrupt): self.transact(operations)
        self.assertEqual(operations.calls[-1], "recover")

    def test_recovery_failure_is_explicit_not_reported_pass(self):
        operations = FakeOperations("change0", recover_fails=True)
        with self.assertRaisesRegex(RuntimeError, "ACTIVATION_FAILED_AND_RECOVERY_FAILED") as raised:
            self.transact(operations)
        self.assertEqual(str(raised.exception.__cause__), "change0")

    def archive(self, entries):
        result = io.BytesIO()
        with tarfile.open(fileobj=result, mode="w:gz") as archive:
            for name, kind in entries:
                info = tarfile.TarInfo(name)
                info.type = kind
                if kind == tarfile.REGTYPE:
                    info.size = 4
                    archive.addfile(info, io.BytesIO(b"test"))
                else:
                    info.linkname = "/etc/passwd"
                    archive.addfile(info)
        return result.getvalue()

    def test_ordinary_archive_is_extracted_under_prefix_only(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory)
            activation.safe_extract(self.archive([("efis-next/config/value", tarfile.REGTYPE)]), destination)
            self.assertEqual((destination / "config/value").read_bytes(), b"test")

    def test_bad_archive_is_rejected_before_any_file_is_extracted(self):
        for name, kind in [("/absolute", tarfile.REGTYPE), ("efis-next/../escape", tarfile.REGTYPE),
                           ("other/value", tarfile.REGTYPE), ("efis-next/link", tarfile.SYMTYPE),
                           ("efis-next/hard", tarfile.LNKTYPE), ("efis-next/fifo", tarfile.FIFOTYPE)]:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                destination = Path(directory)
                archive = self.archive([("efis-next/good", tarfile.REGTYPE), (name, kind)])
                with self.assertRaises(RuntimeError): activation.safe_extract(archive, destination)
                self.assertEqual(list(destination.iterdir()), [])

    def test_read_pinned_hash_mismatch_does_not_return_payload(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "input"
            path.write_bytes(b"one")
            self.assertEqual(activation.read_pinned(path, hashlib.sha256(b"one").hexdigest()), b"one")
            with self.assertRaises(RuntimeError): activation.read_pinned(path, "0" * 64)

    def test_root_helpers_must_not_be_writable_by_other_users(self):
        path = Mock()
        for uid, mode, passes in [(0, 0o100755, True), (1000, 0o100755, False), (0, 0o100775, False), (0, 0o100777, False)]:
            path.stat.return_value = types.SimpleNamespace(st_uid=uid, st_mode=mode)
            with patch.object(activation, "read_pinned", return_value=b"safe"):
                if passes: self.assertEqual(activation.trusted_file(path, "digest"), b"safe")
                else:
                    with self.assertRaises(RuntimeError): activation.trusted_file(path, "digest")

    def test_manifest_validation_executes_only_system_isolated_validator(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            (root / "bin").mkdir()
            (root / "config/active").mkdir(parents=True)
            (root / "bin/efis-pfd-qt").write_bytes(b"ELF")
            (root / "config/active/aircraft-profile.json").write_bytes(b"PROFILE")
            manifest = {"roles": ["MFD"], "platform": "RPI5_AARCH64", "source_commit": activation.SOURCE,
                        "flight_approval": False, "simulation_mode": True}
            raw = json.dumps(manifest).encode()
            (root / "release-manifest.json").write_bytes(raw)
            digest = hashlib.sha256(raw).hexdigest()
            with patch.object(activation, "MANIFEST_SHA", digest), \
                    patch.object(activation, "PROFILE_SHA", hashlib.sha256(b"PROFILE").hexdigest()), \
                    patch.object(activation, "run", return_value="PASS") as run:
                activation.validate_release(root, digest, hashlib.sha256(b"ELF").hexdigest())
                self.assertEqual(run.call_args.args[0][:3], ["/usr/bin/python3", "-I", str(activation.VALIDATOR)])
                self.assertNotEqual(str(activation.VALIDATOR.parent), str(root / "tools/deployment"))

    def test_recovery_attempts_previous_even_if_stop_returns_error(self):
        operations = activation.Operations()
        with patch.object(activation, "run", side_effect=[RuntimeError("stop"), ""]) as run, \
                patch.object(operations, "point") as point, \
                patch.object(operations, "install_dropin") as install, \
                patch.object(activation.time, "sleep"), \
                patch.object(activation, "runtime_identity", return_value="123") as verify, \
                contextlib.redirect_stdout(io.StringIO()):
            operations.recover()
            point.assert_called_once_with(activation.PREVIOUS)
            verify.assert_called_once_with(activation.PREVIOUS)
            self.assertEqual(run.call_args.args[0][1], "start")

    def test_check_only_failure_constructs_no_mutating_operations(self):
        with patch.object(activation.sys, "argv", ["activation", "--check-only"]), \
                patch.object(activation, "preflight", side_effect=RuntimeError("blocked")), \
                patch.object(activation, "Operations") as operations, \
                contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(activation.main(), 1)
            operations.assert_not_called()

    def test_check_only_pass_constructs_no_mutating_operations(self):
        with patch.object(activation.sys, "argv", ["activation", "--check-only"]), \
                patch.object(activation, "preflight", return_value={"preflight": "PASS"}), \
                patch.object(activation, "Operations") as operations, \
                contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(activation.main(), 0)
            operations.assert_not_called()

    def test_atomic_point_failure_cleans_only_its_own_temporary_link(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            previous, sealed, current = root / "previous", root / "sealed", root / "current"
            previous.mkdir()
            sealed.mkdir()
            try: current.symlink_to(previous, target_is_directory=True)
            except OSError as error: self.skipTest(f"symlinks unavailable: {error}")
            with patch.object(activation, "PREVIOUS", previous), patch.object(activation, "SEALED", sealed), \
                    patch.object(activation, "CURRENT", current), \
                    patch.object(activation.os, "replace", side_effect=OSError("rename failed")):
                with self.assertRaises(OSError): activation.Operations().point(sealed)
            self.assertEqual(current.resolve(), previous)
            self.assertEqual(list(root.glob(".mfd-activation-*")), [])

    def test_point_never_overwrites_regular_current_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            current = root / "current"
            current.write_bytes(b"user data")
            with patch.object(activation, "CURRENT", current):
                with self.assertRaises(RuntimeError): activation.Operations().point(activation.SEALED)
            self.assertEqual(current.read_bytes(), b"user data")

    def test_point_refuses_unpinned_target(self):
        with self.assertRaises(RuntimeError): activation.Operations().point(Path("/tmp/arbitrary"))


if __name__ == "__main__":
    unittest.main()

