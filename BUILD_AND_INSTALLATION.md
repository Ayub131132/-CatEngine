# CatEngine: Build & Installation Manual

**Target Platforms:** Android 8.0 - 15.0+ (API 26 - 35)  
**Supported Architectures:** ARM64-v8a, ARMeabi-v7a  
**Development Host:** Windows 11 (PowerShell / CMD), Linux, macOS  

---

## 1. Environment Prerequisites

Ensure the following SDKs and toolchains are installed:

| Tool | Recommended Version | Path Example (Windows) |
| :--- | :--- | :--- |
| **Java JDK** | OpenJDK 17 or Azul Zulu 17 | `C:\Program Files\Java\jdk-17` |
| **Android SDK** | Platforms 34 / 35, Build-Tools `35.0.0` | `C:\AndroidSDK` |
| **Android NDK** | `28.2.13676358` | `C:\AndroidSDK\ndk\28.2.13676358` |
| **Gradle** | `8.13.2` (via bundled `gradlew.bat`) | In-tree wrapper |
| **ADB** | Android Platform Tools `35.0.0+` | `C:\AndroidSDK\platform-tools` |

Set your `ANDROID_HOME` environment variable:
```powershell
[System.Environment]::SetEnvironmentVariable("ANDROID_HOME", "C:\AndroidSDK", "User")
```
Ensure `local.properties` contains your SDK directory:
```properties
sdk.dir=C\:\\AndroidSDK
```

---

## 2. Compilation Instructions

### 2.1 Build Reflection Preprocessors
```powershell
.\gradlew.bat :black-reflection:assemble :compiler:assemble
```

### 2.2 Build Virtualization Core & Native Engine (`libengine.so`)
```powershell
.\gradlew.bat :Bcore:assembleDebug
```
*Note: This runs `ndkBuild` with Android 16KB page-size alignment (`-z,max-page-size=16384`).*

### 2.3 Build Complete Android Application APK
```powershell
.\gradlew.bat :app:assembleDebug
```
The output debug APK will be created at:
```
app\build\outputs\apk\debug\CatEngine_1.0.0_debug.apk
```

---

## 3. Deployment & Device Installation

### 3.1 Verify Connected Hardware
```powershell
adb devices -l
```
Ensure your target device appears as `device` (authorized).

### 3.2 Install CatEngine APK
```powershell
adb install -r -d "app\build\outputs\apk\debug\CatEngine_1.0.0_debug.apk"
```

### 3.3 Launch Application
```powershell
adb shell am start -n com.engine.cat/top.niunaijun.blackboxa.view.main.MainActivity
```

### 3.4 Grant Virtual Process Permissions
CatEngine requires Overlay (`SYSTEM_ALERT_WINDOW`) permission to draw the calibration HUD:
```powershell
adb shell pm grant com.engine.cat android.permission.SYSTEM_ALERT_WINDOW
```

---

## 4. In-Process Virtual Game Setup

1. Open **CatEngine** on your Android phone.
2. Tap the `+` button on the home screen.
3. Select **Carrom Pool** (`com.miniclip.carrom`) from your installed apps list.
4. BlackBox will clone the APK resources into the sandbox storage.
5. Tap the Carrom Pool icon in CatEngine to launch it inside the virtual container (`:p0`).
6. Observe CatEngine native logs in logcat:
   ```powershell
   adb logcat -s CatEngine:I CatEngine-V2:I
   ```
