---
name: no-git-switch-during-build
description: "Don't switch git branches or check out files in a repo while a background firmware build of that repo is still running"
metadata:
  node_type: memory
  type: feedback
  modified: 2026-11-17T16:51:57.871Z
  originSessionId: c8263742-ff5a-43af-bea4-c75f2b09cf4a
---

Don't run `git checkout`, `git switch` or a merge in a repo while a background build of that repo is still compiling.

On 2026-11-17 I checked out `cyd-port` while the relays build was running. That branch doesn't have
`components/pkes_contract/run_meta.h`, so the relays build failed with `run_meta.h: No such file or directory`.
Rebuilding fixed it.

**Why:** the build reads the sources as it compiles, and a checkout rewrites the tracked files under it.

**How to apply:** wait for the build's completion notification, or read its summary, before any branch operation.
Read-only git commands (`rev-parse`, `cat-file`, `log`, `status`) are safe during a build.
