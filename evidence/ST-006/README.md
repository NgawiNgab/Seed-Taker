# ST-006 evidence

- Date: 2026-11-14. Provided by: user (Zaphkiel run on-device).
- Target: MCPE 1.26.62, arm64 (same lib; file VAs only in `fn<>` output).
- `zInput.txt` (4 targets): `fn<0x1114ecac>`, `fn<0x1114fe58>`,
  `fn<0x68b3158>`, `fn<0x101c5f64>`.
- File: `zaphkiel_out.txt` (8268 lines / ~294 KB, verbatim copy of user
  upload `zaphkiel_out-6.txt`).
- Layout: lines 18-1295 fn1 (disasm 22-44; 4 callees; "callers (4)" but
  only ONE caller context dumped — continuous slot37 region 0xa2fc1ec+,
  single `bl` site at `0xa2fc22c`); lines 1296-3655 fn2 (disasm
  1300-1364; 10 callees; 1 caller); lines 3656-6110 fn3 (disasm
  3660-4084 — starts MID-FUNCTION, no prologue; 22 callees; 0 callers);
  lines 6113-8268 fn4 (5-insn disasm 6118-6124; 1 caller).
- Verdicts: fn1 = 16B conditional-update helper (pkt+0x4e0 = blob, not
  seed); fn2 = factory feeding &pkt+0x4c8 to ctor `0x11146b24`;
  fn3 @ `0x68b350c` READS `+0x3b0` via slot 8 (getter) and forwards the
  value to `0xe804a08`; fn4 = u16+byte extractor (pkt+0x50c/0x510, not
  seed). The SETTER's writer is still unknown (ST-005's xref answered the
  reader) → ST-007 `xref_tree<0x1114eec8>`.
