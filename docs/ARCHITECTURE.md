# Seed-Taker Architecture (initial implementation)

Standalone native Levi mod. No dependency on BedrockTools at build or runtime;
only the preloader SDK (`pl/*`) and `nlohmann_json`.

```
xmake.lua                  shared C++20 lib, android arm64, levipack step
levimod.json               repo-side mod descriptor (target: 1.26.62)
scripts/package_levipack.py  manifest.json + libSeedTaker.so (+ optional icon) -> .levipack
include/seedtaker/
  Version.hpp              Name/Author/Description/Version (parsed by packager)
  Export.hpp               SEEDTAKER_API visibility macro
  SeedTaker.hpp            umbrella header
  sdk/Memory.hpp           field/virtualCall/patchMemory (no game values)
  sdk/Types.hpp            Vec2/Vec3/BlockPos/AABB/Color
  sdk/Offsets.hpp          version-specific offsets (EMPTY: pending ST-001)
  sdk/Functions.hpp        resolved-address -> function pointer
  memory/Signatures.hpp    SignatureId table (EMPTY: pending ST-001)
  events/                  Event/EventBus/FrameEvent/Events
src/
  main.cpp                 PL_REGISTER_MOD -> Runtime
  core/Runtime.*           load/enable/disable/unload, dlopen-watch, install order
  core/GameHooks.*         EGL frame pump only (game hooks: pending ST-001)
  core/memory/Hooks.hpp    pl::memory hook/dlopen wrapper
  core/memory/Signatures.cpp  resolver over the (empty) definition table
  core/events/Events.cpp   bus() singleton
  modules/Module.hpp       lifecycle + config contract
  modules/ModuleRegistry.* ownership, lookup, fan-out, draw helper
  modules/seed/SeedModule.*  drain/validate/render (packet+object+7 src windows)
  modules/seed/capture/SeedCapture.*  slot-37 + LevelInit detours, atomics
  modules/seed/capture/VtableScan.*   vtable-base verify + libraryBase()
  modules/seed/capture/SrcScan.*      src-getter hook, 7-window snapshots
  config/ConfigManager.*   debounced JSON store
  launcher/ModuleMenu.*    pl::modmenu registration + typed config inference
```

## Data flow (skeleton)

`eglSwapBuffers` -> `FrameEvent` -> `ModuleRegistry::onFrame()` ->
`SeedModule::onFrame()` -> `pl::modmenu::submitDrawCommands`.

## Deliberate deviations from BedrockTools

1. `seedtaker` namespace everywhere (BedrockTools leaves `Module` global).
2. `using` aliases instead of `PLModMenu_*` macros.
3. `Runtime::install()` continues with a warning when signature resolution
   fails (BedrockTools aborts) — required while the signature table is empty.
4. No `Api` runtime export (`BedrockTools_GetApi` equivalent): nothing
   consumes a Seed-Taker API yet.
5. No mouse/screen-event wiring and no overlay bridge: producers are pending
   ST-001; `ModuleRegistry` already exposes the consumer-side plumbing.
6. Trimmed menu config inference (generic types only, plus one documented
   key-specific case: `validateExpectedSeed` renders as free-form text
   instead of a 0..200 slider) and no xmake requires-lock until CI produces
   one.
7. `package_levipack.py`: icon optional, no bundled font (launcher renders
   overlay text; `registerFont` remains available if a custom font is needed).

## Evidence gates (do not bypass)

- `sdk/offsets/*`, `memory/Signatures.*`, `sdk/world/*`, game hooks in
  `GameHooks.*`, and any `field<>`/`virtualCall<>` on game objects: ST-001+
  evidence required. See `docs/EVIDENCE_LOG.md`.
