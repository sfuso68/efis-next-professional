"""Local-only read-only viewer and scheduled renderer of the server Git mirror."""
import argparse
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import os
from pathlib import Path
import subprocess
import tempfile
from tools.project_status.generate_dashboard import build

ROOT = Path.home() / '.local/state/avionix-dashboard'
BACKUP_STATUS = Path.home() / '.local/state/efis-git-backup/latest.json'
REPO = ROOT / 'github.git'
REMOTE = 'git@github.com:sfuso68/efis-next-professional.git'


def git(*args):
    env = os.environ.copy()
    env['GIT_SSH_COMMAND'] = ('ssh -F /dev/null -i /home/sfuso/.ssh/avionix_dashboard_github '
                             '-o IdentitiesOnly=yes -o BatchMode=yes '
                             '-o StrictHostKeyChecking=yes -o ConnectTimeout=10 '
                             '-o UserKnownHostsFile=/home/sfuso/.local/lib/avionix-dashboard/tools/project_status/github_dashboard_known_hosts')
    return subprocess.check_output(['git', '-C', str(REPO), *args], env=env,
                                   timeout=120, stderr=subprocess.PIPE).decode('utf-8').strip()


def refresh():
    ROOT.mkdir(parents=True, exist_ok=True)
    now = datetime.now(timezone.utc).isoformat()
    try:
        if not REPO.exists():
            subprocess.run(['git', 'init', '--bare', str(REPO)], check=True,
                           capture_output=True, timeout=15)
        git('fetch', '--no-tags', REMOTE, 'refs/heads/main:refs/heads/main')
        commit = git('rev-parse', 'refs/heads/main')
        commit_date = git('show', '-s', '--format=%cI', commit)
        matrix = git('show', commit + ':docs/status/EFIS_NEXT_MASTER_MATRIX.md')
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / 'matrix.md'
            p.write_text(matrix + '\n', encoding='utf-8')
            html = build(p)
        pending = ROOT / 'index.pending'
        pending.write_text(html, encoding='utf-8')
        pending.replace(ROOT / 'index.html')
        status = dict(status='GITHUB_SYNC_OK', checked_utc=now,
                      commit=commit, commit_date=commit_date,
                      source=REMOTE, github_verified=True,
                      note='Main GitHub verificato; solo stato documentale. Non prova stato hardware.')
    except Exception as exc:
        detail = exc.stderr.decode('utf-8', errors='replace') if isinstance(exc, subprocess.CalledProcessError) and exc.stderr else str(exc)
        status = dict(status='ERROR', checked_utc=now, error=detail)
    pending = ROOT / 'status.pending'
    pending.write_text(json.dumps(status), encoding='utf-8')
    pending.replace(ROOT / 'status.json')
    if status['status'] == 'ERROR':
        raise RuntimeError(status['error'])


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/release.json':
            manifest = Path(__file__).resolve().parents[2] / 'manifest.json'
            identity = json.loads(manifest.read_text()).get('package_id') if manifest.exists() else 'legacy'
            body = json.dumps({'package_id': identity}).encode()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            self.wfile.write(body)
            return
        if self.path == '/backup-status.json':
            path = BACKUP_STATUS
            if not path.exists():
                self.send_error(503)
                return
            try:
                raw = json.loads(path.read_text())
                if not isinstance(raw, dict):
                    raise ValueError('Expected object')
            except (OSError, ValueError):
                self.send_error(503, 'Backup status unavailable')
                return
            fields = ['status', 'timestamp_utc', 'checked_utc', 'main', 'restore',
                      'no_change', 'preexisting_nas_refs_preserved']
            body = json.dumps({k: raw[k] for k in fields if k in raw}).encode()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            self.wfile.write(body)
            return
        names = {'/': 'index.html', '/status.json': 'status.json'}
        name = names.get(self.path)
        if not name or not (ROOT / name).exists():
            self.send_error(404)
            return
        body = (ROOT / name).read_bytes()
        if name == 'index.html':
            monitor = '''<aside id="monitor" style="background:#593b12;padding:16px">Verifica aggiornamento…</aside><script>
async function health(){try{const r=await fetch('/status.json',{cache:'no-store'});if(!r.ok)throw Error(r.status);const s=await r.json(),age=(Date.now()-Date.parse(s.checked_utc))/60000;document.getElementById('monitor').textContent=(age>15?'MONITOR OBSOLETO — ':'')+s.status+' • verifica '+s.checked_utc+' • commit '+(s.commit||'?')+' del '+(s.commit_date||'?')+' • '+(s.note||s.error||'');}catch(e){document.getElementById('monitor').textContent='MONITOR NON DISPONIBILE: '+e;}}health();setInterval(health,60000);setTimeout(()=>location.reload(),300000);</script>'''
            backup = '''<aside id="backup" style="background:#213249;padding:16px">Backup: verifica…</aside><script>
async function backupHealth(){try{const r=await fetch('/backup-status.json',{cache:'no-store'});if(!r.ok)throw Error(r.status);const s=await r.json(),t=s.checked_utc||s.timestamp_utc;const iso=t&&t.includes('T')&&!t.includes('-')?t.slice(0,4)+'-'+t.slice(4,6)+'-'+t.slice(6,8)+'T'+t.slice(9,11)+':'+t.slice(11,13)+':'+t.slice(13,15)+'Z':t;const stale=!iso||!Number.isFinite(Date.parse(iso))||(Date.now()-Date.parse(iso))>45*60000;document.getElementById('backup').textContent='BACKUP '+s.status+(stale?' — OBSOLETO':'')+' • controllo '+t+' • restore '+s.restore+' del '+s.timestamp_utc+' • main '+s.main;}catch(e){document.getElementById('backup').textContent='BACKUP NON VERIFICABILE: '+e;}}backupHealth();setInterval(backupHealth,60000);</script>'''
            body = body.replace(b'<h1>', (monitor + backup).encode('utf-8') + b'<h1>', 1)
        self.send_response(200)
        self.send_header('Content-Type', 'application/json' if name.endswith('json') else 'text/html; charset=utf-8')
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.end_headers()
        self.wfile.write(body)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--refresh', action='store_true')
    args = parser.parse_args()
    if args.refresh:
        refresh()
    else:
        HTTPServer(('127.0.0.1', 8766), Handler).serve_forever()
