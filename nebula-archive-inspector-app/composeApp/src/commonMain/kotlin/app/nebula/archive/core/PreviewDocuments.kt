package app.nebula.archive

/**
 * HTML wrappers used to preview a single archive file.
 *
 * They are plain strings so every platform that can display HTML reuses the same viewer, and so they can be
 * unit tested without a browser.
 */

private const val PAGE_STYLE =
    "html,body{margin:0;width:100%;height:100%;overflow:auto;background:#05070e;color:#eaf1ff;" +
        "font-family:system-ui,sans-serif}" +
        "body{display:grid;place-items:center;background:radial-gradient(circle at 50% 35%,#162744,#05070e 68%)}" +
        ".image{display:block;max-width:100%;max-height:100vh;object-fit:contain}" +
        "video{width:100%;height:100%;object-fit:contain;background:#000}" +
        ".audio{display:flex;flex-direction:column;align-items:center;gap:28px;width:min(88%,520px)}audio{width:100%}" +
        ".disc{display:grid;place-items:center;width:170px;height:170px;border-radius:50%;font-size:58px;color:#79e9ff;" +
        "background:radial-gradient(circle,#172a47 0 20%,#05070e 21% 38%,#9076ff 39% 41%,#101827 42%);box-shadow:0 0 50px #79e9ff33}" +
        ".font{display:flex;flex-direction:column;gap:24px;padding:32px;font-size:28px}.font b{font-size:60px;color:#79e9ff}" +
        ".notice{text-align:center;padding:22px;color:#b9c7da}.notice small{color:#7688a4}" +
        "model-viewer{width:100%;height:100%;--poster-color:transparent}" +
        ".bottom{position:fixed;left:0;right:0;bottom:0;font-size:12px;background:#05070ecc}"

/** Visual preview for images, media, fonts and 3D models. [fileUrl] must already be URL encoded. */
fun assetPreviewDocument(entry: ArchiveEntry, fileUrl: String, modelNotice: String): String {
    val title = htmlEscape(entry.path)
    val body = when (entry.kind) {
        ArchiveKind.Image -> """<img class="image" src="$fileUrl" alt="$title">"""
        ArchiveKind.Media -> when (entry.extension) {
            "mp4", "m4v", "webm", "mov" -> """<video src="$fileUrl" controls playsinline preload="metadata"></video>"""
            else -> """<div class="audio"><div class="disc">♪</div><audio src="$fileUrl" controls></audio></div>"""
        }
        ArchiveKind.Font ->
            """<style>@font-face{font-family:Preview;src:url('$fileUrl')}</style>""" +
                """<div class="font" style="font-family:Preview"><b>Aa Bb Cc 123</b>""" +
                """<span>The quick brown fox jumps over the lazy dog.</span>""" +
                """<span>Съешь ещё этих мягких французских булок.</span></div>"""
        ArchiveKind.Model ->
            """<script type="module" src="https://unpkg.com/@google/model-viewer/dist/model-viewer.min.js"></script>""" +
                """<model-viewer src="$fileUrl" camera-controls auto-rotate shadow-intensity="1" environment-image="neutral">""" +
                """<div slot="poster" class="notice">3D…</div></model-viewer>""" +
                """<div class="notice bottom">${htmlEscape(modelNotice)}</div>"""
        else -> """<div class="notice">No visual renderer is available for this file.<br><small>$title · ${entry.size} bytes</small></div>"""
    }
    return document(PAGE_STYLE, body)
}

/**
 * Scaled preview stage for an inlined SVG. The `!important` sizing also beats a `style="width:24px"` left on
 * the root by icon exporters, and the checkerboard makes a transparent icon readable on either background.
 */
fun svgPreviewDocument(markup: String, lightBackground: Boolean, fitToStage: Boolean): String {
    val page = if (lightBackground) "#f2f5fb" else "#05070e"
    val square = if (lightBackground) "#dbe3f2" else "#141d31"
    val stageSvg = if (fitToStage) {
        ".stage>svg{width:100%!important;height:100%!important;max-width:100%;max-height:100%;display:block}"
    } else {
        ".stage>svg{max-width:none!important;max-height:none!important;display:block}"
    }
    val style = "html,body{margin:0;width:100%;height:100%}" +
        "body{box-sizing:border-box;padding:14px;display:flex;align-items:center;justify-content:center;" +
        "overflow:${if (fitToStage) "hidden" else "auto"};" +
        "background-color:$page;background-size:22px 22px;background-position:0 0,0 11px,11px -11px,-11px 0;" +
        "background-image:linear-gradient(45deg,$square 25%,transparent 25%)," +
        "linear-gradient(-45deg,$square 25%,transparent 25%)," +
        "linear-gradient(45deg,transparent 75%,$square 75%)," +
        "linear-gradient(-45deg,transparent 75%,$square 75%)}" +
        ".stage{width:100%;height:100%;display:flex;align-items:center;justify-content:center}" +
        stageSvg
    return document(style, """<div class="stage">$markup</div>""")
}

private fun document(style: String, body: String): String =
    """<!doctype html><html><head><meta charset="utf-8">""" +
        """<meta name="viewport" content="width=device-width,initial-scale=1">""" +
        """<style>$style</style></head><body>$body</body></html>"""
