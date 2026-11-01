"""Offline candidate multi-lease planner. NEVER creates/renews a live lease.

Snapshots and their hashes are caller assertions, not authenticated evidence.
No engine, FDM, clock, queue, credentials, network or activation API is used.
PASS means structural/planning checks only; runtime revalidation is mandatory.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

from tools.simulation import session_contract as c
from tools.simulation.session_engine import encode

SCHEMA = "fuso-avionics.sil-rearm-preflight.v1"
MODE = "FROZEN_LEASE_CATALOGUE_PLAN_ONLY"


def sha(value):return hashlib.sha256(encode(value)).hexdigest()


def policy(obj):
    c.exact(obj,"schema kind mode session_id authority_id model_sha256 source_commit leases")
    c.require(obj["schema"]==SCHEMA and obj["kind"]=="POLICY" and obj["mode"]==MODE,"rearm policy mode")
    c.token(obj["session_id"]);c.token(obj["authority_id"]);c.digest(obj["model_sha256"])
    c.require(isinstance(obj["source_commit"],str) and re.fullmatch(r"[a-f0-9]{40}",obj["source_commit"]),"source commit")
    leases=obj["leases"]
    c.require(isinstance(leases,list) and 2<=len(leases)<=8,"lease catalogue bound")
    seen=set()
    for index,lease in enumerate(leases):
        c.exact(lease,"lease_id epoch wall_limit_ns tick_limit")
        c.token(lease["lease_id"]);c.integer(lease["epoch"],index,index)
        c.integer(lease["wall_limit_ns"],1,5_000_000_000)
        c.integer(lease["tick_limit"],1,500)
        c.require(lease["lease_id"] not in seen,"duplicate lease identity")
        seen.add(lease["lease_id"])
    return obj


def snapshot(obj):
    c.exact(obj,"schema kind session_id authority_id epoch state revision config_sha256 model_sha256 source_commit "
                "current_lease_id used_leases revoked audit_closed audit_sha256 pending dispatched uncertain "
                "history_ids history_bytes history_sha256 simulated flight_approval")
    c.require(obj["schema"]==SCHEMA and obj["kind"]=="SNAPSHOT" and obj["simulated"] is True
              and obj["flight_approval"] is False,"rearm snapshot envelope")
    for key in ("session_id","authority_id","current_lease_id"):c.token(obj[key])
    for key in ("config_sha256","model_sha256","audit_sha256","history_sha256"):c.digest(obj[key])
    c.require(isinstance(obj["source_commit"],str) and re.fullmatch(r"[a-f0-9]{40}",obj["source_commit"]),"source commit")
    c.require(obj["state"] in ("RUNNING","PAUSED","FAILED","COMPLETE"),"snapshot state")
    for key in ("epoch","revision","pending","dispatched","uncertain"):c.integer(obj[key])
    for key in ("revoked","audit_closed"):c.require(type(obj[key]) is bool,"snapshot boolean")
    c.integer(obj["history_bytes"],0,524288)
    for key,limit in (("used_leases",8),("history_ids",256)):
        ids=obj[key]
        c.require(isinstance(ids,list) and len(ids)<=limit,"snapshot identity bound")
        for identity in ids:c.token(identity)
        c.require(len(set(ids))==len(ids),"duplicate snapshot identity")
    c.require(obj["history_bytes"]>=sum(len(i) for i in obj["history_ids"]),"history byte lower bound")
    return obj


def request(obj):
    c.exact(obj,"schema kind command_id resume_command_id session_id expected_epoch expected_revision "
                "previous_lease_id next_lease_id expected_config_sha256 expected_history_sha256 policy_sha256 confirmed")
    c.require(obj["schema"]==SCHEMA and obj["kind"]=="PLAN_REARM","rearm request kind")
    for key in ("command_id","resume_command_id","session_id","previous_lease_id","next_lease_id"):c.token(obj[key])
    for key in ("expected_epoch","expected_revision"):c.integer(obj[key])
    for key in ("expected_config_sha256","expected_history_sha256","policy_sha256"):c.digest(obj[key])
    c.require(type(obj["confirmed"]) is bool,"explicit confirmation boolean")
    return obj


def plan(policy_value,snapshot_value,request_value):
    """Pure preflight: no reservation/consumption/dedup store or mutation."""
    p,s,r=policy(policy_value),snapshot(snapshot_value),request(request_value)
    blocked=[]
    def gate(condition,reason):
        if not condition:blocked.append(reason)
    gate(r["confirmed"],"EXPLICIT_CONFIRMATION_REQUIRED")
    gate(s["state"]=="PAUSED","NOT_PAUSED")
    gate(s["revoked"] and s["audit_closed"],"OLD_AUTHORITY_OR_AUDIT_OPEN")
    gate(s["pending"]==s["dispatched"]==s["uncertain"]==0,"UNRESOLVED_COMMANDS")
    gate(r["session_id"]==s["session_id"]==p["session_id"],"SESSION_MISMATCH")
    gate(s["authority_id"]==p["authority_id"],"AUTHORITY_MISMATCH")
    gate(s["source_commit"]==p["source_commit"] and s["model_sha256"]==p["model_sha256"],"SOURCE_OR_MODEL_CHANGED")
    gate(r["expected_config_sha256"]==s["config_sha256"],"CONFIG_CHANGED")
    gate(r["expected_history_sha256"]==s["history_sha256"] and r["expected_revision"]==s["revision"],"STALE_SNAPSHOT")
    gate(r["policy_sha256"]==sha(p),"POLICY_CHANGED")
    gate(r["expected_epoch"]==s["epoch"],"EPOCH_CHANGED")
    ids=[lease["lease_id"] for lease in p["leases"]]
    used=s["used_leases"];n=len(used)
    gate(n>0 and used==ids[:n] and s["current_lease_id"]==used[-1]
         and r["previous_lease_id"]==s["current_lease_id"] and s["epoch"]==n-1,"LEASE_CHAIN_MISMATCH")
    gate(n<len(ids),"LEASE_CATALOGUE_EXHAUSTED")
    gate(n<len(ids) and r["next_lease_id"]==ids[n] and r["next_lease_id"] not in used,"LEASE_NOT_NEXT_UNUSED")
    gate(r["command_id"]!=r["resume_command_id"] and r["command_id"] not in s["history_ids"]
         and r["resume_command_id"] not in s["history_ids"],"COMMAND_ID_ALREADY_USED")
    resume=dict(action="RESUME",command_id=r["resume_command_id"],session_id=r["session_id"],expected_epoch=s["epoch"])
    extra_bytes=len(encode(r))+len(encode(resume))
    gate(len(s["history_ids"])+2<=256 and s["history_bytes"]+extra_bytes<=524288,"HISTORY_CAPACITY")
    result=dict(schema=SCHEMA,kind="PREFLIGHT_RESULT",validation="BLOCKED" if blocked else "PASS_PLAN_ONLY",
                execution="NOT_ARMED",snapshot_attestation="CALLER_ASSERTED_NOT_AUTHENTICATED",
                blockers=blocked,plan=None,plan_sha256=None,simulated=True,flight_approval=False)
    if blocked:return result
    target=dict(p["leases"][n])
    candidate=dict(session_id=s["session_id"],authority_id=p["authority_id"],
        previous_lease_id=s["current_lease_id"],next_lease=target,
        command_id=r["command_id"],resume_request=resume,
        policy_sha256=sha(p),snapshot_sha256=sha(s),request_sha256=sha(r),
        expected_revision=s["revision"],expected_config_sha256=s["config_sha256"],
        expected_history_sha256=s["history_sha256"],closed_audit_sha256=s["audit_sha256"],
        additional_history_ids=2,additional_history_bytes=extra_bytes,
        activation="NOT_IMPLEMENTED_RECHECK_AFTER_IDENTIFIED_RESUME",
        recovery="NOT_IMPLEMENTED_NO_AUTOMATIC_RETRY")
    result.update(plan=candidate,plan_sha256=sha(candidate))
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ("policy","snapshot","request"):parser.add_argument("--"+name,type=Path,required=True)
    args=parser.parse_args()
    try:
        from tools.simulation.session_trace import checked_stream
        values=[]
        for path in (args.policy,args.snapshot,args.request):
            with checked_stream(path,65536) as stream:values.append(c.decode(stream.read(65537)))
        result=plan(*values)
        print(json.dumps(result,sort_keys=True,allow_nan=False))
        if result["validation"]!="PASS_PLAN_ONLY":raise SystemExit(3)
    except (ValueError,OSError,KeyError,TypeError) as exc:parser.exit(2,f"Rearm preflight INVALID: {exc}\n")


if __name__=="__main__":main()
