# ST-003 evidence

- Date: 2026-11-14. Provided by: user (Zaphkiel run on-device).
- Target: MCPE 1.26.62, arm64 (same lib, new ASLR slide).
- `zInput.txt` (4 targets): `fullvf<LevelData>`, `fullvf<StartGamePacket>`,
  `findclass<ScriptWorld>`, `hier<ScriptWorld>`.
- File: `zaphkiel_out.txt` (3235 lines, verbatim copy of user upload
  `zaphkiel_out-3.txt`).
- Layout: lines 18-716 `FULL_VFT: LevelData` (12 slots); lines 718-2806
  `FULL_VFT: StartGamePacket` (18 slots); lines 2808-3232
  `FINDCLASS: ScriptWorld` (418 matches); lines 3233-3235
  `HIERARCHY: ScriptWorld` (empty — no output).
- Address forms: `--- slot[N] fn=0x797... ---` is the live VA (run-scoped);
  `[VFT_SLOT_N] fn=0x...` + disassembly use stable file VAs. Only file VAs
  and names carry across runs.
