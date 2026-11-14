# LeviMemEditor

**Native memory editor and reverse-engineering toolkit for Minecraft Bedrock on LeviLauncher / Launchroid.**

LeviMemEditor gives you a draggable memory editor inside Minecraft, with value scanning, saved addresses, memory freezing, a hex inspector, IDA-inspired signature tools, and Cheat Engine-inspired value-scan utilities. It works on the **Minecraft process itself**; it is not a standalone desktop debugger.

> **Current release:** v1.5.0  
> **Target Minecraft:** Bedrock 1.26.62.1  
> **Manifest compatibility:** `1.26.*`, `1.26.6*`  
> **Target launcher:** LeviLauncher / Launchroid 1.6.1  
> **Architecture:** Android ARM64 (`arm64-v8a`), API 26+

## Contents

- [Installation and controls](#installation-and-controls)
- [Results — value search](#results--value-search)
- [Saved — addresses and freezing](#saved--addresses-and-freezing)
- [Analyze — the IDA-inspired tools](#analyze--the-ida-inspired-tools)
  - [1. String search](#1-string-search)
  - [2. Hex / AOB search](#2-hex--aob-search)
  - [3. Address and RVA inspector](#3-address-and-rva-inspector)
  - [4. Find pointer references](#4-find-pointer-references)
  - [5. Generate wildcard signatures](#5-generate-wildcard-signatures)
  - [6. Test a signature](#6-test-a-signature)
- [CE Tools — value scans, pointers and hooks](#ce-tools--value-scans-pointers-and-hooks)
- [Common questions and troubleshooting](#common-questions-and-troubleshooting)
- [Limitations, safety and credits](#limitations-safety-and-credits)

## Installation and controls

1. Import `libLeviMemEditor_1.5.0.levipack` into **LeviLauncher / Launchroid 1.6.1** and enable it.
2. Disable older LeviMemEditor installations so that only **one** version loads.
3. Start Minecraft Bedrock. A floating **M** button should appear once the Minecraft Android activity is running, including at the main menu.
4. Tap **M** to show or hide the editor. The editor starts **hidden** by default; tapping **×** also closes it.
5. Drag the window by its top bar. Resize with the bottom-right grip. **Auto fit** scales the window for the current screen size and is enabled by default.

The editor is an offline **HTML/CSS/JavaScript WebView** backed by native C++; its JavaScript only drives the interface. It does **not** use Minecraft Script API. The M button is attached to Minecraft's Android activity, not a specific world; it cannot appear before that activity exists or above unrelated apps.

| Tab | Purpose |
| --- | --- |
| **Results** | Search for numeric values and edit matching addresses. |
| **Saved** | Keep addresses, rename them, freeze values and configure quick HUD buttons. |
| **Analyze** | Search strings and byte patterns, inspect memory, work with module RVAs and generate signatures. |
| **CE Tools** | Exact/unknown scans, change-based refinement, pointer chains and experimental branch hooks. |

## Results — value search

Use this tab when you know a value, for example a counter displaying `92`.

1. Choose a **value type**: Byte (8-bit), Word (16-bit), Dword (32-bit), Qword (64-bit), Float, Double, XOR or Auto.
2. Enter the current value in **Search value** and tap **Search**.
3. Change that value in-game, then enter the new value and tap **Filter results** using `=`, `≠`, `>`, `<`, `≥` or `≤`.
4. Repeat until you have a small set of candidates. A matched address is **not automatically the correct game variable**.
5. Edit a result's value or tap **Save**, **Freeze** or **HUD**.

**Tip:** For integer-looking values, try Dword or Byte first. For decimal values, try Float. Multiple representations and copies can coexist in memory. Scanning for a common number can return thousands of unrelated matches.

## Saved — addresses and freezing

- Tap **Save** on a result, or use **Add address** to enter a known address and value type manually.
- Tap a saved address label to **rename** it.
- Change its value using the editable value field.
- Tap **Freeze** to repeatedly write the chosen value (approximately every 50 ms while enabled), or tap it again to unfreeze.
- Tap **HUD** to configure a floating value button with an optional name, OFF value, ON value and toggle behavior.
- Delete entries that you no longer need.

**Addresses can become invalid** after restarting Minecraft, loading a different build or reallocating heap objects. The mod checks Minecraft library build identity for applicable saved module-relative entries, but temporary heap addresses should not be assumed stable across runs.

---

## Analyze — the IDA-inspired tools

**Start here if you have never used IDA Pro.** You do **not** need IDA installed. The Analyze tab is a live memory search and signature tool inspired by a few reverse-engineering workflows, **not** a disassembler or decompiler.

### The three terms to know

| Term | Meaning | Example |
| --- | --- | --- |
| **VA (virtual address)** | The address currently used by Minecraft while it runs. | `0x73946FF000` *(illustrative)* |
| **RVA (relative virtual address)** | An offset from a loaded module's base. | `libminecraftpe.so+0x123456` |
| **AOB / signature** | A sequence of bytes used to find matching data or code. Wildcards ignore changing bytes. | `FD 7B BF A9 ?? ?? 00 94` |

Android uses address-space layout randomization (ASLR), so **absolute VAs can change between launches**. Module-relative offsets are often more useful for the same library build. LeviMemEditor calculates an RVA relative to the first matching mapped region of the module; this may need adjustment when comparing against addresses shown in other tools.

### 1. String search

**Goal:** Find text stored in Minecraft's memory, such as a menu label or an internal identifier.

1. Open **M → Analyze**.
2. Set **Search kind** to **String / text**.
3. Set **Region scope** to **Minecraft .so** to start with `libminecraftpe.so`, or choose **All readable** to expand the search.
4. Enter text in **Search expression**, for example `minecraft:stone` *(an example, not a guaranteed in-memory match)*.
5. Select **UTF-8**, **UTF-16 LE** or **ASCII** under **Encoding**. Most ordinary ASCII text also matches UTF-8.
6. Optionally adjust **Case-sensitive** and **Null-terminated**, then tap **Find matches**.
7. Tap a hit under **Search hits** to view it in the **Address / RVA inspector**.

**If there are zero results:** Try a shorter string, a different encoding, disable **Null-terminated**, or broaden the region scope. Strings may be compressed, generated dynamically, optimized away, or stored outside `libminecraftpe.so`.

**Limits:** Input text is at most 512 UTF-8 bytes and the encoded search pattern is at most 256 bytes. Case-insensitive matching covers ASCII letter case, not full Unicode case folding.

### 2. Hex / AOB search

**Goal:** Find an instruction or data sequence by its bytes.

1. Set **Search kind** to **Hex signature**.
2. Choose **Minecraft .so**, **Named module**, **All readable**, **Writable** or **Executable** under **Region scope**.
3. For **Named module**, enter the library name, for example `libminecraftpe.so`.
4. Enter a pattern and tap **Find matches**:

```text
FD 7B BF A9 ?? ?? 00 94
```

The pattern is **illustrative**. It is not a verified Minecraft symbol or function signature.

| Token | Matches |
| --- | --- |
| `FD` | Exactly `FD` |
| `??` or `?` | Any whole byte |
| `4?` | Any byte from `40` to `4F` |
| `?F` | Any byte whose low nibble is `F` |
| `FD7BBFA9` | The same four bytes as `FD 7B BF A9` |

Patterns may use spaces or commas and can be **1–256 bytes**. An all-wildcard pattern cannot be searched because it would match almost everywhere.

**How to interpret results:** One hit is a good starting point, not proof of a stable function signature. Several hits mean your pattern is not unique in the scanned scope. Zero hits may mean the bytes changed, the region was excluded or the scan budget was exhausted.

### 3. Address and RVA inspector

**Goal:** View raw bytes at a known location and find its module-relative offset.

In **Address / RVA inspector**, enter one of these expressions and tap **Go**:

```text
0x73946FF000                  # Full VA; example only
libminecraftpe.so+0x123456    # Module + RVA
MC+0x123456                   # Shortcut for libminecraftpe.so
```

The inspector shows the module name (when identifiable), the relative offset, and a **128-byte hex-and-ASCII view**. In the hex listing, the left side is the address, the middle contains hexadecimal bytes, and the right shows printable ASCII characters (`.` for non-printable bytes).

**Important:** This does **not** interpret assembly instructions or show function names. A readable mapping is required. An RVA from a different Minecraft version may point to unrelated data.

### 4. Find pointer references

**Goal:** Look for locations that store a direct pointer to an address.

1. Change **Search kind** to **Pointer reference**.
2. In **Search expression**, enter a VA such as `0x73946FF000` or `libminecraftpe.so+0x123456`.
3. Select **64-bit** pointer size for normal ARM64 pointers (or 32-bit if specifically analyzing a 32-bit value).
4. Select the region scope and tap **Find matches**.

This searches for the **little-endian bytes of the address value** in memory. It finds **direct stored pointers**, not assembly call references, cross-references, or a complete pointer-chain scan.

### 5. Generate wildcard signatures

**Goal:** Turn an interesting memory range into a reusable hardcoded pattern that can be copied into native C++ mods.

1. Find a match via **String / text** or **Hex signature**, or type an address into the inspector and tap **Go**.
2. Scroll to **Signature generator**. The selected or inspected address is the starting point.
3. Set **Length, bytes** (from **4 to 256**; a useful initial trial for ARM64 code is 32–48 bytes).
4. Leave **Manual wildcard offsets** blank for an exact pattern, or enter offsets to ignore, e.g.:

   ```text
   4-7,16,0x20-0x27
   ```

   These are **zero-based byte positions within your selected range**. `4-7` masks bytes 4, 5, 6 and 7. Ranges are inclusive, and hexadecimal offsets use `0x`.
5. For **ARM64 code**, optionally enable **Auto-mask ARM64 PC-relative instructions**. This masks *entire four-byte instructions* recognized by heuristics (for example branches, ADR/ADRP and some PC-relative loads).
6. Optionally enable **Auto-mask in-process absolute pointers**, which masks aligned 8-byte words that look like currently mapped addresses.
7. Tap **Generate signature**.

The generator gives you two outputs:

**IDA-style wildcard pattern** — paste this into LeviMemEditor's **Hex signature** search:

```text
FD 7B BF A9 ?? ?? ?? ?? 00 00 80 52
```

**C++ hardcoded bytes and mask** — use these arrays in your own native scanner:

```cpp
// Example only; not copied from an actual Minecraft function.
constexpr uint8_t pattern[] = {
    0xFD, 0x7B, 0xBF, 0xA9,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x80, 0x52
};

constexpr uint8_t mask[] = {
    0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF
};

// 0xFF: compare the byte exactly.
// 0x00: ignore the byte at that position.
```

**Choosing good wildcards:** Mask bytes expected to change between builds or loads; keep enough stable bytes to distinguish the target. Do not blindly mask everything. On ARM64, auto-masking a whole PC-relative instruction is a conservative heuristic, **not** real instruction relocation analysis. Avoid enabling ARM64 instruction masking when you are making a signature for ordinary text or non-code data.

### 6. Test a signature

After generating the pattern, tap **Find this signature**. LeviMemEditor sends it back into the hexadecimal signature scanner for `libminecraftpe.so`.

- **1 match:** Promising for *this build and selected scan scope*; inspect it to confirm it is the intended location.
- **Several matches:** Too generic. Try more fixed bytes or a longer signature.
- **0 matches:** The scan scope, budget or bytes may be wrong, or your inspected address was not in the Minecraft module.

A signature can be unique today and break after a Minecraft update. **Recheck it on every target version.**

### Analyze options at a glance

| Setting | What it does |
| --- | --- |
| **Region scope** | Restricts scanning to Minecraft, a named library, all readable, writable, or executable mappings. |
| **Module / library name** | Used when the scope is **Named module**. |
| **Scan budget** | Caps the bytes scanned at 128 MiB, 512 MiB, 1 GiB, 2 GiB or 4 GiB. |
| **Search hits** | Shows up to 50,000 stored matches, **32 per page**. |
| **Loaded shared libraries** | Lists mapped `.so` modules for reference. |

---

## CE Tools — value scans, pointers and hooks

The **CE Tools** tab adds a smaller, Cheat Engine-inspired set of live-memory workflows. It is not an identical copy of Cheat Engine.

### Exact value and unknown-initial scans

1. Choose a value type and **Memory regions** (for typical item-count experiments, start with **Heap / anonymous (items)**).
2. Enter a value and tap **First scan**, or tap **Unknown initial** if you do not know the starting value.
3. Change the value in-game.
4. Under **Next scan type**, select **Changed**, **Unchanged**, **Increased**, **Decreased**, **Exact** or **Not equal**, then tap **Next scan** to narrow candidates.
5. Select a candidate, enter **New value**, then use **Write**, **Freeze / Unfreeze**, **Add to Saved** or **Hex view**.

**Example: 54 items → 64.** Tap **Set up stack scan (54 → 64)**, run **First scan** for 54, change the count normally (for example to 53), then narrow with **Decreased value**. When you have identified a likely address, write `64` and inspect the outcome. This preset fills fields; it does **not** directly edit Minecraft's ItemStack API. Copies of the count or server-authoritative state may overwrite the memory value.

**Limits:** CE Tools scans 32–512 MiB per operation, stores at most 180,000 scan candidates and displays 64 per result page. Large unknown-initial scans may not cover every mapping.

### Pointer chain

This is different from Analyze's **Pointer reference** search. In **CE Tools → Pointer chain**, enter a module-relative base and a list of offsets:

```text
Base:    libminecraftpe.so+0x100000
Offsets: 0x18, 0x30, 0x10
```

Tap **Resolve chain**. The tool reads successive **64-bit pointers**, applying the offsets in the supplied order, and selects the resulting address if all reads succeed. These are *illustrative* offsets, not a working Minecraft pointer chain. They must describe actual valid data in the current build.

### Experimental ARM64 branch hook

The **ARM64 branch hook** section applies a temporary 4-byte AArch64 branch patch at an executable source address. It requires a destination address and **exact original instruction bytes**, and checks alignment, executable mappings, branch range and expected bytes before writing. Installed hooks can be restored using their displayed ID.

**Use only when you understand AArch64 code and have a backup of your world.** This is **not** a trampoline or full function hook: it cannot automatically relocate overwritten instructions, call the original function, build a code cave, or bypass Android memory protection rules. A wrong branch may crash the game; Android W^X restrictions may prevent the patch entirely. Hooks are temporary, not saved across mod restarts.

## Common questions and troubleshooting

**Why does searching a value or string return nothing?**  
Try another type/encoding or scan scope. Increase the budget if necessary. The value may not exist as a simple raw byte sequence, or may be outside the selected mapped regions.

**Why do I get thousands of matches?**  
Common values and short byte patterns are frequent. Filter after an in-game change, search a longer pattern, or restrict the scope.

**Why doesn't an address work after restarting?**  
Virtual addresses change with ASLR, memory allocation and Minecraft updates. Use a verified module+RVA for the same build, a stable pointer chain, or a new signature scan.

**Why did my generated signature stop working after an update?**  
The code or data changed. Regenerate the pattern against the new build and verify uniqueness. Automatic wildcards are heuristics, not a promise of cross-version compatibility.

**Does Analyze show C++ source, assembly graphs, or decompiled functions?**  
No. It provides raw bytes, ASCII, address navigation, pointer-value matches and signature generation. It does not provide IDA Pro's instruction disassembly, decompiler, function analysis or full cross-reference graph.

**Does the mod need root or attach to another process?**  
No. The included engine works within the Minecraft process into which LeviLauncher loads the mod, not arbitrary third-party processes.

**Known UI issue in v1.5.0:** The bundled CE Tools HTML currently reuses the `ceNext` element ID for **Next scan** and the result pager's **Next** button. Because of that duplicate ID, forward pagination in CE Tools may not respond correctly. This documentation does not change the binary; a future code update should use distinct IDs for those controls.

## Limitations, safety and credits

- Built for **Minecraft Bedrock 1.26.62.1**, LeviLauncher / Launchroid **1.6.1**, Android **ARM64**. Manifest patterns are `["1.26.*", "1.26.6*"]`; matching a version pattern does **not** guarantee addresses or hooks remain compatible across builds.
- Work is limited to readable/mapped memory in the running Minecraft process. Scans and hooks are not a replacement for a full native debugger or game data API.
- Editing a wrong address, freezing live structures or patching executable code can crash Minecraft or damage world state. Prefer testing in an expendable **singleplayer** world. Multiplayer servers may reject or correct client-side edits.
- The v1.5.0 release was validated by native host tests, ARM64 compilation and browser UI checks. The release documentation does **not** claim every feature was verified on a physical Android device.
- The reverse-engineering tools are **inspired by** familiar workflows; they do not contain IDA Pro or Cheat Engine proprietary code, nor claim feature parity with those products.

**Author:** TheRealBatu  
**Original launcher memory-editor basis:** [LiteLDev / LeviLauncher-Android at commit `b373b146`](https://github.com/LiteLDev/LeviLauncher-Android/tree/b373b146f9cb9c7b3eebf9311f125c9405b88989)  
**License:** Apache License 2.0; see the included `LICENSE` and `PORTING_NOTES.md` for attribution and porting details.  
**Original M-button textures:** Supplied Levi inbuilt-button template assets.
