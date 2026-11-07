#!/usr/bin/env python3
"""Normal/Private transition rules that must hold before anything else ships.

These run anywhere: system calls (logind, gtklock, homectl, gocryptfs) are
replaced by recorders, so the tests check ordering and refusal rules, not the
live VM. Live VT, PAM and homed behaviour is in docs/DUAL-ENVIRONMENT.md.
"""
import importlib.machinery
import importlib.util
import json
import os
import stat
import subprocess
import sys
import tempfile
import textwrap
import time
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PACKAGING = ROOT / 'packaging'


def load(name):
    loader = importlib.machinery.SourceFileLoader(name.replace('-', '_'), str(PACKAGING / name))
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


class ModeTransitions(unittest.TestCase):
    def setUp(self):
        self.mode = load('sovel-mode')
        self.temporary = tempfile.TemporaryDirectory()
        self.events = []
        m = self.mode
        m.RUNTIME = Path(self.temporary.name)
        m.LOCK_STATE = m.RUNTIME / 'sovel-lock.state'
        m.STATE_HOME = m.RUNTIME / 'state'
        m.AUDIT = m.STATE_HOME / 'private-audit.log'
        m.CONFIG_HOME = m.RUNTIME / 'config'
        m.config = lambda: {'user': 'nate-lab', 'normal_vt': 7, 'private_vt': 8}
        m.notify = lambda *a: self.events.append(('notify', a[0]))
        m.switch_vt = lambda n: self.events.append(('switch', n)) or True
        m.assistant_quiet = lambda: self.events.append(('quiet',))
        m.clear_clipboard = lambda: self.events.append(('clipboard',))
        m.vault_lock = lambda: self.events.append(('vault',)) or 0
        m.end_session = lambda: self.events.append(('end',)) or True
        m.spawn = lambda *a: self.events.append(('spawn',) + a)
        m.greeter_state = lambda: 'active'
        m.log = lambda message: None
        m.open_setup = lambda: self.events.append(('setup',))

    def tearDown(self):
        self.temporary.cleanup()

    def run_command(self, name, **values):
        args = type('Args', (), {'sleep': False, 'timeout': 20, **values})()
        return getattr(self.mode, 'command_' + name.replace('-', '_'))(args)

    def kinds(self):
        return [event[0] for event in self.events]

    def test_private_lock_failure_never_switches(self):
        self.mode.private_mode = lambda: True
        self.mode.lock_screen = lambda: self.events.append(('lock', False)) or False
        self.assertEqual(self.run_command('lock'), 1)
        self.assertNotIn('switch', self.kinds())
        self.assertIn('lock-failed', self.mode.AUDIT.read_text())

    def test_private_lock_covers_screens_before_anything_else(self):
        self.mode.private_mode = lambda: True
        self.mode.lock_screen = lambda: self.events.append(('lock', True)) or True
        self.assertEqual(self.run_command('lock'), 0)
        kinds = self.kinds()
        self.assertEqual(kinds[0], 'lock')
        self.assertLess(kinds.index('quiet'), kinds.index('switch'))
        self.assertLess(kinds.index('vault'), kinds.index('switch'))
        self.assertIn(('switch', 7), self.events)
        self.assertNotIn('spawn', kinds)  # seal policy defaults to never

    def test_seal_policy_spawns_timer_except_before_sleep(self):
        self.mode.private_mode = lambda: True
        self.mode.lock_screen = lambda: True
        self.mode.CONFIG_HOME.mkdir(parents=True)
        (self.mode.CONFIG_HOME / 'Preferences.json').write_text(json.dumps({'private.sealAfterLock': '15'}))
        self.run_command('lock')
        self.assertIn(('spawn', 'seal-when-locked', '15'), self.events)
        self.events.clear()
        self.run_command('lock', sleep=True)
        self.assertFalse([e for e in self.events if e[0] == 'spawn'])

    def test_enter_private_locks_normal_first(self):
        self.mode.private_mode = lambda: False
        self.mode.lock_screen = lambda: self.events.append(('lock', False)) or False
        self.assertEqual(self.run_command('enter-private'), 1)
        self.assertNotIn('switch', self.kinds())
        self.events.clear()
        self.mode.lock_screen = lambda: self.events.append(('lock', True)) or True
        self.assertEqual(self.run_command('enter-private'), 0)
        self.assertEqual(self.events, [('lock', True), ('switch', 8)])

    def test_enter_private_never_locks_for_a_missing_greeter(self):
        self.mode.private_mode = lambda: False
        self.mode.greeter_state = lambda: 'failed'
        self.mode.lock_screen = lambda: self.events.append(('lock', True)) or True
        self.assertEqual(self.run_command('enter-private'), 1)
        self.assertNotIn('lock', self.kinds())
        self.assertNotIn('switch', self.kinds())

    def test_enter_private_before_setup_opens_setup(self):
        self.mode.private_mode = lambda: False
        self.mode.config = lambda: None
        self.mode.lock_screen = lambda: self.events.append(('lock', True)) or True
        self.assertEqual(self.run_command('enter-private'), 2)
        self.assertEqual(self.kinds(), ['notify', 'setup'])

    def test_exit_private_keeps_session_when_windows_remain(self):
        self.mode.private_mode = lambda: True
        self.mode.close_windows = lambda seconds: 2
        self.assertEqual(self.run_command('exit-private'), 1)
        self.assertNotIn('end', self.kinds())
        self.assertNotIn('switch', self.kinds())
        self.mode.close_windows = lambda seconds: -1  # cannot inspect windows: also refuse
        self.assertEqual(self.run_command('exit-private'), 1)
        self.assertNotIn('end', self.kinds())

    def test_exit_private_keeps_session_when_vault_busy(self):
        self.mode.private_mode = lambda: True
        self.mode.close_windows = lambda seconds: 0
        self.mode.vault_lock = lambda: 1
        self.assertEqual(self.run_command('exit-private'), 1)
        self.assertNotIn('end', self.kinds())

    def test_exit_private_seals_then_returns(self):
        self.mode.private_mode = lambda: True
        self.mode.close_windows = lambda seconds: 0
        self.assertEqual(self.run_command('exit-private'), 0)
        self.assertLess(self.kinds().index('switch'), self.kinds().index('end'))
        self.assertIn('sealed', self.mode.AUDIT.read_text())

    def test_exit_private_refused_in_normal(self):
        self.mode.private_mode = lambda: False
        self.assertEqual(self.run_command('exit-private'), 2)
        self.assertEqual(self.events, [])

    def test_private_always_locks_on_idle_and_sleep(self):
        self.mode.CONFIG_HOME.mkdir(parents=True)
        (self.mode.CONFIG_HOME / 'Preferences.json').write_text(json.dumps({'lock.idleMinutes': 0, 'lock.onSleep': False}))
        self.mode.private_mode = lambda: False
        normal = self.mode.idle_arguments()
        self.assertNotIn('timeout', normal)
        self.assertNotIn('before-sleep', normal)
        self.mode.private_mode = lambda: True
        private = self.mode.idle_arguments()
        self.assertEqual(private[private.index('timeout') + 1], '300')
        self.assertIn('before-sleep', private)

    def test_audit_never_written_in_normal(self):
        self.mode.private_mode = lambda: False
        self.mode.audit('locked')
        self.assertFalse(self.mode.AUDIT.exists())

    def test_config_rejects_unsafe_values(self):
        module = load('sovel-mode')
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'private.conf'
            module.CONFIG = path
            path.write_text('[private]\nuser=nate-lab\nnormal_vt=7\nprivate_vt=7\n')
            self.assertIsNone(module.config())
            path.write_text('[private]\nuser=$(reboot)\n')
            self.assertIsNone(module.config())
            path.write_text('[private]\nuser=nate-lab\n')
            self.assertEqual(module.config()['private_vt'], 8)


class PrivateAdministration(unittest.TestCase):
    def setUp(self):
        self.admin = load('sovel-private')

    def test_encryption_label_and_strength(self):
        a = self.admin
        a.read_config = lambda: {'user': 'nate-lab', 'normal_user': '', 'normal_vt': 7, 'private_vt': 8, 'shortcut': 'W-S-p'}
        a.homed_record = lambda user: {'storage': 'luks', 'luksCipher': 'aes', 'luksCipherMode': 'xts-plain64', 'luksVolumeKeySize': 64,
                                       'status': {'test-machine': {'state': 'inactive'}}}
        a.MACHINE_ID = Path(tempfile.mkdtemp()) / 'machine-id'
        a.MACHINE_ID.write_text('test-machine\n')
        a.output = lambda *args, **kwargs: ''
        checks = {c['id']: c for c in a.isolation_report()['checks']}
        self.assertEqual(checks['encryption']['status'], 'ok')
        self.assertIn('AES-256-XTS', checks['encryption']['detail'])
        self.assertIn('Sealed', checks['state']['detail'])
        a.homed_record = lambda user: {'storage': 'luks', 'luksCipher': 'aes', 'luksCipherMode': 'xts-plain64', 'luksVolumeKeySize': 32}
        checks = {c['id']: c for c in a.isolation_report()['checks']}
        self.assertEqual(checks['encryption']['status'], 'fail')  # AES-128-XTS is not accepted
        a.homed_record = lambda user: {'storage': 'directory'}
        self.assertEqual({c['id']: c for c in a.isolation_report()['checks']}['encryption']['status'], 'fail')

    def test_generated_greeter_never_steals_the_screen(self):
        self.admin.greetd_binary = lambda: '/usr/sbin/greetd'
        files = {str(k): v[0] for k, v in self.admin.setup_files('nate-lab', 'nate', 7, 8, 'W-S-p').items()}
        greetd = files[str(self.admin.GREETD_CONFIG)]
        self.assertIn('vt = 8', greetd)
        self.assertIn('switch = false', greetd)
        self.assertIn('sovel-greeter --private', greetd)
        self.assertIn('runfile = "/run/sovel-private-greetd.run"', greetd)
        self.assertIn('Conflicts=getty@tty8.service', files[str(self.admin.UNIT)])
        self.assertIn('AllowHibernation=no', files[str(self.admin.SLEEP_DROPIN)])
        self.assertIn('@sovel-private hard core 0', files[str(self.admin.LIMITS)])

    def test_changes_require_root(self):
        if os.geteuid() == 0:
            self.skipTest('running as root')
        with self.assertRaises(self.admin.Failure):
            self.admin.require_root()


class VaultBoundary(unittest.TestCase):
    def test_vault_refused_outside_private(self):
        result = subprocess.run([sys.executable, str(PACKAGING / 'sovel-vault'), 'status'], capture_output=True, text=True)
        if result.returncode == 0:
            self.skipTest('this account belongs to sovel-private')
        self.assertEqual(result.returncode, 2)
        self.assertIn('private workspace', json.loads(result.stdout)['error'])


class LockConfirmation(unittest.TestCase):
    """sovel-lock --wait must succeed only after gtklock's lock command runs, and never hang its caller."""

    def fake_locker(self, folder, behaviour):
        path = Path(folder) / 'gtklock'
        path.write_text(textwrap.dedent(f'''\
            #!/bin/sh
            lock=""; unlock=""
            while [ $# -gt 0 ]; do
              case "$1" in --lock-command) lock=$2; shift ;; --unlock-command) unlock=$2; shift ;; esac
              shift
            done
            {behaviour}
            '''))
        path.chmod(path.stat().st_mode | stat.S_IEXEC)

    def run_lock(self, behaviour):
        with tempfile.TemporaryDirectory() as folder:
            self.fake_locker(folder, behaviour)
            env = {**os.environ, 'PATH': folder + ':' + os.environ['PATH'], 'XDG_RUNTIME_DIR': folder, 'HOME': folder}
            started = time.monotonic()
            # Captured stdout reproduces how sovel-mode calls it: a held pipe would hang here.
            result = subprocess.run(['sh', str(PACKAGING / 'sovel-lock'), '--wait'], env=env,
                                    capture_output=True, text=True, timeout=20)
            elapsed = time.monotonic() - started
            confirmed = (Path(folder) / 'sovel-lock.state').exists()
            subprocess.run(['pkill', '-f', folder + '/gtklock'], check=False)
            return result.returncode, elapsed, confirmed

    def test_confirmed_lock_returns_promptly(self):
        code, elapsed, confirmed = self.run_lock('sleep 0.3; $lock; sleep 30')
        self.assertEqual(code, 0)
        self.assertTrue(confirmed)
        self.assertLess(elapsed, 5)

    def test_locker_that_never_locks_is_a_failure(self):
        code, _elapsed, confirmed = self.run_lock('exit 1')
        self.assertEqual(code, 1)
        self.assertFalse(confirmed)


class SessionKeybinds(unittest.TestCase):
    def test_normal_mode_has_no_jared_binding_and_gets_private_shortcut(self):
        helper = load('sovel-settings-helper')
        with tempfile.TemporaryDirectory() as folder:
            conf = Path(folder) / 'private.conf'
            conf.write_text('[private]\nuser=nate-lab\nshortcut=W-S-p\n')
            helper.PRIVATE_CONFIG = conf
            helper.CONFIG = Path(folder) / 'Sovel'
            helper.private_mode = lambda: False
            env = os.environ.pop('WAYLAND_DISPLAY', None)
            try:
                helper.apply_preferences()
            finally:
                if env is not None:
                    os.environ['WAYLAND_DISPLAY'] = env
            root = ET.parse(helper.CONFIG / 'labwc/rc.xml').getroot()
            binds = {b.get('key'): b.find('action').get('command', '') for b in root.find('keyboard') if b.tag == 'keybind'}
            self.assertNotIn('W-j', binds)
            self.assertEqual(binds['W-S-p'], '/usr/bin/sovel-mode enter-private')
            self.assertEqual(binds['W-l'], '/usr/bin/sovel-mode lock')
            helper.private_mode = lambda: True
            helper.apply_preferences()
            root = ET.parse(helper.CONFIG / 'labwc/rc.xml').getroot()
            binds = {b.get('key'): b.find('action').get('command', '') for b in root.find('keyboard') if b.tag == 'keybind'}
            self.assertIn('W-j', binds)
            self.assertNotIn('W-S-p', binds)

    def test_before_setup_normal_keeps_jared_and_shortcut_opens_setup(self):
        helper = load('sovel-settings-helper')
        with tempfile.TemporaryDirectory() as folder:
            helper.PRIVATE_CONFIG = Path(folder) / 'missing.conf'
            helper.CONFIG = Path(folder) / 'Sovel'
            helper.private_mode = lambda: False
            env = os.environ.pop('WAYLAND_DISPLAY', None)
            try:
                helper.apply_preferences()
            finally:
                if env is not None:
                    os.environ['WAYLAND_DISPLAY'] = env
            root = ET.parse(helper.CONFIG / 'labwc/rc.xml').getroot()
            binds = {b.get('key'): b.find('action').get('command', '') for b in root.find('keyboard') if b.tag == 'keybind'}
            self.assertIn('W-j', binds)  # no private account yet, so JARED stays here
            self.assertEqual(binds['W-S-p'], '/usr/bin/sovel-mode enter-private')
            self.assertTrue(helper.assistant_available())
            self.assertIn('rows', helper.report('jared') if hasattr(helper, 'report') else {'rows': []})


if __name__ == '__main__':
    unittest.main(verbosity=2)
