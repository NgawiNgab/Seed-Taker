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
slot-37 detour (network thread)          ctor ASM wrapper (F-074; any game thread)
  getId == 0x0B?                           save regs -> snapshot 7 int64 windows
    yes -> packet+0xA8 -(atomic)->           over src(x1)+0x60..0x78 -> restore ->
    no  -> rejectedPackets++                 tail-call original (residue kept)
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
- `src/modules/seed/capture/SrcScan.*` (F-071/F-072/F-074): LevelData-ctor
  hook via a preserve-all ASM wrapper (file VA + slide, dual `bl`-word
  anchors verified pre-hook); entry-time 7-window int64 snapshot of
  src(x1) via per-window value+seq atomics; lock-free, status string.
  (The 8-byte src-getter hook crashed the game — F-073; never again.)
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

## FIELD-001 verdict + memory-editor hunt (on-device, user-operated)

FIELD-001 (SP, known seed -3821773851083165131): ctor wrapper
survived, scan verdict 0/7 — the seed is not in src+0x60..0x78
(F-075/F-076). Next: live hunt with LeviMemEditor v1.5.0 (F-077):

1. Load the known-seed SP world, open the editor (M button).
2. Analyze → Hex signature `35 B2 4F 5A EE 54 F6 CA`, scope All
   readable (else Writable), budget up. (Alternate: Results → Qword
   exact search for -3821773851083165131 / 14624970222626386485.)
3. Per hit: RVA inspector 128B view, record VA + surrounding bytes.
4. Pointer-reference search on the hit / container base to identify
   the holding object. Read-only: no freeze/write/hook.
5. Return: hit VAs + surrounding bytes + pointer-ref results → maps
   to struct+offset; if LevelData, extend the wrapper to x0 windows.

## ST-011: consumer-side hunt (Zaphkiel, user-operated)

Memory-editor path retired after 3 scoreless attempts (F-081). The
ST-007 re-grep proved the LevelData ctor copies nothing 64-bit from
src (F-082) — producer-side exhausted. ST-011 works from the seed's
CONSUMERS: T1 `STRUCT SeedHandler` (the copy-seed UI provably
displays the live seed on-device); T2 conditional `fn<>` on the
seed-reading method; T3 `findclass<RandomSeed>` backup. Input filed
at `evidence/ST-011/zInput.txt`; return `zaphkiel_out-11.txt`.

Run 1 (2026-11-14, PARTIAL, F-083): T1 misfired — bare STRUCT went
down the STRING path (retry: `struct<SeedHandler>`); T3 correctly
negative (RandomSeed has no typeinfo — NBT key only, retry as raw
string search + xref). Retry input: `evidence/ST-011/zInput-retry.txt`.
Side proof: lib size 342231392 B ≈ 326 MiB confirms memeditor
attempt-1's 323 MiB scan was .so-only (F-078).

## ST-012: setter + src getter dumps (Zaphkiel, user-operated)

ST-011 T3-string found the seed loop (F-084) and setter-sharing
proved ab28 = src's u64 seed getter (F-085). ST-012 resolves BOTH
homes: P0 `fn<0x111463bc>` (setter body — predict container insert,
possibly + direct store) + P1 `fn<0x10b0ab28>` (predict `ldr
x0,[x0,#OFF2]` — explains FIELD-001's 0/7; kill-condition stated) +
P2 `struct<SeedHandler>` re-issue. Input:
`evidence/ST-012/zInput.txt`; return `zaphkiel_out-12.txt` (~3K
lines acceptable). Clean resolution ⇒ runtime read design.

## Window-8 implementation (ST-012 resolution, uncommitted)

ST-012 resolved both homes: LevelData-side = container rule
(this+0x4f8, type-7, +0x38 — F-086, no direct member); src-side =
src+0x0 (F-087). Implementation: 8th scan window at src+0x0
(window 0 = prediction, windows 1-7 = negative controls) —
consts-only change (`Seed.hpp` + `SrcScan.hpp` comments; all loops
already count-driven; hook untouched). On-device verdict: +0x0
match with 0/7 controls = Recovered (perfect discrimination).
Staleness: nil by idempotence (fallback: hook the setter).
Committed in PR #4; on-device result: local + LAN CORRECT (F-088).

## Setter hook: the server path (F-088, implemented — uncommitted)

Window-8 fixed local play and left the big Survival Server with no
seed: `pkt ... got 0; src 0/8`, windows `+0x0=-1, +0x60=1,
+0x64..+0x78=0`. Both failures are structural, not tuning:

- the packet leg reads the LEGACY INT32 `StartGamePacket+0xA8`
  (F-058/F-087) — an old 32-bit field, always 0 on a server;
- the src snapshot is taken at ctor ENTRY, and on the MP path the
  server seed is written into src only AFTER the ctor runs, so the
  snapshot sees a placeholder (`-1`).

So the seed must be captured where it is REGISTERED, not where it is
stored. `LevelData`'s seed setter `0x111463bc` is that point (F-086):
`setter(this, seed)` with the u64 in x1, called by NBT load, the ctor,
and the network path alike. DESIGN:

- **Hook**: detour at `libraryBase("libminecraftpe.so") +
  0x111463bc`; install is gated on TWO `bl` words inside the function
  (`0x11146414`=`0x96FC3C16`, `0x11146494`=`0x94001FD3`, script-
  computed and cross-checked against the ctor anchor `0x97E75B05`).
  Anchor mismatch ⇒ refuse to patch (fail-safe, no wrong-function
  patch). Prologue is a normal `sub sp,#0x80` frame — safely hookable
  (the ≤16-byte-getter rule of F-073 does not apply).
- **Publish**: full `int64` through a seqlock (odd sequence = write in
  flight; reader retries, bounded) — the 32-bit packed atomic of the
  legacy sources cannot carry the seed.
- **Priority**: verified src window > setter > legacy packet/object.
  The setter is authoritative and shows a value whenever it has fired;
  confidence is Recovered once validated, Inferred otherwise (the seed
  is still the seed — it is never withheld for lack of a check).
- **Re-validation**: compared on EVERY new sequence (last call wins),
  so a placeholder registered early on a network load can never latch
  a stale mismatch (the ctor may register before the server value).
- **Range guard removed**: the `[! outside int32 range: cannot match]`
  caveat is obsolete — setter and src windows carry full int64, so an
  out-of-int32 expectation can and must be able to match.
- **Unchanged**: legacy packet/object legs stay as cross-checks; the
  src windows stay as negative controls; nothing logs or allocates in
  any detour.

Host syntax gate 12/12. Pending: Survival Server re-test on-device
(the setter must report as `capture: setter (primary)`), then PR.

## F-089: first runtime verdict + reporting-layer fixes

On-device result of the F-088 build, two paths:

- **Local world** — seed RECOVERED, exact value
  (`-6348884760859271012`), source `level-setter:seed register`. The
  setter hook, its script-computed `bl` anchors, the int64 seqlock and
  the "register point, not storage" premise are all CONFIRMED working.
- **Survival Server (Mega SMP)** — still no seed: `~-1 (INFERRED)`,
  `src+0x0=-1`, `+0x60=1`, `+0x64..0x78=0`, `armed, expected 0`. The
  hook reported no fault (no anchor-mismatch state), but the old build
  could not show whether the setter FIRED there.

Three defects in the reporting layer surfaced, all fixed here:

1. **Placeholder rendered as a seed.** With the src windows latched, the
   HUD fell through to the legacy int32 packet leg — which is 0 on a
   server — and printed `~-1 (INFERRED)`, i.e. it presented the legacy
   slot's value as a candidate seed. When the setter has fired and
   contradicts the armed expectation, that contradiction is now the
   answer (`setter MISMATCH: expected X got Y`), not a fall-through.
2. **Latent mismatches latched permanently.** A mismatch at ctor-entry
   time (a placeholder on a server) locked the window forever, so a
   real value arriving later could never validate. Mismatches are now
   re-checked; only matches latch.
3. **`armed, expected 0`.** `saveConfig` wrote `validateExpectedSeed=0`
   for "unset", and `loadConfig` sets `hasExpectedSeed=true`
   unconditionally — so an unconfigured or cleared install returned as
   a REAL expected seed of 0. The key is now omitted when unset.

Diagnostics added: an always-rendered `setter: fires=N last=V` line.
This is the line that decides the open question — `fires=0` means the
server seed never passes through `0x111463bc` (F-086's
single-registration premise falsified for the MP path, next probe = the
13 other callers); `fires=N` means it does and only the value is in
question.

Also added: `tools/hostgate/` — the host syntax gate now lives in the
repo (stubs + script) instead of `/tmp`, which was being wiped between
sessions. Still not a substitute for CI; it catches type errors only.

## F-093: packet-side seed capture (the server leg)

Why: F-091/F-092 proved the setter is a world-settings randomizer and the
StartGame handler never calls it, so no amount of setter work can ever
yield a server seed. The decoded packet is the only place the server's
value exists client-side.

Where: the handler copies the decoded payload with
`add x0, sp, #0x7d0; add x1, x20, #0x30; bl 0x9b56e60` (0xa2fabcc..
0xa2fabd4), `x20` = packet. So the copy SOURCE is `packet+0x30` =
LevelSettings, and it runs on every StartGame including servers.

How: pre-hook `0x9b56e60` (VA + two anchor words, refuse-on-mismatch),
snapshot a **16-byte band** at source `+0x0C` and `+0x14`, publish via
seqlock. Two slots, not one, deliberately: the copy proves the seed lies
somewhere in `src+0x0C..0x1C` but the exact index is inferred from the
codec, and an unverified offset is exactly the mistake behind `+0xA8`.

Selection: verified src window > verified band slot > setter > legacy
packet/object.

Safety properties, stated so they can be checked:

- An **unverified** band value never enters the seed display. It appears
  only on the `packet band:` line, marked `?`.
- A `0` expected seed is a **no-op** everywhere, because the `src+0x64`
  control window is all-zero on every path and previously "matched" it,
  reporting `0 [recovered]`.
- The original copy-ctor is called LAST, so any result register survives.

Read-out for the next run:

| HUD line | Meaning |
|---|---|
| `packet band: none (hooked, no StartGame yet)` | hook installed, no StartGame seen |
| `packet band: +0xc=… ?  +0x14=… ?` | hook ran, candidates unvalidated — supply a known seed |
| `+0xc=… <SEED>` | that slot matched a known seed: offset CONFIRMED, now the seed source |

Local world first (known seed picks the slot), then Mega SMP.
