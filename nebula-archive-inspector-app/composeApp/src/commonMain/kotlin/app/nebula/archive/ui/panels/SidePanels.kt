package app.nebula.archive.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
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

@Composable
fun PerformancePanel(state: WorkspaceState, modifier: Modifier = Modifier) {
    val strings = state.strings
    val report = state.performanceReport
    LazyColumn(modifier.background(PanelRaised).padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        item {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                SectionLabel(strings.performance, Modifier.weight(1f))
                SmallAction("×", state::closePerformance)
            }
        }
        when {
            state.performanceRunning -> item {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    LinearProgressIndicator(modifier = Modifier.fillMaxWidth(), color = Nebula, trackColor = PanelSoft)
                    Text(strings.runningTest, color = TextMain, fontWeight = FontWeight.Bold)
                    Text(strings.localTestNotice, color = TextMuted, style = MaterialTheme.typography.bodySmall)
                }
            }
            state.performanceUnavailable -> item {
                Text(strings.testUnavailable, color = Coral, style = MaterialTheme.typography.bodySmall)
            }
            report == null -> item {
                Text(strings.localTestNotice, color = TextMuted, style = MaterialTheme.typography.bodySmall)
            }
            else -> {
                item {
                    Text(
                        "${report.score}/100 · ${report.grade}",
                        color = gradeColor(report.score),
                        style = MaterialTheme.typography.headlineLarge,
                        fontWeight = FontWeight.Black,
                    )
                }
                item { MetricRow(strings.loadMetric, formatMillis(report.metrics.loadMs)) }
                item { MetricRow(strings.paintMetric, report.metrics.firstContentfulPaintMs?.let(::formatMillis) ?: "—") }
                item { MetricRow(strings.framesMetric, "${report.metrics.slowFramePercent.toInt()}% · avg ${formatMillis(report.metrics.averageFrameMs)}") }
                item { MetricRow(strings.domMetric, report.metrics.domNodes.toString()) }
                item { MetricRow(strings.resourcesMetric, "${report.metrics.resourceCount} · ${formatBytes(report.metrics.decodedResourceBytes)}") }
                item { Box(Modifier.fillMaxWidth().height(1.dp).background(Border)) }
                items(report.advice.size) { index ->
                    Text("• ${report.advice[index]}", color = TextMain, style = MaterialTheme.typography.bodySmall)
                }
                item { MonoText(strings.localTestNotice, size = 9, maxLines = 3) }
            }
        }
        if (!state.performanceRunning) {
            item {
                Button(
                    onClick = state::startPerformanceTest,
                    colors = ButtonDefaults.buttonColors(containerColor = Nebula, contentColor = Space),
                    modifier = Modifier.fillMaxWidth(),
                ) { Text(strings.runTest, fontWeight = FontWeight.Black) }
            }
        }
    }
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
