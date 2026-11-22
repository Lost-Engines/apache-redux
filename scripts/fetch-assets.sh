#!/bin/bash
# Download and extract the reference data-only CD. No DOS installer is executed.
if [ "$#" -gt 1 ] || [ "${1:-}" = --help ]; then
  echo "Usage: $0 [destination]"
  echo 'APACHE_EXECUTABLE selects the built Apache executable when necessary.'
  exit 0
fi
script_dir=$(cd -- "$(dirname -- "$0")" && pwd) || exit 1
binary=${APACHE_EXECUTABLE:-}
if [ -z "$binary" ]; then
  for candidate in "$script_dir/../apache" "$script_dir/../build/apache"; do
    if [ -x "$candidate" ]; then
      binary="$candidate"
      break
    fi
  done
fi
if [ -z "$binary" ] || [ ! -x "$binary" ]; then
  echo 'Build Apache first, or set APACHE_EXECUTABLE to its executable.' >&2
  exit 1
fi
destination=${1:-$("$binary" --asset-directory)} || exit 1
if "$binary" --check-assets --data-dir "$destination" >/dev/null 2>&1; then
  echo "Game resources are already installed in $destination"
  exit 0
fi
download=$(mktemp "${TMPDIR:-/tmp}/apache-cd.XXXXXXXX") || exit 1
trap 'rm -f -- "$download"' EXIT
url='https://archive.org/download/redump-id-91728/Apache%20Longbow%20%28Europe%29%20%28Rerelease%29%20%2820001017%29.bin'
echo 'Downloading the Apache Longbow reference CD (358 MiB)...'
curl --user-agent 'Lost Engines Apache CPP' --fail --location --retry 3 --retry-all-errors --connect-timeout 30 --output "$download" "$url" || exit 1
expected=1a21c143c52c4cbad446ecaf4ebb5429307736b4061f20c3abe13128ee8e374a
if command -v sha256sum >/dev/null 2>&1; then
  actual=$(sha256sum "$download") || exit 1
else
  actual=$(shasum -a 256 "$download") || exit 1
fi
actual=${actual%% *}
if [ "$actual" != "$expected" ]; then
  echo 'The CD checksum does not match. Please retry.' >&2
  exit 1
fi
"$binary" --install-cd "$download" --data-dir "$destination" || exit 1
"$binary" --check-assets --data-dir "$destination" || exit 1
echo 'Ready.'
