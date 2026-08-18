package app.nebula.archive

/**
 * Comparison of two opened archives — "what changed after the AI rewrote it".
 *
 * Both sides are ordinary [ArchiveProject]s, so comparing v1 against v2 needs no new import path: open both
 * as tabs and diff them. Everything here is pure Kotlin, which keeps it testable and shared with desktop.
 */

enum class ChangeKind { Added, Removed, Modified }

data class FileChange(
    val path: String,
    val kind: ChangeKind,
    val oldSize: Long = 0,
    val newSize: Long = 0,
    val oldKind: ArchiveKind = ArchiveKind.Other,
) {
    val sizeDelta: Long get() = newSize - oldSize
    /** Only a text file can be shown line by line; everything else stays a size comparison. */
    val comparable: Boolean
        get() = kind == ChangeKind.Modified &&
            oldKind in setOf(ArchiveKind.Html, ArchiveKind.Css, ArchiveKind.JavaScript, ArchiveKind.Json, ArchiveKind.Text)
}

data class ArchiveDiff(
    val fromName: String,
    val toName: String,
    val changes: List<FileChange>,
    val unchangedCount: Int,
    val fromBytes: Long,
    val toBytes: Long,
) {
    val added: Int = changes.count { it.kind == ChangeKind.Added }
    val removed: Int = changes.count { it.kind == ChangeKind.Removed }
    val modified: Int = changes.count { it.kind == ChangeKind.Modified }
    val byteDelta: Long = toBytes - fromBytes
    val identical: Boolean = changes.isEmpty()
}

/** Files larger than this are compared by size only; hashing them would stall the UI for no real gain. */
private const val MAX_HASHED_BYTES = 8L * 1024 * 1024

/**
 * Compares [from] against [to]. Same size is not enough to call a file unchanged, so equal-sized entries are
 * hashed; only oversized files fall back to a pure size comparison.
 */
fun diffArchives(from: ArchiveProject, to: ArchiveProject): ArchiveDiff {
    val fromEntries = from.entries.associateBy { it.path }
    val toEntries = to.entries.associateBy { it.path }
    val changes = mutableListOf<FileChange>()
    var unchanged = 0

    fromEntries.forEach { (path, oldEntry) ->
        val newEntry = toEntries[path]
        when {
            newEntry == null -> changes += FileChange(path, ChangeKind.Removed, oldEntry.size, 0, oldEntry.kind)
            newEntry.size != oldEntry.size ->
                changes += FileChange(path, ChangeKind.Modified, oldEntry.size, newEntry.size, oldEntry.kind)
            sameContent(from, to, path, oldEntry.size) -> unchanged++
            else -> changes += FileChange(path, ChangeKind.Modified, oldEntry.size, newEntry.size, oldEntry.kind)
        }
    }
    toEntries.forEach { (path, newEntry) ->
        if (!fromEntries.containsKey(path)) {
            changes += FileChange(path, ChangeKind.Added, 0, newEntry.size, newEntry.kind)
        }
    }

    return ArchiveDiff(
        fromName = from.name,
        toName = to.name,
        changes = changes.sortedWith(compareBy({ changeRank(it.kind) }, { it.path })),
        unchangedCount = unchanged,
        fromBytes = from.totalBytes,
        toBytes = to.totalBytes,
    )
}

private fun changeRank(kind: ChangeKind) = when (kind) {
    ChangeKind.Modified -> 0
    ChangeKind.Added -> 1
    ChangeKind.Removed -> 2
}

private fun sameContent(from: ArchiveProject, to: ArchiveProject, path: String, size: Long): Boolean {
    if (size > MAX_HASHED_BYTES) return true
    val left = from.bytes(path) ?: return false
    val right = to.bytes(path) ?: return false
    return contentHash(left) == contentHash(right)
}

/** FNV-1a: fast, allocation free, and good enough to tell two build outputs apart. */
fun contentHash(bytes: ByteArray): Long {
    var hash = -3750763034362895579L
    for (byte in bytes) {
        hash = hash xor (byte.toLong() and 0xFF)
        hash *= 1099511628211L
    }
    return hash
}

/** Compact diff summary for a coding assistant, in the same dense style as the other AI blocks. */
fun buildDiffAiContext(diff: ArchiveDiff, maxFiles: Int = 40): String = buildString {
    append("NEBULA diff · ${diff.fromName} → ${diff.toName} · ")
    append("+${diff.added} −${diff.removed} ~${diff.modified} files")
    append(", ${diff.unchangedCount} unchanged, ${signedBytes(diff.byteDelta)}")
    diff.changes.take(maxFiles).forEach { change ->
        appendLine()
        append(
            when (change.kind) {
                ChangeKind.Added -> "+ ${change.path} (${formatBytes(change.newSize)})"
                ChangeKind.Removed -> "- ${change.path} (${formatBytes(change.oldSize)})"
                ChangeKind.Modified ->
                    "~ ${change.path} (${formatBytes(change.oldSize)} → ${formatBytes(change.newSize)}, ${signedBytes(change.sizeDelta)})"
            },
        )
    }
    if (diff.changes.size > maxFiles) {
        appendLine()
        append("… ${diff.changes.size - maxFiles} more changed files")
    }
    appendLine()
    append("Describe what changed between these two builds and what it means for the interface.")
}

fun signedBytes(delta: Long): String = when {
    delta > 0 -> "+${formatBytes(delta)}"
    delta < 0 -> "−${formatBytes(-delta)}"
    else -> "±0 B"
}
