package app.nebula.archive

import kotlin.test.Test
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

class SvgTest {
    @Test
    fun rebuildsViewBoxFromIntrinsicSizeSoIconsCanScale() {
        val prepared = prepareSvgMarkup("""<svg width="24" height="24" fill="none"><path d="M0 0h24v24H0z"/></svg>""")
        assertNotNull(prepared)
        assertTrue(prepared.contains("""viewBox="0 0 24 24""""), prepared)
        assertTrue(prepared.contains("preserveAspectRatio"), prepared)
    }

    @Test
    fun readsIntrinsicSizesWithUnitsAndIgnoresStrokeWidth() {
        val prepared = prepareSvgMarkup("""<svg stroke-width="2" width="16px" height="10.5pt"><g/></svg>""")
        assertNotNull(prepared)
        assertTrue(prepared.contains("""viewBox="0 0 16 10.5""""), prepared)
    }

    @Test
    fun leavesAnExistingViewBoxAlone() {
        val prepared = prepareSvgMarkup("""<svg viewBox="0 0 100 50" width="10" height="5"><g/></svg>""")
        assertNotNull(prepared)
        assertTrue(prepared.contains("""viewBox="0 0 100 50""""), prepared)
        assertTrue(!prepared.contains("viewBox=\"0 0 10 5\""), prepared)
    }

    @Test
    fun percentageSizesCannotDefineAViewBox() {
        val prepared = prepareSvgMarkup("""<svg width="100%" height="100%"><g/></svg>""")
        assertNotNull(prepared)
        assertTrue(!prepared.contains("viewBox"), prepared)
    }

    @Test
    fun skipsTheXmlPrologAndDoctype() {
        val raw = """<?xml version="1.0" encoding="UTF-8"?>""" +
            """<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">""" +
            """<svg width="32" height="32"><circle r="4"/></svg>"""
        val prepared = prepareSvgMarkup(raw)
        assertNotNull(prepared)
        assertTrue(prepared.startsWith("<svg"), prepared)
        assertTrue(prepared.contains("""viewBox="0 0 32 32""""), prepared)
    }

    @Test
    fun anAngleBracketInsideAnAttributeDoesNotEndTheRootTag() {
        val prepared = prepareSvgMarkup("""<svg data-note="a>b" width="8" height="8"><g/></svg>""")
        assertNotNull(prepared)
        assertTrue(prepared.contains("""viewBox="0 0 8 8""""), prepared)
        assertTrue(prepared.contains("<g/>"), prepared)
    }

    @Test
    fun keepsASelfClosingRootValid() {
        val prepared = prepareSvgMarkup("""<svg width="12" height="12"/>""")
        assertNotNull(prepared)
        assertTrue(prepared.trimEnd().endsWith("/>"), prepared)
        assertTrue(prepared.contains("""viewBox="0 0 12 12""""), prepared)
    }

    @Test
    fun addsTheNamespaceSoInlinedMarkupStillRenders() {
        val prepared = prepareSvgMarkup("""<svg viewBox="0 0 4 4"><rect width="4" height="4"/></svg>""")
        assertNotNull(prepared)
        assertTrue(prepared.contains("xmlns=\"http://www.w3.org/2000/svg\""), prepared)
    }

    @Test
    fun rejectsMarkupWithoutAnSvgRoot() {
        assertNull(prepareSvgMarkup("<html><body>not an icon</body></html>"))
        assertNull(prepareSvgMarkup(""))
    }

    @Test
    fun svgDocumentScalesToTheStageAndSwitchesBackground() {
        val fitted = svgPreviewDocument("<svg/>", lightBackground = false, fitToStage = true)
        assertTrue(fitted.contains("width:100%!important"), fitted)
        assertTrue(fitted.contains("#05070e"), fitted)

        val actual = svgPreviewDocument("<svg/>", lightBackground = true, fitToStage = false)
        assertTrue(actual.contains("max-width:none!important"), actual)
        assertTrue(actual.contains("#f2f5fb"), actual)
        assertTrue(actual.contains("overflow:auto"), actual)
    }

    @Test
    fun assetDocumentEscapesThePathItShows() {
        val entry = ArchiveEntry("media/<script>.bin", 10, ArchiveKind.Other)
        val document = assetPreviewDocument(entry, "https://nebula.local/media/x.bin", "3D notice")
        assertTrue(!document.contains("<script>"), document)
        assertTrue(document.contains("&lt;script&gt;"), document)
    }
}
