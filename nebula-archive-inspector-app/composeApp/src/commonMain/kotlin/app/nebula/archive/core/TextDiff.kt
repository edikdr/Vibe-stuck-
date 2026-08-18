package app.nebula.archive

/**
 * Line diff for text files inside two archives.
 *
 * Trims the shared prefix and suffix first — two builds of the same page usually differ in a few lines — and
 * only then runs an LCS over what is left. When that middle is still too large, the file falls back to a
 * plain replacement block instead of stalling the device on an O(n·m) table.
 */

enum class DiffLineKind { Context, Added, Removed }

data class DiffLine(
    val kind: DiffLineKind,
    val text: String,
    val oldNumber: Int? = null,
    val newNumber: Int? = null,
)

data class TextDiff(
    val lines: List<DiffLine>,
    val added: Int,
    val removed: Int,
    val truncated: Boolean,
    val approximate: Boolean,
)

/** Above this many cells the LCS table stops being worth its cost on a phone. */
private const val MAX_LCS_CELLS = 250_000

fun diffText(
    oldText: String,
    newText: String,
    contextLines: Int = 3,
    maxLines: Int = 4_000,
): TextDiff {
    val old = oldText.lines()
    val new = newText.lines()

    var prefix = 0
    while (prefix < old.size && prefix < new.size && old[prefix] == new[prefix]) prefix++
    var suffix = 0
    while (
        suffix < old.size - prefix &&
        suffix < new.size - prefix &&
        old[old.size - 1 - suffix] == new[new.size - 1 - suffix]
    ) {
        suffix++
    }

    val oldMiddle = old.subList(prefix, old.size - suffix)
    val newMiddle = new.subList(prefix, new.size - suffix)
    val approximate = oldMiddle.size.toLong() * newMiddle.size.toLong() > MAX_LCS_CELLS
    val middle = if (approximate) {
        replacementBlock(oldMiddle, newMiddle, prefix)
    } else {
        lcsDiff(oldMiddle, newMiddle, prefix)
    }

    val all = buildList {
        // Shared head and tail are folded into the context that surrounds the real changes.
        for (index in 0 until prefix) {
            add(DiffLine(DiffLineKind.Context, old[index], index + 1, index + 1))
        }
        addAll(middle)
        for (index in 0 until suffix) {
            val oldIndex = old.size - suffix + index
            val newIndex = new.size - suffix + index
            add(DiffLine(DiffLineKind.Context, old[oldIndex], oldIndex + 1, newIndex + 1))
        }
    }

    val focused = keepContextAround(all, contextLines)
    return TextDiff(
        lines = focused.take(maxLines),
        added = all.count { it.kind == DiffLineKind.Added },
        removed = all.count { it.kind == DiffLineKind.Removed },
        truncated = focused.size > maxLines,
        approximate = approximate,
    )
}

private fun replacementBlock(old: List<String>, new: List<String>, offset: Int): List<DiffLine> = buildList {
    old.forEachIndexed { index, line -> add(DiffLine(DiffLineKind.Removed, line, offset + index + 1, null)) }
    new.forEachIndexed { index, line -> add(DiffLine(DiffLineKind.Added, line, null, offset + index + 1)) }
}

private fun lcsDiff(old: List<String>, new: List<String>, offset: Int): List<DiffLine> {
    if (old.isEmpty() && new.isEmpty()) return emptyList()
    if (old.isEmpty() || new.isEmpty()) return replacementBlock(old, new, offset)

    val rows = old.size + 1
    val columns = new.size + 1
    val table = Array(rows) { IntArray(columns) }
    for (i in old.size - 1 downTo 0) {
        for (j in new.size - 1 downTo 0) {
            table[i][j] = if (old[i] == new[j]) {
                table[i + 1][j + 1] + 1
            } else {
                maxOf(table[i + 1][j], table[i][j + 1])
            }
        }
    }

    val result = mutableListOf<DiffLine>()
    var i = 0
    var j = 0
    while (i < old.size && j < new.size) {
        when {
            old[i] == new[j] -> {
                result += DiffLine(DiffLineKind.Context, old[i], offset + i + 1, offset + j + 1)
                i++
                j++
            }
            table[i + 1][j] >= table[i][j + 1] -> {
                result += DiffLine(DiffLineKind.Removed, old[i], offset + i + 1, null)
                i++
            }
            else -> {
                result += DiffLine(DiffLineKind.Added, new[j], null, offset + j + 1)
                j++
            }
        }
    }
    while (i < old.size) {
        result += DiffLine(DiffLineKind.Removed, old[i], offset + i + 1, null)
        i++
    }
    while (j < new.size) {
        result += DiffLine(DiffLineKind.Added, new[j], null, offset + j + 1)
        j++
    }
    return result
}

/** Drops context that is far from any change, so a one-line edit does not print the whole file. */
private fun keepContextAround(lines: List<DiffLine>, contextLines: Int): List<DiffLine> {
    if (lines.none { it.kind != DiffLineKind.Context }) return emptyList()
    val keep = BooleanArray(lines.size)
    lines.forEachIndexed { index, line ->
        if (line.kind == DiffLineKind.Context) return@forEachIndexed
        val from = (index - contextLines).coerceAtLeast(0)
        val to = (index + contextLines).coerceAtMost(lines.lastIndex)
        for (position in from..to) keep[position] = true
    }
    return lines.filterIndexed { index, _ -> keep[index] }
}
