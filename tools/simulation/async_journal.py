"""Opt-in SIL recorder process: QUEUED is not DURABLE. No flight recorder API."""
from collections import deque
import hashlib
import multiprocessing
import os
from pathlib import Path
import queue
import time
from tools.simulation.session_engine import TruthJournal, encode

MAX_RECORDS, MAX_BYTES, ACK_TIMEOUT_S = 16, 512*1024, .1


def writer(directory, inbox, acks):
    journal = None
    try:
        journal = TruthJournal(directory)
        acks.put(("READY", None))
        expected, deferred = 1, None
        while True:
            item = deferred if deferred is not None else inbox.get()
            deferred = None
            if item[0] == "FINISH":
                journal.finish(item[1]); acks.put(("FINISHED", expected-1)); return
            batch = [item]
            for _ in range(7):
                try: following = inbox.get_nowait()
                except queue.Empty: break
                if following[0] != "APPEND":
                    deferred = following; break
                batch.append(following)
            for kind, sequence, data in batch:
                if kind != "APPEND" or sequence != expected or not isinstance(data, bytes) or len(data)>65536:
                    raise ValueError("writer order or size")
                if journal.stream.write(data) != len(data): raise OSError("short writer write")
                expected += 1
            os.fsync(journal.stream.fileno())
            for _, _, data in batch:
                journal.digest.update(data); journal.records += 1
            # Cumulative acknowledgement is sent only AFTER fsync.
            acks.put(("DURABLE", expected-1))
    except Exception as exc:
        try: acks.put_nowait(("ERROR", type(exc).__name__))
        except queue.Full: pass
    finally:
        if journal is not None: journal.close()


class AsyncTruthJournal:
    policy = dict(mode="ASYNC_TEST_V1", acceptance="QUEUED_NOT_DURABLE",
        pending_records=MAX_RECORDS,pending_bytes=MAX_BYTES,durability_ack_ms=100,
        startup_timeout_s=5,finish_timeout_s=3)

    def __init__(self, directory, *, _target=writer, _clock=time.monotonic):
        self.root, self.clock = Path(directory), _clock
        self.root.mkdir(parents=False,exist_ok=False)
        self.closed = self.finished = False
        self.pending = deque()
        self.pending_bytes = self.sequence = self.durable = 0
        self.maximum_pending = self.maximum_ack_age_ms = 0
        ctx = multiprocessing.get_context("spawn")
        self.inbox, self.acks = ctx.Queue(maxsize=MAX_RECORDS),ctx.Queue(maxsize=MAX_RECORDS)
        self.process = ctx.Process(target=_target,args=(str(self.root/"writer-private"),self.inbox,self.acks),daemon=True)
        self.process.start()
        try:
            if self.acks.get(timeout=5) != ("READY",None): raise ValueError("writer startup failed")
        except Exception:
            self.close(); raise

    def _drain(self):
        while True:
            try: kind,value = self.acks.get_nowait()
            except queue.Empty: break
            if kind == "ERROR": raise OSError("writer error: "+str(value))
            if kind == "DURABLE":
                if type(value) is not int or not self.durable < value <= self.sequence:
                    raise ValueError("invalid durability acknowledgement")
                while self.pending and self.pending[0][0] <= value:
                    _,enqueued,length = self.pending.popleft()
                    age = self.clock()-enqueued
                    self.maximum_ack_age_ms = max(self.maximum_ack_age_ms,age*1000)
                    if age > ACK_TIMEOUT_S: raise TimeoutError("late durability acknowledgement")
                    self.pending_bytes -= length
                self.durable = value
            elif kind == "FINISHED":
                if value != self.sequence or self.pending: raise ValueError("premature writer finish")
                self.finished = True
            else: raise ValueError("unexpected writer message")

    def check(self):
        if self.closed: raise ValueError("async journal closed")
        self._drain()
        if self.pending and self.clock()-self.pending[0][1] > ACK_TIMEOUT_S:
            raise TimeoutError("durability acknowledgement deadline exceeded")
        if not self.process.is_alive() and not self.finished: raise OSError("writer process exited")

    def append(self, record):
        self.check()
        data = encode(record)
        if len(data)>65536: raise ValueError("async record too large")
        if len(self.pending)>=MAX_RECORDS or self.pending_bytes+len(data)>MAX_BYTES:
            raise BufferError("async journal capacity exhausted")
        sequence = self.sequence+1
        self.inbox.put_nowait(("APPEND",sequence,data))
        self.sequence = sequence
        self.pending.append((sequence,self.clock(),len(data)))
        self.pending_bytes += len(data)
        self.maximum_pending = max(self.maximum_pending,len(self.pending))

    def finish(self, report):
        self.check(); deadline = time.monotonic()+3
        while self.pending:
            self.check()
            if time.monotonic()>deadline: raise TimeoutError("writer drain timeout")
            time.sleep(.001)
        self.inbox.put_nowait(("FINISH",dict(report,recording_policy=self.policy)))
        while not self.finished:
            self.check()
            if time.monotonic()>deadline: raise TimeoutError("writer finish timeout")
            time.sleep(.001)
        self.process.join(timeout=1)
        if self.process.is_alive() or self.process.exitcode!=0: raise OSError("writer did not exit cleanly")
        # Final copying/metadata is outside the timed FDM loop. Only the root
        # result.json is authoritative; writer-private is never a run success.
        private = self.root/"writer-private"
        from tools.simulation.session_contract import decode
        sealed = decode((private/"result.json").read_bytes())
        digest = hashlib.sha256()
        with (private/"truth.jsonl").open("rb") as source,(self.root/"truth.jsonl").open("xb") as dest:
            while True:
                chunk = source.read(65536)
                if not chunk: break
                if dest.write(chunk)!=len(chunk): raise OSError("short final copy")
                digest.update(chunk)
            dest.flush();os.fsync(dest.fileno())
        if sealed["truth_sha256"]!=digest.hexdigest() or sealed["truth_records"]!=self.sequence:
            raise ValueError("sealed trace mismatch")
        pending = self.root/"result.pending.json"
        with pending.open("xb") as output:
            data = encode(sealed)
            if output.write(data)!=len(data): raise OSError("short final result")
            output.flush();os.fsync(output.fileno())
        if os.name=="nt": os.rename(pending,self.root/"result.json")
        else: os.link(pending,self.root/"result.json")
        self.close()

    def close(self):
        if self.closed: return
        self.closed = True
        if self.process.is_alive():
            self.process.terminate();self.process.join(timeout=.5)
            if self.process.is_alive():self.process.kill();self.process.join(timeout=.5)
        for channel in (self.inbox,self.acks):
            channel.cancel_join_thread();channel.close()
