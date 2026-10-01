# Repository Guidelines

## Project Structure

- `src/Lawn/` contains board simulation, plants, zombies, projectiles, and game UI; `src/Lawn/System/` holds systems such as save handling.
- `src/PvzpLib/` contains project-specific effects, particles, reanimations, trails, and attachments. `src/SexyAppFramework/` provides graphics, resources, audio, widgets, and platform support.
- `android/`, `ios/`, `wasm/`, and platform subdirectories configure platform builds. `scripts/` and `tools/` contain utilities. The project requires users to provide game assets (`main.pak` and `properties/`); do not add them to commits.
- Asset-free gameplay rule and save-container tests are under `tests/`; enable them with `-DPVZ_BUILD_TESTS=ON` and run `ctest --test-dir build --output-on-failure`. SDL Mixer X contains its own tests under `src/SexyAppFramework/sound/SDL-Mixer-X/test/`.
- Read `docs/architecture/index.md` for the subsystem map.

## Build and Development

Use the repository Nix shell or an environment with the dependencies listed in `README.md`:

```sh
nix develop -c cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
nix develop -c cmake --build build
./build/pvz-portable -resdir "$PWD"
```

The run command requires local game resources. For editor navigation, `.clangd` uses `build/compile_commands.json` and background indexing; enter or reload the dev shell so the editor can find `clangd`.

## Coding Style

Follow neighboring C++ code: C++20, tab indentation, braces on separate lines, `PascalCase` types and methods, `m`-prefixed member fields, and uppercase constants. Keep changes focused and preserve platform guards, save compatibility, and existing ownership patterns. No repository-wide formatter or linter is configured; avoid broad reformatting.

## Testing

Prioritize the requested implementation and quick iteration. Do not add tests for every change; add or update focused tests when they protect important behavior, a regression, or a boundary, or when the user requests them. Do not automatically configure or run builds and test suites after each change. Leave routine build and test execution to the user unless they ask for it or a specific check is necessary to resolve a concrete implementation risk. When checks are run, keep them focused and report exactly what was checked. For gameplay or rendering changes, describe any in-game scenario and platform checked; add screenshots for visual changes when useful. A successful build alone does not verify runtime behavior.

## Commits and Pull Requests

Recent commit subjects are short and file-focused (for example, `Update Plant.cpp`). Use a concise imperative subject naming the change or main area. Pull requests should explain the player-visible effect, list build/runtime checks and platforms, link a related issue when applicable, and include screenshots for visual changes. No PR template is currently defined.

## Assets and Local Work

Keep licensed game data, saves, logs, build outputs, and clangd indexes out of commits. Before editing, inspect `git status`; preserve existing user changes and avoid staging or committing unrelated files.
