package app.nebula.archive.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/** Copies text to the platform clipboard and confirms it. Provided by the platform shell. */
val LocalCopyText = staticCompositionLocalOf<(String) -> Unit> { {} }

@Composable
fun SmallAction(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    active: Boolean = false,
    enabled: Boolean = true,
) {
    Box(
        modifier.clip(MaterialTheme.shapes.small)
            .background(if (active) Color(0xFF16445A) else if (enabled) PanelRaised else Color(0xFF0D1320))
            .border(1.dp, if (active) Color(0xAA79E9FF) else Color(0xFF24314A), MaterialTheme.shapes.small)
            .clickable(enabled = enabled) { onClick() }
            .padding(7.dp),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text,
            color = if (active) Nebula else if (enabled) TextMain else Color(0xFF4E5B70),
            fontSize = 9.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            maxLines = 1,
        )
    }
}

@Composable
fun ToolAction(
    icon: String,
    label: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    active: Boolean = false,
    accent: Color = Violet,
) {
    Row(
        modifier.clip(MaterialTheme.shapes.medium)
            .background(
                if (active) {
                    Brush.horizontalGradient(listOf(Color(0xFF164A5B), Color(0xFF24325A)))
                } else {
                    Brush.horizontalGradient(listOf(PanelRaised, Color(0xFF0E1625)))
                },
            )
            .border(1.dp, if (active) Color(0xAA79E9FF) else Border, MaterialTheme.shapes.medium)
            .clickable { onClick() }
            .padding(horizontal = 11.dp, vertical = 7.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Text(icon, color = if (active) Nebula else accent, fontSize = 11.sp, fontWeight = FontWeight.Black)
        Text(label, color = TextMain, fontSize = 9.sp, fontWeight = FontWeight.Bold, fontFamily = FontFamily.Monospace, maxLines = 1)
    }
}

@Composable
fun SectionLabel(text: String, modifier: Modifier = Modifier, color: Color = Nebula) {
    Text(
        text,
        color = color,
        fontSize = 9.sp,
        fontFamily = FontFamily.Monospace,
        letterSpacing = 1.sp,
        maxLines = 1,
        overflow = TextOverflow.Ellipsis,
        modifier = modifier,
    )
}

@Composable
fun MonoText(
    text: String,
    modifier: Modifier = Modifier,
    color: Color = TextMuted,
    size: Int = 9,
    maxLines: Int = 1,
) {
    Text(
        text,
        color = color,
        fontSize = size.sp,
        fontFamily = FontFamily.Monospace,
        maxLines = maxLines,
        overflow = TextOverflow.Ellipsis,
        modifier = modifier,
    )
}

/** One `name: value` line used by the computed-style and attribute cards. */
@Composable
fun PropertyRow(name: String, value: String, accent: Color = Nebula) {
    Row(
        Modifier.fillMaxWidth().padding(vertical = 2.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalAlignment = Alignment.Top,
    ) {
        MonoText(name, color = accent, modifier = Modifier.width(84.dp), maxLines = 2)
        Text(
            value,
            color = CodeText,
            fontSize = 9.sp,
            lineHeight = 13.sp,
            fontFamily = FontFamily.Monospace,
            maxLines = 3,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.weight(1f),
        )
    }
}

@Composable
fun MetricRow(label: String, value: String) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
        Text(label, color = TextMuted, fontSize = 10.sp)
        Text(value, color = TextMain, fontSize = 10.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
    }
}

@Composable
fun Card(
    modifier: Modifier = Modifier,
    borderColor: Color = Color(0xFF1C2940),
    content: @Composable () -> Unit,
) {
    Column(
        modifier.fillMaxWidth().clip(MaterialTheme.shapes.small).background(CodeBackground)
            .border(1.dp, borderColor, MaterialTheme.shapes.small).padding(10.dp),
    ) {
        content()
    }
}

@Composable
fun CodeCard(title: String, code: String, actions: List<Pair<String, () -> Unit>> = emptyList()) {
    Card {
        SectionLabel(title)
        Text(
            code,
            color = CodeText,
            fontSize = 9.sp,
            lineHeight = 14.sp,
            fontFamily = FontFamily.Monospace,
            modifier = Modifier.padding(top = 6.dp).horizontalScroll(rememberScrollState()),
        )
        if (actions.isNotEmpty()) {
            Row(Modifier.horizontalScroll(rememberScrollState()).padding(top = 8.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                actions.forEach { (label, action) -> SmallAction(label, action) }
            }
        }
    }
}

/** Numbered badge matching the marks drawn over the preview. */
@Composable
fun SelectionBadge(number: Int, color: Color, selected: Boolean = false) {
    Box(
        Modifier.size(18.dp).clip(MaterialTheme.shapes.extraLarge)
            .background(if (selected) color else color.copy(alpha = .18f))
            .border(1.dp, color, MaterialTheme.shapes.extraLarge),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            number.toString(),
            color = if (selected) Space else color,
            fontSize = 9.sp,
            fontWeight = FontWeight.Black,
            fontFamily = FontFamily.Monospace,
        )
    }
}

/**
 * Grab handle that resizes the panel by drag. [horizontal] flips the axis: a panel docked to the side of a
 * wide screen is resized by width, one docked to the bottom of a phone by height.
 */
@Composable
fun ResizeHandle(horizontal: Boolean, onResizeBy: (Float) -> Unit) {
    val density = LocalDensity.current.density
    Box(
        Modifier.fillMaxWidth().height(18.dp).pointerInput(horizontal, density) {
            detectDragGestures { change, dragAmount ->
                change.consume()
                onResizeBy((if (horizontal) -dragAmount.x else -dragAmount.y) / density)
            }
        },
        contentAlignment = Alignment.Center,
    ) {
        MonoText(if (horizontal) "↔ ━━━━━" else "↕ ━━━━━", color = Color(0xFF526582))
    }
}
