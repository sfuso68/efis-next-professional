#!/usr/bin/env python3
"""Authenticated, deduplicating collector for EFIS fuel blackbox events."""
import argparse, hashlib, hmac, json, os, sqlite3, subprocess, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

def canonical(value): return json.dumps(value, sort_keys=True, separators=(",", ":"))

class Hub:
    def __init__(self, database, tokens, maintenance_script, root, key, export_root):
        self.tokens=tokens; self.lock=threading.Lock(); Path(database).parent.mkdir(parents=True,exist_ok=True)
        self.db=sqlite3.connect(database,check_same_thread=False)
        self.db.execute("PRAGMA journal_mode=WAL")
        self.db.execute("CREATE TABLE IF NOT EXISTS events(id TEXT PRIMARY KEY,source TEXT NOT NULL,received_utc TEXT NOT NULL,event_utc TEXT,state TEXT,payload TEXT NOT NULL)")
        self.db.execute("CREATE TABLE IF NOT EXISTS maintenance_requests(source TEXT NOT NULL,request_id TEXT NOT NULL,action TEXT NOT NULL,state TEXT NOT NULL,updated_utc TEXT NOT NULL,result TEXT NOT NULL,PRIMARY KEY(source,request_id))")
        self.db.commit()
        self.database=database;self.maintenance_script=maintenance_script;self.root=root;self.key=key;self.export_root=export_root;self.maintenance_lock=threading.Lock();self.archive_lock=threading.Lock();self.archive_cache={};self.last_maintenance=self.load_last_maintenance()
    def authorized(self,source,token): return source in self.tokens and hmac.compare_digest(self.tokens[source],token)
    def insert(self,source,event):
        digest=hashlib.sha256((source+"\n"+canonical(event)).encode()).hexdigest()
        received=time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime())
        with self.lock: cur=self.db.execute("INSERT OR IGNORE INTO events VALUES(?,?,?,?,?,?)",(digest,source,received,event.get("utc"),event.get("state"),canonical(event)));self.db.commit()
        return digest,cur.rowcount==1
    def recent(self,limit):
        with self.lock: rows=self.db.execute("SELECT id,source,received_utc,payload FROM events ORDER BY rowid DESC LIMIT ?",(limit,)).fetchall()
        return [{"id":r[0],"source":r[1],"received_utc":r[2],"event":json.loads(r[3])} for r in rows]
    def count(self):
        with self.lock: return self.db.execute("SELECT COUNT(*) FROM events").fetchone()[0]
    def summary(self):
        with self.lock: rows=self.db.execute("SELECT source,COUNT(*),MAX(received_utc) FROM events GROUP BY source ORDER BY source").fetchall()
        return [{"source":r[0],"records":r[1],"last_contact_utc":r[2]} for r in rows]
    def reader_authorized(self,token): return any(hmac.compare_digest(value,token) for value in self.tokens.values())
    def archive_inventory(self):
        with self.archive_lock:
            key=Path(self.key).read_bytes();items=[];seen=set()
            for kind,folder in (("ARCHIVE","archives"),("INCIDENT","incidents")):
                for path in (Path(self.root)/folder).glob("*.gz"):
                    name=str(path);seen.add(name);stat=path.stat();fingerprint=(stat.st_size,stat.st_mtime_ns)
                    if name in self.archive_cache and self.archive_cache[name][0]==fingerprint: item=self.archive_cache[name][1]
                    else:
                        sha=path.with_suffix(path.suffix+".sha256");mac=path.with_suffix(path.suffix+".hmac");status="MISSING"
                        if sha.exists() and mac.exists():
                            content=path.read_bytes();valid=hmac.compare_digest(sha.read_text().split()[0],hashlib.sha256(content).hexdigest()) and hmac.compare_digest(mac.read_text().split()[0],hmac.new(key,content,hashlib.sha256).hexdigest());status="VALID" if valid else "INVALID"
                        item={"name":path.name,"kind":kind,"bytes":stat.st_size,"modified_utc":time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime(stat.st_mtime)),"signature":status};self.archive_cache[name]=(fingerprint,item)
                    items.append(item)
            self.archive_cache={name:value for name,value in self.archive_cache.items() if name in seen}
            return sorted(items,key=lambda item:item["modified_utc"],reverse=True)
    def load_last_maintenance(self):
        row=self.db.execute("SELECT source,request_id,action,state,updated_utc,result FROM maintenance_requests ORDER BY rowid DESC LIMIT 1").fetchone()
        return {"state":"READY"} if not row else {"source":row[0],"request_id":row[1],"action":row[2],"state":row[3],"utc":row[4],"result":row[5]}
    def save_maintenance(self,source,request_id,action,state,result):
        value={"source":source,"request_id":request_id,"action":action,"state":state,"utc":time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime()),"result":result}
        with self.lock:
            self.db.execute("INSERT OR REPLACE INTO maintenance_requests VALUES(?,?,?,?,?,?)",(source,request_id,action,state,value["utc"],result));self.db.commit()
        self.last_maintenance=value;return value
    def run_maintenance(self,source,action,request_id):
        if not request_id: raise ValueError("request_id_required")
        with self.maintenance_lock:
            with self.lock: previous=self.db.execute("SELECT action,state,updated_utc,result FROM maintenance_requests WHERE source=? AND request_id=?",(source,request_id)).fetchone()
            if previous and previous[0]!=action: raise ValueError("request_id_action_mismatch")
            if previous and previous[1]=="COMPLETE":
                self.last_maintenance={"source":source,"request_id":request_id,"action":action,"state":"COMPLETE","utc":previous[2],"result":previous[3],"replayed":True};return self.last_maintenance
            commands={"CREATE_INCIDENT":["incident","--hours","2"],"VERIFY_SIGNATURES":["verify"]}
            if action=="EXPORT_USB":
                if not self.export_root or not os.path.ismount(self.export_root):
                    self.save_maintenance(source,request_id,action,"WAITING","USB_NOT_MOUNTED");raise RuntimeError("USB_NOT_MOUNTED")
                command=["export-usb","--usb",self.export_root]
            elif action in commands: command=commands[action]
            else: raise ValueError("unsupported_action")
            self.save_maintenance(source,request_id,action,"RUNNING","")
            args=["/usr/bin/python3",self.maintenance_script,*command,"--database",self.database,"--root",self.root,"--key",self.key]
            try: result=subprocess.run(args,capture_output=True,text=True,timeout=60,check=True)
            except Exception as exc:
                self.save_maintenance(source,request_id,action,"FAILED",str(exc));raise
            return self.save_maintenance(source,request_id,action,"COMPLETE",result.stdout.strip())

def handler(hub):
    class Handler(BaseHTTPRequestHandler):
        def reply(self,status,data):
            body=canonical(data).encode();self.send_response(status);self.send_header("Content-Type","application/json");self.send_header("Content-Length",str(len(body)));self.end_headers();self.wfile.write(body)
        def do_GET(self):
            parsed=urlparse(self.path)
            if parsed.path=="/health": return self.reply(200,{"ok":True,"records":hub.count(),"nodes":hub.summary(),"maintenance":hub.last_maintenance})
            if parsed.path=="/v1/events":
                token=self.headers.get("Authorization","").removeprefix("Bearer ")
                if not hub.reader_authorized(token): return self.reply(401,{"error":"unauthorized"})
                return self.reply(200,{"events":hub.recent(min(200,max(1,int(parse_qs(parsed.query).get("limit",[50])[0]))))})
            if parsed.path=="/v1/archives":
                token=self.headers.get("Authorization","").removeprefix("Bearer ")
                if not hub.reader_authorized(token): return self.reply(401,{"error":"unauthorized"})
                return self.reply(200,{"archives":hub.archive_inventory()})
            self.reply(404,{"error":"not_found"})
        def do_POST(self):
            if self.path not in ("/v1/events","/v1/maintenance"): return self.reply(404,{"error":"not_found"})
            try:
                data=json.loads(self.rfile.read(min(int(self.headers.get("Content-Length","0")),65536)))
                source=str(data["source"]); token=self.headers.get("Authorization","").removeprefix("Bearer ")
                if not hub.authorized(source,token): return self.reply(401,{"error":"unauthorized"})
                if self.path=="/v1/maintenance": return self.reply(200,hub.run_maintenance(source,str(data["action"]),str(data.get("request_id",""))))
                digest,inserted=hub.insert(source,data["event"]);self.reply(201 if inserted else 200,{"id":digest,"inserted":inserted})
            except Exception as exc: self.reply(400,{"error":"bad_request","detail":str(exc)})
        def log_message(self,fmt,*args): pass
    return Handler

def main():
    p=argparse.ArgumentParser();p.add_argument("--bind",default="0.0.0.0");p.add_argument("--port",type=int,default=18093);p.add_argument("--database",required=True);p.add_argument("--tokens",required=True);p.add_argument("--maintenance-script",required=True);p.add_argument("--root",required=True);p.add_argument("--key",required=True);p.add_argument("--export-root",default="/media/blackbox/EFIS_EXPORT");a=p.parse_args()
    tokens=json.loads(Path(a.tokens).read_text());hub=Hub(a.database,tokens,a.maintenance_script,a.root,a.key,a.export_root);ThreadingHTTPServer((a.bind,a.port),handler(hub)).serve_forever()
if __name__=="__main__": main()
