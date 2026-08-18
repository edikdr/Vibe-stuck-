package app.nebula.archive.ui

import androidx.compose.runtime.Stable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import app.nebula.archive.AppLocale
import app.nebula.archive.AppStrings
import app.nebula.archive.ArchiveEntry
import app.nebula.archive.ArchiveKind
import app.nebula.archive.ArchiveProject
import app.nebula.archive.InspectedElement
import app.nebula.archive.NetworkProfile
import app.nebula.archive.PagePerformance
import app.nebula.archive.PerformanceFinding
import app.nebula.archive.PreviewCommands
import app.nebula.archive.PreviewDevice
import app.nebula.archive.PreviewIssue
import app.nebula.archive.PreviewNavigationState
import app.nebula.archive.RuntimeMetrics
import app.nebula.archive.RuntimePerformanceReport
import app.nebula.archive.SelectedElement
import app.nebula.archive.SitePerformance
import app.nebula.archive.SourceHit
import app.nebula.archive.evaluateRuntimePerformance
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.filterNotNull
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull

/** The side panel currently attached to the preview. Only one can be open at a time. */
enum class WorkspacePanel { None, Inspection, Performance, Diagnostics }

/** A full-surface layer above the workspace. */
sealed interface WorkspaceOverlay {
    data object Files : WorkspaceOverlay
    data object Diff : WorkspaceOverlay
    data class Source(val hit: SourceHit) : WorkspaceOverlay
    data class Asset(val entry: ArchiveEntry) : WorkspaceOverlay
}

private const val MAX_ISSUES = 80
private const val DEFAULT_PANEL_EXTENT = 320f
const val MIN_PANEL_EXTENT = 210f
const val MAX_PANEL_EXTENT = 620f

/** A run that produced nothing by now never will; the UI must not stay stuck on it. */
private const val MEASURE_TIMEOUT_MS = 9_000L

/** Waiting for a page to finish loading before measuring it. */
private const val PAGE_LOAD_TIMEOUT_MS = 15_000L

/** A 200-page archive would take half an hour; the first pages are the ones that matter. */
private const val MAX_TESTED_PAGES = 20

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
    private val scope: CoroutineScope,
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
    var networkProfile: NetworkProfile by mutableStateOf(NetworkProfile.Unthrottled)
        private set
    var device: PreviewDevice by mutableStateOf(PreviewDevice.Auto)
        private set
    var landscape: Boolean by mutableStateOf(false)
        private set

    // ---- performance ------------------------------------------------------------------------------------

    /** Result of the whole run: one page for a single measurement, several after a site test. */
    var site: SitePerformance? by mutableStateOf(null)
        private set
    var testing: Boolean by mutableStateOf(false)
        private set
    var testProgress: Int by mutableStateOf(0)
        private set
    var testTotal: Int by mutableStateOf(0)
        private set
    var testFailed: Boolean by mutableStateOf(false)
        private set

    /** Handoff from the preview bridge to the measuring coroutine. */
    private var pendingReport: RuntimePerformanceReport? by mutableStateOf(null)
    private var testJob: Job? = null

    /** Which measured page the panel is showing the details of. */
    var openPageResult: PagePerformance? by mutableStateOf(null)
        private set

    /** True when the archive is a single HTML document, so a site-wide run is meaningless. */
    val singlePage: Boolean get() = htmlPages().size <= 1

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
        stopTest()
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
    }

    fun inspectSelected(id: Int) = preview.inspectSelected(id)

    /** Also used by a performance finding to jump straight to the element that causes it. */
    fun inspectSelector(selector: String) {
        if (selector.isBlank()) return
        panel = WorkspacePanel.Inspection
        preview.inspectSelector(selector)
    }

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
        // A test drives navigation itself; resetting on its own page loads would cancel the run.
        if (!changedPage || testing) return
        // Selectors and source hits describe the page that was just replaced.
        stopInspecting()
        selection = emptyList()
        clearInspection()
        if (panel == WorkspacePanel.Inspection) panel = WorkspacePanel.None
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

    /** Cycles the simulated network. Applies to normal browsing too, not just to a test run. */
    fun cycleNetworkProfile() {
        networkProfile = networkProfile.next()
        preview.setNetworkProfile(networkProfile)
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
            stopTest()
            clearInspection()
            WorkspacePanel.Diagnostics
        }
    }

    fun closePanel() {
        if (panel == WorkspacePanel.Performance) stopTest()
        if (panel == WorkspacePanel.Inspection) clearInspection()
        panel = WorkspacePanel.None
    }

    fun openFiles() {
        overlay = WorkspaceOverlay.Files
    }

    fun openDiff() {
        overlay = WorkspaceOverlay.Diff
    }

    /** Opens an archive entry in the reader that fits it: formatted source, or a visual preview. */
    fun openEntry(entry: ArchiveEntry) {
        overlay = if (entry.isSourceText) {
            WorkspaceOverlay.Source(SourceHit(entry.path, 1, ""))
        } else {
            WorkspaceOverlay.Asset(entry)
        }
    }

    fun openSource(hit: SourceHit) {
        overlay = WorkspaceOverlay.Source(hit)
    }

    fun closeOverlay() {
        overlay = null
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

    // ---- performance test -------------------------------------------------------------------------------

    /** Called by the preview bridge when the in-page measurement finishes. */
    fun onPerformanceResult(metrics: RuntimeMetrics, findings: List<PerformanceFinding>) {
        pendingReport = evaluateRuntimePerformance(metrics, findings, networkProfile)
    }

    /** Measures the page that is open right now. */
    fun startPageTest() = launchTest(wholeSite = false)

    /** Walks every HTML page of the archive and measures each one. */
    fun startSiteTest() = launchTest(wholeSite = true)

    fun stopTest() {
        testJob?.cancel()
        testJob = null
        testing = false
        preview.cancelPerformanceTest()
    }

    fun showPageResult(page: PagePerformance?) {
        openPageResult = page
    }

    private fun launchTest(wholeSite: Boolean) {
        testJob?.cancel()
        stopInspecting()
        clearInspection()
        selection = emptyList()
        preview.clearSelection()
        overlay = null
        panel = WorkspacePanel.Performance
        site = null
        openPageResult = null
        testFailed = false
        testJob = scope.launch { runTest(wholeSite) }
    }

    private suspend fun runTest(wholeSite: Boolean) {
        val startPath = pagePath
        val pages = if (wholeSite) htmlPages() else listOf(startPath)
        testing = true
        testProgress = 0
        testTotal = pages.size
        val measured = mutableListOf<PagePerformance>()
        try {
            pages.forEachIndexed { index, path ->
                testProgress = index + 1
                if (wholeSite || path != startPath) {
                    preview.openPage(path)
                    if (!awaitPageReady(path)) return@forEachIndexed
                }
                val report = measureCurrentPage() ?: return@forEachIndexed
                measured += PagePerformance(path, report)
            }
            site = SitePerformance(measured, networkProfile)
            openPageResult = measured.firstOrNull()
            testFailed = measured.isEmpty()
        } finally {
            testing = false
            if (wholeSite && startPath.isNotBlank() && pagePath != startPath) {
                // Put the user back where they were before the run took over navigation.
                preview.openPage(startPath)
            }
        }
    }

    private suspend fun awaitPageReady(path: String): Boolean = withTimeoutOrNull(PAGE_LOAD_TIMEOUT_MS) {
        snapshotFlow { navigation }
            .filterNotNull()
            .first { it.pageReady && (it.path == path || it.path.isBlank()) }
        true
    } ?: false

    private suspend fun measureCurrentPage(): RuntimePerformanceReport? {
        pendingReport = null
        if (!preview.runPerformanceTest()) return null
        return withTimeoutOrNull(MEASURE_TIMEOUT_MS) {
            snapshotFlow { pendingReport }.filterNotNull().first()
        }.also { if (it == null) preview.cancelPerformanceTest() }
    }

    private fun htmlPages(): List<String> = project.entries
        .filter { it.kind == ArchiveKind.Html }
        .map { it.path }
        .sortedBy { if (it == project.entryPoint) 0 else 1 }
        .take(MAX_TESTED_PAGES)
}
