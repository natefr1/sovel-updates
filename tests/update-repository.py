#!/usr/bin/python3
"""Exercise real APT signature checks and downloading from the public endpoint."""
from pathlib import Path
import argparse,functools,hashlib,http.server,json,shutil,subprocess,tempfile,threading

parser=argparse.ArgumentParser()
parser.add_argument('repository',type=Path)
parser.add_argument('--evidence',type=Path,required=True)
parser.add_argument('--package',default='sovel-desktop=0.1.0-1')
args=parser.parse_args()
repo=args.repository.resolve()
key=repo/'keys/sovel-updates-archive-keyring.gpg'
class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self,*args):pass
with tempfile.TemporaryDirectory(prefix='sovel-apt-verification-') as scratch:
    root=Path(scratch);root.chmod(0o755)
    shutil.copy2(key,root/'public-keyring.gpg');key=root/'public-keyring.gpg';key.chmod(0o644)
    shutil.copytree(repo/'apt',root/'tampered')
    handler=functools.partial(QuietHandler,directory=str(root))
    http=http.server.ThreadingHTTPServer(('127.0.0.1',0),handler)
    thread=threading.Thread(target=http.serve_forever,daemon=True);thread.start()
    def apt(name,url):
        state=root/name;state.mkdir();(state/'lists/partial').mkdir(parents=True);(state/'cache/archives/partial').mkdir(parents=True)
        sources=state/'sources.list';sources.write_text(f'deb [arch=amd64 signed-by={key}] {url} stable main\n')
        return ['apt-get','-o','APT::Update::Error-Mode=any','-o','Debug::NoLocking=1',
            '-o','Dir::State::lists='+str(state/'lists'),'-o','Dir::Cache='+str(state/'cache'),
            '-o','Dir::Etc::sourcelist='+str(sources),'-o','Dir::Etc::sourceparts=-']
    public=apt('public','https://raw.githubusercontent.com/natefr1/sovel-updates/main/apt')
    try:
        check=subprocess.run([*public,'update'],capture_output=True,text=True,timeout=180)
        print(check.stdout,check.stderr)
        assert check.returncode==0,'Public signed APT update failed'
        download=subprocess.run([*public,'download',args.package],cwd=root,capture_output=True,text=True,timeout=180)
        print(download.stdout,download.stderr)
        assert download.returncode==0,'Verified package download failed'
        name=args.package.split('=',1)[0]
        downloaded=next(root.glob(name+'_*.deb'))
        original=repo/'apt/pool/main'/name[0]/name/downloaded.name
        digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
        assert digest(downloaded)==digest(original),'Public download differs from signed release package'
        release=root/'tampered/dists/stable/InRelease'
        data=release.read_bytes();assert b'Label: Sovel' in data
        release.write_bytes(data.replace(b'Label: Sovel',b'Label: Tovel',1))
        bad=apt('bad',f'http://127.0.0.1:{http.server_port}/tampered')
        rejected=subprocess.run([*bad,'update'],capture_output=True,text=True,timeout=60)
        print(rejected.stdout,rejected.stderr)
        assert rejected.returncode!=0,'APT accepted tampered repository metadata'
        assert any(x in rejected.stderr for x in ['signature','BADSIG','not signed','verification failed']),rejected.stderr
        evidence={'package':args.package,'public_signed_metadata':True,'verified_public_download':True,'tampered_signature_rejected':True,'package_sha256':digest(downloaded)}
        args.evidence.write_text(json.dumps(evidence,indent=2)+'\n')
        print(json.dumps(evidence,indent=2))
    finally:http.shutdown()
