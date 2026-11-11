# CatEngine Critical Bug Investigation & System Repair Analysis

**Date:** November 11, 2026  
**Target Hardware:** Vivo T3 Lite 5G (`V2356` / `V2346iM1`, Serial `10ME8DF1CT0001Z`)  
**Operating System:** Android 15 (API 35, 16KB Kernel)  
**Display Physical Resolution:** 720 x 1612 px (Aspect ratio: 20.15:9)  
**Target Package:** `com.miniclip.carrom` (v19.6.0, build 1491) within CatEngine container (`com.engine.cat`)  

---

## 1. Initial Root-Cause Findings & Architectural Discrepancies

### Bug 1: Hardcoded 1080x1920 Screen Dimensions vs. Physical 720x1612
* **Root Cause:**
  `BoardDetectionEngine.h` and the native coordinate mapping layer were configured with hardcoded constants:
  ```cpp
  const float SCREEN_WIDTH = 1080.0f;
  const float SCREEN_HEIGHT = 1920.0f;
  const float BOARD_SIZE_PIXELS = 1014.0f;
  const float BOARD_CENTER_X = 540.0f;
  const float BOARD_CENTER_Y = 870.0f;
  ```
  On the Vivo T3 Lite 5G, the physical resolution reported by `wm size` is **`720 x 1612`**.
* **Impact:**
  - Trajectory lines are rendered at incorrect pixel offsets, appearing misaligned and severely obstructing the game pieces.
  - Pocket coordinates and cushion reflection lines do not match the real display boundaries.
  - Any pixel-based color sampling or coin bounding-box checks are offset by ~33% horizontally and ~16% vertically.
* **Resolution Plan:**
  Refactor `BoardDetectionEngine` and `GraphicsEngine` to dynamically query and ingest actual display dimensions (`display_width`, `display_height`, `density_dpi`) from Android `DisplayMetrics` / `ANativeWindow` during GLES surface initialization.

---

### Bug 2: Trajectory Line Visual Obstruction
* **Root Cause:**
  Overlay rendering used heavy, solid OpenGL lines without depth distinction, translucency, or anti-aliasing.
* **Impact:**
  Thick trajectory paths completely cover the target puck, striker, and pockets, making it difficult for the user to see the actual game board.
* **Resolution Plan:**
  - Introduce thin, dashed/stippled lines for candidate trajectories and translucent paths with a subtle outline.
  - Render trajectory lines beneath puck visualization markers.
  - Reduce collision marker radii and render unobtrusive directional arrows rather than solid overlapping discs.
  - Provide HUD toggles to enable/disable specific overlay layers (Striker Path, Puck Path, Cushion Bounces, Target Indicator).

---

### Bug 3: Stale State & Missing Game-State Turn Transitions
* **Root Cause:**
  Prediction overlays remained rendered on screen continuously, even while pieces were actively moving, during opponent turns, or when the match was paused/ended.
* **Impact:**
  Users saw ghost trajectory paths that did not reflect the current board state.
* **Resolution Plan:**
  Implement a formal **Game State Machine**:
  - `STATE_IDLE`
  - `STATE_WAITING_FOR_BOARD`
  - `STATE_READY` (Local turn, striker placed)
  - `STATE_AIMING` (User actively adjusting aim/force)
  - `STATE_SHOT_IN_PROGRESS` (Discs in motion; clear all prediction overlays immediately)
  - `STATE_SETTLING` (Discs decelerating; wait for velocity cutoff across multiple consecutive frames)
  - `STATE_SHOT_COMPLETED` (Recalculate board state, check pocketing events)
  - `STATE_OPPONENT_TURN` (Disable active aim overlays)

---

### Bug 4: Physics Trajectory Discrepancies
* **Root Cause:**
  Fixed coefficient of restitution and simplified cushion reflection did not account for the velocity-dependent damping and pocket rim capture geometry present in Carrom Pool v19.6.0.
* **Resolution Plan:**
  - Calibrate dynamic cushion restitution based on incident velocity and angle.
  - Implement realistic pocket rim drop-off physics with radial pull towards pocket centers.
  - Match disc radii precisely to detected board dimensions.

---

### Bug 6: Turn Confusion & Opponent Turn Misidentification
* **Root Cause:**
  `isValidBaselineY` accepted any Y where `abs(y) > 10.0`. During the opponent's turn, when the opponent placed their striker on the top baseline (`+25.35`), `isValidBaselineY(+25.35)` evaluated to `true`, causing `s_isLocalTurnReady` to be prematurely asserted as `true`.
* **Resolution:**
  - Added atomic flag `s_isOpponentTurn`. When `setupOpponentStrikerBar` fires, `s_isOpponentTurn` is asserted, and auto-shot immediately pauses.
  - Restricted local striker baseline validation strictly to $Y \in [-35.0, -15.0]$ (bottom baseline). Top baseline is never treated as local turn.
  - Cleared aim vectors and prediction paths upon opponent turn initialization.

---

### Bug 7: Target Puck Color Inflexibility (White vs. Black vs. Any)
* **Root Cause:**
  `GraphicsEngine.cpp` lines 2709-2712 and 2897 contained hardcoded assumptions that the local player was always White (`puckType == 1`) and explicitly skipped all Black pucks (`if (traj.puckType == 2) continue;`).
* **Resolution:**
  - Implemented `s_targetColorMode` (0=Auto, 1=White, 2=Black, 3=Any).
  - In Auto mode, both White and Black are evaluated based on active board configuration and player assignment.
  - Exposed `setTargetColorMode` via JNI, `NativeCore`, and `CatEngineConfig` with interactive Compose filter buttons.

---

### Bug 8: Striker Scratch Foul Misclassified as Pot Point
* **Root Cause:**
  In `GraphicsEngine.cpp`, line 1773 evaluated `if (sim.strikerInPocket) pocketPotted[sim.strikerPocketIndex] = true;`, which turned `pocketPotted` green, celebrating a foul as a valid pot.
* **Resolution:**
  - Separated `pocketPotted` (green) from `pocketScratched` (warning crimson red).
  - When striker enters a pocket, rendered a prominent Red Foul Warning on that pocket rim and colored the striker trajectory line red.
  - Disqualified scratch shots in `evaluateCandidateShot` with immediate rejection.

---

### Bug 9: Settings Screen Recomposition & Slider Lag
* **Root Cause:**
  Sliders directly invoked synchronous `SharedPreferences` writes and JNI roundtrips on every fractional pixel movement, stalling the Compose render pipeline.
* **Resolution:**
  - Implemented local slider position state (`remember(value)`) that updates at 60/120 FPS smoothly.
  - Deferred disk persistence and JNI updates to `onValueChangeFinished`.
  - Reorganized the settings dialog into clear, logical sections.

