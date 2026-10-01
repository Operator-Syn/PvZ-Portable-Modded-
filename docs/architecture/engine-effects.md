# Engine, effects, rendering, and profiling

**Scope:** source map for the reusable engine/effect layer and its board integration. **Evidence:** source-observed at the baseline revision in [baseline.md](baseline.md); profiling code does not establish a measured performance result by itself.

## Engine layer

`src/PvzpLib/` contains project-specific support for attachments, effects, particles, reanimations, and trails. `EffectSystem::Update` coordinates effect updates and delegates to the corresponding holders/systems. `PvzpParticleHolder` owns particle-related pools and update work; `PvzpParticleSystem` and `PvzpParticleEmitter` define system/emitter behavior.

The broader `src/SexyAppFramework/` tree supplies framework functionality including graphics, widgets, resource management, pak access, and audio. Prefer using the existing framework path when a change belongs there rather than adding parallel infrastructure in `Lawn`.

## Rendering path

`Board::Draw` builds the board presentation and invokes `DrawGameObjects`. Draw ordering and render-list accounting are board concerns; particle and reanimation visuals are supplied by the effect/animation systems. GPU execution time cannot be inferred from CPU-side timing alone.

## Frame and particle diagnostics

`src/SexyAppFramework/misc/FrameProfiler.*` owns frame timing aggregation and performance-log output. Detailed mode adds subsystem/effect metrics; `EffectSystem`, particle code, and the graphics/presentation path feed measurements. The current local source also has an optional particle calculation worker path. Its presence in source is not evidence that it improves performance or is race-free under every scene.

## Investigation guidance

- Follow particle creation through pool allocation, update, deletion/recycling, and draw submission.
- Check whether an effect is gameplay-significant or decorative before changing spawn rate, lifetime, or quality policy.
- Compare frame/update, board/effect, particle update/draw, and presentation-wait metrics separately. CPU draw time is not GPU time.
- Treat log captures as evidence for the captured run only; preserve session/build/profile metadata when comparing runs.
