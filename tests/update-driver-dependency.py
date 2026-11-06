"""Verify signed APT selects the driver when upgrading an older desktop."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

parser=argparse.ArgumentParser()
parser.add_argument('repository',type=Path)
parser.add_argument('image',type=Path)
parser.add_argument('--evidence',type=Path,required=True)
args=parser.parse_args()
with tempfile.TemporaryDirectory(prefix='sovel-upgrade-solver-') as directory:
    root=Path(directory);root.chmod(0o755)
    key=root/'key.gpg'
    shutil.copyfile(args.repository/'keys/sovel-updates-archive-keyring.gpg',key)
    key.chmod(0o644)
    status=root/'status'
    entries=(args.image/'var/lib/dpkg/status').read_text().split('\n\n')
    entries=[entry for entry in entries if not entry.startswith('Package: sovel-aic8800-dkms\n')]
    assert any(entry.startswith('Package: sovel-desktop\n') and '\nVersion: 0.1.0-2\n' in entry for entry in entries)
    status.write_text('\n\n'.join(entries))
    (root/'lists/partial').mkdir(parents=True)
    (root/'cache/archives/partial').mkdir(parents=True)
    sources=root/'sources.list'
    sources.write_text(f'deb [arch=amd64 signed-by={key}] https://raw.githubusercontent.com/natefr1/sovel-updates/main/apt stable main\n')
    apt=['apt-get','-o','Dir::Etc::main=-','-o','Dir::Etc::parts=-',
         '-o','Dir::Etc::sourcelist='+str(sources),'-o','Dir::Etc::sourceparts=-',
         '-o','Dir::State::status='+str(status),'-o','Dir::State::lists='+str(root/'lists'),
         '-o','Dir::Cache='+str(root/'cache'),'-o','Debug::NoLocking=1',
         '-o','APT::Update::Error-Mode=any']
    subprocess.run([*apt,'update'],check=True,timeout=180)
    result=subprocess.run([*apt,'--simulate','install','sovel-desktop=0.1.0-3'],capture_output=True,text=True,timeout=60)
    print(result.stdout,result.stderr)
    assert result.returncode==0
    assert 'Inst sovel-aic8800-dkms ' in result.stdout
    assert 'Inst sovel-desktop [0.1.0-2] (0.1.0-3 ' in result.stdout
    assert not any(line.startswith('Remv ') for line in result.stdout.splitlines())
    evidence={'signed_public_metadata':True,'desktop_upgrade':'0.1.0-2 -> 0.1.0-3',
              'missing_driver_selected_automatically':True,'packages_removed':False,
              'scope':'APT solver using an image status copy with the driver removed; not a live PackageKit installation'}
    args.evidence.write_text(json.dumps(evidence,indent=2)+'\n')
    print(json.dumps(evidence,indent=2))
