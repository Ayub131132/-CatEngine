package top.niunaijun.blackbox.widget

import android.graphics.Bitmap
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import kotlin.math.roundToInt

// Studio Dark Palette matching reference color picker
private val StudioBg = Color(0xFF242629)
private val StudioHeaderBg = Color(0xFF1E2023)
private val StudioBorder = Color(0xFF383C42)
private val StudioTextPrimary = Color(0xFFEEEEEE)
private val StudioTextSecondary = Color(0xFFAAAAAA)
private val StudioAccentBlue = Color(0xFF00BBFF)

// Swatches on left column matching reference image
private val StudioPresets = listOf(
    Color(0xFF8B1E1E), // Deep Red
    Color(0xFFD29B78), // Peach Tan
    Color(0xFFE5B582), // Golden Tan
    Color(0xFF2E7D32), // Forest Green
    Color(0xFF1B6A5E), // Teal
    Color(0xFF6D4C41), // Deep Earth Brown
    Color(0xFF111111), // Pure Black
    Color(0xFFFFFFFF), // Pure White
    Color(0xFF00BBFF), // Electric Blue
    Color(0xFFFFEA00)  // Golden Yellow
)

/**
 * Clean Color Picker Row without background or borders:
 * Displays title on the left and a clickable color preview badge on the right.
 * Clicking opens the studio-grade visual color picker dialog.
 */
@Composable
fun VisualColorPickerRow(
    title: String,
    selectedColor: Color,
    onColorSelected: (Color) -> Unit,
    modifier: Modifier = Modifier
) {
    var showDialog by remember { mutableStateOf(false) }

    Row(
        modifier = modifier
            .fillMaxWidth()
            .clickable { showDialog = true }
            .padding(vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Text(
            text = title,
            color = Color.White,
            fontSize = 15.sp,
            fontWeight = FontWeight.Medium
        )

        // Visual Color Swatch
        Box(
            modifier = Modifier
                .size(36.dp)
                .clip(CircleShape)
                .background(selectedColor)
                .border(BorderStroke(2.dp, Color.White.copy(alpha = 0.85f)), CircleShape)
        )
    }

    if (showDialog) {
        VisualColorPickerDialog(
            initialColor = selectedColor,
            onDismiss = { showDialog = false },
            onColorConfirmed = {
                onColorSelected(it)
                showDialog = false
            }
        )
    }
}

/**
 * Studio-grade Visual Color Picker dialog matching reference design:
 * - Left swatch palette column
 * - 2D Saturation-Value box with crosshair
 * - Vertical Hue rainbow strip
 * - Vertical Alpha checkerboard strip
 * - Interactive RGB/HSV sliders with colored tracks
 */
@Composable
fun VisualColorPickerDialog(
    initialColor: Color,
    onDismiss: () -> Unit,
    onColorConfirmed: (Color) -> Unit
) {
    // Current HSV and Alpha state
    val initialHsv = remember(initialColor) {
        FloatArray(3).apply {
            android.graphics.Color.colorToHSV(
                android.graphics.Color.argb(
                    (initialColor.alpha * 255).roundToInt(),
                    (initialColor.red * 255).roundToInt(),
                    (initialColor.green * 255).roundToInt(),
                    (initialColor.blue * 255).roundToInt()
                ),
                this
            )
        }
    }

    var hue by remember { mutableFloatStateOf(initialHsv[0]) }
    var sat by remember { mutableFloatStateOf(initialHsv[1].coerceIn(0f, 1f)) }
    var value by remember { mutableFloatStateOf(initialHsv[2].coerceIn(0f, 1f)) }
    var alpha by remember { mutableFloatStateOf(initialColor.alpha.coerceIn(0f, 1f)) }

    // Computed Color
    val currentColor = remember(hue, sat, value, alpha) {
        val rgb = android.graphics.Color.HSVToColor(floatArrayOf(hue, sat, value))
        Color(
            red = android.graphics.Color.red(rgb) / 255f,
            green = android.graphics.Color.green(rgb) / 255f,
            blue = android.graphics.Color.blue(rgb) / 255f,
            alpha = alpha
        )
    }

    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(usePlatformDefaultWidth = false)
    ) {
        Surface(
            modifier = Modifier
                .fillMaxWidth(0.96f)
                .wrapContentHeight()
                .clip(RoundedCornerShape(12.dp))
                .border(BorderStroke(1.dp, StudioBorder), RoundedCornerShape(12.dp)),
            color = StudioBg
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .verticalScroll(rememberScrollState())
            ) {
                // Top Header: "Colors" title, Tabs, and Close Button
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .background(StudioHeaderBg)
                        .padding(horizontal = 14.dp, vertical = 8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(10.dp)
                    ) {
                        Text(
                            text = "Colors",
                            color = StudioTextPrimary,
                            fontSize = 14.sp,
                            fontWeight = FontWeight.Bold
                        )

                        // Active "Picker" Tab Tag
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(4.dp))
                                .background(Color(0xFF32363D))
                                .border(BorderStroke(1.dp, Color(0xFF454A54)), RoundedCornerShape(4.dp))
                                .padding(horizontal = 8.dp, vertical = 2.dp)
                        ) {
                            Text(
                                text = "Picker",
                                color = StudioAccentBlue,
                                fontSize = 11.sp,
                                fontWeight = FontWeight.SemiBold
                            )
                        }
                    }

                    IconButton(
                        onClick = onDismiss,
                        modifier = Modifier.size(28.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Default.Close,
                            contentDescription = "Close Color Picker",
                            tint = StudioTextSecondary,
                            modifier = Modifier.size(16.dp)
                        )
                    }
                }

                // Main Content Area
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(14.dp),
                    verticalArrangement = Arrangement.spacedBy(14.dp)
                ) {
                    // Top Interactive Row: [Left Swatches] + [2D SV Box] + [Vertical Hue Strip] + [Vertical Alpha Strip]
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(180.dp),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        // 1. Left Swatch Column (Recent / Presets)
                        Column(
                            modifier = Modifier
                                .width(34.dp)
                                .fillMaxHeight(),
                            verticalArrangement = Arrangement.SpaceBetween
                        ) {
                            // Current color preview box on top
                            Box(
                                modifier = Modifier
                                    .size(34.dp, 28.dp)
                                    .clip(RoundedCornerShape(4.dp))
                                    .background(currentColor)
                                    .border(BorderStroke(1.dp, Color.White.copy(alpha = 0.6f)), RoundedCornerShape(4.dp))
                            )

                            // Preset Palette swatches
                            StudioPresets.take(6).forEach { presetColor ->
                                Box(
                                    modifier = Modifier
                                        .size(34.dp, 20.dp)
                                        .clip(RoundedCornerShape(3.dp))
                                        .background(presetColor)
                                        .border(BorderStroke(1.dp, Color(0x33FFFFFF)), RoundedCornerShape(3.dp))
                                        .clickable {
                                            val hsv = FloatArray(3)
                                            android.graphics.Color.colorToHSV(
                                                android.graphics.Color.argb(
                                                    255,
                                                    (presetColor.red * 255).roundToInt(),
                                                    (presetColor.green * 255).roundToInt(),
                                                    (presetColor.blue * 255).roundToInt()
                                                ),
                                                hsv
                                            )
                                            hue = hsv[0]
                                            sat = hsv[1]
                                            value = hsv[2]
                                            alpha = 1f
                                        }
                                )
                            }
                        }

                        // 2. 2D Saturation-Value Canvas Box
                        Box(
                            modifier = Modifier
                                .weight(1f)
                                .fillMaxHeight()
                                .clip(RoundedCornerShape(4.dp))
                                .border(BorderStroke(1.dp, Color(0xFF454A54)), RoundedCornerShape(4.dp))
                        ) {
                            SaturationValueBox(
                                hue = hue,
                                saturation = sat,
                                value = value,
                                onSatValChanged = { s, v ->
                                    sat = s
                                    value = v
                                }
                            )
                        }

                        // 3. Vertical Rainbow Hue Strip
                        Box(
                            modifier = Modifier
                                .width(24.dp)
                                .fillMaxHeight()
                                .clip(RoundedCornerShape(4.dp))
                                .border(BorderStroke(1.dp, Color(0xFF454A54)), RoundedCornerShape(4.dp))
                        ) {
                            VerticalHueStrip(
                                hue = hue,
                                onHueChanged = { hue = it }
                            )
                        }

                        // 4. Vertical Alpha / Opacity Strip
                        Box(
                            modifier = Modifier
                                .width(24.dp)
                                .fillMaxHeight()
                                .clip(RoundedCornerShape(4.dp))
                                .border(BorderStroke(1.dp, Color(0xFF454A54)), RoundedCornerShape(4.dp))
                        ) {
                            VerticalAlphaStrip(
                                currentColor = currentColor,
                                alpha = alpha,
                                onAlphaChanged = { alpha = it }
                            )
                        }
                    }

                    // Bottom Sliders Panel: R, G, B, and A with gradient tracks
                    Column(
                        modifier = Modifier.fillMaxWidth(),
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        // RGB Sliders
                        StudioColorSliderRow(
                            label = "R",
                            value = currentColor.red,
                            gradient = listOf(Color.Black, Color(0xFFFF2222)),
                            onValueChange = { newR ->
                                val rgb = android.graphics.Color.rgb(
                                    (newR * 255).roundToInt(),
                                    (currentColor.green * 255).roundToInt(),
                                    (currentColor.blue * 255).roundToInt()
                                )
                                val hsv = FloatArray(3)
                                android.graphics.Color.colorToHSV(rgb, hsv)
                                hue = hsv[0]
                                sat = hsv[1]
                                value = hsv[2]
                            }
                        )

                        StudioColorSliderRow(
                            label = "G",
                            value = currentColor.green,
                            gradient = listOf(Color.Black, Color(0xFF22FF44)),
                            onValueChange = { newG ->
                                val rgb = android.graphics.Color.rgb(
                                    (currentColor.red * 255).roundToInt(),
                                    (newG * 255).roundToInt(),
                                    (currentColor.blue * 255).roundToInt()
                                )
                                val hsv = FloatArray(3)
                                android.graphics.Color.colorToHSV(rgb, hsv)
                                hue = hsv[0]
                                sat = hsv[1]
                                value = hsv[2]
                            }
                        )

                        StudioColorSliderRow(
                            label = "B",
                            value = currentColor.blue,
                            gradient = listOf(Color.Black, Color(0xFF2266FF)),
                            onValueChange = { newB ->
                                val rgb = android.graphics.Color.rgb(
                                    (currentColor.red * 255).roundToInt(),
                                    (currentColor.green * 255).roundToInt(),
                                    (newB * 255).roundToInt()
                                )
                                val hsv = FloatArray(3)
                                android.graphics.Color.colorToHSV(rgb, hsv)
                                hue = hsv[0]
                                sat = hsv[1]
                                value = hsv[2]
                            }
                        )

                        StudioColorSliderRow(
                            label = "A",
                            value = alpha,
                            gradient = listOf(Color(0x00000000), currentColor.copy(alpha = 1f)),
                            onValueChange = { alpha = it }
                        )
                    }

                    // Action Buttons Row: Preview & Confirm
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(top = 4.dp),
                        horizontalArrangement = Arrangement.End,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        TextButton(onClick = onDismiss) {
                            Text(text = "Cancel", color = StudioTextSecondary, fontSize = 13.sp)
                        }

                        Spacer(modifier = Modifier.width(8.dp))

                        Button(
                            onClick = { onColorConfirmed(currentColor) },
                            colors = ButtonDefaults.buttonColors(
                                containerColor = StudioAccentBlue,
                                contentColor = Color.White
                            ),
                            shape = RoundedCornerShape(6.dp),
                            contentPadding = PaddingValues(horizontal = 18.dp, vertical = 8.dp)
                        ) {
                            Text(
                                text = "Select Color",
                                fontSize = 13.sp,
                                fontWeight = FontWeight.Bold
                            )
                        }
                    }
                }
            }
        }
    }
}

/**
 * 2D Saturation-Value Canvas Box:
 * Horizontal: Saturation (White -> Pure Hue)
 * Vertical: Value (Bright -> Pure Black)
 * Interactive crosshair indicator.
 */
@Composable
private fun SaturationValueBox(
    hue: Float,
    saturation: Float,
    value: Float,
    onSatValChanged: (Float, Float) -> Unit
) {
    val pureHueColor = remember(hue) {
        val rgb = android.graphics.Color.HSVToColor(floatArrayOf(hue, 1f, 1f))
        Color(rgb)
    }

    BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
        val width = constraints.maxWidth.toFloat()
        val height = constraints.maxHeight.toFloat()

        Canvas(
            modifier = Modifier
                .fillMaxSize()
                .pointerInput(Unit) {
                    detectTapGestures { offset ->
                        val s = (offset.x / width).coerceIn(0f, 1f)
                        val v = (1f - (offset.y / height)).coerceIn(0f, 1f)
                        onSatValChanged(s, v)
                    }
                }
                .pointerInput(Unit) {
                    detectDragGestures { change, _ ->
                        change.consume()
                        val s = (change.position.x / width).coerceIn(0f, 1f)
                        val v = (1f - (change.position.y / height)).coerceIn(0f, 1f)
                        onSatValChanged(s, v)
                    }
                }
        ) {
            // Horizontal gradient: White to pure Hue
            drawRect(
                brush = Brush.horizontalGradient(
                    colors = listOf(Color.White, pureHueColor)
                )
            )

            // Vertical gradient: Transparent to Black
            drawRect(
                brush = Brush.verticalGradient(
                    colors = listOf(Color.Transparent, Color.Black)
                )
            )

            // Crosshair indicator
            val crossX = saturation * size.width
            val crossY = (1f - value) * size.height

            // Outer dark ring
            drawCircle(
                color = Color.Black,
                radius = 7.dp.toPx(),
                center = Offset(crossX, crossY),
                style = Stroke(width = 2.dp.toPx())
            )
            // Inner white ring
            drawCircle(
                color = Color.White,
                radius = 6.dp.toPx(),
                center = Offset(crossX, crossY),
                style = Stroke(width = 1.5.dp.toPx())
            )
        }
    }
}

/**
 * Vertical Rainbow Hue Strip
 */
@Composable
private fun VerticalHueStrip(
    hue: Float,
    onHueChanged: (Float) -> Unit
) {
    val rainbowColors = listOf(
        Color(0xFFFF0000), // Red
        Color(0xFFFFFF00), // Yellow
        Color(0xFF00FF00), // Green
        Color(0xFF00FFFF), // Cyan
        Color(0xFF0000FF), // Blue
        Color(0xFFFF00FF), // Magenta
        Color(0xFFFF0000)  // Red
    )

    BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
        val height = constraints.maxHeight.toFloat()

        Canvas(
            modifier = Modifier
                .fillMaxSize()
                .pointerInput(Unit) {
                    detectTapGestures { offset ->
                        val fraction = (offset.y / height).coerceIn(0f, 1f)
                        onHueChanged(fraction * 360f)
                    }
                }
                .pointerInput(Unit) {
                    detectDragGestures { change, _ ->
                        change.consume()
                        val fraction = (change.position.y / height).coerceIn(0f, 1f)
                        onHueChanged(fraction * 360f)
                    }
                }
        ) {
            drawRect(brush = Brush.verticalGradient(rainbowColors))

            // Indicator line / pointers
            val indicatorY = (hue / 360f) * size.height
            drawLine(
                color = Color.White,
                start = Offset(0f, indicatorY),
                end = Offset(size.width, indicatorY),
                strokeWidth = 2.5.dp.toPx()
            )
            drawLine(
                color = Color.Black,
                start = Offset(0f, indicatorY - 1.5f),
                end = Offset(size.width, indicatorY - 1.5f),
                strokeWidth = 1.dp.toPx()
            )
        }
    }
}

/**
 * Vertical Alpha / Opacity Strip with checkerboard simulation
 */
@Composable
private fun VerticalAlphaStrip(
    currentColor: Color,
    alpha: Float,
    onAlphaChanged: (Float) -> Unit
) {
    BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
        val height = constraints.maxHeight.toFloat()

        Canvas(
            modifier = Modifier
                .fillMaxSize()
                .pointerInput(Unit) {
                    detectTapGestures { offset ->
                        val fraction = (1f - (offset.y / height)).coerceIn(0f, 1f)
                        onAlphaChanged(fraction)
                    }
                }
                .pointerInput(Unit) {
                    detectDragGestures { change, _ ->
                        change.consume()
                        val fraction = (1f - (change.position.y / height)).coerceIn(0f, 1f)
                        onAlphaChanged(fraction)
                    }
                }
        ) {
            // Draw gradient from solid color (top) to transparent (bottom)
            drawRect(
                brush = Brush.verticalGradient(
                    colors = listOf(currentColor.copy(alpha = 1f), Color.Transparent)
                )
            )

            val indicatorY = (1f - alpha) * size.height
            drawLine(
                color = Color.White,
                start = Offset(0f, indicatorY),
                end = Offset(size.width, indicatorY),
                strokeWidth = 2.5.dp.toPx()
            )
        }
    }
}

/**
 * Studio-style horizontal slider row for R, G, B, and A with gradient track
 */
@Composable
private fun StudioColorSliderRow(
    label: String,
    value: Float,
    gradient: List<Color>,
    onValueChange: (Float) -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(28.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        // Label (R, G, B, A)
        Text(
            text = label,
            color = StudioTextSecondary,
            fontSize = 12.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.width(16.dp)
        )

        // Value text
        Text(
            text = "${(value * 255).roundToInt()}",
            color = StudioTextPrimary,
            fontSize = 11.sp,
            fontFamily = FontFamily.Monospace,
            modifier = Modifier.width(28.dp)
        )

        // Gradient Slider Bar
        Box(
            modifier = Modifier
                .weight(1f)
                .height(18.dp)
                .clip(RoundedCornerShape(3.dp))
                .border(BorderStroke(1.dp, Color(0xFF454A54)), RoundedCornerShape(3.dp))
        ) {
            BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
                val width = constraints.maxWidth.toFloat()

                Canvas(
                    modifier = Modifier
                        .fillMaxSize()
                        .pointerInput(Unit) {
                            detectTapGestures { offset ->
                                val fraction = (offset.x / width).coerceIn(0f, 1f)
                                onValueChange(fraction)
                            }
                        }
                        .pointerInput(Unit) {
                            detectDragGestures { change, _ ->
                                change.consume()
                                val fraction = (change.position.x / width).coerceIn(0f, 1f)
                                onValueChange(fraction)
                            }
                        }
                ) {
                    drawRect(brush = Brush.horizontalGradient(gradient))

                    // Pointer notch
                    val notchX = value * size.width
                    drawLine(
                        color = Color.White,
                        start = Offset(notchX, 0f),
                        end = Offset(notchX, size.height),
                        strokeWidth = 2.dp.toPx()
                    )
                }
            }
        }
    }
}
