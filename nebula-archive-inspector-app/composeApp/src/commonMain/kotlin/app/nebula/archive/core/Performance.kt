package app.nebula.archive

data class RuntimeMetrics(
    val responseStartMs: Double,
    val domContentLoadedMs: Double,
    val loadMs: Double,
    val firstContentfulPaintMs: Double?,
    val averageFrameMs: Double,
    val worstFrameMs: Double,
    val slowFramePercent: Double,
    val domNodes: Int,
    val resourceCount: Int,
    val decodedResourceBytes: Long,
)

data class RuntimePerformanceReport(
    val score: Int,
    val grade: Char,
    val metrics: RuntimeMetrics,
    val advice: List<String>,
)

/**
 * Scores a measured page. Load, paint, frame pacing, DOM size, resource count and shipped JavaScript each
 * remove points, so a heavy archive cannot reach a high score on timing alone.
 */
fun evaluateRuntimePerformance(
    project: ArchiveProject,
    metrics: RuntimeMetrics,
    locale: AppLocale = AppLocale.English,
): RuntimePerformanceReport {
    val scriptBytes = project.entries.filter { it.kind == ArchiveKind.JavaScript }.sumOf { it.size }
    val paint = metrics.firstContentfulPaintMs
    var score = 100
    score -= threshold(metrics.loadMs, 900.0 to 0, 1_600.0 to 5, 2_800.0 to 12, 4_500.0 to 22, fallback = 32)
    if (paint != null) score -= threshold(paint, 700.0 to 0, 1_300.0 to 4, 2_200.0 to 10, fallback = 18)
    score -= threshold(metrics.slowFramePercent, 5.0 to 0, 15.0 to 6, 30.0 to 14, fallback = 24)
    score -= threshold(metrics.averageFrameMs, 20.0 to 0, 28.0 to 7, fallback = 12)
    score -= threshold(metrics.domNodes.toDouble(), 1_500.0 to 0, 3_000.0 to 5, 6_000.0 to 12, fallback = 20)
    score -= threshold(metrics.resourceCount.toDouble(), 120.0 to 0, 250.0 to 5, fallback = 10)
    score -= threshold(scriptBytes.toDouble(), 2 * MB to 0, 5 * MB to 5, fallback = 10)
    score = score.coerceIn(0, 100)

    val advice = buildList {
        if (metrics.loadMs > 2_800) add(localized(locale, "Reduce work performed during initial page load.", "Сократи объём работы при первоначальной загрузке страницы."))
        if (paint != null && paint > 2_200) add(localized(locale, "Prioritize above-the-fold CSS, fonts and images.", "Приоритизируй CSS, шрифты и изображения первого экрана."))
        if (metrics.slowFramePercent > 15 || metrics.averageFrameMs > 20) add(localized(locale, "Reduce heavy animation, layout recalculation and main-thread JavaScript.", "Сократи тяжёлые анимации, перерасчёт макета и JavaScript в основном потоке."))
        if (metrics.domNodes > 3_000) add(localized(locale, "Simplify the DOM and render long lists only when needed.", "Упрости DOM и отображай длинные списки только по необходимости."))
        if (metrics.resourceCount > 120) add(localized(locale, "Combine tiny resources and lazy-load non-critical media.", "Объедини мелкие ресурсы и отложи загрузку некритичных медиа."))
        if (scriptBytes > 2 * MB) add(localized(locale, "Split large JavaScript bundles and remove unused code.", "Раздели крупные JavaScript-бандлы и удали неиспользуемый код."))
        if (isEmpty()) add(localized(locale, "No major runtime bottlenecks were detected in the local preview.", "В локальном предпросмотре серьёзных узких мест не обнаружено."))
    }
    return RuntimePerformanceReport(score, gradeFor(score), metrics, advice)
}

fun gradeFor(score: Int): Char = when {
    score >= 90 -> 'A'
    score >= 80 -> 'B'
    score >= 65 -> 'C'
    score >= 50 -> 'D'
    else -> 'E'
}

private const val MB = 1024.0 * 1024.0

/** Returns the penalty of the first bucket [value] fits into, or [fallback] when it exceeds every bucket. */
private fun threshold(value: Double, vararg buckets: Pair<Double, Int>, fallback: Int): Int =
    buckets.firstOrNull { value <= it.first }?.second ?: fallback

internal fun localized(locale: AppLocale, english: String, russian: String) =
    if (locale == AppLocale.English) english else russian
