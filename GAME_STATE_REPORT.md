# CatEngine Game-State, Turn Detection & State Machine Report

**Module:** CatEngine Native Game State Controller  
**Target Hardware:** Vivo T3 Lite 5G (`V2356`), Android 15 (API 35)  
**Package:** `com.miniclip.carrom` within CatEngine Container (`com.engine.cat`)  

---

## 1. Game State Machine Overview

The CatEngine state machine coordinates match transitions, turn ownership, physics calculation, and trajectory rendering to ensure prediction lines and automated actions only execute when strictly appropriate:

```
[STATE_IDLE / LOBBY]
         |
         v (libgame loaded / match started)
[STATE_MATCH_ACTIVE]
    +---> |
    |     +--> [STATE_LOCAL_TURN_READY] (setupLocalStrikerBar: Y < -15.0cm)
    |     |          |
    |     |          v (user or auto aim adjustment)
    |     |    [STATE_AIMING] ---> (Dynamic physics raycast active)
    |     |          |
    |     |          v (onShotFired / release)
    |     |    [STATE_SHOT_IN_PROGRESS] ---> (Prediction lines immediately hidden)
    |     |          |
    |     |          v (discs decelerate < 0.05 cm/s)
    |     |    [STATE_SETTLING] ---> (Multi-frame verification streak)
    |     |          |
    |     |          v (all discs stationary)
    |     |    [STATE_STATE_RECALCULATE] ---> (Update active puck list, check pockets)
    |     |          |
    |     +----------+
    |     |
    |     +--> [STATE_OPPONENT_TURN] (setupOpponentStrikerBar: Y > +15.0cm)
    |                |
    |                +--> (All prediction overlays & auto-shot strictly disabled)
    |
    v (startMatchEnding)
[STATE_MATCH_DESTROYED]
```

---

## 2. Turn Detection Invariants & Validation

| Turn Condition | Identification Metric | Action Taken | Validation Result |
| :--- | :--- | :--- | :--- |
| **Local Player Turn** | `setupLocalStrikerBar` hook (RVA `0x1A28F98`) & Striker $Y \in [-35.0, -15.0]$ | `s_isLocalTurnReady = true`, `s_isOpponentTurn = false`, Aim and prediction enabled | **VERIFIED** |
| **Opponent Turn** | `setupOpponentStrikerBar` hook (RVA `0x1A1DDF4`) or Striker $Y > 0$ | `s_isOpponentTurn = true`, `s_isLocalTurnReady = false`, Overlays hidden | **VERIFIED** |
| **Shot Execution** | `onShotFired` hook (RVA `0x1984568`) | `s_isShotExecuting = true`, Trajectories hidden from board during disc motion | **VERIFIED** |
| **Match Teardown** | `startMatchEnding` hook (RVA `0x19DF2A8`) | `g_matchActive = false`, affine transform reset, zero memory polling | **VERIFIED** |

---

## 3. Discrepancy Fixes

1. **Top Baseline False Positive:** Previously, `isValidBaselineY` accepted $|Y| > 10.0\text{ cm}$, treating the opponent's striker position as a local turn. Fixed by enforcing bottom baseline $Y \in [-35.0, -15.0]$ exclusively for local player turns.
2. **Post-Shot Overlay Lingering:** Previously, `keepLineAfterShot` defaulted to `true`. Fixed by setting the default to `false` and automatically wiping trajectories upon shot release.
