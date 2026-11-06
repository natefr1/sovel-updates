#!/usr/bin/env python3
"""Render the actual Qt application, never a web mockup.

Run with a working DISPLAY or a configured QT_QPA_PLATFORM=offscreen.
An offscreen software renderer exercises the solid fallback, not refraction.
No real authentication is allowed during capture.
"""
from pathlib import Path
import argparse
import os
import subprocess

p=argparse.ArgumentParser()
p.add_argument('binary',type=Path)
p.add_argument('--output',type=Path,default=Path('/tmp/sovel-render-check'))
args=p.parse_args()
if os.environ.get('GREETD_SOCK'):raise SystemExit('Do not run captures in a real greeter session.')
args.output.mkdir(parents=True,exist_ok=True)
env=os.environ.copy()
env['QT_QUICK_CONTROLS_STYLE']='Basic'
bad=['Binding loop','is not defined','Cannot assign','TypeError','Unsupported image format','Error decoding','is not available']
for name,mode in [('desktop',['--preview']),('greeter',['--greeter']),('files',['--app','files']),('settings',['--app','settings'])]:
    image=args.output/(name+'.png')
    result=subprocess.run([str(args.binary.resolve()),*mode,'--capture',str(image.resolve())],env=env,text=True,capture_output=True,timeout=20)
    if result.returncode or not image.exists() or any(t in result.stderr for t in bad):
        raise SystemExit(f'{name} failed:\n{result.stderr}')
    print(name,'rendered')
