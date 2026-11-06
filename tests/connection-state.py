"""Pairing keys must stay private, and live-state links must never be followed."""
import importlib.util,sys,tempfile,types,os
from pathlib import Path
sys.modules['libcalamares']=types.SimpleNamespace()
module_path=Path(__file__).resolve().parents[1]/'installer/calamares/modules/sovelconnections/main.py'
spec=importlib.util.spec_from_file_location('sovelconnections',module_path)
job=importlib.util.module_from_spec(spec);spec.loader.exec_module(job)
with tempfile.TemporaryDirectory() as directory:
    root=Path(directory);source=root/'source';source.mkdir();(source/'info').write_text('private test key')
    target=root/'target';job.copy_private_tree(source,target)
    assert (target/'info').read_text()=='private test key'
    if os.name!='nt':
        assert target.stat().st_mode&0o777==0o700
        assert (target/'info').stat().st_mode&0o777==0o600
        outside=root/'outside';outside.write_text('do not copy')
        (source/'link').symlink_to(outside)
        try:job.copy_private_tree(source,root/'blocked')
        except ValueError:pass
        else:raise AssertionError('Source links must be rejected')
        (source/'link').unlink();(target/'info').unlink();(target/'info').symlink_to(outside)
        try:job.copy_private_tree(source,target)
        except ValueError:pass
        else:raise AssertionError('Destination links must be rejected')
        assert outside.read_text()=='do not copy'
print('Connection-state privacy checks passed.')
