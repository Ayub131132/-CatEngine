# CatEngine: Implementation Roadmap & Execution Plan

**Project:** CatEngine  
**Lead Engineer:** Senior Android, Rust/C++, and Computer Vision Engineer  
**Scope:** Architecture Stabilization, Physics Optimization, Computer Vision Integration, and Authorized Test Automation  
**Constraint Rule:** Strictly test automation only in owned or authorized environments. Do not claim 100% prediction accuracy.

---

## Phase Breakdown & Milestones

```
+--------------------------------------------------------------------------------+
| Phase 1: Technical Audit & Repository Architecture Assessment [COMPLETED]      |
+--------------------------------------------------------------------------------+
                                       |
                                       v
+--------------------------------------------------------------------------------+
| Phase 2: Build Normalization, Toolchain Harmonization & Regression Harness     |
+--------------------------------------------------------------------------------+
                                       |
                                       v
+--------------------------------------------------------------------------------+
| Phase 3: Screen Capture Pipeline & Computer Vision Perception Engine          |
+--------------------------------------------------------------------------------+
                                       |
                                       v
+--------------------------------------------------------------------------------+
| Phase 4: Trajectory Prediction Accuracy & Physics Engine Calibration          |
+--------------------------------------------------------------------------------+
                                       |
                                       v
+--------------------------------------------------------------------------------+
| Phase 5: Humanized Input Injection & Authorized Autoplay Testing Harness       |
+--------------------------------------------------------------------------------+
                                       |
                                       v
+--------------------------------------------------------------------------------+
| Phase 6: Anti-Detection Hardening, Android 15 16KB Page Alignment & Release    |
+--------------------------------------------------------------------------------+
```

---

## Detailed Milestones & Execution Steps

### Phase 1: Technical Audit & Architecture Assessment [COMPLETED]
- **Milestone 1.1**: Full code inspection of Java, Kotlin, C++, AIDL, and build scripts.
- **Milestone 1.2**: Deconstruction of BlackBox virtualization, Dobby inline hooks, and `eglSwapBuffers` render pipeline.
- **Milestone 1.3**: Identification of missing components: `settings.gradle`, JitPack repo, CV libraries, screen-capture APIs.
- **Deliverables**: `AUDIT_REPORT.md`, `ARCHITECTURE.md`, `DEPENDENCY_REVIEW.md`, `IMPLEMENTATION_ROADMAP.md`.

---

### Phase 2: Build Normalization & Regression Test Harness
- **Milestone 2.1**: **Repository Build Fixes**:
  - Permanently commit root `settings.gradle` declaring Google, MavenCentral, Gradle Plugin Portal, and JitPack.
  - Align `ndkVersion` in `build.gradle` to match installed stable NDK (`28.2.13676358`).
  - Standardize Gradle build properties and verify `./gradlew assembleDebug` succeeds cleanly on clean environments.
- **Milestone 2.2**: **C++ Physics Standalone Unit Tests**:
  - Decouple `PhysicsEngineV2.cpp` from Android logging and JNI dependencies to allow compiling and running standalone unit tests on host PC / CI.
  - Author deterministic unit test suite verifying:
    - Striker launch velocity scaling across varying pull percentages ($0.1$ to $1.0$).
    - Disc-disc elastic collision momentum conservation ($m_1 v_1 + m_2 v_2 = m_1 v_1' + m_2 v_2'$).
    - Cushion bounce angle of incidence equals angle of reflection under zero spin.
    - Pocket drop hole gravity suction cutoff threshold ($v \le 250.0\text{ cm/s}$).
- **Testing Criteria**: 100% pass rate on physics conservation laws in synthetic test suites.

---

### Phase 3: Screen Capture Pipeline & Computer Vision Perception Engine
- **Milestone 3.1**: **Authorized Screen Capture Module**:
  - Implement non-invasive native frame extraction via `glReadPixels()` inside `GraphicsEngine.cpp`'s `eglSwapBuffers` hook for in-process testing.
  - Implement external Android `MediaProjection` + `ImageReader` service for external / black-box evaluation without game code modification.
- **Milestone 3.2**: **OpenCV Integration**:
  - Integrate OpenCV 4.10+ Android SDK into `:Bcore`.
  - Implement Table Homography & Perspective Rectification: detect the 4 outer corner pockets, rectify the camera angle to a canonical $76\text{ cm} \times 76\text{ cm}$ top-down view.
  - Implement Disc Detection: Color masking (HSV/LAB) and circle detection (HoughCircles) to identify puck positions (White, Black, Red Queen) and striker position.
- **Testing Criteria**: Disc center detection error within $\le 2.0\text{ mm}$ ($< 0.2\text{ cm}$) of memory ground-truth on canonical 1080p boards.

---

### Phase 4: Trajectory Prediction Accuracy & Physics Engine Calibration
- **Milestone 4.1**: **Prediction vs. Outcome Telemetry Verification**:
  - Enhance `logShotTelemetry()` in `catEngine.cpp` to systematically log:
    - Predicted first contact point $(x_{\text{pred}}, y_{\text{pred}})$ vs. Actual first contact point $(x_{\text{act}}, y_{\text{act}})$.
    - Predicted target puck final resting coordinates vs. Actual final coordinates.
    - True pocket potted status vs. predicted potted status.
- **Milestone 4.2**: **Dynamic Offset & Signature Scanner**:
  - Replace fragile hardcoded function RVAs with an AArch64 pattern/signature scanner (`pattern_scan.cpp`) to locate target functions in `libgame.so` across game updates without manual disassembly.
- **Milestone 4.3**: **Multi-Bank Shot Optimization**:
  - Extend `GraphicsEngine_ScanAndExecuteBestShot()` with recursive depth-2 and depth-3 path search (striker $\to$ cushion $\to$ puck $\to$ pocket, and puck $\to$ cushion $\to$ pocket).
- **Testing Criteria**: Prediction accuracy benchmarked across 200 recorded test shots, documenting realistic statistical bounds (e.g. $>92\%$ direct pot trajectory alignment).

---

### Phase 5: Humanized Input Injection & Authorized Autoplay Testing Harness
- **Milestone 5.1**: **Human-Like Touch Emulation**:
  - Implement synthetic touch path generator:
    - Replaces instantaneous `orig_shotFired` call with interpolated Bezier curve touch paths dispatched via Android `MotionEvent` or `/dev/uinput`.
    - Introduces Gaussian timing jitter ($\sigma = 15\text{ ms}$) on pull back and release.
    - Simulates human micro-tremors and natural angle aiming adjustments.
- **Milestone 5.2**: **Safety Net & Fair-Play Safeguards**:
  - Implement kill-switch and strict authorized-environment checks.
  - Enforce shot-clock compliance (avoids shooting in $< 100\text{ ms}$ or exceeding turn countdown).
- **Testing Criteria**: Autoplay execution must produce natural `MotionEvent` gesture logs indistinguishable from physical finger interactions in touch analyzer tools.

---

### Phase 6: Anti-Detection Hardening & Android 15 Compatibility
- **Milestone 6.1**: **16 KB Memory Page Size Compliance**:
  - Recompile and verify all native static archives (`libdobby.a`) with Android 15 16KB page-alignment flags (`-z,max-page-size=16384`).
  - Validate native library memory mappings via `readelf -l`.
- **Milestone 6.2**: **Virtual Container Cloaking**:
  - Audit and enhance `AntiDetection.cpp` against modern anti-cheat checks:
    - Cloak `/proc/self/maps` against `libengine.so` and `Dobby` trampoline page markers (`rwxp`).
    - Hook `readlink("/proc/self/exe")` and `getpid()` to prevent sandbox leakage.
- **Testing Criteria**: Zero crashes or hook detection alerts when running under strict Android 15 developer preview environments.

