package top.niunaijun.blackboxa.view.apps

import android.content.Context
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.os.Build
import android.util.Log
import androidx.compose.runtime.State
import androidx.compose.runtime.mutableStateOf
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import top.niunaijun.blackbox.BlackBoxCore
import top.niunaijun.blackbox.core.NativeCore
import top.niunaijun.blackboxa.api.RetrofitInstance
import top.niunaijun.blackboxa.api.ServerResponse
import top.niunaijun.blackboxa.bean.AppInfo
import top.niunaijun.blackboxa.data.AppsRepository
import top.niunaijun.blackboxa.util.DeviceManager
import top.niunaijun.blackboxa.util.LicenseManager
import top.niunaijun.blackboxa.util.LicenseState
import top.niunaijun.blackboxa.view.base.BaseViewModel
import top.niunaijun.blackboxa.view.main.AppStateType
import top.niunaijun.blackboxa.view.main.AppType
import com.google.gson.Gson

class AppsViewModel(private val repo: AppsRepository) : BaseViewModel() {

    val appsLiveData = MutableLiveData<List<AppInfo>>()
    val resultLiveData = MutableLiveData<String>()
    val launchLiveData = MutableLiveData<Boolean>()

    // Single active target app status according to server configuration
    private val _appTypeState = mutableStateOf(AppType(stateType = AppStateType.LOADING))
    val appTypeState: State<AppType> = _appTypeState

    val serverApps = mutableStateOf(emptyList<ServerResponse>())
    val isLaunching = mutableStateOf(false)
    val launchingPackage = mutableStateOf<String?>(null)
    val licenseState = mutableStateOf(LicenseState())
    val isServerOnline = mutableStateOf(false)
    val isCheckingServer = mutableStateOf(false)

    fun initLicense(context: Context) {
        licenseState.value = LicenseManager.getLicenseState(context)
        viewModelScope.launch {
            val updated = LicenseManager.syncWithServer(context)
            licenseState.value = updated
        }
    }

    fun checkServerHealth(onResult: ((Boolean) -> Unit)? = null) {
        isServerOnline.value = true
        onResult?.invoke(true)
    }

    fun redeemLicense(context: Context, key: String, onResult: (Boolean, String) -> Unit) {
        viewModelScope.launch {
            val (success, message) = LicenseManager.redeemKey(context, key)
            if (success) {
                licenseState.value = LicenseManager.getLicenseState(context)
            }
            onResult(success, message)
        }
    }

    /**
     * Fetches package configurations from the backend server.
     * Takes the single target app and verifies against local device installation.
     */
    fun fetchAndVerifyApp(context: Context) {
        initLicense(context)

        // Fast-path: Immediately verify default target "com.miniclip.carrom" locally
        // so UI and launch are instant and not blocked by network latency or timeouts.
        verifyDevicePackage(context, "com.miniclip.carrom", "19.6.0")
        prewarmPackage("com.miniclip.eightballpool")

        // Sync Carrom offsets (either from server, local cache, or hardcoded fallback)
        syncAppOffsets(context, "com.miniclip.carrom", "1491")

        viewModelScope.launch {
            isCheckingServer.value = true
            try {
                val response = RetrofitInstance.api.getApps()
                if (response.isNotEmpty()) {
                    isServerOnline.value = true
                    serverApps.value = response
                    val target = response.first()
                    verifyDevicePackage(context, target.packageName, target.versionCode)
                    syncAppOffsets(context, target.packageName, target.versionCode)
                } else {
                    isServerOnline.value = false
                }
            } catch (e: Exception) {
                if (e is retrofit2.HttpException && e.code() == 429) {
                    isServerOnline.value = true
                } else {
                    Log.w("AppsViewModel", "Server check offline/unreachable: ${e.message}")
                    isServerOnline.value = false
                }
                if (_appTypeState.value.stateType == AppStateType.LOADING) {
                    verifyDevicePackage(context, "com.miniclip.carrom", "19.6.0")
                }
                syncAppOffsets(context, "com.miniclip.carrom", "1491")
            } finally {
                isCheckingServer.value = false
            }
        }
    }

    /**
     * Fetches remote dynamic offsets for a package from server and passes to NativeCore.
     * If server is unreachable or offline, automatically falls back to locally cached offsets
     * or hardcoded native C++ offsets with zero latency.
     */
    fun syncAppOffsets(context: Context, packageName: String, versionCode: String) {
        viewModelScope.launch(Dispatchers.IO) {
            val sp = context.getSharedPreferences("cat_offsets_cache", Context.MODE_PRIVATE)
            val deviceKey = DeviceManager.getUniqueDeviceKey(context)
            try {
                Log.d("AppsViewModel", "Querying remote offsets for $packageName v$versionCode (deviceKey=$deviceKey)...")
                val resp = RetrofitInstance.api.getAppOffsets(packageName, versionCode, deviceKey)
                if (resp.offsets != null && resp.offsets.isNotEmpty()) {
                    val gson = Gson()
                    val jsonOffsets = gson.toJson(resp.offsets)
                    sp.edit()
                        .putString(packageName, jsonOffsets)
                        .putString("${packageName}_$versionCode", jsonOffsets)
                        .apply()
                    NativeCore.applyEngineOffsets(jsonOffsets)
                    Log.i("AppsViewModel", "Applied ${resp.offsets.size} remote dynamic offsets for $packageName from server!")
                    return@launch
                }
            } catch (e: Exception) {
                Log.w("AppsViewModel", "Server offsets query failed/unreachable: ${e.message}")
            }

            // Fallback 1: Use cached offsets from previous successful fetch
            val cached = sp.getString(packageName, null)
            if (!cached.isNullOrEmpty()) {
                Log.i("AppsViewModel", "Server unreachable: applied cached dynamic offsets for $packageName.")
                NativeCore.applyEngineOffsets(cached)
            } else {
                // Fallback 2: Hardcoded offsets in native C++ engine
                Log.i("AppsViewModel", "Server unreachable & no cache: using native hardcoded offsets fallback.")
                NativeCore.applyEngineOffsets("")
            }
        }
    }

    /**
     * Pre-installs target package into BlackBox in the background so that launch is INSTANT.
     */
    private fun prewarmPackage(packageName: String) {
        viewModelScope.launch(Dispatchers.IO) {
            try {
                val blackBoxCore = BlackBoxCore.get()
                // Cleanup virtual GMS if previously installed to prevent Chimera buffer overflow crash loop
                if (blackBoxCore.isInstallGms(0)) {
                    try {
                        Log.d("AppsViewModel", "Uninstalling virtual GMS to prevent Chimera buffer crash...")
                        blackBoxCore.uninstallGms(0)
                    } catch (e: Exception) {
                        Log.w("AppsViewModel", "Error uninstalling GMS: ${e.message}")
                    }
                }
                if (!blackBoxCore.isInstalled(packageName, 0)) {
                    // Only clone if the package actually exists on the device host
                    val existsOnDevice = try {
                        BlackBoxCore.getContext().packageManager.getPackageInfo(packageName, 0) != null
                    } catch (e: Exception) {
                        false
                    }
                    if (existsOnDevice) {
                        Log.d("AppsViewModel", "Pre-installing $packageName into virtual container in background...")
                        val result = blackBoxCore.installPackageAsUser(packageName, 0)
                        Log.d("AppsViewModel", "Pre-install finish for $packageName: success=${result.success}")
                    }
                }
            } catch (e: Exception) {
                Log.w("AppsViewModel", "Background prewarm error: ${e.message}")
            }
        }
    }

    /**
     * Verifies whether target package exists on device and checks version code matching.
     */
    fun verifyDevicePackage(context: Context, packageName: String, serverVersion: String) {
        try {
            val packageInfo: PackageInfo = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                context.packageManager.getPackageInfo(packageName, PackageManager.PackageInfoFlags.of(0))
            } else {
                @Suppress("DEPRECATION")
                context.packageManager.getPackageInfo(packageName, 0)
            }

            val localVersionCode: Long = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                packageInfo.longVersionCode
            } else {
                @Suppress("DEPRECATION")
                packageInfo.versionCode.toLong()
            }

            val appLabel = try {
                packageInfo.applicationInfo?.let {
                    context.packageManager.getApplicationLabel(it).toString()
                } ?: packageName
            } catch (e: Exception) {
                packageName
            }

            val serverVersionCode = serverVersion.toLongOrNull()

            // Prewarm package in background so launch is instant
            prewarmPackage(packageName)

            if (serverVersionCode != null && localVersionCode == serverVersionCode) {
                // Both version and app exist on device -> READY TO LAUNCH
                _appTypeState.value = AppType(
                    packageName = packageName,
                    serverVersion = serverVersion,
                    localVersion = localVersionCode.toString(),
                    appName = appLabel,
                    message = "App and version match. Ready to launch in virtual box.",
                    stateType = AppStateType.READY_TO_LAUNCH
                )
            } else {
                // Package exists but version does not match
                _appTypeState.value = AppType(
                    packageName = packageName,
                    serverVersion = serverVersion,
                    localVersion = localVersionCode.toString(),
                    appName = appLabel,
                    message = "Version mismatch: Expected $serverVersion, found $localVersionCode.",
                    stateType = AppStateType.VERSION_MISMATCH
                )
            }
        } catch (e: PackageManager.NameNotFoundException) {
            _appTypeState.value = AppType(
                packageName = packageName,
                serverVersion = serverVersion,
                localVersion = "Not Installed",
                appName = packageName,
                message = "The app is not installed on this device.",
                stateType = AppStateType.NOT_INSTALLED
            )
        }
    }

    /**
     * Launches the target app inside BlackBox virtual container.
     */
    fun launchInVirtualBox(packageName: String, userId: Int = 0, onResult: (Boolean, String) -> Unit) {
        if (isLaunching.value) {
            return
        }
        isLaunching.value = true
        launchingPackage.value = packageName
        val appFriendlyName = when {
            packageName.contains("eightballpool") -> "8 Ball Pool"
            packageName.contains("carrom") -> "Carrom Pool"
            else -> packageName
        }

        viewModelScope.launch(Dispatchers.IO) {
            try {
                val blackBoxCore = BlackBoxCore.get()

                // Ensure virtual GMS is not present to prevent Chimera buffer overflow crash loop
                if (blackBoxCore.isInstallGms(userId)) {
                    try {
                        Log.d("AppsViewModel", "Uninstalling virtual GMS for user $userId to prevent Chimera buffer crash...")
                        blackBoxCore.uninstallGms(userId)
                    } catch (e: Exception) {
                        Log.w("AppsViewModel", "Error uninstalling GMS: ${e.message}")
                    }
                }

                // If not yet installed inside virtual container, clone/install from host system
                if (!blackBoxCore.isInstalled(packageName, userId)) {
                    val isHostInstalled = try {
                        BlackBoxCore.getContext().packageManager.getPackageInfo(packageName, 0) != null
                    } catch (e: Exception) {
                        false
                    }

                    if (!isHostInstalled) {
                        withContext(Dispatchers.Main) {
                            isLaunching.value = false
                            launchingPackage.value = null
                            onResult(false, "$appFriendlyName is not installed on your phone. Please install it first.")
                        }
                        return@launch
                    }

                    val installResult = blackBoxCore.installPackageAsUser(packageName, userId)
                    if (!installResult.success) {
                        withContext(Dispatchers.Main) {
                            isLaunching.value = false
                            launchingPackage.value = null
                            onResult(false, "Virtual install failed: ${installResult.msg}")
                        }
                        return@launch
                    }
                }

                // Ensure offsets are synced and applied to NativeCore before launching
                if (packageName == "com.miniclip.carrom") {
                    val targetVer = _appTypeState.value.serverVersion.ifEmpty { "1491" }
                    val ctx = BlackBoxCore.getContext()
                    syncAppOffsets(ctx, packageName, targetVer)
                }

                // Launch inside virtual container
                val success = blackBoxCore.launchApk(packageName, userId)
                withContext(Dispatchers.Main) {
                    isLaunching.value = false
                    launchingPackage.value = null
                    if (success) {
                        onResult(true, "Launching $appFriendlyName in virtual box...")
                    } else {
                        onResult(false, "Failed to launch $appFriendlyName in virtual box.")
                    }
                }
            } catch (e: Exception) {
                Log.e("AppsViewModel", "Error launching in virtual box", e)
                withContext(Dispatchers.Main) {
                    isLaunching.value = false
                    launchingPackage.value = null
                    onResult(false, "Error: ${e.message}")
                }
            }
        }
    }

    fun getInstalledApps(userId: Int) {
        launchOnUI {
            repo.getVmInstallList(userId, appsLiveData)
        }
    }

    fun getInstalledAppsWithRetry(userId: Int, maxRetries: Int = 3) {
        var retryCount = 0

        fun attemptLoad() {
            launchOnUI {
                repo.getVmInstallList(userId, appsLiveData)
                val currentApps = appsLiveData.value
                if ((currentApps == null || currentApps.isEmpty()) && retryCount < maxRetries) {
                    retryCount++
                    android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
                        attemptLoad()
                    }, 1000)
                }
            }
        }

        attemptLoad()
    }

    fun install(source: String, userID: Int) {
        launchOnUI {
            repo.installApk(source, userID, resultLiveData)
        }
    }

    fun unInstall(packageName: String, userID: Int) {
        launchOnUI {
            repo.unInstall(packageName, userID, resultLiveData)
        }
    }

    fun clearApkData(packageName: String, userID: Int) {
        launchOnUI {
            repo.clearApkData(packageName, userID, resultLiveData)
        }
    }

    fun launchApk(packageName: String, userID: Int) {
        launchOnUI {
            repo.launchApk(packageName, userID, launchLiveData)
        }
    }
}
