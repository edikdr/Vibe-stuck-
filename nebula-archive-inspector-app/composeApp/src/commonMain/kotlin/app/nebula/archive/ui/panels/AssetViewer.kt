package app.nebula.archive.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import app.nebula.archive.ArchiveEntry
import app.nebula.archive.ArchiveKind
import app.nebula.archive.formatBytes

/**
 * Frame around a single non-text archive file. The chrome — identity, size, SVG options, copy and close —
 * is shared; only the surface that paints the file itself comes from the platform.
 */
@Composable
fun AssetViewer(
    state: WorkspaceState,
    platform: NebulaPlatform,
    entry: ArchiveEntry,
    modifier: Modifier = Modifier,
) {
    val strings = state.strings
    val copy = LocalCopyText.current
    Column(modifier.fillMaxSize().background(Space).padding(8.dp)) {
        Row(
            Modifier.fillMaxWidth().height(62.dp).clip(MaterialTheme.shapes.medium).background(PanelRaised).padding(horizontal = 10.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            MonoText(kindLabel(entry.kind), color = kindColor(entry.kind))
            Column(Modifier.weight(1f)) {
                SectionLabel(strings.filePreview)
                Text(
                    entry.path,
                    color = TextMain,
                    fontSize = 10.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                MonoText("${entry.extension.uppercase().ifBlank { "FILE" }} · ${formatBytes(entry.size)}", size = 8)
            }
            if (entry.isSvg) {
                SmallAction(if (state.svgFitToStage) strings.svgFit else strings.svgActualSize, state::toggleSvgFit, active = !state.svgFitToStage)
                SmallAction(if (state.svgLightBackground) "☀" else "☾", state::toggleSvgBackground, active = state.svgLightBackground)
            }
            SmallAction(strings.copyPath, { copy(entry.path) })
            SmallAction("×", state::closeOverlay)
        }
        if (entry.kind == ArchiveKind.Model) {
            MonoText(strings.modelOnlineNotice, size = 8, maxLines = 2, modifier = Modifier.padding(horizontal = 8.dp, vertical = 5.dp))
        }
        platform.previewHost.AssetPreview(
            state,
            entry,
            Modifier.fillMaxSize().clip(MaterialTheme.shapes.medium).border(1.dp, Border, MaterialTheme.shapes.medium),
        )
    }
}
