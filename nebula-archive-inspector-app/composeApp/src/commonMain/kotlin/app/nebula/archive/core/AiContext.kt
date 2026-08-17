package app.nebula.archive

/**
 * The block copied by "Copy for AI".
 *
 * It is written to be dense: an assistant needs the element's identity, its real box, the styles that were
 * actually applied, the attributes it can hook onto and where to look in the archive — but every extra
 * character costs the reader context. So the block carries one short line per fact, no fenced code, no
 * repeated file excerpts: only the best candidate keeps a one-line excerpt, the rest are `path:line`.
 */

private const val MAX_ELEMENTS = 12
private const val MAX_STYLES = 12
private const val MAX_ATTRIBUTES = 8
private const val MAX_HTML = 600
private const val MAX_TEXT = 160
private const val MAX_EXCERPT = 160
private const val MAX_SOURCES = 6

fun buildAiContext(
    projectName: String,
    pagePath: String,
    elements: List<InspectedElement>,
    sources: Map<Int, List<SourceHit>> = emptyMap(),
): String = buildString {
    val visible = elements.take(MAX_ELEMENTS)
    append("NEBULA read-only inspection · $projectName · $pagePath")
    if (visible.size > 1) append(" · ${visible.size} elements selected")
    visible.forEachIndexed { index, element ->
        appendLine()
        appendElement(index + 1, element, sources[element.id].orEmpty())
    }
    appendLine()
    append(
        if (visible.size > 1) {
            "These elements were selected together. Use the paths as candidates and verify them before editing."
        } else {
            "Use the paths as candidates and verify them before editing."
        },
    )
}

private fun StringBuilder.appendElement(number: Int, element: InspectedElement, hits: List<SourceHit>) {
    val marker = if (element.classes.isEmpty() && element.elementId.isBlank()) {
        "<${element.tag}>"
    } else {
        "<${element.tag}${element.elementId.let { if (it.isBlank()) "" else "#$it" }}" +
            element.classes.take(3).joinToString("") { ".$it" } + ">"
    }
    appendLine("[$number] $marker  ${compactSelector(element.selector)}")
    appendLine("    box ${element.size} @${formatNumber(element.left)},${formatNumber(element.top)} · ${element.childCount} children · depth ${element.depth}")
    if (element.styles.isNotEmpty()) {
        appendLine("    css " + element.styles.take(MAX_STYLES).joinToString("; ") { "${it.name}:${it.value}" })
    }
    val attributes = element.attributes
        .filter { it.name != "class" && it.name != "id" && it.name != "style" }
        .take(MAX_ATTRIBUTES)
    if (attributes.isNotEmpty()) {
        appendLine("    attr " + attributes.joinToString(" ") { """${it.name}="${it.value}"""" })
    }
    if (element.text.isNotBlank()) appendLine("""    text "${oneLine(element.text, MAX_TEXT)}"""")
    if (element.outerHtml.isNotBlank()) appendLine("    html " + oneLine(element.outerHtml, MAX_HTML))
    if (hits.isEmpty()) {
        appendLine("    src no match in the archive — the element may be created by JavaScript")
        return
    }
    val best = hits.first()
    val excerpt = oneLine(best.excerpt, MAX_EXCERPT)
    appendLine("    src ${best.path}:${best.line}" + if (excerpt.isBlank()) "" else " ▸ $excerpt")
    val rest = hits.drop(1).take(MAX_SOURCES - 1)
    if (rest.isNotEmpty()) appendLine("        " + rest.joinToString(" · ") { "${it.path}:${it.line}" })
}

/** `body > main > button.cta` is unambiguous at half the width as `body>main>button.cta`. */
private fun compactSelector(selector: String): String = selector.replace(" > ", ">")

private fun oneLine(value: String, limit: Int): String {
    val collapsed = value.replace(Regex("\\s+"), " ").trim()
    return if (collapsed.length <= limit) collapsed else collapsed.take(limit).trimEnd() + "…"
}
