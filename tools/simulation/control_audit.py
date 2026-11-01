"""Bounded live sidecar integrity correlation, NOT executive/aerodynamic replay.

No admission events or wall timestamps exist in this candidate. A PASS never
proves admission-time horizon, wall-lease compliance, authenticity or fidelity.
"""
import hashlib
from tools.simulation import session_contract as c
from tools.simulation.pilot_fixture import INPUTS
from tools.simulation.session_engine import encode

SCHEMA = "fuso-avionics.sil-control-audit.v1"
FILE = "control-audit.jsonl"
SCOPE = "SYNC_SIDECAR_INTEGRITY_V1"
MAX_BYTES, MAX_RECORDS, MAX_LINE = 1048576, 1024, 65536


def execution_policy(authority_id, lease_id, lease_until_tick):
    c.token(authority_id);c.token(lease_id);c.integer(lease_until_tick,1,360000)
    return dict(mode="LOCAL_OWNER_SYNC_V1",authority_id=authority_id,lease_id=lease_id,
                lease_until_tick=lease_until_tick,audit_file=FILE,controls=dict(INPUTS),
                max_wall_lease_ns=5_000_000_000)


def verify_bytes(data, metadata, manifest, policy, *, epoch_end_tick):
    c.require(type(data) is bytes and 1<=len(data)<=MAX_BYTES,"control audit size")
    c.manifest(manifest);c.integer(epoch_end_tick,0,360000)
    c.exact(metadata,"schema scope file sha256 records bytes")
    c.require(metadata["schema"]==SCHEMA and metadata["scope"]==SCOPE
              and metadata["file"]==FILE,"control audit metadata policy")
    c.integer(metadata["records"],2,MAX_RECORDS);c.integer(metadata["bytes"],1,MAX_BYTES)
    c.digest(metadata["sha256"])
    c.require(metadata["bytes"]==len(data)
              and metadata["sha256"]==hashlib.sha256(data).hexdigest(),"control audit digest")
    c.require(data.endswith(b"\n"),"truncated control audit")
    lines=data.splitlines(keepends=True)
    c.require(len(lines)==metadata["records"]<=MAX_RECORDS,"control audit records")
    binding=None;last_tick=0;released=False
    intents={};seen=set();sequences=set();applied=set();rejected={};controls=set();phase=0
    base="schema kind index event session_id epoch tick simulated flight_approval"
    for index,raw in enumerate(lines):
        c.require(len(raw)<=MAX_LINE and raw.endswith(b"\n"),"control audit line bound")
        row=c.decode(raw)
        c.require(isinstance(row,dict),"control audit object")
        event=row.get("event")
        c.require(isinstance(event,str) and event in ("BOUND","INTENT","APPLIED","REJECTED","RELEASED"),
                  "control audit event")
        extra={"BOUND":"binding","INTENT":"request","APPLIED":"request ack property readback",
               "REJECTED":"request ack","RELEASED":"scope uncertain_commands"}[event]
        c.exact(row,base+" "+extra)
        c.integer(row["index"],index,index);c.integer(row["tick"],last_tick,epoch_end_tick)
        c.require(row["schema"]==SCHEMA and row["kind"]=="CONTROL_AUDIT"
                  and row["session_id"]==manifest["session_id"]
                  and type(row["epoch"]) is int and row["epoch"]==manifest["epoch"]
                  and row["simulated"] is True and row["flight_approval"] is False
                  and not released,"control audit identity/lifecycle")
        tick=row["tick"]
        if tick!=last_tick:
            c.require(set(intents)==applied,"unresolved prior physical step")
            phase=0
        last_tick=tick
        next_phase={"BOUND":0,"REJECTED":0,"INTENT":1,"APPLIED":2,"RELEASED":3}[event]
        c.require(next_phase>=phase,"control step audit phase")
        phase=next_phase
        if event=="BOUND":
            c.require(index==0 and tick==0 and binding is None,"control binding order")
            binding=row["binding"]
            c.exact(binding,"manifest authority_id lease_id lease_until_tick")
            c.manifest(binding["manifest"])
            c.require(encode(binding["manifest"])==encode(manifest),"control manifest mismatch")
            c.require(encode(policy)==encode(execution_policy(binding["authority_id"],binding["lease_id"],
                                                binding["lease_until_tick"])),"unfrozen control policy")
            continue
        c.require(binding is not None,"control audit missing binding")
        if event=="RELEASED":
            c.require(index==len(lines)-1 and row["scope"]=="OVERRIDE_MAP_ONLY"
                      and row["uncertain_commands"]==[] and set(intents)==applied,
                      "control incomplete/uncertain release")
            released=True
            continue
        request=row["request"]
        c.require(isinstance(request,dict),"audit command object")
        apply=request.get("apply_tick");c.integer(apply,1)
        c.command(request,manifest,current_tick=apply-1,authority_id=binding["authority_id"],
                  lease_id=binding["lease_id"],lease_until_tick=binding["lease_until_tick"])
        c.integer(tick,request["apply_tick"],request["expires_tick"])
        identity=request["command_id"]
        if event in ("INTENT","REJECTED"):
            c.require(request["sequence"] not in sequences,"duplicate control sequence")
            sequences.add(request["sequence"])
        if event=="INTENT":
            c.require(identity not in seen and len(seen)<256 and request["action"]=="SET_CONTROL"
                      and request["args"]["control"] in INPUTS,"audit intent capability/identity")
            key=(tick,request["args"]["control"])
            c.require(key not in controls,"conflicting control intent")
            controls.add(key);seen.add(identity);intents[identity]=(request,tick)
        else:
            ack=c.acknowledgement(row["ack"],request)
            if event=="APPLIED":
                c.require(identity in intents and identity not in applied
                          and intents[identity]==(request,tick) and ack["status"]=="APPLIED"
                          and ack["applied_tick"]==tick,"audit applied correlation")
                c.number(row["readback"])
                c.require(row["property"]==INPUTS[request["args"]["control"]]
                          and abs(row["readback"]-request["args"]["value"])<=1e-12,
                          "audit false readback")
                applied.add(identity)
            else:
                c.require(identity not in seen and len(seen)<256 and ack["status"]=="REJECTED"
                          and ack["reason"] in ("UNSUPPORTED","INVALID"),"audit rejected correlation")
                seen.add(identity);rejected[identity]=(request,tick,ack["reason"])
    c.require(released,"control audit not released")
    for request,tick,reason in rejected.values():
        if reason=="INVALID":
            c.require(request["action"]=="SET_CONTROL"
                      and (tick,request["args"]["control"]) in controls,"unexplained control conflict")
    return dict(status="PASS_INTEGRITY_ONLY",applied=len(applied),rejected=len(rejected),
                records=len(lines),sha256=metadata["sha256"],flight_approval=False,
                limitations="NO_ADMISSION_WALL_TIME_AUTHENTICITY_OR_EXECUTIVE_REPLAY_PROOF")
