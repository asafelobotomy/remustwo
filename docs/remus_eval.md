# Remusone evaluation (historical)

> **Status:** planning archaeology from the remusone → remustwo port. Not the current product truth.
> Authoritative surface: `README.md`, CLI `--help`, and the binary under test.

Source: [github.com/asafelobotomy/remus](https://github.com/asafelobotomy/remus) at `0aae9fe` (2026-07-06), clone `/tmp/remus-eval`, CI run [28793418463](https://github.com/asafelobotomy/remus/actions/runs/28793418463). Reviewed 17 Aug 2026.

**Naming.** The existing product is **remusone**. This repo is **remustwo**, a continuation that ports the library and leaves the remusone tree behind.

## Verdict

Start remustwo as a new repo. Port `src/core`, `src/metadata`, `data/compendium`, and the unit tests that pin them. Do not rewrite DAT ingest, hashing, CHD/RVZ content SHA1, disc-set topology, or merge policy. Do not keep building on remusone/main.

Remusone is a real ROM manager with expensive, working catalog logic. The GitHub tree is not a product you can continue: main has been CI-red since 19 Jun 2026, the CLI owns the compiler, docs contradict the binary, and a 27k-line archived TUI plus 114 markdown files will keep eating every future agent session.

"Continue remusone and extract the library" is the same work as a new repo, except the archive, script zoo, and stale docs stay in every checkout. There is no GitHub URL or release worth preserving: the only tag is `v0.11.0` with empty assets; HEAD advertises `0.12.0`.

A blank-slate rewrite would re-learn No-Intro, Redump, CHD v5, RetroAchievements digests, and disc-set completeness. That cost is already paid in remusone tests. Pay it again only if the goal is to abandon the domain.

## What remusone actually is

C++17 / Qt 6 CLI + Qt Quick GUI for scanning, hashing, matching, organizing, converting, and patching ROM libraries. Created 16 Feb 2026. Last product commit 6 Jul 2026. Almost every commit is Cursor-coauthored.

| Tree | Approx. lines | Role |
|---|---|---|
| `src/` | 74k | Production code |
| `tests/` | 28k | Mostly real unit coverage of core/metadata |
| `docs/` | 32k | Mix of current surface and celebration/archive |
| `archive/gui-tui/` | 27k | Dead TUI, not in CMake, still in git |
| `scripts/` | 8k | 54 shell files, two competing bootstraps |

Production layers (`src/` C++/QML):

| Layer | LOC | Keep? |
|---|---|---|
| core | 22,265 | Yes — hasher, DAT, CHD header SHA1, disc sets, organize/verify |
| cli | 18,529 | No as-is — owns the compiler; rebuild thin |
| metadata | 14,972 | Yes — orchestrator, merge, identity, DAT ingest |
| gui | 14,172 | Later, only after a real library API |
| services | 4,264 | No — TUI-era wrappers; GUI does not use `MatchService` |

## Why remusone/main is not the base

| Signal | Evidence | Why it matters |
|---|---|---|
| main is red | Last green CI 19 Jun 2026. HEAD: 9 failed tests, shellcheck, clang-format pin, fixture ingest | Every feature lands on a broken gate |
| Schema drift | `CompendiumMergeResolverTest`: `no such column: cover_url` | Catalog correctness is the product; migrations and merge SQL diverged |
| CLI owns the compiler | `src/cli` 18.5k vs `src/services` 4.3k. Build-phases file is 1,529 lines | GUI shells `remus-cli` via `QProcess`. The shared API is leftover from the archived TUI |
| Docs lie | README Quick Start runs `setup_compendium_db.sh` (schema only). Matching needs `init_compendium.sh` (hours) | A cold clone cannot identify ROMs. Agents follow the wrong bootstrap |
| Not installable | `v0.11.0` assets: `[]`. Tree is `0.12.0` plus a large Unreleased batch | There is no product identity to continue |
| Agent shell | 114 markdown files, 54 shell scripts, 27k-line archive still in git | Context-window tax. Future sessions re-read stale audits |

HEAD Debug job: 97/106 tests passed. Failures to treat as **out of scope for the first remustwo cut**: converter/tool tests (`ChdConverter`, `CsoConverter`, `DiscConverter`, `WbfsConverter`, `PBPExporter`), plus `ArtworkDownloaderTest`. Failures to **fix during the catalog port**: `CompendiumMergeResolverTest` (`cover_url`), `CompendiumBuildPlanTest`, `ConstantsTest` (provider registry).

Also red on the same run: lint (pinned `llvm.sh` checksum), shellcheck, sanitizer (same tests), coverage (tests fail first), `compendium-fixture-build`. CodeQL still passes on a schedule.

## What to port vs leave

**Port with tests**

- `src/core` — hasher (CRC32/MD5/SHA1, header strip, RA digest, CHD/RVZ content SHA1), DAT parsers, disc-set keys, organize/verify engines, 100+ system defs
- `src/metadata` — provider orchestrator, merge resolver, identity linker, `CompendiumCompilerService` (DAT ingest)
- `data/compendium` — migrations, seeds, validation SQL. Offline-first model
- Focused unit tests: hasher, DAT, disc-set, merge, orchestrator, identity linker
- Qt 6 + SQLite stack. MIT license and copyright

**Leave in remusone**

- `archive/gui-tui/`
- Most of `docs/reports` and `docs/archive`
- Script zoo (54 `.sh` files, two bootstraps)
- CLI-owned enrichment files and the vestigial services layer
- GUI until the library API exists (remusone GUI is a process wrapper for catalog build)
- Converter/patch/mod workflows until scan/hash/match is a green product

## House rules remustwo inherits

1. **Library first.** Scan, hash, match, ingest, and merge live in a library. CLI and GUI are thin callers. Never put a 1,500-line compiler pipeline in the CLI.
2. **One bootstrap story.** Schema seed and full catalog build are different commands. README Quick Start must produce a matcher that works, or say that offline match needs a built catalog.
3. **Dead code never enters the working tree.** History can keep the TUI in remusone. remustwo git should not.
4. **Docs follow the binary.** No milestone reports, "project complete" summaries, or audits that claim green CI while main is red.
5. **CI is a gate.** Do not lower coverage to match the floor. Do not pin clang-format installers to a checksum that bit-rots. Fix tests when migrations add columns. Main is green from commit 1.
6. **Keep C++/Qt.** Hashing, SQLite, and a desktop GUI fit this stack. A Python rewrite would be slower; a Rust rewrite would redo DAT/SQL/QML for no product gain.
7. **Tests travel with every file copied from remusone.** A port without the pinning tests will rot immediately.
8. **remusone is read-only.** Clone or add a git remote. Do not submodule the whole tree. Cherry-pick, do not vendor the mess.

---

# How to start remustwo

The first product is not a GUI, not converters, and not a full No-Intro ingest. It is a library that can hash a ROM, ingest a small DAT, and name the file, behind a thin CLI, with green CI.

## Phase 0 — Repo that can stay green (1–2 days)

Empty tree becomes a real C++17 / Qt 6 project.

- MIT license (same as remusone)
- CMake: `remustwo` 0.1.0, warnings on by default, ccache optional
- Libraries: `remustwo-core`, later `remustwo-metadata`. No `services` target
- Binary: `remustwo` (CLI only). No GUI target
- CI on Ubuntu: configure, build, `ctest`, distro `clang-format`. No coverage floor, no pinned `llvm.sh`
- `AGENTS.md` with the house rules above
- README that states the first slice (hash + ingest + match) and that a schema-only catalog cannot identify ROMs
- Document remusone as a read-only reference at a local path / GitHub URL. Do not copy the tree

**Exit.** `cmake --build` and `ctest` pass on a fresh clone. README matches the commands that exist.

## Phase 1 — Kernel library (3–5 days)

Copy from remusone **with tests**, in this order. Keep the `Remus` namespace until the slice is green; rename in a dedicated pass.

1. Constants / system defs
2. `Hasher`, `HeaderDetector`, `RaHasher`
3. `Scanner`, `SystemDetector`
4. Logiqx `DatParser`, `MatchingEngine`, title similarity
5. Library SQLite (files / hashes / matches) — not the full catalog
6. Disc-title parser and disc-set keys (needed before multi-file matching lies)

Also port `chd_header.cpp` (v5 content SHA1 without spawning chdman). Do **not** port CHD/CSO/WBFS/PBP converters or their failing tests.

**Exit.** Unit tests for hasher, DAT, matching, header strip, and disc-set keys pass in remustwo CI.

## Phase 2 — Catalog as a library (5–7 days)

This is the expensive part remusone already paid for.

- Copy `data/compendium` migrations, seeds, validation SQL
- Copy catalog compiler, DAT extractor, identity linker, merge resolver, fact inserter, disc-set inserter
- Match via catalog hash signatures (CompendiumProvider was later removed as unused)
- Fix `cover_url` schema drift here, while the merge test is in the same commit as the migration
- Library API, not scripts:
  - `catalog.init()` — schema + seeds (cannot match)
  - `catalog.ingest(dat)` — one source, fixture-sized, used in tests and README
  - `catalog.build(manifest)` — full pipeline, later

Do not copy `cli_compendium_build_phases.cpp` or `scripts/init_compendium.sh`. Re-host the compiler in metadata. Fixture DAT ingest must finish in seconds; the hours-long full catalog is an operator command, not Quick Start.

**Exit.** Fixture DAT ingest + hash match works in-process. Merge/identity/compiler tests pass. Schema-only vs ingested catalog is obvious in `--help` and README.

## Phase 3 — Thin CLI (3–4 days)

Subcommands, not an 80-flag bag. Non-interactive, `--help` with examples, `--json`, `--dry-run` on writes. Progress on stderr, data on stdout. CLI links `QCoreApplication` only — no `Qt6::Gui`.

```text
remustwo hash PATH
remustwo scan PATH
remustwo catalog init
remustwo catalog ingest DAT
remustwo match PATH
remustwo list
remustwo organize --dry-run DEST
remustwo verify
```

**Exit.** README Quick Start is those commands against `testdata/`. A cold clone can identify the fixture ROM. No second bootstrap script.

## Phase 4 — Organize, verify, archives (about a week)

Port `OrganizeEngine`, `TemplateEngine`, `VerificationEngine`, archive extract, catalog-ordered M3U, disc-set completeness. These engines already exist in remusone core and are mostly well tested.

**Exit.** Dry-run organize + verify-set against the fixture catalog. Still no GUI.

## Phase 5 — Enrichment in the library (1–2 weeks)

Runtime `ProviderOrchestrator` already lives in metadata. Build-time bulk enrichment in remusone lives in the CLI (`compendium_enrichment_*.cpp`). Move that beside the compiler so one provider implementation serves ingest and runtime. Online fallback is opt-in (`--online-fallback`), default remains offline-first.

**Exit.** `catalog ingest` can merge Hasheous/IGDB/RA facts without CLI-owned SQL. CLI stays a caller.

## Phase 6 — GUI (only after the library API is stable)

Qt Quick that calls the library in-process. Catalog build uses the existing compiler progress callback — never `QProcess` to `remustwo`. Skip QML until scan/hash/match/organize are boring.

## Phase 7 — Converters, patches, mods (last)

External tools (chdman, dolphin-tool, and so on) are optional. remusone's red converter tests are the reason this is last, not first. Do not block matching on a 300s CHD timeout.

## First vertical slice (definition of done for "remustwo has started")

On a cold clone, with no remusone scripts:

```bash
cmake -S . -B build && cmake --build build -j"$(nproc)"
./build/remustwo hash testdata/fixture.nes
./build/remustwo catalog init
./build/remustwo catalog ingest testdata/fixture.dat
./build/remustwo match testdata/fixture.nes
```

CI is green. README lists exactly those commands. remusone is not in the working tree.

## What not to do in week 1

- Copy remusone wholesale, then "clean it"
- Add a GUI target
- Add `src/services`
- Import `archive/`
- Import the 54 scripts or 114 markdown files
- Import CLI enrichment / build-phases
- Import converter tests that already fail on remusone HEAD
- Write a coverage threshold to match today's floor
- Submodule remusone

---

# Stack decision (no code)

remusone GitHub repo archived 17 Aug 2026: https://github.com/asafelobotomy/remus (`gh repo archive asafelobotomy/remus --yes`). HEAD still `0aae9fe`. Unarchive later with `gh repo unarchive asafelobotomy/remus` if needed.

## Verdict

Do not change language. Do not reinvent hashing, DAT ingest, disc-set topology, merge policy, or the SQLite catalog. remustwo should stay **C++17 / Qt 6 Core + Sql**, with a **library-shaped refactor**: engines are not QObjects, core does not link Qt Gui, CLI is the first UI so the GUI toolkit is swappable later.

A new language looks attractive because remusone's C++ is messy. The mess is architecture (CLI-owned compiler, QObject-everywhere, Qt-as-stdlib, dual provider planes), not a missing language feature. 352 of 364 `src/` files use `QString`; **zero** use `std::string`. A Rust/Go/Python/C# port is a rewrite of the type system plus a rewrite of the domain tests. remusone already paid for the domain tests.

## Coupling (why a language hop is a rewrite)

| Layer | Files | QString | QObject | QSql | std::string |
|---|---|---|---|---|---|
| core | 135 | 96% | 33% | 18% | 0% |
| metadata | 89 | 98% | 42% | 30% | 0% |
| cli | 71 | 97% | 10% | 52% | 0% |
| gui | 43 | 95% | 88% | 21% | 0% |
| services | 26 | 96% | 50% | 12% | 0% |

22 core headers make engines `QObject` (Hasher, DatParser, Scanner, Database, MatchingEngine, converters, …) mostly for a progress signal. `remus-core` links `Qt6::Gui` with **no** `QImage`/`QPixmap` includes in core. Gui belongs in artwork/CLI/GUI only (`artwork_downloader.cpp`, `terminal_image.h`). `matching_engine.h` includes `metadata_provider.h` — layering inversion, not a language problem.

Qt is the standard library here: strings, lists, files, XML (`QXmlStreamReader` for DATs), JSON, SQL, HTTP, thread pool. Keep that for the port. Stop using it as an object model for pure functions.

## Language options

| Stack | Fit for this product | Cost vs remusone | Verdict |
|---|---|---|---|
| C++17 / Qt 6, library refactor | Desktop ROM manager + SQLite + concurrent hash + later GUI | Days–weeks to port core/metadata/tests | **Do this** |
| C++ without Qt (std::string, sqlite3, pugixml) | Same product, two string worlds at a Qt GUI boundary | Mechanical rewrite of 37k library lines | No. Gain is FFI purity you do not need yet |
| Rust (rusqlite + CLI, Slint/egui later) | Excellent hasher/parser/SQL. Weakest at “desktop manager this year” | Re-encode every DAT/hash/disc-set test | No for remustwo v0. If you ever extract a hasher crate, that is a different repo |
| Go | Good CLI + SQL. Desktop GUI is the hole | Full rewrite | No |
| Python | Fine for catalog *scripts*; slow on multi-GB CHD/RVZ hash | RomM already occupies this niche | No. remusone already suffered from scripts-as-product |
| C# / Avalonia | Fine desktop; LaunchBox/RomVault world | Full rewrite; Linux is your machine | No |
| TypeScript (igir-shaped) | DAT 1G1R organize, not an offline catalog + desktop manager | Wrong product | Do not replace remusone with igir; do not reinvent igir either |

Two languages in one repo on day 1 (Rust hasher + Qt GUI, Python ingest + C++ match) would recreate remusone's schema drift, across an FFI boundary.

## What to reinvent vs port vs leave

**Port (domain). Do not reinvent.**

- Hash semantics: CRC32/MD5/SHA1, header strip, RA digest, CHD v5 content SHA1 without chdman, RVZ content SHA1
- Logiqx + ClrMamePro DAT dialects
- Disc-set keys, completeness, catalog-ordered M3U
- Identity linker + merge policy + fact insert
- System matrix (100+)
- Offline-first SQLite catalog schema
- Organize templates / verification engines
- MIT license and tests that pin the above

**Refactor (architecture). This is the stack change.**

- Engines as values + callbacks, not `QObject` / `Q_OBJECT`
- `Qt6::Core` + `Qt6::Sql` on the library; **no** `Qt6::Gui` on core/CLI
- Catalog compiler in metadata, not CLI
- One provider implementation for ingest and runtime
- Subcommand CLI; JSON on stdout
- GUI later, in-process, so toolkit (QML vs something else) can change without another 37k rewrite
- Converters stay thin wrappers around chdman/dolphin-tool/maxcso/wit — do not reimplement CHD

**Leave in remusone. Do not port and do not reinvent.**

- Archived TUI, script zoo, services layer, QProcess catalog wizard, coverage-floor CI, terminal half-block art in the CLI

**Defer, do not decide now.**

- QML vs another GUI toolkit — decide at phase 6, after the library API exists
- Rust hasher crate — only if remustwo's C++ hasher becomes a product other tools want, not as a prerequisite

---

# Lessons learned

Full report: canvas `remusone-lessons-learned`. Root lesson: if README, `--help`, tests, and CI disagree, the feature is not done.

**Worth following:** hash-first identity (file hash ≠ content hash ≠ RA digest); offline-first SQLite catalog (DuckDB correctly deferred); schema as migrations/seeds/validation SQL; DAT ingest + identity/merge; disc-set topology; focused unit tests; wrap chdman rather than reimplement CHD; `--dry-run` on writes.

**Failed or never worked:** main as a gate (red since 19 Jun); merge SQL vs `cover_url` migrations; `MatchService` as the shared API (GUI never calls it); GUI catalog via `QProcess` + bash; converter “unit” tests without tools; README Quick Start that cannot match; `ConstantsTest` freezing `PROVIDER_REGISTRY.size() == 10` (registry has 11); empty `v0.11.0` release.

**Overengineered:** 400-line file splits that did not extract modules; constants library while `MatchingEngine` keeps different Low/Medium numbers; 54 scripts hiding a missing library API; three workflow implementations; runtime providers plus CLI enrichment SQL; CI inventory (informational qml-lint/clang-tidy, coverage threshold set to today’s 54–55% floor); 114 markdown files including “Project Complete.”

**Assumed or conflicting:** schema-only DB vs matcher; ScreenScraper waterfall comments vs compendium-only default; `localdatabase` and compendium as twin offline stores; min-confidence 60 in code, 70 in README, 50/70 in CLI docs; CONTRIBUTING coverage 50% vs script 54%; changelog “CLI-only” catalog vs GUI wizard; TUI still named as a caller.

**remustwo alternative:** one catalog, one provider plane, subcommands `catalog init` vs `catalog ingest`, adapters not services, callbacks not `QObject`, tests+migration in the same commit, no archive/docs zoo. A new language would not have prevented `cover_url` drift.
