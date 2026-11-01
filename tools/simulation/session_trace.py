"""Offline trace integrity/normalization verifier, not an independent FDM oracle."""
import argparse
import hashlib
import json
import os
import stat
from contextlib import contextmanager
from pathlib import Path
from tools.simulation import session_contract as c
from tools.simulation.session_engine import encode
from tools.simulation.session_jsbsim import normalize
from tools.simulation.pilot_fixture import Audit

MAX_TRUTH = 64*1024*1024


def _fingerprint(info):
    base=(info.st_dev,info.st_ino,info.st_size,info.st_mtime_ns)
    return base if os.name=="nt" else (*base,info.st_ctime_ns)


@contextmanager
def checked_stream(path,limit):
    info=path.lstat()
    c.require(stat.S_ISREG(info.st_mode) and not path.is_symlink() and info.st_size<=limit,
              "trace source type/size")
    with path.open("rb") as stream:
        c.require(_fingerprint(os.fstat(stream.fileno()))==_fingerprint(info),"trace source changed before read")
        yield stream
        c.require(_fingerprint(os.fstat(stream.fileno()))==_fingerprint(info),"trace source changed during read")


def verify(directory, *, allow_control_audit=False):
    root=Path(directory)
    with checked_stream(root/"result.json",65536) as stream:result=c.decode(stream.read(65537))
    has_control="control_execution" in result
    c.require(not has_control or allow_control_audit,"live control sidecar replay/archive unsupported")
    c.require(result["status"]=="COMPLETE_SOFTWARE_ONLY" and result["simulated"] is True
              and result["flight_approval"] is False,"not a completed SIM run")
    digest=hashlib.sha256();count=frames=0;pending=None;manifest=None
    phase="NEW";offset=0;last_sequence=-1;last_mono=-1;last_total=0;pilot=None
    initial_manifest=None;control_policy=None;control_epoch_end=None
    with checked_stream(root/"truth.jsonl",MAX_TRUTH) as stream:
        while True:
            raw=stream.readline(65537)
            c.require(stream.tell()<=MAX_TRUTH,"truth byte bound")
            if not raw:break
            c.require(len(raw)<=65536 and raw.endswith(b"\n"),"truncated or oversize trace record")
            digest.update(raw);count+=1
            c.require(count<=200000,"trace bound")
            r=c.decode(raw)
            if r["kind"]=="RUN_CONFIGURATION":
                c.require(count==1,"configuration order")
                manifest=c.manifest(r["manifest"])
                initial_manifest=dict(manifest)
                control_policy=r["configuration"].get("control_policy")
                c.require(("control_policy" in r["configuration"])==has_control,
                          "control policy/result marker mismatch")
                c.require(control_policy is None or allow_control_audit,
                          "live control sidecar replay/archive unsupported")
                c.require(control_policy is None or r["configuration"].get("pilot_fixture") is None,
                          "competing control fixture")
                c.require(manifest["epoch"]==0,"initial epoch")
                c.require(hashlib.sha256(encode(r["configuration"])).hexdigest()==manifest["config_sha256"],"configuration digest")
                pilot=Audit(r["configuration"].get("pilot_fixture"),manifest)
                continue
            c.require(manifest is not None and r["session_id"]==manifest["session_id"]
                and r["simulated"] is True and r["flight_approval"] is False,"trace identity/SIM")
            if r["kind"]=="PILOT_FIXTURE":
                c.require(pending is None and r["epoch"]==0 and manifest["epoch"]==0
                    and last_total<=r["total_ticks"]<=last_total+5,"pilot frame/epoch order")
                pilot.record(r,phase)
                continue
            if r["kind"]=="PHYSICS_WORLD":
                c.require(phase=="RUNNING" and pending is None,"physics order")
                pending=r;continue
            c.require(r["kind"]=="SESSION_EVENT","unknown trace record")
            event=r["event"]
            if event=="EPOCH":
                c.require(phase=="PAUSED" and pending is None and r["epoch"]==manifest["epoch"]+1,"epoch order")
                c.require(r["total_ticks"]==last_total,"pause advanced physical clock")
                offset=r["parent_total_ticks"];c.require(offset==last_total,"epoch parent")
                manifest=dict(manifest,epoch=r["epoch"]);last_sequence=-1;phase="RESUMING"
                continue
            c.require(r["epoch"]==manifest["epoch"],"event epoch")
            if event=="FRAME":
                c.require(phase=="RUNNING" and pending is not None,"frame order")
                f=c.frame(r["frame"],manifest)
                c.require(f["sequence"]==last_sequence+1 and f["producer_mono_ns"]>=last_mono,"frame continuity")
                total=r["total_ticks"]
                c.require(total==offset+f["tick"] and pending["total_ticks"]==total
                          and pending["relative_tick"]==f["tick"] and pending["epoch"]==f["epoch"],"physical/epoch clock")
                c.require(pending["frame"]["sim_time_ms"]==total*10,"raw physical clock")
                c.require(normalize(pending["frame"],manifest,f["tick"],f["producer_mono_ns"])==f,"normalization differs")
                pending=None;frames+=1;last_sequence=f["sequence"];last_mono=f["producer_mono_ns"];last_total=total
            else:
                c.require(pending is None,"unpaired physics")
                transitions={"CREATED":("NEW","CREATED"),"VALIDATED":("CREATED","VALIDATED"),
                    "ARMED_TEST":("VALIDATED","ARMED_TEST"),"PAUSED":("RUNNING","PAUSED"),
                    "END_OF_DATA":("STOPPING","ENDED")}
                if event=="RUNNING":
                    c.require(phase in ("ARMED_TEST","RESUMING"),"run transition");phase="RUNNING"
                elif event=="STOPPING":
                    c.require(phase in ("RUNNING","PAUSED"),"stop transition");phase="STOPPING"
                else:
                    c.require(event in transitions and phase==transitions[event][0],"lifecycle transition")
                    phase=transitions[event][1]
                if event in ("PAUSED","STOPPING"):
                    c.require(r["total_ticks"]>=last_total,"physical clock reversed")
                    last_total=r["total_ticks"]
                    if manifest["epoch"]==0 and control_epoch_end is None:
                        control_epoch_end=last_total
    c.require(phase=="ENDED" and pending is None,"incomplete lifecycle")
    applied=pilot.finish()
    c.require(result["session_id"]==manifest["session_id"] and result["truth_records"]==count
        and result["truth_sha256"]==digest.hexdigest() and result["frames"]==frames
        and result["total_ticks"]==last_total,"trace result digest/count")
    report=dict(status="PASS",frames=frames,total_ticks=last_total,epochs=manifest["epoch"]+1,
                pilot_commands_applied=applied,
                scope="TRACE_INTEGRITY_NOT_MODEL_VALIDATION",flight_approval=False)
    if has_control:
        from tools.simulation.control_audit import FILE,MAX_BYTES,verify_bytes
        with checked_stream(root/FILE,MAX_BYTES) as audit_stream:
            data=audit_stream.read(MAX_BYTES+1)
        report["control_execution"]=verify_bytes(data,result["control_execution"],initial_manifest,
                                               control_policy,epoch_end_tick=control_epoch_end)
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("directory",type=Path)
    parser.add_argument("--live-control",action="store_true",help="explicit control audit integrity verification")
    args=parser.parse_args()
    try:
        report=verify(args.directory,allow_control_audit=args.live_control)
        if args.live_control:c.require("control_execution" in report,"live audit required")
        print(json.dumps(report))
    except (ValueError,OSError,KeyError,TypeError) as exc:parser.exit(2,f"Session trace FAILED: {exc}\n")


if __name__=="__main__":main()
