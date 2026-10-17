"""Repeatable offline gates A4/A6/A7. Never touches devices or promotes status."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import sys

GROUPS = {
    'A4-contracts': ['tests.test_avionix_icd', 'tests.test_avionix_data_contract',
                     'tests.test_g1_g2_readiness', 'tests.test_g2_vertical_acceptance'],
    'A6-profiles': ['tests.test_product_contracts_batch', 'tests.test_product_tools_batch',
                    'tests.test_product_review_gates', 'tests.test_product_evidence_review',
                    'tests.test_product_implementation_plan'],
    'A7-hmi': ['tests.test_avionix_hmi_contract', 'tests.test_skyview_responsive',
               'tests.test_ems_state_presentation', 'tests.test_ems_responsive_state_matrix',
               'tests.test_hmi_golden_tools'],
    'project-control': ['tests.test_project_status_control', 'tests.test_project_dashboard',
                         'tests.test_dashboard_refresh', 'tests.test_backup_guards',
                         'tests.test_dashboard_package', 'tests.test_dashboard_activation'],
}
NATIVE_REGEX = '^efis_(freshness_budget|arbitration|ddp_v2|edp_v1|avionix_icd_vector|engine|engine_runtime|engine_runtime_store|operational_profile_model|pfd_aircraft_profile)_tests$'


def execute(command, root, runner=subprocess.run):
    try:
        result = runner(command, cwd=root, capture_output=True, text=True, timeout=180)
        return dict(status='PASS' if result.returncode == 0 else 'FAIL',
                    exit_code=result.returncode, command=command,
                    stdout=result.stdout[-16000:], stderr=result.stderr[-16000:])
    except (OSError, subprocess.TimeoutExpired) as exc:
        return dict(status='BLOCKED', command=command, reason=str(exc))


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--ctest', help='CTest executable; omission reports native gates NOT_RUN')
    p.add_argument('--build-dir', default='build-offline-qt')
    args = p.parse_args()
    root = Path(__file__).resolve().parents[2]
    report = dict(schema='avionix-next.autonomous-verification.v1',
                  utc=datetime.now(timezone.utc).isoformat(), flight_approval=False,
                  hardware_verification='NOT_RUN', groups={})
    git = ['git', '-c', 'safe.directory=' + str(root), '-C', str(root)]
    for label, command in [('head', git + ['rev-parse', 'HEAD']),
                           ('tracked_changes', git + ['diff', '--name-only', 'HEAD'])]:
        result = execute(command, root)
        report[label] = result.get('stdout', '').strip() if result['status'] == 'PASS' else result
    # The embedded Windows Python ignores PYTHONPATH: explicitly inject root.
    for name, modules in GROUPS.items():
        code = ('import sys,unittest,tempfile;sys.path.insert(0,' + repr(str(root)) + ');'
                'tempfile.tempdir=' + repr(str(root.parent)) + ';'
                's=unittest.defaultTestLoader.loadTestsFromNames(' + repr(modules) + ');'
                'r=unittest.TextTestRunner(verbosity=1).run(s);sys.exit(not r.wasSuccessful())')
        report['groups'][name] = execute([sys.executable, '-c', code], root)
        print(name + ': ' + report['groups'][name]['status'], flush=True)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    report['groups']['native'] = (execute([args.ctest, '--test-dir', args.build_dir,
                                           '-R', NATIVE_REGEX, '--timeout', '90', '--output-on-failure'], root)
                                  if args.ctest else dict(status='NOT_RUN'))
    states = [g['status'] for g in report['groups'].values()]
    report['status'] = 'PASS' if all(s == 'PASS' for s in states) else 'INCOMPLETE'
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({name: g['status'] for name, g in report['groups'].items()}))
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
