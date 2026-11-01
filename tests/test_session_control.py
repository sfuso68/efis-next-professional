import copy
import threading
import unittest

from tools.simulation import session_contract as c
from tools.simulation.control_inbox import ControlInbox
from tools.simulation.session_control import SessionAdmission
from tools.simulation.session_engine import SessionEngine, TICK_NS, MAX_LAG_NS, encode
from tests.test_sil_session_engine import Clock, FixtureBackend, MemoryJournal


class CheckedJournal(MemoryJournal):
    def __init__(self):
        super().__init__()
        self.checks = 0
        self.check_failure = False

    def check(self):
        self.checks += 1
        if self.closed or self.check_failure:
            raise OSError("injected writer watchdog")


class SessionControlTests(unittest.TestCase):
    def setUp(self):
        vectors = c.decode((c.ROOT/"tests/data/protocols/sil-session-v1-vectors.json").read_bytes())
        self.manifest, self.request = vectors["manifest"], vectors["command"]
        self.clock, self.backend, self.journal = Clock(), FixtureBackend(), CheckedJournal()
        self.engine = SessionEngine(self.manifest, self.backend, self.journal,
                                    clock=self.clock, max_ticks=1000)
        self.control = self.new_control()
        self.engine.attach_admission(self.control)

    def new_control(self, manifest=None, duration=1_000_000_000):
        return SessionAdmission(manifest or self.manifest, principal="instructor",
                                lease_id="lease-001", lease_until_tick=100,
                                lease_duration_ns=duration, clock=self.clock)

    def raw(self, number=1):
        request = copy.deepcopy(self.request)
        request.update(command_id=f"cmd-{number}", sequence=number)
        return encode(request)

    def start(self):
        self.engine.validate()
        self.engine.arm_software_test()
        self.engine.start()

    def offer(self, number=1):
        return self.control.offer(self.raw(number), principal="instructor")

    def test_owner_pump_admits_before_due_tick_without_executing(self):
        self.start()
        thread = threading.Thread(target=self.offer)
        thread.start();thread.join(timeout=1)
        self.assertFalse(thread.is_alive())
        self.assertEqual(self.control.commands.counts["pending"], 0)
        self.assertFalse(self.engine.pump())
        self.assertEqual(self.control.commands.counts["pending"], 1)
        self.assertEqual(c.decode(self.control.latest_replies())[0]["ack"]["status"], "QUEUED")
        self.assertEqual(self.backend.steps, 0)

    def test_service_budget_and_immutable_replies(self):
        self.start()
        for number in range(1, 7):self.offer(number)
        self.engine.pump()
        replies = self.control.latest_replies()
        self.assertIsInstance(replies, bytes)
        self.assertEqual(len(c.decode(replies)), 4)
        self.assertLessEqual(len(replies), 4*ControlInbox.MAX_MESSAGE_BYTES)
        self.engine.pump()
        self.assertEqual(len(c.decode(self.control.latest_replies())), 2)
        self.assertEqual(len(c.decode(replies)), 4)

    def test_wall_lease_expires_without_due_physics_tick(self):
        engine = SessionEngine(self.manifest, FixtureBackend(), MemoryJournal(), clock=self.clock)
        control = self.new_control(duration=100)
        engine.attach_admission(control)
        engine.validate();engine.arm_software_test();engine.start()
        self.clock.now = 100
        self.assertFalse(engine.pump())
        self.assertTrue(control.commands.closed)
        self.assertEqual(engine.total_ticks, 0)

    def test_pause_revokes_pending_before_journal_write(self):
        self.start();self.offer();self.engine.pump()
        original = self.journal.append
        def observe(record):
            if record.get("event") == "PAUSED":
                self.assertTrue(self.control.commands.closed)
                self.assertEqual(self.control.commands.counts["pending"], 0)
            original(record)
        self.journal.append = observe
        self.engine.pause()
        self.clock.now += 10_000_000_000
        frames = self.engine.frames
        self.assertFalse(self.engine.pump())
        self.assertEqual(self.engine.frames, frames)
        self.assertEqual(self.backend.steps, 0)
        self.assertEqual(self.control.lookup_outcome(self.raw(), principal="instructor")["reason"], "NO_LEASE")
        self.assertGreaterEqual(self.journal.checks, 4)

    def test_resume_never_rearms_old_epoch_or_reverses_command_tick(self):
        self.start();self.offer();self.engine.pump()
        self.clock.now = TICK_NS
        self.engine.pump()
        self.engine.pause();self.engine.resume()
        self.assertEqual((self.engine.manifest["epoch"], self.engine.relative_tick), (1, 0))
        self.engine.pump()
        self.assertTrue(self.control.commands.closed)
        self.assertEqual(self.offer(2), "REVOKED")
        self.assertEqual(self.control.lookup_outcome(self.raw(), principal="instructor")["reason"], "NO_LEASE")
        with self.assertRaises(ValueError):self.engine.attach_admission(self.new_control(self.engine.manifest))

    def test_attachment_exact_manifest_single_owner_and_created_only(self):
        with self.assertRaises(ValueError):self.engine.attach_admission(self.new_control())
        other = SessionEngine(self.manifest, FixtureBackend(), MemoryJournal(), clock=self.clock)
        wrong = copy.deepcopy(self.manifest);wrong["epoch"] += 1
        with self.assertRaises(ValueError):other.attach_admission(self.new_control(wrong))
        self.assertIsNone(other._control)
        other.attach_admission(self.new_control())
        with self.assertRaises(ValueError):
            SessionEngine(self.manifest, FixtureBackend(), MemoryJournal(), clock=self.clock).attach_admission(other._control)
        late = SessionEngine(self.manifest, FixtureBackend(), MemoryJournal(), clock=self.clock)
        late.validate()
        with self.assertRaises(ValueError):late.attach_admission(self.new_control())

    def test_cross_thread_owner_operations_rejected(self):
        errors = []
        def wrong_owner():
            for action in (self.engine.pump, self.control.latest_replies,
                           lambda:self.control.service(current_tick=0, epoch=0), self.control.revoke):
                try:action()
                except RuntimeError:errors.append(True)
        thread = threading.Thread(target=wrong_owner);thread.start();thread.join(timeout=1)
        self.assertEqual(len(errors), 4)

    def test_signal_revoke_is_serviced_before_next_step(self):
        self.start();self.offer()
        thread = threading.Thread(target=self.control.signal_revoke)
        thread.start();thread.join(timeout=1)
        self.clock.now = TICK_NS
        self.engine.pump()
        self.assertTrue(self.control.commands.closed)
        self.assertEqual(self.control.commands.counts["history"], 0)
        self.assertEqual(self.backend.steps, 1)  # Physics continues, no command executor.

    def test_admission_latency_counts_against_deadline_before_step(self):
        self.start()
        original = self.control.service
        def slow(**kwargs):
            original(**kwargs)
            self.clock.now += TICK_NS+MAX_LAG_NS+1
        self.control.service = slow
        with self.assertRaises(ValueError):self.engine.pump()
        self.assertEqual(self.backend.steps, 0)
        self.assertEqual(self.engine.state, "FAILED")
        self.assertTrue(self.control.commands.closed)

    def test_post_service_watchdog_checked_before_step_and_while_paused(self):
        for paused in (False, True):
            with self.subTest(paused=paused):
                self.setUp();self.start()
                if paused:self.engine.pause()
                original = self.control.service
                def poison(**kwargs):
                    original(**kwargs)
                    self.journal.check_failure = True
                self.control.service = poison
                self.clock.now += TICK_NS
                with self.assertRaises(OSError):self.engine.pump()
                self.assertEqual(self.backend.steps, 0)
                self.assertEqual(self.engine.state, "FAILED")
                self.assertTrue(self.control.commands.closed)

    def test_lifecycle_journal_failure_leaves_no_lease(self):
        for transition in ("validate", "pause", "resume", "stop"):
            with self.subTest(transition=transition):
                self.setUp()
                if transition != "validate":self.start()
                if transition == "resume":self.engine.pause()
                self.journal.fail = True
                with self.assertRaises(OSError):getattr(self.engine, transition)()
                self.assertTrue(self.control.commands.closed)
                self.assertEqual(self.engine.state, "FAILED")

    def test_stop_and_automatic_completion_revoke_and_terminal_pump_is_noop(self):
        for automatic in (False, True):
            with self.subTest(automatic=automatic):
                self.setUp();self.start();self.offer();self.engine.pump()
                if automatic:
                    self.engine.max_ticks = 1
                    self.clock.now = TICK_NS
                    self.engine.pump()
                else:self.engine.stop()
                self.assertEqual(self.engine.state, "COMPLETE")
                self.assertTrue(self.control.commands.closed)
                checks = self.journal.checks
                self.assertFalse(self.engine.pump())
                self.assertEqual(self.journal.checks, checks)

    def test_failed_pump_does_not_check_closed_writer_again(self):
        self.start();self.journal.check_failure = True
        with self.assertRaises(OSError):self.engine.pump()
        checks = self.journal.checks
        self.assertFalse(self.engine.pump())
        self.assertEqual(self.journal.checks, checks)

    def test_foreign_owner_attachment_and_mutated_manifest_rejected(self):
        controls = []
        thread = threading.Thread(target=lambda:controls.append(self.new_control()))
        thread.start();thread.join(timeout=1)
        other = SessionEngine(self.manifest, FixtureBackend(), MemoryJournal(), clock=self.clock)
        with self.assertRaises(RuntimeError):other.attach_admission(controls[0])
        original = copy.deepcopy(self.manifest)
        control = self.new_control(original)
        original["epoch"] += 1
        with self.assertRaises(ValueError):control.bind(original)
        other.attach_admission(control)  # Original exact snapshot is still valid.

    def test_arm_start_frame_and_finalization_failure_revoke(self):
        for phase in ("arm", "start", "frame", "finalization", "backend"):
            with self.subTest(phase=phase):
                self.setUp()
                if phase in ("arm", "start"):self.engine.validate()
                if phase == "start":self.engine.arm_software_test()
                if phase in ("frame", "finalization", "backend"):self.start()
                if phase in ("arm", "start"):
                    self.journal.fail = True
                    action = self.engine.arm_software_test if phase == "arm" else self.engine.start
                elif phase == "frame":
                    self.backend.bad = True
                    self.clock.now = TICK_NS
                    for _ in range(4):self.engine.pump();self.clock.now += TICK_NS
                    action = self.engine.pump
                elif phase == "backend":
                    def broken_step():raise OSError("injected FDM failure")
                    self.backend.step = broken_step
                    self.clock.now = TICK_NS
                    action = self.engine.pump
                else:
                    def broken_finish(_):raise OSError("injected finalization failure")
                    self.journal.finish = broken_finish
                    action = self.engine.stop
                with self.assertRaises((ValueError, OSError)):action()
                self.assertTrue(self.control.commands.closed)
                self.assertEqual(self.engine.state, "FAILED")

    def test_admitted_commands_are_not_dispatched_by_engine(self):
        self.start();self.offer();self.engine.pump()
        ticks = self.request["expires_tick"]+2
        for _ in range(ticks):
            self.clock.now += TICK_NS
            self.engine.pump()
        self.assertEqual(self.control.commands.counts["dispatched"], 0)
        self.assertEqual(self.control.commands.counts["pending"], 0)
        self.assertEqual(self.control.lookup_outcome(self.raw(), principal="instructor")["reason"], "EXPIRED")
        self.assertEqual(self.backend.steps, ticks)

if __name__ == "__main__":unittest.main()
