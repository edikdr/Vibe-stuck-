package app.nebula.archive

/**
 * Platform-independent archive model.
 *
 * Everything in this file is pure Kotlin so Android, desktop and any future adapter share one description of
 * what an opened project is. Platforms only provide an [ArchiveContentSource] and an [ArchiveOpener].
 */

enum class ArchiveKind { Html, Css, JavaScript, Json, Image, Media, Font, Model, Text, Other }

enum class AppLocale { English, Russian }

/** Read-only byte access to the files of one opened project. */
interface ArchiveContentSource {
    fun readBytes(path: String): ByteArray?
    fun readText(path: String): String? = readBytes(path)?.decodeToString()
    fun close()
}

data class ArchiveEntry(
    val path: String,
    val size: Long,
    val kind: ArchiveKind,
) {
    val extension: String get() = path.substringAfterLast('.', "").lowercase()
    val name: String get() = path.substringAfterLast('/')
    val isSvg: Boolean get() = kind == ArchiveKind.Image && extension == "svg"
    val isSourceText: Boolean
        get() = kind in setOf(ArchiveKind.Html, ArchiveKind.Css, ArchiveKind.JavaScript, ArchiveKind.Json, ArchiveKind.Text)
}

data class ArchiveProject(
    val name: String,
    val entries: List<ArchiveEntry>,
    val entryPoint: String,
    val openedAtMs: Long,
    val content: ArchiveContentSource,
) {
    val totalBytes: Long by lazy { entries.sumOf { it.size } }

    /** Directory of the entry point; an archive that ships `release/index.html` is served from `release/`. */
    val siteRoot: String get() = entryPoint.substringBeforeLast('/', "")

    private val entriesByPath by lazy { entries.associateBy { it.path } }
    private val entriesByLowerPath by lazy { entries.associateBy { it.path.lowercase() } }

    fun file(path: String): ArchiveEntry? = entriesByPath[path] ?: entriesByLowerPath[path.lowercase()]
    fun bytes(path: String): ByteArray? = file(path)?.let { content.readBytes(it.path) }
    fun text(path: String): String? = file(path)?.let { content.readText(it.path) }
    fun close() = content.close()
}

/** A document the user opened before. [reference] is platform specific (a content URI, a file path…). */
data class RecentDocument(
    val reference: String,
    val name: String,
    val openedAtMs: Long,
)

/** Turns a platform document reference into a project. Implemented once per platform. */
interface ArchiveOpener {
    suspend fun open(reference: String): ArchiveProject
}

interface RecentDocumentsStore {
    fun load(): List<RecentDocument>
    fun record(reference: String, name: String): List<RecentDocument>
}

class ArchiveOpenException(message: String) : IllegalArgumentException(message)

fun archiveKind(path: String): ArchiveKind = when (path.substringAfterLast('.', "").lowercase()) {
    "html", "htm" -> ArchiveKind.Html
    "css", "scss", "sass", "less" -> ArchiveKind.Css
    "js", "mjs", "cjs", "ts", "tsx", "jsx", "vue", "svelte" -> ArchiveKind.JavaScript
    "json", "json5", "jsonl", "webmanifest", "map" -> ArchiveKind.Json
    "png", "jpg", "jpeg", "jpe", "gif", "webp", "avif", "svg", "ico", "bmp" -> ArchiveKind.Image
    "mp4", "m4v", "webm", "mov", "mp3", "m4a", "aac", "wav", "ogg", "oga", "opus", "flac" -> ArchiveKind.Media
    "woff", "woff2", "ttf", "otf", "eot" -> ArchiveKind.Font
    "glb", "gltf" -> ArchiveKind.Model
    "txt", "md", "mdx", "xml", "csv", "tsv", "yml", "yaml", "toml", "ini", "conf", "config", "properties",
    "kt", "kts", "java", "py", "rb", "php", "go", "rs", "swift", "c", "h", "cpp", "hpp", "cs", "sh", "bash",
    "gradle", "sql", "graphql", "gql", "env", "gitignore", "dockerfile" -> ArchiveKind.Text
    else -> ArchiveKind.Other
}

/** Rejects Zip Slip paths and normalises separators; `null` means the entry must not be extracted. */
fun normalizeArchivePath(raw: String): String? {
    val parts = mutableListOf<String>()
    raw.replace('\\', '/').split('/').forEach { part ->
        when (part) {
            "", "." -> Unit
            ".." -> return null
            else -> parts += part
        }
    }
    return parts.joinToString("/").takeIf { it.isNotEmpty() }
}

fun locateEntryPoint(entries: List<ArchiveEntry>): String? = entries
    .filter { it.path.endsWith("index.html", ignoreCase = true) || it.path.endsWith("index.htm", ignoreCase = true) }
    .minByOrNull { it.path.count { char -> char == '/' } }
    ?.path

/**
 * Maps a request coming from the preview to a real archive entry.
 *
 * Handles extensionless links, directory indexes, the deployed-site root, nested build folders and
 * case-mismatched resources. A main-frame request that looks like an SPA route falls back to the entry point.
 */
fun resolveArchiveRequestPath(project: ArchiveProject, requested: String, mainFrame: Boolean): String? {
    val raw = requested.substringBefore('?').substringBefore('#').trimStart('/')
    if (raw.isBlank()) return project.entryPoint
    val clean = raw.trimEnd('/')
    val siteRoot = project.siteRoot

    fun expanded(path: String): List<String> = buildList {
        add(path)
        if (!path.substringAfterLast('/').contains('.')) {
            add("$path.html")
            add("$path.htm")
            add("$path/index.html")
            add("$path/index.htm")
        }
    }

    val roots = buildList {
        add(clean)
        if (siteRoot.isNotBlank() && clean != siteRoot && !clean.startsWith("$siteRoot/")) add("$siteRoot/$clean")
        var suffix = clean.substringAfter('/', "")
        while (suffix.isNotBlank()) {
            add(suffix)
            if (siteRoot.isNotBlank() && !suffix.startsWith("$siteRoot/")) add("$siteRoot/$suffix")
            suffix = suffix.substringAfter('/', "")
        }
    }.distinct()

    roots.flatMap(::expanded).distinct().forEach { candidate ->
        project.file(candidate)?.let { return it.path }
    }
    val hasExtension = clean.substringAfterLast('/').contains('.')
    return if (mainFrame && !hasExtension) project.entryPoint else null
}
