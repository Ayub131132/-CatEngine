package top.niunaijun.blackboxa.api

import retrofit2.http.Body
import retrofit2.http.GET
import retrofit2.http.POST
import retrofit2.http.Path
import retrofit2.http.Query

interface ApiService {
    @GET("/apps")
    suspend fun getApps(): List<ServerResponse>

    @GET("/app/{packageName}/{versionCode}")
    suspend fun getAppOffsets(
        @Path("packageName") packageName: String,
        @Path("versionCode") versionCode: String,
        @Query("deviceKey") deviceKey: String? = null
    ): AppOffsetsResponse

    @POST("/license/redeem")
    suspend fun redeemLicense(
        @Body request: LicenseRedeemRequest
    ): LicenseRedeemResponse

    @GET("/license/check")
    suspend fun checkLicense(
        @Query("deviceKey") deviceKey: String
    ): LicenseStatusResponse
}
