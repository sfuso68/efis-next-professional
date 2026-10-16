"""Deterministic manifest package; verified immutable installation, no service mutation."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile

FILES = tuple('tools/project_status/' + name for name in (
    'generate_status.py', 'generate_dashboard.py', 'serve_dashboard.py',
    'sync_backup.py', 'verify_backup_bundle.py', 'github_dashboard_known_hosts',
    'install_dashboard.sh', 'efis-git-sync-nas', 'package_dashboard.py'))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(source, target):
    if target.exists():
        raise ValueError('Package destination must not exist')
    target.mkdir(parents=True)
    for name in FILES:
        src = source / name
        if src.is_symlink() or not src.is_file():
            raise ValueError('Invalid source: ' + name)
        dest = target / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        # Normalize text for reproducibility across Git Windows/Linux checkouts.
        dest.write_bytes(src.read_text(encoding='utf-8').replace('\r\n', '\n').encode())
    hashes = {name: digest(target / name) for name in FILES}
    identity = hashlib.sha256(json.dumps(hashes, sort_keys=True).encode()).hexdigest()
    manifest = dict(schema='avionix-dashboard-package.v1', package_id=identity, files=hashes)
    (target / 'manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    return verify(target)


def verify(package):
    if package.is_symlink():
        raise ValueError('Package symlink rejected')
    manifest_path = package / 'manifest.json'
    if manifest_path.is_symlink():
        raise ValueError('Manifest symlink rejected')
    manifest = json.loads(manifest_path.read_text())
    if manifest.get('schema') != 'avionix-dashboard-package.v1' or set(manifest['files']) != set(FILES):
        raise ValueError('Unexpected manifest schema/files')
    actual = set()
    for p in package.rglob('*'):
        if p.is_symlink():
            raise ValueError('Package symlink rejected')
        if p.is_file():
            actual.add(p.relative_to(package).as_posix())
    if actual != set(FILES) | {'manifest.json'}:
        raise ValueError('Unexpected package content')
    for name, expected in manifest['files'].items():
        if digest(package / name) != expected:
            raise ValueError('Digest mismatch: ' + name)
    identity = hashlib.sha256(json.dumps(manifest['files'], sort_keys=True).encode()).hexdigest()
    if manifest.get('package_id') != identity:
        raise ValueError('Package identity mismatch')
    return manifest


def install(package, releases):
    manifest = verify(package)
    releases.mkdir(parents=True, exist_ok=True)
    if releases.is_symlink():
        raise ValueError('Release root symlink rejected')
    target = releases / manifest['package_id']
    if target.exists():
        if verify(target) != manifest:
            raise ValueError('Existing release differs')
        return target
    with tempfile.TemporaryDirectory(prefix='.install-', dir=releases) as tmp:
        staged = Path(tmp) / 'release'
        shutil.copytree(package, staged)
        verify(staged)
        staged.rename(target)
    return target


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=['build', 'verify', 'install'])
    parser.add_argument('package', type=Path)
    parser.add_argument('--source', type=Path, default=Path('.'))
    parser.add_argument('--releases', type=Path)
    args = parser.parse_args()
    if args.action == 'build':
        print(json.dumps(build(args.source, args.package)))
    elif args.action == 'verify':
        print(json.dumps(verify(args.package)))
    else:
        if args.releases is None:
            parser.error('--releases required')
        print(install(args.package, args.releases))
