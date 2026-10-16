from pathlib import Path
import tempfile
import unittest
from tools.project_status.package_dashboard import build, install, FILES
from tools.project_status.activate_dashboard import activate, SERVICES


class ActivationTests(unittest.TestCase):
    def fixture(self, home):
        source = home / 'source'
        for name in FILES:
            p = source / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text('fixture')
        build(source, home / 'package')
        return install(home / 'package', home / '.local/lib/avionix-dashboard-releases')

    def test_activation_and_rollback_restore_exact_dropins(self):
        with tempfile.TemporaryDirectory() as tmp:
            home = Path(tmp).resolve()
            release = self.fixture(home)
            commands = []
            run = lambda *args: commands.append(args)
            self.assertEqual('ACTIVE', activate(release, home, run, lambda _: None)['status'])
            paths = [home / '.config/systemd/user' / (s + '.d') / '80-avionix-release.conf' for s in SERVICES]
            originals = [p.read_bytes() for p in paths]
            attempts = []
            def fail(_):
                attempts.append(1)
                if len(attempts) == 1:
                    raise RuntimeError('fixture health failure')
            result = activate(release, home, run, fail)
            self.assertEqual('ROLLED_BACK', result['status'])
            self.assertEqual(originals, [p.read_bytes() for p in paths])
            self.assertIn(('daemon-reload',), commands)

    def test_outside_release_rejected_before_mutation(self):
        with tempfile.TemporaryDirectory() as tmp:
            home = Path(tmp).resolve()
            with self.assertRaises(ValueError):
                activate(home, home, lambda *_: self.fail('Unexpected service mutation'))

    def test_first_activation_failure_removes_only_created_dropins(self):
        with tempfile.TemporaryDirectory() as tmp:
            home = Path(tmp).resolve()
            release = self.fixture(home)
            result = activate(release, home, lambda *_: None, lambda _: None, drill=True)
            self.assertEqual('ROLLED_BACK', result['status'])
            self.assertFalse(any(home.glob('.config/systemd/user/*/80-avionix-release.conf')))
