#!/usr/bin/env python3
"""Reliably forwards a local append-only JSONL journal to the blackbox hub."""
import argparse, json, os, time, urllib.error, urllib.request
from pathlib import Path

def post(url,source,token,event):
    body=json.dumps({"source":source,"event":event},separators=(",",":")).encode()
    req=urllib.request.Request(url+"/v1/events",body,{"Content-Type":"application/json","Authorization":"Bearer "+token},method="POST")
    with urllib.request.urlopen(req,timeout=5) as response: return response.status in (200,201)

def refresh_cache(url,token,path):
    headers={"Authorization":"Bearer "+token}; result={"fetched_utc":time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime())}
    for key,endpoint in (("health","/health"),("events","/v1/events?limit=50"),("archives","/v1/archives")):
        req=urllib.request.Request(url+endpoint,headers=headers)
        with urllib.request.urlopen(req,timeout=5) as response: result[key]=json.load(response)
    target=Path(path);target.parent.mkdir(parents=True,exist_ok=True);tmp=target.with_suffix(".tmp");tmp.write_text(json.dumps(result,separators=(",",":")));os.replace(tmp,target)
def send_command(url,source,token,path):
    target=Path(path)
    if not target.exists() or not target.stat().st_size:return
    lines=[line for line in target.read_text().splitlines() if line.strip()]
    if not lines:return
    request=json.loads(lines[0]);body=json.dumps({"source":source,**request},separators=(",",":")).encode();req=urllib.request.Request(url+"/v1/maintenance",body,{"Content-Type":"application/json","Authorization":"Bearer "+token},method="POST")
    with urllib.request.urlopen(req,timeout=65): pass
    tmp=target.with_suffix(".tmp");tmp.write_text(("\n".join(lines[1:])+"\n") if len(lines)>1 else "");os.replace(tmp,target)

def main():
    p=argparse.ArgumentParser();p.add_argument("--journal",required=True);p.add_argument("--state",required=True);p.add_argument("--url",required=True);p.add_argument("--source",required=True);p.add_argument("--token-file",required=True);p.add_argument("--cache");p.add_argument("--commands");p.add_argument("--once",action="store_true");a=p.parse_args()
    journal=Path(a.journal); state=Path(a.state);state.parent.mkdir(parents=True,exist_ok=True);offset=int(state.read_text() or 0) if state.exists() else 0;token=Path(a.token_file).read_text().strip()
    while True:
        try:
            if journal.exists():
                with journal.open("rb") as stream:
                    if offset>journal.stat().st_size: offset=0
                    stream.seek(offset)
                    while True:
                        start=stream.tell();line=stream.readline()
                        if not line: break
                        event=json.loads(line);post(a.url,a.source,token,event);offset=stream.tell();tmp=state.with_suffix(".tmp");tmp.write_text(str(offset));os.replace(tmp,state)
            if a.cache: refresh_cache(a.url,token,a.cache)
            if a.commands: send_command(a.url,a.source,token,a.commands)
        except (OSError,ValueError,urllib.error.URLError) as exc: print("forwarder_error",type(exc).__name__,str(exc),flush=True)
        if a.once:return
        time.sleep(2)
if __name__=="__main__": main()
