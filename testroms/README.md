# testroms

Put **local** ROM dumps here for machine-specific checks. This directory is gitignored except this file.

- Do not commit dumps, archives, or extracted discs.
- CI and unit tests use `testdata/` only (tiny synthetic bytes that match `fixture.dat`).
- `ctest` test `LocalTestRomsTest` scans this folder when it contains files, and skips when it is empty.
