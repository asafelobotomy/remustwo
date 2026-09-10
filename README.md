# remustwo

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/U5R225QZH3)

Offline SQLite catalog and library manager for ROM collections. C++20 / Qt 6.

**remustwo** hashes ROMs, builds an offline catalog from DAT files, and matches your library. **igir** is a complementary tool for 1G1R collection building — they work well together but serve different roles.

## Build

```bash
cmake -S . -B build -G Ninja -DREMUSTWO_ENABLE_WARNINGS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

libarchive is enabled by default when found (`libarchive-dev`) and is used for ROM archive scan, bundle zips, and Redump DAT extraction. Pass `-DREMUSTWO_ENABLE_LIBARCHIVE=OFF` only if you accept no archive support (Redump `catalog fetch` then needs `unzip` on PATH). GUI: `-DREMUSTWO_BUILD_GUI=ON` (requires `qt6-declarative-dev`).

Debian/Ubuntu build packages: `build-essential cmake ninja-build qt6-base-dev libqt6sql6-sqlite zlib1g-dev libarchive-dev` (+ `qt6-declarative-dev` for GUI).

Install to a prefix (system Qt stays on the machine; converters stay external):

```bash
cmake --install build --prefix "$HOME/.local"
# or a relocatable tree + tarball:
cmake --install build --prefix /tmp/remustwo-prefix
tar -C /tmp/remustwo-prefix -czf remustwo-0.5.0-linux.tar.gz .
```

`remustwo` and `remustwo-gui` share `~/.local/share/remustwo/{catalog,library}.db`. Catalog ingest stays CLI-only.

## Quick Start

Two databases: **catalog.db** (DAT reference) and **library.db** (your files). Init cannot match offline.

```bash
# 1. Catalog — schema, then DAT hashes
./build/remustwo catalog init --db /tmp/catalog.db

# Redump (automated download + ingest):
./build/remustwo catalog fetch --system PlayStation --db /tmp/catalog.db
# or a Redump slug: --system psx

# No-Intro (manual download from Datomatic — captchas block automation):
#   place *.dat files in a folder, then:
./build/remustwo catalog fetch --from-dir ~/dats/no-intro --db /tmp/catalog.db
# or: catalog ingest PATH for a single DAT

# 2. Library — find ROMs, then identify them
./build/remustwo scan testdata --library-db /tmp/library.db
./build/remustwo match --catalog-db /tmp/catalog.db --library-db /tmp/library.db

# Optional: identify unmatched dumps via Hasheous (online hash identity)
./build/remustwo match --online --catalog-db /tmp/catalog.db --library-db /tmp/library.db

# 3. Inspect and act
./build/remustwo list --library-db /tmp/library.db
./build/remustwo verify --catalog-db /tmp/catalog.db --library-db /tmp/library.db
./build/remustwo organize /tmp/roms --dry-run --catalog-db /tmp/catalog.db --library-db /tmp/library.db
./build/remustwo organize undo --library-db /tmp/library.db
```

Optional after match: `enrich --dry-run` (matched games only; `--online` for live Hasheous). Offline enrich synthesizes Libretro boxart/snap/title/logo URLs into `game_assets` (and `cover_url` for boxart). Online enrich also fills description / developer / publisher / release_date from Hasheous and HEAD-probes Libretro candidates before storing. With `--deep --online`, remustwo calls Hasheous’s IGDB MetadataProxy for genre, rating, hero/banner art, and IGDB screenshots when a client API key is set (`hasheous/client_api_key` in QSettings or `REMUSTWO_HASHEOUS_API_KEY`). `organize DEST --bundle` writes zip + `.remus.md` (add `--include-art` to copy a cached cover; `--online` downloads covers; `--convert auto` for CHD/RVZ/CSO when tools exist). `catalog import-hasheous JSON` is the offline cover/description/metadata import. `patch apply BASE PATCH` applies IPS (built-in), BPS/UPS (needs `flips`), XDelta (`xdelta3`), or PPF (`applyppf` / `ppf3`).

`match FILE` identifies one ROM. `match` with no file identifies every scanned file. `match --online` calls Hasheous only for files that miss local DAT signatures and upserts a provisional catalog entry. `hash PATH` only prints checksums; it does not write a database.

`--db` and `--catalog-db` are the same catalog path. `--library-db` is always the library. `scan --no-archives` skips zip/7z/rar members.

Redump DATs cache under `~/.local/share/remustwo/dats/`. Put local dumps in `testroms/` (gitignored). Never commit game files or DAT blobs; `testdata/fixture.bin` is a 276-byte synthetic vector for CI.

Optional converters (skip if missing): `chdman` (`mame-tools`; Debian often puts it in `/usr/games`), `dolphin-tool` (Dolphin .deb, Flatpak, or Snap), `maxcso` for PSP ISO→CSO, and `wit` for Wii WBFS. Patch tools: `flips`, `xdelta3`, `applyppf`/`ppf3`. RetroAchievements digests use built-in hashing where possible; some systems need an optional `RAHasher` / `rahasher` binary on PATH. remustwo searches `REMUSTWO_TOOL_PATH`, `~/.local/bin`, PATH, `/usr/games`, Homebrew/Nix/Guix, Snap aliases, and Flatpak app binaries (no manual wrapper). A local [remustwo-maxcso](https://github.com/asafelobotomy/remustwo-maxcso) tree at `../remustwo-maxcso` is also picked up; `make PREFIX="$HOME/.local" install` is still the durable install.

## Status

v0.5.0 — CLI (hash, catalog init/ingest/fetch, match [--online], list, organize/undo, verify, enrich, patch apply), archive scan when libarchive is present, `organize --bundle`, Hasheous JSON import + online identity, Qt Quick GUI (`remustwo-gui`), converter discovery for .deb/Flatpak/Snap.
