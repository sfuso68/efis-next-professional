"""Frozen local pilot fixture; no remote authentication or hardware commands.

Only three proven C172P FDM inputs. All commands are admitted at tick zero,
applied in the first second, and released after tick100 or five wall seconds.
No lifecycle commands, pause, epoch changes or live console are implied.
"""
import copy
from tools.simulation import session_contract as c
from tools.simulation.command_queue import CommandQueue
from tools.simulation.session_engine import encode

INPUTS = {"aileron":"fcs/aileron-cmd-norm", "elevator":"fcs/elevator-cmd-norm",
          "throttle":"fcs/throttle-cmd-norm"}


def validate_plan(plan):
    c.require(isinstance(plan, list) and 1 <= len(plan) <= 16, "pilot fixture size")
    seen = set()
    for item in plan:
        c.exact(item, "apply_tick control value")
        c.integer(item["apply_tick"], 1, 100)
        c.require(isinstance(item["control"], str) and item["control"] in INPUTS, "unsupported fixture control")
        c.number(item["value"], *c.ICD["controls"][item["control"]])
        key = (item["apply_tick"], item["control"])
        c.require(key not in seen, "conflicting same-tick control")
        seen.add(key)
    return copy.deepcopy(plan)


def requests(plan, manifest):
    return [dict(schema=c.SCHEMA, kind="COMMAND", session_id=manifest["session_id"],
        epoch=manifest["epoch"], simulated=True, flight_approval=False,
        command_id=f"fixture-{i}", sequence=i, authority_id="local-fixture",
        lease_id="fixture-lease", apply_tick=item["apply_tick"], expires_tick=item["apply_tick"],
        action="SET_CONTROL", args=dict(control=item["control"], value=item["value"]))
        for i, item in enumerate(validate_plan(plan), 1)]


class PilotFixture:
    def __init__(self, plan, manifest, backend, journal, **queue_options):
        self.manifest = copy.deepcopy(manifest)
        self.backend, self.journal = backend, journal
        self.queue = CommandQueue(manifest, principal="local-fixture", lease_id="fixture-lease",
                                  lease_until_tick=100, lease_duration_ns=5_000_000_000, **queue_options)
        self.inflight = ()
        self.released = False
        self.applied = 0
        for request in requests(plan, manifest):
            c.require(INPUTS[request["args"]["control"]] in backend.catalog, "FDM capability missing")
            ack = self.queue.submit(encode(request), authenticated_principal="local-fixture", current_tick=0)
            c.require(ack["status"] == "QUEUED", "fixture not admitted")
            self._record("ADMITTED", 0, request=request, ack=ack)

    def _record(self, event, tick, **fields):
        self.journal.append(dict(kind="PILOT_FIXTURE", event=event, session_id=self.manifest["session_id"],
            epoch=self.manifest["epoch"], total_ticks=tick, simulated=True, flight_approval=False, **fields))

    def before_step(self):
        tick = self.backend.ticks+1
        self.inflight = tuple(c.decode(raw) for raw in self.queue.take_due(current_tick=tick))
        if self.queue.closed and not self.released:
            self.backend.clear_test_controls()
            self._record("RELEASED", tick)
            self.released = True
        for request in self.inflight:
            self._record("INTENT", tick, request=request)
            c.require(self.queue.authorize_dispatch(request,current_tick=tick),"fixture dispatch revoked")
            self.backend.set_test_control(**request["args"])

    def after_step(self):
        tick = self.backend.ticks
        for request in self.inflight:
            prop = INPUTS[request["args"]["control"]]
            value = self.backend.read(prop)
            c.number(value)
            c.require(abs(value-request["args"]["value"]) <= 1e-12, "FDM input readback mismatch")
            ack = CommandQueue._ack(request)
            ack.update(status="APPLIED", applied_tick=tick)
            c.acknowledgement(ack, request)
            self._record("APPLIED", tick, request=request, ack=ack, property=prop, readback=value)
            self.queue.record_outcome(request,ack,current_tick=tick)
            self.applied += 1
        self.inflight = ()


class Audit:
    """Trace correlation, NOT an independent aerodynamic oracle."""
    def __init__(self, plan, manifest):
        self.manifest = copy.deepcopy(manifest)
        self.expected = {r["command_id"]:r for r in requests(plan, manifest)} if plan is not None else {}
        self.admitted, self.intents, self.applied = set(), set(), set()
        self.last_tick = 0
        self.released = False

    def record(self, r, phase):
        c.require(phase == "RUNNING", "pilot lifecycle")
        tick = r["total_ticks"]
        c.integer(tick, self.last_tick)
        self.last_tick = tick
        event = r["event"]
        if event == "RELEASED":
            c.exact(r, "kind event session_id epoch total_ticks simulated flight_approval")
            c.require(not self.released and bool(self.expected) and not self.intents, "pilot release order")
            self.released = True
            return
        c.require(event in ("ADMITTED", "INTENT", "APPLIED") and not self.released, "pilot event")
        base = "kind event session_id epoch total_ticks simulated flight_approval request"
        c.exact(r, base + (" ack property readback" if event == "APPLIED" else " ack" if event == "ADMITTED" else ""))
        request = r["request"]
        c.command(request, self.manifest, current_tick=0, authority_id="local-fixture",
                  lease_id="fixture-lease", lease_until_tick=100)
        identity = request["command_id"]
        c.require(identity in self.expected and request == self.expected[identity], "pilot request differs from frozen plan")
        if event == "ADMITTED":
            c.require(tick == 0 and identity not in self.admitted, "duplicate pilot admission")
            c.require(r["ack"] == CommandQueue._ack(request), "pilot admission ack")
            self.admitted.add(identity)
        elif event == "INTENT":
            c.require(identity in self.admitted and identity not in self.applied and identity not in self.intents
                      and tick == request["apply_tick"], "pilot intent order")
            self.intents.add(identity)
        else:
            c.require(identity in self.intents and tick == request["apply_tick"], "pilot application order")
            c.acknowledgement(r["ack"], request)
            c.require(r["ack"]["status"] == "APPLIED" and r["ack"]["applied_tick"] == tick
                and r["property"] == INPUTS[request["args"]["control"]], "pilot application identity")
            c.number(r["readback"])
            c.require(abs(r["readback"]-request["args"]["value"]) <= 1e-12, "pilot application readback")
            self.intents.remove(identity)
            self.applied.add(identity)

    def finish(self):
        c.require(not self.intents and self.applied == set(self.expected), "incomplete pilot fixture")
        return len(self.applied)
