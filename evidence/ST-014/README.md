# ST-014 — where the decrypted server seed lands (ISSUED 2026-11-14)

## The question

> Where exactly does 1.26.62 store `StartGamePacket.seed` after decryption
> and decoding?

## Answer from evidence (F-090), before any new run

It is stored at **src+0x0** — the same slot as every other world, and the
same slot the LevelData ctor already reads through `ab28`
(`ldr x0,[x0]; ret`, F-087).

The chain, each link sourced:

1. **Wire**: `seed` is `int64` little-endian inside LevelSettings.
   Codec v291 read it as 32-bit; **v527 switched to `readLongLE()`** and
   v671→v898 inherit that unchanged. So the field is 64-bit on the wire.
2. **Decode**: after decryption the packet is parsed into a
   `StartGamePacket` whose `seed` member is `long` (64-bit).
3. **Hand-off**: the decoded payload is copied out by the payload copy-ctor
   `0x9b56e60`, which opens with an overlapping
   `ldur q0,[x1,#0xc]; ldr q1,[x1],#0x20; stur q0,[x0,#0xc]; str q1,[x0],#0x20`
   — a 16B+16B block copy of `src[0x00..0x20]` into `[0x00..0x20]`, which
   **covers 0x0C..0x1C**: seed, spawnBiomeType, customBiomeName,
   dimensionId, generatorId.
4. **Access**: `ab28(src)` returns `src+0x0` as a u64 — and local worlds
   prove it holds the real seed (F-089 recovered the exact value).

## Why our hooks missed it anyway

The **location** was never wrong; the **timing and the vantage point** were:

- The LevelData ctor-entry snapshot fires too early on a server (`-1`).
- The wire is **encrypted**, and decryption happens inside the client's
  network stack — so a hook at packet-header level cannot see the seed.
  A proxy (Lumina) can, because it already holds plaintext.

## Trap ruled out

Lumina applies **no seed masking or filtering** — a repo-wide grep for
`seed` in its app code yields only extraction, the `!seed` command, two
UI overlays, and a LevelDB `putLong("RandomSeed", seed)`. On the servers
it supports, the extracted value is genuine. So "server withholds the
seed" is not supported by that implementation.

## What ST-014 dumps

T1 is the answer: the payload copy-ctor's body and callers, to confirm
that a **server** packet reaches it, and to place a hook on the
**source** side (`x1`) rather than the destination (`x0`).

If T1 shows the copy-ctor is reached on MP decode, the fix is a hook at
`0x9b56e60` + snapshot of `x1+0x0` — 29 bytes of straight-line prologue,
`0xb56e60` unique, no anchor pair needed.
