package top.niunaijun.blackbox.widget

import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

// Custom switch colors matching user assets:
// OFF track: Rectangle 1.png (Grey #A1A1A1)
// ON track: Rectangle 3.png (Vibrant Electric Blue #00BBFF)
// Knob: Rectangle 2/4.png (Solid Pure White #FFFFFF)
private val SwitchTrackOffColor = Color(0xFFA1A1A1)
private val SwitchTrackOnColor = Color(0xFF00BBFF)
private val SwitchThumbColor = Color(0xFFFFFFFF)

/**
 * Custom toggle switch matching user's design assets:
 * Smooth rounded pill track with animated sliding white circular thumb.
 */
@Composable
fun CatEngineSwitch(
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
    width: Dp = 52.dp,
    height: Dp = 28.dp,
    thumbSize: Dp = 22.dp,
    padding: Dp = 3.dp
) {
    val interactionSource = remember { MutableInteractionSource() }

    // Animate track color between Grey and Blue
    val trackColor by animateColorAsState(
        targetValue = if (checked) SwitchTrackOnColor else SwitchTrackOffColor,
        animationSpec = tween(durationMillis = 200),
        label = "switchTrackColor"
    )

    // Calculate maximum travel distance for thumb
    val maxOffset = width - thumbSize - (padding * 2)

    // Animate thumb horizontal glide
    val thumbOffset by animateDpAsState(
        targetValue = if (checked) maxOffset else 0.dp,
        animationSpec = spring(
            dampingRatio = 0.82f,
            stiffness = Spring.StiffnessMediumLow
        ),
        label = "switchThumbOffset"
    )

    Box(
        modifier = modifier
            .size(width = width, height = height)
            .clip(RoundedCornerShape(height / 2))
            .background(trackColor)
            .clickable(
                interactionSource = interactionSource,
                indication = null
            ) {
                onCheckedChange(!checked)
            }
            .padding(padding),
        contentAlignment = Alignment.CenterStart
    ) {
        // Circular white knob sliding horizontally
        Box(
            modifier = Modifier
                .offset(x = thumbOffset)
                .size(thumbSize)
                .clip(CircleShape)
                .background(SwitchThumbColor)
        )
    }
}
