# Seed-Taker Specification: Absolute Seed Taker

Source: user-stated requirements, 2026-11-14 (branch
`arena/26e6ac23-seed-taker`). These are CONFIRMED as requirements; technical
feasibility of each item is a separate, evidence-gated question (see
`docs/EVIDENCE_LOG.md`).

Target: MCPE 1.26.62, arm64-v8a, LeviLauncher (Android). Standalone native
mod — no BedrockTools dependency.

## Objective

Retrieve and display the world's actual origin seed, regardless of whether
the client receives a spoofed or modified seed value.

## Requirements

1. **Local world seed.** Retrieve the actual seed of a locally hosted
   single-player world.
2. **Multiplayer / server world seed.** Investigate whether the actual
   server-side world seed can be recovered from the client. Do not simply
   report the client-side seed if it may have been spoofed.
3. **Spoof detection and separation.** Distinguish, as separate values:
   - actual world-generation seed,
   - server-provided seed,
   - client-side seed,
   - spoofed or modified client seed.
   
   Example: if a server forces the client-visible seed to `0`, Seed-Taker
   must not automatically report `0` as the actual world seed.
4. **Evidence and confidence.** Every reported value carries:
   - how it was obtained: directly recovered, inferred, or unavailable;
   - the source of the value.
   
   Never present a guessed or inferred seed as confirmed.

## Central research question

Can the original world-generation seed still be recovered when the server
spoofs the seed value exposed to the client? Do not assume this is always
possible. Investigate the Bedrock architecture, identify where seed
information originates and how it reaches the client, and determine what
information remains accessible in multiplayer environments.

## Non-goals / constraints

- No speculative offsets, signatures, or native calls (see evidence gates in
  `docs/ARCHITECTURE.md`).
- No inferred value may be rendered without an explicit INFERRED marking
  (enforced in `SeedModule::onFrame`).
- Bedrock seeds are signed 32-bit on the wire (Start Game `Seed`:
  SignedVarInt per public protocol documentation); negative seeds must
  render correctly.

## Acceptance sketch

- SP world: actual seed shown as RECOVERED with its source.
- MP world, unspoofed: server-provided and client-side seeds shown with
  sources; relationship between them stated only as far as evidence allows.
- MP world, spoofed (e.g. client-visible `0`): the `0` is reported as the
  spoofed/client-visible value, never as the world-generation seed; the
  world-generation seed is reported as INFERRED (with method) or
  UNAVAILABLE, never fabricated.
