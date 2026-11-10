package top.niunaijun.blackboxa.api

data class ServerResponse(
    val packageName: String,
    val versionCode: String
)

data class AppOffsetsResponse(
    val packageName: String,
    val versionCode: String,
    val offsets: Map<String, String>? = null
)

data class LicenseRedeemRequest(
    val licenseKey: String,
    val deviceKey: String
)

data class LicenseRedeemResponse(
    val success: Boolean = false,
    val message: String = "",
    val status: String? = null,
    val licenseKey: String? = null,
    val scope: String? = null,
    val isLifetime: Boolean = false,
    val expiresAt: String? = null,
    val daysRemaining: Int = 0,
    val signature: String? = null,
    val error: String? = null
)

data class LicenseStatusResponse(
    val isLicensed: Boolean = false,
    val status: String = "Not Activated",
    val licenseKey: String? = null,
    val isLifetime: Boolean = false,
    val expiresAt: String? = null,
    val daysRemaining: Int = 0,
    val signature: String? = null,
    val error: String? = null
)
