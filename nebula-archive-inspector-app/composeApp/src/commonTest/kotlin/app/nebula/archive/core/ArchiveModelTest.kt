package app.nebula.archive

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

class ArchiveModelTest {
    @Test
    fun classifiesFilesUsedByTheStandaloneViewer() {
        assertEquals(ArchiveKind.Css, archiveKind("styles/theme.scss"))
        assertEquals(ArchiveKind.JavaScript, archiveKind("src/widget.tsx"))
        assertEquals(ArchiveKind.Json, archiveKind("dist/app.js.map"))
        assertEquals(ArchiveKind.Image, archiveKind("assets/photo.bmp"))
        assertEquals(ArchiveKind.Media, archiveKind("media/song.m4a"))
        assertEquals(ArchiveKind.Media, archiveKind("media/clip.mov"))
        assertEquals(ArchiveKind.Model, archiveKind("models/planet.glb"))
        assertEquals(ArchiveKind.Text, archiveKind("server/main.kt"))
        assertEquals(ArchiveKind.Text, archiveKind("data/table.csv"))
    }

    @Test
    fun recognisesSvgAndSourceEntries() {
        val svg = ArchiveEntry("icons/logo.svg", 12, archiveKind("icons/logo.svg"))
        assertTrue(svg.isSvg)
        assertTrue(!svg.isSourceText)
        assertTrue(ArchiveEntry("app.css", 1, ArchiveKind.Css).isSourceText)
        assertTrue(!ArchiveEntry("a.png", 1, ArchiveKind.Image).isSvg)
    }

    @Test
    fun rejectsUnsafePaths() {
        assertNull(normalizeArchivePath("../index.html"))
        assertNull(normalizeArchivePath("site/../../secret.txt"))
        assertEquals("site/assets/app.css", normalizeArchivePath("./site\\assets/app.css"))
    }

    @Test
    fun selectsShallowestEntryPoint() {
        val entries = listOf(
            ArchiveEntry("docs/demo/index.html", 1, ArchiveKind.Html),
            ArchiveEntry("index.html", 1, ArchiveKind.Html),
            ArchiveEntry("assets/app.js", 1, ArchiveKind.JavaScript),
        )
        assertEquals("index.html", locateEntryPoint(entries))
    }

    @Test
    fun resolvesPagesDirectoriesCaseAndSpaRoutes() {
        val project = testProject(
            mapOf(
                "index.html" to "<html></html>",
                "about.html" to "about",
                "docs/index.htm" to "docs",
                "assets/Logo.PNG" to "image",
            ),
        )
        assertEquals("about.html", resolveArchiveRequestPath(project, "/about", mainFrame = true))
        assertEquals("docs/index.htm", resolveArchiveRequestPath(project, "docs/", mainFrame = true))
        assertEquals("assets/Logo.PNG", resolveArchiveRequestPath(project, "assets/logo.png", mainFrame = false))
        assertEquals("index.html", resolveArchiveRequestPath(project, "dashboard/settings", mainFrame = true))
        assertNull(resolveArchiveRequestPath(project, "api/profile", mainFrame = false))
        project.close()
    }

    @Test
    fun treatsEntryPointFolderAsDeployedSiteRoot() {
        val project = testProject(
            mapOf(
                "release/index.html" to "<html></html>",
                "release/assets/app.js" to "app",
                "release/pages/about.html" to "about",
            ),
            entryPoint = "release/index.html",
        )
        assertEquals("release", project.siteRoot)
        assertEquals("release/assets/app.js", resolveArchiveRequestPath(project, "/assets/app.js", mainFrame = false))
        assertEquals("release/pages/about.html", resolveArchiveRequestPath(project, "/pages/about", mainFrame = true))
        assertEquals("release/assets/app.js", resolveArchiveRequestPath(project, "/dashboard/assets/app.js", mainFrame = false))
        project.close()
    }

    @Test
    fun servesTextMimeTypesForUnknownTextEntries() {
        assertEquals("text/css", mimeType("assets/app.css"))
        assertEquals("image/svg+xml", mimeType("icons/logo.svg"))
        assertEquals("application/octet-stream", mimeType("data/blob.bin"))
        assertEquals("text/plain", mimeType("notes/readme.md"))
        assertTrue(isTextMime("image/svg+xml"))
        assertTrue(!isTextMime("image/png"))
    }

    @Test
    fun englishIsTheDefaultProductLocale() {
        val strings = AppStrings.forLocale(AppLocale.English)
        assertEquals("Choose ZIP or HTML", strings.chooseArchive)
        assertTrue(!strings.description.contains("архив"))
    }
}

internal class MemorySource(private val files: Map<String, ByteArray>) : ArchiveContentSource {
    var closed = false
        private set

    override fun readBytes(path: String): ByteArray? = files[path]
    override fun close() {
        closed = true
    }
}

internal fun testProject(files: Map<String, String>, entryPoint: String = "index.html"): ArchiveProject {
    val source = MemorySource(files.mapValues { it.value.encodeToByteArray() })
    val entries = files.map { (path, value) ->
        ArchiveEntry(path, value.encodeToByteArray().size.toLong(), archiveKind(path))
    }
    return ArchiveProject("fixture", entries, entryPoint, 1L, source)
}
