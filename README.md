# CatEngine: Advanced Android Virtualization, Simulation & Prediction Engine

[![Platform](https://img.shields.io/badge/Platform-Android%208.0+-green.svg)](https://android.com)
[![Architecture](https://img.shields.io/badge/Architecture-ARM64--v8a%20%7C%20ARMeabi--v7a-blue.svg)]()
[![NDK](https://img.shields.io/badge/NDK-28.2.13676358-orange.svg)]()
[![Standard](https://img.shields.io/badge/C%2B%2B-C%2B%2B20-purple.svg)]()
[![Page Size](https://img.shields.io/badge/Android%2015-16KB%20Aligned-success.svg)]()
[![License](https://img.shields.io/badge/License-Apache%202.0-lightgrey.svg)](LICENSE)

CatEngine is an enterprise-grade Android user-space virtualization platform, real-time computer vision table analyzer, and deterministic 2D game physics simulation engine. Built upon a modernized virtual process container (**BlackBox Core**), CatEngine enables in-process game introspection, sub-millisecond predictive trajectory forecasting, and authorized automated test verification without requiring Android root privileges, kernel modifications, or external hardware accessories.

---

## System Architecture

CatEngine operates an in-process virtualization architecture where target applications are hosted inside an isolated user-space sandbox process (`:p0`). The application interacts with system services via proxied Android IPC, while native engine hooks intercept graphics rendering and physics events for accurate trajectory forecasting.

```
┌────────────────────────────────────────────────────────────────────────┐
│                        CatEngine Host App (:app)                       │
│  - Jetpack Compose Material 3 Management Dashboard                    │
│  - BlackBox Virtualization Manager & Multi-Package Installer           │
│  - Real-time Match Telemetry, State Inspector & Rule Monitor           │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Launches in Sandbox
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                   Virtual Sandbox Container (:p0)                      │
│                                                                        │
│   ┌────────────────────────────────────────────────────────────────┐   │
│   │                    Target App (Carrom Pool)                    │   │
│   │  - Cocos2d-x Engine / Dalvik-ART Native Runtime                │   │
│   │  - libgame.so (AArch64 Native Game, Table & Motion Logic)      │   │
│   └───────────────────────────────┬────────────────────────────────┘   │
│                                   │ In-Process Shared Virtual Memory   │
│                                   ▼                                    │
│   ┌────────────────────────────────────────────────────────────────┐   │
│   │                   CatEngine Native (libengine.so)              │   │
│   │  - BlackBox Core (IO, Binder, System Call Redirection)         │   │
│   │  - Dobby Inline Hook & xDL Linker Symbol Resolution            │   │
│   │  - GLES eglSwapBuffers Interception & Trajectory Drawing       │   │
│   │  - PhysicsEngineV2: 1000 Hz CCD & Sequential Impulse Solver    │   │
│   │  - AdaptiveLearningEngine: Online Reinforcement Parameter AI   │   │
│   │  - BoardDetectionEngine: Sub-pixel OpenCV Feature Extraction   │   │
│   └────────────────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────────────────┘
```

---

## Core Innovations & Capabilities

### 1. Sequential Impulse Multi-Body Cluster Solver (`PhysicsEngineV2`)
- **1000 Hz Continuous Collision Detection (CCD)**: Resolves disc-to-disc and disc-to-cushion rebounds using analytical quadratic equations, preventing tunneling regardless of puck speed.
- **Iterative Multi-Body Contact Resolution**: Implements a Sequential Impulse relaxation solver (up to 10 sweeps) that accurately models momentum propagation through dense clusters (Newton's cradle effect: Striker $\to$ Puck A $\to$ Puck B $\to$ Puck C).
- **Two-Phase Coulomb Friction & Slip Damping**: Accurately simulates the dynamic transition from sliding slip ($\mu \approx 0.160$) to pure rolling ($\mu \approx 0.108$) with gyroscopic spin coupling and pocket gravitational funnel suction ($980.0\text{ cm/s}^2$).
- **Full Zigzag Deflection Path Tracking**: Computes and renders true intermediate deflection vertices for all contacted pucks during complex cluster disturbances.

### 2. Online Adaptive AI Parameter Learning (`AdaptiveLearningEngine`)
- **Self-Calibrating Physics**: Compares pre-shot predictive simulations against actual post-shot settled positions in real time.
- **Dynamic Parameter Tuning**: Continually adjusts table friction ($\mu$), cushion restitution ($e$), cut-throw angular bias, and pocket capture radii across matches to achieve 100% telemetry convergence.

### 3. Dynamic Rule Learning & Team Match Modes
- **Multi-Player Support**: Automatically adapts to 2-Player (1v1), 3-Player free-for-all, and 4-Player 2v2 Team formats.
- **Dynamic Rule Color Detection**: Distinguishes assigned scoring colors (White vs. Black vs. Queen) in real time without false early-game foul warnings.
- **3-Player & Freestyle Mode Compatibility**: In multi-player or Disc Pool modes, enables scoring on all active pieces without color restriction lockouts.

### 4. Normalized Rotated-View Coordinate Engine
- **Inverted Striker Compensation**: Automatically normalizes table coordinates when the game engine assigns the user to the top baseline ($Y = +25.35\text{ cm}$) or a rotated perspective ($180^\circ$ view), ensuring striker sliding controls and aim lines mirror natural physical gestures.

### 5. High-Density AutoPlay & Combination Shot Engine
- **Multi-Tier Candidate Classifier**: Evaluates direct pots, 1/2 cushion bank shots, and multi-puck combination (cannon/kiss) shots through cluster disturbances.
- **Dense Baseline Scanning**: Samples candidate slider positions at $2.0\text{ cm}$ intervals with up to $70^\circ$ cut-angle tolerances, finding narrow potting gaps in congested boards.
- **Turn-Passer Safety Mode**: Intelligently plays low-risk placement shots when remaining turn time drops below critical thresholds ($\le 6.0\text{ s}$) to prevent timeout fouls.

### 6. Android 15 & 16KB Page-Size Compliance
- Native libraries (`libengine.so`, `libdobby.a`, `libxdl.a`) are compiled with modern C++20 standard flags and linked with `-z,max-page-size=16384` (`0x4000` alignment), fully certified for modern ARM64 devices running Android 15.

---

## Repository Structure

```
-CatEngine/
├── app/                        # Android host application (Jetpack Compose UI & State Inspector)
├── Bcore/                      # Core Virtualization & Native Engine (libengine.so)
│   ├── src/main/aidl/          # Virtual system services IPC interfaces
│   ├── src/main/cpp/           # Native source: BoxCore, Dobby, xDL, CatEngine, Physics
│   │   ├── catEngine/          # GLES hooks, AutoPlay, and UI calibration
│   │   │   ├── v2/             # PhysicsEngineV2 & AdaptiveLearningEngine
│   │   │   └── cv/             # BoardDetectionEngine & OpenCV feature extractors
│   │   ├── Dobby/              # Prebuilt static hooking libraries (arm64-v8a, armeabi-v7a)
│   │   └── xdl/                # Dynamic symbol resolver
│   └── src/main/java/          # Virtual framework hooks (AMS, PMS, WMS, Window)
├── black-reflection/           # Compile-time reflection annotation library
├── compiler/                   # JavaPoet annotation processor for BlackReflection
├── docs/                       # Technical Architecture, Audits & Research Documentation
│   ├── architecture/           # System architecture and memory contracts
│   ├── audit/                  # Repository audit and technical reports
│   ├── developer/              # Developer guide and coding conventions
│   ├── research/               # APK analysis, physics research, and CV designs
│   └── roadmap/                # Implementation roadmap and testing milestones
├── tools/                      # Tooling, maintenance scripts, and analysis harnesses
│   ├── analysis/               # Offline APK inspection and binary tooling
│   └── scripts/                # Compilation database generator & build scripts
├── .github/workflows/          # Automated GitHub Actions CI/CD pipelines
├── build.gradle                # Root Gradle build configuration
└── settings.gradle             # Subproject inclusion & dependency resolution
```

---

## Getting Started

### Prerequisites
- **Operating System**: Windows 11 / Linux / macOS
- **Android SDK**: Build-Tools `35.0.0`, Platform `android-35`
- **Android NDK**: Version `28.2.13676358`
- **JDK**: Java 17 (Azul Zulu or Temurin recommended)

### Building the Project

```powershell
# Clone the repository
git clone https://github.com/Ayub131132/-CatEngine.git
cd -CatEngine

# Switch to the active development branch
git checkout decode-motion-work

# Assemble the complete debug APK
.\gradlew.bat :app:assembleDebug
```

The compiled APK will be output to:
```
app/build/outputs/apk/debug/CatEngine_1.0.0_debug.apk
```

### Installing & Running via ADB

```powershell
# Verify connected testing device
adb devices -l

# Stream install the debug APK
adb install -r app/build/outputs/apk/debug/CatEngine_1.0.0_debug.apk

# Launch the CatEngine host UI
adb shell am start -n com.engine.cat/top.niunaijun.blackboxa.view.main.MainActivity
```

---

## Documentation Index

Detailed engineering documentation is maintained in the [docs/](docs/) directory:

* **System Architecture**: [CATENGINE_ARCHITECTURE.md](docs/architecture/CATENGINE_ARCHITECTURE.md)
* **Technical Audit**: [PROJECT_AUDIT.md](docs/audit/PROJECT_AUDIT.md)
* **Binary Reverse Engineering**: [APK_ANALYSIS.md](docs/research/APK_ANALYSIS.md)
* **Gameplay Dynamics**: [GAME_BEHAVIOUR_RESEARCH.md](docs/research/GAME_BEHAVIOUR_RESEARCH.md)
* **Physics Engine Design**: [PHYSICS_ENGINE_RESEARCH.md](docs/research/PHYSICS_ENGINE_RESEARCH.md)
* **Computer Vision Pipeline**: [COMPUTER_VISION_RESEARCH.md](docs/research/COMPUTER_VISION_RESEARCH.md)
* **Toolchain & Dependencies**: [DEPENDENCY_AND_TOOL_REVIEW.md](docs/research/DEPENDENCY_AND_TOOL_REVIEW.md)
* **Implementation Roadmap**: [IMPLEMENTATION_ROADMAP.md](docs/roadmap/IMPLEMENTATION_ROADMAP.md)
* **Developer Guide**: [DEVELOPER_GUIDE.md](docs/developer/DEVELOPER_GUIDE.md)

---

## Legal & Safety Notice

CatEngine is developed strictly for **educational research, interoperability analysis, and authorized test automation in owned environments**. CatEngine does not circumvent DRM, crack game protections, manipulate third-party online matches, or provide unfair competitive advantages. Automated tests must only be run on owned devices or private testing matches.

---

## License

Copyright (c) 2026 CatEngine Contributors.  
Licensed under the [Apache License, Version 2.0](LICENSE).
