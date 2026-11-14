# ST-002 evidence

- Date: 2026-11-14. Provided by: user (Zaphkiel run on-device).
- Target: MCPE 1.26.62, arm64 (same lib as ST-001, different ASLR slide —
  VAs differ run to run and must not be reused as addresses).
- `zInput.txt` (6 targets): `hier<LevelData>`, `hier<StartGamePacket>`,
  `findclass<getSeed>`, `findclass<GetSeed>`, `findclass<LevelSeed>`,
  `findclass<StartGamePacketPayload>`.
- File: `zaphkiel_out.txt` (69 lines, verbatim copy of user upload
  `zaphkiel_out-2.txt`).
