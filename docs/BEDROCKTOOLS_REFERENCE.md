# BedrockTools Reference Findings

Reference: https://github.com/NgawiNgab/BedrockTools (shallow clone inspected
2026-11-14; version 1.5.12 per `include/bedrocktools/Version.hpp` and
`levimod.json`, game target `1.26.6X.X`, Android arm64-v8a, LeviLauncher).

BedrockTools is a **development reference only**. Seed-Taker does not link
against it, use its runtime API, or require it at runtime. Patterns below were
re-implemented in Seed-Taker's own codebase under the `seedtaker` namespace.

## 1. Module architecture

- Base class: `src/modules/Module.hpp` — class `Module` (global namespace).
  - Identity: `name`, `description`, `moduleId` (`"bedrocktools." + name`).
  - Enablement is two-factor: `masterEnabled` (menu toggle) AND
    `keybindActive` (keybind toggle) combine in `updateEnabledState()` to
    drive `enabled`, which fires `onEnable()`/`onDisable()`.
  - Lifecycle virtuals: `onInit`, `onEnable`, `onDisable`, `onFrame`,
    `onMouseEvent`, `onMenuConfigChanged`, `showInLegacyMenu`,
    `onMenuRegistered`, `onKeybindEvent`.
  - Config virtuals: `loadConfig` / `saveConfig` over `nlohmann::json`
    (`keybind`, `keybindActive`, `masterEnabled` base keys).
- Typical module: header + cpp pair under `src/modules/<category>/`, e.g.
  `src/modules/hud/playercoords.hpp` / `.cpp`.
  - `PlayerCoordsModule::onInit()` subscribes to
    `bedrocktools::events::LocalPlayerTickEvent` via `bus().subscribe<...>()`
    and caches data; `onFrame()` submits `pl::modmenu::DrawCommand`s through
    the registry's `submitDrawCommands(moduleId, cmds)` helper.
  - HUD position convention: `hudPosX` / `hudPosY` / `isHudModule` config keys
    (matches preloader `HudEditorElement.positionKeyX/Y` defaults — verified in
    preloader `include/pl/ModMenu.hpp`).

## 2. Module registry

- `src/modules/ModuleRegistry.hpp` / `.cpp` — class `ModuleRegistry`
  (singleton `get()`).
  - Ownership: `mOwned` (`vector<unique_ptr<Module>>`); lookup: `mById`
    (`unordered_map<string, Module*>` with transparent hash); ordered view:
    `mView` (`vector<Module*>`).
  - `emplace<T>(...)` constructs, indexes, and returns the module.
  - `find(id)`, `modules()`, `initialize()` (calls every `onInit()` once),
    `onFrame()` (calls `onFrame()` on enabled modules only),
    `onMouseEvent()` fan-out, `setKeybindBlocked()` / `keybindBlocked()`.
  - `registerAllModules()` guards on `!modules().empty()` and emplaces every
    module (~50 in BedrockTools).
- Draw-command aliases + helper live in the registry header
  (`PLModMenu_DrawCommand*` macros, `submitDrawCommands`).

## 3. Native SDK and runtime interaction

- Memory helpers: `include/bedrocktools/sdk/Memory.hpp` — `field<T>(obj, off)`,
  `virtualCall<Ret>(instance, index, args...)`, `patchMemory`,
  `makeWritableExecutable`.
- Offsets: `include/bedrocktools/sdk/offsets/*.hpp`, aggregated by
  `sdk/Offsets.hpp`. **Version-specific; not applicable to Seed-Taker's
  1.26.62 target without re-verification.**
- Resolution: `include/bedrocktools/memory/Signatures.hpp` +
  `src/core/memory/Signatures.cpp` — `SignatureId` enum, `SignatureDefinition`
  (pattern | rtti+vtableSlot), `resolveAll()` via
  `pl::memory::resolveSignatures` / `pl::memory::resolveVtableFunction`
  (preloader APIs verified in `preloader-android/include/pl/memory/*.hpp`).
- Hooks: `src/core/memory/Hooks.hpp` — `install`/`remove` over
  `pl::memory::hook`/`unhook` (verified: `hook` returns int, 0 on success).
- Hook sites: `src/core/GameHooks.cpp` — `hookSignature(id, detour, &original)`
  best-effort per hook; `installed = tickOriginal && clientUpdateOriginal`;
  `clientUpdateDetour` captures the `ClientInstance*` and publishes
  `ClientInstanceUpdateEvent`.
- Frame pump: `hookEgl()` resolves `eglSwapBuffers` from `libEGL.so` via
  `dlsym` and `swapBuffersDetour` publishes `FrameEvent`. This hook target is
  a system library, not the game binary.
- Runtime: `src/core/Runtime.{hpp,cpp}` — `load` (mod id, dirs, config path,
  `launcherContext()` via `/proc/self/cmdline`, dlopen-watch for
  `libminecraftpe.so`), `enable` (resolve + install), `minecraftLoaded`,
  `wireEvents` (`FrameEvent` -> `ModuleRegistry::onFrame()`, mouse, screen
  depth tracking). Install order: resolve signatures -> install hooks ->
  `registerAllModules()` -> `wireEvents()` -> `initialize()` -> config load ->
  `registerModulesWithLauncher()`.
- Entry: `src/main.cpp` — `PL_REGISTER_MOD` with `load`/`enable`/`disable`/
  `unload` forwarded to `Runtime`.

## 4. Events

- `include/bedrocktools/events/Event.hpp` — `EventType` (`Frame = 1`,
  `LocalPlayerTick`, ...), `EventPriority` (`First = 200` .. `Last = -200`),
  `Cancellable`.
- `include/bedrocktools/events/EventBus.hpp` — thread-safe bus
  (`subscribeRaw`/`subscribe`, `unsubscribe`, `publishRaw`/`publish`, snapshot
  under shared lock), singleton `bus()` defined in
  `src/core/events/Events.cpp`.
- `FrameEvent` is an empty struct (`events/FrameEvent.hpp`).

## 5. Config and menu

- `src/config/ConfigManager.{hpp,cpp}` — singleton, debounced (150 ms) worker
  thread, atomic temp-file + rename writes, JSON shape
  `{ "Modules": { "<name>": { ... } } }`, `flush()` generation barrier.
- `src/launcher/ModuleMenu.cpp` — `registerModulesWithLauncher()` builds one
  `pl::modmenu::ModuleBuilder` per module (verified builder API:
  `description`/`modId`/`defaultEnabled`/`hideInHudEditor`/`onToggle`/
  `onConfigChanged`/`onKeybind`/`config(...)`/`registerModule()`), with generic
  JSON-type -> `ConfigType` inference plus BedrockTools-specific slider
  heuristics (not carried over).

## 6. Build and packaging

- `xmake.lua`: `shared` C++20 target, `preloader` xmake package (git, cmake),
  `nlohmann_json v3.11.3`, android arm64 flags (`-fno-rtti`, gc-sections,
  `max-page-size=16384`), `after_build` levipack step via python.
- `scripts/package_levipack.py`: parses `Name|Author|Description|Version` from
  `Version.hpp` via regex, writes `manifest.json`
  (`type: "preload-native"`, `entry: "libBedrockTools.so"`, ...) into a zip
  with round-trip verification.
- `levimod.json`: repo-side descriptor (`schema_version`, `id`, `version`,
  `minecraft_versions`, `info`, `release_assets`).
- `.github/workflows/build.yml`: xmake + NDK r28c, uploads `.levipack`.

## 7. Explicit limitation

A case-insensitive search for `seed` over all BedrockTools headers and sources
matches only an RNG variable in `debugmenu.cpp` and a map-seeding helper
(`seedBundledMapsLocked`) in `hivemaps.cpp`. **BedrockTools contains no world
seed, LevelData/LevelSettings, StartGamePacket, or seed-accessor support.**
Seed-Taker's seed path cannot be derived from this reference and requires
direct binary evidence (ST-001).

Two related patterns ARE present and useful as capture-design references:

- `Level::initialize` hook: `src/modules/hud/debugmenu.cpp`
  (`s_origLevelInitialize`) detours `LevelInit` and receives the level-settings
  object as an opaque `void* levelSettings` parameter — the local-world
  settings entry point, if ST-001 confirms a seed field there.
- Packet-handler interception: `src/core/memory/Signatures.cpp` resolves
  `PlayerListPacketRead` by RTTI (`"16PlayerListPacket"`, slot 17) and
  `ResourcePacksInfoPacketHandle` / `ResourcePackStackPacketHandle` by byte
  pattern; consumed in `src/modules/hud/chatheads.cpp` and
  `src/modules/misc/forceglobalrp.cpp`. Same shape a StartGame/LevelSettings
  capture hook would take (pending ST-001/002).
