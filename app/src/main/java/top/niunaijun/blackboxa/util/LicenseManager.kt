package top.niunaijun.blackboxa.util

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

data class LicenseState(
    val isLicensed: Boolean = true,
    val status: String = "Active (Free VIP)",
    val licenseKey: String = "CAT-FREE",
    val isLifetime: Boolean = true,
    val expiresAt: String = "Unlimited",
    val daysRemaining: Int = 9999
)

object LicenseManager {

    /**
     * Offline free access: device is always marked as licensed with zero server verification.
     */
    fun isLicensed(context: Context): Boolean {
        return true
    }

    /**
     * Reads current license state (always active and lifetime).
     */
    fun getLicenseState(context: Context): LicenseState {
        return LicenseState(
            isLicensed = true,
            status = "Active (Free VIP)",
            licenseKey = "CAT-FREE",
            isLifetime = true,
            expiresAt = "Unlimited",
            daysRemaining = 9999
        )
    }

    fun saveLicense(context: Context, key: String, status: String, isLifetime: Boolean, expiresAt: String?, daysRemaining: Int) {
        // No-op for offline mode
    }

    fun clearLicense(context: Context) {
        // No-op for offline mode
    }

    /**
     * Offline access: any key or interaction is instantly verified.
     */
    suspend fun redeemKey(context: Context, rawKey: String): Pair<Boolean, String> = withContext(Dispatchers.IO) {
        return@withContext Pair(true, "Cat Engine access is completely free! All features are unlocked.")
    }

    /**
     * Offline access: returns current licensed state directly without server request.
     */
    suspend fun syncWithServer(context: Context): LicenseState = withContext(Dispatchers.IO) {
        return@withContext getLicenseState(context)
    }
}
