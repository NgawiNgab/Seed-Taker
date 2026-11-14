# ST-008 evidence

- Date: 2026-11-14. Provided by: user (Zaphkiel run on-device).
- Target: MCPE 1.26.62, arm64 (same lib; file VAs only in output).
- `zInput.txt` (2 targets): `fn<0x9b56e60>`, `fn<0x10b1da08>`.
- File: `zaphkiel_out.txt` (3753 lines / ~138 KB, verbatim copy of user
  upload `zaphkiel_out-8.txt`).
- Layout: lines 19-3684 extractor (disasm 22-294; 30 callees;
  "callers (16)" but only ONE dumped: `0x9b54ebc` mid-function ctx,
  single `bl` at `0x9b54efc` — F-038 4th instance); lines 3686-3753
  getter (2-insn body `ldr w0,[x0,#0x78]; ret`; "callers (2)" but only
  ONE dumped: `0xd824364` cereal-wrapper ctx — F-038 5th instance,
  ctor known-but-undumped).
- Chain closed: getter → `+0x3b0` = src+0x78; extractor = payload
  copy-ctor, src+0x78 ← payload+0x78 via the [0x60–0x7F] SIMD window
  with NO overwrite (zero `0x78` mentions in body) → LevelData.+0x3b0
  = StartGame packet+0xA8 (int32). Seed-ness: INFERRED-strong
  (5 indicators); wire-map → runtime validation. RE phase complete;
  no ST-009 (sig-gen optional backup only).
