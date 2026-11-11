# CatEngine: Technical Gaps, Deficiencies & Blocker Analysis

**Lead Engineer:** Senior Android Reverse Engineer & System Architect  
**Project:** CatEngine  
**Assessment Date:** November 2026

---

## 1. Build System & Toolchain Deficiencies

1. **Omission of `settings.gradle` in Version Control**:
   - **Gap**: The repository was pushed to Git without `settings.gradle` or `settings.gradle.kts`.
   - **Impact**: Any clean clone fails immediately when running `./gradlew`, unable to find Google's Maven repository or subprojects (`:app`, `:Bcore`, `:black-reflection`, `:compiler`).
   - **Remedy**: A normalized `settings.gradle` must be tracked in version control declaring `google()`, `mavenCentral()`, `gradlePluginPortal()`, and `https://jitpack.io`.
2. **Missing JitPack Maven Repository**:
   - **Gap**: `Bcore/build.gradle` line 136 references `com.github.tiann:FreeReflection:3.2.2`, but JitPack is not configured in `build.gradle`.
   - **Impact**: Unresolvable dependency error during compilation.
3. **NDK Version Specification Mismatch**:
   - **Gap**: `build.gradle` declares `ndkVersion = "30.0.16248370"`, which is not bundled with standard Android SDK setups (where NDKs 26, 27, and 28 are prevalent).
   - **Impact**: AGP triggers a slow ~1 GB background download of NDK 30 or fails in offline environments.
4. **Corrupted JADX Archive in Workspace**:
   - **Gap**: `jadx-1.5.6.zip` in the repository root is an HTML error page (572 KB) saved with a `.zip` extension.
   - **Impact**: Cannot be used for automated DEX decompilation.

---

## 2. Reverse Engineering & Binary Fragility

1. **Fragility of Hardcoded Function RVAs**:
   - **Gap**: `catEngine.cpp` relies on hardcoded function offsets targeting a specific version of `libgame.so` (v1489/v1491):
     - `setupLocalStrikerBar = 0x1A28F98`
     - `setupOpponentStrikerBar = 0x1A1DDF4`
     - `forcePuckPosition = 0x1B3352C`
     - `forceStrikerPosition = 0x1B33570`
     - `onAimChanged = 0x19843A0`
     - `onShotFired = 0x1984568`
   - **Impact**: If the game is updated to a newer version (e.g. v1492+), these offsets point to invalid memory or differing instructions, causing immediate crashes (`SIGSEGV`) or silent failure.
   - **Remedy**: Implement an automated AArch64 bytecode pattern/signature scanner (`pattern_scan.cpp`) that locates function prologues dynamically at runtime based on unique instruction sequences.

---

## 3. Perception & Computer Vision Gaps

1. **Total Absence of Optical Perception**:
   - **Gap**: CatEngine currently contains **zero** image-processing code, **zero** OpenCV integration, and **zero** screen-capture APIs.
   - **Impact**: If game memory structs change or if CatEngine is operated outside BlackBox (as an external assistant or testing tool), the system has no alternative perception capability.
   - **Remedy**: Implement a dual-path perception architecture:
     - Path A (High-Performance In-Process): Native `glReadPixels()` inside `eglSwapBuffers`.
     - Path B (External Non-Invasive): Android `MediaProjection` API with OpenCV Homography board rectification.

---

## 4. Physics & Prediction Deficiencies

1. **Lack of Deep Recursive Path Optimization**:
   - **Gap**: While `PhysicsEngineV2.cpp` accurately simulates a single forward trajectory, `GraphicsEngine_ScanAndExecuteBestShot()` evaluates candidate shots using basic radial iterations, lacking tree-search / branch-and-bound optimization for multi-cushion bank shots or carom combination shots.
   - **Impact**: Fails to discover complex winning bank shots when direct line-of-sight is blocked by opponent discs.
2. **Disregard of Spin-Induced Friction in Certain Angles**:
   - **Gap**: While normal impulse restitution is well-calibrated, high-power grazing cushion bounces experience complex roll-slip transitions that can slightly deviate the post-cushion line over long distances.

---

## 5. Autoplay & Input Injection Deficiencies

1. **Detectable Direct Native Invocation**:
   - **Gap**: The autonomous worker invokes the game's internal `orig_shotFired` C++ function pointer instantaneously:
     ```cpp
     orig_shotFired(shotDelegate, pIndex, true, 0, pullAngle, forceValClamped);
     ```
   - **Impact**: Leaves zero touch event footprint in the Android `InputDispatcher` and executes in 0 milliseconds with zero finger drag telemetry. Server-side anomaly detection can easily flag this as non-human automation.
   - **Remedy**: Construct an interpolated touch event pipeline (`MotionEvent` or `uinput`) that simulates human finger dragging, micro-jitter, and pull-back delays ($\sim 300 - 600\text{ ms}$).
2. **Absence of Safety Net / Kill Switch**:
   - **Gap**: No automated abort mechanism exists if the turn timer expires or if unexpected disc motion occurs during the shot.
   - **Remedy**: Enforce strict turn countdown thresholds and an instant emergency stop trigger.

---

## 6. Verification & Test Harness Gaps

1. **No Automated Regression Testing**:
   - **Gap**: The project lacks unit tests verifying that changes to `PhysicsEngineV2.cpp` do not degrade simulation accuracy.
   - **Remedy**: Build a standalone C++ CLI test runner compiling `PhysicsEngineV2.cpp` against synthetic board scenarios and empirical test datasets.

