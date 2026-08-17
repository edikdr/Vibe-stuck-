package app.nebula.archive

import java.io.File
import java.io.InputStream
import java.util.UUID
import java.util.zip.ZipInputStream

/**
 * Extraction shared by every JVM platform.
 *
 * Android and desktop only differ in how they hand over the bytes of the chosen document, so both pass an
 * [InputStream] factory here and get a ready [ArchiveProject] back. The archive is expanded into a working
 * directory rather than held in memory, and every limit that protects the device is enforced while writing.
 */
class JvmArchiveExtractor(private val workRoot: File) {

    fun open(document: JvmDocument): ArchiveProject = when {
        document.isHtml -> openHtml(document)
        document.name.endsWith(".zip", ignoreCase = true) -> openZip(document)
        else -> throw ArchiveOpenException("Choose a ZIP archive or HTML file.")
    }

    private fun openZip(document: JvmDocument): ArchiveProject {
        if (document.size > MAX_ARCHIVE_BYTES) throw ArchiveOpenException("Archive exceeds the 350 MB limit.")
        val directory = createProjectDirectory()
        val extracted = mutableListOf<ExtractedEntry>()
        var unpackedBytes = 0L
        try {
            ZipInputStream(document.open().buffered(BUFFER)).use { zip ->
                while (true) {
                    val zipEntry = zip.nextEntry ?: break
                    if (zipEntry.isDirectory || zipEntry.name.startsWith("__MACOSX/")) {
                        zip.closeEntry()
                        continue
                    }
                    val safePath = normalizeArchivePath(zipEntry.name)
                        ?: throw ArchiveOpenException("The archive contains an unsafe path.")
                    if (extracted.size >= MAX_ENTRIES) throw ArchiveOpenException("The archive contains more than 25,000 files.")

                    val outputFile = File(directory, safePath)
                    // Second Zip Slip guard: a normalised name can still escape through a symlinked parent.
                    if (!outputFile.canonicalPath.startsWith(directory.canonicalPath + File.separator)) {
                        throw ArchiveOpenException("The archive contains an unsafe path.")
                    }
                    outputFile.parentFile?.mkdirs()

                    var fileBytes = 0L
                    outputFile.outputStream().buffered(BUFFER).use { output ->
                        val buffer = ByteArray(BUFFER)
                        while (true) {
                            val count = zip.read(buffer)
                            if (count <= 0) break
                            unpackedBytes += count
                            fileBytes += count
                            if (unpackedBytes > MAX_UNPACKED_BYTES) throw ArchiveOpenException("The extracted archive exceeds 800 MB.")
                            if (fileBytes > MAX_SINGLE_FILE_BYTES) throw ArchiveOpenException("A file inside the archive exceeds 180 MB.")
                            output.write(buffer, 0, count)
                        }
                    }
                    extracted += ExtractedEntry(safePath, fileBytes, outputFile)
                    zip.closeEntry()
                }
            }
            if (extracted.isEmpty()) throw ArchiveOpenException("The archive is empty.")

            // Archives usually wrap the site in one folder; serving from inside it keeps root-absolute paths working.
            val prefix = commonRoot(extracted.map { it.path })
            val visible = LinkedHashMap<String, File>()
            val entries = mutableListOf<ArchiveEntry>()
            extracted.forEach { item ->
                val path = item.path.removePrefix(prefix).takeIf { it.isNotBlank() } ?: return@forEach
                if (visible.put(path, item.file) == null) entries += ArchiveEntry(path, item.size, archiveKind(path))
            }
            val entryPoint = locateEntryPoint(entries)
                ?: throw ArchiveOpenException("index.html was not found. This ZIP is not a ready static website.")

            return ArchiveProject(
                name = document.name.substringBeforeLast('.'),
                entries = entries.sortedBy { it.path },
                entryPoint = entryPoint,
                openedAtMs = System.currentTimeMillis(),
                content = DiskArchiveSource(directory, visible),
            )
        } catch (error: Throwable) {
            directory.deleteRecursively()
            throw error
        }
    }

    private fun openHtml(document: JvmDocument): ArchiveProject {
        if (document.size > MAX_HTML_BYTES) throw ArchiveOpenException("HTML file exceeds the 25 MB limit.")
        val directory = createProjectDirectory()
        val safeName = normalizeArchivePath(document.name.substringAfterLast('/'))
            ?.takeIf { it.endsWith(".html", true) || it.endsWith(".htm", true) }
            ?: "index.html"
        val outputFile = File(directory, safeName)
        try {
            var total = 0L
            document.open().use { input ->
                outputFile.outputStream().buffered(BUFFER).use { output ->
                    val buffer = ByteArray(BUFFER)
                    while (true) {
                        val count = input.read(buffer)
                        if (count <= 0) break
                        total += count
                        if (total > MAX_HTML_BYTES) throw ArchiveOpenException("HTML file exceeds the 25 MB limit.")
                        output.write(buffer, 0, count)
                    }
                }
            }
            return ArchiveProject(
                name = document.name.substringBeforeLast('.'),
                entries = listOf(ArchiveEntry(safeName, total, ArchiveKind.Html)),
                entryPoint = safeName,
                openedAtMs = System.currentTimeMillis(),
                content = DiskArchiveSource(directory, mapOf(safeName to outputFile)),
            )
        } catch (error: Throwable) {
            directory.deleteRecursively()
            throw error
        }
    }

    private fun createProjectDirectory(): File {
        val directory = File(workRoot, "nebula-archives/${UUID.randomUUID()}")
        if (!directory.mkdirs()) throw ArchiveOpenException("Could not create local archive storage.")
        return directory
    }

    private data class ExtractedEntry(val path: String, val size: Long, val file: File)

    companion object {
        const val MAX_ARCHIVE_BYTES = 350L * 1024 * 1024
        const val MAX_HTML_BYTES = 25L * 1024 * 1024
        const val MAX_UNPACKED_BYTES = 800L * 1024 * 1024
        const val MAX_SINGLE_FILE_BYTES = 180L * 1024 * 1024
        const val MAX_ENTRIES = 25_000
        private const val BUFFER = 64 * 1024
    }
}

/** One document chosen by a platform picker, with lazy access to its bytes. */
class JvmDocument(
    val name: String,
    val size: Long,
    val mimeType: String?,
    val open: () -> InputStream,
) {
    val isHtml: Boolean
        get() = name.endsWith(".html", ignoreCase = true) ||
            name.endsWith(".htm", ignoreCase = true) ||
            mimeType in setOf("text/html", "application/xhtml+xml")
}

/** Serves the extracted files and owns their directory. */
class DiskArchiveSource(
    private val root: File,
    private val files: Map<String, File>,
) : ArchiveContentSource {
    override fun readBytes(path: String): ByteArray? = file(path)?.readBytes()
    override fun readText(path: String): String? = file(path)?.readText(Charsets.UTF_8)

    /** Direct file access lets a preview stream large media instead of loading it into memory. */
    fun file(path: String): File? = files[path]?.takeIf { it.isFile }

    override fun close() {
        root.deleteRecursively()
    }
}

/** The single top-level folder shared by every entry, if there is one. */
internal fun commonRoot(paths: List<String>): String {
    val first = paths.firstOrNull()?.substringBefore('/')?.takeIf { it.isNotBlank() } ?: return ""
    return if (paths.all { it.startsWith("$first/") }) "$first/" else ""
}
