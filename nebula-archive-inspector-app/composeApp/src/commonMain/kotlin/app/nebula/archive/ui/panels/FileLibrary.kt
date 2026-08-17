package app.nebula.archive.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextField
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import app.nebula.archive.formatBytes

/** Searchable list of every file in the archive; each one opens in the reader that fits it. */
@Composable
fun FileLibrary(state: WorkspaceState, modifier: Modifier = Modifier) {
    val strings = state.strings
    val project = state.project
    var query by remember(project.openedAtMs) { mutableStateOf("") }
    val visibleFiles = remember(project.openedAtMs, query) {
        val needle = query.trim().lowercase()
        if (needle.isBlank()) {
            project.entries
        } else {
            project.entries.filter { file ->
                file.path.lowercase().contains(needle) ||
                    file.extension == needle.removePrefix(".") ||
                    kindLabel(file.kind).lowercase().contains(needle)
            }
        }
    }

    Column(
        modifier.background(Brush.verticalGradient(listOf(Color(0xFF10192A), Panel)))
            .border(1.dp, Border)
            .clickable(enabled = false) {}
            .padding(12.dp),
    ) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            Column {
                SectionLabel("STRUCTURE")
                Text(strings.projectFiles, color = TextMain, fontWeight = FontWeight.Bold, modifier = Modifier.padding(top = 4.dp))
            }
            SmallAction("×", state::closeOverlay)
        }
        Spacer(Modifier.height(10.dp))
        TextField(
            value = query,
            onValueChange = { query = it },
            placeholder = { Text(strings.searchFiles, fontSize = 10.sp, color = TextMuted) },
            leadingIcon = { Text("⌕", color = Nebula, fontWeight = FontWeight.Black) },
            trailingIcon = if (query.isNotBlank()) ({ SmallAction("×", { query = "" }) }) else null,
            singleLine = true,
            colors = TextFieldDefaults.colors(
                focusedContainerColor = Color(0xFF090F1A),
                unfocusedContainerColor = Color(0xFF090F1A),
                focusedTextColor = TextMain,
                unfocusedTextColor = TextMain,
                focusedIndicatorColor = Nebula,
                unfocusedIndicatorColor = Border,
            ),
            modifier = Modifier.fillMaxWidth(),
        )
        MonoText(
            "${visibleFiles.size} / ${project.entries.size} · ${formatBytes(project.totalBytes)}",
            size = 8,
            modifier = Modifier.padding(horizontal = 4.dp, vertical = 6.dp),
        )
        LazyColumn(verticalArrangement = Arrangement.spacedBy(2.dp)) {
            if (visibleFiles.isEmpty()) {
                item { Text(strings.noMatchingFiles, color = TextMuted, fontSize = 10.sp, modifier = Modifier.padding(12.dp)) }
            }
            items(visibleFiles, key = { it.path }) { file ->
                Row(
                    Modifier.fillMaxWidth().clip(MaterialTheme.shapes.small)
                        .background(if (file.path == project.entryPoint) Color(0x1521D9F3) else Color.Transparent)
                        .clickable {
                            state.closeOverlay()
                            state.openEntry(file)
                        }
                        .padding(8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    MonoText(kindLabel(file.kind), color = kindColor(file.kind), size = 8, modifier = Modifier.width(28.dp))
                    Column(Modifier.weight(1f)) {
                        MonoText(file.path, color = TextMain, size = 10)
                        Text("${formatBytes(file.size)} · ${strings.previewFile}", color = TextMuted, fontSize = 8.sp)
                    }
                    Text("›", color = Nebula, fontSize = 15.sp, fontWeight = FontWeight.Bold)
                }
            }
        }
    }
}
