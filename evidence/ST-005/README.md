# ST-005 evidence

- Date: 2026-11-14. Provided by: user (Zaphkiel run on-device).
- Target: MCPE 1.26.62, arm64 (same lib, new ASLR slide:
  file_va = live_va - `0x7969219000`; live VAs run-scoped, never reused).
- `zInput.txt` (3 targets): `vft<LegacyClientNetworkHandler><37>`,
  `findclass<LegacyClientNetworkHandler>`, `xref_tree<0x1114eeb0>`
  (LevelData slot-9 setter LDR site, file VA from ST-004).
- File: `zaphkiel_out.txt` (4343 lines / ~157 KB, verbatim copy of user
  upload `zaphkiel_out-5.txt`).
- Layout: lines 19-4296 slot-37 decompile (fn file VA `0xa2faa10`;
  disasm 37-2038, no `ret` in range; 253 callees with veneer→PLT dumps
  2039-4295; 0 direct callers); lines 4298-4318 findclass (14 matches,
  incl. `handle(...StartGamePacket...)::$_1` lambda symbols);
  lines 4320-4343 xref_tree (0 direct callers; 1 virtual caller:
  `blr @ 0x68b350c` in fn `0x68b3158`).
- Register note (verified by branch-target analysis): x20 = packet on all
  packet-read paths; the `mov w20,#1` / `cset w20,eq` / `mov w20,wzr`
  writes are a flag lifetime confined to the early-exit convergence at
  `0xa2fb448` (all three clobber paths jump there and leave via
  `b 0xa2fd268`), while packet-processing paths jump OVER the clobbers
  (`0xa2fb3e8 cbz → 0xa2fb4b4`, `0xa2fb440 b → 0xa2fb4d4`).
