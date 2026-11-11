# CatEngine Project Technical Audit

**Lead Engineer:** Senior Android Reverse Engineer, Game Physics Engineer & Software Architect  
**Project:** CatEngine  
**Repository:** [https://github.com/Ayub131132/-CatEngine](https://github.com/Ayub131132/-CatEngine)  
**Target Application:** Carrom Pool (Miniclip)  
**Date:** November 2026

---

## 1. Executive Overview

CatEngine is an Android virtualization and in-process game introspection project designed around the **BlackBox** user-space sandbox. The repository contains a complete Android application (`:app`), a virtualization core library (`:Bcore`), a compile-time reflection annotation system (`:black-reflection`), and an annotation processor (`:compiler`).

The primary purpose of CatEngine in its current state is to provide aim prediction and autonomous shot execution for the Android game *Carrom Pool* (`com.miniclip.carrom`) without requiring Android root privileges or external hardware.

### High-Level Summary of Findings
1. **In-Process Virtualization Paradigm**: BlackBox enables CatEngine to run the commercial game inside its own process namespace (`:p0`). As a result, CatEngine shares virtual memory directly with `libgame.so`.
2. **Absence of Computer Vision & Screen Capture**: The repository contains **0% computer vision** and **0% screen capture**. Prediction lines are drawn via an OpenGL ES `eglSwapBuffers` hook, and shot parameters are acquired by reading C++ objects in memory.
3. **Build Configuration & Status (Verified)**:
   - Root `settings.gradle` created with Google Maven, JitPack (`https://jitpack.io`), and Gradle Plugin Portal.
   - NDK version aligned with installed NDK `28.2.13676358` and 16KB page size alignment verified (`-z,max-page-size=16384`).
   - `:black-reflection` and `:compiler` build succeeded.
   - `:Bcore` assembleDebug build succeeded (3m 1s), building `libengine.so` for arm64-v8a and armeabi-v7a.
   - `:app` assembleDebug build succeeded (3m 2s), producing `CatEngine_1.0.0_debug.apk` (22,484,774 bytes).
4. **Authentic Binary Assets & Device Extraction**:
   - `base.apk`, `split_config.arm64_v8a.apk`, `split_gpdeku.apk`, and `split_gpdeku.config.arm64_v8a.apk` pulled directly from physical device (`10ME8DF1CT0001Z`, Vivo V2356, Android 15/API 35).
   - Package version: `com.miniclip.carrom` v1491 (`19.6.0`).
   - Extracted `libgame-CARROM-GooglePlay-Gold-Release-Module-1491.so` (48,998,992 bytes, SHA-256: `4CF2BA474FC9AEFDA142FBDCA46CF7B4D3EBF06A2114ECD8487A3E48448710B4`).
   - Verified that CatEngine's native AArch64 function hook RVAs in `catEngine.cpp` match the exact function prologues in the live Module-1491 binary.

---

## 2. Repository Structure & Module Breakdown

```
-CatEngine/
├── app/                        # Android application frontend (Jetpack Compose)
│   ├── src/main/java/          # Host UI, App.kt, BlackBoxLoader.kt, HomeScreen.kt
│   ├── libs/                   # Bundled AAR dependencies (catloading, floatingview, StateView)
│   └── build.gradle            # Target SDK 28, Compile SDK 35, Min SDK 24
├── Bcore/                      # Core Virtualization & Native CatEngine
│   ├── src/main/aidl/          # Virtual system services AIDL declarations
│   ├── src/main/cpp/           # Native engine (libengine.so): BoxCore, Dobby, xDL, catEngine
│   │   ├── catEngine/          # In-engine hook, OpenGL overlay, and custom physics V2
│   │   ├── Dobby/              # Prebuilt static libraries (arm64-v8a, armeabi-v7a)
│   │   └── xdl/                # Dynamic linker symbol resolver
│   ├── src/main/java/          # Virtual AMS, PMS, WMS, hooks, and calibration widgets
│   └── build.gradle            # Builds libengine.so via ndk-build (Android.mk)
├── black-reflection/           # Reflection annotations & runtime utility
│   └── src/main/java/          # @BClass, @BMethod, @BField, Reflector.java
├── compiler/                   # Compile-time annotation processor
│   └── src/main/java/          # JavaPoet proxy code generator
├── lib/arm64-v8a/              # Extracted Carrom Pool ARM64 native library (v1489)
├── scratch_se/                 # Extracted DEX and SO files from Carrom Pool
├── Bcore/scratch/              # Python hot-patching scripts used during past development
├── build.gradle                # Root project build script
└── gradle.properties           # JVM and AndroidX configuration
```

---

## 3. Subsystem Audit

### 3.1 Bcore Virtualization Engine
- **Sandboxed User Partitions**: Stores guest app data under `/data/data/com.engine.cat/virtual/users/<userId>/<pkg>/`.
- **System Service Stubs**: Replaces core Android system services with virtual implementations (`BActivityManagerService`, `BPackageManagerService`, `BStorageManagerService`, `BJobManagerService`).
- **Filesystem Virtualization (`IO.cpp`, `FileSystemHook.cpp`)**: Inline hooks POSIX file APIs (`open`, `openat`, `stat`, `access`) via Dobby to redirect access from `/data/data/com.miniclip.carrom` to the sandboxed path.
- **Anti-Detection (`AntiDetection.cpp`, `VirtualSpoof.cpp`)**: Filters `/proc/self/maps` and `/proc/self/status` to cloak `libengine.so` and virtual environment artifacts from guest integrity checks.

### 3.2 Overlay & Graphics Rendering
- **In-Engine OpenGL ES Hook (`GraphicsEngine.cpp`)**:
  - Hooks `eglSwapBuffers` using `DobbyHook`.
  - Injects custom GLSL vertex/fragment shaders (`s_lineProgram`, `s_circleProgram`) on the Cocos2d-x render thread.
  - Draws trajectory lines, reflection rays, and target circles directly onto the display surface before swapping buffers.
- **DecorView Overlay (`BoardCalibrationOverlay.java`)**:
  - Attached via `Application.ActivityLifecycleCallbacks` on `onActivityResumed`.
  - Injected directly into `activity.getWindow().getDecorView()` as a `FrameLayout` child.
  - Bypasses Android `SYSTEM_ALERT_WINDOW` permission because it lives in the Activity window hierarchy.

### 3.3 Input & Autoplay Execution
- **No Touch Emulation**: Autoplay does not inject Android touch events (`MotionEvent`, `AccessibilityService`, or `uinput`).
- **Direct Function Invocation**:
  ```cpp
  orig_aimChanged(aimDelegate, 2, pullAngle, forceValClamped);
  orig_shotFired(shotDelegate, pIndex, true, 0, pullAngle, forceValClamped);
  ```
  Calls the game's internal native delegates directly from C++.

### 3.4 Reverse Engineering Tools Audit
- `jadx-1.5.6.zip` located in root was verified via byte inspection (`0D-0A-0D-0A...<!DOCTYPE html>`). It is a corrupted HTML download page, not an executable decompiler.
- JADX must be obtained via a valid distribution or run via REA / system JDK tools.

---

## 4. Reusable Code vs. Missing Components

| Component | Status | Action Required |
| :--- | :--- | :--- |
| **BlackBox Virtualization** | **Fully Functional** | Retain as primary execution container. |
| **Physics Simulation V2** | **Fully Functional** | Retain and benchmark against gameplay dataset. |
| **OpenGL ES Overlay Hook** | **Fully Functional** | Retain for zero-latency in-game rendering. |
| **DecorView Calibration UI** | **Fully Functional** | Retain for manual user fine-tuning. |
| **Root Build System** | **Fully Resolved** | Added `settings.gradle`, aligned NDK 28.2, full APK build verified. |
| **Computer Vision Engine** | **Missing (0%)** | Must be designed and implemented for external mode. |
| **Screen-Capture Pipeline** | **Missing (0%)** | Must be implemented (`glReadPixels` and `MediaProjection`). |
| **Dynamic Signature Scanner** | **Missing** | Replace hardcoded RVAs with pattern scanning. |
| **Humanized Input Dispatcher** | **Missing** | Implement Bezier-curve touch emulation with timing jitter. |

