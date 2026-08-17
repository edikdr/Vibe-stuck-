package app.nebula.archive.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
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
import app.nebula.archive.AppStrings
import app.nebula.archive.RecentDocument

@Composable
fun LandingScreen(
    strings: AppStrings,
    loading: Boolean,
    error: String?,
    recentDocuments: List<RecentDocument>,
    onChooseDocument: () -> Unit,
    onOpenRecent: (RecentDocument) -> Unit,
    onToggleLocale: () -> Unit,
) {
    Column(
        Modifier.fillMaxSize()
            .background(Brush.radialGradient(listOf(Color(0xFF162B4B), Space), radius = 1_100f))
            .windowInsetsPadding(WindowInsets.safeDrawing)
            .padding(24.dp),
    ) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            Brand()
            SmallAction(strings.localeSwitch, onToggleLocale)
        }
        Spacer(Modifier.height(18.dp))
        Box(Modifier.fillMaxWidth().weight(1f)) {
            Column(
                Modifier.align(Alignment.TopCenter).fillMaxWidth().widthIn(max = 560.dp)
                    .verticalScroll(rememberScrollState())
                    .background(Brush.verticalGradient(listOf(Color(0xE8172033), Color(0xF20A0F1B))), MaterialTheme.shapes.extraLarge)
                    .padding(24.dp),
                verticalArrangement = Arrangement.spacedBy(14.dp),
            ) {
                MonoText(strings.localOnly, color = Green, size = 10)
                Text(strings.tagline, color = TextMain, style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.Black)
                Text(strings.description, color = TextMuted, style = MaterialTheme.typography.bodyMedium, lineHeight = 21.sp)
                Button(
                    enabled = !loading,
                    onClick = onChooseDocument,
                    colors = ButtonDefaults.buttonColors(containerColor = Nebula, contentColor = Space),
                    modifier = Modifier.fillMaxWidth().height(52.dp),
                ) {
                    Text(if (loading) strings.openingArchive else strings.chooseArchive, fontWeight = FontWeight.Black)
                }
                error?.let { Text(it, color = Coral, style = MaterialTheme.typography.bodySmall) }
                MonoText("ZIP · HTML · up to 4 projects · local only", size = 9)
                if (recentDocuments.isNotEmpty()) {
                    MonoText(strings.recent, color = Violet, size = 9)
                    recentDocuments.forEach { item ->
                        Row(
                            Modifier.fillMaxWidth().clip(MaterialTheme.shapes.medium).background(PanelSoft)
                                .clickable(enabled = !loading) { onOpenRecent(item) }
                                .padding(horizontal = 12.dp, vertical = 10.dp),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(10.dp),
                        ) {
                            MonoText(if (item.name.endsWith(".htm", true) || item.name.endsWith(".html", true)) "HTML" else "ZIP", color = Nebula, size = 8)
                            Text(item.name, color = TextMain, fontSize = 11.sp, maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.weight(1f))
                            Text("›", color = TextMuted)
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun Brand() {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(9.dp)) {
        Box(Modifier.size(34.dp).clip(MaterialTheme.shapes.extraLarge).background(Brush.radialGradient(listOf(Nebula, Violet, Panel))))
        Column {
            Text("NEBULA", color = TextMain, fontWeight = FontWeight.Black, fontFamily = FontFamily.Monospace, letterSpacing = 1.5.sp)
            MonoText("ARCHIVE INSPECTOR", size = 8)
        }
    }
}
