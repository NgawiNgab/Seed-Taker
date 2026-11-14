# CRASH-001 evidence

- Date: 2026-11-14. Provided by: user (on-device tombstone, xCrash 3.1.0).
- Build: PR #3 commit `3c7affa` (src-getter hook live).
- Device: Samsung SM-A566B, Android 17 (API 37), arm64, non-rooted.
- Crash: SIGILL (ILL_ILLOPC) at `0x796fbfca10` = lib+`0x10b1da10`,
  thread MINECRAFT MAIN, ~23s after start, during StartGame processing
  (slot-37 frame #03 @`0xa2fb398` → factory #02 @`0x1114fee0` → ctor).
- Cause (F-073): the 8-byte getter hook's ≥12-byte inline patch
  destroyed the neighbor function `0x10b1da10`, called by the ctor
  @`0x1114767c` (ST-007:2786; its result feeds `bl 0x11147d4c` —
  load-bearing, not dead code). lr = #01+4 exact.
- Corollary: slide + body-verify + detour + snapshot ALL worked —
  earlier ctor calls flowed through the hook before the crash.
- Fix: ctor-hook ASM wrapper (F-074).
- File: `tombstone.txt` (15897 lines, verbatim copy of user upload
  `tombstone_00001794642153008860_1.6.1__org.levimc.launcher.native.xcrash.txt`).
