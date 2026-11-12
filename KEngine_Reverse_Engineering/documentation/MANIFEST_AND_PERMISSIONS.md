# Android Manifest & Security Permissions Analysis

## Package Identifiers
- **Package ID**: `com.kos`
- **Version**: `2.1-VStable` (Code: `12`)
- **Compile SDK**: `36`
- **Min SDK**: `24`

---

## Declared Permissions (173 Total)

| Category | Permission Name | Security Impact |
|:---|:---|:---|
| **Network** | `android.permission.INTERNET` | Communication with auth & update servers |
| **Network** | `android.permission.ACCESS_NETWORK_STATE` | Check connectivity state |
| **System** | `android.permission.QUERY_ALL_PACKAGES` | Inspect installed games on the device |
| **System** | `android.permission.WAKE_LOCK` | Prevent CPU sleep during active matches |
| **Foreground** | `android.permission.FOREGROUND_SERVICE_DATA_SYNC` | Run overlay engine in background |
| **Foreground** | `android.permission.FOREGROUND_SERVICE_SPECIAL_USE` | Elevated background privileges |
| **System / Display** | `android.permission.DETECT_SCREEN_CAPTURE` | Anti-screenshot & detection protection |
| **Storage / Sync** | `android.permission.READ_USER_DICTIONARY` | Virtualization sandbox compatibility |
| **System Account** | `android.permission.AUTHENTICATE_ACCOUNTS` | Virtual Google/FB account login support |

---

## Declared Components Summary

- **Total Activities**: 158
- **Total Services**: 102
- **Total Content Providers**: 53
- **Total Broadcast Receivers**: 2

### Key Components:
1. `com.kos.MainActivity`: Primary user entrance and game selection launcher.
2. `com.kos.ComposeAuthActivity`: Authentication and key activation screen (Jetpack Compose).
3. `com.kos.engine.core.system.DaemonService`: Persistence daemon ensuring virtual processes stay alive.
4. `com.kos.engine.core.system.SystemCallProvider`: IPC broker facilitating cross-process communication between host and virtual app.
