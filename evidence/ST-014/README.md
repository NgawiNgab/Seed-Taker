# ST-014 — the decode seam (COMPLETE 2026-11-14)

Filed verbatim: `zaphkiel_out-14.txt` (3682 lines; the upload arrived as
`zaphkiel_out-13.txt` — same naming confusion as ST-013, the content is
this batch). Single target, and it answered everything it was asked.

## 1. The seam

The copy-ctor's caller list contains **`0xa2fabd4`**, which is inside the
StartGame packet handler `0xa2faa10` (F-033) at **+0x1C4**.

The handler binds the packet at `x20` — `0xa2fabac add x2, x20, #0x4b0`.
Then:

```
0xa2fabcc  add  x0, sp, #0x7d0      ; destination: handler stack
0xa2fabd0  add  x1, x20, #0x30      ; source: packet + 0x30 = LevelSettings
0xa2fabd4  bl   0x9b56e60           ; payload copy-ctor
```

So the decoded StartGamePacket is copied into handler stack at
`sp+0x7d0` on **every** StartGame — server included. That is the moment
the server seed exists in client memory in decoded form.

The copy-ctor body confirms the field layout:

```
0x9b56e98  ldur q0, [x1, #0xc]
0x9b56e9c  ldr  q1, [x1], #0x20
0x9b56ea0  stur q0, [x0, #0xc]
0x9b56ea4  str  q1, [x0], #0x20
```

The overlapping q0/q1 stores also fill dst+0x00..0x0C from src+0x0C..0x1C,
which is `[u64 seed, u16 spawnBiomeType]`. **The seed is LevelSettings
+0x0C**, matching the codec where `seed` is the first LevelSettings field.

## 2. Why the setter never saw the server seed

`0x111463bc` (the seed setter) does **not** appear anywhere in the
StartGame handler body, and neither does the LevelData ctor `0x11146b24`.
The handler instead reads the stack LevelSettings through the
`0x10b1dxxx` family — one int/byte getter per field.

So the setter is the **world-settings** path (what F-091 showed to be an
RNG/default-seed randomizer), and the **packet path never touches it**.
`fires=164 last=-1` is fully explained and is not a bug: the setter is
simply not the server seed's carrier.

## 3. Hook safety

`0x9b56e60` is a 0x144-frame function with a 23-instruction prologue
before its first `bl` at `0x9b56ea8`. It comfortably satisfies the F-073
rule (never hook anything under 16 bytes / no 8-byte getters).

## 4. What remains

Only the timing/ownership of `sp+0x7d0` after the handler returns — i.e.
where the decoded settings object is *handed off*. The seed's location on
the packet side is settled: `packet+0x30+0x0C` at the copy, `sp+0x7d0+0x0C`
after it.

---

## CORRECTION (2026-11-14, from the Lumina Client study — F-097)

§1 above says the overlapping q0/q1 stores "fill dst+0x00..0x0C from
src+0x0C..0x1C, which is `[u64 seed, u16 spawnBiomeType]`". The seed
location (`LevelSettings +0x0C`) is **right**; the bracket is **off by
four bytes**. The pair is `+0x0C` (u64 seed) and `+0x14` (u16
spawnBiomeType) — if the seed ended at `+0x14`, spawnBiomeType could not
start there without padding, and `buffer.writeShortLE` at `+0x14` needs
none.

Also, the opening four instructions are **one 28-byte inlined `memcpy`**
covering `[src+0x00, src+0x1C)` completely, not four independent copies.
`ldr q1,[x1],#0x20` is a post-index load: it reads at `x1` and only then
adds `0x20`. Two 16-byte accesses at offsets `0` and `0x0C` are clang's
`memcpy(N)` idiom for `16 < N <= 32` (`N - 16 == 0x0C` ⇒ `N = 0x1C`).
The `+0x00` start is explained by `algorithmVersion`, written before the
seed by the v419/v534/v671 serializers.

Two further facts from the same body, for whoever reads this next:
`src+0x30` is a **u16** (`ldrh/strh` at `0x9b56eb4/eb8`) — not a seed
word; and **the seed has no dedicated load in this function at all**, it
only moves inside those two vector stores, which is why the band has to
be sampled *before* the call rather than after.

Nothing in §2–§4 is affected. The location and the seam are unchanged;
this is arithmetic hygiene in the "what covers what" sentence, recorded
here rather than silently edited because §1 was filed as a finding.
