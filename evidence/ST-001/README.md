# ST-001 evidence

- Date: 2026-11-14. Provided by: user (Zaphkiel run on-device).
- Target: MCPE 1.26.62, arm64.
  `lib: /data/data/org.levimc.launcher/files/minecraft_libs/com.mojang.minecraftpe/arm64/libminecraftpe.so`
  (342,231,392 bytes, 7 segments).
- `zInput.txt` (4 targets):
  `findclass<LevelData>`, `findclass<LevelSettings>`,
  `findclass<StartGame>`, `findclass<Seed>`.
- File: `zaphkiel_out.txt` (265 lines, verbatim copy).

NOTE: addresses in the output are live ASLR-slid VAs from that run, not
stable RVAs. Stable identifiers are the raw RTTI/symbol strings. A string
match alone does not prove semantic identity (see `docs/EVIDENCE_LOG.md`).
