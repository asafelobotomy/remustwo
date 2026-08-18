#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ROOT}/build/remustwo"
DB="${ROOT}/build/smoke-catalog.db"
LIB="${ROOT}/build/smoke-library.db"

rm -f "$DB" "${DB}-wal" "${DB}-shm" "$LIB" "${LIB}-wal" "${LIB}-shm"

"${BIN}" catalog init --db "$DB"
"${BIN}" catalog ingest --db "$DB" "${ROOT}/testdata/fixture.dat"
"${BIN}" hash "${ROOT}/testdata/fixture.bin"
"${BIN}" match --catalog-db "$DB" --library-db "$LIB" "${ROOT}/testdata/fixture.bin"
"${BIN}" list --library-db "$LIB"
"${BIN}" verify --catalog-db "$DB" --library-db "$LIB"
"${BIN}" organize "${ROOT}/build/smoke-organize" --dry-run --catalog-db "$DB" --library-db "$LIB"
