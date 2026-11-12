# KEngine V2.1 (`KEngineV2.1_sign.apk`) — Complete Reverse Engineering Dossier

> **Decompiled & Reconstructed Artifact Analysis**  
> Generated automatically via Antigravity Reverse Engineering Engine (REA)

---

## 📌 Executive Summary

| Attribute | Value |
|:---|:---|
| **Package Name** | `com.kos` |
| **Version Name** | `2.1-VStable` |
| **Version Code** | `12` |
| **Minimum SDK** | Android 7.0 (API 24) |
| **Target SDK** | Android 15 / 16 (API 36) |
| **Architectures** | `arm64-v8a` (64-bit only) |
| **Original File Size** | 11,025,399 bytes (~10.5 MB) |
| **Primary Native Library** | `libkos.so` (6,425,080 bytes) |
| **Virtualization Core** | Rebranded & Hardened **BlackBox Engine** (`com.kos.engine`) |
| **Native Bridge** | `com.kos.Native.NativeBridge` via `doTask([Object])[Object]` |

---

## 📁 Repository & Folder Organization

This dedicated folder contains the full extraction and reverse-engineering artifacts:

```
KEngine_Reverse_Engineering/
├── KEngineV2.1_sign.apk              # Original signed APK
├── extracted_apk/                     # Full unpacked APK contents
│   ├── AndroidManifest.xml            # Binary manifest
│   ├── classes.dex                    # 4.5 MB Dalvik bytecode (6,104 classes)
│   ├── lib/arm64-v8a/                 # Native shared libraries
│   │   ├── libkos.so                  # Core injection & virtualization engine
│   │   └── libandroidx.graphics.path.so
│   ├── res/                           # Compiled UI resources
│   ├── assets/                        # Baseline runtime profiles
│   └── META-INF/                      # Signing certificates
├── so_libraries/                      # Dedicated Native SO Section
│   ├── arm64-v8a/
│   │   ├── libkos.so                  # Primary native module (6.4 MB)
│   │   └── libandroidx.graphics.path.so
│   └── analysis/
│       ├── libkos_dynamic_symbols.txt # 551 dynamic symbols
│       ├── libkos_dependencies.txt    # DT_NEEDED shared dependencies
│       └── libkos_hashes.txt          # Cryptographic digests
├── dex_analysis/
│   └── all_classes.txt                # 6,104 extracted Java/Kotlin class descriptors
└── documentation/
    ├── README.md                      # This main architectural report
    ├── ARCHITECTURE.md                # System-level subsystem & execution architecture
    ├── NATIVE_ENGINE_ANALYSIS.md      # Deep dive into libkos.so & dispatch mechanism
    ├── MANIFEST_AND_PERMISSIONS.md    # AndroidManifest decoding & security permissions
    └── REBUILD_GUIDE.md               # Step-by-step rebuild and reconstruction guide
```

---

## 🔍 Key Findings

1. **Origin & Core Heritage**:
   `KEngine` is a custom-branded and armored build of **BlackBox** (the Android sandbox/virtualization container originally by `top.niunaijun.blackbox`). The internal packages have been systematically refactored to `com.kos.engine`.

2. **Native Module (`libkos.so`)**:
   - Compiles both the BlackBox IO redirection virtualization core (`NativeCore`) and the custom game injection / guideline graphics logic into a single monolithic 6.4 MB shared library.
   - Symbols are stripped and partially obfuscated (`A00000001xx`, `B0000001dxx`, etc.) with OLLVM-style control-flow flattening.
   - Dynamic dispatch occurs through a unified JNI hook: `com.kos.Native.NativeBridge.doTask([Ljava/lang/Object;)[Ljava/lang/Object;`.

3. **Protection & Anti-Analysis**:
   - Critical string constants in Java are encrypted using dynamic 64-bit constant decoders (`Landroidx/emoji2/text/sp0;.L:(J)Ljava/lang/String;`).
   - `System.loadLibrary("kos")` is invoked dynamically at runtime with deciphered string values.
