package app.nebula.archive

/**
 * Standalone SVG files are prepared for display instead of being opened directly.
 *
 * Opening the file makes the renderer honour the SVG's own `width`/`height`, so an icon authored at 16 or 24
 * units is painted a few pixels wide in the corner of a dark page and reads as "nothing was displayed".
 * Inlining the markup lets CSS size it, and a missing `viewBox` is reconstructed from the intrinsic size so it
 * can scale at all.
 */
fun prepareSvgMarkup(raw: String): String? {
    val start = svgRootIndex(raw) ?: return null
    var markup = raw.substring(start)
    val closing = markup.lastIndexOf("</svg", ignoreCase = true)
    if (closing >= 0) {
        val closingEnd = markup.indexOf('>', closing)
        if (closingEnd > 0) markup = markup.substring(0, closingEnd + 1)
    }
    val openTagEnd = openTagEnd(markup) ?: return null
    val selfClosing = markup[openTagEnd - 1] == '/'
    var openTag = markup.substring(0, if (selfClosing) openTagEnd - 1 else openTagEnd).trimEnd()
    if (!hasAttribute(openTag, "viewBox")) {
        val width = svgIntrinsicLength(openTag, "width")
        val height = svgIntrinsicLength(openTag, "height")
        if (width != null && height != null && width > 0.0 && height > 0.0) {
            openTag += " viewBox=\"0 0 ${formatNumber(width)} ${formatNumber(height)}\""
        }
    }
    if (!hasAttribute(openTag, "preserveAspectRatio")) openTag += " preserveAspectRatio=\"xMidYMid meet\""
    if (!hasAttribute(openTag, "xmlns")) openTag += " xmlns=\"http://www.w3.org/2000/svg\""
    return openTag + (if (selfClosing) "/" else "") + markup.substring(openTagEnd)
}

/** The `<svg` that opens the document, skipping the XML prolog, a doctype and any leading comment. */
private fun svgRootIndex(raw: String): Int? {
    var from = 0
    while (true) {
        val index = raw.indexOf("<svg", from, ignoreCase = true)
        if (index < 0) return null
        val next = raw.getOrNull(index + 4)
        if (next == null || next.isWhitespace() || next == '>' || next == '/') return index
        from = index + 4
    }
}

/** Index of the `>` that closes the root tag, ignoring any `>` inside a quoted attribute value. */
private fun openTagEnd(markup: String): Int? {
    var quote: Char? = null
    for (index in markup.indices) {
        val char = markup[index]
        when {
            quote != null -> if (char == quote) quote = null
            char == '"' || char == '\'' -> quote = char
            char == '>' -> return if (index > 0) index else null
        }
    }
    return null
}

private fun hasAttribute(openTag: String, name: String): Boolean =
    Regex("""(?<![-\w:])$name\s*=""", RegexOption.IGNORE_CASE).containsMatchIn(openTag)

/**
 * Reads an intrinsic `width`/`height`. Percentages are skipped on purpose: they describe a share of the
 * parent, so they cannot define a viewBox. The lookbehind keeps `stroke-width` from being read as a size.
 */
private fun svgIntrinsicLength(openTag: String, attribute: String): Double? =
    Regex(
        """(?<![-\w:])$attribute\s*=\s*["']\s*([0-9]*\.?[0-9]+)\s*(px|pt|pc|mm|cm|in|em|rem|ex)?\s*["']""",
        RegexOption.IGNORE_CASE,
    ).find(openTag)?.groupValues?.get(1)?.toDoubleOrNull()
