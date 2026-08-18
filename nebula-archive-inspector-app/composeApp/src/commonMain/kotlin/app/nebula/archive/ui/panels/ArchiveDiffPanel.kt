package app.nebula.archive.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import app.nebula.archive.ArchiveProject
import app.nebula.archive.ChangeKind
import app.nebula.archive.DiffLineKind
import app.nebula.archive.FileChange
import app.nebula.archive.TextDiff
import app.nebula.archive.buildDiffAiContext
import app.nebula.archive.diffArchives
import app.nebula.archive.diffText
import app.nebula.archive.formatBytes
import app.nebula.archive.signedBytes
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/**
 * Compares two open archives — what a rewrite actually changed.
 *
 * The comparison itself is pure logic in `core/ArchiveDiff.kt`; this only picks the other side, lists the
 * changes and shows a line diff for text files.
 */
@Composable
fun ArchiveDiffPanel(
    state: WorkspaceState,
    projects: List<ArchiveProject>,
    modifier: Modifier = Modifier,
) {
    val strings = state.strings
    val copy = LocalCopyText.current
    val others = projects.filter { it.openedAtMs != state.project.openedAtMs }
    var baseline by remember(projects.size) { mutableStateOf(others.firstOrNull()) }
    var openFile by remember { mutableStateOf<FileChange?>(null) }

    Column(
        modifier.fillMaxSize().background(Brush.verticalGradient(listOf(Color(0xFF101A2C), Space))).padding(10.dp),
    ) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Column(Modifier.weight(1f)) {
                SectionLabel(strings.compareArchives)
                Text(
                    baseline?.let { "${it.name} → ${state.project.name}" } ?: state.project.name,
                    color = TextMain,
                    fontWeight = FontWeight.Bold,
                    fontFamily = FontFamily.Monospace,
                    fontSize = 12.sp,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }
            SmallAction("×", state::closeOverlay)
        }

        if (others.isEmpty()) {
            Text(
                strings.openSecondArchive,
                color = TextMuted,
                style = MaterialTheme.typography.bodySmall,
                modifier = Modifier.padding(top = 14.dp),
            )
            return@Column
        }

        if (others.size > 1) {
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(top = 8.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                others.forEach { candidate ->
                    SmallAction(candidate.name, { baseline = candidate }, active = candidate.openedAtMs == baseline?.openedAtMs)
                }
            }
        }

        val from = baseline ?: return@Column
        val diff = remember(from.openedAtMs, state.project.openedAtMs) { diffArchives(from, state.project) }
        val file = openFile

        if (file != null) {
            FileDiffView(state, from, file) { openFile = null }
            return@Column
        }

        Row(Modifier.fillMaxWidth().padding(top = 10.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            DiffCount("+${diff.added}", Green)
            DiffCount("−${diff.removed}", Coral)
            DiffCount("~${diff.modified}", Amber)
            MonoText(signedBytes(diff.byteDelta), color = TextMain, size = 10, modifier = Modifier.weight(1f))
        }
        MonoText("${diff.unchangedCount} ${strings.unchangedFiles}", size = 8, modifier = Modifier.padding(top = 4.dp))

        Row(Modifier.fillMaxWidth().padding(vertical = 8.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            SmallAction(strings.copyDiffForAi, { copy(buildDiffAiContext(diff)) }, active = true)
        }

        if (diff.identical) {
            Text(strings.archivesIdentical, color = Green, style = MaterialTheme.typography.bodySmall)
            return@Column
        }

        LazyColumn(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(3.dp)) {
            items(diff.changes, key = { it.path }) { change ->
                ChangeRow(change) { if (change.comparable) openFile = change }
            }
        }
    }
}

@Composable
private fun DiffCount(text: String, color: Color) {
    Text(text, color = color, fontSize = 13.sp, fontWeight = FontWeight.Black, fontFamily = FontFamily.Monospace)
}

@Composable
private fun ChangeRow(change: FileChange, onOpen: () -> Unit) {
    val color = when (change.kind) {
        ChangeKind.Added -> Green
        ChangeKind.Removed -> Coral
        ChangeKind.Modified -> Amber
    }
    val marker = when (change.kind) {
        ChangeKind.Added -> "+"
        ChangeKind.Removed -> "−"
        ChangeKind.Modified -> "~"
    }
    Row(
        Modifier.fillMaxWidth().clip(MaterialTheme.shapes.small).background(Color(0xFF080D17))
            .border(1.dp, color.copy(alpha = .3f), MaterialTheme.shapes.small)
            .clickable(enabled = change.comparable, onClick = onOpen)
            .padding(horizontal = 8.dp, vertical = 7.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        MonoText(marker, color = color, size = 11, modifier = Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            MonoText(change.path, color = TextMain, size = 9, maxLines = 2)
            MonoText(
                when (change.kind) {
                    ChangeKind.Added -> formatBytes(change.newSize)
                    ChangeKind.Removed -> formatBytes(change.oldSize)
                    ChangeKind.Modified ->
                        "${formatBytes(change.oldSize)} → ${formatBytes(change.newSize)} · ${signedBytes(change.sizeDelta)}"
                },
                size = 8,
            )
        }
        if (change.comparable) Text("›", color = Nebula, fontSize = 14.sp)
    }
}

/** Line diff of one text file, loaded off the main thread. */
@Composable
private fun FileDiffView(
    state: WorkspaceState,
    from: ArchiveProject,
    change: FileChange,
    onBack: () -> Unit,
) {
    val strings = state.strings
    var diff by remember(change.path) { mutableStateOf<TextDiff?>(null) }

    LaunchedEffect(change.path, from.openedAtMs) {
        diff = withContext(Dispatchers.Default) {
            diffText(
                from.text(change.path).orEmpty(),
                state.project.text(change.path).orEmpty(),
            )
        }
    }

    Row(Modifier.fillMaxWidth().padding(top = 10.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        SmallAction("‹", onBack)
        MonoText(change.path, color = TextMain, size = 10, maxLines = 2, modifier = Modifier.weight(1f))
        diff?.let { MonoText("+${it.added} −${it.removed}", color = Nebula, size = 9) }
    }

    when (val loaded = diff) {
        null -> Text(strings.loadingSource, color = TextMuted, modifier = Modifier.padding(12.dp))
        else -> {
            if (loaded.approximate) {
                MonoText(strings.diffApproximate, color = Amber, size = 8, maxLines = 2, modifier = Modifier.padding(vertical = 6.dp))
            }
            LazyColumn(
                Modifier.fillMaxSize().padding(top = 6.dp).clip(MaterialTheme.shapes.medium).background(CodeBackground).padding(vertical = 6.dp),
            ) {
                items(loaded.lines.size) { index ->
                    val line = loaded.lines[index]
                    DiffLineRow(line.kind, line.text, line.oldNumber, line.newNumber)
                }
                if (loaded.truncated) {
                    item { MonoText(strings.sourceTruncated, color = Amber, size = 9, modifier = Modifier.padding(12.dp)) }
                }
            }
        }
    }
}

@Composable
private fun DiffLineRow(kind: DiffLineKind, text: String, oldNumber: Int?, newNumber: Int?) {
    val background = when (kind) {
        DiffLineKind.Added -> Color(0x2266E8BD)
        DiffLineKind.Removed -> Color(0x22FF9E9E)
        DiffLineKind.Context -> Color.Transparent
    }
    val marker = when (kind) {
        DiffLineKind.Added -> "+"
        DiffLineKind.Removed -> "−"
        DiffLineKind.Context -> " "
    }
    val color = when (kind) {
        DiffLineKind.Added -> Green
        DiffLineKind.Removed -> Coral
        DiffLineKind.Context -> Color(0xFFD4DEEE)
    }
    val lineScrollState = rememberScrollState()
    Row(
        Modifier.fillMaxWidth().background(background).padding(vertical = 2.dp),
        verticalAlignment = Alignment.Top,
    ) {
        MonoText(
            (oldNumber?.toString() ?: "").padStart(5, ' '),
            color = TextFaint,
            size = 9,
            modifier = Modifier.width(38.dp),
        )
        MonoText(
            (newNumber?.toString() ?: "").padStart(5, ' '),
            color = TextFaint,
            size = 9,
            modifier = Modifier.width(38.dp),
        )
        MonoText(marker, color = color, size = 9, modifier = Modifier.width(12.dp))
        Text(
            text.ifEmpty { " " },
            color = color,
            fontSize = 9.sp,
            lineHeight = 14.sp,
            maxLines = 1,
            softWrap = false,
            fontFamily = FontFamily.Monospace,
            modifier = Modifier.weight(1f, fill = false).horizontalScroll(lineScrollState).padding(end = 12.dp),
        )
    }
}
