package app.nebula.archive

/**
 * Network conditions the preview can simulate.
 *
 * Values follow the Chrome DevTools presets so the numbers mean the same thing they mean everywhere else.
 * This is a real bandwidth/latency simulation applied while serving archive files — it is not a CPU
 * throttle, which an embedded WebView cannot expose.
 */
enum class NetworkProfile(val shortLabel: String, val latencyMs: Long, val bytesPerSecond: Long) {
    Unthrottled("FULL", 0, 0),
    Fast4G("4G", 40, 500_000),
    Slow4G("4G−", 150, 200_000),
    Slow3G("3G", 400, 50_000);

    val throttled: Boolean get() = this != Unthrottled

    fun next(): NetworkProfile = entries[(ordinal + 1) % entries.size]
}

data class RuntimeMetrics(
    val responseStartMs: Double = 0.0,
    val domContentLoadedMs: Double = 0.0,
    val loadMs: Double = 0.0,
    val firstContentfulPaintMs: Double? = null,
    val averageFrameMs: Double = 0.0,
    val worstFrameMs: Double = 0.0,
    val slowFramePercent: Double = 0.0,
    val domNodes: Int = 0,
    val resourceCount: Int = 0,
    val decodedResourceBytes: Long = 0,
    /** JavaScript actually loaded by THIS page, not everything the archive happens to contain. */
    val scriptBytes: Long = 0,
    val layoutShiftScore: Double = 0.0,
    val longTaskCount: Int = 0,
    val longTaskTotalMs: Double = 0.0,
)

/** What slows the page down. Metric-level findings are derived here; DOM-level ones come from the page. */
enum class PerformanceFindingKind {
    SlowLoad,
    SlowPaint,
    Jank,
    LayoutShift,
    LongTask,
    RenderBlocking,
    OversizedImage,
    MissingDimensions,
    HeavyResource,
    ScriptWeight,
    CompositingCost,
    DomSize,
}

enum class FindingSeverity { High, Medium, Low }

/**
 * One concrete cause. [selector] non-blank means the panel can jump straight into the element inspector;
 * [target] that matches an archive entry means it can open the source file.
 */
data class PerformanceFinding(
    val kind: PerformanceFindingKind,
    val target: String = "",
    val selector: String = "",
    val note: String = "",
    val value: Double = 0.0,
    val extra: Double = 0.0,
) {
    val severity: FindingSeverity
        get() = when (kind) {
            PerformanceFindingKind.SlowLoad -> if (value > 4_000) FindingSeverity.High else FindingSeverity.Medium
            PerformanceFindingKind.SlowPaint -> if (value > 2_500) FindingSeverity.High else FindingSeverity.Medium
            PerformanceFindingKind.Jank -> if (value > 30) FindingSeverity.High else FindingSeverity.Medium
            PerformanceFindingKind.LayoutShift -> if (value >= 0.25) FindingSeverity.High else FindingSeverity.Medium
            PerformanceFindingKind.LongTask -> if (value >= 250) FindingSeverity.High else FindingSeverity.Medium
            PerformanceFindingKind.RenderBlocking -> FindingSeverity.High
            PerformanceFindingKind.OversizedImage -> if (extra >= .9) FindingSeverity.High else FindingSeverity.Medium
            PerformanceFindingKind.MissingDimensions -> FindingSeverity.Medium
            PerformanceFindingKind.HeavyResource -> if (value > 1024 * 1024) FindingSeverity.High else FindingSeverity.Medium
            PerformanceFindingKind.ScriptWeight -> if (value > 5 * MB) FindingSeverity.High else FindingSeverity.Medium
            PerformanceFindingKind.CompositingCost -> FindingSeverity.Low
            PerformanceFindingKind.DomSize -> if (value > 6_000) FindingSeverity.High else FindingSeverity.Low
        }

    /** Sort weight: severity first, then the raw number, so the worst offender of a kind leads its group. */
    val impact: Double
        get() = when (severity) {
            FindingSeverity.High -> 2_000_000.0
            FindingSeverity.Medium -> 1_000_000.0
            FindingSeverity.Low -> 0.0
        } + value.coerceAtMost(999_999.0)
}

data class RuntimePerformanceReport(
    val score: Int,
    val grade: Char,
    val metrics: RuntimeMetrics,
    val findings: List<PerformanceFinding>,
    val profile: NetworkProfile = NetworkProfile.Unthrottled,
)

/** One measured page of the site. */
data class PagePerformance(
    val path: String,
    val report: RuntimePerformanceReport,
)

/** The whole run: every page that was measured, plus where it hurts most. */
data class SitePerformance(
    val pages: List<PagePerformance>,
    val profile: NetworkProfile,
) {
    val averageScore: Int = if (pages.isEmpty()) 0 else pages.sumOf { it.report.score } / pages.size
    val worst: PagePerformance? = pages.minByOrNull { it.report.score }
    val grade: Char = gradeFor(averageScore)
}

private const val MB = 1024.0 * 1024.0

/**
 * Scores a measured page and turns its metrics into concrete findings.
 *
 * Everything here describes the page that was actually loaded — page-level resource weight comes from the
 * measurement, never from the size of the whole archive, so a multi-page site with code splitting is not
 * penalised for code this page never executed.
 */
fun evaluateRuntimePerformance(
    metrics: RuntimeMetrics,
    pageFindings: List<PerformanceFinding> = emptyList(),
    profile: NetworkProfile = NetworkProfile.Unthrottled,
): RuntimePerformanceReport {
    val paint = metrics.firstContentfulPaintMs
    var score = 100
    score -= threshold(metrics.loadMs, 900.0 to 0, 1_600.0 to 5, 2_800.0 to 12, 4_500.0 to 22, fallback = 32)
    if (paint != null) score -= threshold(paint, 700.0 to 0, 1_300.0 to 4, 2_200.0 to 10, fallback = 18)
    score -= threshold(metrics.slowFramePercent, 5.0 to 0, 15.0 to 6, 30.0 to 14, fallback = 24)
    score -= threshold(metrics.averageFrameMs, 20.0 to 0, 28.0 to 7, fallback = 12)
    score -= threshold(metrics.domNodes.toDouble(), 1_500.0 to 0, 3_000.0 to 5, 6_000.0 to 12, fallback = 20)
    score -= threshold(metrics.resourceCount.toDouble(), 120.0 to 0, 250.0 to 5, fallback = 10)
    score -= threshold(metrics.scriptBytes.toDouble(), 2 * MB to 0, 5 * MB to 5, fallback = 10)
    score -= threshold(metrics.layoutShiftScore, 0.1 to 0, 0.25 to 6, fallback = 12)
    score -= threshold(metrics.longTaskTotalMs, 200.0 to 0, 600.0 to 5, fallback = 10)
    score = score.coerceIn(0, 100)

    val findings = (metricFindings(metrics) + pageFindings).sortedByDescending { it.impact }
    return RuntimePerformanceReport(score, gradeFor(score), metrics, findings, profile)
}

/** Turns the measured numbers into the same kind of finding the page reports, so the UI has one list. */
private fun metricFindings(metrics: RuntimeMetrics): List<PerformanceFinding> = buildList {
    if (metrics.loadMs > 2_800) {
        add(PerformanceFinding(PerformanceFindingKind.SlowLoad, value = metrics.loadMs))
    }
    metrics.firstContentfulPaintMs?.let { paint ->
        if (paint > 2_200) add(PerformanceFinding(PerformanceFindingKind.SlowPaint, value = paint))
    }
    if (metrics.slowFramePercent > 15 || metrics.averageFrameMs > 20) {
        add(
            PerformanceFinding(
                kind = PerformanceFindingKind.Jank,
                value = metrics.slowFramePercent,
                extra = metrics.averageFrameMs,
            ),
        )
    }
    if (metrics.domNodes > 3_000) {
        add(PerformanceFinding(PerformanceFindingKind.DomSize, value = metrics.domNodes.toDouble()))
    }
    if (metrics.scriptBytes > 2 * MB) {
        add(PerformanceFinding(PerformanceFindingKind.ScriptWeight, value = metrics.scriptBytes.toDouble()))
    }
}

fun gradeFor(score: Int): Char = when {
    score >= 90 -> 'A'
    score >= 80 -> 'B'
    score >= 65 -> 'C'
    score >= 50 -> 'D'
    else -> 'E'
}

/** Returns the penalty of the first bucket [value] fits into, or [fallback] when it exceeds every bucket. */
private fun threshold(value: Double, vararg buckets: Pair<Double, Int>, fallback: Int): Int =
    buckets.firstOrNull { value <= it.first }?.second ?: fallback

internal fun localized(locale: AppLocale, english: String, russian: String) =
    if (locale == AppLocale.English) english else russian
