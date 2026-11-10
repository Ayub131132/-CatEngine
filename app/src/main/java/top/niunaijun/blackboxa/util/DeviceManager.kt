package top.niunaijun.blackboxa.util

import android.content.Context
import android.media.MediaDrm
import android.os.Build
import android.provider.Settings
import java.nio.ByteBuffer
import java.security.MessageDigest
import java.util.UUID

object DeviceManager {

    private const val PREFS_NAME = "cat_device_identity"
    private const val KEY_UNIQUE_DEVICE_ID = "permanent_device_key_10digit"

    /**
     * Returns a deterministic, permanent, never-changing 10-digit unique device key.
     * Stored in SharedPreferences to ensure it remains 100% stable across OS updates,
     * app restarts, and launches.
     */
    @Synchronized
    fun getUniqueDeviceKey(context: Context): String {
        val sp = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val existingKey = sp.getString(KEY_UNIQUE_DEVICE_ID, null)
        if (!existingKey.isNullOrEmpty() && existingKey.length == 10 && existingKey.all { it.isDigit() }) {
            return existingKey
        }

        // Generate deterministic hardware identity entropy
        val rawEntropy = buildHardwareEntropy(context)
        val generated10DigitKey = hashTo10DigitNumber(rawEntropy)

        // Persist permanently
        sp.edit().putString(KEY_UNIQUE_DEVICE_ID, generated10DigitKey).apply()
        return generated10DigitKey
    }

    /**
     * Returns the 10-digit key formatted nicely for user readability (e.g., "1234-5678-90").
     */
    fun getFormattedDeviceKey(context: Context): String {
        val key = getUniqueDeviceKey(context)
        return if (key.length == 10) {
            "${key.substring(0, 4)}-${key.substring(4, 8)}-${key.substring(8)}"
        } else {
            key
        }
    }

    private fun buildHardwareEntropy(context: Context): String {
        val sb = StringBuilder()

        // 1. Android ID (SSAID)
        val androidId = try {
            Settings.Secure.getString(context.contentResolver, Settings.Secure.ANDROID_ID) ?: ""
        } catch (e: Exception) {
            ""
        }
        sb.append("android_id:").append(androidId).append(";")

        // 2. Hardware Widevine DRM unique device ID (Hardware-baked TEE identifier)
        val widevineId = getWidevineUniqueId()
        if (widevineId.isNotEmpty()) {
            sb.append("widevine:").append(widevineId).append(";")
        }

        // 3. Static Hardware Characteristics
        sb.append("board:").append(Build.BOARD).append(";")
        sb.append("brand:").append(Build.BRAND).append(";")
        sb.append("device:").append(Build.DEVICE).append(";")
        sb.append("hardware:").append(Build.HARDWARE).append(";")
        sb.append("manufacturer:").append(Build.MANUFACTURER).append(";")
        sb.append("model:").append(Build.MODEL).append(";")
        sb.append("product:").append(Build.PRODUCT)

        return sb.toString()
    }

    private fun getWidevineUniqueId(): String {
        return try {
            val widevineUuid = UUID(-0x121074568629b532L, -0x5c37d8232ae2de13L)
            val mediaDrm = MediaDrm(widevineUuid)
            val bytes = mediaDrm.getPropertyByteArray(MediaDrm.PROPERTY_DEVICE_UNIQUE_ID)
            mediaDrm.close()
            bytes.joinToString("") { "%02x".format(it) }
        } catch (e: Exception) {
            ""
        }
    }

    /**
     * Converts arbitrary hardware string entropy to a strict 10-digit decimal number (range 1000000000 to 9999999999).
     */
    private fun hashTo10DigitNumber(input: String): String {
        val digest = MessageDigest.getInstance("SHA-256")
        val hash = digest.digest(input.toByteArray(Charsets.UTF_8))
        val buffer = ByteBuffer.wrap(hash)
        val longVal = buffer.long and Long.MAX_VALUE

        // Map to uniform 10-digit range [1_000_000_000, 9_999_999_999]
        val tenDigit = 1_000_000_000L + (longVal % 9_000_000_000L)
        return tenDigit.toString()
    }
}
