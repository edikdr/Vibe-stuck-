package app.nebula.archive

/**
 * Inspector and preview contracts.
 *
 * The data classes describe what a platform preview reports back; [PreviewCommands] describes what the shared
 * UI may ask of it. A new platform only has to fill both sides — no shared UI code changes.
 */

/** Viewport emulated by the preview. [Auto] fills the surface, the others scale a real device viewport down. */
enum class PreviewDevice(val widthDp: Int, val heightDp: Int) {
    Auto(0, 0),
    Phone(390, 844),
    Tablet(834, 1_194),
    Desktop(1_280, 800);

    val emulated: Boolean get() = this != Auto

    fun viewportWidthDp(landscape: Boolean): Int = if (landscape) heightDp else widthDp

    fun viewportHeightDp(landscape: Boolean): Int = if (landscape) widthDp else heightDp

    fun next(): PreviewDevice = entries[(ordinal + 1) % entries.size]
}

data class ElementReference(
    val selector: String,
    val tag: String,
    val label: String,
    val depth: Int,
)

data class ElementProperty(val name: String, val value: String)

/** One entry of the running multi-selection: enough to list it without paying for a full inspection. */
data class SelectedElement(
    val id: Int,
    val selector: String,
    val tag: String,
    val label: String,
    val depth: Int,
    val childCount: Int,
    val width: Double,
    val height: Double,
) {
    val size: String get() = "${formatNumber(width)} × ${formatNumber(height)}"
}

/**
 * One layer of the stack under the point that was tapped, in painting order — the topmost first.
 *
 * The inspector never substitutes an ancestor for what was tapped, so when several layers overlap the whole
 * stack is offered and the user picks the one they meant, the way DevTools breadcrumbs work.
 */
data class ElementCandidate(
    val index: Int,
    val selector: String,
    val tag: String,
    val label: String,
    val depth: Int,
    val width: Double,
    val height: Double,
    /** 0 real content, 1 a section background, 2 a layer that paints nothing under the point. */
    val tier: Int,
    /** Carries `pointer-events:none`: decoration the page itself would never hand a click to. */
    val decor: Boolean,
    val selected: Boolean,
) {
    val size: String get() = "${formatNumber(width)} × ${formatNumber(height)}"

    /** Backgrounds and empty overlays are offered, but marked as the weaker guess they are. */
    val backdrop: Boolean get() = tier > 0
}

/** Full inspection of one element. [id] is its multi-selection number, or 0 when it is not part of one. */
data class InspectedElement(
    val selector: String,
    val tag: String,
    val id: Int = 0,
    val elementId: String = "",
    val classes: List<String> = emptyList(),
    val text: String = "",
    val outerHtml: String = "",
    val depth: Int = 0,
    val childCount: Int = 0,
    val width: Double = 0.0,
    val height: Double = 0.0,
    val left: Double = 0.0,
    val top: Double = 0.0,
    val attributes: List<ElementProperty> = emptyList(),
    val styles: List<ElementProperty> = emptyList(),
    val parents: List<ElementReference> = emptyList(),
    val children: List<ElementReference> = emptyList(),
    val siblings: List<ElementReference> = emptyList(),
) {
    val size: String get() = "${formatNumber(width)} × ${formatNumber(height)}"
    val position: String get() = "x ${formatNumber(left)} · y ${formatNumber(top)}"
    val reference: ElementReference get() = ElementReference(selector, tag, text.ifBlank { selector }, depth)
}

enum class PreviewIssueKind { JavaScript, MissingFile, Network, Blocked }

data class PreviewIssue(
    val kind: PreviewIssueKind,
    val message: String,
    val resource: String,
)

data class PreviewNavigationState(
    val canGoBack: Boolean = false,
    val canGoForward: Boolean = false,
    val path: String = "",
    val progress: Int = 0,
    val pageReady: Boolean = false,
)

data class SourceHit(val path: String, val line: Int, val excerpt: String)

/**
 * Commands the shared workspace sends to whatever renders the archived page.
 *
 * Android implements this with a `WebView`; a desktop adapter can implement it with an embedded browser
 * without touching the shared UI.
 */
interface PreviewCommands {
    fun navigateBack()
    fun navigateForward()
    fun reload()
    fun setInspectorEnabled(enabled: Boolean)
    fun setOnlineResourcesEnabled(enabled: Boolean)

    /** Loads one page of the archive by its entry path; used to walk the site during a full test. */
    fun openPage(path: String)

    /** Applies a simulated network profile to everything served from the archive. */
    fun setNetworkProfile(profile: NetworkProfile)

    /** Picks one layer of the reported stack instead of the topmost one. */
    fun pickCandidate(index: Int)

    /** Outlines a candidate in the page while the user moves through the stack. */
    fun highlightCandidate(index: Int)

    /** Drops the reported stack and its highlight. */
    fun dismissCandidates()

    /** Inspects the element matching [selector] without adding it to the multi-selection. */
    fun inspectSelector(selector: String)

    /** Re-inspects one member of the multi-selection. */
    fun inspectSelected(id: Int)
    fun dropSelected(id: Int)
    fun clearSelection()

    /** Asks for a full inspection of every selected element at once. */
    fun inspectWholeSelection()

    fun runPerformanceTest(): Boolean
    fun cancelPerformanceTest()

    companion object {
        /** Used before a preview is attached, and by platforms without a live preview. */
        val Unavailable: PreviewCommands = object : PreviewCommands {
            override fun navigateBack() = Unit
            override fun navigateForward() = Unit
            override fun reload() = Unit
            override fun setInspectorEnabled(enabled: Boolean) = Unit
            override fun setOnlineResourcesEnabled(enabled: Boolean) = Unit
            override fun openPage(path: String) = Unit
            override fun setNetworkProfile(profile: NetworkProfile) = Unit
            override fun pickCandidate(index: Int) = Unit
            override fun highlightCandidate(index: Int) = Unit
            override fun dismissCandidates() = Unit
            override fun inspectSelector(selector: String) = Unit
            override fun inspectSelected(id: Int) = Unit
            override fun dropSelected(id: Int) = Unit
            override fun clearSelection() = Unit
            override fun inspectWholeSelection() = Unit
            override fun runPerformanceTest() = false
            override fun cancelPerformanceTest() = Unit
        }
    }
}
