#!/usr/bin/env python3
"""Retention, signed archives, flight index and incident export for fuel events."""
import argparse, datetime as dt, gzip, hashlib, hmac, json, shutil, sqlite3, struct, tarfile, tempfile
from pathlib import Path

def canonical(value): return json.dumps(value,sort_keys=True,separators=(",",":"))
def file_sha256(path):
    digest=hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda:stream.read(1024*1024),b""): digest.update(block)
    return digest.hexdigest()
def signature(path,key):
    content=path.read_bytes();digest=hashlib.sha256(content).hexdigest();mac=hmac.new(key,content,hashlib.sha256).hexdigest()
    path.with_suffix(path.suffix+".sha256").write_text(digest+"  "+path.name+"\n");path.with_suffix(path.suffix+".hmac").write_text(mac+"  "+path.name+"\n")
def verify(path,key):
    sha=path.with_suffix(path.suffix+".sha256");mac=path.with_suffix(path.suffix+".hmac")
    if not sha.exists() or not mac.exists(): return False
    content=path.read_bytes();return hmac.compare_digest(sha.read_text().split()[0],hashlib.sha256(content).hexdigest()) and hmac.compare_digest(mac.read_text().split()[0],hmac.new(key,content,hashlib.sha256).hexdigest())
def utc_now(): return dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
def write_signed_json(path,payload,key):
    path.parent.mkdir(parents=True,exist_ok=True);tmp=path.with_suffix(path.suffix+".tmp");tmp.write_text(canonical(payload));tmp.replace(path);signature(path,key)
def load_can_manifest(path,key):
    if not path.exists() or not verify(path,key): return {}
    try: return {item["name"]:item for item in json.loads(path.read_text()).get("segments",[])}
    except (KeyError,TypeError,ValueError,json.JSONDecodeError): return {}
def inspect_bbx(path):
    size=path.stat().st_size
    if size<64:return "REJECTED_SHORT",0,size
    with path.open("rb") as stream:header=stream.read(64)
    magic,version,record_size,_,interfaces=struct.unpack_from("<8sHHQB",header)
    if magic!=b"FUSOBBX1":return "REJECTED_MAGIC",0,size
    if version!=1:return "REJECTED_VERSION",0,size
    if record_size!=24:return "REJECTED_RECORD_SIZE",0,size
    if not 1<=interfaces<=4:return "REJECTED_INTERFACES",0,size
    complete=(size-64)//record_size;trailing=(size-64)%record_size
    return ("VALID" if trailing==0 else "RECOVERED_TRUNCATED"),complete,trailing
def seal_can(root,can_root,key):
    manifest=root/"can-segment-manifest.json";previous=load_can_manifest(manifest,key);segments=[]
    for path in sorted(can_root.glob("bbx-*.bbx")):
        stat=path.stat();cached=previous.get(path.name);fingerprint=(stat.st_size,stat.st_mtime_ns,stat.st_ctime_ns)
        if cached and tuple(cached.get("fingerprint",()))==fingerprint:item=cached
        else:
            status,records,trailing=inspect_bbx(path);digest=file_sha256(path)
            item={"name":path.name,"bytes":stat.st_size,"modified_utc":dt.datetime.fromtimestamp(stat.st_mtime,dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),"sha256":digest,"format":status,"records":records,"trailing_bytes":trailing,"fingerprint":list(fingerprint)}
        segments.append(item)
    valid=sum(item["format"] in ("VALID","RECOVERED_TRUNCATED") for item in segments)
    payload={"schema":"efis.blackbox.can-segment-manifest.v1","generated_utc":utc_now(),"source":"FUSOBBX1","segments":segments,"summary":{"segments":len(segments),"valid":valid,"rejected":len(segments)-valid,"bytes":sum(item["bytes"] for item in segments)}}
    write_signed_json(manifest,payload,key);return payload
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
    p=argparse.ArgumentParser();p.add_argument("command",choices=("maintain","incident","verify","export-usb","seal-can"));p.add_argument("--database");p.add_argument("--root",required=True);p.add_argument("--key",required=True);p.add_argument("--can-root");p.add_argument("--retention-days",type=int,default=180);p.add_argument("--hours",type=int,default=2);p.add_argument("--usb");a=p.parse_args();root=Path(a.root);root.mkdir(parents=True,exist_ok=True);key=Path(a.key).read_bytes()
    if a.command=="seal-can":
        if not a.can_root:p.error("--can-root is required for seal-can")
        payload=seal_can(root,Path(a.can_root),key);print(canonical({"manifest":"SIGNED",**payload["summary"]}));return
    if not a.database:p.error("--database is required")
    db=sqlite3.connect(a.database)
    if a.command=="maintain": maintain(db,root,key,a.retention_days)
    elif a.command=="incident": print(incident(db,root,key,a.hours))
    elif a.command=="verify":
        packages=list((root/"archives").glob("*.gz"))+list((root/"incidents").glob("*.gz"));manifest=root/"can-segment-manifest.json";checked=packages+([manifest] if manifest.exists() else []);valid=[verify(path,key) for path in checked];print(canonical({"verified":sum(valid),"total":len(checked),"packages":len(packages),"can_manifest":"VALID" if manifest.exists() and verify(manifest,key) else "MISSING" if not manifest.exists() else "INVALID"}));raise SystemExit(0 if all(valid) else 2)
    elif not a.usb: p.error("--usb is required for export-usb")
    else: print(export_usb(root,Path(a.usb),key))
if __name__=="__main__": main()
