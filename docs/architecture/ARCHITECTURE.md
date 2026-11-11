# CatEngine: Architecture & System Design Document

**Author:** Senior Android, Rust/C++, and Computer Vision Engineer  
**System Target:** Android 8.0 - Android 15 (ARM64-v8a, armeabi-v7a)  
**Core Domain:** Android Runtime Virtualization, Native Reverse-Engineering, In-Engine Physics & OpenGL Graphics Interception

---

## 1. High-Level Architectural Overview

CatEngine operates as a unified multi-process container. It launches guest Android packages inside an in-process sandbox without root access, hooking both Java framework services and native C/C++ game engines.

```
+-----------------------------------------------------------------------------------+
|                               CatEngine Host APK                                  |
|                               (com.engine.cat)                                    |
|                                                                                   |
|  +--------------------+   +-----------------------+   +------------------------+  |
|  |   Jetpack Compose  |   |   BlackBox Loader     |   |    Device Manager      |  |
|  |    UI Frontend     |-->|   & App Manager       |-->|   (Hardware Entropy)   |  |
|  |  (HomeScreen.kt)   |   |   (BlackBoxCore.java) |   |   (DeviceManager.kt)   |  |
|  +--------------------+   +-----------------------+   +------------------------+  |
+--------------------------------------|--------------------------------------------+
                                       | Launches Guest in Sandboxed Process (:p0)
                                       v
+-----------------------------------------------------------------------------------+
|                       Sandboxed Guest Process (:p0)                               |
|                                                                                   |
|  +-----------------------------------------------------------------------------+  |
|  |                             Java Runtime Layer                              |  |
|  |  - BActivityThread & ClassInvocationStub (System Service Virtualization)    |  |
|  |  - BoardCalibrationOverlay.java (Injected directly into DecorView)          |  |
|  |  - CatEngineConfig.kt (Compose Settings & SharedPreferences Sync)          |  |
|  +-----------------------------------------------------------------------------+  |
|                                       | JNI Bridge (NativeCore.java)              |
|                                       v                                           |
|  +-----------------------------------------------------------------------------+  |
|  |                         Native Core (libengine.so)                          |  |
|  |                                                                             |  |
|  |  +-------------------------------------+   +-----------------------------+  |  |
|  |  |          BlackBox Hooks             |   |         CatEngine           |  |  |
|  |  |  - IO.cpp (POSIX VFS Redirection)   |   |  - catEngine.cpp (Hooks)    |  |  |
|  |  |  - AntiDetection.cpp (/proc maps)   |   |  - GraphicsEngine.cpp (GLES)|  |  |
|  |  |  - Dobby & xDL (Dynamic Linker)     |   |  - PhysicsEngineV2.cpp(Sim) |  |  |
|  |  +-------------------------------------+   +-----------------------------+  |  |
|  +-----------------------------------|-----------------------------------------+  |
|                                      | Inline Hooks & Memory Introspection        |
|                                      v                                            |
|  +-----------------------------------------------------------------------------+  |
|  |                     Guest Native Engine (libgame.so)                        |  |
|  |  - Cocos2d-x Render Loop -> eglSwapBuffers Hook (Trajectory Overlay)        |  |
|  |  - TableController / TableLogic (Discs, Baseline, Friction, Restitution)   |  |
|  |  - ControlsLogicLocal (onAimChanged & onShotFired Function Pointers)        |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

---

## 2. Virtualization & Subsystem Architecture

### 2.1 Java-Level Interception Pipeline
1. **Application Attachment**: `App.kt` invokes `BlackBoxCore.get().onBeforeMainApplicationAttach()`.
2. **Activity Thread Replacement**: `BActivityThread` captures the Android `ActivityThread.mH` handler, substituting the application class loader and hooking `Instrumentation`.
3. **AIDL Virtual Services**:
   - `BActivityManagerService` replaces `IActivityManager` Binder tokens.
   - `BPackageManagerService` provides virtual package metadata without registering with system `PackageManager`.
   - `BStorageManagerService` virtualizes storage volume mounts.
4. **Compile-Time Reflection Proxying (`:black-reflection` & `:compiler`)**:
   Instead of fragile runtime reflection, the `:compiler` annotation processor inspects `@BClass` and `@BMethod` declarations to generate fast, type-safe reflection wrappers (`Reflector.java`) that circumvent Android hidden API access restrictions.

---

## 3. CatEngine Native Architecture (`libengine.so`)

The native core is organized into three primary engines executing concurrently:

```
                         libengine.so
                              |
    +-------------------------+-------------------------+
    |                         |                         |
    v                         v                         v
catEngine.cpp          GraphicsEngine.cpp      PhysicsEngineV2.cpp
(State & Hooks)        (Render & Shot Search)  (2D Collision Sim)
```

### 3.1 Memory Introspection & Hook Engine (`catEngine.cpp`)
- **Base Resolution (`resolveLibgameBase`)**: Scans `/proc/self/maps` using `dl_iterate_phdr` to dynamically locate the base address of `libgame.so` (Cocos2d-x / Miniclip engine).
- **Target Offsets & Struct Layout**:
  ```cpp
  struct CarromOffsets {
      uintptr_t setupLocalStrikerBar   = 0x1A28F98; // Local turn & match start
      uintptr_t setupOpponentStrikerBar= 0x1A1DDF4; // Opponent turn
      uintptr_t startMatchEnding       = 0x19DF2A8; // Match termination
      uintptr_t forcePuckPosition      = 0x1B3352C; // Puck coordinates
      uintptr_t forceStrikerPosition   = 0x1B33570; // Striker coordinates
      uintptr_t onAimChanged           = 0x19843A0; // Slingshot pull update
      uintptr_t onShotFired            = 0x1984568; // Shot trigger delegate
      uintptr_t tableLogicStep         = 0x1B2B88C; // In-engine physics step
      uintptr_t launchStriker          = 0x1B2C3A4; // In-engine striker impulse
  };
  ```
- **State Capture**:
  - Dynamically captures instances of `TableController`, `TableLogic`, `MatchController`, and `ControlsLogicLocal`.
  - Reads live disc arrays (`tlPuckMap`), baseline constraints (`blMinX`, `blMaxX`, `blY`), and pocket radii directly from memory addresses.

### 3.2 Graphical Overlay Engine (`GraphicsEngine.cpp`)
- **Hook Target**: `eglSwapBuffers` in the active OpenGL ES driver.
- **Render Pipeline**:
  1. Intercepts the frame swap before presentation to the display.
  2. Queries current aim state (`GraphicsEngine_UpdateAim`) and table coordinates.
  3. Prepares GLSL vertex buffers (`a_Pos`, `a_Coord`) and sets uniform matrices (`u_MVP`, `u_Color`).
  4. Renders:
     - **Primary Striker Ray**: Trajectory from striker to first obstacle.
     - **Cushion Deflection Vectors**: Angle-of-incidence reflections off table borders.
     - **Target Puck Trajectory**: Predicted post-collision path towards target pockets.
     - **Pocket Ring Highlights**: Visual cues identifying valid pot destinations.
  5. Cleans up GL state and calls the original `eglSwapBuffers`.

### 3.3 Physics Simulation Engines

#### Mode 0: Continuous Collision Detection Simulator (`PhysicsEngineV2.cpp`)
A custom deterministic 2D forward-Euler simulator:
- **Discs**: Striker ($m=3.0, r=2.65\text{ cm}$), Pucks ($m=1.0, r=1.95\text{ cm}$).
- **Friction Model**: Coulomb friction replicating boric acid on polished wood:
  - Striker sliding deceleration: $\mu_s = 0.1600 \implies a = 156.8\text{ cm/s}^2$
  - Puck rolling friction: $\mu_p = 0.1080 \implies a = 105.8\text{ cm/s}^2$
- **Collision Dynamics**:
  - Disc-disc elastic restitution: $e_{\text{body}} = 1.00$
  - Cushion bounce restitution: $e_{\text{cushion}} = 0.88$
- **Pocket Gravity Wells**:
  - Inward gravitational suction acceleration: $g_{\text{pocket}} = 980\text{ cm/s}^2$
  - Maximum capture velocity cutoff: $v_{\text{drop}} = 250.0\text{ cm/s}$

#### Mode 1: Native In-Engine Simulator (`CatEngine_SimulateInEngineNative`)
- Clones internal `TableLogic` state from snapshot memory (`tlSnapshot` at offset `0x1D0`).
- Calls the game's actual native `launchStriker` function.
- Repeatedly invokes `tableLogicStep(tc, dt)` across forward timesteps, recording trajectory points.
- Restores the snapshot memory, leaving the live match completely unaltered.

---

## 4. Input Injection & Autoplay Architecture

```
+-------------------------------------------------------------+
|               startAutoShotWorker() (Thread)                |
+-------------------------------------------------------------+
                              |
       +----------------------+----------------------+
       | Polls turn status, puck velocity, baseline |
       v                                             v
[Pucks Moving?]                              [Discs Stationary]
       |                                             |
  Yield (Sleep)                        +-----------------------------+
                                       | Best Shot Search & Trajectory|
                                       | (GraphicsEngine_Scan...)    |
                                       +-----------------------------+
                                                     |
                                       +-----------------------------+
                                       | Direct In-Engine Invocation |
                                       | - orig_aimChanged()         |
                                       | - orig_shotFired()          |
                                       +-----------------------------+
```

Rather than simulating imprecise touchscreen taps on the Android display, CatEngine's autoplay operates entirely via in-memory function calling:
1. When all pucks are confirmed stationary, `GraphicsEngine_ScanAndExecuteBestShot()` evaluates candidate shot angles across all table pucks.
2. The optimal shot trajectory (direct pot, single bank, or combo) is selected.
3. The engine invokes `orig_aimChanged()` to aim the striker.
4. The engine invokes `orig_shotFired()` on the captured `shotDelegate` pointer with exact power and radians.

