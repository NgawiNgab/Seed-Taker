# ST-013 — undumped setter callers (COMPLETE 2026-11-14)

Filed verbatim: `zaphkiel_out-13.txt` (3574 lines). The upload arrived named
`zaphkiel_out-12.txt`; the **content** is the 11-target ST-013 batch. All
11 targets dumped.

## Headline: all 11 call sites call the setter

`0xa1cf6e8, 0x1112ffe8, 0x11157bd8, 0x11145608, 0x111496b0, 0xa1cf72c,
0xa5d85e8, 0xa764564, 0xa77a3d4, 0xae30168, 0xb921f80` — every one
contains `bl 0x111463bc`. So ST-012's "callers (14)" counts **call sites**,
not distinct functions. The setter is reached from many more places than
the three characterised shapes, which is consistent with a fires=164 count
but does **not** by itself explain the `-1`.

Two findings that do matter:

### 1. `0xa764564` is the seam between decode and registration

It is the only function in the batch that calls **both**:

```
0xa764a08  bl 0x9b56e60   ; payload copy-ctor
0xa764a48  bl 0x9b56e60
...
0xa764564  bl 0x111463bc  ; seed setter (entry)
```

Copy runs 67%/71% into a 1772-byte body. But the copy **source** is a
stack staging buffer (`add x1, sp+#0x1, lsl #0xc; add x1,x1,#0x810`), not
src+0x0 — so this is copy **#2**, not the packet seam itself. It also
calls `0x111503bc`, `0x1112f1f4`, `0x1112f2cc` (same TU as ctor/setter,
still undumped).

### 2. The "server-seed" caller is a DEFAULT-SEED RANDOMIZER

`0xa1cf6e8` (caller of the gated path) calls
`0x11bc3fe0(buffer, &stack, 0x20, 1)` — an RNG helper — immediately
before the setter. Shape: `seed = RNG(); setter(this, seed)`.

**This is a strong explanation for `fires=164 last=-1`.** If the ctor
receives the seed by value (`x21 = x1` at `0x11146b70`) it registers
whatever it was handed, and a server/client **default** world is handed a
`-1` sentinel. The setter is behaving as a *default-seed randomizer*, not
as the carrier of the server seed.

Consequence: the plan I sketched before this run — "read the previously
registered value instead of the last one" — would have **amplified** this
error, not fixed it. It is discarded.

## Still not run

**ST-014 T1** (payload copy-ctor `0x9b56e60` body + callers) has never
been executed. `0x9b56e60` appears here only as a callee label, twice, with
no body and no caller list. That dump is the actual decode seam and is
what the next run must contain.
