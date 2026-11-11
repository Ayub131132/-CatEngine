package top.niunaijun.blackbox.widget

import android.graphics.Bitmap
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.clickable
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
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
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt

// Clean Dark Palette with Blue theme matching user's design assets (#00BBFF)
private val WindowBg = Color(0xFF0F111D)
private val WindowBorder = Color(0xFF25293F)
private val AccentBlue = Color(0xFF00BBFF)
private val AccentGreen = Color(0xFF4ADE80)
private val TextPrimary = Color(0xFFFFFFFF)
private val TextSecondary = Color(0xFF94A3B8)

@Composable
fun CatEngineSettingsScreen(
    onDismiss: () -> Unit
) {
    val context = LocalContext.current

    // Ensure config is loaded
    LaunchedEffect(Unit) {
        CatEngineConfig.load(context)
        CatEngineConfig.applyAll(context)
    }

    // Dialog Window Surface (Instant display with zero animation lag)
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 24.dp)
            .shadow(24.dp, RoundedCornerShape(24.dp)),
        shape = RoundedCornerShape(24.dp),
        color = WindowBg,
        border = BorderStroke(1.dp, WindowBorder)
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 20.dp, vertical = 18.dp)
        ) {
            // Header: [Logo + Title] ... [Cross (Close) Button]
            WindowHeader(onDismiss = onDismiss)

            Spacer(modifier = Modifier.height(14.dp))

            // Scrollable Content
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f, fill = false)
                    .verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                // Section 1: Aiming & Prediction
                SectionTitle(title = "Aiming & Prediction")

                // Show Prediction switch (Default ON)
                SettingSwitchRow(
                    title = "Show Prediction",
                    checked = CatEngineConfig.showPrediction,
                    onCheckedChange = { CatEngineConfig.updateShowPrediction(context, it) }
                )

                // Target Coin Filter (Auto, White, Black, Any)
                TargetColorModeSelector(
                    selectedMode = CatEngineConfig.targetColorMode,
                    onModeSelected = { CatEngineConfig.updateTargetColorMode(context, it) }
                )

                // Show Pocket switch
                SettingSwitchRow(
                    title = "Show Pocket Guides",
                    checked = CatEngineConfig.showPocket,
                    onCheckedChange = { CatEngineConfig.updateShowPocket(context, it) }
                )

                // Show Table switch
                SettingSwitchRow(
                    title = "Show Table Border",
                    checked = CatEngineConfig.showTable,
                    onCheckedChange = { CatEngineConfig.updateShowTable(context, it) }
                )

                // Keep Line After Shot switch
                SettingSwitchRow(
                    title = "Keep Line After Shot",
                    checked = CatEngineConfig.keepLineAfterShot,
                    onCheckedChange = { CatEngineConfig.updateKeepLineAfterShot(context, it) }
                )

                // Fast Shot (First Found) switch
                SettingSwitchRow(
                    title = "Fast Shot (First Found)",
                    checked = CatEngineConfig.fastFirstShotMode,
                    onCheckedChange = { CatEngineConfig.updateFastFirstShotMode(context, it) }
                )


                Spacer(modifier = Modifier.height(4.dp))



                // Section 2: Visual Sliders (Blue themed)
                SectionTitle(title = "Geometry & Line Sliders")

                // Line Thickness Slider (1 to 10, default 4)
                SettingSliderRow(
                    title = "Line Thickness",
                    value = CatEngineConfig.lineThickness.toFloat(),
                    valueRange = 1f..10f,
                    steps = 8,
                    displayValue = "${CatEngineConfig.lineThickness}",
                    onValueChange = { CatEngineConfig.updateLineThickness(context, it.roundToInt()) }
                )

                // Line Transparency Slider (1 to 10)
                SettingSliderRow(
                    title = "Line Transparency",
                    value = CatEngineConfig.lineTransparency.toFloat(),
                    valueRange = 1f..10f,
                    steps = 8,
                    displayValue = "${CatEngineConfig.lineTransparency}",
                    onValueChange = { CatEngineConfig.updateLineTransparency(context, it.roundToInt()) }
                )

                // Pocket Size Slider
                SettingSliderRow(
                    title = "Pocket Size",
                    value = CatEngineConfig.pocketSize.toFloat(),
                    valueRange = 1f..10f,
                    steps = 8,
                    displayValue = "${CatEngineConfig.pocketSize}",
                    onValueChange = { CatEngineConfig.updatePocketSize(context, it.roundToInt()) }
                )

                Spacer(modifier = Modifier.height(4.dp))

                // Section 3: Visual Color Pickers
                SectionTitle(title = "Visual Color Customization")

                // Striker Color Picker
                VisualColorPickerRow(
                    title = "Striker Color",
                    selectedColor = CatEngineConfig.strikerColor,
                    onColorSelected = { CatEngineConfig.updateStrikerColor(context, it) }
                )

                // My Puck Color Picker
                VisualColorPickerRow(
                    title = "My Puck Color",
                    selectedColor = CatEngineConfig.myPuckColor,
                    onColorSelected = { CatEngineConfig.updateMyPuckColor(context, it) }
                )

                // Opponent Puck Color Picker
                VisualColorPickerRow(
                    title = "Opponent Puck Color",
                    selectedColor = CatEngineConfig.opponentPuckColor,
                    onColorSelected = { CatEngineConfig.updateOpponentPuckColor(context, it) }
                )

                Spacer(modifier = Modifier.height(4.dp))

                // Section 4: Diagnostics & Support
                SectionTitle(title = "Diagnostics & Support")

                SettingActionRow(
                    title = "Share Logs to Support",
                    subtitle = "Open Telegram (@catengine_owner) with log file attached",
                    buttonText = "Share Logs",
                    onClick = {
                        CatEngineLogManager.shareLogsToTelegram(context)
                    }
                )

                Spacer(modifier = Modifier.height(8.dp))
            }
        }
    }
}

/**
 * Top Header: Cat Engine Logo badge, Title and the Cross (X) Close button
 */
@Composable
private fun WindowHeader(
    onDismiss: () -> Unit
) {
    val logoBitmap: Bitmap? = remember {
        try {
            CatEngineLogoData.getLogoBitmap()
        } catch (ignored: Throwable) {
            null
        }
    }

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            // Cat Engine circular icon
            Box(
                modifier = Modifier
                    .size(38.dp)
                    .clip(CircleShape)
                    .background(Color(0xFF0A0C14))
                    .border(BorderStroke(1.5.dp, AccentBlue), CircleShape),
                contentAlignment = Alignment.Center
            ) {
                if (logoBitmap != null && !logoBitmap.isRecycled) {
                    Image(
                        bitmap = logoBitmap.asImageBitmap(),
                        contentDescription = "Cat Engine Logo",
                        modifier = Modifier
                            .fillMaxSize()
                            .clip(CircleShape)
                    )
                } else {
                    Text(
                        text = "🐱",
                        fontSize = 18.sp
                    )
                }
            }

            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                Text(
                    text = "Cat Engine",
                    color = TextPrimary,
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold
                )

                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(4.dp))
                        .background(AccentGreen.copy(alpha = 0.2f))
                        .border(BorderStroke(1.dp, AccentGreen.copy(alpha = 0.5f)), RoundedCornerShape(4.dp))
                        .padding(horizontal = 6.dp, vertical = 2.dp)
                ) {
                    Text(
                        text = "PRO",
                        color = AccentGreen,
                        fontSize = 10.sp,
                        fontWeight = FontWeight.Bold
                    )
                }
            }
        }

        // Cross (Close) Button at the top
        IconButton(
            onClick = onDismiss,
            modifier = Modifier
                .size(32.dp)
                .clip(CircleShape)
                .background(Color(0xFF1E2235))
        ) {
            Icon(
                imageVector = Icons.Default.Close,
                contentDescription = "Close Settings",
                tint = TextPrimary,
                modifier = Modifier.size(16.dp)
            )
        }
    }
}

@Composable
private fun SectionTitle(title: String) {
    Text(
        text = title.uppercase(),
        color = AccentBlue,
        fontSize = 11.sp,
        fontWeight = FontWeight.Bold,
        letterSpacing = 1.1.sp,
        modifier = Modifier.padding(start = 2.dp, top = 8.dp, bottom = 4.dp)
    )
}

/**
 * Reusable switch row component:
 * NO background, NO border, and NO description text.
 * Uses custom CatEngineSwitch matching user design.
 */
@Composable
private fun SettingSwitchRow(
    title: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Text(
            text = title,
            color = TextPrimary,
            fontSize = 15.sp,
            fontWeight = FontWeight.Medium
        )

        CatEngineSwitch(
            checked = checked,
            onCheckedChange = onCheckedChange
        )
    }
}

/**
 * Reusable slider row component:
 * NO background, NO border, and NO description text.
 * Styled with user's blue theme.
 */
@Composable
private fun SettingSliderRow(
    title: String,
    value: Float,
    valueRange: ClosedFloatingPointRange<Float>,
    steps: Int,
    displayValue: String,
    onValueChange: (Float) -> Unit
) {
    var localPos by remember(value) { mutableFloatStateOf(value) }

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Text(
                text = title,
                color = TextPrimary,
                fontSize = 14.sp,
                fontWeight = FontWeight.Medium
            )

            // Value Pill in Blue
            Box(
                modifier = Modifier
                    .clip(RoundedCornerShape(8.dp))
                    .background(Color(0xFF1E2235))
                    .padding(horizontal = 10.dp, vertical = 2.dp),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = "${localPos.roundToInt()}",
                    color = AccentBlue,
                    fontSize = 13.sp,
                    fontWeight = FontWeight.Bold
                )
            }
        }

        Spacer(modifier = Modifier.height(2.dp))

        Slider(
            value = localPos,
            onValueChange = { localPos = it },
            onValueChangeFinished = { onValueChange(localPos) },
            valueRange = valueRange,
            steps = steps,
            colors = SliderDefaults.colors(
                thumbColor = AccentBlue,
                activeTrackColor = AccentBlue,
                inactiveTrackColor = Color(0xFF282C40)
            ),
            modifier = Modifier.fillMaxWidth()
        )
    }
}

@Composable
private fun TargetColorModeSelector(
    selectedMode: Int,
    onModeSelected: (Int) -> Unit
) {
    val modes = listOf(
        0 to "Auto",
        1 to "White",
        2 to "Black",
        3 to "Any Coin"
    )

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp)
    ) {
        Text(
            text = "Target Coin Filter",
            color = TextPrimary,
            fontSize = 14.sp,
            fontWeight = FontWeight.Medium
        )
        Spacer(modifier = Modifier.height(6.dp))
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            modes.forEach { (modeId, label) ->
                val isSelected = (selectedMode == modeId)
                Box(
                    modifier = Modifier
                        .weight(1f)
                        .clip(RoundedCornerShape(8.dp))
                        .background(if (isSelected) AccentBlue else Color(0xFF1E2235))
                        .border(
                            BorderStroke(1.dp, if (isSelected) AccentBlue else Color(0xFF282C40)),
                            RoundedCornerShape(8.dp)
                        )
                        .clickable { onModeSelected(modeId) }
                        .padding(vertical = 8.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = label,
                        color = if (isSelected) Color(0xFF0F111D) else TextPrimary,
                        fontSize = 12.sp,
                        fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium
                    )
                }
            }
        }
    }
}

/**
 * Action row with title, description, and an action button styled with AccentBlue.
 */
@Composable
private fun SettingActionRow(
    title: String,
    subtitle: String,
    buttonText: String,
    onClick: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Column(modifier = Modifier.weight(1f).padding(end = 12.dp)) {
            Text(
                text = title,
                color = TextPrimary,
                fontSize = 15.sp,
                fontWeight = FontWeight.Medium
            )
            Spacer(modifier = Modifier.height(2.dp))
            Text(
                text = subtitle,
                color = TextSecondary,
                fontSize = 12.sp
            )
        }

        Box(
            modifier = Modifier
                .clip(RoundedCornerShape(8.dp))
                .background(AccentBlue)
                .clickable { onClick() }
                .padding(horizontal = 14.dp, vertical = 8.dp),
            contentAlignment = Alignment.Center
        ) {
            Text(
                text = buttonText,
                color = Color.Black,
                fontSize = 13.sp,
                fontWeight = FontWeight.Bold
            )
        }
    }
}


