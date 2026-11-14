# ST-011 — consumer-side seed-origin hunt (PARTIAL 2026-11-14)

Input as issued: `zInput.txt`. NOTE on labels: the user's return files
are named T1/T2, but by content the `T2` file is the issued T3
(`findclass<RandomSeed>`) — the issued T2 (conditional `fn<>`) has not
run. Files below are renamed by content (verbatim bytes, cmp-checked).

## Run 1 verdict

- `zaphkiel_out_T1_struct-misfire.txt` (user file `zaphkiel_out_T1.txt`):
  bare `STRUCT SeedHandler` routed to the STRING path (`NOT FOUND`) —
  command misfire, not a negative. Proves the syntax needs angle
  brackets (retry: `struct<SeedHandler>`) and usefully PROVES a raw
  string-search path exists.
- `zaphkiel_out_T3-findclass-RandomSeed.txt` (user file
  `zaphkiel_out_T2.txt`): `findclass<RandomSeed>` ran correctly → no
  typeinfo contains `RandomSeed` ⇒ not an RTTI class (NBT key string
  only). Backup front pivots to raw string search + xref.
- Retrospective: lib loads 342231392 B ≈ 326 MiB ≈ memeditor
  attempt-1's 323 MiB ⇒ attempt-1 AOB was .so-only, CONFIRMED.

## Retry (issued same turn)

`zInput-retry.txt`: `struct<SeedHandler>` + bare `RandomSeed` string
search. The issued T2 (`fn<>` on T1's seed-reading method) stays queued
behind T1. Logged as F-083.

## T3-string (COMPLETE 2026-11-14)

Verbatim (6764 lines): `zaphkiel_out_T3-string-RandomSeed.txt`.
`RandomSeed` @0x258f803, 2 xrefs: LOAD fn 0x11130798 (u64 seed →
`bl 0x111463bc`, legacy sxtw path at StorageVersion≤8) + SAVE fn
0x1114afe4 (container lookup + type==7 → NBT put). Findings F-084
(loop + setter VA) / F-085 (setter-sharing ⇒ ab28 src seed getter;
F-082 corrected; F-051 refuted). Follow-up: ST-012.
