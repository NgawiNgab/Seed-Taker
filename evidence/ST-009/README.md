# ST-009 evidence

- Date: 2026-11-14. Provided by: user (Zaphkiel run on-device).
- Target: MCPE 1.26.62, arm64 (same lib, new ASLR slide —
  findclass hits are live VAs `0x7964...`; fn disasm in file VAs).
- `zInput.txt` (3 targets): `fn<0x10b1db08>`, `fn<0x10b1d9c8>`,
  `findclass<LevelSettings>`.
- File: `zaphkiel_out.txt` (2168 lines / ~78 KB, verbatim copy of user
  upload `zaphkiel_out-9.txt`).
- Layout: lines 19-1085 target 1 (2-insn body + ~1050-line caller =
  the LevelData ctor, mislabeled `fn=0x11146b44` — F-066); lines
  1087-2153 target 2 (same shape); lines 2155-2168 target 3
  (10 findclass matches).
- T1 VERDICT: `0x10b1db08` = `ldr w0,[x0,#0x6c]; ret` (NOT src+0x74)
  → LevelData.+0x3a8 ← src+0x6c — PAIR HYPOTHESIS DEAD (F-063).
- T2: `0x10b1d9c8` = `ldr w0,[x0,#0x64]; ret` → LevelData.+0x3a0 ←
  src+0x64; known src reads 0x64/0x6c/0x78 all non-adjacent (F-064).
- T3: `13LevelSettings` confirmed real (`__shared_ptr_emplace` +
  cereal `CompositeSchema`/`TypeSchema` + Realms-upload ref) —
  prime src-class candidate; Education variants out of scope (F-065).
- Strategy out: LevelInit vehicle retired for ctor-hook (F-067);
  dense-run model predicts src int32s 0x64..0x78 (F-068) → ST-010.
- Numbering: issued as ST-010, filed as ST-009 to match the user
  upload sequence (PR #2's title predates the renumber).
