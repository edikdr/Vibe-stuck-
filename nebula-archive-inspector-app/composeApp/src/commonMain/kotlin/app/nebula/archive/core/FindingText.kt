package app.nebula.archive

/**
 * Human text for a [PerformanceFinding].
 *
 * The page reports raw facts, this turns them into a localized line. Keeping the wording here — and out of
 * the injected JavaScript — is what lets both locales stay in [AppStrings] and stay type-checked.
 */
data class FindingText(val title: String, val detail: String)

fun describeFinding(finding: PerformanceFinding, strings: AppStrings): FindingText {
    val kinds = strings.findingTitles
    val title = when (finding.kind) {
        PerformanceFindingKind.SlowLoad -> kinds.slowLoad
        PerformanceFindingKind.SlowPaint -> kinds.slowPaint
        PerformanceFindingKind.Jank -> kinds.jank
        PerformanceFindingKind.LayoutShift -> kinds.layoutShift
        PerformanceFindingKind.LongTask -> kinds.longTask
        PerformanceFindingKind.RenderBlocking -> kinds.renderBlocking
        PerformanceFindingKind.OversizedImage -> kinds.oversizedImage
        PerformanceFindingKind.MissingDimensions -> kinds.missingDimensions
        PerformanceFindingKind.HeavyResource -> kinds.heavyResource
        PerformanceFindingKind.ScriptWeight -> kinds.scriptWeight
        PerformanceFindingKind.CompositingCost -> kinds.compositingCost
        PerformanceFindingKind.DomSize -> kinds.domSize
    }
    val detail = when (finding.kind) {
        PerformanceFindingKind.SlowLoad,
        PerformanceFindingKind.SlowPaint,
        -> formatMillis(finding.value)

        PerformanceFindingKind.Jank ->
            "${finding.value.toInt()}% ${kinds.slowFrames} · ${formatMillis(finding.extra)}/${kinds.frame}"

        PerformanceFindingKind.LayoutShift -> buildString {
            append("CLS ${formatDecimals(finding.value)}")
            if (finding.extra > 0) append(" · ${kinds.shift} ${formatNumber(finding.extra)} px")
        }

        PerformanceFindingKind.LongTask ->
            "${formatMillis(finding.value)} · ${finding.extra.toInt()} ${kinds.tasks}"

        PerformanceFindingKind.RenderBlocking,
        PerformanceFindingKind.HeavyResource,
        PerformanceFindingKind.ScriptWeight,
        -> formatBytes(finding.value.toLong())

        PerformanceFindingKind.OversizedImage -> buildString {
            append(formatBytes(finding.value.toLong()))
            if (finding.note.isNotBlank()) append(" · ${finding.note}")
            if (finding.extra > 0) append(" · ${(finding.extra * 100).toInt()}% ${kinds.wasted}")
        }

        PerformanceFindingKind.MissingDimensions -> finding.note.ifBlank { kinds.causesShift }

        PerformanceFindingKind.CompositingCost -> "${finding.value.toInt()} ${kinds.elements}"

        PerformanceFindingKind.DomSize -> "${finding.value.toInt()} ${kinds.nodes}"
    }
    return FindingText(title, detail)
}

/** Two decimals, for scores like CLS where the fraction is the whole point. */
private fun formatDecimals(value: Double): String {
    val hundredths = ((if (value < 0) value * 100 - 0.5 else value * 100 + 0.5)).toLong()
    val whole = hundredths / 100
    val fraction = (if (hundredths < 0) -hundredths else hundredths) % 100
    return "$whole.${fraction.toString().padStart(2, '0')}"
}
