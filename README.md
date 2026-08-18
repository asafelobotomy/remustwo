# remustwo

Offline SQLite catalog and library manager for ROM collections. C++20 / Qt 6.

**remustwo** hashes ROMs, builds an offline catalog from DAT files, and matches your library. **igir** is a complementary tool for 1G1R collection building — they work well together but serve different roles.

## Build

```bash
cmake -S . -B build -G Ninja -DREMUSTWO_ENABLE_WARNINGS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Install to a prefix (system Qt stays on the machine; converters stay external):

```bash
cmake --install build --prefix "$HOME/.local"
# or a relocatable tree + tarball:
cmake --install build --prefix /tmp/remustwo-prefix
tar -C /tmp/remustwo-prefix -czf remustwo-0.5.0-linux.tar.gz .
```

`remustwo` and `remustwo-gui` share `~/.local/share/remustwo/{catalog,library}.db`. Catalog ingest stays CLI-only.

## Quick Start

Two databases: **catalog.db** (DAT reference) and **library.db** (your files). Init cannot match.

```bash
# 1. Catalog — schema, then DAT hashes
./build/remustwo catalog init --db /tmp/catalog.db
./build/remustwo catalog ingest --db /tmp/catalog.db testdata/fixture.dat

# 2. Library — find ROMs, then identify them
./build/remustwo scan testdata --library-db /tmp/library.db
./build/remustwo match --catalog-db /tmp/catalog.db --library-db /tmp/library.db

# 3. Inspect and act
./build/remustwo list --library-db /tmp/library.db
./build/remustwo verify --catalog-db /tmp/catalog.db --library-db /tmp/library.db
./build/remustwo organize /tmp/roms --dry-run --catalog-db /tmp/catalog.db --library-db /tmp/library.db
```

Optional after match: `enrich --dry-run` (matched games only; `--online` for live Hasheous). `organize DEST --bundle` writes zip + `.remus.md` (add `--include-art` to copy a cached cover; `--convert auto` for CHD/RVZ/CSO when tools exist). `catalog import-hasheous JSON` is the offline cover/description import.

`match FILE` identifies one ROM. `match` with no file identifies every scanned file. `hash PATH` only prints checksums; it does not write a database.

`--db` and `--catalog-db` are the same catalog path. `--library-db` is always the library.

Real use: download No-Intro/Redump DATs for your systems, then `catalog ingest` each one before scan/match.

Build with libarchive for archive extraction and zip bundles: `-DREMUSTWO_ENABLE_LIBARCHIVE=ON` (requires `libarchive-dev`). GUI: `-DREMUSTWO_BUILD_GUI=ON` (requires `qt6-declarative-dev`).

Optional converters (skip if missing): `chdman` (`mame-tools`; Debian often puts it in `/usr/games`), `dolphin-tool` (Dolphin .deb, Flatpak, or Snap), `maxcso` for PSP ISO→CSO, plus `wit` and `PSXPackager` when present. remustwo searches `REMUSTWO_TOOL_PATH`, `~/.local/bin`, PATH, `/usr/games`, Homebrew/Nix/Guix, Snap aliases, and Flatpak app binaries (no manual wrapper). A local [remustwo-maxcso](https://github.com/asafelobotomy/remustwo-maxcso) tree at `../remustwo-maxcso` is also picked up; `make PREFIX="$HOME/.local" install` is still the durable install.

## Status

v0.5.0 — CLI (hash, catalog, match, list, organize, verify, enrich), `organize --bundle`, Hasheous JSON import, Qt Quick GUI (`remustwo-gui`), converter discovery for .deb/Flatpak/Snap.
