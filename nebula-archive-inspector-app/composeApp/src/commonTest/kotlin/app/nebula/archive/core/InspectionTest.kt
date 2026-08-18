package app.nebula.archive

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class InspectionTest {
    @Test
    fun mapsElementToHtmlCssAndJavaScript() {
        val project = testProject(
            mapOf(
                "index.html" to "<button id=\"launch\" class=\"primary\">Launch</button>",
                "assets/app.css" to ".primary { padding: 12px; }",
                "assets/app.js" to "document.querySelector('#launch')?.addEventListener('click', run)",
            ),
        )
        val hits = findElementSources(project, element(selector = "#launch", elementId = "launch", classes = listOf("primary")))
        assertEquals(3, hits.size)
        assertTrue(hits.any { it.path == "index.html" })
        assertTrue(hits.any { it.path == "assets/app.css" })
        assertTrue(hits.any { it.path == "assets/app.js" })
        project.close()
    }

    @Test
    fun mapsPlainElementByVisibleText() {
        val project = testProject(
            mapOf(
                "index.html" to "<main><button>Start playlist</button></main>",
                "assets/player.js" to "const label = 'Start playlist'",
            ),
        )
        val hits = findElementSources(
            project,
            element(
                selector = "main > button",
                tag = "button",
                text = "Start playlist",
                outerHtml = "<button>Start playlist</button>",
            ),
        )
        assertEquals(2, hits.size)
        assertEquals("index.html", hits.first().path)
        project.close()
    }

    @Test
    fun aiContextIsCompactAndCarriesLayoutFacts() {
        val project = testProject(
            mapOf(
                "index.html" to "<button class=\"cta\" data-action=\"subscribe\">Subscribe</button>",
                "assets/app.css" to ".cta{padding:12px 24px;background:var(--accent)}",
            ),
        )
        val subject = element(
            selector = "body > main > button.cta",
            tag = "button",
            classes = listOf("cta"),
            text = "Subscribe",
            outerHtml = "<button class=\"cta\" data-action=\"subscribe\">Subscribe</button>",
        ).copy(
            id = 1,
            width = 320.0,
            height = 48.0,
            left = 12.0,
            top = 340.0,
            childCount = 2,
            depth = 7,
            attributes = listOf(
                ElementProperty("class", "cta"),
                ElementProperty("data-action", "subscribe"),
            ),
            styles = listOf(
                ElementProperty("display", "flex"),
                ElementProperty("background", "rgb(10, 132, 255)"),
            ),
        )
        val hits = findElementSources(project, subject)
        val context = buildAiContext("my-site", "index.html", listOf(subject), mapOf(1 to hits))

        assertTrue(context.contains("[1] <button.cta>"), context)
        assertTrue(context.contains("body>main>button.cta"), context)
        assertTrue(context.contains("box 320 × 48 @12,340 · 2 children · depth 7"), context)
        assertTrue(context.contains("css display:flex; background:rgb(10, 132, 255)"), context)
        // `class` is already carried by the selector, so it never repeats in the attribute line.
        assertTrue(context.contains("""attr data-action="subscribe""""), context)
        assertTrue(!context.contains("""attr class="cta""""), context)
        assertTrue(context.contains("src index.html:1"), context)
        assertTrue(!context.contains("```"), "the compact block must not use fenced code")
        assertTrue(context.length < 1_200, "block grew to ${context.length} characters")
        project.close()
    }

    @Test
    fun aiContextDescribesAWholeSelection() {
        val elements = listOf(
            element(selector = "header", tag = "header").copy(id = 1),
            element(selector = "footer", tag = "footer").copy(id = 2),
        )
        val context = buildAiContext("my-site", "index.html", elements)

        assertTrue(context.contains("2 elements selected"), context)
        assertTrue(context.contains("[1] <header>"), context)
        assertTrue(context.contains("[2] <footer>"), context)
        assertTrue(context.contains("selected together"), context)
    }

    private fun element(
        selector: String,
        tag: String = "button",
        elementId: String = "",
        classes: List<String> = emptyList(),
        text: String = "",
        outerHtml: String = "",
    ) = InspectedElement(
        selector = selector,
        tag = tag,
        elementId = elementId,
        classes = classes,
        text = text,
        outerHtml = outerHtml,
    )

}
