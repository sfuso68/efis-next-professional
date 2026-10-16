"""Transactional systemd-user activation of a verified monitor/backup package."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import time
import urllib.request
from tools.project_status.package_dashboard import verify

SERVICES = ('avionix-dashboard.service', 'avionix-dashboard-refresh.service',
            'efis-git-nas-sync.service')
TIMERS = ('avionix-dashboard-refresh.timer', 'efis-git-nas-sync.timer')


def control(*args):
    subprocess.run(['systemctl', '--user', *args], check=True, timeout=300,
                   capture_output=True)


def health(expected):
    last = None
    for _ in range(30):
        try:
            if expected is not None:
                with urllib.request.urlopen('http://127.0.0.1:8766/release.json', timeout=2) as r:
                    if json.load(r).get('package_id') != expected:
                        raise ValueError('Wrong running release')
            with urllib.request.urlopen('http://127.0.0.1:8766/status.json', timeout=2) as r:
                if json.load(r).get('status') != 'GITHUB_SYNC_OK':
                    raise ValueError('Refresh not healthy')
            return
        except Exception as exc:
            last = exc
            time.sleep(0.2)
    raise RuntimeError('Activation health failed: ' + str(last))


def activate(release, home, runner=control, probe=health, drill=False):
    release = Path(release).resolve(strict=True)
    home = Path(home).resolve(strict=True)
    if release.parent != home / '.local/lib/avionix-dashboard-releases':
        raise ValueError('Release outside managed directory')
    manifest = verify(release)
    if release.name != manifest['package_id']:
        raise ValueError('Release directory identity mismatch')
    paths = {s: home / '.config/systemd/user' / (s + '.d') / '80-avionix-release.conf' for s in SERVICES}
    originals = {s: p.read_bytes() if p.exists() else None for s, p in paths.items()}
    previous_id = None
    if originals[SERVICES[0]] is not None:
        for line in originals[SERVICES[0]].decode().splitlines():
            if line.startswith('WorkingDirectory='):
                old_manifest = Path(line.split('=', 1)[1]) / 'manifest.json'
                if old_manifest.exists():
                    previous_id = json.loads(old_manifest.read_text())['package_id']
    record = {'release': manifest['package_id'], 'utc': datetime.now(timezone.utc).isoformat()}
    reports = home / '.local/state/avionix-dashboard/activations'
    reports.mkdir(parents=True, exist_ok=True)
    # Persist the exact pre-change drop-ins before touching the services.
    snapshot = dict(record, previous={s: b.decode() if b is not None else None for s, b in originals.items()})
    (reports / (str(time.time_ns()) + '-before.json')).write_text(json.dumps(snapshot, indent=2))
    try:
        runner('stop', *TIMERS)
        runner('stop', SERVICES[1], SERVICES[2])
        for service, path in paths.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            module = ('tools.project_status.sync_backup' if service == SERVICES[2]
                      else 'tools.project_status.serve_dashboard')
            suffix = ' --refresh' if service == SERVICES[1] else ''
            text = ('[Service]\nWorkingDirectory=' + str(release) +
                    '\nEnvironment=PYTHONDONTWRITEBYTECODE=1\nExecStart=\nExecStart=/usr/bin/python3 -B -m ' + module + suffix + '\n')
            pending = path.with_suffix('.pending')
            pending.write_text(text)
            pending.replace(path)
        runner('daemon-reload')
        runner('start', SERVICES[1], SERVICES[2])
        runner('restart', SERVICES[0])
        probe(manifest['package_id'])
        if drill:
            raise RuntimeError('Intentional rollback drill')
        runner('start', *TIMERS)
        record['status'] = 'ACTIVE'
    except Exception as exc:
        for service, path in paths.items():
            if originals[service] is None:
                path.unlink(missing_ok=True)
            else:
                pending = path.with_suffix('.pending')
                pending.write_bytes(originals[service])
                pending.replace(path)
        runner('daemon-reload')
        runner('start', SERVICES[1], SERVICES[2])
        runner('restart', SERVICES[0])
        runner('start', *TIMERS)
        runner('is-active', SERVICES[0], *TIMERS)
        probe(previous_id)
        record.update(status='ROLLED_BACK', reason=str(exc), rollback_services_verified=True,
                      restored_release=previous_id or 'legacy')
    (reports / (str(time.time_ns()) + '-result.json')).write_text(json.dumps(record, indent=2))
    return record


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('release', type=Path)
    p.add_argument('--home', type=Path, default=Path.home())
    p.add_argument('--rollback-drill', action='store_true')
    args = p.parse_args()
    import fcntl
    lockpath = args.home / '.local/state/avionix-dashboard/activation.lock'
    lockpath.parent.mkdir(parents=True, exist_ok=True)
    with lockpath.open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        result = activate(args.release, args.home, drill=args.rollback_drill)
        print(json.dumps(result))
        drill_pass = args.rollback_drill and result.get('reason') == 'Intentional rollback drill'
        raise SystemExit(0 if result['status'] == 'ACTIVE' or drill_pass else 1)
