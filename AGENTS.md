# Repository Guidelines

## Project Structure

- `src/Lawn/` contains board simulation, plants, zombies, projectiles, and game UI; `src/Lawn/System/` holds systems such as save handling.
- `src/PvzpLib/` contains project-specific effects, particles, reanimations, trails, and attachments. `src/SexyAppFramework/` provides graphics, resources, audio, widgets, and platform support.
- `android/`, `ios/`, `wasm/`, and platform subdirectories configure platform builds. `scripts/` and `tools/` contain utilities. The project requires users to provide game assets (`main.pak` and `properties/`); do not add them to commits.
- The root has no gameplay test suite. SDL Mixer X contains its own tests under `src/SexyAppFramework/sound/SDL-Mixer-X/test/`.
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

After C++ changes, build the affected configuration with `cmake --build build`. The GitHub Actions workflow builds release targets but does not provide gameplay regression coverage. For gameplay or rendering changes, describe the in-game scenario and platform checked; add screenshots for visual changes when useful. Do not report runtime behavior as verified from a successful build alone.

## Commits and Pull Requests

Recent commit subjects are short and file-focused (for example, `Update Plant.cpp`). Use a concise imperative subject naming the change or main area. Pull requests should explain the player-visible effect, list build/runtime checks and platforms, link a related issue when applicable, and include screenshots for visual changes. No PR template is currently defined.

## Assets and Local Work

Keep licensed game data, saves, logs, build outputs, and clangd indexes out of commits. Before editing, inspect `git status`; preserve existing user changes and avoid staging or committing unrelated files.
