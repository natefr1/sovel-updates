#!/bin/bash
set -euo pipefail
test "$(id -u)" = 0 || { echo 'Run as root.' >&2; exit 1; }
build_dir=${1:?Provide an absolute, dedicated image-build directory}
resume=${2:-}
[[ -z "$resume" || "$resume" = --resume-downloads ]] || exit 2
[[ "$build_dir" = /* && "$build_dir" != / ]] || exit 2
mkdir -p "$build_dir"
build_dir=$(realpath "$build_dir")
case "$build_dir" in */iso-build) ;; *) echo 'Build directory must end in /iso-build.' >&2; exit 2;; esac
root="$build_dir/rootfs"
if [[ -f "$build_dir/bootstrap.complete" ]]; then
    test -f "$root/etc/debian_version"
    echo 'Verified existing bootstrap stage.'
    exit 0
fi
if [[ -d "$root" && -n "$(ls -A "$root")" ]]; then
    if [[ "$resume" = --resume-downloads && -f "$root/debootstrap/debpaths" && ! -e "$root/var/lib/dpkg/status" && ! -e "$root/bin/sh" ]]; then
        echo 'Resuming the verified download-only bootstrap stage.'
    else
        echo 'A partial root exists outside the supported download-only recovery stage. Inspect it before continuing; nothing was deleted.' >&2
        exit 3
    fi
fi
available_kib=$(df -Pk "$build_dir" | awk 'END {print $4}')
(( available_kib >= 8*1024*1024 )) || { echo 'At least 8 GiB of guest free space is required.' >&2; exit 4; }
command -v debootstrap >/dev/null
test -f /usr/share/keyrings/ubuntu-archive-keyring.gpg
debootstrap --arch=amd64 --variant=minbase --components=main,restricted,universe,multiverse \
    --keyring=/usr/share/keyrings/ubuntu-archive-keyring.gpg \
    resolute "$root" https://archive.ubuntu.com/ubuntu
touch "$build_dir/bootstrap.complete"
echo 'Clean Ubuntu root bootstrap complete.'
