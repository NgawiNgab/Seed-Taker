# SERVER-001 — first runtime report (F-089)

Verbatim user report, 2026-11-14, build = PR #4 / `4088169` (setter hook).

## Local World

- Seed successfully recovered: `-6348884760859271012`
- Source: `level-setter:seed register`

## Survival Server (Mega SMP)

- Seed remains `~ -1 (INFERRED)`
- `src+0x0 = -1`
- `src+0x60 = 1`
- `src+0x64..0x78 = 0`
- Validation status: `armed, expected 0`

## Reading

Local world: F-088 CONFIRMED. The value is exact and the source string is
the setter path, so hook install, anchors, x1 capture, seqlock and the
priority logic all work on-device.

Server: unresolved, and the report is under-determined — the old build
could not distinguish "setter never fired" from "setter fired with a
wrong value". Notably the HUD printed `-1` *with source `level-setter`
absent*, consistent with the fall-through bug: the legacy int32 packet
slot was 0 and the HUD showed a placeholder instead of the mismatch.

`armed, expected 0` is a separate defect: the expected seed was stored as
0 for "unset" and resurrected as a real expectation.

## Next run must answer

The new always-on line `setter: fires=N last=V`:

- `fires=0` → the server seed never reaches `0x111463bc`; F-086's
  single-registration premise is falsified for the MP path, and the hunt
  moves to the 13 other callers.
- `fires=N` with a value → the premise holds; only "is this the server's
  seed" is open.
