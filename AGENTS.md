# remustwo agent rules

1. Library first. CLI/GUI are adapters.
2. One bootstrap: `catalog ingest`. Init cannot match, and says so.
3. Dead code never enters git.
4. Docs follow the binary. No "project complete" files.
5. CI is a gate from commit 1.
6. Tests travel with every ported file. Migration + merge SQL + test in one commit.
7. remusone is read-only. Copy, do not submodule.
8. No artwork blobs, no committed catalogs, no second offline database, no copyrighted ROMs. Local dumps belong in gitignored `testroms/`. `testdata/` is synthetic vectors only.
9. One confidence threshold source. One systems definition source (SQL after phase 2).
