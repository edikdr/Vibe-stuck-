package app.nebula.archive

/**
 * The block copied by "Copy for AI".
 *
 * It is written to be dense: an assistant needs the element's identity, the rule that styled it, the file and
 * line that rule lives on, and the markup it came from — and every extra character costs the reader context.
 * So there are no sentences, no fenced code and no labels that repeat what the value already says. What is
 * kept is what cannot be guessed: paths, names, and the code itself.
 */

private const val MAX_ELEMENTS = 12
private const val MAX_STYLES = 8
private const val MAX_ATTRIBUTES = 6
private const val MAX_RULES = 4
private const val MAX_HTML = 220
private const val MAX_TEXT = 90
private const val MAX_EXCERPT = 120
private const val MAX_FINDINGS = 12
private const val MAX_TARGET = 90

fun buildAiContext(
    projectName: String,
    pagePath: String,
    elements: List<InspectedElement>,
    sources: Map<Int, List<SourceHit>> = emptyMap(),
    rules: Map<Int, List<StyleRule>> = emptyMap(),
): String = buildString {
    val visible = elements.take(MAX_ELEMENTS)
    append("NEBULA $projectName/$pagePath")
    if (visible.size > 1) append(" · ${visible.size} elements")
    visible.forEachIndexed { index, element ->
        appendLine()
        appendElement(index + 1, element, sources[element.id].orEmpty(), rules[element.id] ?: element.rules)
    }
}

private fun StringBuilder.appendElement(
    number: Int,
    element: InspectedElement,
    hits: List<SourceHit>,
    rules: List<StyleRule>,
) {
    val marker = if (element.classes.isEmpty() && element.elementId.isBlank()) {
        element.tag
    } else {
        element.tag + (if (element.elementId.isBlank()) "" else "#${element.elementId}") +
            element.classes.take(3).joinToString("") { ".$it" }
    }
    val children = if (element.childCount > 0) " c${element.childCount}" else ""
    val box = "${formatNumber(element.width)}×${formatNumber(element.height)}" +
        "@${formatNumber(element.left)},${formatNumber(element.top)}"
    appendLine("[$number] $marker · ${compactSelector(element.selector)} · $box · d${element.depth}$children")

    val applied = rules.filter { rule -> rule.declarations.any { it.winning } }.take(MAX_RULES)
    applied.forEach { rule ->
        val declarations = rule.declarations.filter { it.winning }.joinToString(";") { "${it.name}:${it.value}" }
        val condition = if (rule.condition.isBlank()) "" else "${rule.condition} "
        appendLine("  ${rule.place} $condition${rule.selector}{$declarations}")
    }
    val inline = element.inlineStyle.filter { it.winning }
    if (inline.isNotEmpty()) appendLine("  style{" + inline.joinToString(";") { "${it.name}:${it.value}" } + "}")

    // Computed values a rule already states are dropped: the rule line is the one worth reading.
    val claimed = applied.flatMap { rule -> rule.declarations.filter { it.winning }.map { it.name } }.toSet()
    val tail = element.styles
        .filter { it.name !in claimed && it.name !in setOf("size", "font", "color") }
        .take(MAX_STYLES)
    if (tail.isNotEmpty()) appendLine("  css " + tail.joinToString(";") { "${it.name}:${it.value}" })

    val attributes = element.attributes
        .filter { it.name != "class" && it.name != "id" && it.name != "style" }
        .take(MAX_ATTRIBUTES)
    if (attributes.isNotEmpty()) appendLine("  attr " + attributes.joinToString(" ") { """${it.name}="${it.value}"""" })
    if (element.text.isNotBlank()) appendLine("""  text "${oneLine(element.text, MAX_TEXT)}"""")

    val best = hits.firstOrNull()
    if (best != null) {
        val excerpt = oneLine(best.excerpt, MAX_EXCERPT)
        appendLine("  ${best.path}:${best.line}" + if (excerpt.isBlank()) "" else " ▸ $excerpt")
        return
    }
    // Nothing in the markup means a script built it, so the live tag is the only code there is to show.
    appendLine("  html " + oneLine(element.outerHtml, MAX_HTML))
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
/** A long chain is trimmed in the middle: the ends are what identifies an element, not the path through it. */
private fun compactSelector(selector: String): String {
    val parts = selector.split(" > ")
    return if (parts.size <= 3) parts.joinToString(">") else "${parts.first()}>…>${parts.takeLast(2).joinToString(">")}"
}

private fun oneLine(value: String, limit: Int): String {
    val collapsed = value.replace(Regex("\\s+"), " ").trim()
    return if (collapsed.length <= limit) collapsed else collapsed.take(limit).trimEnd() + "…"
}
