#!/usr/bin/env python3
"""Sovel update bundle: preflight, apply, status, rollback. No app removal."""
from pathlib import Path
import argparse,hashlib,json,os,platform,shutil,subprocess,sys
ROOT=Path(__file__).resolve().parent
UUID='sovel-glass@sovel.local'
GNOME_PACKAGES=['xdg-desktop-portal-gnome','ibus','ibus-mozc','ibus-libpinyin','ibus-hangul','ibus-m17n']
STATE=Path(os.environ.get('XDG_STATE_HOME',str(Path.home()/'.local/state')))/'sovel/shell-update-6'
def run(args,check=True):
 p=subprocess.run(args,text=True,capture_output=True,env={**os.environ,'LC_ALL':'C.UTF-8'})
 if check and p.returncode:raise RuntimeError(p.stderr.strip() or p.stdout.strip() or str(args))
 return p
def manifest():
 m=json.loads((ROOT/'bundle-manifest.json').read_text(encoding='utf-8'))
 for name,digest in m['sha256'].items():
  path=ROOT/name
  if ROOT not in path.resolve().parents:raise RuntimeError('Invalid bundle path')
  if hashlib.sha256(path.read_bytes()).hexdigest()!=digest:raise RuntimeError('Bundle integrity failure: '+name)
 return m
def sudo(args):
 if os.geteuid()!=0:args=['sudo',*args]
 subprocess.run(args,check=True)

def package_targets(m):
 return [str(ROOT/name) for name in [m['package'],*m.get('dependency_packages',[])]]
def preflight(gnome=False):
 if sys.platform!='linux' or platform.machine() not in ('x86_64','amd64'):raise RuntimeError('Apply this bundle inside the amd64 Sovel installation, not Windows.')
 if os.geteuid()==0:raise RuntimeError('Run as your normal desktop user; this script requests system authorization when needed.')
 m=manifest();package=ROOT/m['package']
 if not shutil.which('apt-get') or not shutil.which('labwc'):raise RuntimeError('This bundle targets the current Sovel Qt/Labwc desktop.')
 if gnome:
  version=run(['gnome-shell','--version']).stdout
  if not version.split()[-1].startswith('50.'):raise RuntimeError('Optional extension requires GNOME Shell 50. The default Sovel session is kept.')
 plan=run(['apt-get','--simulate','--no-install-recommends','install',*package_targets(m),*(GNOME_PACKAGES if gnome else [])])
 if any(line.startswith('Remv ') for line in plan.stdout.splitlines()):raise RuntimeError('Refusing an upgrade that removes installed packages.')
 ids=set(json.loads((ROOT/'assets/store-preserved-ids.json').read_text()))
 apps=json.loads((ROOT/'assets/store-catalog.json').read_text())
 if not ids<={app['id'] for app in apps}:raise RuntimeError('A protected Store listing is absent.')
 print(json.dumps({'preflight':'passed','package':m['version'],'store_listings':len(apps),'package_removals':False,'current_blur':'Labwc transparency fallback; no compositor backdrop blur','optional_gnome':gnome},indent=2))
 return m
def installed():
 return run(['dpkg-query','-W','-f=${Version}','sovel-desktop'],False).stdout.strip()
def apply(gnome=False):
 m=preflight(gnome);STATE.mkdir(parents=True,exist_ok=True)
 backup=STATE/'backup.json'
 if not backup.exists():
  previous=installed()
  if previous!='0.1.0-5':raise RuntimeError('Rollback package is 0.1.0-5; this installer requires that installed baseline.')
  record={'previous':previous,'gnome':False}
  backup.write_text(json.dumps(record,indent=2),encoding='utf-8')
 sudo(['apt-get','install','--no-install-recommends','--no-remove',*package_targets(m),*(GNOME_PACKAGES if gnome else [])])
 # Use the maintained Discord-compatible client with Wayland sharing support.
 # Keep official Discord and every other installed app in place.
 if run(['flatpak','info','dev.vencord.Vesktop'],False).returncode:
  run(['flatpak','remote-add','--user','--if-not-exists','--from','flathub','https://flathub.org/repo/flathub.flatpakrepo'])
  subprocess.run(['flatpak','install','--user','--noninteractive','--assumeyes','flathub','dev.vencord.Vesktop'],check=True)
 if gnome:
  record=json.loads(backup.read_text())
  if not record.get('gnome'):
   record['enabled_extensions']=run(['gsettings','get','org.gnome.shell','enabled-extensions']).stdout.strip()
   record['screenshot_keys']=run(['gsettings','get','org.gnome.shell.keybindings','show-screenshot-ui']).stdout.strip()
   record['gnome']=True;backup.write_text(json.dumps(record,indent=2),encoding='utf-8')
  target=Path.home()/'.local/share/gnome-shell/extensions'/UUID
  if target.exists() and not (STATE/'previous-extension').exists():shutil.copytree(target,STATE/'previous-extension')
  shutil.copytree(ROOT/'optional-gnome'/UUID,target,dirs_exist_ok=True)
  # Preserve all other enabled extensions and existing screenshot keys.
  from gi.repository import Gio,GLib
  settings=Gio.Settings.new('org.gnome.shell');values=settings.get_strv('enabled-extensions')
  if UUID not in values:settings.set_strv('enabled-extensions',values+[UUID])
  keys=Gio.Settings.new('org.gnome.shell.keybindings');values=keys.get_strv('show-screenshot-ui')
  if '<Super><Shift>s' not in values:keys.set_strv('show-screenshot-ui',values+['<Super><Shift>s'])
  inputs=Gio.Settings.new('org.gnome.desktop.input-sources')
  if 'input_sources' not in record:
   record['input_sources']=inputs.get_value('sources').print_(True)
   backup.write_text(json.dumps(record,indent=2),encoding='utf-8')
  sources=inputs.get_value('sources').unpack()
  for item in [('ibus','mozc-jp'),('ibus','libpinyin'),('ibus','hangul'),('ibus','m17n:hi:itrans'),('xkb','ara'),('xkb','ru')]:
   if item not in sources:sources.append(item)
  inputs.set_value('sources',GLib.Variant('a(ss)',sources))
  portal=Path(os.environ.get('XDG_CONFIG_HOME',str(Path.home()/'.config')))/'xdg-desktop-portal/gnome-portals.conf'
  if 'gnome_portal' not in record:
   record['gnome_portal']=portal.read_text(encoding='utf-8') if portal.exists() else None
   backup.write_text(json.dumps(record,indent=2),encoding='utf-8')
  portal.parent.mkdir(parents=True,exist_ok=True)
  shutil.copyfile(ROOT/'optional-gnome/gnome-portals.conf',portal)
  Gio.Settings.sync()
 print('Applied. Log out and sign in again, or restart Sovel. Greeter unchanged.')
def status():
 result={'installed_desktop':installed(),'desktop':os.environ.get('XDG_CURRENT_DESKTOP','SSH console'),'rollback_available':(STATE/'backup.json').exists()}
 for name,args in {
  'fonts':['fc-match','Noto Sans CJK JP'],
  'services':['systemctl','--user','is-active','pipewire','pipewire-pulse','wireplumber','xdg-desktop-portal'],
  'portal':['gdbus','introspect','--session','--dest','org.freedesktop.portal.Desktop','--object-path','/org/freedesktop/portal/desktop'],
  'input':['fcitx5-remote','-n']}.items():
  if name in ('portal','input') and not os.environ.get('WAYLAND_DISPLAY'):
   result[name]={'status':'desktop sign-in required','note':'Run status from the Sovel desktop terminal to inspect this graphical session.'}
   continue
  try:
   p=subprocess.run(args,text=True,capture_output=True,timeout=15);result[name]={'exit':p.returncode,'output':p.stdout.strip(),'error':p.stderr.strip()}
  except (OSError,subprocess.TimeoutExpired) as e:result[name]={'error':str(e)}
 print(json.dumps(result,ensure_ascii=False,indent=2))
def rollback():
 m=manifest();backup=STATE/'backup.json'
 if not backup.exists():raise RuntimeError('No update applied by this installer.')
 record=json.loads(backup.read_text());package=ROOT/m['rollback_package']
 plan=run(['apt-get','--simulate','--no-install-recommends','--allow-downgrades','install',str(package)])
 if any(line.startswith('Remv ') for line in plan.stdout.splitlines()):raise RuntimeError('Rollback would remove an installed package; stopped.')
 sudo(['apt-get','install','--no-install-recommends','--allow-downgrades','--no-remove',str(package)])
 if record.get('gnome'):
  run(['gsettings','set','org.gnome.shell','enabled-extensions',record['enabled_extensions']])
  run(['gsettings','set','org.gnome.shell.keybindings','show-screenshot-ui',record['screenshot_keys']])
  if 'input_sources' in record:run(['gsettings','set','org.gnome.desktop.input-sources','sources',record['input_sources']])
  previous=STATE/'previous-extension'
  if previous.exists():shutil.copytree(previous,Path.home()/'.local/share/gnome-shell/extensions'/UUID,dirs_exist_ok=True)
  portal=Path(os.environ.get('XDG_CONFIG_HOME',str(Path.home()/'.config')))/'xdg-desktop-portal/gnome-portals.conf'
  if record.get('gnome_portal') is not None:portal.write_text(record['gnome_portal'],encoding='utf-8')
  elif portal.exists() and portal.read_bytes()==(ROOT/'optional-gnome/gnome-portals.conf').read_bytes():portal.unlink()
 print('Previous desktop restored. Added fonts and apps remain installed. Restart Sovel.')
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('action',choices=['preflight','apply','status','rollback']);parser.add_argument('--gnome',action='store_true');args=parser.parse_args()
 try:
  if args.action=='preflight':preflight(args.gnome)
  elif args.action=='apply':apply(args.gnome)
  elif args.action=='rollback':rollback()
  else:status()
 except (OSError,RuntimeError,subprocess.CalledProcessError) as error:print(str(error),file=sys.stderr);sys.exit(1)
