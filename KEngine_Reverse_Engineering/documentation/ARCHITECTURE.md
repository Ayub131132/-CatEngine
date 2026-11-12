# KEngine V2.1 — System Architecture Deep Dive

## 1. System Overview & Component Diagram

```
+--------------------------------------------------------------------------+
|                            Android OS Runtime                            |
+--------------------------------------------------------------------------+
                                     |
                                     v
+--------------------------------------------------------------------------+
|                         Host App: com.kos (KOS)                          |
|                                                                          |
|  [MainActivity] ----> [ComposeAuthActivity] ----> [BlackBoxSystem]       |
|          |                                                |              |
|          v                                                v              |
|  [NativeBridge]                                   [ServiceManager]       |
|    - a(String)                                      - BProcessManager    |
|    - b(Application)                                 - BAccountManager    |
|    - doTask([Object])[Object]                       - BActivityManager   |
+--------------------------------------------------------------------------+
                                     |
                                     | JNI: JNI_OnLoad + NativeCore::init
                                     v
+--------------------------------------------------------------------------+
|                     Monolithic Binary: libkos.so                         |
|                                                                          |
|  +-------------------------+     +------------------------------------+  |
|  | BlackBox Virtualization |     |     CatEngine / Guideline Engine   |  |
|  |  - IO Path Redirection  |     |  - EGL Hook (eglSwapBuffers)       |  |
|  |  - UID / GID Spoofing   |     |  - libgame.so Memory Scanning      |  |
|  |  - Binder Proxying      |     |  - Physics Simulation (1000Hz CCD)|  |
|  |  - Fake System Call Map |     |  - ImGui / GL Overlay Rendering    |  |
|  +-------------------------+     +------------------------------------+  |
+--------------------------------------------------------------------------+
                                     |
                   Spawns Virtual Sandboxed Process (:p0)
                                     v
+--------------------------------------------------------------------------+
|                       Target Game: Carrom Pool                           |
|                                                                          |
|  - Injected libkos.so hooks TableLogic & Graphics Pipeline               |
|  - Striker & Puck physics captured live from memory (0x1998bc0, etc.)    |
|  - Real-time trajectory prediction rendered over native EGL surface       |
+--------------------------------------------------------------------------+
```

---

## 2. Java Package Hierarchy

The APK contains **6,104 classes**. The breakdown by subsystem is:

| Subsystem | Java Package | Purpose |
|:---|:---|:---|
| **Entry & UI** | `com.kos.*` | Launcher, Auth, MainActivity, UI widgets |
| **Native Glue** | `com.kos.Native.*` | `NativeBridge` JNI dispatcher |
| **Virtual Sandbox Core** | `com.kos.engine.core.*` | BlackBox virtualization engine |
| **Process Management** | `com.kos.engine.core.system.*` | Virtual process isolation (`:p0` - `:p7`) |
| **Environment** | `com.kos.engine.core.env.*` | Fake filesystems, app data isolation |
| **Obfuscated UI / Helper**| `androidx.emoji2.text.*` | Repackaged helper classes & string decryptors |
| **Network & Comms** | `okhttp3.*`, `retrofit2.*` | License validation, cloud telemetry |

---

## 3. Sandboxed Multi-Process Model

The app registers multiple virtual isolation processes in `AndroidManifest.xml`:

- `:daemon`: Background supervisor service (`com.kos.engine.core.system.DaemonService`)
- `:p0` through `:p7`: Sandboxed guest processes (`com.kos.engine.app.dispatcher.AppServiceDispatcher$P0`, etc.)
- When the target game (Carrom Pool) is launched, it runs isolated inside process `:p0`.
- `libkos.so` is loaded into `:p0` to hook runtime memory without requiring root access on the host device.
