import copy
import hashlib
import tempfile
import threading
import unittest
from pathlib import Path

from tools.simulation import session_contract as c
from tools.simulation.session_engine import SessionEngine, TruthJournal, LatestSlot, TICK_NS, MAX_LAG_NS


class Clock:
    def __init__(self): self.now = 0
    def __call__(self): return self.now
    def sleep(self, seconds): self.now += round(seconds*1e9)


class MemoryJournal:
    def __init__(self): self.records=[];self.closed=False;self.fail=False
    def append(self, record):
        if self.fail: raise OSError("injected storage error")
        self.records.append(copy.deepcopy(record))
    def close(self):self.closed=True
    def finish(self,report):
        self.close();self.result=copy.deepcopy(report)


class FixtureBackend:
    """Counter fixture, NOT flight dynamics or a calibrated sensor model."""
    def __init__(self):self.steps=0;self.bad=False
    def step(self):self.steps+=1
    def frame(self, manifest, tick, mono):
        groups={}
        for name, signals in c.ICD["signals"].items():
            groups[name]={signal:dict(value=None,raw=None,unit=unit,validity="UNAVAILABLE",
                simulated=True,source_node=65000,channel=i,sample_tick=tick,age_ms=0,
                quality=0,calibration_revision="fixture",profile_revision="fixture")
                for i,(signal,unit) in enumerate(signals.items())}
        groups["flight"]["roll_deg"].update(value=float(self.steps),raw=float(self.steps),
                                              validity="SIMULATED",quality=1)
        result=dict(schema=c.SCHEMA,kind="FRAME",session_id=manifest["session_id"],epoch=manifest["epoch"],
            simulated=True,flight_approval=False,sequence=tick//5,tick=tick,sim_time_ns=tick*TICK_NS,
            producer_mono_ns=mono,config_sha256=manifest["config_sha256"],groups=groups,
            products={kind:dict(status="UNAVAILABLE",sha256=None,observed_sim_ns=None,valid_until_sim_ns=None)
                      for kind in c.ICD["product_kinds"]})
        if self.bad:result["epoch"]+=1
        return result


class SessionEngineTests(unittest.TestCase):
    def setUp(self):
        self.manifest=c.decode((c.ROOT/"tests/data/protocols/sil-session-v1-vectors.json").read_bytes())["manifest"]
        self.clock=Clock();self.backend=FixtureBackend();self.journal=MemoryJournal()
        self.engine=SessionEngine(self.manifest,self.backend,self.journal,clock=self.clock,max_ticks=1000)

    def start(self, consumers=("PFD","MFD","EMS")):
        for name in consumers:self.engine.register(name)
        self.engine.validate();self.engine.arm_software_test();self.engine.start()

    def advance(self, ticks):
        for _ in range(ticks):self.clock.now+=TICK_NS;self.engine.pump()

    def test_lifecycle_nominal_and_terminal(self):
        self.start();self.advance(10);self.engine.stop()
        self.assertEqual(self.engine.state,"COMPLETE")
        self.assertEqual(self.backend.steps,10)
        self.assertEqual(self.engine.frames,3)
        self.assertTrue(self.journal.closed)
        with self.assertRaises(ValueError):self.engine.start()
        self.assertFalse(self.engine.pump())

    def test_wrong_transitions_no_side_effects(self):
        for method in (self.engine.start,self.engine.pause,self.engine.resume,self.engine.stop,self.engine.arm_software_test):
            with self.assertRaises(ValueError):method()
        self.assertEqual(self.engine.state,"CREATED")
        self.assertEqual(self.backend.steps,0)

    def test_consumer_absent_or_slow_does_not_stop_producer(self):
        self.start();fast=[]
        for i in range(100):
            self.advance(1)
            if i%5==4:fast.append(c.decode(self.engine.poll("PFD"))["sequence"])
        self.assertEqual(fast,list(range(1,21)))
        self.assertEqual(self.backend.steps,100)
        self.assertEqual(c.decode(self.engine.poll("EMS"))["sequence"],20)
        self.assertEqual(self.engine.slots["EMS"].overwritten,20)
        self.assertIsNone(self.engine.poll("EMS"))

    def test_no_consumers_still_runs(self):
        self.start(());self.advance(25)
        self.assertEqual(self.engine.frames,6)

    def test_busy_consumer_lock_drops_only_that_slot(self):
        self.start();slot=self.engine.slots["MFD"]
        slot.lock.acquire()
        try:self.advance(10)
        finally:slot.lock.release()
        self.assertEqual(slot.busy_drops,2)
        self.assertEqual(c.decode(self.engine.poll("PFD"))["sequence"],2)
        self.assertEqual(self.backend.steps,10)

    def test_pause_no_frames_no_steps_resume_new_epoch(self):
        self.start();self.advance(7);self.engine.pause()
        before=self.engine.frames
        self.clock.now+=10_000_000_000
        self.assertFalse(self.engine.pump());self.assertIsNone(self.engine.poll("PFD"))
        self.assertEqual(self.backend.steps,7)
        self.assertEqual(self.engine.frames,before)
        self.engine.resume()
        resumed=c.decode(self.engine.poll("PFD"))
        self.assertEqual((resumed["epoch"],resumed["tick"]),(1,0))
        self.assertEqual(resumed["groups"]["flight"]["roll_deg"]["value"],7)
        self.advance(5)
        self.assertEqual(self.backend.steps,12)
        self.assertEqual(c.decode(self.engine.poll("PFD"))["sequence"],1)

    def test_old_epoch_packet_not_returned_after_busy_resume(self):
        self.start();self.engine.pause();slot=self.engine.slots["EMS"]
        slot.lock.acquire()
        try:self.engine.resume()
        finally:slot.lock.release()
        self.assertIsNone(self.engine.poll("EMS"))

    def test_overrun_fails_before_any_step_no_catchup(self):
        self.start();self.clock.now=TICK_NS+MAX_LAG_NS+1
        with self.assertRaises(ValueError):self.engine.pump()
        self.assertEqual(self.engine.state,"FAILED")
        self.assertEqual(self.backend.steps,0)
        self.assertIsNone(self.engine.poll("PFD"))

    def test_late_tick_does_not_burst_or_skip_steps(self):
        self.start();self.clock.now=3*TICK_NS
        self.assertTrue(self.engine.pump())
        self.assertFalse(self.engine.pump())
        self.assertEqual(self.backend.steps,1)
        self.clock.now+=TICK_NS;self.engine.pump()
        self.assertEqual(self.backend.steps,2)

    def test_clock_reverse_and_wrong_frame_fail_closed(self):
        self.start();self.advance(1);self.clock.now=0
        with self.assertRaises(ValueError):self.engine.pump()
        self.assertEqual(self.engine.state,"FAILED")
        engine=SessionEngine(self.manifest,FixtureBackend(),MemoryJournal(),clock=Clock())
        engine.backend.bad=True;engine.validate();engine.arm_software_test()
        with self.assertRaises(ValueError):engine.start()
        self.assertEqual(engine.state,"FAILED")

    def test_storage_failure_stops_before_publishing(self):
        self.start();old=self.engine.frames;self.journal.fail=True
        with self.assertRaises(OSError):self.advance(5)
        self.assertEqual(self.engine.frames,old)
        self.assertEqual(self.engine.state,"FAILED")
        self.assertTrue(self.journal.closed)

    def test_backend_stall_is_failure_before_frame_delivery(self):
        self.start()
        def stall():self.clock.now+=MAX_LAG_NS+1
        self.backend.step=stall;self.clock.now=TICK_NS
        with self.assertRaises(ValueError):self.engine.pump()
        self.assertEqual(self.engine.state,"FAILED")
        self.assertEqual(self.engine.frames,1)

    def test_manifest_is_frozen_and_consumers_bounded(self):
        self.manifest["epoch"]=999
        external=self.engine.manifest;external["epoch"]=77
        self.assertEqual(self.engine.manifest["epoch"],0)
        for i in range(8):self.engine.register(f"C{i}")
        with self.assertRaises(ValueError):self.engine.register("C8")
        with self.assertRaises(ValueError):self.engine.register("C0")

    def test_owner_thread_and_immutable_packet(self):
        self.start();errors=[]
        def wrong_owner():
            try:self.engine.pause()
            except RuntimeError:errors.append(True)
        worker=threading.Thread(target=wrong_owner);worker.start();worker.join(timeout=2)
        self.assertEqual(errors,[True])
        packet=self.engine.poll("PFD");self.assertIsInstance(packet,bytes)
        value=c.decode(packet);value["epoch"]=100
        self.assertEqual(c.decode(self.engine.poll("EMS"))["epoch"],0)

    def test_bounded_owner_loop_without_display_requests(self):
        engine=SessionEngine(self.manifest,self.backend,self.journal,clock=self.clock,max_ticks=100)
        engine.validate();engine.arm_software_test();engine.start();engine.run(sleep=self.clock.sleep)
        self.assertEqual(engine.state,"COMPLETE")
        self.assertEqual(self.backend.steps,100)
        self.assertEqual(engine.frames,21)
        self.assertEqual(self.clock.now,1_000_000_000)

    def test_journal_exclusive_durable_and_hash(self):
        with tempfile.TemporaryDirectory() as temp:
            directory=Path(temp)/"new-run"
            journal=TruthJournal(directory);journal.append({"test":"SIM"});journal.close()
            raw=(directory/"truth.jsonl").read_bytes()
            self.assertEqual(journal.digest.hexdigest(),hashlib.sha256(raw).hexdigest())
            self.assertEqual(journal.records,1)
            with self.assertRaises(FileExistsError):TruthJournal(directory)

    def test_result_only_after_successful_finalization(self):
        self.start()
        def broken(report):raise OSError("injected finalization failure")
        self.journal.finish=broken
        with self.assertRaises(OSError):self.engine.stop()
        self.assertEqual(self.engine.state,"FAILED")
        self.assertFalse(any(r.get("event")=="COMPLETE" for r in self.journal.records))

    def test_disk_run_final_result_hash_matches_truth(self):
        with tempfile.TemporaryDirectory() as temp:
            directory=Path(temp)/"run"
            journal=TruthJournal(directory)
            engine=SessionEngine(self.manifest,self.backend,journal,clock=self.clock,max_ticks=10)
            engine.validate();engine.arm_software_test();engine.start();engine.run(sleep=self.clock.sleep)
            result=c.decode((directory/"result.json").read_bytes())
            self.assertEqual(result["status"],"COMPLETE_SOFTWARE_ONLY")
            self.assertEqual(result["truth_sha256"],hashlib.sha256((directory/"truth.jsonl").read_bytes()).hexdigest())
            self.assertEqual(result["frames"],3)

    def test_existing_result_never_overwritten(self):
        with tempfile.TemporaryDirectory() as temp:
            directory=Path(temp)/"run";journal=TruthJournal(directory)
            path=directory/"result.json";path.write_bytes(b"existing-test-evidence")
            with self.assertRaises(OSError):journal.finish({"status":"CANDIDATE"})
            self.assertEqual(path.read_bytes(),b"existing-test-evidence")


if __name__=="__main__":unittest.main()
