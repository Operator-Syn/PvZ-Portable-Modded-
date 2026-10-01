# PvZ-Portable architecture map

**Current structure:** see [source modules and refactoring contract](refactoring.md) for responsibility ownership and checks. The snapshot in [baseline.md](baseline.md) is historical. These notes are source-observed unless explicitly labeled otherwise.

## Repository layers

| Area | Responsibility | Starting points |
| --- | --- | --- |
| Gameplay | Board state, plants, zombies, projectiles, waves, challenges, and save integration | [gameplay.md](gameplay.md), `src/Lawn/Board/Board.cpp`, `src/Lawn/Plant/Plant.cpp`, `src/Lawn/Zombie/Zombie.cpp`, `src/Lawn/Projectile/Projectile.cpp` |
| Engine and effects | Reanimations, particle systems, trails, attachments, rendering support, and frame profiling | [engine-effects.md](engine-effects.md), `src/PvzpLib/`, `src/SexyAppFramework/graphics/`, `src/SexyAppFramework/misc/FrameProfiler.*` |
| Persistence | Profile data and portable mid-level save state | [resources, persistence, and platforms](resources-persistence-platforms.md), `src/Lawn/System/SaveGame.cpp` |
| Resources and platforms | Resource parsing/loading, pak access, platform-specific build targets | [resources, persistence, and platforms](resources-persistence-platforms.md), `src/SexyAppFramework/misc/ResourceManager.*`, `src/SexyAppFramework/paklib/PakInterface.*`, `CMakeLists.txt` |

## Representative runtime paths

- **Frame and board flow:** `LawnApp::UpdateFrames` → `Board::Update` → `Board::UpdateGameObjects`; board rendering gathers game objects in `Board::DrawGameObjects` from `Board::Draw`.
- **Plant attack:** `Plant::Update` and its attack helpers select targets and allocate projectiles; `Projectile::Update` handles motion, collision, impact, splash, and damage.
- **Zombie behavior:** `Zombie::Update` dispatches behavior by type/phase; target selection and damage/effect eligibility are handled in zombie and board logic.
- **Effects:** `EffectSystem::Update` advances reanimations, particle systems, and trails; particle emitters allocate and update particles through `PvzpParticleHolder`.
- **Save/load:** `Board::SaveGame` / `Board::LoadGame` call the save layer in `src/Lawn/System/SaveGame.cpp`; the portable `.v4` path serializes explicitly tagged fields.
- **Resource startup:** `LawnApp` creates a `PvzpResourceManager`, parses `properties/resources.xml`, then loads named resource groups. Pak access is implemented by `PakInterface`.

These are navigation paths, not complete call graphs. Use clangd's definition/reference navigation from the source symbol for current callers.

## Maintainer entry points

- [Source modules, design principles, and checks](refactoring.md)
- [Gameplay systems and data flow](gameplay.md)
- [Engine, effects, rendering, and profiling](engine-effects.md)
- [Resources, persistence, platforms, and build](resources-persistence-platforms.md)
- [Fork-specific source delta](fork-delta.md)
- [Captured checkout and restore procedure](baseline.md)

## Navigation tools

The repository already enables CMake's compilation database export and has `.clangd` configured to use `build/` with background indexing. `compile_commands.json` and clangd's index are generated/local artifacts; do not commit them. The README's DeepWiki link is a supplemental browsing surface, not the canonical fork map.
