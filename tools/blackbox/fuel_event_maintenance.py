#!/usr/bin/env python3
"""Retention, signed archives, flight index and incident export for fuel events."""
import argparse, datetime as dt, gzip, hashlib, hmac, json, shutil, sqlite3, tarfile, tempfile
from pathlib import Path

def canonical(value): return json.dumps(value,sort_keys=True,separators=(",",":"))
def signature(path,key):
    digest=hashlib.sha256(path.read_bytes()).hexdigest(); mac=hmac.new(key,path.read_bytes(),hashlib.sha256).hexdigest()
    path.with_suffix(path.suffix+".sha256").write_text(digest+"  "+path.name+"\n");path.with_suffix(path.suffix+".hmac").write_text(mac+"  "+path.name+"\n")
def verify(path,key):
    sha=path.with_suffix(path.suffix+".sha256");mac=path.with_suffix(path.suffix+".hmac")
    if not sha.exists() or not mac.exists(): return False
    content=path.read_bytes();return hmac.compare_digest(sha.read_text().split()[0],hashlib.sha256(content).hexdigest()) and hmac.compare_digest(mac.read_text().split()[0],hmac.new(key,content,hashlib.sha256).hexdigest())
def rows(db,where="1",args=()): return db.execute("SELECT id,source,received_utc,event_utc,state,payload FROM events WHERE "+where+" ORDER BY received_utc",args).fetchall()
def event_record(row): return {"id":row[0],"source":row[1],"received_utc":row[2],"event_utc":row[3],"state":row[4],"event":json.loads(row[5])}
def write_archive(path,records,key):
    with gzip.open(path,"wt",encoding="utf-8") as output:
        for row in records: output.write(canonical(event_record(row))+"\n")
    signature(path,key)
def build_index(db,path):
    sessions=[];current=None
    for row in rows(db):
        stamp=dt.datetime.fromisoformat(row[2].replace("Z","+00:00"))
        if current is None or (stamp-current["last_stamp"]).total_seconds()>1800:
            current={"session_id":stamp.strftime("%Y%m%dT%H%M%SZ"),"start_utc":row[2],"end_utc":row[2],"records":0,"sources":set(),"last_stamp":stamp};sessions.append(current)
        current["end_utc"]=row[2];current["records"]+=1;current["sources"].add(row[1]);current["last_stamp"]=stamp
    for item in sessions: item["sources"]=sorted(item["sources"]);item.pop("last_stamp")
    tmp=path.with_suffix(".tmp");tmp.write_text(canonical({"schema":"efis.blackbox.flight-index.v1","sessions":sessions}));tmp.replace(path)
def maintain(db,root,key,retention):
    archive=root/"archives";archive.mkdir(parents=True,exist_ok=True);today=dt.datetime.now(dt.timezone.utc).date().isoformat()
    dates=[r[0] for r in db.execute("SELECT DISTINCT substr(received_utc,1,10) FROM events WHERE substr(received_utc,1,10) < ?",(today,))]
    for day in dates:
        target=archive/(day+".fuel-events.jsonl.gz")
        if not target.exists(): write_archive(target,rows(db,"substr(received_utc,1,10)=?",(day,)),key)
    build_index(db,root/"flight-index.json")
    cutoff=(dt.datetime.now(dt.timezone.utc)-dt.timedelta(days=retention)).date().isoformat()
    for path in archive.glob("*.fuel-events.jsonl.gz"):
        if path.name[:10]<cutoff:
            for item in (path,path.with_suffix(path.suffix+".sha256"),path.with_suffix(path.suffix+".hmac")): item.unlink(missing_ok=True)
def incident(db,root,key,hours):
    out=root/"incidents";out.mkdir(parents=True,exist_ok=True);since=(dt.datetime.now(dt.timezone.utc)-dt.timedelta(hours=hours)).isoformat().replace("+00:00","Z");stamp=dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    with tempfile.TemporaryDirectory() as temp:
        folder=Path(temp);records=rows(db,"received_utc>=?",(since,));(folder/"events.jsonl").write_text("".join(canonical(event_record(r))+"\n" for r in records));shutil.copy2(root/"flight-index.json",folder/"flight-index.json") if (root/"flight-index.json").exists() else None
        (folder/"manifest.json").write_text(canonical({"schema":"efis.blackbox.incident.v1","created_utc":stamp,"since_utc":since,"records":len(records)}));target=out/(stamp+".incident.tar.gz")
        with tarfile.open(target,"w:gz") as tar:
            for item in folder.iterdir(): tar.add(item,arcname=item.name)
    signature(target,key);return target
def export_usb(root,target,key):
    signed=list((root/"archives").glob("*.gz"))+list((root/"incidents").glob("*.gz"))
    invalid=[str(path) for path in signed if not verify(path,key)]
    if invalid: raise RuntimeError("signature verification failed: "+", ".join(invalid))
    target.mkdir(parents=True,exist_ok=True);destination=target/("EFIS-BLACKBOX-"+dt.datetime.now().strftime("%Y%m%d-%H%M%S"));destination.mkdir()
    for name in ("archives","incidents"):
        if (root/name).exists(): shutil.copytree(root/name,destination/name)
    if (root/"flight-index.json").exists(): shutil.copy2(root/"flight-index.json",destination)
    return destination
def main():
    p=argparse.ArgumentParser();p.add_argument("command",choices=("maintain","incident","verify","export-usb"));p.add_argument("--database",required=True);p.add_argument("--root",required=True);p.add_argument("--key",required=True);p.add_argument("--retention-days",type=int,default=180);p.add_argument("--hours",type=int,default=2);p.add_argument("--usb");a=p.parse_args();root=Path(a.root);root.mkdir(parents=True,exist_ok=True);key=Path(a.key).read_bytes();db=sqlite3.connect(a.database)
    if a.command=="maintain": maintain(db,root,key,a.retention_days)
    elif a.command=="incident": print(incident(db,root,key,a.hours))
    elif a.command=="verify":
        checked=list((root/"archives").glob("*.gz"))+list((root/"incidents").glob("*.gz"));print(canonical({"verified":sum(verify(path,key) for path in checked),"total":len(checked)}));raise SystemExit(0 if all(verify(path,key) for path in checked) else 2)
    elif not a.usb: p.error("--usb is required for export-usb")
    else: print(export_usb(root,Path(a.usb),key))
if __name__=="__main__": main()
