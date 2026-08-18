# remustwo

Offline SQLite catalog and library manager for ROM collections. C++20 / Qt 6.

**remustwo** hashes ROMs, builds an offline catalog from DAT files, and matches your library. **igir** is a complementary tool for 1G1R collection building — they work well together but serve different roles.

## Build

```bash
cmake -S . -B build -G Ninja -DREMUSTWO_ENABLE_WARNINGS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Quick Start

```bash
./build/remustwo catalog init --db /tmp/catalog.db
./build/remustwo catalog ingest --db /tmp/catalog.db testdata/fixture.dat
./build/remustwo hash testdata/fixture.bin
./build/remustwo match --catalog-db /tmp/catalog.db testdata/fixture.bin
./build/remustwo scan testdata --library-db /tmp/library.db
./build/remustwo list --library-db /tmp/library.db
./build/remustwo verify --catalog-db /tmp/catalog.db
./build/remustwo organize /tmp/roms --dry-run --catalog-db /tmp/catalog.db
```

`catalog init` creates schema and seeds only — it **cannot match** until you ingest a DAT.

Real use: download No-Intro/Redump DATs for your systems, then run `catalog ingest` for each.

Build with libarchive for archive extraction: `-DREMUSTWO_ENABLE_LIBARCHIVE=ON` (requires `libarchive-dev`).

## Status

v0.4.0 — CLI (hash, catalog, match, list, organize, verify, enrich), Hasheous JSON import, Qt Quick GUI (`remustwo-gui`), CHD/patch tooling with external-tool discovery.
