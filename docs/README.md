# Repository knowledge base

This documentation describes the source tree and configuration in this fork. It is a navigation aid; source and build configuration remain authoritative.

Start with the [architecture map](architecture/index.md). The [baseline record](architecture/baseline.md) identifies the captured checkout and its restorable local patch. The [fork delta](architecture/fork-delta.md) inventories local source changes at capture time.

## Evidence labels

- **Source-observed** means current source or configuration supports the statement. It does not imply a successful build or in-game verification.
- **Runtime-verified** is used only when a representative runtime check was performed and recorded.
- **Inference** marks a relationship derived from source call paths.

For C++ symbol navigation, CMake exports `compile_commands.json` and the root `.clangd` points clangd at `build/` with background indexing enabled. Keep generated indexes and build outputs local.
