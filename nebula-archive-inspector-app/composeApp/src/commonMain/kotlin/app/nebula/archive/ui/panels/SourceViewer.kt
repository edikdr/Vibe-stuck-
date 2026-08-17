package app.nebula.archive.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.withFrameNanos
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
import app.nebula.archive.SourceDisplay
import app.nebula.archive.SourceHit
import app.nebula.archive.sourceDisplay
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

private const val MAX_LINES = 20_000

/** Read-only, line-numbered source view that jumps to the detected line. Minified files are formatted first. */
@Composable
fun SourceViewer(state: WorkspaceState, source: SourceHit, modifier: Modifier = Modifier) {
    val strings = state.strings
    val copy = LocalCopyText.current
    var display by remember(source.path) { mutableStateOf<SourceDisplay?>(null) }
    val listState = rememberLazyListState()

    LaunchedEffect(source.path) {
        display = withContext(Dispatchers.Default) {
            sourceDisplay(source.path, state.project.text(source.path).orEmpty(), maxLines = MAX_LINES + 1)
        }
    }
    LaunchedEffect(display, source.line) {
        val lines = display?.lines ?: return@LaunchedEffect
        if (lines.isEmpty()) return@LaunchedEffect
        withFrameNanos { }
        // A formatted file no longer matches the line the search found, so it opens at the top instead.
        val target = if (display?.formatted == true) 0 else source.line - 3
        listState.scrollToItem(target.coerceIn(0, lines.lastIndex))
    }

    Column(
        modifier.fillMaxSize().background(Brush.verticalGradient(listOf(Color(0xFF101A2C), Space))).padding(10.dp),
    ) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Column(Modifier.weight(1f)) {
                SectionLabel(strings.sourceReadOnly)
                Text(source.path, color = TextMain, fontWeight = FontWeight.Bold, fontFamily = FontFamily.Monospace, fontSize = 12.sp, maxLines = 1, overflow = TextOverflow.Ellipsis)
            }
            SmallAction("×", state::closeOverlay)
        }
        Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(vertical = 8.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            SmallAction(strings.copyPath, { copy("${source.path}:${source.line}") }, active = true)
            SmallAction(
                strings.copyCode,
                {
                    val lines = display?.lines.orEmpty()
                    val snippet = source.excerpt.ifBlank {
                        val focus = if (display?.formatted == true) 0 else source.line - 1
                        val start = (focus - 3).coerceAtLeast(0)
                        val end = (focus + 8).coerceIn(start, lines.size)
                        lines.subList(start, end).joinToString("\n")
                    }
                    copy("${source.path}:${source.line}\n$snippet")
                },
                enabled = display != null,
            )
        }
        if (display?.formatted == true) {
            Text(strings.formattedSource, color = Amber, fontSize = 9.sp, modifier = Modifier.padding(bottom = 7.dp))
        }
        when (val loaded = display) {
            null -> Text(strings.loadingSource, color = TextMuted, modifier = Modifier.padding(12.dp))
            else -> LazyColumn(
                state = listState,
                modifier = Modifier.fillMaxSize().clip(MaterialTheme.shapes.medium).background(CodeBackground).padding(vertical = 6.dp),
            ) {
                val lines = loaded.lines.take(MAX_LINES)
                itemsIndexed(lines) { index, codeLine ->
                    SourceLine(
                        number = index + 1,
                        code = codeLine,
                        highlighted = !loaded.formatted && index + 1 == source.line,
                    )
                }
                if (loaded.lines.size > MAX_LINES) {
                    item { Text(strings.sourceTruncated, color = Amber, fontSize = 9.sp, modifier = Modifier.padding(12.dp)) }
                }
            }
        }
    }
}

@Composable
private fun SourceLine(number: Int, code: String, highlighted: Boolean) {
    val lineScrollState = rememberScrollState()
    Row(
        Modifier.fillMaxWidth().background(if (highlighted) Color(0x3321D9F3) else Color.Transparent).padding(vertical = 2.dp),
        verticalAlignment = Alignment.Top,
    ) {
        Text(
            number.toString().padStart(5, ' '),
            color = if (highlighted) Nebula else TextFaint,
            fontSize = 9.sp,
            fontFamily = FontFamily.Monospace,
            modifier = Modifier.width(48.dp).padding(horizontal = 6.dp),
        )
        Text(
            code.ifEmpty { " " },
            color = Color(0xFFD4DEEE),
            fontSize = 9.sp,
            lineHeight = 14.sp,
            maxLines = 1,
            softWrap = false,
            fontFamily = FontFamily.Monospace,
            modifier = Modifier.weight(1f, fill = false).horizontalScroll(lineScrollState).padding(end = 12.dp),
        )
    }
}
