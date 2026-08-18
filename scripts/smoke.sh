#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ROOT}/build/remustwo"
DB="${ROOT}/build/smoke-catalog.db"
LIB="${ROOT}/build/smoke-library.db"

rm -f "$DB" "${DB}-wal" "${DB}-shm" "$LIB" "${LIB}-wal" "${LIB}-shm"

"${BIN}" catalog init --db "$DB"
"${BIN}" catalog ingest --db "$DB" "${ROOT}/testdata/fixture.dat"
"${BIN}" scan "${ROOT}/testdata" --library-db "$LIB"
"${BIN}" match --catalog-db "$DB" --library-db "$LIB"
"${BIN}" list --library-db "$LIB"
"${BIN}" verify --catalog-db "$DB" --library-db "$LIB"
"${BIN}" organize "${ROOT}/build/smoke-organize" --dry-run --catalog-db "$DB" --library-db "$LIB"

ENRICH_JSON="$("${BIN}" enrich --catalog-db "$DB" --library-db "$LIB" --dry-run --json)"
echo "$ENRICH_JSON"
echo "$ENRICH_JSON" | grep -E '"would_fetch":[01]' >/dev/null

BUNDLE_JSON="$("${BIN}" organize "${ROOT}/build/smoke-bundle" --bundle --dry-run --catalog-db "$DB" --library-db "$LIB" --json)"
echo "$BUNDLE_JSON"
echo "$BUNDLE_JSON" | grep -F '.remus.md' >/dev/null
