# ST-004 evidence

- Date: 2026-11-14. Provided by: user (Zaphkiel run on-device).
- Target: MCPE 1.26.62, arm64 (same lib, new ASLR slide).
- `zInput.txt` (3 targets):
  `fullvf<31PacketHandlerDispatcherInstanceI15StartGamePacketLb0EE>`,
  `struct<LevelData><0x3b0>`, `struct<StartGamePacket><0x748>`.
- File: `zaphkiel_out.txt` (100716 lines / ~3.6 MB, verbatim copy of user
  upload `zaphkiel_out-4.txt`).
- Layout: lines 18-100376 dispatcher vtable (slots 0-2 real; slot[1],
  lines 31-100036, is a ~100K-line linear-disasm run through the PLT
  reached via a veneer — tool artifact, no semantic content; slots 3-5
  invalid/past-end); lines 100377-100678 `STRUCT: LevelData<+0x3b0>`;
  lines 100680-100716 `STRUCT: StartGamePacket<+0x748>`.
- `struct<>` scopes to class methods (45 for LevelData, 101 for
  StartGamePacket), not binary-wide; adjacent functions share attribution
  of the same access sites (tool artifact — see F-030).
