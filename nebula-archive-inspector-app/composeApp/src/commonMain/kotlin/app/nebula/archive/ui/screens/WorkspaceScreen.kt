package app.nebula.archive.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import app.nebula.archive.AppLocale
import app.nebula.archive.ArchiveProject
import app.nebula.archive.PreviewDevice
import app.nebula.archive.findElementSources
import app.nebula.archive.formatBytes
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext

/** A runtime test that produced nothing by now is never going to; the UI must not stay stuck on it. */
private const val PERFORMANCE_TIMEOUT_MS = 8_000L

/** Source lookup for a large group would scan the archive once per element; the rest keep their own panel. */
private const val MAX_GROUP_SOURCE_LOOKUPS = 8

/** Below this width the panel docks under the preview instead of beside it. */
private val WIDE_LAYOUT_BREAKPOINT = 840.dp

@Composable
fun WorkspaceScreen(
    project: ArchiveProject,
    projects: List<ArchiveProject>,
    locale: AppLocale,
    platform: NebulaPlatform,
    onSelectProject: (ArchiveProject) -> Unit,
    onOpenAnother: () -> Unit,
    onToggleLocale: () -> Unit,
    onClose: () -> Unit,
) {
    val state = remember(project.openedAtMs) { WorkspaceState(project, locale) }
    SideEffect { state.locale = locale }

    LaunchedEffect(state.inspected) {
        val elements = state.inspected
        if (elements.isEmpty()) return@LaunchedEffect
        state.sources = withContext(Dispatchers.Default) {
            elements.take(MAX_GROUP_SOURCE_LOOKUPS).associate { element ->
                element.id to findElementSources(project, element)
            }
        }
    }
    LaunchedEffect(state.performanceRun, state.performanceRunning) {
        if (!state.performanceRunning) return@LaunchedEffect
        val run = state.performanceRun
        delay(PERFORMANCE_TIMEOUT_MS)
        state.onPerformanceTimeout(run)
    }

    Column(
        Modifier.fillMaxSize().background(Space).windowInsetsPadding(WindowInsets.safeDrawing),
    ) {
        if (projects.size > 1) ProjectTabs(projects, project, onSelectProject)
        WorkspaceHeader(state, onClose, onOpenAnother, onToggleLocale)

        Box(Modifier.fillMaxWidth().weight(1f)) {
            BoxWithConstraints(Modifier.fillMaxSize()) {
                val wide = maxWidth >= WIDE_LAYOUT_BREAKPOINT
                val available = if (wide) maxWidth.value * .55f else maxHeight.value * .72f
                val maxExtent = available.coerceIn(MIN_PANEL_EXTENT, MAX_PANEL_EXTENT)
                SideEffect { state.panelExtentRange = MIN_PANEL_EXTENT..maxExtent }

                val panelModifier = if (wide) {
                    Modifier.width(state.panelExtent.dp).fillMaxHeight()
                } else {
                    Modifier.fillMaxWidth().height(state.panelExtent.dp)
                }
                if (wide) {
                    Row(Modifier.fillMaxSize()) {
                        PreviewPane(state, platform, Modifier.weight(1f).fillMaxHeight())
                        WorkspacePanelContent(state, wide = true, modifier = panelModifier)
                    }
                } else {
                    Column(Modifier.fillMaxSize()) {
                        PreviewPane(state, platform, Modifier.weight(1f).fillMaxWidth())
                        WorkspacePanelContent(state, wide = false, modifier = panelModifier)
                    }
                }
            }
            WorkspaceOverlays(state, platform)
        }
    }
}

@Composable
private fun WorkspacePanelContent(state: WorkspaceState, wide: Boolean, modifier: Modifier) {
    when (state.panel) {
        WorkspacePanel.Inspection -> InspectionPanel(state, wide, modifier)
        WorkspacePanel.Performance -> PerformancePanel(state, modifier)
        WorkspacePanel.Diagnostics -> DiagnosticsPanel(state, modifier)
        WorkspacePanel.None -> Unit
    }
}

@Composable
private fun BoxScope.WorkspaceOverlays(state: WorkspaceState, platform: NebulaPlatform) {
    when (val overlay = state.overlay) {
        null -> Unit
        is WorkspaceOverlay.Files -> {
            Box(Modifier.fillMaxSize().background(Color(0xAA000000)).clickable(onClick = state::closeOverlay))
            FileLibrary(
                state,
                Modifier.fillMaxWidth(.92f).widthIn(max = 390.dp).fillMaxHeight().align(Alignment.CenterStart),
            )
        }
        is WorkspaceOverlay.Source -> SourceViewer(state, overlay.hit, Modifier.fillMaxSize())
        is WorkspaceOverlay.Asset -> AssetViewer(state, platform, overlay.entry, Modifier.fillMaxSize())
    }
}

@Composable
private fun ProjectTabs(projects: List<ArchiveProject>, active: ArchiveProject, onSelect: (ArchiveProject) -> Unit) {
    Row(
        Modifier.fillMaxWidth().height(42.dp).background(Color(0xFF070B14))
            .horizontalScroll(rememberScrollState()).padding(horizontal = 6.dp, vertical = 5.dp),
        horizontalArrangement = Arrangement.spacedBy(6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        projects.forEachIndexed { index, project ->
            val selected = project.openedAtMs == active.openedAtMs
            Row(
                Modifier.clip(MaterialTheme.shapes.medium)
                    .background(if (selected) PanelSoft else Panel)
                    .border(1.dp, if (selected) Nebula else Border, MaterialTheme.shapes.medium)
                    .clickable { onSelect(project) }
                    .padding(horizontal = 10.dp, vertical = 6.dp),
                horizontalArrangement = Arrangement.spacedBy(7.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    "${index + 1}",
                    color = if (selected) Space else TextMuted,
                    fontSize = 8.sp,
                    fontWeight = FontWeight.Black,
                    modifier = Modifier.clip(MaterialTheme.shapes.small)
                        .background(if (selected) Nebula else PanelRaised)
                        .padding(horizontal = 5.dp, vertical = 2.dp),
                )
                Text(project.name, color = if (selected) TextMain else TextMuted, fontSize = 10.sp, maxLines = 1, overflow = TextOverflow.Ellipsis)
            }
        }
    }
}

@Composable
private fun WorkspaceHeader(
    state: WorkspaceState,
    onClose: () -> Unit,
    onOpenAnother: () -> Unit,
    onToggleLocale: () -> Unit,
) {
    val strings = state.strings
    Column(
        Modifier.fillMaxWidth()
            .background(Brush.horizontalGradient(listOf(Color(0xFF0A101D), Color(0xFF111A2D), Color(0xFF0A101D))))
            .border(1.dp, Color(0xFF1E2A41)),
    ) {
        Row(
            Modifier.fillMaxWidth().height(54.dp).padding(horizontal = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            SmallAction("‹", onClose)
            Box(
                Modifier.size(28.dp).clip(MaterialTheme.shapes.extraLarge)
                    .background(Brush.radialGradient(listOf(Color.White, Nebula, Violet, Color(0xFF10172A))))
                    .border(1.dp, Color(0x9979E9FF), MaterialTheme.shapes.extraLarge),
            )
            Column(Modifier.weight(1f)) {
                Text(state.project.name, color = TextMain, maxLines = 1, overflow = TextOverflow.Ellipsis, fontWeight = FontWeight.Bold)
                MonoText("${state.project.entries.size} files · ${formatBytes(state.project.totalBytes)}", size = 9)
            }
            SmallAction(strings.anotherZip, onOpenAnother)
        }
        Row(
            Modifier.fillMaxWidth().height(45.dp).horizontalScroll(rememberScrollState()).padding(horizontal = 8.dp, vertical = 5.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            ToolAction("☰", strings.files, state::openFiles)
            ToolAction(
                "⌖",
                if (state.inspecting) strings.selecting else strings.element,
                state::toggleInspecting,
                active = state.inspecting,
            )
            ToolAction("◴", strings.test + if (state.performanceRunning) "…" else "", state::startPerformanceTest, active = state.performanceRunning)
            ToolAction("文", strings.localeSwitch, onToggleLocale)
        }
    }
}

/** Preview toolbar, the platform preview surface, and the picking overlay drawn above it. */
@Composable
private fun PreviewPane(state: WorkspaceState, platform: NebulaPlatform, modifier: Modifier) {
    val strings = state.strings
    Column(modifier.background(Space).padding(1.dp)) {
        Row(
            Modifier.fillMaxWidth().height(39.dp).background(Color(0xFF090E18)).padding(horizontal = 6.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            SmallAction("‹", { state.preview.navigateBack() }, enabled = state.navigation.canGoBack)
            SmallAction("›", { state.preview.navigateForward() }, enabled = state.navigation.canGoForward)
            SmallAction("↻", { state.preview.reload() }, enabled = state.navigation.pageReady)
            MonoText(state.pagePath, modifier = Modifier.weight(1f).padding(horizontal = 5.dp))
            SmallAction(deviceLabel(state.device, state), state::cycleDevice, active = state.device.emulated)
            if (state.device.emulated) SmallAction("⟳", state::rotateDevice, active = state.landscape)
            SmallAction(if (state.onlineResources) "NET" else "OFF", state::toggleOnlineResources, active = state.onlineResources)
            SmallAction(if (state.issues.isEmpty()) "!" else "!${state.issues.size}", state::toggleDiagnostics, active = state.issues.isNotEmpty())
        }
        if (state.navigation.progress in 1..99) {
            LinearProgressIndicator(
                progress = { state.navigation.progress / 100f },
                modifier = Modifier.fillMaxWidth().height(2.dp),
                color = Nebula,
                trackColor = Color.Transparent,
            )
        }
        Box(Modifier.fillMaxWidth().weight(1f)) {
            platform.previewHost.Preview(state, Modifier.fillMaxSize())
            if (state.device.emulated) {
                MonoText(
                    "${state.device.viewportWidthDp(state.landscape)} × ${state.device.viewportHeightDp(state.landscape)} CSS px",
                    size = 8,
                    modifier = Modifier.align(Alignment.TopEnd).padding(6.dp)
                        .clip(MaterialTheme.shapes.small).background(Color(0xCC0B1424))
                        .padding(horizontal = 6.dp, vertical = 3.dp),
                )
            }
            // While picking, the selection travels with the preview so the "all at once" action stays in reach.
            if (state.inspecting) {
                Column(
                    Modifier.align(Alignment.BottomCenter).fillMaxWidth().padding(10.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    // The open inspection panel already carries the strip; showing it twice is just clutter.
                    if (state.selection.isNotEmpty() && state.panel != WorkspacePanel.Inspection) SelectionStrip(state)
                    Row(
                        Modifier.clip(MaterialTheme.shapes.medium).background(Color(0xEE0D1726))
                            .padding(horizontal = 14.dp, vertical = 10.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        Text("⌖", color = Nebula, fontWeight = FontWeight.Black)
                        Text(
                            if (state.selection.isEmpty()) strings.inspectorHelp else strings.selectionHelp,
                            color = TextMain,
                            fontSize = 11.sp,
                            fontWeight = FontWeight.Bold,
                        )
                    }
                }
            }
        }
    }
}

private fun deviceLabel(device: PreviewDevice, state: WorkspaceState) = when (device) {
    PreviewDevice.Auto -> state.strings.deviceFit
    PreviewDevice.Phone -> state.strings.devicePhone
    PreviewDevice.Tablet -> state.strings.deviceTablet
    PreviewDevice.Desktop -> state.strings.deviceDesktop
}
