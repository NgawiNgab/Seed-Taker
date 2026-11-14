# ST-010 evidence

- Date: 2026-11-14. Provided by: user (Zaphkiel run on-device).
- Target: MCPE 1.26.62, arm64 (same lib; all addresses file VAs —
  no live-VA output this run, so no slide to record).
- `zInput.txt` (3 targets): `fullvf<LevelSettings>`,
  `hier<LevelSettings>`, `fn<0x10b1d9d0>`.
- File: `zaphkiel_out.txt` (1094 lines / ~39 KB, verbatim copy of user
  upload `zaphkiel_out-10.txt`).
- Layout: lines 19-23 fullvf FAILED ("typeinfo exists, no _ZTV");
  lines 25-27 hier FAILED ("not found"); lines 29-1094 fn (2-insn
  body + ~1050-line caller = the LevelData ctor, again mislabeled
  `fn=0x11146b44` — F-066 holds).
- T3: `0x10b1d9d0` = `ldrb w0,[x0,#0x69]; ret` — F-068 prediction
  wrong-as-stated, locationally consistent; dense-run → mixed
  int32/byte cluster (F-069).
- T1+T2: LevelSettings has NO vtable → non-virtual accessors, no
  vtable-slot vehicle (F-070).
- Out: 7-window getter-hook scan designed (F-071) + implemented
  (F-072). No ST-011 — implementation reports next.
