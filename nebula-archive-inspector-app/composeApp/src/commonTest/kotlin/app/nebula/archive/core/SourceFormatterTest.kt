package app.nebula.archive

import kotlin.test.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class SourceFormatterTest {
    @Test
    fun formatsMinifiedCssWithoutChangingStrings() {
        val source = ".card{background:url(\"data:image/svg+xml;a;b\");color:red}.card.open{display:block}".repeat(4)
        val result = sourceDisplay("assets/app.min.css", source)

        assertTrue(result.formatted)
        assertTrue(result.lines.size > 4)
        assertTrue(result.lines.joinToString("\n").contains("data:image/svg+xml;a;b"))
    }

    @Test
    fun formatsMinifiedJavascriptWithoutSplittingForHeader() {
        val source = "function openModal(){for(let i=0;i<10;i++){document.body.classList.add('open')}};".repeat(6)
        val result = sourceDisplay("assets/app.js", source)

        assertTrue(result.formatted)
        assertTrue(result.lines.any { "for(let i=0; i<10; i++)" in it })
        assertTrue(result.lines.any { "classList.add('open')" in it })
    }

    @Test
    fun leavesReadableSourceUntouched() {
        val source = "body {\n  color: white;\n}\n"
        val result = sourceDisplay("style.css", source)

        assertFalse(result.formatted)
        assertTrue(result.lines.size >= 3)
    }
}
