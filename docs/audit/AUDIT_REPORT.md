# CatEngine: Phase 1 Repository Technical Audit Report

**Date:** November 2026  
**Auditor:** Senior Android, Rust/C++, and Computer Vision Engineer  
**Target Repository:** [https://github.com/Ayub131132/-CatEngine](https://github.com/Ayub131132/-CatEngine)  
**Evaluated Branch/Commit:** `main` (`c9df76de47bff84f76caccb87a4d61cc7b7a5270`)

---

## Executive Summary

CatEngine is a specialized Android virtualization and game-augmentation platform built upon the **BlackBox** open-source user-space sandbox. The repository contains a hybrid architecture combining Kotlin/Java Android framework interception with high-performance C++ native inline hooking (`Dobby`), dynamic linker symbol resolution (`xDL`), OpenGL ES swap buffer interception (`eglSwapBuffers`), and a custom 2D rigid-body carrom physics engine.

Contrary to conventional aim-assist tools that rely on external screen capture and Computer Vision (CV), CatEngine executes **in-process within the virtualized guest app's address space**. It directly intercepts the target game's native library (`libgame.so` from Miniclip's *Carrom Pool*) to read disc coordinates, geometry, baseline limits, and friction directly from memory, injecting custom OpenGL shaders to render trajectory lines and calling internal game delegates for shot execution.

**Crucial Findings:**
1. **Zero Computer Vision Integration**: There is **no OpenCV**, no ML model, and no image-processing code in the repository.
2. **Zero Screen Capture Capability**: No `MediaProjection`, `PixelCopy`, or `glReadPixels` implementations exist.
3. **Build Pipeline Broken Out-of-the-Box**: The root repository was committed without `settings.gradle` / `settings.gradle.kts`. Furthermore, `build.gradle` specifies an uninstalled NDK version (`30.0.16248370`), and missing JitPack repository declarations cause dependency resolution failures for `FreeReflection`.
4. **Authentic Physics vs. In-Engine Snapshot**: The codebase contains two distinct simulation modes:
   - **Mode 0 (Snake Engine CCD)**: A custom continuous collision detection rigid-body forward simulator (`PhysicsEngineV2.cpp`).
   - **Mode 1 (In-Engine Native)**: A state snapshotting and restore routine (`CatEngine_SimulateInEngineNative`) that executes the game's actual physics routine (`tableLogicStep`).
5. **High Security & Account Ban Risks**: In-process hooking via hardcoded Relative Virtual Addresses (RVAs) directly into a commercial multiplayer game poses severe detection and ban risks if used outside an owned/authorized environment.

---

## 1. Source Code & Module Inventory

The repository consists of four primary modules, alongside auxiliary reverse-engineering payloads:

| Module / Directory | Type / Language | Description & Key Responsibilities |
| :--- | :--- | :--- |
| **`:app`** | Android Application (Kotlin, Jetpack Compose) | Host frontend application (`com.engine.cat`). Manages BlackBox core initialization (`App.kt`, `BlackBoxLoader.kt`), game launcher UI (`HomeScreen.kt`), device entropy binding (`DeviceManager.kt`), mock license validation (`LicenseManager.kt`), and user consent dialogs (`LegalDocuments.kt`). |
| **`:Bcore`** | Android Library (Java, Kotlin, C/C++, AIDL) | BlackBox virtualization core (`top.niunaijun.blackbox`). Virtualizes Android runtime and system services (AMS, PMS, WMS, JobScheduler, Storage, Location). Houses the native engine `libengine.so` (`Android.mk`, `catEngine/`, `Hook/`, `JniHook/`, `Utils/`). Houses UI overlay widgets (`BoardCalibrationOverlay.java`, `CatEngineConfig.kt`). |
| **`:black-reflection`**| Java Library (Java 17) | Reflection abstraction framework. Defines annotations (`@BClass`, `@BMethod`, `@BField`) for type-safe compile-time access to hidden Android OS APIs without runtime crashes. |
| **`:compiler`** | Java Annotation Processor (Java 17) | Annotation processor utilizing `javapoet` and `auto-service` to generate runtime reflection proxies for `:black-reflection`. |
| **`lib/` & `scratch_se/`** | Reverse Engineering Binaries | Extracted ARM64 binaries of *Carrom Pool* (`libgame-CARROM-GooglePlay-Gold-Release-Module-1489.so`), decompiler binaries (`jadx-1.5.6.zip`), test APK DEX files (`classes.dex`), and compiled native artifacts (`libengine.so`, `libapp.so`). |
| **`Bcore/scratch/`** | Python Automation Scripts | Hot-patching scripts (`fix_hooks_trigger.py`, `patch_graphics_engine.py`, `update_cat_engine.py`, `update_overlay.py`) used during development to rewrite C++ and Java sources. |

---

## 2. Deep Dive: BlackBox Virtualization Engine

### 2.1 Sandboxing & Process Isolation
BlackBox implements a user-space container (similar to VirtualApp / VirtualAPK):
- **User-Space Namespacing**: Guest applications run under the host application's Linux UID, isolated into virtual user partitions (`/data/data/com.engine.cat/virtual/users/<userId>/<packageName>/`).
- **Lifecycle & Process Management**: `BActivityThread` intercepts application creation. Android components (Activities, Services, Providers, Receivers) declared in guest manifests are dynamically bound to stub components declared in `Bcore/src/main/AndroidManifest.xml` (e.g., `StubActivity$P0`, `StubService$P0`).

### 2.2 Dual-Tier Hooking Architecture
1. **Java Framework Interception (`ClassInvocationStub`)**:
   - Uses Java reflection (`FreeReflection` by @tiann and `:black-reflection`) to unlock Android hidden API restrictions.
   - Replaces system service Binder tokens (obtained via `ServiceManager`) with dynamic proxies. When the guest calls `Context.getSystemService(Context.ACTIVITY_SERVICE)`, `ActivityManagerService` calls are routed to `BActivityManagerService`.
2. **Native Hooking (`libengine.so`)**:
   - Built via `ndk-build` using `Android.mk`.
   - **`IO.cpp` & `FileSystemHook.cpp`**: Hooks POSIX file calls (`open`, `openat`, `stat`, `access`) via Dobby inline hooks to reroute guest app filesystem paths into virtual app private storage.
   - **`AntiDetection.cpp` & `VirtualSpoof.cpp`**: Intercepts `/proc/self/maps` and `/proc/self/status` reads to conceal virtual container signatures, memory allocations, and hook signatures from guest anti-cheat scanners.

---

## 3. Analysis of Existing Capabilities

### 3.1 Screen-Capture Capabilities
- **Status**: **Completely Absent (0%)**.
- **Source Verification**: A complete scan of `app/` and `Bcore/` confirms that `MediaProjection`, `VirtualDisplay`, `PixelCopy`, and `glReadPixels` do not exist in the active codebase.
- **Note**: The lone match for `VirtualDisplay` in `IDisplayManagerProxy.java` is a virtualization hook preventing guest apps from spawning unauthorized virtual displays.

### 3.2 Image-Processing & Computer Vision
- **Status**: **Completely Absent (0%)**.
- **Source Verification**: No OpenCV headers (`opencv2`), no OpenCV `.so` libraries, and no machine learning inference models (TFLite, ONNX, NCNN) exist anywhere in the repository.
- **Mechanism**: The engine does not perceive the screen visually. Disc positions are extracted directly from C++ memory structures inside `libgame.so`.

### 3.3 Graphical Rendering & Overlay
CatEngine employs two distinct overlay systems:

1. **In-Game OpenGL ES SwapBuffers Hook (`GraphicsEngine.cpp`)**:
   - Hooks `eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)` using `DobbyHook`.
   - Directly compiles embedded GLSL vertex and fragment shaders (`s_lineProgram`, `s_circleProgram`) on the game's active OpenGL ES context.
   - Performs coordinate transformation between Carrom Table coordinates (centimeters, origin `[0,0]`, range `[-38, +38]`) and screen viewport pixels via affine transformation matrices (`u_MVP`).
   - Renders anti-aliased laser prediction lines, cushion bounce deflection vectors, target puck trajectories, and target pocket circles before swapping the frame to display.
2. **Android Window DecorView Overlay (`BoardCalibrationOverlay.java`)**:
   - Since the virtualized guest Activity executes within the same host process, BlackBox registers an `ActivityLifecycleCallbacks` listener.
   - Upon `onActivityResumed`, it retrieves the Activity window (`activity.getWindow().getDecorView()`) and directly injects `FloatingButtonView` and `AutoShotButtonView` into the `FrameLayout`.
   - Bypasses `SYSTEM_ALERT_WINDOW` permission checks since views are attached inside the application window rather than a system-level overlay.

### 3.4 Input Handling & Autoplay Execution
- **Manual Input**: Touch input on the floating UI is handled via standard Android `View.OnTouchListener` with `MotionEvent` handling for dragging and opening Compose configuration dialogs.
- **Autoplay / Auto-Shot Execution (`catEngine.cpp`, `GraphicsEngine.cpp`)**:
   - Autoplay is driven by an autonomous C++ background worker thread `startAutoShotWorker()`.
   - **Turn Detection**: Polls `s_isLocalTurnReady`, `g_matchActive`, and verifies that discs are stationary (`dist < 0.35 cm`).
   - **Target Evaluation**: Calls `GraphicsEngine_ScanAndExecuteBestShot()` to evaluate pocket angles and bounce trajectories.
   - **Execution Mechanism**: Does **NOT** inject synthetic touch events (no `MotionEvent`, no Accessibility, no `sendPointerSync`). Instead, it directly invokes the game's internal native C++ delegates:
     ```cpp
     orig_aimChanged(aimDelegate, 2, pullAngle, forceValClamped);
     orig_shotFired(shotDelegate, pIndex, true, 0, pullAngle, forceValClamped);
     ```
   - This bypasses all Android input dispatch systems, writing shot parameters directly into the game's internal event loop.

---

## 4. Build System Diagnostics & Verification (Task 6)

When attempting to build the project from a clean state using `./gradlew.bat projects` or `./gradlew.bat assembleDebug`, multiple critical build failures occur:

### Error 1: Missing Root `settings.gradle`
```
FAILURE: Build failed with an exception.
* Where: Build file '...\build.gradle' line: 2
* What went wrong:
Plugin [id: 'com.android.application', version: '8.13.2', apply: false] was not found in any of the following sources:
- Gradle Core Plugins
- Plugin Repositories (could not resolve plugin artifact 'com.android.application:com.android.application.gradle.plugin:8.13.2')
  Searched in the following repositories: Gradle Central Plugin Repository
```
- **Cause**: No `settings.gradle` or `settings.gradle.kts` exists in the repository. Gradle cannot discover Google's Maven repository for plugins and cannot resolve submodules (`:app`, `:Bcore`, `:black-reflection`, `:compiler`).

### Error 2: Missing JitPack Repository Declaration
- **Cause**: `Bcore/build.gradle` line 136 specifies:
  `implementation 'com.github.tiann:FreeReflection:3.2.2'`
- Without `maven { url 'https://jitpack.io' }` in `dependencyResolutionManagement` or subproject repositories, dependency resolution fails.

### Error 3: NDK Version Mismatch & Forced 1GB+ Download
- **Cause**: `build.gradle` line 12 defines:
  `ndkVersion = "30.0.16248370"`
- While the installed SDK environment provides NDK versions `26.1.10909125`, `27.1.12297006`, and `28.2.13676358`, AGP attempts to download the uninstalled NDK 30 from Google over the network, stalling command-line builds on unmetered or offline environments.

### Error 4: Windows AIDL Path Unicode Escape Syntax Bug
- **Cause**: In `Bcore/build.gradle` lines 83-108, the author had to write a custom Gradle hook to strip backslashes (`\u`) generated by the AIDL compiler on Windows file paths (e.g., `C:\Users\...`), which causes `unclosed Unicode escape` fatal compilation errors in `javac`.

---

## 5. Security, Anti-Cheat, and Compatibility Risks

| Risk Category | Severity | Technical Details |
| :--- | :--- | :--- |
| **Account Ban / Detection** | **CRITICAL** | Calling `orig_shotFired` directly in memory without corresponding touch event dispatch (`MotionEvent.ACTION_DOWN/MOVE/UP`) leaves zero input latency and missing gesture curves. Server-side anomaly detection can easily flag non-human shot releases. |
| **Hardcoded RVAs** | **HIGH** | Function offsets (e.g., `0x1A28F98`, `0x1B3352C`, `0x1984568`) target a specific build of `libgame.so` (v1489/v1491). Any minor game update causes immediate memory corruption, `SIGSEGV`, or game crash. |
| **Android 14+ / 15 Security** | **HIGH** | Android 14+ enforces stricter blocked access to private app data directories and non-SDK interfaces. Android 15 introduces mandatory 16 KB memory page size alignment (`-z,max-page-size=16384` is present in `Android.mk`, but third-party static libraries such as `libdobby.a` must also be verified for 16 KB compliance). |
| **Process Sandboxing Detection** | **HIGH** | Commercial integrity checkers (Play Integrity API, SafetyNet, anti-cheat daemons) check for split UIDs, modified `/proc/self/cmdline`, hook trampolines in memory, and `p0` process name extensions. |

---

## 6. Summary of Strengths and Gaps

### What Works & Is Reusable:
- Full Android user-space virtualization harness (BlackBox core).
- Deep memory introspection engine for Carrom Pool game state (`catEngine.cpp`).
- Reverse-engineered continuous collision detection 2D physics engine (`PhysicsEngineV2.cpp`).
- High-performance OpenGL ES trajectory and UI overlay renderer (`GraphicsEngine.cpp`).
- Non-intrusive `DecorView` floating calibration and settings interface (`BoardCalibrationOverlay.java`).

### What Is Missing:
- Functioning Gradle build configuration (`settings.gradle` omitted from Git).
- Screen-capture and computer vision pipeline for external / non-virtualized operation.
- Fallback automated element locating and testing framework.
- Humanized shot execution timing (randomized pull delays, touch curve simulation).
- Dynamic signature scanning / pattern scanning to replace fragile hardcoded RVAs.

