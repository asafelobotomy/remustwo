#!/usr/bin/env bash
# Local-only: scan gitignored testroms/. CI never runs this.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIR="${ROOT}/testroms"
BIN="${ROOT}/build/remustwo"

if ! find "$DIR" -maxdepth 1 -type f ! -name README.md -size +0 2>/dev/null | grep -q .; then
    echo "testroms/ is empty; skipping local dump scan"
    exit 0
fi

LIB="${ROOT}/build/smoke-testroms-library.db"
rm -f "$LIB" "${LIB}-wal" "${LIB}-shm"
"${BIN}" scan "$DIR" --library-db "$LIB"
