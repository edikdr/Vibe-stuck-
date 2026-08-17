package app.nebula.archive.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import app.nebula.archive.ArchiveKind
import app.nebula.archive.PreviewIssueKind

val Space = Color(0xFF05070E)
val Panel = Color(0xFF0B101C)
val PanelRaised = Color(0xFF111827)
val PanelSoft = Color(0xFF182238)
val Nebula = Color(0xFF79E9FF)
val Violet = Color(0xFF9076FF)
val Green = Color(0xFF66E8BD)
val Amber = Color(0xFFFFD479)
val Coral = Color(0xFFFF9E9E)
val TextMain = Color(0xFFEAF1FF)
val TextMuted = Color(0xFF8695AE)
val TextFaint = Color(0xFF53627A)
val Border = Color(0xFF26334C)
val CodeBackground = Color(0xFF070B13)
val CodeText = Color(0xFFC9D6E9)

@Composable
fun NebulaTheme(content: @Composable () -> Unit) = MaterialTheme(
    colorScheme = darkColorScheme(
        primary = Nebula,
        secondary = Violet,
        background = Space,
        surface = Panel,
        onSurface = TextMain,
    ),
    content = content,
)

/** Colour of a DOM branch at [depth]; the preview overlay uses the same wheel, so panel and page agree. */
fun domDepthColor(depth: Int): Color = DomDepthColors[depth.coerceAtLeast(0) % DomDepthColors.size]

private val DomDepthColors = listOf(
    Nebula,
    Violet,
    Green,
    Amber,
    Color(0xFFFF8FB3),
    Color(0xFF6FA8FF),
    Color(0xFFD78CFF),
)

fun gradeColor(score: Int): Color = when {
    score >= 90 -> Green
    score >= 75 -> Nebula
    score >= 60 -> Amber
    else -> Coral
}

fun issueColor(kind: PreviewIssueKind): Color = when (kind) {
    PreviewIssueKind.JavaScript -> Amber
    PreviewIssueKind.MissingFile -> Coral
    PreviewIssueKind.Network -> Color(0xFFFFB36B)
    PreviewIssueKind.Blocked -> Violet
}

fun kindLabel(kind: ArchiveKind): String = when (kind) {
    ArchiveKind.Html -> "<>"
    ArchiveKind.Css -> "CSS"
    ArchiveKind.JavaScript -> "JS"
    ArchiveKind.Json -> "{}"
    ArchiveKind.Image -> "IMG"
    ArchiveKind.Media -> "MED"
    ArchiveKind.Font -> "FNT"
    ArchiveKind.Model -> "3D"
    ArchiveKind.Text -> "TXT"
    ArchiveKind.Other -> "•"
}

fun kindColor(kind: ArchiveKind): Color = when (kind) {
    ArchiveKind.Html -> Color(0xFFFFA078)
    ArchiveKind.Css -> Color(0xFF72CAED)
    ArchiveKind.JavaScript -> Color(0xFFE9D66C)
    ArchiveKind.Image -> Color(0xFFBC91EE)
    ArchiveKind.Media -> Color(0xFFE681A1)
    else -> TextMuted
}
