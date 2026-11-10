package top.niunaijun.blackboxa.util

import android.content.Context
import android.content.Intent
import androidx.core.content.pm.ShortcutInfoCompat
import androidx.core.content.pm.ShortcutManagerCompat
import androidx.core.graphics.drawable.IconCompat
import androidx.core.graphics.drawable.toBitmap
import top.niunaijun.blackboxa.R
import top.niunaijun.blackboxa.bean.AppInfo
import top.niunaijun.blackboxa.view.main.ShortcutActivity

object ShortcutUtil {

    fun createShortcut(context: Context, userID: Int, info: AppInfo, customName: String? = null) {
        if (ShortcutManagerCompat.isRequestPinShortcutSupported(context)) {
            val labelName = customName ?: info.name
            val intent = Intent(context, ShortcutActivity::class.java)
                .setAction(Intent.ACTION_MAIN)
                .putExtra("pkg", info.packageName)
                .putExtra("userId", userID)

            val builder = ShortcutInfoCompat.Builder(context, info.packageName + userID)
                .setIntent(intent)
                .setShortLabel(labelName)
                .setLongLabel(labelName)

            info.icon?.let { icon ->
                try {
                    builder.setIcon(IconCompat.createWithBitmap(icon.toBitmap()))
                } catch (e: Exception) {
                    // Ignore icon conversion error
                }
            }

            ShortcutManagerCompat.requestPinShortcut(context, builder.build(), null)
            toast(R.string.try_add_shortcut)
        } else {
            toast(R.string.cannot_create_shortcut)
        }
    }
}
