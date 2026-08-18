# remustwo implementation plan

Working spec synthesized from the three canvases (start plan, stack, lessons), plan audit (17 Aug 2026), and Qt/SQLite/CI research. Do not grow this into a docs zoo.

**Audit.** Pre-implementation review in canvas `remustwo-plan-audit` (17 Aug 2026). Amendments below are applied.

**Product.** C++20 / Qt 6 library + thin CLI that hashes a ROM, ingests a DAT, and names the file offline. GUI and converters are later adapters. remusone is a read-only reference.

**v1 (done) = phases 0–4.** Enrichment, GUI, converters are v1.1+.

## Constraints (from canvases)

| Source | Constraint |
|---|---|
| Start plan | New repo. Port library + tests. First slice: hash → ingest → match. |
| Stack | Stay Qt. Engines are not QObjects. No Gui on core/CLI. One catalog. |
| Lessons | README, `--help`, tests, and CI must tell the same story. No scripts-as-product. |
| This pass | No bloat: no artwork CAS, no FTS5 until GUI search, no committed DBs/DATs/ROMs, no 54 scripts, no archive TUI. |

## Researched stack (August 2026)

| Choice | Decision | Why |
|---|---|---|
| Language | C++20, CMake `CMAKE_CXX_STANDARD 20` | Qt 6 requires C++17 ([doc.qt.io](https://doc.qt.io/qt-6/cmake-get-started.html)). C++20 is free on gcc 13 (Ubuntu 24.04). No C++23, no modules. |
| Qt | **≥ 6.4** from distro packages | Ubuntu 24.04 apt is Qt **6.4.2** — that is the CI floor. Qt 6.8 is current LTS (6.8.8 commercial, Jun 2026) but pulling it via aqtinstall is hundreds of MB per CI run. Local newer Qt is fine. |
| Qt modules (v1) | Core + Sql; Concurrent from phase 1 | Network in phase 5. Quick/Qml in phase 6. Never Gui on the library or CLI. No qtkeychain until credentials exist. |
| Hashing | `QCryptographicHash` + zlib CRC32, one streaming pass | Qt 6 hashes from `QIODevice` in chunks. Do not add OpenSSL, BLAKE3, or xxHash — product hashes are CRC32/MD5/SHA1 for No-Intro/Redump, not throughput benchmarks. |
| Parallel scan | `QtConcurrent::mapped` | Already in Qt; use for directory hash when scan lands (phase 1/4). No new dep. |
| CLI parser | Tiny subcommand dispatcher + `QCommandLineParser` per command | Zero extra deps. CLI11 is the upgrade if the dispatcher grows; do not FetchContent it on day 1. |
| JSON output | `QJsonDocument` on stdout for `--json` | Already linked via Qt Core. Document schema per subcommand. No nlohmann. |
| Logging | `qCDebug` + logging categories | Port remusone `logging_categories.h` pattern. No spdlog. |
| Settings | `QSettings` under `GenericConfigLocation/remustwo` | Defer secrets to phase 5; env vars for CI. |
| DB | Qt Sql + SQLite, WAL, `foreign_keys=ON`, `busy_timeout` | DuckDB stays rejected. No SQLite BLOBs for images. |
| Build | CMake ≥ 3.22, Ninja, ccache, PCH, warnings **on** | Matches Ubuntu 24.04. No unity builds. No Vulkan stub. No qt6-svg workaround. |
| CI (phase 0–3) | `ubuntu-24.04`, apt `qt6-base-dev zlib1g-dev ninja-build clang-format` | One Debug job until the slice is green. Distro clang-format, not pinned `llvm.sh`. |
| CI (phase 4+) | add `libarchive-dev` | Do not install until organize/archive extract lands. |
| Packaging | none until a tarball exists | No AppImage, no scripts/ in the prefix, no version badge theater. |

## Lean tree (git)

Target clone: **source + tests + SQL, under a few MB.** Generated catalogs live in `~/.local/share/remustwo/`.

```text
LICENSE  README.md  AGENTS.md  CMakeLists.txt  .clang-format  .gitignore
.github/workflows/ci.yml          # one workflow
src/core/                         # hash, scan, DAT, match, disc-set, library DB
src/catalog/                      # compiler, merge, identity, catalog provider
src/cli/                          # subcommand adapters only
data/catalog/migrations|seeds|validation
testdata/fixture.dat              # tiny synthetic Logiqx DAT (<50 KB)
testdata/fixture.bin              # tiny synthetic ROM bytes matching fixture.dat (generated or committed)
tests/                            # Qt Test, generate extra bytes in /tmp as needed
```

**Never in git**

- `*.db`, `*.db-wal`, DAT dumps, libretro-thumbnails, remus-thumbnails, AppImages
- Copyrighted ROMs (hasher tests write synthetic files, as remusone already does)
- Prebuilt multi-GB `catalog.db` (copyright, stale immediately)
- `archive/`, `scripts/` zoo, `src/services/`, 114 markdown files
- FTS5 trigram index, materialized coverage tables, blob CAS — until a phase needs them

**Artwork policy (storage)**

remusone planned a tens-of-GB `remus-thumbnails` CAS. remustwo stores **HTTPS URLs or nothing** in the catalog. Downloads go to `~/.cache/remustwo/artwork/` on demand (phase 5+). Never SQLite BLOBs, never a second git-sized asset library.

## Two-database model

Two SQLite files, never merged:

| File | Role | Default path |
|---|---|---|
| `catalog.db` | Read-mostly reference data: sources, systems, games, signatures, facts, merge policy | `~/.local/share/remustwo/catalog.db` |
| `library.db` | User collection: scanned files, computed hashes, match rows | `~/.local/share/remustwo/library.db` |

**Contract.**

- Library match rows reference catalog `game_id` (FK or stored integer with validation at match time).
- Catalog is populated by `catalog ingest`; library is populated by `scan`.
- `match` reads catalog signatures, writes results to library.
- Do not `ATTACH` catalog into library for normal operations — open two connections or pass both handles to APIs that need both.
- No twin `localdatabase` DAT matcher. One catalog plane only.

## Catalog acquisition

| Audience | Path |
|---|---|
| CI / Quick Start | `testdata/fixture.dat` + `testdata/fixture.bin` — ingest in seconds |
| Real users | Download No-Intro/Redump DATs for their systems, run `remustwo catalog ingest path/to/system.dat` |
| Operator (hours) | Ingest many DATs locally; optional future downloadable release artifact — **not in git** |

`catalog init` creates schema + seeds only. It **cannot match** and must say so in `--help` and README. There is no `init_compendium.sh` bootstrap.

**Re-ingest.** `catalog ingest` is idempotent per source: re-ingesting the same DAT updates changed entries; port remusone compiler purge/incremental semantics in phase 2b.

## Migration policy

Ship numbered SQL under `data/catalog/migrations/` with `manifest.json` (ordered list, same pattern as remusone).

On `catalog::init`:

1. Open DB, apply pragmas (see below).
2. Read `PRAGMA user_version`.
3. Apply migrations from manifest in order, skipping already-applied versions.
4. Bump `user_version` after each migration.
5. Run seed SQL for systems/regions if empty.

Validation SQL in `data/catalog/validation/` runs in tests after ingest — not on every user `init` until a full catalog exists.

**No rollback in v1.** Forward-only migrations; restore from backup if needed.

## SQLite pragmas

Port remusone pragma helpers into the catalog layer at phase 2a (`compendium_sql_pragmas.h` equivalent):

- **Write path:** `journal_mode=WAL`, `busy_timeout=5000`, `foreign_keys=ON`, transactions use `BEGIN IMMEDIATE` for ingest.
- **Read path:** `query_only=ON` where applicable for match lookups.
- **Finalize:** checkpoint WAL after bulk ingest.

Without WAL + busy_timeout, catalog ingest will hit `SQLITE_BUSY` under concurrent readers.

## Error model

Port `result.h` in phase 0. Library APIs return `Result<T>` (value or `QString` error), not bool + side-effect logging. CLI maps errors to exit code 1 and a message on stderr.

## Confidence thresholds

One module owns match confidence thresholds during phase 1 port. Delete duplicate values (remusone had MatchingEngine Medium=70 vs constants MEDIUM=60). Tests assert the single source.

## Scan / organize security

Phase 1 scan and phase 4 organize must:

- Reject or skip symlinks outside the scan root (document policy: skip by default).
- Normalize paths; reject `..` components that escape the root.
- Never follow symlinks when writing in organize.

## README positioning

One paragraph in README:

- **remustwo** — offline SQLite catalog + library manager; hash, match, organize, enrich metadata.
- **igir** — collection 1G1R builder, copy/move, patch; complements remustwo but does not replace the catalog compiler.
- Do not shell igir from remustwo v1 — two bootstrap stories again.

## Catalog schema to port vs defer

Port in phase 2 (from remusone `data/compendium`, rename to `data/catalog`):

- `0001` canonical tables: sources, systems, regions, games, names, signatures, serials, facts, merge_policy
- `0003` libretro name (optional until frontend export)
- `0007` disc sets
- `0008` facts lookup index
- `0009` signature source key
- `0010` extended metadata — **nullable `cover_url` column only**; defer `game_assets` table to phase 5+; update merge SQL + test in same commit

Defer (bloat / not needed for match):

- `0002` patch catalog → phase 7
- `0004` FTS5 trigram → phase 6 (GUI search; use external-content FTS + triggers, not inline duplicate storage)
- `0006` achievement count → phase 5
- `0011` materialized coverage → when a full catalog exists
- `0012` blob_inventory / remus-thumbnails CAS → never as default

## Library API (the product)

```text
remustwo::hash_file(path) -> Result<HashResult>     # crc32, md5, sha1, raMd5, chdSha1
remustwo::catalog::init(db_path) -> Result<void>    # schema + migrations + seeds; cannot match
remustwo::catalog::ingest_dat(db, dat_path) -> Result<void>  # one source; idempotent
remustwo::match_file(catalog, path) -> Result<Match> # offline signatures only
remustwo::scan(library_db, root) -> Result<void>    # QtConcurrent for files
remustwo::organize(..., dry_run=true) -> Result<...>
```

Progress is `std::function<void(int percent)>`, not `Q_OBJECT`. Namespace `remustwo` from commit 1 (sed `Remus` → `remustwo` while copying).

## CLI (phase 3)

Non-interactive, layered `--help` with Examples, `--json` (QJsonDocument schema per command), progress on stderr, data on stdout, `--dry-run` on writes, idempotent ingest.

```text
remustwo hash PATH
remustwo scan PATH
remustwo catalog init [--db PATH]
remustwo catalog ingest DAT
remustwo match PATH
remustwo list
remustwo organize DEST [--dry-run]
remustwo verify
```

`catalog init` help text must say it cannot match. Default DB: `~/.local/share/remustwo/catalog.db` and `library.db`.

## Phases

### 0 — Bootstrap (1–2 days)

MIT license (remusone copyright retained on ported files). CMake project `remustwo` 0.1.0. Stub `remustwo` that prints help and exits 0. Port `result.h`; one `Result` test. Add `testdata/fixture.dat` and `testdata/fixture.bin` (synthetic bytes matching the DAT). CI: configure + build + ctest + clang-format — **no libarchive yet**. `AGENTS.md` = house rules. README only lists commands that exist; stub `--help` only until phase 3.

**Exit.** Fresh clone on Ubuntu 24.04-equivalent: build and test pass. Tree has no `scripts/`, no `archive/`.

### 1 — Kernel (3–5 days)

Copy from remusone **with tests**, strip `QObject`/`Q_OBJECT` as you go, drop `Qt6::Gui`:

- hasher, header_detector, ra_hasher, `chd_header` (no chdman)
- scanner, system_detector — **document symlink skip policy**
- dat_parser (Logiqx), matching_engine, title_similarity, match_utils — **unify confidence thresholds here**
- disc_title_parser, disc_set_key, disc_set_utils
- library SQLite (files/hashes/matches) — user library schema referencing catalog `game_id`
- system defs: copy `systems*.cpp` for now; phase 2 makes SQL seeds canonical
- `QtConcurrent::mapped` for parallel directory hash when scan API lands

Do not copy converters, organize, verification, bundler, patch, Qt Gui, services.

**Exit.** Hasher/DAT/match/disc-set tests green. Identify a synthetic file in-process (no CLI required).

### 2 — Catalog library (5–7 days, split sub-phases)

**2a — Schema + migrations (2 days)**

- Port lean migrations + `manifest.json` + migration runner + `user_version`
- Port SQLite pragma helpers
- `catalog::init` applies migrations and seeds

**2b — Compiler (2–3 days)**

- Port compiler, DAT extractor, fact inserter, disc-set inserter
- Idempotent re-ingest / incremental refresh semantics from remusone
- Fixture DAT in `testdata/`; ingest completes in seconds

**2c — Merge + provider (2 days)**

- Port identity linker, merge resolver, catalog provider
- **Fix `cover_url`:** nullable URL column only; merge SQL + migration + test in one commit
- **Systems dedupe milestone:** SQL seeds are canonical; trim `systems*.cpp` to lookup helpers only

**Exit.** In-process: init → ingest fixture → hash/match synthetic ROM. Merge/identity tests pass. No `localdatabase` provider.

### 3 — Thin CLI (3–4 days) → **0.1.0**

Wire the five commands. README Quick Start is exactly those commands. CI smoke step runs all five against `testdata/`:

```bash
remustwo catalog init
remustwo catalog ingest testdata/fixture.dat
remustwo hash testdata/fixture.bin
remustwo match testdata/fixture.bin
remustwo list
```

No second bootstrap.

**Exit.** Cold clone identifies the fixture. README, `--help`, tests, CI agree.

### 4 — Organize / verify (about a week) → **0.2.0 / v1**

Add `libarchive-dev` to CI. Port OrganizeEngine, TemplateEngine, VerificationEngine, archive extract, disc completeness, M3U. Test zip extract only; 7z edge cases may need external `7zz` fallback later. Still no GUI.

**Exit.** `organize --dry-run` and `verify` against the fixture catalog.

### 5 — Enrichment (1–2 weeks) → **0.3.0**

One provider implementation for ingest and runtime. Qt Network appears here. Online is `--online`. Default remains offline. Artwork = `cover_url` + optional cache dir, not a blob library. qtkeychain only if a provider requires a stored secret. Optional: Hasheous offline JSON import as an ingest source — not v1.

### 6 — GUI (after API is boring) → **0.4.0**

Qt Quick, in-process, no `QProcess`. CI adds `qt6-declarative-dev`. FTS5 only if search is slow without it — use **external-content FTS + triggers**; trigram extension only if substring search is required.

### 7 — Convert / patch / mods (last)

Optional tools; discover via `QStandardPaths::findExecutable`; tests skip if missing. Do not block matching on chdman.

## Port map (do / don't)

| remusone | remustwo |
|---|---|
| `src/core` hasher/DAT/match/disc-set | Port, drop QObject, no Gui |
| `src/core/result.h` | Port phase 0; use on all new APIs |
| `src/metadata` compiler/merge/orchestrator | `src/catalog`, one provider plane |
| `data/compendium` SQL + manifest | `data/catalog`, lean migrations |
| `compendium_sql_pragmas.h` | Port phase 2a |
| Focused unit tests | Copy beside the code |
| `src/cli` 18k + build-phases | Rewrite as adapters |
| `src/services`, `archive/`, `scripts/` | Leave |
| `src/gui` | Phase 6, rewrite controllers to call library |
| Converter tests | Phase 7, skip without tools |

## CI (v1)

One workflow, steps that matter:

1. `clang-format --dry-run` (apt package)
2. `cmake -G Ninja -DREMUSTWO_WARNINGS=ON` → build → `ctest --output-on-failure`
3. Phase 3+: smoke script — five CLI commands against `testdata/` (see phase 3)

Add ASan as a **second** job after phase 1 is green. No coverage floor. No shellcheck until there are scripts (prefer zero scripts). Pin GitHub Actions by SHA. No CodeQL until post-v1.

**Phase 4+:** add `libarchive-dev` to apt install line.

## House rules (AGENTS.md)

1. Library first. CLI/GUI are adapters.
2. One bootstrap: `catalog ingest`. Init cannot match, and says so.
3. Dead code never enters git.
4. Docs follow the binary. No “project complete” files.
5. CI is a gate from commit 1.
6. Tests travel with every ported file. Migration + merge SQL + test in one commit.
7. remusone is read-only. Copy, do not submodule.
8. No artwork blobs, no committed catalogs, no second offline database.
9. One confidence threshold source. One systems definition source (SQL after phase 2).

## First commands (definition of started)

```bash
cmake -S . -B build -G Ninja && cmake --build build
./build/remustwo catalog init
./build/remustwo catalog ingest testdata/fixture.dat
./build/remustwo hash testdata/fixture.bin
./build/remustwo match testdata/fixture.bin
```

## Post-v1 pipeline (enrich + bundle)

Required path is unchanged: `catalog init` → `catalog ingest` → `scan` → `match` → `list` / `verify` / `organize`. Optional work runs **after match** only.

- Persist `files.catalog_game_id` so library rows join catalog games. Match uses stored hashes; do not re-hash unless `hash_calculated` is false. Organize matched titles only; write M3U playlists for disc sets.
- `enrich [--online] [--dry-run]` is library-scoped. HTTP count must follow unmatched gaps in the library, not DAT size. Store HTTPS URLs in catalog.db; download bytes to `~/.cache/remustwo/artwork/` on GUI paint or `organize --include-art` cache hit. Negative-cache empty Hasheous results in library.db. `catalog enrich` is rejected.
- `organize DEST --bundle` writes zip + `.remus.md` (no network). `--convert auto` (default with `--bundle`) converts CD images to CHD, GC/Wii to RVZ, PSP ISO to CSO when tools exist; never zip-of-zip or a whole disc set into one archive.
- Artwork policy unchanged: no SQLite BLOBs, no remus-thumbnails CAS, no catalog-wide provider waterfall.
