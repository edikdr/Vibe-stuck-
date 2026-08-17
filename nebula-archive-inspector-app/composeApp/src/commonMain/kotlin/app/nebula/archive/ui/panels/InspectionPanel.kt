package app.nebula.archive.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
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
import app.nebula.archive.ElementProperty
import app.nebula.archive.ElementReference
import app.nebula.archive.InspectedElement
import app.nebula.archive.SelectedElement
import app.nebula.archive.SourceHit
import app.nebula.archive.buildAiContext

/**
 * The inspector: what was picked, what it looks like, and where it comes from.
 *
 * One panel serves a single element and a whole multi-selection, because everything except the layout of the
 * cards is identical — including the AI block, which simply receives one element or several.
 */
@Composable
fun InspectionPanel(state: WorkspaceState, wide: Boolean, modifier: Modifier = Modifier) {
    val strings = state.strings
    val copy = LocalCopyText.current
    val elements = state.inspected
    val group = elements.size > 1

    Column(
        modifier.background(Brush.verticalGradient(listOf(Color(0xFF151F34), Color(0xFF0B111E))))
            .border(1.dp, Border)
            .padding(horizontal = 12.dp),
    ) {
        ResizeHandle(wide, state::resizePanelBy)

        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Column(Modifier.weight(1f)) {
                SectionLabel(if (group) strings.groupInspection else strings.inspector)
                Text(
                    if (group) "${elements.size} × ${elements.joinToString(" ") { "<${it.tag}>" }}" else elements.firstOrNull()?.selector.orEmpty(),
                    color = TextMain,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold,
                    fontSize = 12.sp,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }
            SmallAction("×", state::closeInspection)
        }

        if (state.selection.isNotEmpty()) {
            SelectionStrip(state, Modifier.padding(top = 8.dp))
        }

        Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(top = 8.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            ToolAction("AI", if (group) strings.copyAllForAi else strings.copyForAi, active = true, onClick = {
                copy(buildAiContext(state.project.name, state.pagePath, elements, state.sources))
            })
            ToolAction("⌖", strings.selectAnother, onClick = state::startInspecting)
            if (!group) {
                elements.firstOrNull()?.let { element ->
                    ToolAction("⧉", strings.copySelector, onClick = { copy(element.selector) })
                }
            }
        }

        PanelSizeRow(state, Modifier.padding(vertical = 4.dp))
        Box(Modifier.fillMaxWidth().height(1.dp).background(Border))

        LazyColumn(
            Modifier.fillMaxSize().padding(top = 9.dp),
            verticalArrangement = Arrangement.spacedBy(9.dp),
        ) {
            if (group) {
                items(elements, key = { it.id }) { element ->
                    GroupElementCard(state, element, state.sources[element.id].orEmpty())
                }
            } else {
                elements.firstOrNull()?.let { element -> singleElement(state, element) }
            }
            item { Box(Modifier.height(12.dp)) }
        }
    }
}

private fun androidx.compose.foundation.lazy.LazyListScope.singleElement(
    state: WorkspaceState,
    element: InspectedElement,
) {
    val strings = state.strings

    item { ElementFactsCard(state, element) }

    if (element.text.isNotBlank()) {
        item {
            Text(element.text, color = TextMuted, style = MaterialTheme.typography.bodySmall, maxLines = 4, overflow = TextOverflow.Ellipsis)
        }
    }
    if (element.styles.isNotEmpty()) {
        item { PropertyCard(strings.computedStyles, element.styles, Nebula) }
    }
    if (element.attributes.isNotEmpty()) {
        item { PropertyCard(strings.attributes, element.attributes, Violet) }
    }
    item { DomTreeCard(state, element) }
    if (element.outerHtml.isNotBlank()) {
        item {
            val copy = LocalCopyText.current
            CodeCard(strings.liveDom, element.outerHtml.take(1_800), listOf(strings.copyCode to { copy(element.outerHtml) }))
        }
    }
    item {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            Text(strings.sourceFiles, color = TextMuted, fontSize = 9.sp, fontWeight = FontWeight.Bold, letterSpacing = .8.sp)
            MonoText("${state.sources[element.id].orEmpty().size} ${strings.sourceCandidates}", color = Nebula, size = 8)
        }
    }
    val hits = state.sources[element.id].orEmpty()
    if (hits.isEmpty()) {
        item { Text(strings.noSource, color = TextMuted, style = MaterialTheme.typography.bodySmall) }
    } else {
        items(hits, key = { "${it.path}:${it.line}" }) { hit -> SourceCandidateCard(state, hit) }
    }
}

/** Identity, box and page of one element — the facts a layout problem is usually diagnosed from. */
@Composable
private fun ElementFactsCard(state: WorkspaceState, element: InspectedElement) {
    val strings = state.strings
    Card(borderColor = Color(0xFF22304A)) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(7.dp)) {
            if (element.id > 0) SelectionBadge(element.id, domDepthColor(element.depth), selected = true)
            MonoText("<${element.tag}>", color = Violet)
            MonoText(state.pagePath, modifier = Modifier.weight(1f))
        }
        Row(Modifier.padding(top = 8.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Fact(strings.boxModel, element.size)
            Fact("xy", element.position)
            Fact(strings.children, element.childCount.toString())
        }
        if (element.classes.isNotEmpty()) {
            MonoText(
                element.classes.joinToString(" ") { ".$it" },
                color = Green,
                size = 9,
                maxLines = 2,
                modifier = Modifier.padding(top = 8.dp),
            )
        }
    }
}

@Composable
private fun Fact(label: String, value: String) {
    Column {
        MonoText(label.uppercase(), color = TextFaint, size = 8)
        Text(value, color = TextMain, fontSize = 11.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold)
    }
}

@Composable
private fun PropertyCard(title: String, properties: List<ElementProperty>, accent: Color) {
    Card {
        SectionLabel(title, color = accent)
        Column(Modifier.padding(top = 6.dp)) {
            properties.forEach { property -> PropertyRow(property.name, property.value, accent) }
        }
    }
}

/** The running multi-selection: numbered chips, plus the action that inspects all of them at once. */
@Composable
fun SelectionStrip(state: WorkspaceState, modifier: Modifier = Modifier) {
    val strings = state.strings
    Column(
        modifier.fillMaxWidth().clip(MaterialTheme.shapes.small).background(Color(0xFF08101C)).padding(8.dp),
        verticalArrangement = Arrangement.spacedBy(7.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            MonoText("${strings.selectedElements} ${state.selection.size}", color = Nebula, modifier = Modifier.weight(1f))
            if (state.selection.size > 1) {
                SmallAction("${strings.selectAllAtOnce} (${state.selection.size})", state::inspectWholeSelection, active = true)
            }
            SmallAction(strings.clearSelection, state::clearSelection)
        }
        Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            state.selection.forEach { selected -> SelectionChip(state, selected) }
        }
    }
}

@Composable
private fun SelectionChip(state: WorkspaceState, selected: SelectedElement) {
    val color = domDepthColor(selected.depth)
    val open = state.inspected.any { it.id == selected.id }
    Row(
        Modifier.clip(MaterialTheme.shapes.small)
            .background(if (open) color.copy(alpha = .16f) else PanelRaised)
            .border(1.dp, color.copy(alpha = if (open) .9f else .4f), MaterialTheme.shapes.small)
            .clickable { state.inspectSelected(selected.id) }
            .padding(horizontal = 7.dp, vertical = 5.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        SelectionBadge(selected.id, color, open)
        MonoText("<${selected.tag}>", color = color)
        Box(
            Modifier.size(14.dp).clip(MaterialTheme.shapes.extraLarge).clickable { state.dropSelected(selected.id) },
            contentAlignment = Alignment.Center,
        ) {
            Text("×", color = TextMuted, fontSize = 11.sp)
        }
    }
}

/** Compact per-element card inside a group inspection. */
@Composable
private fun GroupElementCard(state: WorkspaceState, element: InspectedElement, hits: List<SourceHit>) {
    val strings = state.strings
    val color = domDepthColor(element.depth)
    Card(borderColor = color.copy(alpha = .45f)) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(7.dp)) {
            SelectionBadge(element.id, color, selected = true)
            MonoText("<${element.tag}>", color = color)
            MonoText(element.size, modifier = Modifier.weight(1f))
            SmallAction("⌖", { state.inspectSelector(element.selector) })
        }
        MonoText(element.selector, color = TextMain, size = 9, maxLines = 2, modifier = Modifier.padding(top = 6.dp))
        if (element.text.isNotBlank()) {
            Text(element.text, color = TextMuted, fontSize = 10.sp, maxLines = 2, overflow = TextOverflow.Ellipsis, modifier = Modifier.padding(top = 5.dp))
        }
        element.styles.take(6).forEach { property -> PropertyRow(property.name, property.value, color) }
        if (hits.isEmpty()) {
            MonoText(strings.noSource, size = 8, maxLines = 2, modifier = Modifier.padding(top = 6.dp))
        } else {
            hits.take(3).forEach { hit ->
                Row(
                    Modifier.fillMaxWidth().padding(top = 5.dp).clickable { state.openSource(hit) },
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(6.dp),
                ) {
                    MonoText("${hit.path}:${hit.line}", color = Nebula, modifier = Modifier.weight(1f))
                    MonoText("›", color = Nebula)
                }
            }
        }
    }
}

@Composable
private fun DomTreeCard(state: WorkspaceState, element: InspectedElement) {
    val strings = state.strings
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        SectionLabel(strings.domTree)
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(3.dp)) {
            repeat(7) { depth -> Box(Modifier.weight(1f).height(3.dp).background(domDepthColor(depth))) }
        }
        if (element.parents.isNotEmpty()) {
            MonoText(strings.parents, size = 8)
            element.parents.asReversed().forEach { reference ->
                DomNodeRow(reference, false) { state.inspectSelector(reference.selector) }
            }
        }
        DomNodeRow(element.reference, true) {}
        if (element.children.isNotEmpty()) {
            MonoText(strings.children, size = 8)
            element.children.forEach { reference ->
                DomNodeRow(reference, false) { state.inspectSelector(reference.selector) }
            }
        }
        if (element.siblings.isNotEmpty()) {
            MonoText(strings.siblings, size = 8)
            element.siblings.forEach { reference ->
                DomNodeRow(reference, false) { state.inspectSelector(reference.selector) }
            }
        }
    }
}

@Composable
private fun DomNodeRow(reference: ElementReference, selected: Boolean, open: () -> Unit) {
    val color = domDepthColor(reference.depth)
    Row(
        Modifier.fillMaxWidth().padding(start = (reference.depth.coerceIn(0, 8) * 4).dp)
            .clip(MaterialTheme.shapes.small)
            .background(if (selected) color.copy(alpha = .18f) else Color(0xFF080E18))
            .border(1.dp, color.copy(alpha = if (selected) .9f else .45f), MaterialTheme.shapes.small)
            .clickable(enabled = !selected) { open() }
            .padding(horizontal = 8.dp, vertical = 7.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(7.dp),
    ) {
        Box(Modifier.size(7.dp).clip(MaterialTheme.shapes.extraLarge).background(color))
        MonoText("<${reference.tag}>", color = color, size = 8)
        Text(reference.label, color = TextMain, fontSize = 9.sp, maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.weight(1f))
        Text(if (selected) "●" else "›", color = if (selected) color else TextMuted, fontSize = 10.sp)
    }
}

@Composable
private fun SourceCandidateCard(state: WorkspaceState, hit: SourceHit) {
    val strings = state.strings
    val copy = LocalCopyText.current
    Column(
        Modifier.fillMaxWidth().clip(MaterialTheme.shapes.medium).background(Color(0xFF080E19))
            .border(1.dp, Color(0xFF22304A), MaterialTheme.shapes.medium).padding(10.dp),
    ) {
        MonoText("${hit.path}:${hit.line}", color = Nebula)
        Text(
            hit.excerpt.ifBlank { "…" },
            color = CodeText,
            fontSize = 9.sp,
            lineHeight = 14.sp,
            fontFamily = FontFamily.Monospace,
            maxLines = 4,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.padding(top = 6.dp),
        )
        Row(Modifier.horizontalScroll(rememberScrollState()).padding(top = 8.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            SmallAction(strings.openSource, { state.openSource(hit) }, active = true)
            SmallAction(strings.copyPath, { copy("${hit.path}:${hit.line}") })
            SmallAction(strings.copyCode, { copy("${hit.path}:${hit.line}\n${hit.excerpt}") })
        }
    }
}
