package app.nebula.archive

data class SourceDisplay(
    val lines: List<String>,
    val formatted: Boolean,
)

fun sourceDisplay(path: String, source: String, maxLines: Int = 20_000): SourceDisplay {
    val rawLines = source.lineSequence().toList()
    val looksMinified = source.length > 240 && rawLines.size <= 4 && (rawLines.maxOfOrNull { it.length } ?: 0) > 180
    if (!looksMinified) return SourceDisplay(rawLines.take(maxLines), false)

    val extension = path.substringAfterLast('.', "").lowercase()
    val formatted = when (extension) {
        "html", "htm", "xml", "svg" -> formatMarkup(source)
        "css", "scss", "sass", "less" -> formatStructured(source, FormatLanguage.Css)
        "json", "webmanifest" -> formatStructured(source, FormatLanguage.Json)
        "js", "mjs", "cjs", "jsx", "ts", "tsx" -> formatStructured(source, FormatLanguage.JavaScript)
        else -> source
    }
    return SourceDisplay(formatted.lineSequence().take(maxLines).toList(), formatted != source)
}

private enum class FormatLanguage { Css, Json, JavaScript }

private fun formatMarkup(source: String): String {
    val result = StringBuilder(source.length + source.length / 5)
    var index = 0
    var indent = 0
    val voidTags = setOf("area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param", "source", "track", "wbr")

    fun line(value: String, level: Int = indent) {
        val clean = value.trim()
        if (clean.isEmpty()) return
        if (result.isNotEmpty() && result.last() != '\n') result.append('\n')
        repeat(level.coerceAtLeast(0)) { result.append("  ") }
        result.append(clean)
    }

    while (index < source.length) {
        val open = source.indexOf('<', index)
        if (open < 0) {
            line(source.substring(index))
            break
        }
        if (open > index) line(source.substring(index, open))
        var cursor = open + 1
        var quote: Char? = null
        while (cursor < source.length) {
            val char = source[cursor]
            if (quote != null) {
                if (char == quote && source.getOrNull(cursor - 1) != '\\') quote = null
            } else if (char == '\'' || char == '"') {
                quote = char
            } else if (char == '>') {
                break
            }
            cursor++
        }
        if (cursor >= source.length) {
            line(source.substring(open))
            break
        }
        val tag = source.substring(open, cursor + 1)
        val closing = tag.startsWith("</")
        val declaration = tag.startsWith("<!") || tag.startsWith("<?")
        val name = tag.removePrefix("</").removePrefix("<").trimStart().takeWhile { it.isLetterOrDigit() }.lowercase()
        val selfClosing = declaration || tag.endsWith("/>") || name in voidTags
        if (closing) indent = (indent - 1).coerceAtLeast(0)
        line(tag)
        if (!closing && !selfClosing) indent++
        index = cursor + 1
    }
    return result.toString().trim()
}

private fun formatStructured(source: String, language: FormatLanguage): String {
    val out = StringBuilder(source.length + source.length / 4)
    var indent = 0
    var parenDepth = 0
    var bracketDepth = 0
    var quote: Char? = null
    var escaped = false
    var lineComment = false
    var blockComment = false
    var regex = false
    var regexClass = false
    var previousSignificant: Char? = null

    fun trimTrailingSpaces() {
        while (out.isNotEmpty() && (out.last() == ' ' || out.last() == '\t')) out.deleteCharAt(out.lastIndex)
    }
    fun newline(force: Boolean = false) {
        trimTrailingSpaces()
        if (out.isEmpty() || out.last() == '\n') return
        out.append('\n')
        if (!force) repeat(indent.coerceAtLeast(0)) { out.append("  ") }
    }
    fun appendSpace() {
        if (out.isNotEmpty() && !out.last().isWhitespace()) out.append(' ')
    }
    fun canStartRegex(previous: Char?): Boolean = previous == null || previous in "([{=:;,!&|?+-*%^~<>"

    var index = 0
    while (index < source.length) {
        val char = source[index]
        val next = source.getOrNull(index + 1)
        if (lineComment) {
            out.append(char)
            if (char == '\n') {
                lineComment = false
                repeat(indent) { out.append("  ") }
            }
            index++
            continue
        }
        if (blockComment) {
            out.append(char)
            if (char == '*' && next == '/') {
                out.append('/')
                index += 2
                blockComment = false
                newline()
            } else index++
            continue
        }
        if (quote != null) {
            out.append(char)
            if (escaped) escaped = false
            else if (char == '\\') escaped = true
            else if (char == quote) quote = null
            index++
            continue
        }
        if (regex) {
            out.append(char)
            if (escaped) escaped = false
            else if (char == '\\') escaped = true
            else if (char == '[') regexClass = true
            else if (char == ']') regexClass = false
            else if (char == '/' && !regexClass) regex = false
            index++
            continue
        }
        if (char == '/' && next == '/') {
            out.append("//")
            lineComment = true
            index += 2
            continue
        }
        if (char == '/' && next == '*') {
            appendSpace()
            out.append("/*")
            blockComment = true
            index += 2
            continue
        }
        if (language == FormatLanguage.JavaScript && char == '/' && canStartRegex(previousSignificant)) {
            out.append(char)
            regex = true
            index++
            continue
        }
        if (char == '\'' || char == '"' || (language == FormatLanguage.JavaScript && char == '`')) {
            quote = char
            out.append(char)
            previousSignificant = char
            index++
            continue
        }
        when (char) {
            '{' -> {
                appendSpace()
                out.append('{')
                indent++
                newline()
            }
            '}' -> {
                indent = (indent - 1).coerceAtLeast(0)
                trimTrailingSpaces()
                if (out.isNotEmpty() && out.last() != '\n') out.append('\n')
                repeat(indent) { out.append("  ") }
                out.append('}')
                val following = source.drop(index + 1).firstOrNull { !it.isWhitespace() }
                if (following !in listOf(';', ',', ')', ']', '.', ':')) newline()
            }
            '(' -> { parenDepth++; out.append(char) }
            ')' -> { parenDepth = (parenDepth - 1).coerceAtLeast(0); out.append(char) }
            '[' -> { bracketDepth++; out.append(char) }
            ']' -> { bracketDepth = (bracketDepth - 1).coerceAtLeast(0); out.append(char) }
            ';' -> {
                out.append(char)
                if (language != FormatLanguage.Css && parenDepth > 0) appendSpace() else newline()
            }
            ',' -> {
                out.append(char)
                if (language == FormatLanguage.Json && bracketDepth + indent > 0) newline() else appendSpace()
            }
            ':' -> {
                out.append(char)
                if (language != FormatLanguage.JavaScript || parenDepth == 0) appendSpace()
            }
            '\r', '\n', '\t', ' ' -> appendSpace()
            else -> out.append(char)
        }
        if (!char.isWhitespace()) previousSignificant = char
        index++
    }
    return out.toString().lines().joinToString("\n") { it.trimEnd() }.trim()
}
