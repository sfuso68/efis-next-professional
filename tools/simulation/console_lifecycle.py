"""Local SIL pause/resume audit contract; never lease renewal or hardware."""
from tools.simulation import session_contract as c
from tools.simulation.session_engine import encode

MODE = "PAUSE_RESUME_IDS_V1"
SCHEMA = "fuso-avionics.sil-console-lifecycle.v1"


def request(obj):
    c.exact(obj, "action command_id session_id expected_epoch")
    c.require(obj["action"] in ("PAUSE", "RESUME"), "console lifecycle action")
    c.token(obj["command_id"]);c.token(obj["session_id"])
    c.integer(obj["expected_epoch"])
    return obj


def state(engine):
    return dict(state=engine.state, epoch=engine.manifest["epoch"], total_ticks=engine.total_ticks)


def outcome(obj, before, session_id):
    if obj["session_id"] != session_id:return "REJECTED_SESSION"
    if obj["expected_epoch"] != before["epoch"]:return "REJECTED_EPOCH"
    required = "RUNNING" if obj["action"] == "PAUSE" else "PAUSED"
    return "APPLIED" if before["state"] == required else "REJECTED_STATE"


def record(index, stage, obj, before, after, result, session_id):
    point = before if after is None else after
    return dict(schema=SCHEMA, kind="CONSOLE_LIFECYCLE", index=index, stage=stage,
                session_id=session_id, epoch=point["epoch"], total_ticks=point["total_ticks"],
                simulated=True, flight_approval=False, request=obj, before=before,
                after=after, outcome=result)


class TraceAudit:
    """Correlates lifecycle intent/results with actual intervening trace events.

    Complete trace integrity only; async intent may not be fsynced at transition
    time. No recovery/reexecution facility or durable immediate ACK is provided.
    """
    def __init__(self, enabled):
        self.enabled = enabled
        self.index = self.completed = self.bytes = 0
        self.identities = set()
        self.pending = None
        self.events = []
        self.running_seen = False

    def transition(self, event):
        if self.enabled:
            action={"PAUSED":"PAUSE","EPOCH":"RESUME"}.get(event)
            if event=="RUNNING" and self.running_seen:action="RESUME"
            if action is not None:
                c.require(self.pending is not None and self.pending["request"]["action"]==action,
                          "unaudited console lifecycle transition")
            if event=="RUNNING":self.running_seen=True
        if self.pending is not None:self.events.append(event)
        c.require(len(self.events)<=3, "lifecycle transition event bound")

    def accept(self, row, *, phase, epoch, session_id, last_total, unpaired):
        c.require(self.enabled and not unpaired, "lifecycle policy/frame boundary")
        c.exact(row, "schema kind index stage session_id epoch total_ticks simulated flight_approval request before after outcome")
        c.require(row["schema"]==SCHEMA and row["kind"]=="CONSOLE_LIFECYCLE"
                  and row["simulated"] is True and row["flight_approval"] is False
                  and row["session_id"]==session_id, "lifecycle envelope")
        c.integer(row["index"]);c.integer(row["epoch"]);c.integer(row["total_ticks"])
        c.require(row["index"]==self.index and row["epoch"]==epoch, "lifecycle index/epoch")
        obj=request(row["request"])
        before=row["before"]
        self._point(before)
        if row["stage"]=="INTENT":
            c.require(self.pending is None and row["after"] is None and row["outcome"] is None,
                      "lifecycle overlapping/nonempty intent")
            c.require(before["state"]==phase and before["epoch"]==epoch
                      and last_total<=before["total_ticks"]<=last_total+4
                      and row["total_ticks"]==before["total_ticks"], "lifecycle intent position")
            c.require(obj["command_id"] not in self.identities and len(self.identities)<256,
                      "lifecycle duplicate/capacity")
            self.bytes+=len(encode(obj))
            c.require(self.bytes<=524288,"lifecycle history bytes")
            self.identities.add(obj["command_id"])
            self.pending=row;self.events=[]
        elif row["stage"]=="RESULT":
            c.require(self.pending is not None and encode(obj)==encode(self.pending["request"])
                      and encode(before)==encode(self.pending["before"]), "lifecycle result identity")
            after=row["after"];self._point(after)
            result=outcome(obj,before,session_id)
            expected=dict(before)
            events=[]
            if result=="APPLIED":
                if obj["action"]=="PAUSE":expected["state"]="PAUSED";events=["PAUSED"]
                else:expected.update(state="RUNNING",epoch=before["epoch"]+1);events=["EPOCH","RUNNING","FRAME"]
            c.require(row["outcome"]==result and encode(after)==encode(expected)
                      and after["state"]==phase and after["epoch"]==epoch
                      and row["total_ticks"]==after["total_ticks"]
                      and self.events==events, "lifecycle result/transition mismatch")
            self.pending=None;self.events=[];self.completed+=1
        else:raise ValueError("lifecycle stage")
        self.index+=1

    @staticmethod
    def _point(obj):
        c.exact(obj,"state epoch total_ticks")
        c.require(obj["state"] in ("RUNNING","PAUSED"),"lifecycle state")
        c.integer(obj["epoch"]);c.integer(obj["total_ticks"])

    def finish(self):
        c.require(self.pending is None,"incomplete lifecycle outcome")
        return dict(completed=self.completed, scope="TRACE_INTEGRITY_NOT_DURABILITY_OR_REARM")

    def controls(self, verified_sidecar):
        """Cross-check observed shared history after sidecar semantic validation.

        Admission-only failures are not present in the old sidecar: this is
        an observed lower bound, NOT proof of the entire ingress history.
        """
        if not self.enabled:return
        from tools.simulation.pilot_fixture import INPUTS
        controls={}
        for raw in verified_sidecar.splitlines():
            row=c.decode(raw)
            if row["event"] not in ("INTENT","REJECTED"):continue
            obj=row["request"]
            c.require(obj["action"]=="SET_CONTROL" and obj["args"]["control"] in INPUTS,
                      "console sidecar capability")
            shorthand=dict(action="SET_CONTROL",command_id=obj["command_id"],sequence=obj["sequence"],
                           control=obj["args"]["control"],value=obj["args"]["value"])
            controls[obj["command_id"]]=len(encode(obj))+len(encode(shorthand))
        c.require(not self.identities.intersection(controls),"shared console identity reused")
        c.require(len(self.identities)+len(controls)<=256 and self.bytes+sum(controls.values())<=524288,
                  "shared console observed history capacity")
