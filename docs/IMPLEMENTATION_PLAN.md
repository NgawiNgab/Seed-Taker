# Seed-Taker Initial Implementation Plan

Approved by the user 2026-11-14 with an explicit risk flag: the captured
int32's seed-ness is INFERRED-strong (F-051), not confirmed. Mitigations are
designed in, not bolted on: SPEC §4 confidence-gating (INFERRED until
runtime-validated), per-path graceful degradation (unresolved →
`unavailable`, never crash), and detour self-validation (getId gate,
vtable-base check).

## What the RE phase delivered (ST-001..ST-008, F-001..F-052)

- MP chain, traced instruction-by-instruction:
  `LegacyClientNetworkHandler` slot 37 ← dispatcher (F-028/F-033),
  packet+0xA8 → extractor → src+0x78 (F-049) → getter (F-048) →
  ctor → LevelData.+0x3b0 (F-045/F-047).
- Slot-8 getter = the only int32 LevelData accessor (F-024); slot-2 getId
  returns 0x0B (F-027); both flat RTTI names present in 1.26.62
  (`9LevelData`, `26LegacyClientNetworkHandler`).
- Consequence (F-052): capture needs NO static signatures and NO copied
  patterns — all addresses resolve at runtime via RTTI+vtable.

## Design (shipped in this branch)

```
slot-37 detour (network thread)          src-getter detour (F-072; games threads below)
  getId == 0x0B?                           call original -> snapshot 7 int64
    yes -> packet+0xA8 -(atomic)->           windows over src+0x60..0x78
    no  -> rejectedPackets++                 -(per-window atomics)->
(Level::initialize detour: RETIRED vehicle, F-067; kept as backup)
SeedModule::onFrame (GL thread): retry installs (idempotent) -> drain by
sequence -> validate (one-shot per source/window while armed) -> primary =
verified src window, else newest usable -> render (SPEC §4) + DIVERGE line
when packet != object + per-window debug lines + validation line (armed).
```

- `include/seedtaker/sdk/offsets/Seed.hpp`: constants with per-value
  provenance comments. Nothing without a finding ID.
- `src/modules/seed/capture/VtableScan.*`: `dl_iterate_phdr` scan of the
  libminecraftpe mapping for two adjacent resolved slots (8+9) → verified
  vtable base; `isLevelDataInstance()` gate. Resolves at most once.
- `src/modules/seed/capture/SeedCapture.*`: hook install/uninstall
  (idempotent, mutex-guarded, module-owned per the BedrockTools pattern),
  both detours, packed-atomic readings, diagnostic counters, status string.
  Detours allocate/lock/log nothing.
- `src/modules/seed/capture/SrcScan.*` (F-071/F-072): src-getter hook
  (file VA + slide, body-verified pre-hook); post-call 7-window int64
  snapshot via per-window value+seq atomics; lock-free, status string.
- `libraryBase()` (`VtableScan.*`): dlpi_addr of a loaded library for
  file-VA rebasing (ST-005 slide, computed at runtime).
- `SeedModule`: default-on passive observer; drains, validates (config keys
  `validationArmed`, `validateExpectedSeed`), renders. Per-source verified/
  failed flags; mismatch excludes that source (sticky until re-armed).
- Consolidation note: the approved plan named `RttiScanner`/`VtableHook`
  files, but `memory::resolve` (RTTI+vtable) and `hooks::install` already
  provide those roles — no duplicate wrappers were written.

## Deliberately NOT in this iteration

- LevelInit address (BT's is a byte pattern for another build — rejected,
  F-054). The detour + install site + vtable gate are written; the
  `SignatureId::LevelInit` definition is empty so the path stays disabled
  and reported (kept as backup; LevelInit hunt retired, F-067). ST-010 hunts the int64 field on LevelSettings (F-068); the ctor-hook (known VA 0x11146b24+slide) is the new vehicle.
- Sig-hooks as backup (optional; F-052). RTTI+vtable is primary.
- Heap-scan LevelData discovery (fallback option if LevelInit disappoints).
- Wire-field map of packet+0xA8 (infeasible statically, F-050) — answered
  at runtime instead (see validation).

## Build

`src/modules/**.cpp` globs pick up the new files; no xmake change needed.
Verification (F-056/F-057/F-008): all 11 TUs pass a host `g++ -fsyntax-only`
gate against mirrored stub headers, then the first CI build (`build.yml`,
NDK r28c, 2026-11-14) went green after two fixes (`fmt` in `add_packages`,
missing `onMenuConfigChanged` definition): compile + link + levipack +
artifact upload all pass. The compile gate is CLEAR; the remaining gate is
on-device runtime validation (protocol below). Never claim on-device
behavior without seeing it.

## Runtime validation protocol (on-device, user-operated)

1. SP known-seed: new world, seed e.g. `12345`. Packet path FIRES on
   loopback 0x0B packets (F-058 — the old "no packet locally" prediction
   was wrong); expect `~0 (INFERRED - server-packet...)` until the int64
   object path lands (ST-010 field decision → ctor-hook). Record the
   status line; use validation mode for verdicts, never the raw line.
2. MP: join any server. Expect `~N (INFERRED - server-packet...)`.
   `rejectedPackets` should stay 0 (slot-37 exclusivity check).
3. Validation: in the menu (or config.json), set `validateExpectedSeed`
   (free-form text, full int64 range incl. negatives) and flip the
   `validationArmed` toggle, then load a world whose true seed is known
   independently. The first reading per source is compared one-shot: match
   → Recovered (promotes F-051 toward CONFIRMED); mismatch → that source
   is excluded with a log warning + HUD note (re-arm to retry).
4. Spoof probe (later): server forcing seed 0 → packet reads 0 while any
   independent channel disagrees → DIVERGE line exercises SPEC §3.

## Residual risks

- Slot-37 void-return assumption (F-055): mitigated by tail-call shape;
  a wrong assumption corrupts one register on one path — watch for
  network instability in MP testing and report it as P0.
- `levelSettings` may not be LevelData (type unverified): mitigated by
  the vtable check (safe skip + counter), but the SP path may need a
  different acquisition (answered: ctor-hook supersedes LevelInit, F-067).
- `resolveVtableFunction` behavior on this build is trusted from the
  preloader API contract; if StartGameHandle fails to resolve on-device,
  the status line reports `capture: unresolved` (check log Rtti names).
