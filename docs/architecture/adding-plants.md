# Adding a plant

**Purpose:** source-based checklist for manually implementing a plant in this checkout. This guide describes code paths, not an automatic generator. It is based on the current source; game behavior still needs to be exercised in-game.

## 1. Decide the plant's identity and availability

1. Add a `SEED_*` value in `src/ConstEnums.h` in the plant section, before `NUM_SEED_TYPES`. Do not insert it among existing values: plant types are used as indices into `gPlantDefs` and are serialized in saves. Append new values so existing numbers stay stable.
2. Add the matching `PlantDefinition` at the same position in `src/Lawn/Plant/PlantDefinitions.cpp`. The array is indexed by `SeedType`, so the entry's `.mSeedType` and array position must agree.
3. Choose how the plant becomes available. `LawnApp::HasSeedType` handles progression and store-owned plants. A custom test plant that should ignore progression needs an explicit case there; if availability depends on an image, return false when that resource is missing. Check the trial-stage guard above the switch too.
4. If the plant must appear in the seed chooser, update `NUM_SEEDS_IN_CHOOSER` and `src/Lawn/Widget/SeedChooserOrder.h`. Keep `SeedChooserTypeAtIndex` and `SeedChooserIndexOf` as inverse mappings for the new entry. Preserve the existing stock slots and special cases for Imitater and Sun Magnet. The chooser's position code uses the reverse mapping, so an enum value appended at the end does not automatically land in the right grid cell.

The current Ephraim entry is an example of an appended custom seed: its enum is after the existing plant values, its chooser slot is separate from its enum value, and `HasSeedType` checks that its image loaded. The transparent `properties/ephraim/Ephraim_Great_Lord_RGBA.png` supplies the current art. Run `python3 scripts/create_ephraim_atlases.py` to regenerate the ignored local assets. It produces a separate six-cel idle atlas and four complete horizontal attack atlases: `lance.png` (21 cels), `critical_lance_complete.png` (21), `javelin_complete.png` (7), and `critical_javelin_complete.png` (11). Each attack includes its matching windup, strike, and recovery in a single row; runtime playback selects one atlas for the entire attack. The generator keeps the source crop groups explicit, omits repeated normal ready poses, splits all four follow-through poses separately, and supplies the critical javelin with two ready recovery poses. All cells are 96 by 96 pixels, with the largest atlas 2016 pixels wide. The attack clocks run for 126, 140, 70, and 110 simulation ticks before anticipation and impact holds; damage lands on cels 11, 11, 4, and 8 respectively. The launch interval accommodates the complete animation and any afterimage follow-up. Afterimages echo the selected attack style with their own delayed clock. These are source and asset-generation details; in-game visual behavior requires runtime review. For a normal progression plant, follow the existing progression order instead of copying that override.

## 2. Fill out the plant definition

`PlantDefinition` in `Plant.h` supplies the seed type, optional direct image array, reanimation type, packet image index, sun cost, cooldown, subclass, launch rate, and name key. Start from the closest existing definition in `PlantDefinitions.cpp` and choose each field deliberately:

- Use a reanimation type for an existing plant animation, with `.mPlantImage = nullptr`.
- Use a direct image array for a custom cel-strip or image, with the resource pointer initialized before the plant can be selected.
- Set `.mPacketIndex` to the matching seed packet art when reusing existing art. For custom packet art, inspect `SeedPacket` rendering and add the smallest corresponding resource/rendering change; `mPlantImage` controls the planted plant image, not automatically the chooser card.
- `.mPlantName` is also used to form translated name and tooltip keys (`[NAME]` and `[NAME_TOOLTIP]`). Add matching strings to the appropriate string resource if the built-in fallback is not suitable.
- Set the subclass and launch rate consistently with similar plants; existing systems use them for behavior such as shooter handling and attack timing.

`Plant::GetCost`, `GetRefreshTime`, `GetNameString`, and `GetToolTip` read the definition. Add a specific branch only when the new plant intentionally differs from those shared rules.

## 3. Load custom art, if needed

For an external cel-strip, declare an `IMAGE_*` resource in `src/Resources.h`, define it in `src/Resources.cpp`, and load it in the startup resource extraction path. Set `mNumCols` and `mNumRows` to the actual cell layout before assigning the image into the one-element `Image*` array referenced by the plant definition. Mark custom images with the existing image-sanding helper when needed. Keep user-provided/license-restricted game art out of commits; this repository expects local `properties/` assets.

`Plant::GetImage` reads the definition's image array. For a direct-image plant, set frame timing in `Plant::PlantInitialize` or its animation update path, and check `PlantAnimation.cpp` and `PlantRendering.cpp` for assumptions about state, scale, shadows, and cel selection. Ephraim derives its idle and complete attack atlases from the PNG; keep source crop groups explicit in the generator. `Plant::Animate` advances within the selected complete attack row, and `Plant::UpdateShooting` applies damage when the displayed pose reaches the matching contact frame. If a missing art file should hide the plant, make `HasSeedType` depend on the loaded resource so the chooser cannot select an unusable seed.

## 4. Add gameplay behavior in the owning module

`Plant.cpp` coordinates lifecycle and update flow; most behavior lives in focused `Plant*.cpp` modules under `src/Lawn/Plant/`. Search for the closest plant family and follow its existing state/update path:

- Shooter and projectile behavior: `PlantShooting.cpp`, `PlantTargeting.cpp`, and the relevant `Projectile*` modules.
- Production and collection: `PlantProduction.cpp` and `BoardEconomy.cpp`.
- Special abilities and area attacks: `PlantAbilities.cpp`, `PlantAreaAbilities.cpp`, and `BoardCombat.cpp`.
- State and animation: `PlantState.cpp` and `PlantAnimation.cpp`.
- Placement restrictions and upgrades: `BoardPlacement.cpp`, `PlantingRules.*`, and existing `Plant::IsUpgrade` / upgrade helpers.

Use existing Board, projectile, damage, healing, and targeting helpers when the behavior matches them. Check consumers in challenges and modes when a new plant changes placement, targeting, or eligibility rules. Avoid adding a large switch to the lifecycle coordinator when a focused plant module owns the behavior.

## 5. Check connected surfaces

Before calling the plant complete, inspect the features it touches:

- **Chooser and seed packet:** `SeedChooserOrder.h`, `SeedChooserScreen.cpp`, `SeedChooserRendering.cpp`, `SeedPacket.*`, and `HasSeedType`.
- **Almanac and text:** `AlmanacDialog.cpp`, `Plant::GetNameString`, `Plant::GetToolTip`, and string resources.
- **Store or unlock flow:** `HasSeedType`, purchase enums/catalog, and purchase handling if the plant is bought rather than progressed.
- **Planting behavior:** `BoardPlacement.cpp` plus the mode/challenge restrictions used by that plant.
- **Persistence:** plant seed types and plant state are serialized in `src/Lawn/System/SaveGamePortable.cpp`. If the plant adds persistent fields, preserve defaults for older saves and use the existing tagged-field conventions. Keep enum numbers stable.

## 6. Manual verification checklist

After coding, check the actual target build and, with local game resources, exercise:

1. The seed appears at the intended picker position, has the intended packet art/name/cost, and is selectable only when unlocked.
2. Plant it on valid and invalid cells, including water, roof, night, and upgrade cases that apply.
3. Observe its idle/attack/ability animations, cooldown, target choice, projectiles or effects, damage, and death/removal.
4. Check relevant challenge or game modes and Almanac behavior.
5. Save and resume with the plant present if it adds or changes persisted state.

The opt-in asset-free rules tests can cover pure rule changes, but they do not verify art loading, rendering, picker interaction, or in-game behavior. Follow the repository's normal build/test instructions in [refactoring.md](refactoring.md) when a code check is needed.

## Source entry points

- `src/ConstEnums.h`
- `src/Lawn/Plant/Plant.h` and `src/Lawn/Plant/PlantDefinitions.cpp`
- `src/Lawn/Plant/Plant.cpp` and the relevant `src/Lawn/Plant/Plant*.cpp` module
- `src/Lawn/Widget/SeedChooserOrder.h`
- `src/LawnApp.cpp` (`GetSeedsAvailable` and `HasSeedType`)
- `src/Resources.h` and `src/Resources.cpp`
- `src/Lawn/System/SaveGamePortable.cpp`
