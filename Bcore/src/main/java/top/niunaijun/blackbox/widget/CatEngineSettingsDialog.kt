package top.niunaijun.blackbox.widget

// CatEngine settings dialog host

import android.app.Activity
import android.app.Dialog
import android.content.Context
import android.graphics.drawable.ColorDrawable
import android.util.TypedValue
import android.view.Gravity
import android.view.Window
import android.view.WindowManager
import androidx.activity.ComponentDialog
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import java.lang.ref.WeakReference

/**
 * Controller to show the Cat Engine settings window with a dimmed background,
 * either programmatically over any Activity (such as Carrom Pool in-game)
 * or as a direct Composable Dialog inside Compose UI screens.
 */
object CatEngineSettingsDialog {
    private var activeDialog: Dialog? = null
    private var activeActivityRef: WeakReference<Activity>? = null

    /**
     * Programmatically launch the Jetpack Compose Cat Engine window over the given Activity.
     * Displays INSTANTLY with zero latency (cached & prewarmed, no window animation delays).
     */
    @JvmStatic
    fun show(activity: Activity) {
        if (activity.isFinishing || activity.isDestroyed) return

        activity.runOnUiThread {
            try {
                val cached = activeDialog
                val cachedAct = activeActivityRef?.get()

                if (cached != null && cachedAct === activity) {
                    if (!cached.isShowing) {
                        cached.show()
                    }
                    return@runOnUiThread
                }

                // If not cached yet or activity changed, create and show immediately
                try {
                    cached?.dismiss()
                } catch (ignored: Throwable) {}

                val dialog = createDialog(activity)
                activeDialog = dialog
                activeActivityRef = WeakReference(activity)
                dialog.show()
            } catch (t: Throwable) {
                android.util.Log.e("CatEngineDialog", "Failed to show CatEngineSettingsDialog", t)
            }
        }
    }

    /**
     * Prewarm Jetpack Compose, Cat Engine config, and pre-instantiate the Dialog ahead of time
     * so that clicking the logo opens the window INSTANTLY with zero delay.
     */
    @JvmStatic
    fun prewarm(activity: Activity) {
        if (activity.isFinishing || activity.isDestroyed) return
        activity.runOnUiThread {
            try {
                CatEngineConfig.load(activity)
                if (activeDialog == null || activeActivityRef?.get() !== activity) {
                    val dialog = createDialog(activity)
                    activeDialog = dialog
                    activeActivityRef = WeakReference(activity)
                }
            } catch (ignored: Throwable) {}
        }
    }

    private fun createDialog(activity: Activity): Dialog {
        val dialog = ComponentDialog(activity)
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE)

        val composeView = ComposeView(activity).apply {
            // Keep composition active throughout the Activity lifecycle for instant re-opening
            setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed)
            setContent {
                CatEngineSettingsScreen(
                    onDismiss = {
                        try {
                            dialog.dismiss()
                        } catch (ignored: Exception) {}
                    }
                )
            }
        }

        dialog.setContentView(composeView)

        // Dim background behind window and disable transition delay for instant popup
        dialog.window?.let { win ->
            win.setBackgroundDrawable(ColorDrawable(android.graphics.Color.TRANSPARENT))
            win.addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND)
            win.setDimAmount(0.68f)
            // 0 = no window animation delay, opens instantly
            win.setWindowAnimations(0)

            val dm = activity.resources.displayMetrics
            val targetWidth = (dm.widthPixels * 0.94f).toInt().coerceAtMost(dpToPx(activity, 480))
            win.setLayout(targetWidth, WindowManager.LayoutParams.WRAP_CONTENT)
            win.setGravity(Gravity.CENTER)
        }

        return dialog
    }

    private fun dpToPx(context: Context, dp: Int): Int {
        return TypedValue.applyDimension(
            TypedValue.COMPLEX_UNIT_DIP,
            dp.toFloat(),
            context.resources.displayMetrics
        ).toInt()
    }
}

/**
 * Direct Composable Dialog for use in Compose-based screens (such as HomeScreen).
 */
@Composable
fun CatEngineComposeDialog(
    onDismiss: () -> Unit
) {
    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(
            dismissOnBackPress = true,
            dismissOnClickOutside = true,
            usePlatformDefaultWidth = false
        )
    ) {
        CatEngineSettingsScreen(onDismiss = onDismiss)
    }
}
