package app.nebula.archive.ui

import androidx.compose.runtime.Stable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import app.nebula.archive.AppLocale
import app.nebula.archive.AppStrings
import app.nebula.archive.ArchiveEntry
import app.nebula.archive.ArchiveProject
import app.nebula.archive.InspectedElement
import app.nebula.archive.PreviewCommands
import app.nebula.archive.PreviewDevice
import app.nebula.archive.PreviewIssue
import app.nebula.archive.PreviewNavigationState
import app.nebula.archive.RuntimeMetrics
import app.nebula.archive.RuntimePerformanceReport
import app.nebula.archive.SelectedElement
import app.nebula.archive.SourceHit
import app.nebula.archive.evaluateRuntimePerformance

/** The side panel currently attached to the preview. Only one can be open at a time. */
enum class WorkspacePanel { None, Inspection, Performance, Diagnostics }

/** A full-surface layer above the workspace. */
sealed interface WorkspaceOverlay {
    data object Files : WorkspaceOverlay
    data class Source(val hit: SourceHit) : WorkspaceOverlay
    data class Asset(val entry: ArchiveEntry) : WorkspaceOverlay
}

private const val MAX_ISSUES = 80
private const val DEFAULT_PANEL_EXTENT = 320f
const val MIN_PANEL_EXTENT = 210f
const val MAX_PANEL_EXTENT = 620f

/**
 * All workspace state for one open project, and the rules that keep it consistent.
 *
 * Panels are mutually exclusive, opening one has to stop whatever the previous one was doing, and a page
 * navigation invalidates every inspection. Keeping those rules here — instead of repeating them in each
 * callback of each layout — is what lets the phone and the wide layout share one behaviour, and what a
 * desktop shell will reuse unchanged.
 */
@Stable
class WorkspaceState(
    val project: ArchiveProject,
    locale: AppLocale,
) {
    /** Switching language must not reset the workspace, so the locale lives in the state. */
    var locale: AppLocale by mutableStateOf(locale)

    val strings: AppStrings get() = AppStrings.forLocale(locale)

    /** Set by the platform preview host once its surface exists. */
    var preview: PreviewCommands by mutableStateOf(PreviewCommands.Unavailable)

    var navigation: PreviewNavigationState by mutableStateOf(PreviewNavigationState())
        private set
    var inspecting: Boolean by mutableStateOf(false)
        private set

    /** Elements picked but not yet inspected, in the order they were tapped. */
    var selection: List<SelectedElement> by mutableStateOf(emptyList())
        private set

    /** Inspected elements: one element for a single inspection, several for a group. */
    var inspected: List<InspectedElement> by mutableStateOf(emptyList())
        private set
    var sources: Map<Int, List<SourceHit>> by mutableStateOf(emptyMap())

    var panel: WorkspacePanel by mutableStateOf(WorkspacePanel.None)
        private set
    var overlay: WorkspaceOverlay? by mutableStateOf(null)
        private set
    var issues: List<PreviewIssue> by mutableStateOf(emptyList())
        private set

    var onlineResources: Boolean by mutableStateOf(true)
        private set
    var device: PreviewDevice by mutableStateOf(PreviewDevice.Auto)
        private set
    var landscape: Boolean by mutableStateOf(false)
        private set

    var performanceReport: RuntimePerformanceReport? by mutableStateOf(null)
        private set
    var performanceRunning: Boolean by mutableStateOf(false)
        private set
    var performanceUnavailable: Boolean by mutableStateOf(false)
        private set

    /** Incremented on every start and every cancel, so a late result of an old run is ignored. */
    var performanceRun: Int by mutableStateOf(0)
        private set

    /** Panel height on phones, panel width on wide screens. */
    private var requestedPanelExtent: Float by mutableStateOf(DEFAULT_PANEL_EXTENT)

    /** Limits of the current layout; set by the workspace once it knows the available surface. */
    var panelExtentRange: ClosedFloatingPointRange<Float> by mutableStateOf(MIN_PANEL_EXTENT..DEFAULT_PANEL_EXTENT)

    val panelExtent: Float get() = requestedPanelExtent.coerceIn(panelExtentRange)

    fun setPanelExtent(value: Float) {
        requestedPanelExtent = value.coerceIn(panelExtentRange)
    }

    fun resizePanelBy(delta: Float) = setPanelExtent(panelExtent + delta)

    val pagePath: String get() = navigation.path.ifBlank { project.entryPoint }
    val isGroupInspection: Boolean get() = inspected.size > 1

    // ---- picking ----------------------------------------------------------------------------------------

    fun startInspecting() {
        clearInspection()
        closePerformance()
        panel = WorkspacePanel.None
        overlay = null
        inspecting = true
        preview.setInspectorEnabled(true)
    }

    fun stopInspecting() {
        inspecting = false
        preview.setInspectorEnabled(false)
    }

    fun toggleInspecting() = if (inspecting) stopInspecting() else startInspecting()

    fun onSelectionChanged(elements: List<SelectedElement>) {
        selection = elements
    }

    fun onInspected(elements: List<InspectedElement>) {
        if (elements.isEmpty()) return
        inspected = elements
        sources = emptyMap()
        panel = WorkspacePanel.Inspection
        performanceRunning = false
        performanceUnavailable = false
    }

    fun inspectSelected(id: Int) = preview.inspectSelected(id)

    fun inspectSelector(selector: String) = preview.inspectSelector(selector)

    fun inspectWholeSelection() = preview.inspectWholeSelection()

    fun dropSelected(id: Int) {
        preview.dropSelected(id)
        selection = selection.filterNot { it.id == id }
        inspected = inspected.filterNot { it.id == id }
        if (inspected.isEmpty() && panel == WorkspacePanel.Inspection) panel = WorkspacePanel.None
    }

    fun clearSelection() {
        preview.clearSelection()
        selection = emptyList()
        clearInspection()
        if (panel == WorkspacePanel.Inspection) panel = WorkspacePanel.None
    }

    fun closeInspection() {
        clearInspection()
        if (panel == WorkspacePanel.Inspection) panel = WorkspacePanel.None
    }

    private fun clearInspection() {
        inspected = emptyList()
        sources = emptyMap()
    }

    // ---- preview ----------------------------------------------------------------------------------------

    fun onNavigation(next: PreviewNavigationState) {
        val changedPage = navigation.path.isNotBlank() && navigation.path != next.path
        navigation = next
        if (!changedPage) return
        // Selectors, source hits and measurements all describe the page that was just replaced.
        stopInspecting()
        selection = emptyList()
        clearInspection()
        closePerformance()
        if (panel != WorkspacePanel.Diagnostics) panel = WorkspacePanel.None
    }

    fun onIssue(issue: PreviewIssue) {
        issues = (issues.filterNot { it == issue } + issue).takeLast(MAX_ISSUES)
    }

    fun clearIssues() {
        issues = emptyList()
    }

    fun toggleOnlineResources() {
        onlineResources = !onlineResources
        preview.setOnlineResourcesEnabled(onlineResources)
    }

    fun cycleDevice() {
        device = device.next()
    }

    fun rotateDevice() {
        landscape = !landscape
    }

    // ---- panels and overlays ----------------------------------------------------------------------------

    fun toggleDiagnostics() {
        panel = if (panel == WorkspacePanel.Diagnostics) {
            WorkspacePanel.None
        } else {
            stopInspecting()
            closePerformance()
            clearInspection()
            WorkspacePanel.Diagnostics
        }
    }

    fun startPerformanceTest() {
        stopInspecting()
        clearInspection()
        selection = emptyList()
        preview.clearSelection()
        overlay = null
        panel = WorkspacePanel.Performance
        performanceReport = null
        performanceRun++
        if (preview.runPerformanceTest()) {
            performanceRunning = true
            performanceUnavailable = false
        } else {
            performanceRunning = false
            performanceUnavailable = true
        }
    }

    /** Called by the screen when a started run produced no result in time. */
    fun onPerformanceTimeout(run: Int) {
        if (!performanceRunning || run != performanceRun) return
        performanceRunning = false
        performanceUnavailable = true
        preview.cancelPerformanceTest()
    }

    fun onPerformanceResult(metrics: RuntimeMetrics) {
        if (panel != WorkspacePanel.Performance) return
        performanceReport = evaluateRuntimePerformance(project, metrics, locale)
        performanceRunning = false
        performanceUnavailable = false
    }

    fun closePerformance() {
        performanceRun++
        performanceRunning = false
        performanceUnavailable = false
        performanceReport = null
        preview.cancelPerformanceTest()
        if (panel == WorkspacePanel.Performance) panel = WorkspacePanel.None
    }

    fun closePanel() = when (panel) {
        WorkspacePanel.Performance -> closePerformance()
        WorkspacePanel.Inspection -> closeInspection()
        else -> panel = WorkspacePanel.None
    }

    fun openFiles() {
        overlay = WorkspaceOverlay.Files
    }

    /** Opens an archive entry in the reader that fits it: formatted source, or a visual preview. */
    fun openEntry(entry: ArchiveEntry) {
        overlay = if (entry.isSourceText) {
            WorkspaceOverlay.Source(SourceHit(entry.path, 1, ""))
        } else {
            WorkspaceOverlay.Asset(entry)
        }
    }

    // ---- SVG viewer options (shared state, applied by the platform preview) -----------------------------

    var svgLightBackground: Boolean by mutableStateOf(false)
        private set
    var svgFitToStage: Boolean by mutableStateOf(true)
        private set

    fun toggleSvgBackground() {
        svgLightBackground = !svgLightBackground
    }

    fun toggleSvgFit() {
        svgFitToStage = !svgFitToStage
    }

    fun openSource(hit: SourceHit) {
        overlay = WorkspaceOverlay.Source(hit)
    }

    fun closeOverlay() {
        overlay = null
    }
}
