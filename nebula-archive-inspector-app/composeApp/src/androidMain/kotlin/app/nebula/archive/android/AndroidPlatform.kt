package app.nebula.archive

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.provider.OpenableColumns
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import app.nebula.archive.ui.DocumentPicker
import app.nebula.archive.ui.NebulaPlatform
import app.nebula.archive.ui.PreviewHost
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject

/** Android side of the platform contract: picking, opening, remembering and copying. */
class AndroidPlatform(
    private val context: Context,
    override val documentPicker: DocumentPicker,
) : NebulaPlatform {
    override val opener: ArchiveOpener = AndroidArchiveOpener(context)
    override val recentDocuments: RecentDocumentsStore = AndroidRecentDocuments(context)
    override val previewHost: PreviewHost = AndroidPreviewHost

    override fun copyToClipboard(text: String, confirmation: String) {
        if (text.isBlank()) return
        context.getSystemService(ClipboardManager::class.java)
            .setPrimaryClip(ClipData.newPlainText("Nebula inspector", text))
        Toast.makeText(context, confirmation, Toast.LENGTH_SHORT).show()
    }
}

/** The system document picker. Registered before the activity starts so it can be launched at any time. */
class AndroidDocumentPicker(activity: ComponentActivity) : DocumentPicker {
    private var pending: ((List<String>) -> Unit)? = null
    private val launcher = activity.registerForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) { uris ->
        pending?.invoke(uris.map(Uri::toString))
        pending = null
    }

    override fun pick(onPicked: (List<String>) -> Unit) {
        pending = onPicked
        launcher.launch(MIME_TYPES)
    }

    private companion object {
        val MIME_TYPES = arrayOf(
            "application/zip",
            "application/x-zip-compressed",
            "text/html",
            "application/xhtml+xml",
            "application/octet-stream",
        )
    }
}

/** Opens a document URI with the shared JVM extractor; no storage permission is involved. */
class AndroidArchiveOpener(private val context: Context) : ArchiveOpener {
    private val extractor = JvmArchiveExtractor(context.cacheDir)

    override suspend fun open(reference: String): ArchiveProject = withContext(Dispatchers.IO) {
        val uri = Uri.parse(reference)
        val resolver = context.contentResolver
        // Keeps the recent-documents entry usable after a restart; a one-shot grant simply fails here.
        runCatching { resolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION) }

        var name = "archive.zip"
        var size = -1L
        resolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE), null, null, null)?.use { cursor ->
            if (cursor.moveToFirst()) {
                val nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                val sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE)
                if (nameIndex >= 0) cursor.getString(nameIndex)?.let { name = it }
                if (sizeIndex >= 0 && !cursor.isNull(sizeIndex)) size = cursor.getLong(sizeIndex)
            }
        }
        extractor.open(
            JvmDocument(name, size, resolver.getType(uri)) {
                resolver.openInputStream(uri) ?: throw ArchiveOpenException("Could not read the file.")
            },
        )
    }
}

class AndroidRecentDocuments(context: Context) : RecentDocumentsStore {
    private val preferences = context.getSharedPreferences("nebula_recent_documents", Context.MODE_PRIVATE)

    override fun load(): List<RecentDocument> = runCatching {
        val array = JSONArray(preferences.getString(KEY, "[]"))
        List(array.length()) { index ->
            val item = array.getJSONObject(index)
            RecentDocument(item.getString("reference"), item.getString("name"), item.optLong("openedAtMs"))
        }.take(MAX_RECENT)
    }.getOrDefault(emptyList())

    override fun record(reference: String, name: String): List<RecentDocument> {
        val updated = (listOf(RecentDocument(reference, name, System.currentTimeMillis())) +
            load().filterNot { it.reference == reference }).take(MAX_RECENT)
        val array = JSONArray()
        updated.forEach { item ->
            array.put(
                JSONObject()
                    .put("reference", item.reference)
                    .put("name", item.name)
                    .put("openedAtMs", item.openedAtMs),
            )
        }
        preferences.edit().putString(KEY, array.toString()).apply()
        return updated
    }

    private companion object {
        const val KEY = "items"
        const val MAX_RECENT = 5
    }
}
