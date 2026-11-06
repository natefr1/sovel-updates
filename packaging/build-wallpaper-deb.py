#!/usr/bin/python3
"""Package pinned external apps, with source and the documented IPC patch."""
from pathlib import Path
import hashlib,json,shutil,subprocess,sys,tarfile,tempfile,urllib.request
source=Path(__file__).resolve().parent
output=Path(sys.argv[1]).resolve();output.mkdir(parents=True,exist_ok=True)
version='2.9+mpv1.9-1'
with tempfile.TemporaryDirectory(prefix='sovel-wallpapers-',dir=output) as temporary:
    work=Path(temporary);trees={};archives=[]
    for item in json.loads((source/'wallpaper-sources.json').read_text()):
        archive=work/(item['name']+'.tar.gz')
        with urllib.request.urlopen(item['url'],timeout=120) as response:archive.write_bytes(response.read())
        assert hashlib.sha256(archive.read_bytes()).hexdigest()==item['sha256'],'Upstream source checksum changed'
        dest=work/item['name'];dest.mkdir()
        with tarfile.open(archive) as bundle:bundle.extractall(dest,filter='data')
        trees[item['name']]=next(dest.iterdir());archives.append(archive)
    subprocess.run([sys.executable,str(source/'patch-waypaper.py'),str(trees['waypaper'])],check=True)
    subprocess.run(['meson','setup',str(work/'mpv-build'),str(trees['mpvpaper']),'--prefix=/usr','--buildtype=release'],check=True)
    subprocess.run(['meson','compile','-C',str(work/'mpv-build'),'-j','1'],check=True)
    stage=work/'package';(stage/'usr/bin').mkdir(parents=True)
    for name in ['mpvpaper','mpvpaper-holder']:shutil.copy2(work/'mpv-build'/name,stage/'usr/bin'/name)
    module=stage/'usr/lib/sovel/external/waypaper';module.parent.mkdir(parents=True)
    shutil.copytree(trees['waypaper']/'waypaper',module)
    launcher=stage/'usr/bin/waypaper';launcher.write_text('#!/usr/bin/python3\nimport sys\nsys.path.insert(0,"/usr/lib/sovel/external")\nfrom gi.repository import GLib\nGLib.set_prgname("waypaper")\nGLib.set_application_name("Waypaper")\nfrom waypaper.__main__ import run\nrun()\n');launcher.chmod(0o755)
    for folder in ['usr/share/applications','usr/share/icons/hicolor/scalable/apps','usr/share/doc/sovel-wallpaper-tools','DEBIAN']:(stage/folder).mkdir(parents=True,exist_ok=True)
    desktop=(trees['waypaper']/'data/waypaper.desktop').read_text().replace('Exec=waypaper','Exec=sovel --app wallpapers')+'\nStartupWMClass=waypaper\n'
    (stage/'usr/share/applications/waypaper.desktop').write_text(desktop)
    shutil.copy2(trees['waypaper']/'data/waypaper.svg',stage/'usr/share/icons/hicolor/scalable/apps/waypaper.svg')
    doc=stage/'usr/share/doc/sovel-wallpaper-tools'
    for archive in archives:shutil.copy2(archive,doc/archive.name)
    shutil.copy2(source/'patch-waypaper.py',doc/'patch-waypaper.py')
    shutil.copy2(source/'wallpaper-sources.json',doc/'sources.json')
    shutil.copy2(trees['waypaper']/'LICENSE',doc/'waypaper-LICENSE');shutil.copy2(trees['mpvpaper']/'LICENSE',doc/'mpvpaper-LICENSE')
    (doc/'README').write_text('External Waypaper 2.9 by Roman Anufriev and mpvpaper 1.9 by GhostNaN.\nGPL-3.0. Complete upstream source archives and the applied compatibility patch are included.\nThe patch uses private JSON IPC for filenames and pauses video behind maximized windows.\n')
    shlib=work/'shlibs';(shlib/'debian').mkdir(parents=True);(shlib/'debian/control').write_text('Source: sovel-wallpaper-tools\nSection: graphics\nPriority: optional\nMaintainer: Sovel Release Team\n\nPackage: sovel-wallpaper-tools\nArchitecture: any\nDescription: External wallpaper tools\n')
    libraries=subprocess.check_output(['dpkg-shlibdeps','--ignore-missing-info','-O','-e'+str(stage/'usr/bin/mpvpaper'),'-e'+str(stage/'usr/bin/mpvpaper-holder')],cwd=shlib,text=True).strip().removeprefix('shlibs:Depends=')
    assert libraries
    (stage/'DEBIAN/control').write_text(f'Package: sovel-wallpaper-tools\nVersion: {version}\nArchitecture: amd64\nMaintainer: Sovel Release Team\nSection: graphics\nPriority: optional\nDepends: {libraries}, python3, python3-gi, gir1.2-gtk-3.0, python3-pil, python3-platformdirs, python3-screeninfo, python3-imageio, python3-imageio-ffmpeg\nDescription: External Waypaper and mpvpaper wallpaper tools for Sovel\n Waypaper provides the wallpaper chooser; mpvpaper displays still images and video.\n')
    subprocess.run(['dpkg-deb','--root-owner-group','--build',str(stage),str(output/f'sovel-wallpaper-tools_{version}_amd64.deb')],check=True)
