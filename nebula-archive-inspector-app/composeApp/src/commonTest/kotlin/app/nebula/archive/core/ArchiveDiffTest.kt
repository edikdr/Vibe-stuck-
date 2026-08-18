package app.nebula.archive

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class ArchiveDiffTest {
    @Test
    fun reportsAddedRemovedAndModifiedFiles() {
        val before = testProject(
            mapOf(
                "index.html" to "<h1>Hello</h1>",
                "assets/app.css" to ".a{color:red}",
                "assets/old.js" to "legacy()",
            ),
        )
        val after = testProject(
            mapOf(
                "index.html" to "<h1>Hello there</h1>",
                "assets/app.css" to ".a{color:red}",
                "assets/new.js" to "modern()",
            ),
        )
        val diff = diffArchives(before, after)

        assertEquals(1, diff.added)
        assertEquals(1, diff.removed)
        assertEquals(1, diff.modified)
        assertEquals(1, diff.unchangedCount)
        assertEquals(ChangeKind.Modified, diff.changes.first().kind)
        assertTrue(diff.changes.any { it.path == "assets/new.js" && it.kind == ChangeKind.Added })
        assertTrue(diff.changes.any { it.path == "assets/old.js" && it.kind == ChangeKind.Removed })
        before.close()
        after.close()
    }

    @Test
    fun equalSizeButDifferentContentStillCountsAsModified() {
        val before = testProject(mapOf("index.html" to "<p>aaa</p>"))
        val after = testProject(mapOf("index.html" to "<p>bbb</p>"))
        val diff = diffArchives(before, after)

        assertEquals(1, diff.modified)
        assertEquals(0, diff.unchangedCount)
        before.close()
        after.close()
    }

    @Test
    fun identicalArchivesProduceNoChanges() {
        val files = mapOf("index.html" to "<h1>Same</h1>", "app.js" to "run()")
        val before = testProject(files)
        val after = testProject(files)
        val diff = diffArchives(before, after)

        assertTrue(diff.identical)
        assertEquals(2, diff.unchangedCount)
        assertEquals(0L, diff.byteDelta)
        before.close()
        after.close()
    }

    @Test
    fun onlyTextFilesCanBeComparedLineByLine() {
        val before = testProject(mapOf("index.html" to "<h1>a</h1>", "logo.png" to "binary-one"))
        val after = testProject(mapOf("index.html" to "<h1>bb</h1>", "logo.png" to "binary-two!"))
        val diff = diffArchives(before, after)

        assertTrue(diff.changes.first { it.path == "index.html" }.comparable)
        assertTrue(!diff.changes.first { it.path == "logo.png" }.comparable)
        before.close()
        after.close()
    }

    @Test
    fun diffAiBlockSummarisesTheRewrite() {
        val before = testProject(mapOf("index.html" to "<h1>Hello</h1>", "old.js" to "legacy()"))
        val after = testProject(mapOf("index.html" to "<h1>Hello there</h1>", "new.js" to "modern()"))
        val context = buildDiffAiContext(diffArchives(before, after))

        assertTrue(context.startsWith("NEBULA diff · fixture → fixture"), context)
        assertTrue(context.contains("+1 −1 ~1 files"), context)
        assertTrue(context.contains("~ index.html"), context)
        assertTrue(context.contains("+ new.js"), context)
        assertTrue(context.contains("- old.js"), context)
        assertTrue(!context.contains("```"), context)
        before.close()
        after.close()
    }

    @Test
    fun lineDiffMarksOnlyWhatChanged() {
        val diff = diffText(
            oldText = "one\ntwo\nthree\nfour\nfive",
            newText = "one\ntwo\nTHREE\nfour\nfive",
        )
        assertEquals(1, diff.added)
        assertEquals(1, diff.removed)
        assertTrue(!diff.approximate)
        assertTrue(diff.lines.any { it.kind == DiffLineKind.Removed && it.text == "three" })
        assertTrue(diff.lines.any { it.kind == DiffLineKind.Added && it.text == "THREE" })
        assertTrue(diff.lines.any { it.kind == DiffLineKind.Context })
    }

    @Test
    fun identicalTextProducesNoDiffLines() {
        val diff = diffText("same\nlines", "same\nlines")
        assertEquals(0, diff.added)
        assertEquals(0, diff.removed)
        assertTrue(diff.lines.isEmpty())
    }

    @Test
    fun distantContextIsDroppedSoASmallEditStaysReadable() {
        val old = (1..200).joinToString("\n") { "line $it" }
        val new = old.replace("line 100", "line 100 changed")
        val diff = diffText(old, new, contextLines = 2)

        assertEquals(1, diff.added)
        assertEquals(1, diff.removed)
        assertTrue(diff.lines.size <= 8, "a one-line edit printed ${diff.lines.size} lines")
    }

    @Test
    fun aHugeRewriteFallsBackToAReplacementBlock() {
        val old = (1..900).joinToString("\n") { "old $it" }
        val new = (1..900).joinToString("\n") { "new $it" }
        val diff = diffText(old, new)

        assertTrue(diff.approximate, "an 900×900 match must not be attempted line by line")
        assertEquals(900, diff.added)
        assertEquals(900, diff.removed)
    }

    @Test
    fun contentHashSeparatesDifferentBytes() {
        assertEquals(contentHash("abc".encodeToByteArray()), contentHash("abc".encodeToByteArray()))
        assertTrue(contentHash("abc".encodeToByteArray()) != contentHash("abd".encodeToByteArray()))
    }

    @Test
    fun byteDeltaIsSigned() {
        assertEquals("+1.0 KB", signedBytes(1024))
        assertEquals("−1.0 KB", signedBytes(-1024))
        assertEquals("±0 B", signedBytes(0))
    }
}
