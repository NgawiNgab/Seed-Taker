# ST-007 evidence

- Date: 2026-11-14. Provided by: user (Zaphkiel run on-device).
- Target: MCPE 1.26.62, arm64 (same lib; file VAs only in output).
- `zInput.txt` (3 targets): `xref_tree<0x1114eec8>`, `fn<0xe804a08>`,
  `fn<0x11146b24>`.
- File: `zaphkiel_out.txt` (3514 lines / ~124 KB, verbatim copy of user
  upload `zaphkiel_out-7.txt`).
- Layout: lines 19-43 xref_tree (IDENTICAL result to ST-005's LDR-site
  query: root `0x1114edec`, same callees, same single virtual caller
  `blr @ 0x68b350c`); lines 46-2053 fn2 (VENEER → PLT `0x135efde0`,
  ~2000-line PLT runaway, 0 callers — identity opaque); lines 2055-3514
  ctor (disasm 2059-2977; 143 callees; "callers (2)" but only ONE dumped:
  `0x9b372b0` — fn2/`0x1114fe58` known-but-undumped, F-038 3rd instance).
- Breakthrough: the ctor writes `[x19+0x3b0]` ← getter `0x10b1da08(src)`
  (line ~2241) and also `+0x440`, `+0x5b8` → constructed object is
  LevelData (behavioral triangulation; vtable file VA `0x13e6eda0` has no
  independent anchor). MP chain: payload → `0x9b56e60` → src-obj → fn2 →
  ctor → LevelData.+0x3b0. ST-008 resolves the last two hops.
