package app.nebula.archive

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.toComposeImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import app.nebula.archive.ui.DocumentPicker
import app.nebula.archive.ui.NebulaPlatform
import app.nebula.archive.ui.PreviewHost
import app.nebula.archive.ui.SectionLabel
import app.nebula.archive.ui.TextMain
import app.nebula.archive.ui.TextMuted
import app.nebula.archive.ui.WorkspaceState
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.awt.Toolkit
import java.awt.datatransfer.StringSelection
import java.io.ByteArrayInputStream
import java.io.File
import javax.imageio.ImageIO
import javax.swing.JFileChooser
import javax.swing.SwingUtilities
import javax.swing.filechooser.FileNameExtensionFilter

/**
 * Desktop side of the platform contract.
 *
 * Opening archives, the file library, the source viewer and every inspector data structure already work here
 * because they are shared. What is still missing is a live page preview: it needs an embedded browser, and
 * [DesktopPreviewHost] marks exactly where that adapter plugs in.
 */
class DesktopPlatform : NebulaPlatform {
    override val opener: ArchiveOpener = DesktopArchiveOpener()
    override val recentDocuments: RecentDocumentsStore = DesktopRecentDocuments()
    override val documentPicker: DocumentPicker = DesktopDocumentPicker()
    override val previewHost: PreviewHost = DesktopPreviewHost

    override fun copyToClipboard(text: String, confirmation: String) {
        if (text.isBlank()) return
        Toolkit.getDefaultToolkit().systemClipboard.setContents(StringSelection(text), null)
    }
}

class DesktopArchiveOpener : ArchiveOpener {
    private val extractor = JvmArchiveExtractor(File(System.getProperty("java.io.tmpdir"), "nebula-archive-inspector"))

    override suspend fun open(reference: String): ArchiveProject = withContext(Dispatchers.IO) {
        val file = File(reference)
        if (!file.isFile) throw ArchiveOpenException("The file no longer exists: $reference")
        extractor.open(JvmDocument(file.name, file.length(), null, file::inputStream))
    }
}

class DesktopDocumentPicker : DocumentPicker {
    override fun pick(onPicked: (List<String>) -> Unit) {
        SwingUtilities.invokeLater {
            val chooser = JFileChooser().apply {
                isMultiSelectionEnabled = true
                dialogTitle = "Open ZIP or HTML"
                fileFilter = FileNameExtensionFilter("Static website (zip, html)", "zip", "html", "htm")
            }
            if (chooser.showOpenDialog(null) != JFileChooser.APPROVE_OPTION) return@invokeLater
            onPicked(chooser.selectedFiles.map { it.absolutePath })
        }
    }
}

/** Recent documents kept next to the user's other application data. */
class DesktopRecentDocuments : RecentDocumentsStore {
    private val storage = File(System.getProperty("user.home"), ".nebula-archive-inspector/recent.tsv")

    override fun load(): List<RecentDocument> = runCatching {
        storage.readLines().mapNotNull { line ->
            val parts = line.split('\t')
            if (parts.size < 3) return@mapNotNull null
            RecentDocument(parts[0], parts[1], parts[2].toLongOrNull() ?: 0L)
        }.take(MAX_RECENT)
    }.getOrDefault(emptyList())

    override fun record(reference: String, name: String): List<RecentDocument> {
        val updated = (listOf(RecentDocument(reference, name, System.currentTimeMillis())) +
            load().filterNot { it.reference == reference }).take(MAX_RECENT)
        runCatching {
            storage.parentFile?.mkdirs()
            storage.writeText(updated.joinToString("\n") { "${it.reference}\t${it.name}\t${it.openedAtMs}" })
        }
        return updated
    }

    private companion object {
        const val MAX_RECENT = 5
    }
}

object DesktopPreviewHost : PreviewHost {
    override val supportsLivePreview = false

    @Composable
    override fun Preview(state: WorkspaceState, modifier: Modifier) {
        Notice(state.strings.livePreviewUnavailable, state.project.entryPoint, modifier)
    }

    @Composable
    override fun AssetPreview(state: WorkspaceState, entry: ArchiveEntry, modifier: Modifier) {
        val image: ImageBitmap? = remember(state.project.openedAtMs, entry.path) {
            if (entry.kind != ArchiveKind.Image || entry.isSvg) {
                null
            } else {
                runCatching {
                    state.project.bytes(entry.path)?.let { bytes ->
                        ImageIO.read(ByteArrayInputStream(bytes))?.toComposeImageBitmap()
                    }
                }.getOrNull()
            }
        }
        if (image == null) {
            Notice(state.strings.livePreviewUnavailable, entry.path, modifier)
        } else {
            Box(modifier.background(Color.Black), contentAlignment = Alignment.Center) {
                Image(image, entry.path, Modifier.fillMaxSize(), contentScale = ContentScale.Fit)
            }
        }
    }

    @Composable
    private fun Notice(message: String, detail: String, modifier: Modifier) {
        Box(modifier.background(Color(0xFF070B14)), contentAlignment = Alignment.Center) {
            Column(
                Modifier.widthIn(max = 420.dp).padding(24.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                SectionLabel("DESKTOP ADAPTER")
                Text(message, color = TextMain, fontSize = 13.sp, fontWeight = FontWeight.Bold)
                Text(detail, color = TextMuted, fontSize = 11.sp)
            }
        }
    }
}
