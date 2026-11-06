"""Run as root on Linux; all temporary mounts live in a private namespace."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import types

assert os.name == 'posix' and os.geteuid() == 0, 'Run with sudo on Linux'
if '--private-mounts' not in sys.argv:
    subprocess.run(['unshare', '--mount', '--propagation', 'private',
                    sys.executable, str(Path(__file__).resolve()), '--private-mounts'], check=True)
    sys.exit(0)

storage = {}
sys.modules['libcalamares'] = types.SimpleNamespace(
    globalstorage=types.SimpleNamespace(value=storage.get))
path = Path(__file__).resolve().parents[1] / 'installer/calamares/modules/soveltarget/main.py'
spec = importlib.util.spec_from_file_location('soveltarget', path)
job = importlib.util.module_from_spec(spec)
spec.loader.exec_module(job)
for invalid in [None, '', '/', 'relative/path']:
    storage['rootMountPoint'] = invalid
    assert job.run() is not None, f'Unsafe target accepted: {invalid!r}'

with tempfile.TemporaryDirectory(prefix='sovel-mount-test-') as directory:
    root = Path(directory)
    storage['rootMountPoint'] = directory
    assert job.run() is not None, 'An ordinary directory must not be accepted'
    mounted = []
    def mount(point):
        point.mkdir(parents=True, exist_ok=True)
        subprocess.run(['mount', '-t', 'tmpfs', '-o', 'size=1m', 'tmpfs', str(point)], check=True)
        mounted.append(point)
    try:
        mount(root)
        assert job.run() is not None, 'Missing system mounts must fail before unpacking'
        required = ['dev', 'proc', 'sys', 'run', 'run/udev']
        if Path('/sys/firmware/efi').is_dir():
            required += ['boot/efi', 'sys/firmware/efi/efivars']
        for name in required:
            mount(root / name)
        assert job.run() is None, 'A mounted target with its required mounts must pass'
        removed = mounted.pop()
        subprocess.run(['umount', str(removed)], check=True)
        assert job.run() is not None, 'Losing a required mount must fail'
    finally:
        for point in reversed(mounted):
            subprocess.run(['umount', str(point)], check=True)
    assert not root.is_mount()
    assert job.run() is not None, 'An unmounted target must fail again'
print('Installer target checks passed with real mounts in a private namespace.')
