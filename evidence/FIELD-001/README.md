# FIELD-001 — first crash-free src-scan run (on-device, SP, 2026-11-14)

Verbatim mod output: `mod_output.txt` (user-pasted; one chat-escape
artifact normalized: `\[!` → `[!` — the HUD renders `[!`, chat escapes
the bracket).

## Verdict

- Hook SURVIVED a full world load: no crash. F-074 CONFIRMED
  on-device (logged as F-075). First crash-free hook run.
- Scan executed and published all 7 windows. Verdict **0/7**: the
  seed is NOT in src+0x60..0x78 as a plain int64/u64 (F-076).
- Packet path again 0 on SP loopback (expected, reconfirms F-058).

## Decoded windows (expected seed u64 = 0xCAF654EE5A4FB235)

| window | value | decoded |
| --- | --- | --- |
| +0x60 | 1 | small int/flag |
| +0x64 | 0 | — |
| +0x68 | 0 | — |
| +0x6c | 0 | — |
| +0x70 | 190099547488256 | 0xACE500000000 (lo=0, hi=44261=0xACE5) |
| +0x74 | 44261 | upper half of the +0x70 u64 — self-consistent |
| +0x78 | 0 | — |

Seed bytes LE (`35 B2 4F 5A EE 54 F6 CA`) appear nowhere in this
region. The region corroborates the mixed int/bool settings cluster
(F-068-refined/F-069): small fields, not the seed.

## Attempt 1 (memory editor, 2026-11-14)

Verbatim: `memeditor_1.txt`. AOB 0 hits / 323 MiB (likely .so-only
scope — heap never searched, inconclusive); Qword exact 4038 hits
(implausible count for a random u64 — unverified, needs byte
inspection). Logged as F-078.

## Attempt 2 (memory editor, 2026-11-14)

Verbatim: `memeditor_2.txt`. AOB over All-readable = 0 hits / 4096
MiB BUT budget reached (coverage possibly partial — not proof of
absence). Qword still 4038, bytes still uninspected; AOB=0 vs
Qword=4038 proves the Qword search is not an exact byte match.
Logged as F-079; world-identity check + Writable-scope AOB +
string/halves searches pending.

## Attempt 3 (memory editor, 2026-11-14)

Verbatim: `memeditor_3.txt`. World-identity CONFIRMED (wrong-world
hypothesis closed). Full-seed 3848 + lo32 3860 for different values
⇒ Results-tab candidate sets are value-independent garbage (quirk
CONFIRMED, tab discredited for exact work). hi32 1 hit @0x799F1A2AE0
but displayed bytes contain no seed-half sequence — inspector
arbitration pending. Logged as F-080.

## Next

Seed-origin still open: (a) elsewhere in the src object, outside
0x60..0x78; (b) in LevelData (`this`, x0 — the wrapper currently
snapshots x1 only); (c) another object entirely. Method: live hunt
with LeviMemEditor v1.5.0 (tool spec filed alongside as
`LeviMemEditor-README.md`, Apache-2.0, matches our stack exactly:
1.26.62.1 + LeviLauncher 1.6.1 arm64) — AOB-search the known seed
bytes while the SP world is loaded, identify the container via
pointer references, map back to struct+offset (F-077). If the
container is LevelData, extending the wrapper to snapshot x0 windows
is trivial (it already preserves everything).
