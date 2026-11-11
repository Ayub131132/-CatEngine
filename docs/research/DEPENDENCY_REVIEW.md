# CatEngine: Dependency Review & Ecosystem Research

**Author:** Senior Android, Rust/C++, and Computer Vision Engineer  
**Target:** CatEngine Native & Android Modules

---

## 1. Existing Dependencies Audit

### 1.1 Android & Java Dependencies

| Dependency | Declared Version | Location | Audit Findings & Compatibility Assessment |
| :--- | :--- | :--- | :--- |
| **Android Gradle Plugin (AGP)** | `8.13.2` | `gradle/libs.versions.toml` | High AGP release. Compatible with Gradle 8.9 - 9.x and Java 17/21. Requires Google Maven repo in `settings.gradle`. |
| **Kotlin Gradle Plugin** | `2.0.21` | `gradle/libs.versions.toml` | Modern Kotlin 2.0 with the K2 compiler enabled. Fully supported. |
| **Compose Compiler Plugin** | `2.0.21` | `gradle/libs.versions.toml` | Integrated via Kotlin 2.0 compose-compiler plugin. |
| **FreeReflection** | `3.2.2` | `Bcore/build.gradle` | Required for hidden API bypass on Android 9 - 14. Hosted on **JitPack** (`com.github.tiann:FreeReflection`). Build fails if JitPack repository is omitted. |
| **Toml4j** | `0.7.2` | `Bcore/build.gradle` | Simple TOML parser for configuration files. Stable, no issues. |
| **Auto-Service** | `1.1.1` | `compiler/build.gradle.kts` | Google service provider generator for annotation processors. Compile-only. |
| **JavaPoet** | `1.13.0` | `compiler/build.gradle.kts` | Source code generation library used by `:compiler`. Stable. |
| **Jetpack Compose BOM** | `2024.04.01` | `app/build.gradle`, `Bcore/build.gradle` | Material3, Foundation, UI, and Icons. Modern UI framework used for host app and in-game calibration dialogs. |
| **Retrofit & Gson** | `2.11.0` | `app/build.gradle` | HTTP client stack for network requests and remote offset synchronization. |
| **AAR Binaries** | Bundled | `app/libs/` | `catloading-release.aar`, `floatingview-release.aar`, `StateView-v3.0.6.aar`. Legacy UI components. |

### 1.2 Native C/C++ Libraries

| Native Library | Source / Location | Purpose | Licensing & Risk Analysis |
| :--- | :--- | :--- | :--- |
| **Dobby** | `Bcore/src/main/cpp/Dobby/` (prebuilt `libdobby.a` for arm64-v8a & armeabi-v7a) | Lightweight multi-platform inline hook framework. Used to hook game functions and `eglSwapBuffers`. | Permissive license (Apache 2.0). ARM64 trampolines must be verified for 16KB page alignment compatibility on Android 15. |
| **xDL** | `Bcore/src/main/cpp/xdl/` (compiled from C source) | Dynamic linker symbol locator and ELF iterator. Bypasses Android `dlopen` restrictions. | MIT License. Highly reliable across Android 5.0 to 14+. |
| **OpenGL ES & EGL** | System (`-lEGL -lGLESv2`) | Render pipeline for prediction lines and table markers. | Standard Android platform library. |

---

## 2. Research on Physics Simulation Libraries

Currently, CatEngine uses a custom 2D forward-Euler integration engine (`catengine::v2::PhysicsEngineV2`). The table below compares the custom implementation with leading 2D physics engines:

| Criteria | Existing Custom Engine (`PhysicsEngineV2.cpp`) | Box2D (v2.4 / v3.0) (C++) | Chipmunk2D (C) |
| :--- | :--- | :--- | :--- |
| **Integration Overhead** | **Zero** (Already implemented and calibrated) | High (Requires rewriting rigid body world and collision filtering) | Moderate (Requires C wrapper and struct mapping) |
| **Binary Footprint** | **~45 KB** | ~350 - 500 KB | ~150 - 200 KB |
| **Simulation Speed (Forward Rays)** | Extremely Fast (~1 - 3 ms for 50-step lookahead) | Moderate (~8 - 15 ms, heavier solver iteration) | Fast (~4 - 8 ms) |
| **Game-Specific Mechanics** | **Exact match**: Specifically calibrated for Carrom Pool mass ratio (3:1), pocket drop thresholds, and cushion restitution. | Generic rigid-body impulses; requires complex tuning to replicate Miniclip's exact physics. | Generic rigid-body impulses; requires custom torque and pocket suction modeling. |
| **Deterministic Substepping** | Fully deterministic forward integration. | Fully deterministic fixed-step world. | Fully deterministic fixed-step world. |
| **Recommendation** | **Retain and Optimize Existing Custom Engine**. Box2D/Chipmunk introduce unnecessary bloat without improving accuracy. | Not recommended for in-process carrom prediction. | Alternative for standalone external simulator. |

---

## 3. Research on Computer Vision (CV) Libraries

While CatEngine currently operates purely via in-process memory introspection, future standalone test automation, external validation, or non-sandboxed environments require a visual perception pipeline.

| Solution | Model / Library | Latency (Android ARM64) | Accuracy / Robustness | Integration Complexity |
| :--- | :--- | :--- | :--- | :--- |
| **OpenCV Android SDK (v4.10+)** | Classical CV: Hough Circles, Perspective Transform, HSV Color Masking | **5 - 12 ms / frame** (CPU / NEON) | Moderate. Sensitive to table glare, dynamic shadows, and differing board skin textures. | Low. Prebuilt native shared libraries (`libopencv_java4.so`). |
| **YOLOv8-Nano / YOLOv11-Nano (INT8)** | Deep Learning Object Detection (TFLite / ONNX Runtime) | **10 - 18 ms / frame** (NNAPI / GPU Delegate) | **High**. Invariant to table skins, lighting, board rotations, and player avatars. | Moderate. Requires small training dataset (~500 annotated carrom screenshots). |
| **BoofCV (Java/Kotlin)** | Pure Java/Android CV | 25 - 45 ms / frame | Lower. Lacks NEON-optimized native acceleration. | Very Low (single Gradle dependency). |

### Recommended Hybrid CV Architecture (for external test harness):
1. **Geometry Stage (OpenCV)**: Extract 4 table corner pockets using Hough Lines / Contours to compute the Homography matrix (`cv::getPerspectiveTransform` and `cv::warpPerspective`), rectifying the angled perspective into a standard $76\text{ cm} \times 76\text{ cm}$ orthogonal board.
2. **Disc Detection Stage (OpenCV + Color Segmentation)**: Filter pucks and striker using LAB/HSV color space masks with circle contour fitting.
3. **ML Fallback (TFLite YOLO-Nano)**: Triggered only when occlusion or board skins cause classical CV detection confidence to drop below 85%.

---

## 4. Missing Dependencies to Add to the Build

To resolve build errors and prepare for automated testing, the following declarations must be added:

1. **Root `settings.gradle`**:
   - `google()`, `mavenCentral()`, `gradlePluginPortal()`, and `https://jitpack.io`.
   - `include ':app', ':Bcore', ':black-reflection', ':compiler'`.
2. **NDK Version Alignment**:
   - Align `ndkVersion` in `build.gradle` to match installed NDK (`28.2.13676358` or `27.1.12297006`) to prevent forced multi-gigabyte downloads.
3. **OpenCV (Phase 3 Prep)**:
   - OpenCV 4.10+ Android C++ SDK headers and prebuilt binaries if screen-capture perception is enabled.

