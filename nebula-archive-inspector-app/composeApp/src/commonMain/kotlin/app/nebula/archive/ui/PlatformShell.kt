package app.nebula.archive.ui

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import app.nebula.archive.ArchiveEntry
import app.nebula.archive.ArchiveOpener
import app.nebula.archive.RecentDocumentsStore

/**
 * Everything the shared UI needs from a platform. Implementing these four members is the whole cost of
 * bringing the inspector to a new target — no shared code is duplicated or overridden.
 */
interface NebulaPlatform {
    val opener: ArchiveOpener
    val recentDocuments: RecentDocumentsStore
    val documentPicker: DocumentPicker
    val previewHost: PreviewHost
    /** Copies [text] and confirms it with [confirmation], already translated by the caller. */
    fun copyToClipboard(text: String, confirmation: String)
}

interface DocumentPicker {
    /** Opens the system file picker. [onPicked] receives platform document references. */
    fun pick(onPicked: (List<String>) -> Unit)
}

/**
 * Renders archived content. The Android implementation drives a `WebView`; a desktop implementation can drive
 * an embedded browser, and until one exists [NoPreviewHost] keeps every other tool usable.
 */
interface PreviewHost {
    val supportsLivePreview: Boolean

    /** Live page of the project, wired to [WorkspaceState]. */
    @Composable
    fun Preview(state: WorkspaceState, modifier: Modifier)

    /** Visual preview of a single non-text archive entry (image, media, font, model). */
    @Composable
    fun AssetPreview(state: WorkspaceState, entry: ArchiveEntry, modifier: Modifier)
}
