# CatEngine Architecture & System Design Document

**Lead Engineer:** Senior Android Reverse Engineer & System Architect  
**Project:** CatEngine  
**Target Environment:** Android 8.0 - 15 (ARM64-v8a / armeabi-v7a)

---

## 1. Process & Runtime Model

CatEngine relies on a dual-process Android architecture:

```
+--------------------------------------------------------------------------+
| 1. Main Host Process: com.engine.cat                                     |
|    - App.kt -> Application entry point                                   |
|    - BlackBoxLoader.kt -> Pre-initialization & lifecycle configuration   |
|    - HomeScreen.kt -> Compose UI frontend (Games, Settings, Community)  |
|    - DeviceManager.kt -> SHA-256 hardware entropy binding               |
+--------------------------------------------------------------------------+
                                    |
                                    | Spawns sandboxed guest via IPC
                                    v
+--------------------------------------------------------------------------+
| 2. Sandboxed Guest Process: com.engine.cat:p0                            |
|    - BActivityThread -> Injected into ActivityThread.mH                  |
|    - Guest Application Context: com.miniclip.carrom                      |
|    - Decorated Window: BoardCalibrationOverlay attached to DecorView     |
|    - Native Library: libengine.so                                        |
|    - Target Game Library: libgame.so (Cocos2d-x Engine)                  |
+--------------------------------------------------------------------------+
```

---

## 2. Java Virtualization & Service Proxying

### 2.1 Hidden API Unlocking
BlackBox leverages `FreeReflection` (`com.github.tiann:FreeReflection:3.2.2`) to disable Android runtime hidden API enforcement (`VMRuntime.setHiddenApiExemptions`), allowing reflective access to system internals on Android 9 through 14+.

### 2.2 Dynamic Service Proxies
When the guest app interacts with Android system services:
1. `ClassInvocationStub.injectHook()` intercepts calls to `ServiceManager.getService()`.
2. Returned Binder tokens are replaced with dynamic proxies (`java.lang.reflect.Proxy`).
3. System calls (`startActivity`, `registerReceiver`, `getPackageInfo`) are routed to CatEngine's internal AIDL server implementations:
   - `BActivityManagerService`
   - `BPackageManagerService`
   - `BStorageManagerService`
   - `BJobManagerService`

---

## 3. Native Layer Architecture (`libengine.so`)

`libengine.so` is compiled via Android NDK `ndk-build` using `Bcore/src/main/cpp/Android.mk`.

### 3.1 Interception Frameworks
- **Dobby (`libdobby.a`)**: In-process runtime inline hooking. Replaces function prologues with AArch64 trampolines to redirect execution to custom replacement handlers while maintaining backup pointers to original instructions.
- **xDL (`xdl.c`)**: Enhanced dynamic linker implementation. Bypasses `dlopen` limitations on Android 7.0+ (Nougat) to inspect ELF headers, symbol tables, and memory mappings in loaded `.so` libraries.

### 3.2 CatEngine Native Subsystems
```
                         libengine.so
                              |
    +-------------------------+-------------------------+
    |                         |                         |
    v                         v                         v
catEngine.cpp          GraphicsEngine.cpp      PhysicsEngineV2.cpp
(Hooks & Memory)       (OpenGL Overlay)        (Rigid-Body Simulator)
```

#### A. Memory Introspection & Game Hooks (`catEngine.cpp`)
1. **Dynamic Base Address Resolution**:
   ```cpp
   uintptr_t resolveLibgameBase() {
       uintptr_t phdrBase = 0;
       dl_iterate_phdr([](struct dl_phdr_info* info, size_t size, void* data) -> int {
           if (info && info->dlpi_name && strstr(info->dlpi_name, "CARROM") && strstr(info->dlpi_name, "libgame")) {
               *reinterpret_cast<uintptr_t*>(data) = info->dlpi_addr;
               return 1;
           }
           return 0;
       }, &phdrBase);
       return phdrBase;
   }
   ```
2. **Hook Registry (Target RVAs for Carrom Pool v1489/v1491)**:
   - `setupLocalStrikerBar` (`0x1A28F98`): Detects local turn start.
   - `setupOpponentStrikerBar` (`0x1A1DDF4`): Detects opponent turn start.
   - `startMatchEnding` (`0x19DF2A8`): Resets state on match termination.
   - `forcePuckPosition` (`0x1B3352C`): Intercepts puck coordinate updates.
   - `forceStrikerPosition` (`0x1B33570`): Intercepts striker coordinate updates.
   - `onAimChanged` (`0x19843A0`): Intercepts manual slingshot angle and pull magnitude.
   - `onShotFired` (`0x1984568`): Intercepts user shot release.

#### B. Graphical Overlay Pipeline (`GraphicsEngine.cpp`)
1. Hook installed on `eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)`.
2. Frame preparation:
   - Queries active TableController transform (`GraphicsEngine_SetNativeAffineTransform`).
   - Maps table coordinate space ($[-38, +38]\text{ cm}$) to device screen pixels.
3. Shader execution:
   - `s_lineProgram`: Compiles GLSL shaders for anti-aliased trajectory rays and deflection lines with feathered edges.
   - `s_circleProgram`: Renders discs and pocket highlight rings.
4. Executes original `eglSwapBuffers`, presenting the combined game and prediction visuals seamlessly.

#### C. Physics Simulation Engine (`PhysicsEngineV2.cpp`)
- Evaluates forward rigid-body motion with continuous collision detection.
- Details covered in `PHYSICS_ENGINE_RESEARCH.md`.

#### D. Autonomous Autoplay Loop
- `startAutoShotWorker()` thread runs continuously at a 35 ms polling interval.
- Validates:
  1. `g_matchActive == true`
  2. `s_isLocalTurnReady == true`
  3. Discs stationary ($\Delta d < 0.35\text{ cm}$)
  4. Striker present on baseline ($|y| \approx 25.35\text{ cm}$)
- Triggers `GraphicsEngine_ScanAndExecuteBestShot()`.
- Calls internal game delegate:
  ```cpp
  orig_shotFired(shotDelegate, playerIndex, true, 0, pullAngle, forceVal);
  ```

