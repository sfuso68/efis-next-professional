"""Failure injection with input-property fixture, NOT native aerodynamics."""
import tempfile
from pathlib import Path
import unittest
from unittest.mock import patch

from tests.live_control_fixture import FixtureFdm
from tools.simulation import session_contract as c
from tools.simulation.owner_console import run
from tools.simulation.session_engine import SessionEngine
from tests.test_sil_session_engine import Clock


class ConsoleRunnerTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)/"run"
        self.world=c.decode((c.ROOT/"config/simulation/treviso-belluno-world-reference.json").read_bytes())
        self.backend=FixtureFdm();self.backend.model_digest=dict(sha256="0"*64)

    def attempt(self, factory):
        # Failure-path unit tests must reach the injected boundary independent
        # of host fsync jitter. Native server tests retain the real clock and
        # unchanged deadlines; this fixture is explicitly NOT timing evidence.
        def engine(*args, **kwargs):return SessionEngine(*args,clock=Clock(),**kwargs)
        with patch("tools.simulation.owner_console.Backend", return_value=self.backend), \
             patch("tools.simulation.owner_console.SessionEngine",side_effect=engine):
            return run(self.root,self.world,session_id="runner-test",source_commit="0"*40,
                       channel_factory=factory,journal_mode="sync",seconds=1)

    def assert_failed(self):
        self.assertFalse((self.root/"result.json").exists())
        self.assertFalse((self.root/"probe-report.json").exists())
        rows=[c.decode(line) for line in (self.root/"truth.jsonl").read_bytes().splitlines()]
        self.assertTrue(any(r.get("event")=="FAILED" for r in rows))
        self.assertEqual(self.backend.test_controls,{})

    def test_channel_factory_error_cannot_finalize_success(self):
        def factory(*_):raise OSError("factory failure")
        with self.assertRaisesRegex(OSError,"factory failure"):self.attempt(factory)
        self.assert_failed()

    def test_channel_start_publish_and_keyboard_failure_leave_incomplete_evidence(self):
        for boundary in ("start", "publish", "interrupt", "close"):
            with self.subTest(boundary=boundary):
                if self.root.exists():self.setUp()
                class Channel:
                    overwritten=0
                    ingress=(0,"NONE")
                    def __init__(self,console,*_):self.console=console
                    def publish(self,_):
                        if boundary=="publish":raise OSError("publish failure")
                    def start(self):
                        if boundary=="interrupt":raise KeyboardInterrupt()
                        raise OSError("start failure")
                    def close(self):
                        if boundary=="close":raise OSError("close failure")
                with self.assertRaises((OSError,KeyboardInterrupt)):self.attempt(Channel)
                self.assert_failed()

    def test_explicit_stop_can_complete_zero_ticks_and_closed_audit(self):
        class Channel:
            overwritten=0
            ingress=(1,"STOP_REQUESTED")
            def __init__(self,console,*_):self.console=console
            def publish(self,_):pass
            def start(self):self.console.request_stop()
            def close(self):pass
        report=self.attempt(Channel)
        self.assertEqual(report["status"],"COMPLETE")
        self.assertEqual(report["total_ticks"],0)
        self.assertTrue((self.root/"result.json").exists())
        from tools.simulation.session_trace import verify
        self.assertEqual(verify(self.root,allow_control_audit=True)["control_execution"]["applied"],0)


if __name__=="__main__":unittest.main()
