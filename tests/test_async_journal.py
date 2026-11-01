import hashlib
import os
from pathlib import Path
import tempfile
import time
import unittest
from tools.simulation.async_journal import AsyncTruthJournal, MAX_RECORDS
from tools.simulation.session_contract import decode


def hanging_writer(directory,inbox,acks):
    acks.put(("READY",None));time.sleep(10)


def crashing_writer(directory,inbox,acks):
    acks.put(("READY",None));inbox.get();os._exit(7)


def error_writer(directory,inbox,acks):
    acks.put(("READY",None));inbox.get();acks.put(("ERROR","INJECTED_DISK_FULL"));time.sleep(10)


class Clock:
    now=0
    def __call__(self):return self.now


class AsyncJournalTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)/"run"

    def make(self,**kwargs):
        j=AsyncTruthJournal(self.root,**kwargs);self.addCleanup(j.close);return j

    def wait(self,journal,predicate):
        deadline=time.monotonic()+2
        while not predicate():
            journal.check()
            if time.monotonic()>deadline:self.fail("worker did not respond")
            time.sleep(.001)

    def test_real_writer_hash_and_publication(self):
        j=self.make()
        j.append(dict(event="SIM_ONE"));j.append(dict(event="SIM_TWO"))
        self.assertFalse((self.root/"result.json").exists())
        self.wait(j,lambda:j.durable==2)
        j.finish(dict(status="COMPLETE_SOFTWARE_ONLY",flight_approval=False))
        result=decode((self.root/"result.json").read_bytes())
        self.assertEqual(result["truth_sha256"],hashlib.sha256((self.root/"truth.jsonl").read_bytes()).hexdigest())
        self.assertEqual(result["recording_policy"]["acceptance"],"QUEUED_NOT_DURABLE")
        self.assertEqual(result["truth_records"],2)
        self.assertFalse(j.process.is_alive())

    def test_hang_is_not_a_blocking_append_and_times_out(self):
        clock=Clock();j=self.make(_target=hanging_writer,_clock=clock)
        start=time.monotonic();j.append(dict(test="QUEUED"))
        self.assertLess(time.monotonic()-start,.1)
        self.assertEqual(j.durable,0)
        clock.now=.100001
        with self.assertRaises(TimeoutError):j.check()
        j.close();self.assertFalse(j.process.is_alive())
        self.assertFalse((self.root/"result.json").exists())

    def test_record_capacity_is_bounded(self):
        j=self.make(_target=hanging_writer,_clock=Clock())
        for i in range(MAX_RECORDS):j.append(dict(index=i))
        with self.assertRaises(BufferError):j.append(dict(index=MAX_RECORDS))
        self.assertEqual(len(j.pending),16)

    def test_byte_capacity_and_oversize_record(self):
        j=self.make(_target=hanging_writer,_clock=Clock())
        with self.assertRaises(ValueError):j.append(dict(payload="x"*65536))
        for i in range(8):j.append(dict(payload="x"*65000,index=i))
        with self.assertRaises(BufferError):j.append(dict(payload="x"*65000))

    def test_writer_crash_is_reported_without_success(self):
        j=self.make(_target=crashing_writer)
        j.append(dict(test="CRASH"))
        with self.assertRaises((OSError,TimeoutError)):
            self.wait(j,lambda:False)
        self.assertFalse((self.root/"result.json").exists())

    def test_writer_io_error_is_not_acknowledged_durable(self):
        j=self.make(_target=error_writer);j.append(dict(test="DISK_FULL"))
        with self.assertRaises((OSError,TimeoutError)):self.wait(j,lambda:False)
        self.assertEqual(j.durable,0)

    def test_existing_run_and_result_not_overwritten(self):
        j=self.make();j.append(dict(test="ONE"))
        path=self.root/"result.json";path.write_bytes(b"existing")
        with self.assertRaises(OSError):j.finish(dict(status="CANDIDATE"))
        self.assertEqual(path.read_bytes(),b"existing")
        with self.assertRaises(FileExistsError):AsyncTruthJournal(self.root)

    def test_closed_journal_cannot_accept_more(self):
        j=self.make();j.close()
        with self.assertRaises(ValueError):j.append(dict(test="AFTER_CLOSE"))

    def test_engine_checks_watchdog_between_output_frames(self):
        from tests.test_sil_session_engine import FixtureBackend, Clock as TickClock
        from tools.simulation.session_engine import SessionEngine
        from tools.simulation.session_contract import ROOT
        clock=Clock();j=self.make(_target=hanging_writer,_clock=clock)
        manifest=decode((ROOT/"tests/data/protocols/sil-session-v1-vectors.json").read_bytes())["manifest"]
        backend=FixtureBackend();engine=SessionEngine(manifest,backend,j,clock=TickClock())
        engine.validate();engine.arm_software_test();engine.start()
        clock.now=.100001
        with self.assertRaises(TimeoutError):engine.pump()
        self.assertEqual(engine.state,"FAILED")
        self.assertEqual(backend.steps,0)
        self.assertFalse(j.process.is_alive())
        self.assertFalse((self.root/"result.json").exists())


if __name__=="__main__":unittest.main()
