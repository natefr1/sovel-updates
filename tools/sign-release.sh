#!/bin/bash
# Sign the APT index on this branch with the Sovel release key and push it.
# Run inside the machine that holds the key. The key never leaves that keyring.
#
#   git fetch origin release-0.1.0-11 && git checkout release-0.1.0-11
#   tools/sign-release.sh
#
# Override the keyring location with SOVEL_GNUPGHOME if needed.
set -euo pipefail
cd "$(dirname "$0")/.."
export GNUPGHOME=${SOVEL_GNUPGHOME:-/home/nate/.local/share/sovel-release/gnupg}
fingerprint=$(tr -d ' \n' < keys/fingerprint.txt)
dist=apt/dists/stable

gpg --batch --list-secret-keys "$fingerprint" >/dev/null 2>&1 || {
    echo "The release key $fingerprint is not in $GNUPGHOME." >&2; exit 1; }

python3 - "$dist" <<'PY'
import datetime, hashlib, sys
from pathlib import Path
dist = Path(sys.argv[1])
now = datetime.datetime.now(datetime.timezone.utc)
stamp = lambda t: t.strftime('%a, %d %b %Y %H:%M:%S GMT')
files = ['main/binary-amd64/Packages', 'main/binary-amd64/Packages.gz']
lines = ['Origin: Sovel', 'Label: Sovel Public Updates', 'Suite: stable', 'Codename: stable',
         f'Date: {stamp(now)}', f'Valid-Until: {stamp(now + datetime.timedelta(days=30))}',
         'Architectures: amd64', 'Components: main', 'Description: Signed Sovel public system updates',
         'Acquire-By-Hash: yes']
for algo in ('SHA256', 'SHA512'):
    lines.append(f'{algo}:')
    for name in files:
        data = (dist / name).read_bytes()
        lines.append(f' {hashlib.new(algo.lower(), data).hexdigest()} {len(data):16d} {name}')
(dist / 'Release').write_text('\n'.join(lines) + '\n', encoding='utf-8')
PY

gpg --batch --yes --local-user "$fingerprint" --digest-algo SHA512 --clearsign -o "$dist/InRelease" "$dist/Release"
gpg --batch --yes --local-user "$fingerprint" --digest-algo SHA512 --no-armor --detach-sign -o "$dist/Release.gpg" "$dist/Release"

# Check both signatures against the public key every Sovel PC trusts.
gpgv --keyring ./keys/sovel-updates-archive-keyring.gpg "$dist/InRelease"
gpgv --keyring ./keys/sovel-updates-archive-keyring.gpg "$dist/Release.gpg" "$dist/Release"

git add "$dist/Release" "$dist/InRelease" "$dist/Release.gpg"
git commit -m "Sign APT index for $(git rev-parse --abbrev-ref HEAD)" ${SOVEL_COMMIT_TRAILER:+-m "$SOVEL_COMMIT_TRAILER"}
git push origin HEAD
echo "Signed and pushed. Claude verifies the signature and publishes it to main."
