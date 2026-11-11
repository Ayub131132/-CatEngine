# CatEngine: Developer & Contributor Guide

**Target Audience:** Senior Android Engineers, C++ Systems Developers, and Security Researchers.

---

## 1. Repository Layout & Module Boundaries

The repository is divided into four distinct Gradle modules and specialized root directories:

### Gradle Modules:
1. **`:app`** (`app/`):
   - **Role:** User interface and application container.
   - **Stack:** Kotlin, Jetpack Compose Material 3, Android Architecture Components.
   - **Responsibilities:**
     - Clones applications from the host system into BlackBox storage (`/data/data/com.engine.cat/virtual/`).
     - Manages user settings, trajectory color preferences, and calibration overlays.
     - Spawns virtual processes (`top.niunaijun.blackbox.core.VApp`).

2. **`:Bcore`** (`Bcore/`):
   - **Role:** Core virtualization framework and native hook injection engine.
   - **Stack:** Java, AIDL, C++17, Android NDK (`Android.mk`).
   - **Responsibilities:**
     - Virtualizes Android framework services: ActivityManagerService (AMS), PackageManagerService (PMS), WindowManagerService (WMS).
     - Compiles `libengine.so` containing BlackBox Core, Dobby hooks, xDL dynamic linker, and `catEngine` prediction logic.
     - Injects `BoardCalibrationOverlay` directly into the DecorView of the virtualized game activity.

3. **`:black-reflection`** (`black-reflection/`):
   - **Role:** Runtime reflection abstraction library.
   - **Stack:** Java 8.
   - **Annotations:** `@BClass`, `@BMethod`, `@BField`, `@BConstructor`.

4. **`:compiler`** (`compiler/`):
   - **Role:** Compile-time JavaPoet annotation processor.
   - **Output:** Generates fast, type-safe reflection proxies for hidden Android framework classes.

---

## 2. Native C++ Engineering Guidelines (`Bcore/src/main/cpp/`)

### 2.1 File Organization
- `catEngine/`: Contains all gameplay prediction, OpenGL ES rendering, and physics code.
  - `catEngine.cpp`: Main hook orchestrator, memory reading routines, and state dispatchers.
  - `GraphicsEngine.cpp` & `GraphicsEngine.h`: OpenGL ES pipeline and trajectory geometry generator.
  - `v2/PhysicsEngineV2.cpp` & `PhysicsEngineV2.h`: Deterministic 1000 Hz CCD physics engine.
- `BoxCore.cpp`, `hidden_api.cpp`, `IO.cpp`: System call redirection and Android runtime virtualization.
- `Hook/`: Dobby-based inline hooks for filesystem, dex files, and binder IPC.
- `Dobby/`: Prebuilt static libraries for AArch64 and ARMv7.
- `xdl/`: Robust symbol resolution bypassing dynamic linker namespace restrictions.

### 2.2 Compilation Invariants
All native components are built via `ndkBuild` through `Bcore/src/main/cpp/Android.mk`:
- **C++ Standard:** `-std=c++17`
- **Visibility:** `-fvisibility=hidden`
- **Optimization:** `-ffunction-sections -fdata-sections -Wl,--gc-sections,--strip-all`
- **16KB Page Alignment:** Mandatory `-Wl,-z,max-page-size=16384` to guarantee compatibility with Android 15.
- **Diagnostics:** Always use `ALOGI`, `ALOGW`, and `ALOGE` macros defined with `LOG_TAG`.

---

## 3. Safe Memory Introspection Patterns

When interacting with the target game process (`com.miniclip.carrom`), adhere to strict memory safety rules:

1. **Never Dereference Raw Pointers Directly:**
   Use the verified memory reading helpers:
   ```cpp
   safeReadPtr(uintptr_t addr, uintptr_t* outVal);
   safeReadDouble(uintptr_t addr, double* outVal);
   safeReadFloat(uintptr_t addr, float* outVal);
   safeReadInsn(uintptr_t addr, uint32_t* outInsn);
   ```
2. **Handle NaN and Infinite Values:**
   Always validate float/double telemetry using `std::isnan()` and `std::isinf()` before passing values into the physics simulator.
3. **Verify Function Prologues Before Hooking:**
   Always inspect the original opcode at the target RVA using `safeReadInsn()` to avoid patching corrupt or unmapped memory regions.

---

## 4. Build & Verification Workflow

```bash
# Clean build outputs
./gradlew clean

# Build native core library
./gradlew :Bcore:assembleDebug

# Build complete application package
./gradlew :app:assembleDebug

# Verify 16KB page alignment
llvm-readelf -l app/build/intermediates/merged_native_libs/debug/mergeDebugNativeLibs/out/lib/arm64-v8a/libengine.so
```
