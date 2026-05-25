#!/usr/bin/env bash
set -euo pipefail

rm -rf deps
mkdir -p deps

find .. \
  -path ../docker -prune -o \
  -name package.xml -type f \
  -exec sh -c '
    for file do
      rel="${file#../}"
      pkgdir="$(dirname "$rel")"

      mkdir -p "deps/$pkgdir"
      cp "$file" "deps/$pkgdir/package.xml"
    done
  ' sh {} +
