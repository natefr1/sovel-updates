#!/usr/bin/env python3
"""Exact-package desktop retirement. No wildcard purge; no autoremove.

Default is read-only. --apply is for the target Sovel VM AFTER a real
greetd login, secure lock/unlock, Files, terminal and networking pass.
"""
import argparse
import os
import re
import subprocess
import sys

PACKAGES = {
    'ubuntu-desktop','ubuntu-desktop-minimal','ubuntu-session','gnome-session',
    'gnome-session-bin','gnome-session-common','gnome-shell','gnome-shell-common',
    'gnome-control-center','gnome-control-center-data','gnome-control-center-faces',
    'gnome-shell-extension-ubuntu-dock','gnome-shell-extension-desktop-icons-ng',
    'gnome-shell-extension-appindicator','gdm3','gnome-tweaks',
    'gnome-extensions-app','mutter','mutter-common','mutter-common-bin',
    'nautilus','nautilus-data',
}

def run(args):
    return subprocess.run(args,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply',action='store_true')
    args=parser.parse_args()
    installed=run(['dpkg-query','-W','-f=${binary:Package}\t${db:Status-Status}\n'])
    if installed.returncode:sys.exit('Unable to inventory installed packages.')
    present={line.split('\t')[0].split(':')[0] for line in installed.stdout.splitlines() if line.endswith('\tinstalled')}
    targets=sorted(PACKAGES & present)
    if not targets:print('No inventoried legacy packages installed.');return
    plan=run(['apt-get','-s','purge',*targets])
    print(plan.stdout)
    if plan.returncode:sys.exit('APT could not produce a valid removal plan. No changes made.')
    removals=set(re.findall(r'^(?:Remv|Purg) ([^ :]+)',plan.stdout,re.M))
    unknown=removals-PACKAGES
    if unknown:sys.exit('Additional dependencies would be removed; review before extending the allowlist: '+', '.join(sorted(unknown)))
    if not args.apply:
        print('\nPLAN ONLY. --apply performs the reviewed exact-package removal after target checks.')
        return
    if os.geteuid()!=0:sys.exit('Run --apply with sudo on the target VM.')
    active=run(['systemctl','is-active','greetd.service'])
    if active.stdout.strip()!='active':sys.exit('The replacement login service is not active. No changes made.')
    if not os.path.isfile('/usr/bin/sovel') or not os.path.isfile('/usr/bin/sovel-session'):
        sys.exit('Replacement binaries are missing. No changes made.')
    print('Removing exact legacy packages. Core services, kernels and home data are not purge targets.')
    result=subprocess.run(['apt-get','purge','--assume-yes',*targets])
    sys.exit(result.returncode)

if __name__=='__main__':main()
