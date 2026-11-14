# Seed-Taker

Standalone native Levi mod (Minecraft Bedrock, Android arm64) that captures and
displays the world seed. Target: **MCPE 1.26.62**.

Status: **initial implementation**. The mod lifecycle, module registry, event
bus, config store, launcher menu, HUD draw path, `.levipack` packaging, and
the evidence-backed seed-capture path (Zaphkiel investigations ST-001..ST-008,
see `docs/EVIDENCE_LOG.md` and `evidence/`) are implemented. Seed-ness of the
captured field is INFERRED-strong pending on-device runtime validation; see
`docs/IMPLEMENTATION_PLAN.md`.

- `docs/ARCHITECTURE.md` — module map, data flow, deviations from the reference.
- `docs/BEDROCKTOOLS_REFERENCE.md` — BedrockTools findings (reference only;
  Seed-Taker never links against or requires BedrockTools).
- `docs/EVIDENCE_LOG.md` — classified findings and what would confirm them.

## Build

Requirements: Android NDK r28c, xmake, Python 3.

```sh
xmake f -y -p android -a arm64-v8a -m release --ndk=/path/to/android-ndk-r28c
xmake -y
```

The release build produces `libSeedTaker.so` and `SeedTaker.levipack`.
CI (`.github/workflows/build.yml`) builds the same way on push/PR.

