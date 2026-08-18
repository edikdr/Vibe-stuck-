package app.nebula.archive.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import app.nebula.archive.FindingSeverity
import app.nebula.archive.PagePerformance
import app.nebula.archive.PerformanceFinding
import app.nebula.archive.SourceHit
import app.nebula.archive.buildFindingAiContext
import app.nebula.archive.buildFindingsAiContext
import app.nebula.archive.buildPerformanceAiContext
import app.nebula.archive.describeFinding
import app.nebula.archive.formatBytes
import app.nebula.archive.formatMillis

/** Slider, step buttons and readout that resize the docked panel. */
@Composable
fun PanelSizeRow(state: WorkspaceState, modifier: Modifier = Modifier) {
    Row(modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        SmallAction("−", { state.setPanelExtent(state.panelExtent - 44f) })
        Slider(
            value = state.panelExtent,
            onValueChange = state::setPanelExtent,
            valueRange = state.panelExtentRange,
            modifier = Modifier.weight(1f),
        )
        SmallAction("+", { state.setPanelExtent(state.panelExtent + 44f) })
        MonoText("${state.panelExtent.toInt()}", size = 8, modifier = Modifier.padding(start = 6.dp))
    }
}

/**
 * Performance results: the score, what actually slows the page down, and how every page compares.
 *
 * Findings carry a selector or an archive path whenever the page could attribute them, which is what turns
 * a number into an action: one tap opens the guilty element in the inspector or its file in the source view.
 */
@Composable
fun PerformancePanel(state: WorkspaceState, modifier: Modifier = Modifier) {
    val strings = state.strings
    val copy = LocalCopyText.current
    val site = state.site
    val page = state.openPageResult ?: site?.pages?.firstOrNull()

    LazyColumn(modifier.background(PanelRaised).padding(14.dp), verticalArrangement = Arrangement.spacedBy(9.dp)) {
        item {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                SectionLabel(strings.performance, Modifier.weight(1f))
                if (state.networkProfile.throttled) {
                    MonoText(state.networkProfile.shortLabel, color = Amber, size = 8)
                }
                SmallAction("×", state::closePanel)
            }
        }

        when {
            state.testing -> item { TestProgress(state) }

            site != null && site.pages.isNotEmpty() -> {
                item { ScoreHeader(state, site.averageScore, site.grade) }
                if (site.pages.size > 1) {
                    site.worst?.let { worst ->
                        item {
                            MonoText(
                                "${strings.worstPage}: ${worst.path} · ${worst.report.score}/100",
                                color = gradeColor(worst.report.score),
                            )
                        }
                    }
                }
                page?.let { current ->
                    if (site.pages.size > 1) item { MonoText(current.path, color = TextMain, size = 10) }
                    item { MetricRow(strings.loadMetric, formatMillis(current.report.metrics.loadMs)) }
                    item {
                        MetricRow(
                            strings.paintMetric,
                            current.report.metrics.firstContentfulPaintMs?.let(::formatMillis) ?: "—",
                        )
                    }
                    item {
                        MetricRow(
                            strings.framesMetric,
                            "${current.report.metrics.slowFramePercent.toInt()}% · ${formatMillis(current.report.metrics.averageFrameMs)}",
                        )
                    }
                    item { MetricRow(strings.domMetric, current.report.metrics.domNodes.toString()) }
                    item {
                        MetricRow(
                            strings.resourcesMetric,
                            "${current.report.metrics.resourceCount} · ${formatBytes(current.report.metrics.decodedResourceBytes)}",
                        )
                    }

                    item { Divider() }
                    item { SectionLabel(strings.problems, color = Coral) }
                    if (current.report.findings.isEmpty()) {
                        item { Text(strings.noProblems, color = TextMuted, style = MaterialTheme.typography.bodySmall) }
                    } else {
                        items(current.report.findings) { finding ->
                            FindingRow(state, current.path, finding)
                        }
                    }
                }
                if (site.pages.size > 1) {
                    item { Divider() }
                    item { SectionLabel(strings.pages) }
                    itemsIndexed(site.pages, key = { _, item -> item.path }) { _, item ->
                        PageRow(state, item, selected = item.path == page?.path)
                    }
                }
                item {
                    MonoText(
                        if (state.networkProfile.throttled) {
                            "${strings.measuredUnder} ${state.networkProfile.shortLabel} · ${strings.simulatedNetwork}"
                        } else {
                            strings.localTestNotice
                        },
                        size = 8,
                        maxLines = 3,
                    )
                }
            }

            state.testFailed -> item {
                Text(strings.testUnavailable, color = Coral, style = MaterialTheme.typography.bodySmall)
            }

            else -> item {
                Text(strings.localTestNotice, color = TextMuted, style = MaterialTheme.typography.bodySmall)
            }
        }

        if (!state.testing) {
            item {
                Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    ToolAction("◴", strings.runTest, state::startPageTest, active = true)
                    if (!state.singlePage) ToolAction("⇊", strings.runSiteTest, state::startSiteTest)
                }
            }
            if (site != null && site.pages.isNotEmpty()) {
                item {
                    Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        SmallAction(strings.copyReportForAi, { copy(buildPerformanceAiContext(state.project.name, site, strings)) }, active = true)
                        page?.let { current ->
                            SmallAction(strings.copyProblemsForAi, { copy(buildFindingsAiContext(state.project.name, current.path, current.report, strings)) })
                        }
                    }
                }
            }
            if (state.singlePage) {
                item { MonoText(strings.singlePageOnly, size = 8, maxLines = 2) }
            }
        }
    }
}

@Composable
private fun TestProgress(state: WorkspaceState) {
    val strings = state.strings
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        LinearProgressIndicator(modifier = Modifier.fillMaxWidth(), color = Nebula, trackColor = PanelSoft)
        Text(
            if (state.testTotal > 1) {
                "${strings.testingPage} ${state.testProgress}/${state.testTotal}"
            } else {
                strings.runningTest
            },
            color = TextMain,
            fontWeight = FontWeight.Bold,
        )
        SmallAction(strings.stopTest, state::stopTest)
    }
}

@Composable
private fun ScoreHeader(state: WorkspaceState, score: Int, grade: Char) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.Bottom, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(
            "$score/100 · $grade",
            color = gradeColor(score),
            style = MaterialTheme.typography.headlineMedium,
            fontWeight = FontWeight.Black,
        )
        state.site?.takeIf { it.pages.size > 1 }?.let { site ->
            MonoText("${site.pages.size} ${state.strings.pages.lowercase()}", modifier = Modifier.padding(bottom = 4.dp))
        }
    }
}

/** One cause, with the actions that make it fixable: open the element, open the file, hand it to an AI. */
@Composable
private fun FindingRow(state: WorkspaceState, pagePath: String, finding: PerformanceFinding) {
    val strings = state.strings
    val copy = LocalCopyText.current
    val text = describeFinding(finding, strings)
    val color = severityColor(finding.severity)
    val sourceEntry = finding.target.takeIf { it.isNotBlank() }?.let { state.project.file(it) }

    Card(borderColor = color.copy(alpha = .4f)) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(7.dp)) {
            Box(Modifier.size(7.dp).clip(MaterialTheme.shapes.extraLarge).background(color))
            Text(
                text.title,
                color = TextMain,
                fontSize = 11.sp,
                fontWeight = FontWeight.Bold,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier.weight(1f),
            )
        }
        if (text.detail.isNotBlank()) {
            MonoText(text.detail, color = CodeText, size = 9, maxLines = 2, modifier = Modifier.padding(top = 4.dp))
        }
        val where = finding.target.ifBlank { finding.selector }
        if (where.isNotBlank()) {
            MonoText(where, size = 9, maxLines = 2, modifier = Modifier.padding(top = 4.dp))
        }
        Row(Modifier.horizontalScroll(rememberScrollState()).padding(top = 7.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            if (finding.selector.isNotBlank()) {
                SmallAction("⌖ ${strings.showElement}", { state.inspectSelector(finding.selector) })
            }
            sourceEntry?.let { entry ->
                SmallAction(strings.openSource, { state.openSource(SourceHit(entry.path, 1, "")) })
            }
            SmallAction("AI", { copy(buildFindingAiContext(state.project.name, pagePath, finding, state.networkProfile, strings)) })
        }
    }
}

@Composable
private fun PageRow(state: WorkspaceState, page: PagePerformance, selected: Boolean) {
    val color = gradeColor(page.report.score)
    Row(
        Modifier.fillMaxWidth().clip(MaterialTheme.shapes.small)
            .background(if (selected) color.copy(alpha = .14f) else Color(0xFF080D17))
            .border(1.dp, color.copy(alpha = if (selected) .8f else .3f), MaterialTheme.shapes.small)
            .clickable { state.showPageResult(page) }
            .padding(horizontal = 8.dp, vertical = 7.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        MonoText("${page.report.score}", color = color, size = 10)
        MonoText(page.path, color = TextMain, size = 9, modifier = Modifier.weight(1f))
        if (page.report.findings.isNotEmpty()) {
            MonoText("${page.report.findings.size}", color = TextMuted, size = 8)
        }
    }
}

private fun severityColor(severity: FindingSeverity) = when (severity) {
    FindingSeverity.High -> Coral
    FindingSeverity.Medium -> Amber
    FindingSeverity.Low -> TextMuted
}

@Composable
private fun Divider() {
    Box(Modifier.fillMaxWidth().height(1.dp).background(Border))
}

@Composable
fun DiagnosticsPanel(state: WorkspaceState, modifier: Modifier = Modifier) {
    val strings = state.strings
    LazyColumn(modifier.background(PanelRaised).padding(14.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        item {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Column(Modifier.weight(1f)) {
                    SectionLabel(strings.diagnostics)
                    MonoText("${state.issues.size}")
                }
                if (state.issues.isNotEmpty()) SmallAction(strings.clearIssues, state::clearIssues)
                SmallAction("×", state::closePanel)
            }
        }
        if (state.issues.isEmpty()) {
            item { Text(strings.noIssues, color = TextMuted, style = MaterialTheme.typography.bodySmall) }
        } else {
            itemsIndexed(state.issues.asReversed(), key = { index, issue -> "${issue.kind}:${issue.resource}:$index" }) { _, issue ->
                Column(Modifier.fillMaxWidth().clip(MaterialTheme.shapes.small).background(Color(0xFF080D17)).padding(10.dp)) {
                    MonoText(issue.kind.name.uppercase(), color = issueColor(issue.kind), size = 8)
                    Text(issue.message, color = TextMain, fontSize = 10.sp, maxLines = 4, overflow = TextOverflow.Ellipsis, modifier = Modifier.padding(top = 5.dp))
                    if (issue.resource.isNotBlank()) {
                        MonoText(issue.resource, size = 8, maxLines = 2, modifier = Modifier.padding(top = 5.dp))
                    }
                }
            }
        }
    }
}
