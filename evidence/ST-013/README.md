# ST-013 — undumped setter callers (ISSUED 2026-11-14, awaiting user run)

## Why this exists

F-089 runtime: on a **Survival Server** the seed-setter hook reports
`fires=164 last=-1`. The hook works (local worlds recover the exact seed),
so the open question is no longer "does the hook fire" but **"what does a
multiplayer load register, and from where"**.

Static state: the setter `0x111463bc` has 14 callers (ST-012). Only three
are characterised:

| Caller | Shape | Passes |
|---|---|---|
| `0x111471a8` (inside ctor `0x11146b24`) | `setter(this, ab28(src))` | src+0x0 |
| `0x11130800` (NBT load `0x11130798`) | `setter(this, RandomSeed)` | disk seed |
| `0xa1bd1b0` | gated `setSeed` on a deref'd object | guard-dependent |

The other **11 are undumped**, so no network-shaped registration point can
be confirmed or excluded. That is the gap this run closes.

## The question, stated so it can be answered negatively

> Is there a registration point that a **multiplayer** world load reaches,
> and what value does it pass the setter?

- **Yes** → the seed is reachable client-side; the fix is to re-point the
  hook (or read the landing member directly).
- **No** → the client never receives the seed on that server type. That is
  a **design limit, not a defect**, and the honest product statement
  becomes "seed recovery for local/LAN worlds only".

## Targets

T1 (decisive first): `fn<0xa1cf6e8>` — caller of the gated path.
T2: `fn<0x1112ffe8>`, `fn<0x11157bd8>` — LevelData TU.
T3: `fn<0x11145608>`, `fn<0x111496b0>` — ctor-adjacent.
T4 (census): the remaining six.

## Withdrawn inference (recorded so it is not reused)

Earlier I argued the client "must know the seed or it cannot generate
terrain". **Withdrawn.** Server worldgen runs server-side and the client
renders received chunks; the client may hold no seed at all. The
`isRandomSeedAllowed` gamerule flag (`LevelData+0x45e`, `0x111321a8`)
exists precisely because seed access is a *policy* question on servers.

## Also withdrawn

"Check the world-settings seed field on the server" — **there is no seed
field in the server UI** (user, 2026-11-14). That test cannot be run.

## Known reporting defect, queued (NOT fixed here)

`Seed: 0 [recovered: src-scan:src+0x64]` — `src+0x64` is a FIELD-001
control window that reads 0 on **every** path. With an expected seed of 0
it "matched" and was promoted to recovered. Any all-zero window will
validate against any zero expectation. Fix belongs in the reporting layer
and is deliberately deferred until the registration behaviour is settled.
