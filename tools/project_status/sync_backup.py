"""Fast-forward-only GitHub -> serverha -> NAS, with bundle and isolated restore."""
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import fcntl

REPO = Path('/home/sfuso/git/efis-next-professional.git')
STATE = Path('/home/sfuso/.local/state/efis-git-backup')
GH = 'git@github.com:sfuso68/efis-next-professional.git'
NAS = 'sfuso@192.168.1.22:/volume1/homes/sfuso/git/efis-next-professional.git'
NAS_READ = 'efis-nas-restore:/volume1/homes/sfuso/git/efis-next-professional.git'
GH_SSH = 'ssh -F /dev/null -i /home/sfuso/.ssh/avionix_dashboard_github -o IdentitiesOnly=yes -o BatchMode=yes -o StrictHostKeyChecking=yes -o UserKnownHostsFile=/home/sfuso/.local/lib/avionix-dashboard/tools/project_status/github_dashboard_known_hosts'
NAS_SSH = 'ssh -F /home/sfuso/.ssh/efis_nas_replica_config'
RESTORE_SSH = 'ssh -F /home/sfuso/.ssh/efis_nas_restore_config'


def run(args, ssh=None, check=True):
    env = os.environ.copy()
    if ssh:
        env['GIT_SSH_COMMAND'] = ssh
    return subprocess.run(args, env=env, check=check, capture_output=True, timeout=300)


def git(*args, ssh=None, repo=REPO):
    return run(['git', '-C', str(repo), *args], ssh).stdout.decode().strip()


def refs(remote, ssh):
    return dict((ref, oid) for oid, ref in
                (line.split() for line in git('ls-remote', '--refs', remote, ssh=ssh).splitlines()))


def main():
    STATE.mkdir(parents=True, exist_ok=True)
    with (STATE / 'lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
        report = {'timestamp_utc': stamp, 'status': 'FAIL', 'flight_approval': False}
        try:
            before = git('for-each-ref', '--format=%(objectname) %(refname)')
            nas_before = refs(NAS_READ, RESTORE_SSH)
            github = refs(GH, GH_SSH)
            latest = STATE / 'latest.json'
            previous = json.loads(latest.read_text()) if latest.exists() else {}
            if (previous.get('status') == 'PASS' and previous.get('main') == github['refs/heads/main']
                    and git('rev-parse', 'refs/heads/main') == previous['main']
                    and previous.get('nas_refs_after') == nas_before
                    and previous.get('github_refs') == github):
                previous.update(checked_utc=stamp, no_change=True)
                pending = STATE / 'latest.pending'
                pending.write_text(json.dumps(previous, indent=2) + '\n')
                pending.replace(latest)
                print('PASS: refs unchanged; prior restore evidence retained')
                return 0
            # Preserve all preexisting server and NAS branches/tags before modifying main.
            archive = f'refs/backup/{stamp}'
            for ref, oid in nas_before.items():
                if ref.startswith(('refs/heads/', 'refs/tags/')):
                    git('fetch', '--no-tags', NAS_READ, f'{ref}:{archive}/nas/{ref[5:]}', ssh=RESTORE_SSH)
            bundle = STATE / (stamp + '-before.bundle')
            git('bundle', 'create', str(bundle), '--all')
            git('bundle', 'verify', str(bundle))
            report.update(server_refs_before=before, nas_refs_before=nas_before,
                          bundle=str(bundle), bundle_sha256=hashlib.sha256(bundle.read_bytes()).hexdigest())
            target = github['refs/heads/main']
            git('fetch', '--no-tags', GH, f'refs/heads/main:refs/incoming/{stamp}/main', ssh=GH_SSH)
            old = git('rev-parse', 'refs/heads/main')
            git('merge-base', '--is-ancestor', old, target)
            git('merge-base', '--is-ancestor', nas_before['refs/heads/main'], target)
            tag_specs = []
            for ref, oid in github.items():
                if not ref.startswith('refs/tags/'):
                    continue
                if ref in nas_before and nas_before[ref] != oid:
                    raise RuntimeError('NAS tag conflict: ' + ref)
                current = run(['git', '-C', str(REPO), 'rev-parse', '--verify', ref], check=False)
                if current.returncode == 0 and current.stdout.decode().strip() != oid:
                    raise RuntimeError('Server tag conflict: ' + ref)
                git('fetch', '--no-tags', GH, f'{ref}:refs/incoming/{stamp}/tags/{ref[10:]}', ssh=GH_SSH)
                tag_specs.append((ref, oid))
            # CAS prevents concurrent writers replacing the observed main.
            git('update-ref', 'refs/heads/main', target, old)
            for ref, oid in tag_specs:
                if run(['git', '-C', str(REPO), 'show-ref', '--verify', '--quiet', ref], check=False).returncode:
                    git('update-ref', ref, oid, '0' * 40)
            # No deletion, force, mirror, or modification of other NAS branches.
            specs = ['refs/heads/main:refs/heads/main'] + [f'{r}:{r}' for r, _ in tag_specs]
            git('push', NAS, *specs, ssh=NAS_SSH)
            after = refs(NAS_READ, RESTORE_SSH)
            if after['refs/heads/main'] != target:
                raise RuntimeError('NAS main mismatch')
            for ref, oid in nas_before.items():
                if ref != 'refs/heads/main' and after.get(ref) != oid:
                    raise RuntimeError('Preexisting NAS ref changed: ' + ref)
            with tempfile.TemporaryDirectory(prefix='efis-restore-', dir=STATE) as tmp:
                restore = Path(tmp) / 'restore.git'
                run(['git', 'clone', '--bare', NAS_READ, str(restore)], RESTORE_SSH)
                git('fsck', '--full', repo=restore)
                if git('rev-parse', 'refs/heads/main', repo=restore) != target:
                    raise RuntimeError('Restore main mismatch')
                hashes = {}
                for name in ['docs/status/EFIS_NEXT_MASTER_MATRIX.md', 'docs/status/EFIS_NEXT_STATE.md', 'CMakeLists.txt']:
                    original = run(['git', '-C', str(REPO), 'show', target + ':' + name]).stdout
                    recovered = run(['git', '-C', str(restore), 'show', target + ':' + name]).stdout
                    if original != recovered:
                        raise RuntimeError('Restore file mismatch: ' + name)
                    hashes[name] = hashlib.sha256(recovered).hexdigest()
            report.update(status='PASS', main=target, restore='PASS', restored_file_sha256=hashes,
                          preexisting_nas_refs_preserved=True, nas_refs_after=after, github_refs=github)
        except Exception as exc:
            report['error'] = (exc.stderr.decode(errors='replace') if isinstance(exc, subprocess.CalledProcessError) else str(exc))
        path = STATE / (stamp + '.json')
        path.write_text(json.dumps(report, indent=2) + '\n')
        pending = STATE / 'latest.pending'
        pending.write_text(json.dumps(report, indent=2) + '\n')
        pending.replace(STATE / 'latest.json')
        print(json.dumps(report))
        return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
