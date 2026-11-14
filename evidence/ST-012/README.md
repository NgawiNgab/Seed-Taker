# ST-012 — setter + src getter dumps (COMPLETE 2026-11-14)

Input as issued: `zInput.txt` (P0/P1/P2).

## P0 (COMPLETE)

Verbatim (432 lines): `zaphkiel_out_P0-setter.txt` (user file
`zaphkiel_out_P0.txt`). `fn<0x111463bc>`: the setter REGISTERS the u64
seed as a type-7 gamerule (rule name #0x8b0, container this+0x4f8,
registrar 0xd05546c = hash-map insert-or-find), stores x1→rule+0x38
with tag handling, notifies via 0x1114e3e0(this,9). NO direct member
store — container-exclusive home. 14 callers, 1 dumped (0xa1bd1b0;
F-038 7th instance: "14" vs 13 listed). Staleness: nil (idempotent
re-sets; hook-the-setter is the designed fallback). Tool quirk:
`return_type (guess): unknown (no ret found)` despite a real ret.
Finding F-086.

## P1 (COMPLETE)

Verbatim (281 lines): `zaphkiel_out_P1-getter.txt` (user file
`zaphkiel_out_P1.txt`). `fn<0x10b0ab28>` = `ldr x0,[x0]; ret` ⇒ src+0x0
= the u64 seed (World A confirmed three ways). Dumped caller 0xbf2b1b0
(world-creation serializer) files the return under literal "WorldSeed"
(+Mode/SaveId/fromTemplate/…, incl. 0x10b1da20 =
EducationFeaturesEnabled). Legacy story closed (packet+0xA8 chain =
legacy 32-bit slot). Runtime design: window-8 (src+0x0 as window 0 +
7 controls), implemented uncommitted. Finding F-087.

## P2 (DROPPED as superseded)

`struct<SeedHandler>` (no offset) failed: "bad struct format. Use
`struct<ClassName><0xOFFSET>`" (tool-relayed; offset semantics
unknown — presumably vtable-start, `0x0` untested). Not re-issued:
the on-device u64 verdict dominates static UI corroboration, and the
seed's homes are resolved. Re-issuable iff validation fails. Syntax
intel filed here for future STRUCT use.
