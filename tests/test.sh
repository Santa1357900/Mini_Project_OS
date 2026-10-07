#!/bin/sh
set -eu
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/dir one/sub" "$tmp/other"
mkdir "$tmp/report-dir"
touch "$tmp/dir one/Report.txt" "$tmp/dir one/sub/report.md" "$tmp/other/note.txt"
ln -s "$tmp/dir one" "$tmp/other/link"
./psearch -j 1 -i "$tmp" report | sort > "$tmp/one"
./psearch -j 4 -i "$tmp" report | sort > "$tmp/four"
cmp "$tmp/one" "$tmp/four"
[ "$(wc -l < "$tmp/four")" -eq 3 ]
./psearch -j 2 "$tmp" Report > "$tmp/case"
[ "$(wc -l < "$tmp/case")" -eq 1 ]
./psearch -f -i "$tmp" report > "$tmp/files"
[ "$(wc -l < "$tmp/files")" -eq 2 ]
if ./psearch -j 0 "$tmp" report >/dev/null 2>&1; then exit 1; fi
echo "All tests passed"
