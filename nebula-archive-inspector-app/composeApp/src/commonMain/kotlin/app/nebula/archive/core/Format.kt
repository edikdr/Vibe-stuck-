package app.nebula.archive

fun formatBytes(value: Long): String {
    if (value < 1024) return "$value B"
    val divisor = if (value < 1024 * 1024) 1024L else 1024L * 1024L
    val unit = if (divisor == 1024L) "KB" else "MB"
    val tenth = value * 10 / divisor
    return "${tenth / 10}.${tenth % 10} $unit"
}

fun formatMillis(value: Double): String = if (value >= 1_000) {
    "${(value / 100.0).toInt() / 10.0} s"
} else {
    "${value.toInt()} ms"
}

/** Drops the fractional part of whole numbers so CSS sizes read as `320 × 48`, not `320.0 × 48.0`. */
fun formatNumber(value: Double): String {
    val rounded = value * 10
    val tenths = (if (rounded < 0) rounded - 0.5 else rounded + 0.5).toLong()
    return if (tenths % 10 == 0L) (tenths / 10).toString() else "${tenths / 10}.${(if (tenths < 0) -tenths else tenths) % 10}"
}

/** MIME type used both by the Android preview server and by future desktop adapters. */
fun mimeType(path: String): String = when (path.substringAfterLast('.', "").lowercase()) {
    "html", "htm" -> "text/html"
    "xhtml", "xht" -> "application/xhtml+xml"
    "css" -> "text/css"
    "js", "mjs", "cjs" -> "text/javascript"
    "json" -> "application/json"
    "svg" -> "image/svg+xml"
    "png" -> "image/png"
    "jpg", "jpeg", "jpe", "jfif" -> "image/jpeg"
    "webp" -> "image/webp"
    "avif" -> "image/avif"
    "gif" -> "image/gif"
    "apng" -> "image/apng"
    "ico" -> "image/x-icon"
    "bmp" -> "image/bmp"
    "tif", "tiff" -> "image/tiff"
    "mp4" -> "video/mp4"
    "m4v" -> "video/x-m4v"
    "mov" -> "video/quicktime"
    "webm" -> "video/webm"
    "ogv" -> "video/ogg"
    "mp3" -> "audio/mpeg"
    "m4a" -> "audio/mp4"
    "aac" -> "audio/aac"
    "wav" -> "audio/wav"
    "ogg", "oga" -> "audio/ogg"
    "opus" -> "audio/opus"
    "flac" -> "audio/flac"
    "woff2" -> "font/woff2"
    "woff" -> "font/woff"
    "ttf" -> "font/ttf"
    "otf" -> "font/otf"
    "eot" -> "application/vnd.ms-fontobject"
    "wasm" -> "application/wasm"
    "glb" -> "model/gltf-binary"
    "gltf" -> "model/gltf+json"
    "obj" -> "model/obj"
    "stl" -> "model/stl"
    "pdf" -> "application/pdf"
    "csv" -> "text/csv"
    "xml" -> "application/xml"
    "webmanifest" -> "application/manifest+json"
    "m3u8" -> "application/vnd.apple.mpegurl"
    "vtt" -> "text/vtt"
    "txt", "md", "markdown", "srt" -> "text/plain"
    // A resource the archive references but whose extension is unknown must still be readable rather than
    // treated as a download; text-like archive entries keep a text type instead of a binary stream.
    else -> if (archiveKind(path) == ArchiveKind.Text) "text/plain" else "application/octet-stream"
}

fun isTextMime(mime: String): Boolean = mime.startsWith("text/") || mime in setOf(
    "application/json", "application/xml", "application/manifest+json", "image/svg+xml", "model/gltf+json",
)

fun htmlEscape(value: String): String = value
    .replace("&", "&amp;")
    .replace("<", "&lt;")
    .replace(">", "&gt;")
    .replace("\"", "&quot;")
