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

Plant healing is centralized in `BoardPlantHealing.cpp`'s `PlantHealing::HealPlant` helper. Sun Magnet stacks, Chomper healing, Pumpkin and Tall-nut regeneration, and Gloom-shroom regeneration all pass through it, so Pumpkin's >=5,000-sun healing multiplier applies consistently. Gloom-shrooms regenerate 25 HP/s and add 150 sun/s each to the board's overdrive upkeep. Pumpkin and Tall-nut variants start at full HP using their active overdrive capacity. When that capacity changes, current HP changes with it to preserve missing HP, with a one-HP floor for living plants. This prevents an overdrive activation from creating artificial injuries for Serra to heal; switching overdrive off preserves healing progress. Capacity changes emit `[health] event=overdrive_capacity_change` with old/new HP and maximum HP rather than reporting combat damage.

In the widened Survival: Endless viewport, Catapult Zombies wait until at least one quarter of their hitbox is visible and they are targetable by ground attacks before beginning a volley.

Zombie Rain drops zombies at the normal right-edge entry X (`screen width - 20`) and one tile (80 pixels) toward the house, past the normal right-edge pool-entry trigger used by Dolphins and Snorkels. Rain-spawned pool-capable ground zombies enter their appropriate pool state on landing; ordinary wave spawns retain their normal pool-entry behavior. Rain Bungees select their target within the last two plant columns using their existing eligibility rules. Falling rain zombies cannot walk horizontally before landing; Pogo bounce updates wait until descent ends, and animation speed is restored on landing. `[zombie_rain]` events record each ordinary rain spawn position, row, column, type, altitude and screen width.

## Gargantuar attack movement

Regular and red-eye Gargantuars remain stationary while an attackable plant is in contact, including the brief normal phase between consecutive smash animations. They resume walking once no target remains. This preserves the high-sun first-spike exception, Bulwark behavior, mind-controlled combat and victory/cutscene movement. Ephraim shares Tall-nut's vault and smash protection, including the Pumpkin above him. Runtime movement has not been verified.

## Sun-tier bucket armor

Female Sniper's direct arrows deal 750% damage against tier buckets, native helmets and shields (450 HP per normal arrow, 1,350 HP per critical arrow). Her body multiplier remains 185%, and wound DOT is unchanged. Armor spillover is converted to body damage units only after the protective layers break.

Each piercing critical arrow additionally scales its direct damage by the number of eligible targets along its travel direction, including targets it already struck. This uses the shared quadratic zombie damage multiplier and its sun-tier resistance ceiling per target, matching Ephraim's melee rules. Targets outside the arrow's lane/path or protected by unreachable terrain do not increase the count. Normal arrows and wound DOT do not receive this multiplier.

Up to five Female Snipers can share an original planting cell. The limit counts living residents by home row and column even while they roam; visiting Snipers do not consume another cell's home slots. Movement still permits stacking without applying this planting limit. Coffee Bean's existing column-wide action also boosts eligible Snipers for 700 game ticks (seven seconds); another bean refreshes that duration. Attack progression, hitstops and recovery advance at three times normal speed (+200%), while lane movement and the boost duration advance at normal speed. Every arrow released during the boost costs 300 banked sun, including each of the three critical arrows; an unaffordable arrow is skipped without stalling its sequence. Outside the boost arrows remain free. The boost timer is preserved in portable save field 121; the normal Plant Practice payment exemption still applies.

The custom bucket is the outer damage layer: it absorbs incoming damage before shields, native helmets and body HP. A flying Balloon Zombie consumes damage with its balloon first, before bucket or native armor; remaining damage can then reach armor. Sniper arrows and wound DOT also ignore shield-bypass and simultaneous shield/body flags. The 800,000-sun tier grants the extra bucket to eligible non-bucket zombies; at 2,500,000 sun native bucket zombies receive it too. Bungee retains its existing exemption. Missing armor is initialized from the current zombie type, tier and three-million-sun durability modifier. A depleted bucket retains its nonzero maximum HP and is never refilled by this initialization. Portable armor validation runs after all zombie fields have loaded and derives its limit from the current rules, including type and durability multipliers, rather than discarding valid high-tier armor using the former fixed cap. Runtime armor behavior has not been verified.

## Zombie health hover

During active gameplay, hovering a living zombie shows its name, total current/max HP and percentage, body HP, and its helmet, shield, tier bucket armor and flying protection when those layers have a maximum HP value. The tooltip refreshes with combat changes and disappears when the pointer leaves. Zombie hover takes precedence over a plant underneath it; packets, collectibles, tools and challenge tooltips retain their existing priority. This uses tooltip-only hit testing, so clicking and tool targeting are unchanged. No persistent zombie health labels are added. Runtime hover behavior has not been verified.

## Placement, packets, and waves

Seed selection and planting cross `SeedPacket` and board placement logic. Challenge-specific constraints are applied in gameplay/UI code, so new placement exceptions should be checked against both ordinary placement and challenge rules. Wave state and zombie selection are board-owned; see `Board::Update` and its wave helpers.

Plant Practice is an always-available challenge mode with a single middle lawn lane, a ten-packet chooser containing all available plant types, and free planting. It has no automatic zombie waves; the board's spawn picker adds a selected land zombie to that lane. Breaches do not end the session, and the mode does not award campaign or challenge completion progress.

For the source-by-source implementation checklist for a new plant, see [Adding a plant](adding-plants.md).

## Breach and mower diagnostics

`Board::ZombiesWon` snapshots the breaching zombie's type and row before the loss sequence, then includes its name and lane in the defeat dialog. `LawnMower::StartMower` records the zombie and collision overlap that caused a mower to activate; mower initialization, deployment, recovery, and state transitions are also logged. Ready mowers retain the existing same-row overlap and Bungee/headless eligibility rules. A hidden ready mower encountered during play is made visible at its normal ready position before collision checks.

`Board::RecordGameplayEvent` adds mode, level, wave, sun, viewport, and playable-grid context. `FrameProfiler` writes these sparse `gameplay_event` records to `userdata/performance.log` with the current session ID and frame sequence. This checkout's build and startup were checked; in-game breach and mower event emission has not yet been exercised in a representative run.

## Save boundary

Board save entry points are `Board::SaveGame` and `Board::LoadGame`; object and board serialization is implemented in `src/Lawn/System/SaveGame.cpp`. Portable save fields are explicitly synchronized there. Any new persistent gameplay state needs a stable field/tag and a default when loading older saves; consult the canonical save discussion in [resources, persistence, and platforms](resources-persistence-platforms.md).

### Custom plant direction and Serra production

Ephraim and Female Sniper scan both sides and prioritize the leftmost eligible zombie, favoring threats closer to breaching. Female Sniper gives attackable Bungees priority over all other targets in the lane she is targeting; ties within either group favor the leftmost zombie. Her existing home-lane priority, adjacent-lane travel and obstacle rules remain in effect. Both idle and complete attack poses face that side; arrows use the matching signed horizontal velocity, including all three critical arrows. Lane selection also considers rear targets, so she retains home-lane priority while one remains behind her. A shot keeps its chosen direction through the complete sequence and its existing pauses.

Serra Bishop has a permanent 237..312-tick sun production cadence, matching Twin Sunflower production overdrive without requiring overdrive and randomly chooses her complete staff or critical source sequence. These yield one or five normal sun coins, respectively, at the casting pose. The shared producer path applies completed-flag sun value scaling and normal coin collection/Sun Magnet rules; it does not add sun directly to the bank. Idle slowly plays the five-frame staff sequence with a resting beat and foot-anchored breathing; this visual loop does not produce extra sun. An injured plant anywhere on the lawn can prompt a healing cast independently of sun production. Serra heals the lowest HP percentage plant for a base 5% of its maximum HP with existing healing modifiers, at 50 sun per base HP restored; bonus healing is free. Available sun limits the base heal before modifiers are applied, and the final heal is capped by the target's missing HP, followed by a three-second healing cooldown, and shows the supplied Divine starburst on the target through the particle overlay pass. Overflow above missing HP is discarded without sun cost; healing blocked by insufficient sun is recorded separately. `[healing]` logger events record cast starts, releases/skips, target HP changes, source/target identity, paid base HP, free bonus HP, overflow and sun accounting. Healing and due sun production share one randomly selected staff/critical cast when both are ready before its release pose; healing-only casts do not create sun. The implementation was reviewed in source and generated atlases were inspected; gameplay and save reload have not been checked in-game.

### Plant damage audit events

`[damage]` events use the same game logger as `[healing]`, so game ticks and plant object IDs connect damage to subsequent healing. Events record cause, zombie name/type/ID when available, projectile type/ID, target name/seed/ID and position, HP and percentages before/after, maximum HP, effective damage, raw damage, overkill, death/squash state and sun balance. Bites, Gargantuar smashes, spike/vehicle contact, vehicle squashes, Jack-in-the-box explosions, Boss fireball/stomp/RV attacks, Zombotany explosions/projectiles, catapult basketballs, overdrive/coffee health drains are instrumented. Overdrive capacity adjustments are recorded separately as `[health]` events. Continuous drains emit only when whole HP changes, after existing nonlethal floors apply. Instant squashes record effective HP as zero and preserve the stored HP separately; gameplay damage and death rules are unchanged.

Zombie-fired projectiles retain source zombie ID and type in portable projectile field 114, including when their shooter dies before impact. Older saved projectiles lack attribution and are explicitly logged as `unrecorded_zombie`; their projectile type/ID and damage cause are still available. These are plant damage events; damage dealt to zombies is outside this audit path. Healing, damage and rain events are written to both SDL/console and app-data `userdata/log.txt` in release and debug builds. The existing file sink also retains crash logging. File logging after restart and save reload remain unverified.

Sun production and sky-spawn countdowns are inactive before `SCENE_PLAYING` or while the seed chooser exists. Existing sun coins also stop moving, collecting and aging in that state. Plant upkeep, Sun Magnet assignment and regeneration timers are paused with them, preventing pre-game sun costs and healing from continuing during the intro/repick. Cutscene rendering and non-sun coins retain their existing behavior. These guards have been reviewed in source; chooser and rain behavior have not been verified in-game.
