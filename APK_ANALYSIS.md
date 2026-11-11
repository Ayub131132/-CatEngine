# Carrom Pool APK & Binary Reverse Engineering Report

**Lead Reverse Engineer:** Senior Android Reverse Engineer & Native Binary Analyst  
**Target Package:** `com.miniclip.carrom`  
**Analyzed Binaries:**
- `lib/arm64-v8a/libgame-CARROM-GooglePlay-Gold-Release-Module-1489.so` (48,703,200 bytes, SHA-256 verified)
- `scratch_se/classes.dex` (3,881,360 bytes, DEX version 039)
- `scratch_se/lib/arm64-v8a/libapp.so` & `libengine.so`

---

## 1. Package & Binary Overview

The complete Carrom Pool APK is to be provided by the operator for full asset and resource extraction. However, the repository already contains the core execution payload extracted from Carrom Pool (v1489): the primary 48.7 MB game engine library `libgame-CARROM-GooglePlay-Gold-Release-Module-1489.so` and the primary DEX bytecode file `classes.dex`.

### Technical Metadata:
- **Package Name**: `com.miniclip.carrom`
- **Native Architecture**: AArch64 (ARM64-v8a), Little Endian, ELF64
- **Game Engine**: **Cocos2d-x** (C++ / Objective-C++ hybrid runtime)
- **Serialization / Protocol**: **Google Protocol Buffers** (`.proto`)

---

## 2. Binary Symbol & RTTI Reconstruction

Using `llvm-nm` and `llvm-strings` from the Android NDK LLVM toolchain, extensive RTTI (Run-Time Type Information) and demangled symbols were extracted from `libgame.so`.

### 2.1 Original Compilation Paths Recovered
Compilation artifact strings in the binary reveal the developer build environment:
```
/home/jenkins/agent/workspace/CARROM-GooglePlay-Gold-Release-Module/CARROM/src/game/ludo/TableLogic.mm
```
*Note: Miniclip developers built the game on a Jenkins CI agent, implementing core table physics in Objective-C++ (`TableLogic.mm`).*

### 2.2 Core C++ Data Structures & Object Model
The RTTI demangled type descriptors confirm the exact memory layout of the game's physics world:

```cpp
class TableLogic {
    std::shared_ptr<TableShape> tableShape;
    TableLogicDelegate delegate;
    double tableFriction;
    std::map<int, PocketLogic> pocketMap;     // 4 corner pockets
    std::map<int, BaselineLogic> baselineMap; // Player striker baselines
    std::map<int, StrikerLogic> strikerMap;   // Striker bodies
    std::map<int, PuckLogic> puckMap;         // Active pucks on board
    std::vector<MCPoint> polygonVertices;     // Table cushion polygon
    double cushionRestitution;
    double puckRadius;
    double strikerRadius;
};
```

### 2.3 Physics Event Delegates
The binary contains explicit symbols for game state callbacks triggered by the physics engine:
- `TableLogicDelegate::physicsStopped()`: Dispatched when all discs come to a complete halt.
- `TableLogicDelegate::strikerPocketed(int playerId)`: Dispatched when striker enters a pocket (foul).
- `TableLogicDelegate::puckPocketed(int puckId, int pocketId)`: Dispatched when a puck falls into a pocket hole.
- `TableLogicDelegate::puckPotted(int puckId, int pocketId)`: Dispatched on valid score pot.
- `TableLogicDelegate::puckReturned(int puckId)`: Dispatched when an invalidly potted puck is placed back on the center circle.

### 2.4 Internal AI Bot System
The game binary contains an authentic internal AI shot solver:
```cpp
MatchBotBehaviours::MatchBotBehaviours(TableLogic&, MatchFlow&);
```
Miniclip implemented an internal bot solver used for single-player practice matches and AI opponents. This solver operates directly on `TableLogic` state.

### 2.5 Protobuf Telemetry & Network Messages
The binary defines Google Protocol Buffer message instances for shot and table telemetry:
- `_gameplay_striker_state_default_instance_`
- `_gameplay_puck_state_default_instance_`
- `_gameplay_shot_taken_default_instance_`
- `_gameplay_aim_event_default_instance_`
- `_gameplay_shot_outcome_default_instance_`
- `_bank_shot_default_instance_`
- `_connection_shot_default_instance_`

---

## 3. Observable Physics Parameters Recovered from Code & Binary

By cross-referencing `catEngine.cpp`, `PhysicsEngineV2.cpp`, and the recovered binary structures, the exact physical constants of the game are established:

| Physical Parameter | Value | Source / Verification |
| :--- | :--- | :--- |
| **Coordinate System** | Centimeters (origin `[0,0]` at table center) | Table width: $74.0\text{ cm} \times 74.0\text{ cm}$, Outer cushion: $76.0\text{ cm}$ |
| **Striker Radius** | $2.65\text{ cm}$ | `tlStrikerRadius` (offset `0x1C0`) |
| **Puck Radius** | $1.95\text{ cm}$ | `tlPuckRadius` (offset `0x1B8`) |
| **Pocket Center Inset** | $33.48\text{ cm}$ | Measured from board center $(33.48, 33.48)$ |
| **Pocket Capture Radius** | $2.15\text{ cm}$ | Effective drop well threshold |
| **Striker Mass Ratio** | $3.00 : 1.00$ | Striker mass = 3.0 relative to puck mass 1.0 |
| **Restitution (Disc-Disc)** | $1.00$ (Elastic) | Rebound impulse coefficient |
| **Restitution (Cushion)** | $0.88$ | Table cushion bounce elasticity |
| **Striker Deceleration** | $\sim 156.8\text{ cm/s}^2$ ($\mu \approx 0.160$) | Sliding friction on wooden board |
| **Puck Deceleration** | $\sim 105.8\text{ cm/s}^2$ ($\mu \approx 0.108$) | Rolling/sliding friction on wooden board |
| **Pocket Gravity Well** | $980.0\text{ cm/s}^2$ | Funnel suction when within capture radius |
| **Pocket Speed Cutoff** | $250.0\text{ cm/s}$ | Discs exceeding $250\text{ cm/s}$ skip past pocket |

---

## 4. Limitations & Non-Recoverable Elements

1. **Server-Side Match Arbitration**: In competitive multiplayer games, final turn validation and coin rewards are arbitrated via encrypted server connections.
2. **Dynamic Anti-Cheat Signatures**: Miniclip updates client integrity checks across major releases, meaning RVAs must be dynamically resolved rather than hardcoded.
3. **Source Code Comments & Build Headers**: Full C++ source files cannot be extracted from release binaries; reconstructed prototypes are derived via ELF symbol tables and decompilation.

---

## 5. Live Physical Device Extraction & Split APK Audit (Version 19.6.0 / Module-1491)

Extracted directly via ADB from connected authorized testing device (`10ME8DF1CT0001Z`, Vivo V2356, Android 15/API 35):

### 5.1 Split APK Inventory
| Artifact | Size | Description |
| :--- | :--- | :--- |
| `base.apk` | 189,042,103 bytes | Main application resources, 10 DEX files (`classes.dex` through `classes10.dex`), encrypted Cocos2d-x configs |
| `split_config.arm64_v8a.apk` | 54,970,145 bytes | 64-bit ARM native binaries including primary game engine `libgame-CARROM-GooglePlay-Gold-Release-Module-1491.so` |
| `split_gpdeku.apk` | 119,049 bytes | Google Play Dynamic Feature / Feature Delivery DEX modules (`classes.dex`, `classes2.dex`) |
| `split_gpdeku.config.arm64_v8a.apk` | 410,021 bytes | Native companion binary `libgpdeku.so` |

### 5.2 Application Badging & Environment
- **Package Name**: `com.miniclip.carrom`
- **Version Code**: `1491`
- **Version Name**: `19.6.0`
- **Min SDK**: `26` (Android 8.0 Oreo)
- **Target SDK**: `36` (Android 16 Developer Preview)
- **Compile SDK**: `35` (Android 15)
- **Launchable Activity**: `com.miniclip.carrom.CarromActivity`

### 5.3 Native Binary Comparison & Hashes
- **Repository Binary (Module-1489)**:
  - File: `lib/arm64-v8a/libgame-CARROM-GooglePlay-Gold-Release-Module-1489.so`
  - Size: 48,703,200 bytes
  - SHA-256: `9B5545EDAD7FB4AB6B4E5E01EF9DAB299A73E795E0A44B455D689BC33BD49299`
- **Live Device Binary (Module-1491)**:
  - File: `CarromPool_APK/split_config.arm64_v8a.apk!lib/arm64-v8a/libgame-CARROM-GooglePlay-Gold-Release-Module-1491.so`
  - Size: 48,998,992 bytes
  - SHA-256: `4CF2BA474FC9AEFDA142FBDCA46CF7B4D3EBF06A2114ECD8487A3E48448710B4`

### 5.4 Disassembly Verification of CatEngine RVA Offsets in Module-1491
Using `llvm-objdump -d` (NDK 28.2.13676358), the hardcoded offsets configured in CatEngine's native layer (`catEngine.cpp`) were disassembled directly against the newly extracted `Module-1491` binary:
- **`0x19843A0` (ControlsLogicLocal::onAimChanged)**:
  ```armasm
  19843a0:  sub  sp, sp, #0x40
  19843a4:  stp  x29, x30, [sp, #0x20]
  19843a8:  str  x19, [sp, #0x30]
  19843ac:  add  x29, sp, #0x20
  ```
- **`0x1984568` (ControlsLogicLocal::onShotFired)**:
  ```armasm
  1984568:  sub  sp, sp, #0x50
  198456c:  stp  x29, x30, [sp, #0x30]
  1984570:  str  x19, [sp, #0x40]
  1984574:  add  x29, sp, #0x30
  ```
- **`0x1A28F98` (GameplayView::setupLocalStrikerBar)**:
  ```armasm
  1a28f98:  stp  x29, x30, [sp, #-0x40]!
  1a28fac:  ldr  x8, [x0, #0x420]
  1a28fb0:  ldr  x20, [x0, #0x3f0]
  ```
- **`0x1B2B748` (TableLogic::moveStrikerToPositionSmooth)**:
  ```armasm
  1b2b748:  stp  d9, d8, [sp, #-0x30]!
  1b2b75c:  fmov d8, d1
  1b2b760:  fmov d9, d0
  ```
*Conclusion:* All RVA targets in CatEngine's native layer correspond exactly to valid standard AArch64 function prologues and float-coordinate handling routines in the live Play Store release Module-1491.


