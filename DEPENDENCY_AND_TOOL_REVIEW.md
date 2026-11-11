# CatEngine: Comprehensive Dependency & Tooling Review

**Lead Engineer:** Senior Android, Rust/C++, and Computer Vision Engineer  
**Host Environment:** Windows 11 x64, JDK 21, Node.js 24, Android SDK/NDK 28.2  
**Target Device Environment:** Android 8.0 - 15 (ARM64-v8a)

---

## 1. Reverse Engineering & Binary Analysis Tooling

| Tool / Framework | Installed Status | Capability / Purpose | Licensing & Platform Compatibility |
| :--- | :--- | :--- | :--- |
| **REA (`rea-agents` v6.7.0/6.8.0)** | **Installed & Verified** (`rea` CLI in PATH) | Reverse engineering orchestration and artifact analysis. Registered as MCP server with Antigravity. | MIT / Open Source. Fully operational on Windows 11 host. |
| **LLVM Toolchain (NDK 28.2)** | **Installed & Verified** | `llvm-readelf`, `llvm-nm`, `llvm-strings`, `llvm-objdump`. Provides dynamic symbol extraction, RTTI recovery, and disassembly. | Apache 2.0 with LLVM Exception. Native Windows CLI tools. |
| **JADX Decompiler** | `jadx-1.5.6.zip` in root was corrupted HTML. | DEX to Java decompiler. Can be executed headless via REA JADX MCP jar or standalone JADX 1.5.0+. | Apache 2.0. Cross-platform Java. |
| **Ghidra (v12.1.x)** | Not installed locally (`GHIDRA_INSTALL_DIR` unset) | Comprehensive NSA decompiler & static disassembler for AArch64. | Apache 2.0. Requires Java 17/21. Recommended for deep offline decompilation. |
| **Kuna / IDA / Hopper** | Not installed / commercial licenses required | Commercial disassemblers. Not required; LLVM and Ghidra provide equivalent open capabilities. | Proprietary. |

---

## 2. Android Virtualization & Runtime Hooking

| Library / Tool | Current Project Integration | Functionality | Licensing & Maintenance |
| :--- | :--- | :--- | :--- |
| **BlackBox (`:Bcore`)** | **Active Core** | User-space process sandboxing, virtual AMS/PMS, filesystem redirection. | Apache 2.0. Community maintained fork. |
| **Dobby (`libdobby.a`)** | **Bundled in `Bcore/src/main/cpp/Dobby/`** | Fast lightweight AArch64 inline hook framework. Used for `eglSwapBuffers` and game function interception. | Apache 2.0. Maintained by jmpews. Compatible with ARM64; 16KB page alignment flag required for Android 15. |
| **xDL** | **Bundled in `Bcore/src/main/cpp/xdl/`** | Android dynamic linker proxy. Bypasses `dlopen` restrictions to locate loaded `.so` memory maps. | MIT License. Maintained by ByteDance. Highly reliable. |
| **FreeReflection** | **Declared in `Bcore/build.gradle` (3.2.2)** | Unlocks Android hidden API access restrictions via runtime ArtMethod manipulation. | MIT License. Maintained by @tiann. Requires JitPack repository. |

---

## 3. Computer Vision & Perception Engines

| Solution | Footprint | Frame Processing Latency | Key Strengths & Weaknesses | Recommendation |
| :--- | :--- | :--- | :--- | :--- |
| **OpenCV Android SDK (4.10+)** | $\sim 15\text{ MB}$ ($\sim 4\text{ MB}$ stripped) | **$6 - 12\text{ ms}$** | **Strengths**: Native C++ NEON acceleration, HoughCircles, Homography warp. **Weaknesses**: Sensitive to harsh glare or customized board skins. | **Primary Choice for CV**. Free, Apache 2.0. |
| **TFLite YOLOv8-Nano (INT8)** | $\sim 6\text{ MB}$ | **$12 - 20\text{ ms}$** | **Strengths**: Invariant to skins, shadows, and reflections. **Weaknesses**: Requires training dataset (~500 annotated frames). | **Secondary / Fallback**. Apache 2.0. |
| **BoofCV** | $\sim 3\text{ MB}$ | $30 - 55\text{ ms}$ | **Strengths**: Pure Java, zero NDK setup. **Weaknesses**: High GC pressure, slow execution on mobile CPUs. | **Not Recommended**. |

---

## 4. 2D Physics Simulation Libraries

| Library | Binary Size | Integration Complexity | Performance & Match Fidelity | Recommendation |
| :--- | :--- | :--- | :--- | :--- |
| **CatEngine V2 (`PhysicsEngineV2.cpp`)** | **~45 KB** | **Zero (Already integrated)** | **$< 2\text{ ms}$ per 500 steps**. Exactly calibrated for Carrom Pool mass ratios (3:1), pocket drop velocity thresholds, and cushion restitution. | **Selected Primary Engine**. Retain and optimize. |
| **Box2D (v2.4 / v3.0)** | ~400 KB | High (Requires rewriting contact solvers) | $\sim 10\text{ ms}$. Complex rigid-body solver with unwanted constraint solver overhead. | Not recommended for in-process carrom prediction. |
| **Chipmunk2D** | ~180 KB | Moderate (C99 wrapper) | $\sim 5\text{ ms}$. Clean C solver, but requires significant tuning to match Miniclip pocket suction dynamics. | Viable alternative for external tools. |

---

## 5. Screen Capture & Rendering Tooling

| Capability | Technical Mechanism | Latency / Overhead | Security / Detection Footprint |
| :--- | :--- | :--- | :--- |
| **In-Game OpenGL ES Overlay** | Hook `eglSwapBuffers` in GLES driver via Dobby | **$< 0.5\text{ ms}$** (Executes directly in Cocos2d-x render loop) | Internal to process. Zero external overlay windows. |
| **DecorView Window Overlay** | Injected into `activity.getWindow().getDecorView()` | Negligible | Internal to Activity window. No `SYSTEM_ALERT_WINDOW` needed. |
| **Native Framebuffer Capture** | `glReadPixels()` inside `eglSwapBuffers` hook | **$3 - 6\text{ ms}$** | Internal to process. Zero permission prompts. |
| **Android MediaProjection** | Android `VirtualDisplay` + `ImageReader` | $15 - 25\text{ ms}$ | External service. Requires user permission prompt. |
| **ADB Frame Streaming** | `scrcpy` / `adb exec-out screencap -p` | $20 - 40\text{ ms}$ | External debugging channel. Requires USB debugging. |

