package app.nebula.archive

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class PerformanceTest {
    private val strings = AppStrings.English

    @Test
    fun scorePenalizesSlowAndJankyPages() {
        val fast = evaluateRuntimePerformance(metrics(load = 600.0, slowFrames = 1.0, domNodes = 400))
        val slow = evaluateRuntimePerformance(metrics(load = 5_500.0, slowFrames = 45.0, domNodes = 7_000))

        assertTrue(fast.score > slow.score)
        assertEquals('A', fast.grade)
        assertTrue(slow.findings.size >= 2, "a slow page must name what is wrong: ${slow.findings}")
    }

    @Test
    fun scoreOnlyCountsJavaScriptThePageActuallyLoaded() {
        // The regression this guards: weight used to be summed over the whole archive, so a multi-page site
        // with code splitting was penalised for bundles the measured page never executed.
        val light = evaluateRuntimePerformance(metrics(load = 600.0, slowFrames = 1.0, domNodes = 400))
        val heavy = evaluateRuntimePerformance(
            metrics(load = 600.0, slowFrames = 1.0, domNodes = 400).copy(scriptBytes = 6L * 1024 * 1024),
        )

        assertTrue(light.score > heavy.score)
        assertTrue(heavy.findings.any { it.kind == PerformanceFindingKind.ScriptWeight })
        assertTrue(light.findings.none { it.kind == PerformanceFindingKind.ScriptWeight })
    }

    @Test
    fun layoutShiftAndLongTasksLowerTheScore() {
        val clean = evaluateRuntimePerformance(metrics(load = 600.0, slowFrames = 1.0, domNodes = 400))
        val shifting = evaluateRuntimePerformance(
            metrics(load = 600.0, slowFrames = 1.0, domNodes = 400)
                .copy(layoutShiftScore = 0.4, longTaskTotalMs = 800.0, longTaskCount = 4),
        )
        assertTrue(clean.score > shifting.score)
    }

    @Test
    fun findingsAreSortedBySeverityThenSize() {
        val report = evaluateRuntimePerformance(
            metrics = metrics(load = 600.0, slowFrames = 1.0, domNodes = 400),
            pageFindings = listOf(
                PerformanceFinding(PerformanceFindingKind.CompositingCost, value = 30.0),
                PerformanceFinding(PerformanceFindingKind.RenderBlocking, target = "vendor.js", value = 640_000.0),
            ),
        )
        assertEquals(PerformanceFindingKind.RenderBlocking, report.findings.first().kind)
        assertEquals(FindingSeverity.Low, report.findings.last().severity)
    }

    @Test
    fun everyFindingKindHasReadableText() {
        PerformanceFindingKind.entries.forEach { kind ->
            val text = describeFinding(PerformanceFinding(kind, value = 1_234.0, extra = 2.0), strings)
            assertTrue(text.title.isNotBlank(), "no title for $kind")
            assertTrue(text.title.length < 40, "title for $kind is too long for a phone row")
        }
    }

    @Test
    fun oversizedImageReadsAsAConcreteInstruction() {
        val finding = PerformanceFinding(
            kind = PerformanceFindingKind.OversizedImage,
            target = "assets/hero.png",
            selector = "body > main > img.banner",
            note = "4200×3000 → 390×280",
            value = 1_887_436.0,
            extra = 0.98,
        )
        val text = describeFinding(finding, strings)
        assertEquals("Oversized image", text.title)
        assertTrue(text.detail.contains("4200×3000 → 390×280"), text.detail)
        assertTrue(text.detail.contains("98%"), text.detail)
        assertEquals(FindingSeverity.High, finding.severity)
    }

    @Test
    fun siteReportAggregatesPagesAndNamesTheWorst() {
        val site = SitePerformance(
            pages = listOf(
                PagePerformance("index.html", report(90)),
                PagePerformance("catalog.html", report(58)),
            ),
            profile = NetworkProfile.Slow3G,
        )
        assertEquals(74, site.averageScore)
        assertEquals("catalog.html", site.worst?.path)
    }

    @Test
    fun performanceAiBlockIsCompactAndNamesTheCause() {
        val findings = listOf(
            PerformanceFinding(
                kind = PerformanceFindingKind.LayoutShift,
                target = "img.banner",
                selector = "body > main > img.banner",
                value = 0.31,
                extra = 210.0,
            ),
            PerformanceFinding(PerformanceFindingKind.RenderBlocking, target = "assets/vendor.js", value = 640_000.0),
        )
        val site = SitePerformance(
            pages = listOf(PagePerformance("index.html", report(72, findings))),
            profile = NetworkProfile.Slow3G,
        )
        val context = buildPerformanceAiContext("my-site", site, strings)

        assertTrue(context.startsWith("NEBULA perf · my-site · 3G (simulated)"), context)
        assertTrue(context.contains("index.html 72/100 · load"), context)
        assertTrue(context.contains("! Layout shift"), context)
        assertTrue(context.contains("body>main>img.banner"), context)
        assertTrue(context.contains("CLS 0.31"), context)
        assertTrue(context.contains("assets/vendor.js"), context)
        assertTrue(!context.contains("```"), "the block must stay fence-free")
        assertTrue(context.length < 900, "block grew to ${context.length} characters")
    }

    @Test
    fun singleFindingBlockCarriesSelectorAndTarget() {
        val finding = PerformanceFinding(
            kind = PerformanceFindingKind.OversizedImage,
            target = "assets/hero.png",
            selector = "body > main > img.banner",
            note = "4200×3000 → 390×280",
            value = 1_887_436.0,
            extra = 0.98,
        )
        val context = buildFindingAiContext("my-site", "index.html", finding, NetworkProfile.Unthrottled, strings)

        assertTrue(context.contains("no throttling"), context)
        assertTrue(context.contains("at assets/hero.png"), context)
        assertTrue(context.contains("selector body>main>img.banner"), context)
        assertTrue(context.length < 400, context)
    }

    @Test
    fun problemsBlockFallsBackWhenNothingWasFound() {
        val context = buildFindingsAiContext("my-site", "index.html", report(98), strings)
        assertTrue(context.contains(strings.noProblems), context)
    }

    @Test
    fun networkProfilesCycleThroughEveryPreset() {
        var profile = NetworkProfile.Unthrottled
        val seen = mutableListOf(profile)
        repeat(NetworkProfile.entries.size - 1) {
            profile = profile.next()
            seen += profile
        }
        assertEquals(NetworkProfile.entries.toList(), seen)
        assertEquals(NetworkProfile.Unthrottled, profile.next())
        assertTrue(NetworkProfile.Slow3G.throttled)
        assertTrue(!NetworkProfile.Unthrottled.throttled)
    }

    private fun report(score: Int, findings: List<PerformanceFinding> = emptyList()) = RuntimePerformanceReport(
        score = score,
        grade = gradeFor(score),
        metrics = metrics(load = 1_200.0, slowFrames = 4.0, domNodes = 900),
        findings = findings,
        profile = NetworkProfile.Slow3G,
    )

    private fun metrics(load: Double, slowFrames: Double, domNodes: Int) = RuntimeMetrics(
        responseStartMs = 20.0,
        domContentLoadedMs = load * .8,
        loadMs = load,
        firstContentfulPaintMs = load * .5,
        averageFrameMs = if (slowFrames > 15) 29.0 else 16.7,
        worstFrameMs = if (slowFrames > 15) 90.0 else 20.0,
        slowFramePercent = slowFrames,
        domNodes = domNodes,
        resourceCount = if (domNodes > 3_000) 280 else 30,
        decodedResourceBytes = 512_000,
    )
}
