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
private const val MAX_FINDINGS = 12
private const val MAX_TARGET = 90

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

// ---- performance ----------------------------------------------------------------------------------------

/** The whole run: one header, one line per page, one line per problem. */
fun buildPerformanceAiContext(projectName: String, site: SitePerformance, strings: AppStrings): String =
    buildString {
        append("NEBULA perf · $projectName · ${profileLabel(site.profile)}")
        if (site.pages.size > 1) {
            append(" · ${site.pages.size} pages · avg ${site.averageScore}/100")
            site.worst?.let { append(" (worst ${it.path} ${it.report.score})") }
        }
        site.pages.forEach { page ->
            appendLine()
            appendPage(page.path, page.report, strings)
        }
        appendLine()
        append("Fix the flagged items; paths point at files inside the archive.")
    }

/** Only the problems of one page — for when the score itself is not the question. */
fun buildFindingsAiContext(
    projectName: String,
    pagePath: String,
    report: RuntimePerformanceReport,
    strings: AppStrings,
): String = buildString {
    appendLine("NEBULA perf · $projectName · $pagePath · ${profileLabel(report.profile)}")
    if (report.findings.isEmpty()) {
        append(strings.noProblems)
        return@buildString
    }
    report.findings.take(MAX_FINDINGS).forEach { finding -> appendLine(findingLine(finding, strings)) }
    append("Fix the flagged items; paths point at files inside the archive.")
}

/** One problem, with everything needed to fix that one thing. */
fun buildFindingAiContext(
    projectName: String,
    pagePath: String,
    finding: PerformanceFinding,
    profile: NetworkProfile,
    strings: AppStrings,
): String = buildString {
    val text = describeFinding(finding, strings)
    appendLine("NEBULA perf · $projectName · $pagePath · ${profileLabel(profile)}")
    appendLine("${severityMark(finding)} ${text.title} — ${text.detail}")
    if (finding.target.isNotBlank()) appendLine("    at ${finding.target}")
    if (finding.selector.isNotBlank()) appendLine("    selector ${compactSelector(finding.selector)}")
    append("Explain how to fix this specific problem.")
}

private fun StringBuilder.appendPage(path: String, report: RuntimePerformanceReport, strings: AppStrings) {
    val metrics = report.metrics
    append("$path ${report.score}/100 · load ${formatMillis(metrics.loadMs)}")
    metrics.firstContentfulPaintMs?.let { append(" · FCP ${formatMillis(it)}") }
    append(" · frames ${metrics.slowFramePercent.toInt()}% slow")
    append(" · DOM ${metrics.domNodes}")
    append(" · res ${metrics.resourceCount} (${formatBytes(metrics.decodedResourceBytes)})")
    if (metrics.scriptBytes > 0) append(" · JS ${formatBytes(metrics.scriptBytes)}")
    appendLine()
    report.findings.take(MAX_FINDINGS).forEach { finding -> appendLine("  " + findingLine(finding, strings)) }
}

private fun findingLine(finding: PerformanceFinding, strings: AppStrings): String {
    val text = describeFinding(finding, strings)
    return buildString {
        append("${severityMark(finding)} ${text.title}")
        // The selector is what an assistant can act on, so it wins over the short human label.
        val where = if (finding.selector.isNotBlank()) compactSelector(finding.selector) else finding.target
        if (where.isNotBlank()) append("  ${oneLine(where, MAX_TARGET)}")
        if (text.detail.isNotBlank()) append(" — ${text.detail}")
    }
}

private fun severityMark(finding: PerformanceFinding) = when (finding.severity) {
    FindingSeverity.High -> "!"
    FindingSeverity.Medium -> "•"
    FindingSeverity.Low -> "·"
}

private fun profileLabel(profile: NetworkProfile) =
    if (profile.throttled) "${profile.shortLabel} (simulated)" else "no throttling"

/** `body > main > button.cta` is unambiguous at half the width as `body>main>button.cta`. */
private fun compactSelector(selector: String): String = selector.replace(" > ", ">")

private fun oneLine(value: String, limit: Int): String {
    val collapsed = value.replace(Regex("\\s+"), " ").trim()
    return if (collapsed.length <= limit) collapsed else collapsed.take(limit).trimEnd() + "…"
}
