"""Read-only backup sizing. Does not select or delete retention candidates."""
import argparse
import json
from pathlib import Path
import shutil


def inventory(directory):
    directory = Path(directory).resolve(strict=True)
    bundles = []
    ignored = []
    for item in sorted(directory.iterdir()):
        if item.is_symlink():
            ignored.append(item.name)
        elif item.is_file() and item.suffix == '.bundle':
            stat = item.stat()
            bundles.append(dict(name=item.name, bytes=stat.st_size))
    total = sum(b['bytes'] for b in bundles)
    return dict(schema='avionix-next.backup-storage-inventory.v1',
                bundles=bundles, count=len(bundles), bundle_bytes=total,
                filesystem_free_bytes=shutil.disk_usage(directory).free,
                ignored_symlinks=ignored, retention_approved=False,
                deletion_candidates=[], modified_backup_files=False,
                note='Sizing only; file presence does not prove restore verification.')


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('directory', type=Path)
    print(json.dumps(inventory(p.parse_args().directory), indent=2))
