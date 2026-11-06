"""Validate assembled image files before ISO creation; this does not prove bootability."""
import json,subprocess,sys,tomllib
from pathlib import Path
root=Path(sys.argv[1]).resolve()
assert root.name=='rootfs' and root.parent.name=='iso-build'
required=['usr/bin/sovel','usr/bin/sovel-session','usr/bin/sovel-greeter','usr/bin/sovel-lock','usr/bin/sovel-setup','usr/lib/sovel/qml/Sovel/Platform/libsovelplatform.so','usr/lib/sovel/qml/Sovel/Platform/qmldir','usr/bin/calamares','usr/bin/unsquashfs','etc/calamares/settings.conf','etc/greetd/config.toml','etc/sovel/live-image','usr/libexec/sovel-finalize-install','etc/calamares/branding/sovel/branding.desc','etc/calamares/branding/sovel/show.qml','etc/calamares/branding/sovel/SetupPage.qml','etc/calamares/branding/sovel/setup-wallpaper.png','etc/calamares/modules/sovelconnections/module.desc']
for name in required:assert (root/name).is_file(),f'Missing {name}'
assert (root/'etc/calamares/modules/mount.conf').is_file(), 'Installer system mounts are not configured'
assert (root/'etc/calamares/modules/soveltarget/main.py').is_file(), 'Pre-copy target validation is missing'
assert (root/'usr/share/sovel/labwc/autostart').stat().st_mode & 0o111, 'Desktop startup script must be executable'
for tool in ['rsync','Xwayland','chvt','update-initramfs','grub-install','grub-mkconfig','efibootmgr','locale-gen','setupcon','hwclock','chpasswd','usb_modeswitch','lsusb']:
    subprocess.run(['chroot',str(root),'/bin/sh','-c','command -v "$1"','sovel-tool-check',tool],check=True,capture_output=True,text=True)
assert 'ID=sovel' in (root/'usr/lib/os-release').read_text()
assert 'BUILD_SYSTEM=Ubuntu' in (root/'etc/casper.conf').read_text()
assert not (root/'etc/apt/sources.list').read_text().strip()
assert (root/'etc/apt/sources.list.d/sovel.sources').exists()
assert (root/'etc/apt/sources.list.d/sovel-public.sources').is_file()
assert (root/'usr/share/keyrings/sovel-updates-archive-keyring.gpg').is_file()
assert (root/'usr/libexec/sovel-setup-display').stat().st_mode & 0o111
assert not (root/'usr/local/bin/sovel').exists(), 'Unmanaged legacy desktop remains'
assert 'SOVEL_RELEASE_CHANNEL=public' in (root/'usr/lib/os-release').read_text()
assert 'Development' not in (root/'usr/lib/os-release').read_text()
assert subprocess.check_output(['chroot',str(root),'dpkg-query','-W','-f=${db:Status-Status}','sovel-desktop'],text=True)=='installed'
assert subprocess.check_output(['chroot',str(root),'dpkg-query','-W','-f=${db:Status-Status}','sovel-aic8800-dkms'],text=True)=='installed'
kernel=sorted((root/'boot').glob('vmlinuz-*'))[-1].name.removeprefix('vmlinuz-')
for driver in ['aic_load_fw', 'aic8800_fdrv', 'aic_zlp_quirk', 'btusb']:
    subprocess.run(['chroot',str(root),'modinfo','-k',kernel,driver],check=True,capture_output=True,text=True)
aliases=subprocess.check_output(['chroot',str(root),'modinfo','-k',kernel,'-F','alias','aic_load_fw'],text=True)
assert 'usb:v368Bp8D81' in aliases, 'Foxtec AX900 USB ID is not supported by the packaged loader'
subprocess.run(['chroot',str(root),'/bin/sh','-c',
    'BUILD_SYSTEM=Custom; . /etc/casper.conf; . /usr/share/initramfs-tools/scripts/casper-helpers; exec modprobe "$MP_QUIET" --dry-run --set-version "$1" -b overlay',
    'sovel-live-boot-check',kernel],check=True,capture_output=True,text=True)
assert 'user = "sovel"' in (root/'etc/greetd/config.toml').read_text()
live_login=tomllib.loads((root/'etc/greetd/config.toml').read_text())
vt=live_login['terminal']['vt']
unit=(root/'usr/lib/systemd/system/greetd.service').read_text()
assert any(line.startswith('Conflicts=') and f'getty@tty{vt}.service' in line.split('=',1)[1].split() for line in unit.splitlines()), 'greetd terminal conflicts with an unmanaged getty'
installed=(root/'usr/share/sovel/greetd-installed.toml').read_text()
assert 'initial_session' not in installed and 'sovel-greeter' in installed
assert tomllib.loads(installed)['terminal']['vt']==vt
assert list(root.glob('usr/lib/**/qt6/plugins/imageformats/libqsvg.so')), 'Qt SVG decoder missing'
assert not (root/'home/nate').exists() and not (root/'home/sovel').exists()
for temporary in ['sovel-ui-config','sovel-test-runtime','sovel-test-cache']:
    assert not (root/'tmp'/temporary).exists(),f'Test files remain in image: {temporary}'
assert not list((root/'etc/ssh').glob('ssh_host_*'))
assert not (root/'root/.ssh').exists()
assert not (root/'var/lib/dkms/mok.key').exists(), 'Image-generated private module-signing key remains'
assert not (root/'var/lib/shim-signed/mok/MOK.priv').exists(), 'Private MOK signing key remains'
assert (root/'etc/machine-id').read_bytes()==b''
assert not (root/'usr/sbin/policy-rc.d').exists()
packages=subprocess.check_output(['chroot',str(root),'dpkg-query','-W','-f=${Package}\n'],text=True).splitlines()
for forbidden in ['gdm3','gnome-shell','gnome-control-center','ubuntu-desktop','ubuntu-desktop-minimal']:
    assert forbidden not in packages,f'Legacy desktop present: {forbidden}'
for name in ['greetd','labwc','casper','network-manager','bluez','blueman','calamares','pipewire','wireplumber','qt6-svg-plugins','console-setup','kbd','xwayland','squashfs-tools','rsync','linux-firmware','usb-modeswitch','usb-modeswitch-data','usbutils']:
    assert name in packages,f'Runtime dependency missing: {name}'
dependencies=subprocess.check_output(['chroot',str(root),'ldd','/usr/bin/sovel'],text=True)
assert 'not found' not in dependencies,dependencies
modules=[p.name for p in root.glob('usr/lib/**/calamares/modules/*') if p.is_dir()]
required_modules=['welcomeq','localeq','keyboardq','partition','usersq','summary','mount','unpackfs','machineid','fstab','localecfg','networkcfg','hwclock','services-systemd','initramfs','bootloader','umount','finishedq','shellprocess','grubcfg']
for name in required_modules:assert name in modules,f'Installer module missing: {name}'
for module in ['welcomeq','localeq','keyboardq','usersq','finishedq']:
    configuration=(root/f'etc/calamares/modules/{module}.conf').read_text()
    assert 'qmlSearch: branding' in configuration
    assert (root/f'etc/calamares/branding/sovel/{module}.qml').is_file()
assert 'sovel-setup' in (root/'usr/bin/sovel-autostart').read_text()
print(json.dumps({'status':'file_checks_passed','packages':len(packages),'installer_modules':len(modules),'boot_and_install_tests':'pending'},indent=2))
