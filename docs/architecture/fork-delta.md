# Fork-specific source delta at baseline

This inventory identifies local files that differ from the captured `HEAD`. It is deliberately source-oriented: the archive is authoritative for the exact patch, and this page does not claim the changes have all been built or exercised in-game.

## Gameplay and UI

- **Board:** zombie toughness tiers and transition handling; special plant/projectile mechanics; screen-shake propagation; board dimensions and placement; Almanac/tooltip text.
- **Plants and projectiles:** high-sun Winter Melon, Plantern, and Cattail behavior; projectile targeting, volley/impact behavior, and damage handling.
- **Zombies:** additional high-tier armor/toughness rules, special damage/effect eligibility, and type-specific target/attack behavior.
- **Save/UI integration:** portable serialization extensions, option persistence and screen-shake control, plus related seed, challenge, store, chooser, and Almanac text.

## Engine and diagnostics

- **Particles/effects:** particle update batching and optional worker calculations, quality handling, shake integration, and effect update coordination.
- **Profiling/render integration:** particle/frame metrics, session/log behavior, and graphics timing hooks.
- **Attachments and reanimations:** local changes to effect attachment and animation paths.

These categories summarize changed-file areas, not a complete semantic review of every hunk. For exact content, use [the captured patch](baseline/working-tree.patch). Revisit this page after the baseline changes and promote stable mechanics into the relevant subsystem page with source pointers and verification status.
