# Native Engine Analysis (`libkos.so`)

## 1. Binary Metadata

| Property | Value |
|:---|:---|
| **Filename** | `libkos.so` |
| **Architecture** | `AArch64` (ARM 64-bit, Little-Endian) |
| **File Size** | 6,425,080 bytes (~6.42 MB) |
| **MD5 Digest** | `27104f504fc32ba1942ef618c06fc285` |
| **SHA-256 Digest** | `68631a1f0d862001ddd9cd5d1710a7d609b6e3bb0319c63c1e92821ef506c1a9` |
| **SONAME** | `libkos.so` |
| **Dynamic Linker (`NEEDED`)** | `liblog.so`, `libz.so`, `libstdc++.so`, `libm.so`, `libc.so`, `libdl.so` |

---

## 2. Dynamic Linking & Dependencies

`libkos.so` links against standard Android runtime shared libraries:
- `liblog.so`: Android logcat output (`__android_log_print`)
- `libz.so`: Compression / decompression (APK unpacking / resource inflation)
- `libdl.so`: Dynamic symbol resolution (`dlopen`, `dlsym`, `dladdr`)
- `libc.so` / `libm.so`: Standard C & math runtime
- `libstdc++.so`: C++20 standard library

---

## 3. Unified JNI Dispatch Protocol (`doTask`)

Instead of exposing dozens of traditional `Java_com_kos_...` exported symbols, `libkos.so` uses **dynamic JNI registration (`RegisterNatives`)** inside `JNI_OnLoad`.

The primary bridge interface is:
```java
package com.kos.Native;

public abstract class NativeBridge {
    // Central JNI multiplexer
    private static final native Object[] doTask(Object[] args);
    
    // Command wrappers:
    public static void a(String payload); // Command 1003
    public static qv1 b(Application app); // Command initialization
    public static ArrayList c(JSONArray arr);
    public static ih0[] d(JSONArray arr);
    public static String e(Context ctx, String key);
}
```

### Command Multiplexing Table:
- `args[0]` contains an `Integer` command ID (e.g. `1003`, `1001`, `2001`, etc.)
- Remaining elements `args[1..n]` carry arguments (Strings, Contexts, Arrays, or byte buffers).
- The return value is an `Object[]` containing result statuses, extracted memory pointers, or parsed state.

---

## 4. Reverse-Engineered Functional Subsystems Inside `libkos.so`

Through static disassembly and binary pattern matching, `libkos.so` merges two primary engines:

### Subsystem A: Virtualization & Syscall Redirection (BlackBox Core)
- `ioctl`, `openat`, `fstatat`, `readlinkat` redirection
- Path mapping from `/data/data/<pkg>` to `/data/data/com.kos/virtual/users/0/<pkg>`
- UID/GID virtualization so the game believes it runs under its own package identity.

### Subsystem B: Graphics & Memory Hook Engine (CatEngine Core)
- **Hook Framework**: Embedded Dobby inline hooker (`DobbyHook`, `DobbySymbolResolver`).
- **Hook Targets**:
  - `eglSwapBuffers` in `libEGL.so` to render guidance lines frame-by-frame.
  - Telemetry interception points in `libgame.so`.
- **Memory Scanning**:
  - Automatically parses `/proc/self/maps` to find `libgame.so`.
  - Reads float constants at pre-calibrated offsets for table physics (restitution, friction, mass ratios).
