# Gameplay systems and data flow

**Scope:** source map for the board simulation and principal combat entities. **Evidence:** source-observed at the baseline revision in [baseline.md](baseline.md); runtime behavior is not claimed as verified.

## Board owns the level simulation

`Board` holds the active lawn state and coordinates level updates, game objects, wave progression, placement, and drawing. The main update path is `Board::Update`, which reaches `Board::UpdateGameObjects` for the entity collections. `Board::Draw` delegates ordered game-object gathering/drawing to `Board::DrawGameObjects`.

Start at `src/Lawn/Board/Board.h` for the board's object/state surface and `src/Lawn/Board/Board.cpp` for frame coordination. Placement, waves, combat, input, and drawing have separate units in the [responsibility map](refactoring.md). Board-level targeting helpers and wave/challenge decisions may affect several plant or zombie types; check those consumers before changing shared behavior.

## Combat flow

1. Plants update through `src/Lawn/Plant/Plant.cpp` and its responsibility modules; plant-specific logic selects targets and calls firing or ability helpers.
2. The `Projectile*` units separate lifecycle, motion, collisions, impact, and presentation.
3. Zombies update in `src/Lawn/Zombie/Zombie.cpp`; type and phase determine movement, attacks, armor/body damage, and susceptibility to effects.
4. Board helpers coordinate cross-entity effects such as explosions, lane-wide abilities, target queries, and zombie tier changes.

Relevant declarations are in `Plant.h`, `Projectile.h`, and `Zombie.h`. When changing an attack, trace both the projectile creation path and the final impact/damage path; a visible shot does not by itself establish that its collision row, target eligibility, or damage flags are correct.

Zombie square-squish impacts remove one active plant layer per impact, choosing Pumpkin first, then the normal plant, underplant, and flying plant. Further layers are exposed to later impacts.

Gargantuar smashes preserve a Pumpkin shell layered over Tall-nut or Chomper Nut; the Tall-nut body takes the smash damage. Other square impacts still resolve one layer at a time.

Cattail targets across lanes. Its target selection and homing spike redirection prioritize Balloon Zombies first, then the leftmost eligible hitbox; distance from the Cattail or projectile breaks ties. There is no per-zombie Cattail targeting limit, so every eligible Cattail may select the same target. Each homing spike can redirect once if its target becomes invalid, then coasts offscreen.

Plant healing is centralized in `BoardPlantHealing.cpp`'s `PlantHealing::HealPlant` helper. Sun Magnet stacks, Chomper healing, Pumpkin and Tall-nut regeneration, and Gloom-shroom regeneration all pass through it, so Pumpkin's >=5,000-sun healing multiplier applies consistently. Gloom-shrooms regenerate 25 HP/s and add 150 sun/s each to the board's overdrive upkeep.

In the widened Survival: Endless viewport, Catapult Zombies wait until at least one quarter of their hitbox is visible and they are targetable by ground attacks before beginning a volley.

Zombie Rain drops zombies directly into playable cells, past the normal right-edge pool-entry trigger used by Dolphins and Snorkels. Rain-spawned pool-capable ground zombies enter their appropriate pool state on landing; ordinary wave spawns retain their normal pool-entry behavior.

## Placement, packets, and waves

Seed selection and planting cross `SeedPacket` and board placement logic. Challenge-specific constraints are applied in gameplay/UI code, so new placement exceptions should be checked against both ordinary placement and challenge rules. Wave state and zombie selection are board-owned; see `Board::Update` and its wave helpers.

## Breach and mower diagnostics

`Board::ZombiesWon` snapshots the breaching zombie's type and row before the loss sequence, then includes its name and lane in the defeat dialog. `LawnMower::StartMower` records the zombie and collision overlap that caused a mower to activate; mower initialization, deployment, recovery, and state transitions are also logged. Ready mowers retain the existing same-row overlap and Bungee/headless eligibility rules. A hidden ready mower encountered during play is made visible at its normal ready position before collision checks.

`Board::RecordGameplayEvent` adds mode, level, wave, sun, viewport, and playable-grid context. `FrameProfiler` writes these sparse `gameplay_event` records to `userdata/performance.log` with the current session ID and frame sequence. This checkout's build and startup were checked; in-game breach and mower event emission has not yet been exercised in a representative run.

## Save boundary

Board save entry points are `Board::SaveGame` and `Board::LoadGame`; object and board serialization is implemented in `src/Lawn/System/SaveGame.cpp`. Portable save fields are explicitly synchronized there. Any new persistent gameplay state needs a stable field/tag and a default when loading older saves; consult the canonical save discussion in [resources, persistence, and platforms](resources-persistence-platforms.md).
