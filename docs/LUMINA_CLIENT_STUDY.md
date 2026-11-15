# Lumina Client — Structural Study

Source studied: `github.com/NgawiNgab/LuminaClient` @ `0a062f0`
("PUBLIC-4.1.8.2"), cloned to `/tmp/LuminaClient` (9 137 tracked files,
73 MB). GPLv3. **Reference only** — Seed-Taker links nothing from it and
copies no code from it; the value here is the *protocol and object-layout
ground truth* it documents, the same way `BEDROCKTOOLS_REFERENCE.md`
documents mod-hosting patterns.

Why this repo matters to us: Lumina is a public, working Bedrock client
that reads the world seed on servers. Whatever it does to obtain a server
seed is a proven-shippable existence proof. Reading it tells us the shape
of the answer even where it cannot tell us the addresses.

---

## 1. The headline: Lumina never touches `libminecraftpe.so`

This is the single most important structural fact, and it reframes our
whole difficulty.

`grep -rn "libminecraftpe\|mprotect\|ptrace\|memorySearch"` over the
entire tree returns **one** hit, and it is `xdl_linker_force_dlopen`'s
prototype in a vendored header. There is no in-process hook of the game
at all. The overlay hooks `libinput.so`, not Minecraft — detail in §6.4.

Lumina is a **RakNet relay (a proxy that terminates the session)** that
runs as its own Android app:

```
Minecraft (unmodified, stock)
   │  joins "localhost:19132" / the LAN server entry Lumina injected
   ▼
Lumina app  ── RakServerChannel on :19132  (advertises a fake pong)
   │        ── BedrockCodec decodes CLIENT_BOUND packets
   │        ── re-encodes and forwards to the real server
   ▼
Real server (Hive / LBSG / the Mega SMP)
```

Every packet in both directions is decrypted, decompressed, decoded into
a POJO (`StartGamePacket`), inspected, re-encoded and forwarded. Lumina
therefore holds the server's seed as a **typed `long` field on a Java
object** — it never needs an offset, a signature, or a hook.

**Consequence for Seed-Taker:** our two approaches are not competing
implementations of one thing. They sit on opposite sides of a wall:

| | Lumina | Seed-Taker |
|---|---|---|
| Runs inside the game process | no | yes |
| Sees ciphertext | no (it *is* the endpoint) | yes, briefly |
| Sees plaintext | always | only after the game decrypts |
| Needs a VA/offset | never | always |
| Wears off when MC updates | yes (codec table) | yes (offsets) |

The wall is the encryption handshake. Lumina terminates it (`EncryptedLoginPacketListener`,
§6.3); an in-process hook sits *below* it and only ever observes the
already-decrypted result. That is exactly why every wire-level hook we
tried read zeros, and it is **not** evidence that the server withholds
the seed.

## 2. The seed feature itself is four lines

```
GameDataManager.kt:269   fun getSeed(): Long? = getStartGameField("seed") as? Long
NetBound.kt:219          Log.i("GameSession", "Seed: ${gameDataManager.getSeed()}")
NetBound.kt:340          fun getWorldSeed(): Long? = gameDataManager.getSeed()
GameInfoCommands.kt:38   val seed = netBound.getWorldSeed()
```

`getStartGameField` is reflection over the packet object's declared
fields (`extractFieldsFromObject`, `GameDataManager.kt:165`), stored when
`StartGamePacket` arrives at `NetBound.kt` beforeServerBound. It is
surfaced as the chat command `!seed` (`CmdListener.kt:197`), the
WorldStats overlay (`overlay/kitsugui/WorldStats.kt:152`), HomeCategoryUi
and a LevelDB `putLong("RandomSeed", seed)` when Lumina writes a local
copy of a world (`game/world/save/LevelDBLevelData.kt:126`).

There is **no** masking, spoof-detection, filtering, or "applied seed"
transformation anywhere on that path. Lumina reports the wire value
verbatim.

## 3. Wire layout of the seed — CONFIRMED against our disassembly

`Protocol/bedrock-codec` is CloudburstMC's protocol, and its
`StartGameSerializer` history is the authoritative statement of where the
seed lives in the packet. `StartGamePacket.java:31` is
`public long seed;` — a **64-bit** value.

| Protocol | Seed encoding on the wire | Where introduced |
|---|---|---|
| v291 – v503 | `VarInts.writeInt` / `readInt` (**32-bit** zigzag) | `StartGameSerializer_v291:157` |
| **v503** | **`buf.readLongLE()` / `writeLongLE`** | `StartGameSerializer_v503` |
| v527 (re-stated) | `readLongLE` / `writeLongLE` | `StartGameSerializer_v527:34` |
| v534 – v898 | inherits the v527 64-bit read | verified: v534/544/567/582/671 override `writeLevelSettings` but call **`writeSeed`**, never re-inline the encoding; v685/v776/v818/v827/v898 only append fields after `super` |

So for 1.26.62's codec the seed is a **little-endian int64** — which is
what F-090/F-092 already established independently, now confirmed from a
second, unrelated codebase.

**And the seed is the FIRST field of `LevelSettings`, unshifted.** I
verified this for all 24 serializer versions by finding which byte gets
written first inside `writeLevelSettings`: every version either starts
with `writeSeed(...)`/`readSeed(...)` or delegates to `super` before
appending. No version ever inserts a field *before* the seed.

That closes a gap in our own record. ST-014 §1 wrote:

> The overlapping q0/q1 stores also fill dst+0x00..0x0C from
> src+0x0C..0x1C, which is `[u64 seed, u16 spawnBiomeType]`. **The seed is
> LevelSettings +0x0C**

The location is right; the bracket is off by four bytes. If the seed
occupied `src+0x0C..0x14` then `spawnBiomeType` would begin at `+0x14`,
and it is only written at `+0x14` because it belongs at `+0x14` — no
padding required.

### 3.1 The four instructions are a 28-byte inlined `memcpy`

The whole opening block is one primitive, not four independent copies.
`ldr q1,[x1],#0x20` is a **post-index** load: it reads at `x1` and only
*then* adds `0x20`. Ignoring that made it look like `src[0x0C..0x1C)` was
being scraped. It is not. Two 16-byte loads at offsets `0x00` and `0x0C`
are how clang inlines `memcpy(N)` for `16 < N <= 32` — one wide store at
`0`, one overlapping tail store at `N-16`:

```
N - 16 == 0x0C   ⇒   N = 0x1C = 28 bytes, copied [0x00, 0x1C) in full
```

Which is byte-exact against the wire order from §3, once you include the
`algorithmVersion` int that `v419`/`v534`/`v671` write before the seed:

```
+0x00  i32  algorithmVersion        ← INFERRED (explains why the copy starts at 0)
+0x04  ..   not individually touched by this ctor; covered by the wide stores
+0x0C  u64  seed                    ← covered by the copy; NO dedicated load
+0x14  u16  spawnBiomeType          ← PROVEN: ldrh/strh at 0x9b56eac/eb0
+0x18  std::string customBiomeName  ← PROVEN: ldr/stp/str at 0x9b56ec8..ed4
+0x30  u16  (0/1-valued)            ← PROVEN width: ldrh/strh at 0x9b56eb4/eb8
```

Two consequences worth carrying:

- **The seed has no dedicated load anywhere in the copy-ctor.** It only
  ever moves as part of those two vector stores. That is why a `+0x0C`
  band read has to take the value *before* the call — which is what our
  detour does, since the detour publishes and then calls the original.
- **`+0x30` is 16 bits, not a seed word.** `ldrh w8,[x22,#0x30]` /
  `strh w8,[x19,#0x30]`. If a future README or probe starts describing
  `+0x30` as a seed, this line is the refutation. (What the field *means*
  is not established here; the string at `+0x18` occupying 24 bytes means
  `+0x30` cannot be the `dimensionId` of a naive field-by-field walk, so
  the surrounding layout is evidently not a flat image of the wire order —
  worth remembering before anyone re-derives offsets by eye.)

> F-097 records both corrections (the bracket, and the `+0x30` width).
> The operational consequence of the bracket is nil — our band already
> lands on `+0x0C` — but the record must not contain arithmetic that a
> future reader would build on.

## 4. What this does and does not promise us

**What it materially raises confidence in:**

1. **The seed really is in `StartGame`, as an int64, at LevelSettings+0x0C,
   first field, unshifted, for 1.26.62's codec.** Two independent sources
   (Cloudburst's serializer lineage + our own disassembly) now agree. The
   `packet band:` hook is pointed at the right place; the F-096 fix makes
   it actually install.
2. **The client genuinely receives the seed on servers.** Lumina is an
   existence proof for the acceptance criterion "HUD shows the true server
   seed" — via a relay, but the client-side value is not withheld.
3. **No masking layer to chase.** §3 shows our copy-ctor copies the
   entire front unmodified, and Lumina reads the value verbatim. If a
   server sent a decoy seed, Lumina would show the decoy and we would show
   the *same* decoy — so "Lumina's number vs our number" is a valid
   cross-check on the device test.

**What it does NOT give us — and must not be over-read as:**

- It is **not** an in-process technique. Nothing here transfers into
  `libSeedTaker.so`. In particular the `H-003` question (a server that
  spoofs `0` and never sends the true generation seed) is **untouched**:
  Lumina has never faced it, because it reads whatever the server says.
- `Bedrock_v818.CODEC` is what Lumina *advertises* (`LuminaRelay.kt:29`),
  with `pickProtocolCodec` selecting from a 291..898 table in
  `Lunaris/src/main/cpp/ProtocolHelper.cpp`. It only reads a codec's
  layout; it has no bearing on 1.26.62 internals.

## 5. Packet-order reality check (a hazard for the §4.1 claim)

Lumina is not object-model-only. When `packet is StartGamePacket` fails to
decode for a given server it still forwards the raw bytes:

```
LuminaRelaySession.kt:49-58   processPacket:
    listeners.forEach { if (beforeFunc(it)) return }
    runCatching { UnknownPacket().apply { payload = wrapper.packetBuffer.retainedSlice()
                                          .skipBytes(wrapper.headerLength) }.let(sendFunc) }
        .onFailure { if (NoClassDefFoundError && "java/awt/Color") return }
```

It also rebuilds the codec wholesale on `RequestNetworkSettingsPacket`
(`AutoCodecPacketListener.kt:31`), forcing ZLIB at threshold 0, and
swaps in v729 inventory serializers plus a **v313** biome serializer for
every protocol above 729. In other words the proxy is willing to lie
about the wire, and the client has to cope. That is a different failure
mode from ours, but it is a reminder that "the packet decoded in a proxy"
is not by itself proof that the *game's* decoder produced the same
structure.

## 6. The rest of the architecture, for completeness

### 6.1 Networking stack
- `Network/transport-raknet` — vendored CloudburstMC RakNet (channels,
  offline state, sliding window, rate limiter — the limiter is *removed*
  in capture mode, `LuminaRelay.kt:83`).
- `Protocol/bedrock-connection` — Netty pipeline: batch
  encode/decode, `CompressionCodec`, `BedrockEncryptionEncoder/Decoder`,
  `BedrockPacketCodec_v1/v2/v3`, `FrameIdCodec`.
- `Protocol/bedrock-codec` — 1 174 files, one serializer class per packet
  version. This is where §3 comes from.
- `Lunaris/src/main/cpp/ProtocolHelper.cpp` — the protocol→class table
  (291 … 898) used by `pickProtocolCodec`.

### 6.2 Session model
`LuminaRelaySession` holds a `ServerSession` (toward stock MC) and a
`ClientSession` (toward the real server), a listener list, and an MPSC
queue for packets that arrive before the client session exists. Listener
contract is `before*` / `after*` returning "I consumed this".

### 6.3 Login / encryption (the wall in §1)
`EncryptedLoginPacketListener`:
- `LoginPacket` from the client is **re-signed** — Lumina forges its own
  certificate chain and skin JWT with a fresh keypair
  (`forgeAuthData` / `forgeSkinData`) and forwards that instead.
- On `ServerToClientHandshakePacket` it parses the server JWT, derives
  the shared secret with its own private key, calls
  `client.enableEncryption(key)`, and replies `ClientToServerHandshake`.
- `XboxLoginPacketListener` does live XBL auth when an account is
  configured; `EncryptedLoginPacketListener` covers the offline path.

This is why it needs `minecraft-msftauth` and `lunarisrpc` in the tree.

### 6.4 Overlay / UI
`Pixie/src/main/cpp` is a small Dobby+ImGui "Muffin" template. It hooks
**`libinput.so`**, not Minecraft:

```
Pixie/.../Muffin.cpp:22  xdl_open("libinput.so")
                          xdl_sym("...InputConsumer::initializeMotionEvent...")
                          MUFF(ptr, hook_Input, orig_Input)  → ImGui_ImplAndroid
```

Loaded from Kotlin only in `util/UpdateCheck.kt:18`
(`System.loadLibrary("pixie")`), inside a try/catch. `Pixie` declares
only `SYSTEM_ALERT_WINDOW`. `SSC/` is a licensing/SHA-1 gate
(`HashCat.getSignaturesSha1`), not seed-related. `TablerIcons/`,
`imgui/`, `animatedux/` are pure assets/UI.

**Therefore Lumina has no game-memory surface at all.** It cannot help
with offsets, and should not be mined for them.

### 6.5 Relay ergonomics
`captureLuminaRelay(...)` binds `0.0.0.0:19132` with a fabricated pong
(`Lunaris.cpp`, "§bWelcome To Lunaris§b", protocol 898) and connects
outward to the user-entered `host:port` (`CaptureModeModel`, default
`play.lbsg.net`). The user then adds "localhost" as a server in MC
(`HomeScreen.kt:187` + `ServerInit.addMinecraftServer`).

### 6.6 App-side seed consumers (inventory)
`constructors/GameDataManager.kt` (reflection store), `NetBound.kt`,
`game/module/api/commands/GameInfoCommands.kt` (`!seed`),
`overlay/kitsugui/WorldStats.kt`, `overlay/kitsugui/HomeCategoryUi.kt`,
`game/world/save/LevelDBLevelData.kt`. That is the complete list — there
is no second, hidden seed path.

## 7. Reading this into our next device run

Nothing here changes the immediate action (`2bcde89`: confirm the packet
band installs, then read out the two slots on a known local seed, then
Mega SMP). It changes how we *interpret* the results:

1. **If the band reads a value on Mega SMP** → that is the wire seed, and
   Lumina says it is the number the client is genuinely given. Report it;
   do not second-guess it for "masking". Masking was looked for and does
   not exist in a client that has been shipping this feature.
2. **If the band reads `+0x0C` == the local world's known seed** → slot
   selection is settled by construction, exactly as the `!seed` path
   implies, and `PacketSeedBandCount` can drop to 1 with `+0x0C`
   documented as confirmed rather than scanned.
3. **The `+0x14` slot is a near-free, independent read — take it.** By the
   §3.1 layout `src+0x14` is the `u16` `spawnBiomeType`, which Cloudburst
   models as a **2-value enum** (`SpawnBiomeType`: `DEFAULT`,
   `USER_DEFINED`). Our band already samples it as its second slot. So on
   a *vanilla* server the second band value should read `0` (or at worst
   `1`) — **a value of `1` rather than `0` is a positive signal that the
   source address is genuinely the LevelSettings front.** This is a
   cheap offset-check that costs nothing: we are already reading the
   bytes. Two caveats, both stated rather than glossed: a plugin server
   may make `USER_DEFINED` common, so `1` is not proof of a wrong
   address; and Minecraft's own settings object may not be a flat image
   of the wire order — the `std::string` at `+0x18` and the `u16` at
   `+0x30` already prove it is *not* naive — so this is corroboration,
   not a gate. Never reject a reading on this basis; only use it to
   decide whether to go looking for a bad source pointer.
4. **If the band installs but both slots read 0 on a server** → the
   remaining explanation is the *decoded object we hook is not the one
   the game uses* (ST-014 §4: the hand-off out of `sp+0x7d0` is still
   unmapped), or the server genuinely sent 0. Lumina distinguishes these
   by being at the endpoint; we would distinguish them by also snapshotting
   `sp+0x7d0` after the copy. That is the next experiment, not a rewrite.
5. **`H-003` stays open.** A server that never transmits a real seed is a
   problem a proxy cannot solve either; the honest answer for such a
   server is "unavailable", and our confidence pipeline (`recovered` only
   when validated) is already the right shape for it.
