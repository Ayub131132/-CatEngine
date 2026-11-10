package top.niunaijun.blackboxa.view.main

import android.app.Activity
import android.content.ComponentName
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.util.Log
import top.niunaijun.blackbox.BlackBoxCore

/**
 * Trampoline activity that captures host browser OAuth callbacks / deep-link redirects
 * (e.g. Facebook SDK login, game auth redirects) and relays them directly into the
 * active BlackBox virtual user session.
 */
class OAuthRelayActivity : Activity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val incomingIntent = intent
        try {
            Log.i(TAG, "=== [FacebookOAuth] OAuthRelayActivity started ===")
            if (incomingIntent == null || incomingIntent.data == null) {
                Log.w(TAG, "[FacebookOAuth] Incoming intent or URI data is null. Action=${incomingIntent?.action}")
                finish()
                return
            }

            val uri = incomingIntent.data!!
            val scheme = uri.scheme ?: ""
            val host = uri.host ?: ""
            Log.i(TAG, "[FacebookOAuth] Received callback URI: $uri (scheme: '$scheme', host: '$host')")

            // Determine target package name inside BlackBox virtual space
            var targetPackage = "com.miniclip.carrom"
            if (host.startsWith("cct.")) {
                targetPackage = host.removePrefix("cct.")
                Log.i(TAG, "[FacebookOAuth] Target package extracted from cct host: $targetPackage")
            } else if (scheme.startsWith("eightball") || uri.toString().contains("eightball", ignoreCase = true) || uri.toString().contains("8ball", ignoreCase = true)) {
                targetPackage = "com.miniclip.eightballpool"
            } else if (scheme.startsWith("carrom") || uri.toString().contains("carrom", ignoreCase = true)) {
                targetPackage = "com.miniclip.carrom"
            } else {
                try {
                    if (BlackBoxCore.get().isInstalled("com.miniclip.carrom", 0)) {
                        targetPackage = "com.miniclip.carrom"
                    } else if (BlackBoxCore.get().isInstalled("com.miniclip.eightballpool", 0)) {
                        targetPackage = "com.miniclip.eightballpool"
                    }
                } catch (e: Throwable) {
                    Log.w(TAG, "[FacebookOAuth] Error checking installed package: ${e.message}")
                }
            }
            Log.i(TAG, "[FacebookOAuth] Destination package confirmed: $targetPackage")

            // 1. If redirected via custom scheme fb<app_id>://authorize#...
            // Facebook SDK inside game expects fbconnect://cct.<package_name> or direct URI
            val queryPart = if (uri.query != null) "?${uri.query}" else ""
            val fragmentPart = if (uri.fragment != null) "#${uri.fragment}" else ""
            val cctUri = if (scheme.startsWith("fb") && !scheme.equals("fbconnect", ignoreCase = true)) {
                val converted = Uri.parse("fbconnect://cct.$targetPackage$queryPart$fragmentPart")
                Log.i(TAG, "[FacebookOAuth] Converted app-scheme '$scheme' to standard cct URI: $converted")
                converted
            } else {
                uri
            }

            // 2. Direct launch of Facebook CustomTabActivity
            val cctActivityIntent = Intent(Intent.ACTION_VIEW, cctUri).apply {
                setPackage(targetPackage)
                component = ComponentName(targetPackage, "com.facebook.CustomTabActivity")
                incomingIntent.extras?.let { putExtras(it) }
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            }
            try {
                Log.i(TAG, "[FacebookOAuth] Relaying direct CustomTabActivity intent to virtual space: $cctActivityIntent")
                BlackBoxCore.getBActivityManager().startActivityAms(
                    0,
                    cctActivityIntent,
                    null,
                    null,
                    null,
                    0,
                    Intent.FLAG_ACTIVITY_NEW_TASK,
                    null
                )
                Log.i(TAG, "[FacebookOAuth] Direct CustomTabActivity relay dispatched successfully")
            } catch (e: Throwable) {
                Log.w(TAG, "[FacebookOAuth] Direct CustomTabActivity relay failed: ${e.message}", e)
            }

            // 3. Also broadcast CustomTabActivity.action_customTabRedirect in case CustomTabMainActivity is actively listening
            try {
                val broadcastIntent = Intent("CustomTabActivity.action_customTabRedirect").apply {
                    setPackage(targetPackage)
                    putExtra("CustomTabMainActivity.extra_url", cctUri.toString())
                }
                Log.i(TAG, "[FacebookOAuth] Broadcasting customTabRedirect to virtual space: $broadcastIntent")
                BlackBoxCore.getBActivityManager().sendBroadcast(broadcastIntent, null, 0)
                Log.i(TAG, "[FacebookOAuth] Broadcast dispatched successfully")
            } catch (e: Throwable) {
                Log.w(TAG, "[FacebookOAuth] Broadcast dispatch failed: ${e.message}", e)
            }

            Log.i(TAG, "=== [FacebookOAuth] OAuthRelayActivity completed ===")
        } catch (e: Throwable) {
            Log.e(TAG, "[FacebookOAuth] Fatal exception relaying OAuth intent: ${e.message}", e)
        }
        finish()
    }

    companion object {
        private const val TAG = "OAuthRelayActivity"
    }
}
