# Source modules and refactoring contract

The game still builds through its existing executable/app target. Source units separate responsibilities, and pure rule modules compile without the application, renderer, or licensed assets.

## Starting point and ownership

The refactor starts at `dd02e6c5402b5a1f54b9828ab8ef4f5099c2d984`. The initial tracked worktree modification was the user-owned `.gitignore` edit, which is preserved. Source behavior at that revision is the reference; the earlier [snapshot](baseline.md) is a historical archive. Assets, private saves, and logs are excluded from the baseline.

| Original unit | Lines | Responsibilities observed |
| --- | ---: | --- |
| `Board.cpp` | 12,046 | Lifecycle, simulation, waves, placement, input, economy, healing, rendering, save integration |
| `Zombie.cpp` | 11,254 | Initialization, movement, behavior families, targeting, damage, effects, animation, rendering, boss |
| `Plant.cpp` | 6,248 | Initialization, abilities, production, collection, targeting, projectiles, animation, rendering, definitions |
| `SaveGame.cpp` | 3,298 | File access, format parsing, portable fields, legacy raw decoding, restoration |

## Responsibility map

| Area | Source units and ownership |
| --- | --- |
| Board orchestration | `Board.cpp` owns lifecycle/state and frame order; `BoardLevel.cpp` owns level setup/endings; `BoardWaves.cpp` owns wave composition/spawns |
| Placement and input | `BoardPlacement.cpp` evaluates placement; `BoardPlanting.cpp` applies input; `BoardPlantingPlan.*` returns affected plant IDs and lane count; input, hit testing, tooltips, tutorials, and cheats have separate `Board*` units |
| Simulation and economy | `BoardCombat.cpp`, `BoardEconomy.cpp`, `BoardGeometry.cpp`, `BoardEnvironment.cpp`, `BoardOverdrive.cpp`, `BoardZombieStrength.cpp`; `BoardPlantHealing.cpp` implements the shared `PlantHealing.*` interface |
| Board presentation/edges | `BoardRendering.cpp` and `BoardRenderList.cpp` draw; `BoardDiagnostics.cpp` emits sparse events; `BoardPersistence.cpp` reports save/load failures |
| Zombies | `Zombie.cpp` coordinates initialization/update. Movement, aquatic, vehicles, Bungee, dancers, Gargantuar, Zombotany, boss, combat, targeting, damage, death, status, effects, animation, rendering, audio, resources, and definitions have named `Zombie*` units |
| Plants | `Plant.cpp` coordinates lifecycle/update. Abilities, area abilities, production, Plantern, magnets, shooting, projectile launch, targeting, combat, state, animation, rendering, input, definitions, and resources have named `Plant*` units |
| Projectiles | `Projectile.cpp` coordinates lifecycle; `ProjectileDefinitions.cpp`, `ProjectileCollision.cpp`, `ProjectileMotion.cpp`, `ProjectileImpact.cpp`, and `ProjectileRendering.cpp` own their named responsibilities |
| Pure rules | `PlantRules.*`, `PlantingRules.*`, `ProjectileRules.*`, `TargetingRules.*`, `WaveRules.*`, `ZombieStatusRules.*`, `ZombieStrengthRules.*`, `SunThresholds.h` take explicit values/snapshots and return predicates, reasons, and multipliers |
| Saves | `System/SaveGame.cpp` owns file access; `SaveGameFormat.*` validates/encodes bytes; `SaveGamePortableContext.h` supplies field primitives; `SaveGamePortable.cpp` serializes entities/effects through one chunk catalogue; `SaveGameBoardFields.cpp` owns stable board tags; `SaveGameLegacy.cpp` decodes raw saves; `SaveGameRestore.cpp` reconnects runtime state |
| Representative UI | Seed chooser lifecycle, selection, input, and rendering have separate units with a shared `SeedChooserOrder.h`. Store lifecycle, purchases, input, and rendering share the immutable `StoreCatalog.h` |
| Particles | `PvzpLib/PvzpParticle.cpp` retains simulation/worker dispatch; `PvzpParticleDefinitions.cpp` owns startup resources; `PvzpParticleRendering.cpp` owns drawing |

Classes retain their existing fields and live state relationships to preserve legacy layouts. These source boundaries and pure seams do not imply complete independence from Board/application state. Smaller units stay in their existing locations where no separate responsibility or duplicated invariant was established.

## Design principles

| Principle | Applied contract |
| --- | --- |
| **Do one thing well** | Name modules for their responsibility; separate orchestration, rules, presentation, and file access |
| **Compose small stages** | Pass explicit values/snapshots; return placement reasons, planning results, or typed save failures |
| **Stay silent by default** | Pure rules and save codecs emit no incidental output; Board persistence and startup loading decide when to report errors |
| **Validate early and fail clearly** | Check cell bounds before grid access and portable header/CRC/outer TLV framing before applying state; return error, byte offset, and record type |
| **Keep side effects at the edges** | Isolate files, restoration, diagnostics, audio, resource loading, and drawing from pure calculations |
| **Make behavior traceable** | Keep stable names and serialization IDs; document ownership and tie tests to protected gameplay/format behavior |

**DRY:** Cattail acquisition and spike redirection share one priority comparator. Upgrade searches share one cell/layer scan while preserving the broader Fume-shroom upgrade predicate. Strength, plant-stack, projectile, and chooser-order rules have one definition. The save-chunk catalogue defines dispatch and writer order together.

**KISS:** retain C++20, the existing target, and concrete functions. Add abstractions for established shared behavior; keep domain-specific differences explicit.

## Compatibility and checks

- Board, Plant, Zombie, Projectile, and effect field order/layout is preserved. Legacy `.dat` keeps its existing build/platform restrictions.
- Portable magic, header size, versions, chunk order, field IDs, missing-field defaults, and unknown-field/chunk handling are preserved. Invalid framing now fails instead of allowing a partial parse to appear successful.
- Boolean `LawnLoadGame`/`LawnSaveGame` APIs remain available. Detailed variants add contextual errors for boundary callers.
- Tests are opt-in for native builds, and the native Linux CI job enables them. Platform packaging keeps its existing target set by default.

```sh
nix develop -c cmake -S . -B build -DPVZ_BUILD_TESTS=ON
nix develop -c cmake --build build
nix develop -c ctest --test-dir build --output-on-failure
```

The asset-free suite checks sun tiers/resistance, Cattail priority/ties, placement bounds, wave restrictions, cold eligibility, chooser order, and portable container bytes/corruption/unknown chunks. Success is silent. These tests do not establish complete gameplay, renderer, legacy-save, or cross-platform runtime coverage.

Use a disposable save directory for representative gameplay:

```sh
./build/pvz-portable -resdir "$PWD" -savedir /path/to/disposable-save-directory
```

Check level initialization/waves, planting/projectiles, zombie damage/defeat, chooser/store interaction where unlocked, save/exit/resume, and an older portable save with omitted extension fields. Keep runtime evidence separate from build and rule-test results.

## Implementation verification (2026-10-01)

- **Passed:** native Release build and all five CTest suites (`strength`, `targeting-placement`, `waves`, `cold`, `save-format`).
- **Passed:** syntax compilation of 88 affected game source units with `PVZ_DEBUG`, `DO_FIX_BUGS`, and `LOW_MEMORY` enabled; subsequently changed persistence/format units rechecked.
- **Passed:** isolated Linux/X11 game startup, level 1-1 setup, Peashooter placement, pause/menu interaction, save/exit, and resume with the plant and sun count restored.
- **Passed:** loading a disposable portable save with the newer Plantern flame damage and zombie-tier sun fields omitted (tags 123/124). This checks missing-field compatibility, not every historical save version.
- **Not run:** cross-platform SDK builds, raw legacy `.dat` fixtures, complete wave/combat/defeat scenarios, and unlocked chooser/store gameplay. Asset-free rules cover selected constraints; those remaining integration paths need platform/gameplay checks.

Existing `.gitignore` contents were preserved byte-for-byte. Temporary profiles and runtime evidence were kept outside the checkout. No commit, push, or deployment was performed.
