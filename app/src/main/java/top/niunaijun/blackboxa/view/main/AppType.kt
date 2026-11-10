package top.niunaijun.blackboxa.view.main

enum class AppStateType {
    LOADING,
    NOT_INSTALLED,      // Package does not exist on device
    VERSION_MISMATCH,   // Package exists but version does not match server version
    READY_TO_LAUNCH,    // Both package and version match and exist on device
    ERROR               // Network or general error
}

data class AppType(
    val packageName: String = "",
    val serverVersion: String = "",
    val localVersion: String = "",
    val appName: String = "",
    val message: String = "",
    val stateType: AppStateType = AppStateType.LOADING
)
