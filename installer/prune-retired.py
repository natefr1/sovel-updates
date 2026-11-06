from pathlib import Path
import sys
root=Path(sys.argv[1]).resolve()
assert root.name=='sovel-rebuild' and (root/'CMakeLists.txt').is_file()
for relative in ['qml/Motion.qml','packaging/apps/sovel-motion.desktop','assets/icons/motion.svg']:
    target=(root/relative).resolve();assert target.is_relative_to(root)
    if target.is_file():target.unlink()
