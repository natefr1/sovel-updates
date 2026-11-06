"""Update a source checkout from a trusted local archive, preserving unchanged mtimes."""
import sys,zipfile
from pathlib import Path
archive=Path(sys.argv[1]).resolve()
target=Path(sys.argv[2]).resolve()
assert target.name=='sovel-rebuild' and (target/'CMakeLists.txt').is_file()
changed=0
with zipfile.ZipFile(archive) as bundle:
    for entry in bundle.infolist():
        path=(target/entry.filename).resolve()
        assert path.is_relative_to(target) and not entry.is_dir()
        data=bundle.read(entry)
        if path.is_file() and path.read_bytes()==data:continue
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_bytes(data)
        changed+=1
print(f'Updated {changed} source files.')
