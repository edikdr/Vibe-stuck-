package app.nebula.archive

private const val MAX_HITS = 24
private const val MAX_SCANNED_FILES = 1_200
private const val MAX_SCANNED_BYTES = 48L * 1024 * 1024
private const val MAX_FILE_BYTES = 4L * 1024 * 1024

/** Ranked source candidates for one inspected element. */
fun findElementSources(project: ArchiveProject, element: InspectedElement): List<SourceHit> = findElementSources(
    project = project,
    selector = element.selector,
    id = element.elementId,
    classes = element.classes,
    tag = element.tag,
    elementText = element.text,
    outerHtml = element.outerHtml,
)

fun findElementSources(
    project: ArchiveProject,
    selector: String,
    id: String,
    classes: List<String>,
    tag: String = "",
    elementText: String = "",
    outerHtml: String = "",
): List<SourceHit> {
    // Needles are grouped by what they mean, not by how they are written: `.cta` in a stylesheet and
    // `class="cta"` in markup are the same evidence, so they must not compete for rank. Ranking by group
    // leaves the file kind — markup first — as the tiebreaker between equally good matches.
    val groups = buildList {
        if (id.isNotBlank()) add(listOf("id=\"$id\"", "id='$id'", "#$id"))
        classes.take(4).forEach { cssClass -> add(listOf(".$cssClass", "class=\"$cssClass", "class='$cssClass")) }
        val openingTag = outerHtml.substringBefore('>').take(1_000)
        Regex("""(?:data-[a-z0-9_-]+|aria-[a-z0-9_-]+|name|role|href|src)=[\"'][^\"']{2,180}[\"']""", RegexOption.IGNORE_CASE)
            .findAll(openingTag)
            .take(6)
            .forEach { match -> add(listOf(match.value)) }
        elementText.trim().replace(Regex("\\s+"), " ").takeIf { it.length in 5..140 }?.let { add(listOf(it)) }
        if (selector.isNotBlank()) add(listOf(selector))
        if (isEmpty() && tag.isNotBlank()) add(listOf("<$tag"))
    }.map { variants -> variants.filter { it.length >= 2 } }.filter { it.isNotEmpty() }
    if (groups.isEmpty()) return emptyList()

    val candidates = project.entries.asSequence()
        .filter { it.kind in setOf(ArchiveKind.Html, ArchiveKind.Css, ArchiveKind.JavaScript) && it.size <= MAX_FILE_BYTES }
        .sortedWith(
            compareBy(
                { if (it.path == project.entryPoint) 0 else 1 },
                { kindRank(it.kind) },
                { it.size },
            ),
        )

    val hits = mutableListOf<Pair<Int, SourceHit>>()
    var scannedFiles = 0
    var scannedBytes = 0L
    for (entry in candidates) {
        if (hits.size >= MAX_HITS || scannedFiles >= MAX_SCANNED_FILES || scannedBytes + entry.size > MAX_SCANNED_BYTES) break
        scannedFiles++
        scannedBytes += entry.size
        val text = project.text(entry.path) ?: continue
        val match = groups.asSequence().mapIndexedNotNull { groupIndex, variants ->
            variants.firstNotNullOfOrNull { needle ->
                text.indexOf(needle, ignoreCase = true).takeIf { it >= 0 }
            }?.let { index -> groupIndex to index }
        }.firstOrNull() ?: continue
        val (groupIndex, index) = match
        val line = text.take(index).count { it == '\n' } + 1
        val excerpt = text.lineSequence().drop((line - 2).coerceAtLeast(0)).take(3).joinToString("\n").take(900)
        val rank = (groups.size - groupIndex) * 100 +
            (3 - kindRank(entry.kind)) * 10 +
            (if (entry.path == project.entryPoint) 8 else 0)
        hits += rank to SourceHit(entry.path, line, excerpt)
    }
    return hits.sortedByDescending { it.first }.map { it.second }.take(MAX_HITS)
}

private fun kindRank(kind: ArchiveKind) = when (kind) {
    ArchiveKind.Html -> 0
    ArchiveKind.Css -> 1
    else -> 2
}

