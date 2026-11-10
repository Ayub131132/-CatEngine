package top.niunaijun.blackbox.widget

// CatEngine configuration state

import android.content.Context
import android.content.SharedPreferences
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import top.niunaijun.blackbox.core.NativeCore

/**
 * Real-time configuration manager and persistence for Cat Engine features.
 * Synchronizes with SharedPreferences and immediately applies settings to the native C++ OpenGL engine.
 */
object CatEngineConfig {
    private const val PREF_NAME = "blackbox_cat_engine_settings"

    private const val KEY_SHOW_PREDICTION = "cat_show_prediction"
    private const val KEY_SHOW_POCKET = "cat_show_pocket"
    private const val KEY_SHOW_TABLE = "cat_show_table"
    private const val KEY_LINE_THICKNESS = "cat_line_thickness"
    private const val KEY_LINE_TRANSPARENCY = "cat_line_transparency"
    private const val KEY_POCKET_SIZE = "cat_pocket_size"
    private const val KEY_STRIKER_COLOR = "cat_striker_color"
    private const val KEY_MY_PUCK_COLOR = "cat_my_puck_color"
    private const val KEY_OPPONENT_PUCK_COLOR = "cat_opponent_puck_color"
    private const val KEY_KEEP_LINE_AFTER_SHOT = "cat_keep_line_after_shot"
    private const val KEY_PHYSICS_ENGINE_MODE = "cat_physics_engine_mode"
    private const val KEY_FAST_FIRST_SHOT = "cat_fast_first_shot"

    // Default Colors
    val DefaultStrikerColor = Color(0xFFFFFFFF)       // White
    val DefaultMyPuckColor = Color(0xFF2699FF)        // Electric Cyan/Blue
    val DefaultOpponentPuckColor = Color(0xFFFFD91A)  // Laser Yellow

    // Reactive Compose States
    var showPrediction by mutableStateOf(true)
        private set
    var showPocket by mutableStateOf(true)
        private set
    var showTable by mutableStateOf(false)
        private set
    var lineThickness by mutableIntStateOf(4) // 1 to 10 (Default 4)
        private set
    var lineTransparency by mutableIntStateOf(10) // 1 to 10 (Default 10 = Fully Opaque)
        private set
    var pocketSize by mutableIntStateOf(5) // 1 to 10 (Default 5 ~ 2.15 cm)
        private set
    var strikerColor by mutableStateOf(DefaultStrikerColor)
        private set
    var myPuckColor by mutableStateOf(DefaultMyPuckColor)
        private set
    var opponentPuckColor by mutableStateOf(DefaultOpponentPuckColor)
        private set
    var keepLineAfterShot by mutableStateOf(true)
        private set
    var physicsEngineMode by mutableIntStateOf(0) // 0 = Snake Engine CCD, 1 = In-Engine Native
        private set
    var fastFirstShotMode by mutableStateOf(false)
        private set

    private var isLoaded = false

    private fun getPrefs(context: Context): SharedPreferences {
        return context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE)
    }

    /**
     * Load settings from storage and populate reactive Compose states.
     */
    fun load(context: Context) {
        try {
            CatEngineLogManager.start(context)
        } catch (ignored: Throwable) {}
        val sp = getPrefs(context)
        val calibSp = try { context.getSharedPreferences("blackbox_carrom_calib", Context.MODE_PRIVATE) } catch (t: Throwable) { null }
        val calibBorder = calibSp?.getBoolean("calib_show_border", false) ?: false
        showPrediction = sp.getBoolean(KEY_SHOW_PREDICTION, true)
        showPocket = sp.getBoolean(KEY_SHOW_POCKET, true)
        showTable = sp.getBoolean(KEY_SHOW_TABLE, calibBorder)
        lineThickness = sp.getInt(KEY_LINE_THICKNESS, 4).coerceIn(1, 10)
        lineTransparency = sp.getInt(KEY_LINE_TRANSPARENCY, 10).coerceIn(1, 10)
        pocketSize = sp.getInt(KEY_POCKET_SIZE, 5).coerceIn(1, 10)

        val scArgb = sp.getInt(KEY_STRIKER_COLOR, DefaultStrikerColor.toArgb())
        strikerColor = Color(scArgb)

        val mcArgb = sp.getInt(KEY_MY_PUCK_COLOR, DefaultMyPuckColor.toArgb())
        myPuckColor = Color(mcArgb)

        val ocArgb = sp.getInt(KEY_OPPONENT_PUCK_COLOR, DefaultOpponentPuckColor.toArgb())
        opponentPuckColor = Color(ocArgb)

        keepLineAfterShot = sp.getBoolean(KEY_KEEP_LINE_AFTER_SHOT, true)
        physicsEngineMode = sp.getInt(KEY_PHYSICS_ENGINE_MODE, 0).coerceIn(0, 1)
        fastFirstShotMode = sp.getBoolean(KEY_FAST_FIRST_SHOT, false)
        isLoaded = true
    }

    /**
     * Apply all current settings to the native OpenGL engine.
     */
    fun applyAll(context: Context) {
        if (!isLoaded) {
            load(context)
        }
        try {
            NativeCore.setShowPrediction(showPrediction)
            NativeCore.setShowPocket(showPocket)
            NativeCore.setShowBorder(showTable)

            // Line thickness mapping: 1 -> 0.15cm (very thin), 4 -> 0.60cm (default), 10 -> 1.50cm (very thick)
            val thicknessCm = 0.15f + (lineThickness - 1) * 0.15f
            NativeCore.setGuidelineThickness(thicknessCm)

            // Line transparency mapping: 1 -> 0.10f, 10 -> 1.00f
            val alpha = (lineTransparency / 10.0f).coerceIn(0.1f, 1.0f)
            NativeCore.setLineTransparency(alpha)

            // Pocket radius mapping: 1 -> 1.35cm, 5 -> 2.15cm, 10 -> 3.15cm
            val radiusCm = 1.35f + (pocketSize - 1) * 0.20f
            NativeCore.setPocketRadius(radiusCm)

            NativeCore.setCustomColors(
                strikerColor.toArgb(),
                myPuckColor.toArgb(),
                opponentPuckColor.toArgb()
            )

            NativeCore.setKeepLineAfterShot(keepLineAfterShot)
            NativeCore.setPhysicsEngineMode(physicsEngineMode)
            NativeCore.setFastFirstShotMode(fastFirstShotMode)
        } catch (t: Throwable) {
            android.util.Log.e("CatEngineConfig", "Failed to apply settings to native engine", t)
        }
    }

    fun updateShowPrediction(context: Context, enabled: Boolean) {
        showPrediction = enabled
        getPrefs(context).edit().putBoolean(KEY_SHOW_PREDICTION, enabled).apply()
        try {
            NativeCore.setShowPrediction(enabled)
        } catch (ignored: Throwable) {}
    }

    fun updateShowPocket(context: Context, enabled: Boolean) {
        showPocket = enabled
        getPrefs(context).edit().putBoolean(KEY_SHOW_POCKET, enabled).apply()
        try {
            NativeCore.setShowPocket(enabled)
        } catch (ignored: Throwable) {}
    }

    fun updateShowTable(context: Context, enabled: Boolean) {
        showTable = enabled
        getPrefs(context).edit().putBoolean(KEY_SHOW_TABLE, enabled).apply()
        try {
            context.getSharedPreferences("blackbox_carrom_calib", Context.MODE_PRIVATE)
                .edit()
                .putBoolean("calib_show_border", enabled)
                .putBoolean("cat_engine_show_table", enabled)
                .apply()
        } catch (ignored: Throwable) {}
        try {
            NativeCore.setShowBorder(enabled)
        } catch (ignored: Throwable) {}
    }

    fun updateLineThickness(context: Context, thickness: Int) {
        val clamped = thickness.coerceIn(1, 10)
        lineThickness = clamped
        getPrefs(context).edit().putInt(KEY_LINE_THICKNESS, clamped).apply()
        try {
            val thicknessCm = 0.15f + (clamped - 1) * 0.15f
            NativeCore.setGuidelineThickness(thicknessCm)
        } catch (ignored: Throwable) {}
    }

    fun updateLineTransparency(context: Context, transparency: Int) {
        val clamped = transparency.coerceIn(1, 10)
        lineTransparency = clamped
        getPrefs(context).edit().putInt(KEY_LINE_TRANSPARENCY, clamped).apply()
        try {
            val alpha = (clamped / 10.0f).coerceIn(0.1f, 1.0f)
            NativeCore.setLineTransparency(alpha)
        } catch (ignored: Throwable) {}
    }

    fun updatePocketSize(context: Context, size: Int) {
        val clamped = size.coerceIn(1, 10)
        pocketSize = clamped
        getPrefs(context).edit().putInt(KEY_POCKET_SIZE, clamped).apply()
        try {
            val radiusCm = 1.35f + (clamped - 1) * 0.20f
            NativeCore.setPocketRadius(radiusCm)
        } catch (ignored: Throwable) {}
    }

    fun updateStrikerColor(context: Context, color: Color) {
        strikerColor = color
        getPrefs(context).edit().putInt(KEY_STRIKER_COLOR, color.toArgb()).apply()
        try {
            NativeCore.setCustomColors(
                color.toArgb(),
                myPuckColor.toArgb(),
                opponentPuckColor.toArgb()
            )
        } catch (ignored: Throwable) {}
    }

    fun updateMyPuckColor(context: Context, color: Color) {
        myPuckColor = color
        getPrefs(context).edit().putInt(KEY_MY_PUCK_COLOR, color.toArgb()).apply()
        try {
            NativeCore.setCustomColors(
                strikerColor.toArgb(),
                color.toArgb(),
                opponentPuckColor.toArgb()
            )
        } catch (ignored: Throwable) {}
    }

    fun updateOpponentPuckColor(context: Context, color: Color) {
        opponentPuckColor = color
        getPrefs(context).edit().putInt(KEY_OPPONENT_PUCK_COLOR, color.toArgb()).apply()
        try {
            NativeCore.setCustomColors(
                strikerColor.toArgb(),
                myPuckColor.toArgb(),
                color.toArgb()
            )
        } catch (ignored: Throwable) {}
    }

    fun updateKeepLineAfterShot(context: Context, enabled: Boolean) {
        keepLineAfterShot = enabled
        getPrefs(context).edit().putBoolean(KEY_KEEP_LINE_AFTER_SHOT, enabled).apply()
        try {
            NativeCore.setKeepLineAfterShot(enabled)
        } catch (ignored: Throwable) {}
    }

    fun updatePhysicsEngineMode(context: Context, mode: Int) {
        val clamped = mode.coerceIn(0, 1)
        physicsEngineMode = clamped
        getPrefs(context).edit().putInt(KEY_PHYSICS_ENGINE_MODE, clamped).apply()
        try {
            NativeCore.setPhysicsEngineMode(clamped)
        } catch (ignored: Throwable) {}
    }

    fun updateFastFirstShotMode(context: Context, enabled: Boolean) {
        fastFirstShotMode = enabled
        getPrefs(context).edit().putBoolean(KEY_FAST_FIRST_SHOT, enabled).apply()
        try {
            NativeCore.setFastFirstShotMode(enabled)
        } catch (ignored: Throwable) {}
    }
}
