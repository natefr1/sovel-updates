"""Compatibility entry point for the native Settings interaction suite."""
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
executable=root/'build/sovel-settings-ui-tests'
if not executable.is_file():raise SystemExit('Build sovel-settings-ui-tests in the Sovel VM first.')
raise SystemExit(subprocess.call([str(executable)],env={**os.environ,'QT_QPA_PLATFORM':'offscreen','QT_QUICK_BACKEND':'software','QT_QUICK_CONTROLS_STYLE':'Basic'}))
