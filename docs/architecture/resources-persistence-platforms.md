# Resources, persistence, platforms, and build

**Scope:** source/configuration map of resource startup, saves, supported build branches, and local C++ navigation. **Evidence:** source-observed at the baseline revision in [baseline.md](baseline.md). Platform builds and save compatibility were not exercised as part of this documentation snapshot.

## Resources and external game data

`LawnApp` constructs `PvzpResourceManager`, parses `properties/resources.xml`, and loads named resource groups. `ResourceManager` handles resource definitions/loading; `PakInterface` provides archive access. Some runtime resources/assets are user-supplied and are not part of this source snapshot. Do not commit or index private/external `main.pak`, runtime saves, or local user data as architecture documentation.

## Persistence

`src/Lawn/System/SaveGame.cpp` contains the portable mid-level `.v4` serialization path and legacy compatibility handling. The repository README describes global user data separately from level saves. `.v4` uses explicit field synchronization and TLV-style records; legacy `.dat` files are raw-layout dumps and have cross-platform limitations. See the README's “Save data compatibility” section and the save entry points in `Board.cpp`.

When adding persistent state, inspect the portable serializer and its older-field defaults, then use the existing converter at `scripts/pvzp-v4-converter.py` only with test/sample data. Do not use private live saves as documentation fixtures.

## Build and platform shape

The project is CMake/C++20. `CMakeLists.txt` selects platform input/window sources and target form: Android builds a shared library; iOS builds an app bundle; other targets build an executable, with additional Windows, Nintendo Switch, and Emscripten branches. The root `flake.nix` provides a Linux development shell with CMake, Ninja, GCC, pkg-config, SDL2, libopenmpt, JPEG, PNG, and zlib development dependencies. README contains non-Nix dependency and build instructions.

## C++ code navigation

`CMakeLists.txt` sets `CMAKE_EXPORT_COMPILE_COMMANDS ON`; `.clangd` points to `build/` and enables background indexing. The current checkout has `build/compile_commands.json`. Keep this database and clangd cache local/ignored. Build configurations and platform targets can produce different macros and source sets, so an index from one build directory is not proof of every platform's compilation state.
