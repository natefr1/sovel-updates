#!/usr/bin/python3
"""Build signed public APT metadata. The signing home stays outside the repo."""
import argparse
import datetime as dt
import email.utils
import gzip
import hashlib
from pathlib import Path
import shutil
import subprocess
import base64
import json

parser=argparse.ArgumentParser()
parser.add_argument('packages',type=Path)
parser.add_argument('repository',type=Path)
parser.add_argument('--signing-home',type=Path,required=True)
args=parser.parse_args()
root=args.repository.resolve();root.mkdir(parents=True,exist_ok=True)
home=args.signing_home.resolve()
if root==home or root in home.parents:raise SystemExit('Signing keys must be outside the public repository.')
gpg=['gpg','--homedir',str(home),'--batch','--yes']
keys=subprocess.check_output([*gpg,'--with-colons','--list-secret-keys'],text=True)
fingerprints=[line.split(':')[9] for line in keys.splitlines() if line.startswith('fpr:')]
if len(fingerprints)!=1:raise SystemExit('Expected exactly one dedicated release signing key.')
fingerprint=fingerprints[0]
allowed_packages={'sovel-desktop','sovel-aic8800-dkms','sovel-wallpaper-tools'}
for source in args.packages.glob('*.deb'):
    name=subprocess.check_output(['dpkg-deb','--field',str(source),'Package'],text=True).strip()
    if name not in allowed_packages:
        raise SystemExit(f'Unexpected package in the public release directory: {name}')
    architecture=subprocess.check_output(['dpkg-deb','--field',str(source),'Architecture'],text=True).strip()
    if architecture not in {'amd64','all'}:
        raise SystemExit(f'Unsupported release architecture: {architecture}')
    pool=root/'apt/pool/main'/name[0]/name
    pool.mkdir(parents=True,exist_ok=True)
    destination=pool/source.name
    if destination.exists() and destination.read_bytes()!=source.read_bytes():
        raise SystemExit('A published package version is immutable; increase its version.')
    shutil.copyfile(source,destination)
entries=[]
ledger_path=home.parent/'publications.json'
try:ledger=json.loads(ledger_path.read_text(encoding='utf-8'))
except FileNotFoundError:ledger={}
for package in sorted((root/'apt/pool/main').rglob('*.deb')):
    metadata=subprocess.check_output(['dpkg-deb','--field',str(package)],text=True).rstrip()
    data=package.read_bytes()
    fields=dict(line.split(': ',1) for line in metadata.splitlines() if ': ' in line and not line.startswith(' '))
    if fields.get('Package')=='sovel-desktop' and fields.get('X-Sovel-Release-Info'):
        info=json.loads(base64.b64decode(fields['X-Sovel-Release-Info'],validate=True))
        version=fields['Version'];digest=hashlib.sha256(data).hexdigest()
        if info.get('version')!=version:raise SystemExit('Release identity differs from package version.')
        if version in ledger and ledger[version]['sha256']!=digest:raise SystemExit('Published release hash changed.')
        if version not in ledger:ledger[version]={'sha256':digest,'publishedAt':dt.datetime.now(dt.timezone.utc).isoformat(timespec='seconds')}
        info['publishedAt']=ledger[version]['publishedAt']
        encoded=base64.b64encode(json.dumps(info,ensure_ascii=False,separators=(',',':')).encode()).decode()
        metadata='\n'.join('X-Sovel-Release-Info: '+encoded if line.startswith('X-Sovel-Release-Info: ') else line for line in metadata.splitlines())
    hashes=''.join(f'{label}: {hashlib.new(algorithm,data).hexdigest()}\n' for label,algorithm in [('MD5sum','md5'),('SHA1','sha1'),('SHA256','sha256'),('SHA512','sha512')])
    entries.append(f'{metadata}\nFilename: {package.relative_to(root/"apt").as_posix()}\nSize: {len(data)}\n{hashes}\n')
if not entries:raise SystemExit('No release packages found.')
temporary=ledger_path.with_suffix('.tmp');temporary.write_text(json.dumps(ledger,indent=2)+'\n',encoding='utf-8');temporary.replace(ledger_path)
distribution=root/'apt/dists/stable';binary=distribution/'main/binary-amd64';binary.mkdir(parents=True,exist_ok=True)
index=''.join(entries).encode()
(binary/'Packages').write_bytes(index)
(binary/'Packages.gz').write_bytes(gzip.compress(index,mtime=0))
now=dt.datetime.now(dt.timezone.utc)
release='\n'.join(['Origin: Sovel','Label: Sovel Public Updates','Suite: stable','Codename: stable',
    'Date: '+email.utils.format_datetime(now,usegmt=True),
    'Valid-Until: '+email.utils.format_datetime(now+dt.timedelta(days=30),usegmt=True),
    'Architectures: amd64','Components: main','Description: Signed Sovel public system updates','Acquire-By-Hash: yes'])+'\n'
for algorithm in ['sha256','sha512']:
    release+=algorithm.upper()+':\n'
    for path in [binary/'Packages',binary/'Packages.gz']:
        data=path.read_bytes();digest=hashlib.new(algorithm,data).hexdigest()
        release+=f' {digest} {len(data):16d} {path.relative_to(distribution).as_posix()}\n'
        by_hash=path.parent/'by-hash'/algorithm.upper()/digest;by_hash.parent.mkdir(parents=True,exist_ok=True);by_hash.write_bytes(data)
(distribution/'Release').write_text(release,encoding='utf-8',newline='\n')
for output,mode in [('InRelease','--clearsign'),('Release.gpg','--detach-sign')]:
    subprocess.run([*gpg,'--local-user',fingerprint,'--digest-algo','SHA256','--output',str(distribution/output),mode,str(distribution/'Release')],check=True)
keydir=root/'keys';keydir.mkdir(exist_ok=True)
for name,extra in [('sovel-updates-archive-keyring.gpg',[]),('sovel-updates.asc',['--armor'])]:
    (keydir/name).write_bytes(subprocess.check_output([*gpg,*extra,'--export',fingerprint]))
(keydir/'fingerprint.txt').write_text(fingerprint+'\n',encoding='ascii')
subprocess.run(['gpgv','--keyring',str(keydir/'sovel-updates-archive-keyring.gpg'),str(distribution/'InRelease')],check=True)
print('Signed public repository:',root,'fingerprint:',fingerprint)
