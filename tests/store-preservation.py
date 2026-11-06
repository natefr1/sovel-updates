"""A release cannot lose any protected listing or add destructive Store commands."""
from pathlib import Path
import json,re
r=Path(__file__).resolve().parents[1]
apps=json.loads((r/'assets/store-catalog.json').read_text(encoding='utf-8'))
ids=[a['id'] for a in apps]
assert len(ids)==len(set(ids)),'Duplicate app ID'
preserved=set(json.loads((r/'assets/store-preserved-ids.json').read_text(encoding='utf-8')))
assert preserved<=set(ids),'Protected Store listings were removed'
source=(r/'src/store.cpp').read_text(encoding='utf-8')
assert 'if(!entry.value("is_eol").toBool())' not in source,'Retired listing hidden'
assert 'if(!allowedOperation(args))' in source
assert not re.search(r'run\(\s*\{\s*"(?:uninstall|remove|remote-delete|remote-modify)"',source)
print('STORE_ADDITIVE_POLICY_PASSED',len(ids),'protected listings')
