# Checkout baseline snapshot

**Captured:** 2026-09-30 18:25:03 +08:00
**Branch:** `main` (101 commits ahead of `origin/main` at capture)
**HEAD:** `afc6f3d912a37405e33c0c0adcff54b57747e02c`
**origin/main:** `c670693ffb7e12e535c3edcdb70c918dfd23a325`
**Worktree:** 28 tracked modified files; no eligible untracked source files were present at capture. Existing edits were left uncommitted.

## Archive

[`working-tree.patch`](baseline/working-tree.patch) is the binary-capable output of `git diff --binary HEAD` captured before these architecture documents were created. It reproduces the tracked modifications listed below against the recorded `HEAD`.

- SHA-256: `9cbeed760734364beebfa78b1de77f6951d699d53df636e2c1795b8f88d3e0ce`
- Size: 151,685 bytes
- Captured tracked paths: 28 (see inventory below)
- [`working-tree.sha256`](baseline/working-tree.sha256) contains the expected restored content hash for every captured path.

Restore into a clean checkout of the recorded commit:

```sh
mkdir -p /tmp/pvz-architecture-baseline
git archive afc6f3d912a37405e33c0c0adcff54b57747e02c | tar -x -C /tmp/pvz-architecture-baseline
cd /tmp/pvz-architecture-baseline
git apply --binary /path/to/working-tree.patch
sha256sum --check /path/to/working-tree.sha256
```

Do this in a temporary extraction, replacing `/path/to` with the repository path that contains both snapshot files. The patch does not contain ignored build outputs, caches, runtime saves/logs, external assets, or the architecture documents added after capture. No such untracked source files were present at capture time. In particular, external/user-provided game archives and local `properties` content are not part of this archive.

## Changes after capture

While this map was being written, `src/Lawn/Zombie.cpp` changed again in the shared worktree. That later edit is preserved in the live checkout but is **not** part of the archive or hash manifest. The baseline restore was checked against the manifest in a temporary clean extraction; do not compare the baseline hashes directly to the now-diverged live file without accounting for that later edit.

## Modified-file inventory

**Gameplay and persistence:**

- `src/Lawn/Board.cpp`, `src/Lawn/Board.h`
- `src/Lawn/CutScene.cpp`
- `src/Lawn/Plant.cpp`, `src/Lawn/Plant.h`
- `src/Lawn/Projectile.cpp`, `src/Lawn/Projectile.h`
- `src/Lawn/SeedPacket.cpp`
- `src/Lawn/System/SaveGame.cpp`
- `src/Lawn/Zombie.cpp`, `src/Lawn/Zombie.h`
- `src/LawnApp.cpp`, `src/LawnApp.h`

**UI and gameplay presentation:**

- `src/Lawn/Widget/AlmanacDialog.cpp`
- `src/Lawn/Widget/ChallengeScreen.cpp`
- `src/Lawn/Widget/NewOptionsDialog.cpp`, `src/Lawn/Widget/NewOptionsDialog.h`
- `src/Lawn/Widget/SeedChooserScreen.cpp`
- `src/Lawn/Widget/StoreScreen.cpp`

**Effects, rendering, and profiling:**

- `src/PvzpLib/Attachment.cpp`, `src/PvzpLib/Attachment.h`
- `src/PvzpLib/EffectSystem.cpp`
- `src/PvzpLib/PvzpParticle.cpp`, `src/PvzpLib/PvzpParticle.h`
- `src/PvzpLib/Reanimator.cpp`
- `src/SexyAppFramework/graphics/GLInterface.cpp`
- `src/SexyAppFramework/misc/FrameProfiler.cpp`, `src/SexyAppFramework/misc/FrameProfiler.h`

This snapshot records source state only. It does not establish build success, platform coverage, or runtime correctness for the archived changes.
