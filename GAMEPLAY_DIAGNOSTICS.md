# CatEngine Real-Time Gameplay Diagnostics & Observation Log

**Session Date:** November 11, 2026  
**Target Device:** Vivo T3 Lite 5G (`V2356`, Serial `10ME8DF1CT0001Z`)  
**OS Version:** Android 15 (API 35, 16KB Page-Aligned Kernel)  
**Display Physical Metrics:** 720 x 1612 px @ ~269 PPI  
**Diagnostics Tooling:** `capture_gameplay.ps1` (Timestamped screen capture, Logcat event capture, PSS/Heap tracking)  
**Storage Path:** `diagnostics/gameplay_session/`  

---

## 1. Pre-Flight Diagnostic Health Check

| Component | Status | Verification Details |
| :--- | :--- | :--- |
| **ADB Connectivity** | **ACTIVE** | `10ME8DF1CT0001Z` authorized and responsive. |
| **CatEngine Installation** | **ACTIVE** | Version 1.0.0 (versionCode 1, targetSdk 28). |
| **Screen Resolution** | **720 x 1612** | Physical size confirmed via `wm size`. |
| **Screen Capture Engine** | **FUNCTIONING** | Screencap pulling sub-second frames to local host. |
| **In-Process Hooks** | **LOADED** | Base `0x724862A000`, 8 hooks verified active in PID 30546. |
| **GLES Shaders** | **INITIALIZED** | `LineProg=9, CircleProg=12` |

---

## 2. Real-Time Telemetry & Metric Sampling

The automated diagnostic harness records the following operational parameters during gameplay sessions:

1. **Active Foreground Focus:** Monitored via `dumpsys window` to detect when CarromActivity (`ProxyActivity$P0`) enters or leaves the foreground.
2. **Native & Dalvik Memory Usage:** Monitored via `dumpsys meminfo com.engine.cat` to detect leaks in frame capture and GLES buffer reallocations.
3. **CatEngine Native Heartbeat:** Monitored via Logcat (`CatEngine:I`) tracking `MatchActive` and in-flight game events.
4. **Frame Capture Evidence:** Timestamped PNG screenshots saved to `diagnostics/gameplay_session/frames/`.

---

## 3. Discrepancy Observation Log

| Timestamp | Observation Area | Expected Behavior | Observed Behavior | Root Cause |
| :--- | :--- | :--- | :--- | :--- |
| **Initial Review** | Display Geometry | Trajectory aligned with 720x1612 screen. | Trajectories scaled for 1080x1920 layout. | Hardcoded coordinate constants in `BoardDetectionEngine.h`. |
| **Aiming Phase** | Visual Overlay | Clean, thin, unobtrusive guide paths. | Heavy solid lines obscuring coins and pocket mouths. | Lack of line stippling, anti-aliasing, and alpha blending. |
| **Motion Phase** | State Transition | Clear prediction lines when shot begins. | Prediction lines remain static on screen while coins move. | Missing state machine transitions for `SHOT_IN_PROGRESS`. |
| **Settings UI** | Responsiveness | Fluid 60 FPS slider adjustments. | Stutter and momentary UI thread stalls. | Synchronous IPC/JNI roundtrips on Compose main thread. |

---

## 4. Live Session Status

* The diagnostic monitor is configured and ready to record gameplay frames and logcat telemetry continuously while the user plays Carrom Pool.

