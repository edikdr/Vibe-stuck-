package app.nebula.archive

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Color
import android.net.Uri
import android.os.Message
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import android.webkit.ConsoleMessage
import android.webkit.CookieManager
import android.webkit.JavascriptInterface
import android.webkit.JsPromptResult
import android.webkit.JsResult
import android.webkit.WebChromeClient
import android.webkit.WebResourceError
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import org.json.JSONArray
import org.json.JSONObject
import java.io.ByteArrayInputStream
import java.io.FileInputStream
import java.io.InputStream
import kotlin.math.hypot

const val PREVIEW_HOST = "nebula.local"
const val PREVIEW_ORIGIN = "https://$PREVIEW_HOST/"

sealed class PageDialogRequest {
    abstract val message: String
    abstract fun cancel()

    data class Alert(override val message: String, val result: JsResult) : PageDialogRequest() {
        override fun cancel() = result.cancel()
    }

    data class Confirm(override val message: String, val result: JsResult) : PageDialogRequest() {
        override fun cancel() = result.cancel()
    }

    data class Prompt(override val message: String, val defaultValue: String, val result: JsPromptResult) : PageDialogRequest() {
        override fun cancel() = result.cancel()
    }
}

/**
 * The Android preview: serves the archive from a virtual HTTPS origin and drives the in-page inspector.
 *
 * Files are served from disk rather than loaded into memory, external navigation is blocked while HTTPS
 * subresources stay allowed in `NET` mode, and every failure is reported as a [PreviewIssue] instead of
 * silently producing a blank page.
 */
@SuppressLint("SetJavaScriptEnabled", "AddJavascriptInterface")
class ArchiveWebView(context: Context) : WebView(context), PreviewCommands {
    private var project: ArchiveProject? = null
    private var pageLoaded = false
    private var inspectorEnabled = false
    private var onlineResources = true
    @Volatile
    private var networkProfile = NetworkProfile.Unthrottled
    private var popupWindow = false
    private var loadingProgress = 0
    private var touchDownX = 0f
    private var touchDownY = 0f
    private var touchScrolling = false
    private val touchSlop by lazy { ViewConfiguration.get(context).scaledTouchSlop.toFloat() }
    private var performanceToken = 0

    var onInspected: ((List<InspectedElement>) -> Unit)? = null
    var onSelectionChanged: ((List<SelectedElement>) -> Unit)? = null
    var onCandidatesChanged: ((List<ElementCandidate>) -> Unit)? = null
    var onPerformanceResult: ((RuntimeMetrics, List<PerformanceFinding>) -> Unit)? = null
    var onNavigationStateChanged: ((PreviewNavigationState) -> Unit)? = null
    var onIssue: ((PreviewIssue) -> Unit)? = null
    var onPopupCreated: ((ArchiveWebView) -> Unit)? = null
    var onCloseRequested: (() -> Unit)? = null
    var onPageDialog: ((PageDialogRequest) -> Unit)? = null

    init {
        setBackgroundColor(Color.rgb(5, 7, 14))
        // No LAYER_TYPE_HARDWARE: forcing the whole WebView into a single texture makes long pages exceed the
        // maximum texture size and paint black bands. The view is hardware accelerated by tiles anyway.
        // No overscroll either: the Android 12+ stretch effect drags the dark view background into view at the
        // end of a page, which reads as a black block appearing under the content.
        overScrollMode = View.OVER_SCROLL_NEVER
        setRendererPriorityPolicy(RENDERER_PRIORITY_IMPORTANT, false)
        isNestedScrollingEnabled = true
        isVerticalScrollBarEnabled = true
        isHorizontalScrollBarEnabled = true
        with(settings) {
            javaScriptEnabled = true
            domStorageEnabled = true
            databaseEnabled = true
            allowFileAccess = false
            allowContentAccess = false
            blockNetworkImage = false
            loadsImagesAutomatically = true
            mediaPlaybackRequiresUserGesture = false
            mixedContentMode = WebSettings.MIXED_CONTENT_NEVER_ALLOW
            cacheMode = WebSettings.LOAD_DEFAULT
            javaScriptCanOpenWindowsAutomatically = true
            setSupportMultipleWindows(true)
            safeBrowsingEnabled = true
            // The archived page must be laid out exactly like a browser would: honour its own viewport meta
            // tag, scale a desktop-width layout down instead of cropping it, and never apply the system font
            // scale, which silently breaks pixel-based CSS layouts.
            useWideViewPort = true
            loadWithOverviewMode = true
            layoutAlgorithm = WebSettings.LayoutAlgorithm.NORMAL
            textZoom = 100
            setSupportZoom(true)
            builtInZoomControls = true
            displayZoomControls = false
        }
        CookieManager.getInstance().setAcceptCookie(true)
        CookieManager.getInstance().setAcceptThirdPartyCookies(this, true)
        addJavascriptInterface(InspectorBridge(), "NebulaInspector")
        webChromeClient = PreviewChromeClient()
        webViewClient = PreviewClient()
    }

    // ---- content ----------------------------------------------------------------------------------------

    fun show(project: ArchiveProject) {
        prepare(project)
        loadUrl(PREVIEW_ORIGIN)
    }

    /** Opens one archive file directly, so the browser applies its own decoder to it. */
    fun showRawFile(project: ArchiveProject, entry: ArchiveEntry) {
        prepare(project)
        loadUrl(PREVIEW_ORIGIN + encodePath(entry.path))
    }

    /** Wraps a media, font or model file in the shared preview document. */
    fun showAsset(project: ArchiveProject, entry: ArchiveEntry, modelNotice: String) {
        prepare(project)
        val html = assetPreviewDocument(entry, PREVIEW_ORIGIN + encodePath(entry.path), modelNotice)
        loadDataWithBaseURL(PREVIEW_ORIGIN + project.siteRoot.let { if (it.isBlank()) "" else "$it/" }, html, "text/html", "UTF-8", null)
    }

    /** Inlines an SVG so CSS can scale it; falls back to the raw file when the markup cannot be prepared. */
    fun showSvg(project: ArchiveProject, entry: ArchiveEntry, markup: String?, lightBackground: Boolean, fitToStage: Boolean) {
        val prepared = markup?.let(::prepareSvgMarkup)
        if (prepared == null) {
            showRawFile(project, entry)
            return
        }
        prepare(project)
        val directory = entry.path.substringBeforeLast('/', "")
        val base = PREVIEW_ORIGIN + if (directory.isBlank()) "" else "${encodePath(directory)}/"
        loadDataWithBaseURL(base, svgPreviewDocument(prepared, lightBackground, fitToStage), "text/html", "UTF-8", null)
    }

    private fun prepare(project: ArchiveProject) {
        this.project = project
        pageLoaded = false
        stopLoading()
        clearHistory()
    }

    private fun encodePath(path: String) = path.split('/').joinToString("/") { Uri.encode(it) }

    // ---- PreviewCommands --------------------------------------------------------------------------------

    override fun navigateBack() {
        if (canGoBack()) goBack()
    }

    override fun navigateForward() {
        if (canGoForward()) goForward()
    }

    override fun reload() = super.reload()

    override fun setInspectorEnabled(enabled: Boolean) {
        if (inspectorEnabled == enabled && pageLoaded) return
        inspectorEnabled = enabled
        if (pageLoaded) applyInspectorState()
    }

    override fun setOnlineResourcesEnabled(enabled: Boolean) {
        onlineResources = enabled
    }

    override fun setNetworkProfile(profile: NetworkProfile) {
        networkProfile = profile
    }

    override fun openPage(path: String) {
        if (project == null || path.isBlank()) return
        pageLoaded = false
        loadUrl(PREVIEW_ORIGIN + encodePath(path))
    }

    override fun pickCandidate(index: Int) = callInspector("pickCandidate($index)")

    override fun highlightCandidate(index: Int) = callInspector("highlightCandidate($index)")

    override fun dismissCandidates() = callInspector("dismissCandidates()")

    override fun inspectSelector(selector: String) =
        callInspector("inspectSelector(${JSONObject.quote(selector)})")

    override fun inspectSelected(id: Int) = callInspector("inspect($id)")

    override fun dropSelected(id: Int) = callInspector("drop($id)")

    override fun clearSelection() = callInspector("clear()")

    override fun inspectWholeSelection() = callInspector("inspectAll()")

    override fun runPerformanceTest(): Boolean {
        if (!pageLoaded) return false
        // Picking must be off while measuring, and the inspector's selector builder has to be present so a
        // layout shift can be attributed to a real element.
        setInspectorEnabled(false)
        applyInspectorState()
        performanceToken += 1
        evaluateJavascript("($PERFORMANCE_SCRIPT)($performanceToken);", null)
        return true
    }

    override fun cancelPerformanceTest() {
        performanceToken += 1
    }

    private fun callInspector(call: String) {
        if (!pageLoaded) return
        evaluateJavascript("window.__nebulaInspector&&window.__nebulaInspector.$call;", null)
    }

    // ---- picking ----------------------------------------------------------------------------------------

    /**
     * While the inspector is on, a tap picks an element - but a drag still scrolls the page.
     *
     * Pressing shows what is under the finger straight away, which is the only aiming a touch screen can
     * offer; a finger that travels past the touch slop is scrolling, so the highlight is dropped and the
     * gesture is left alone. Every event still reaches the WebView, because a page that cannot be scrolled
     * cannot be inspected below the fold - which is what swallowing the whole gesture used to cost.
     * Coordinates are converted to CSS pixels inside the page, which keeps picking aligned in an emulated
     * viewport.
     */
    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!inspectorEnabled || !pageLoaded) return super.onTouchEvent(event)
        val density = resources.displayMetrics.density.coerceAtLeast(1f)
        val x = event.x / density
        val y = event.y / density
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                touchDownX = event.x
                touchDownY = event.y
                touchScrolling = false
                callInspector("hoverAt($x,$y)")
            }
            MotionEvent.ACTION_MOVE -> if (!touchScrolling && travelled(event) > touchSlop) {
                touchScrolling = true
                callInspector("clearHover()")
            }
            MotionEvent.ACTION_UP -> if (touchScrolling) {
                callInspector("clearHover()")
            } else {
                performClick()
                callInspector("pickAt($x,$y)")
            }
            MotionEvent.ACTION_CANCEL -> {
                touchScrolling = false
                callInspector("clearHover()")
            }
        }
        return super.onTouchEvent(event)
    }

    private fun travelled(event: MotionEvent): Float = hypot(event.x - touchDownX, event.y - touchDownY)

    override fun performClick(): Boolean {
        super.performClick()
        return true
    }

    // ---- reporting --------------------------------------------------------------------------------------

    private fun emitNavigationState(currentUrl: String = url.orEmpty()) {
        val path = currentUrl.removePrefix(PREVIEW_ORIGIN).substringBefore('#').substringBefore('?')
        post {
            onNavigationStateChanged?.invoke(
                PreviewNavigationState(canGoBack(), canGoForward(), Uri.decode(path).orEmpty(), loadingProgress, pageLoaded),
            )
        }
    }

    private fun reportIssue(issue: PreviewIssue) {
        post { onIssue?.invoke(issue) }
    }

    private fun applyInspectorState() {
        evaluateJavascript("($INSPECTOR_SCRIPT)($inspectorEnabled);", null)
    }

    // ---- serving ----------------------------------------------------------------------------------------

    private fun archiveResponse(project: ArchiveProject, path: String, rangeHeader: String?): WebResourceResponse {
        applyLatency()
        val diskFile = (project.content as? DiskArchiveSource)?.file(path)
        val mime = mimeType(path)
        val encoding = if (isTextMime(mime)) "UTF-8" else null
        if (diskFile == null) {
            val bytes = project.bytes(path) ?: return notFoundResponse()
            return WebResourceResponse(mime, encoding, throttled(ByteArrayInputStream(bytes))).apply {
                responseHeaders = mapOf("Cache-Control" to "no-store", "Access-Control-Allow-Origin" to "*")
            }
        }
        val length = diskFile.length()
        val range = parseRange(rangeHeader, length)
        if (range != null) {
            // Media elements seek with ranges; answering the whole file makes long audio and video unseekable.
            val stream = FileInputStream(diskFile).apply { channel.position(range.first) }
            val count = range.last - range.first + 1
            return WebResourceResponse(
                mime,
                encoding,
                206,
                "Partial Content",
                mapOf(
                    "Accept-Ranges" to "bytes",
                    "Content-Range" to "bytes ${range.first}-${range.last}/$length",
                    "Content-Length" to count.toString(),
                    "Cache-Control" to "no-store",
                    "Access-Control-Allow-Origin" to "*",
                ),
                throttled(LimitedInputStream(stream, count)),
            )
        }
        return WebResourceResponse(mime, encoding, throttled(FileInputStream(diskFile))).apply {
            responseHeaders = mapOf(
                "Content-Length" to length.toString(),
                "Accept-Ranges" to "bytes",
                "Cache-Control" to "no-store",
                "Access-Control-Allow-Origin" to "*",
            )
        }
    }

    /**
     * Simulated round trip. `shouldInterceptRequest` runs on a Chromium worker thread, never on the UI
     * thread, so holding it is how a slow network is reproduced rather than faked in a timer.
     */
    private fun applyLatency() {
        val latency = networkProfile.latencyMs
        if (latency > 0) runCatching { Thread.sleep(latency) }
    }

    private fun throttled(stream: InputStream): InputStream {
        val rate = networkProfile.bytesPerSecond
        return if (rate > 0) ThrottledInputStream(stream, rate) else stream
    }

    private fun blockedResponse() = WebResourceResponse(
        "text/plain", "UTF-8", 403, "Blocked", emptyMap(), ByteArrayInputStream(ByteArray(0)),
    )

    private fun notFoundResponse() = WebResourceResponse(
        "text/plain", "UTF-8", 404, "Not found", emptyMap(), ByteArrayInputStream("Not found in archive".encodeToByteArray()),
    )

    // ---- clients ----------------------------------------------------------------------------------------

    private inner class PreviewChromeClient : WebChromeClient() {
        override fun onProgressChanged(view: WebView, newProgress: Int) {
            loadingProgress = newProgress
            emitNavigationState()
        }

        override fun onConsoleMessage(message: ConsoleMessage): Boolean {
            if (message.messageLevel() in setOf(ConsoleMessage.MessageLevel.ERROR, ConsoleMessage.MessageLevel.WARNING)) {
                reportIssue(
                    PreviewIssue(
                        PreviewIssueKind.JavaScript,
                        message.message(),
                        "${message.sourceId().substringAfterLast('/')}:${message.lineNumber()}",
                    ),
                )
            }
            return true
        }

        override fun onCreateWindow(view: WebView, isDialog: Boolean, isUserGesture: Boolean, resultMsg: Message): Boolean {
            val currentProject = project ?: return false
            val transport = resultMsg.obj as? WebView.WebViewTransport ?: return false
            val popup = ArchiveWebView(view.context).apply {
                project = currentProject
                popupWindow = true
                onlineResources = this@ArchiveWebView.onlineResources
                onIssue = this@ArchiveWebView.onIssue
                onPopupCreated = this@ArchiveWebView.onPopupCreated
                onPageDialog = this@ArchiveWebView.onPageDialog
            }
            transport.webView = popup
            resultMsg.sendToTarget()
            post { onPopupCreated?.invoke(popup) }
            return true
        }

        override fun onCloseWindow(window: WebView) {
            post { onCloseRequested?.invoke() }
        }

        override fun onJsAlert(view: WebView, url: String, message: String, result: JsResult): Boolean {
            onPageDialog?.invoke(PageDialogRequest.Alert(message, result)) ?: result.cancel()
            return true
        }

        override fun onJsConfirm(view: WebView, url: String, message: String, result: JsResult): Boolean {
            onPageDialog?.invoke(PageDialogRequest.Confirm(message, result)) ?: result.cancel()
            return true
        }

        override fun onJsPrompt(view: WebView, url: String, message: String, defaultValue: String, result: JsPromptResult): Boolean {
            onPageDialog?.invoke(PageDialogRequest.Prompt(message, defaultValue, result)) ?: result.cancel()
            return true
        }
    }

    private inner class PreviewClient : WebViewClient() {
        override fun onPageStarted(view: WebView, url: String, favicon: Bitmap?) {
            pageLoaded = false
            loadingProgress = 0
            performanceToken += 1
            emitNavigationState(url)
        }

        override fun onPageFinished(view: WebView, url: String) {
            pageLoaded = true
            loadingProgress = 100
            evaluateJavascript("($PAGE_COMPATIBILITY_SCRIPT)();", null)
            applyInspectorState()
            emitNavigationState(url)
        }

        override fun doUpdateVisitedHistory(view: WebView, url: String, isReload: Boolean) = emitNavigationState(url)

        override fun shouldOverrideUrlLoading(view: WebView, request: WebResourceRequest): Boolean {
            val uri = request.url
            if (uri.host == PREVIEW_HOST || uri.scheme in setOf("about", "data", "blob")) return false
            if (popupWindow && onlineResources && uri.scheme == "https") return false
            reportIssue(PreviewIssue(PreviewIssueKind.Blocked, "External page navigation was blocked.", uri.toString()))
            return true
        }

        override fun shouldInterceptRequest(view: WebView, request: WebResourceRequest): WebResourceResponse? {
            val uri = request.url
            if (uri.scheme in setOf("about", "data", "blob")) return null
            if (uri.host != PREVIEW_HOST) {
                if (onlineResources && uri.scheme == "https" && (!request.isDocumentRequest() || popupWindow)) return null
                val message = when {
                    !onlineResources -> "Online resources are disabled."
                    uri.scheme == "http" -> "Insecure HTTP resources are blocked; use HTTPS."
                    else -> "External resource was blocked."
                }
                reportIssue(PreviewIssue(PreviewIssueKind.Blocked, message, uri.toString()))
                return blockedResponse()
            }
            val currentProject = project ?: return notFoundResponse()
            val decoded = Uri.decode(uri.encodedPath.orEmpty()).orEmpty().trimStart('/')
            val path = resolveArchiveRequestPath(currentProject, decoded, request.isDocumentRequest())
            if (path == null) {
                reportIssue(PreviewIssue(PreviewIssueKind.MissingFile, "File was not found in the archive.", decoded))
                return notFoundResponse()
            }
            return archiveResponse(currentProject, path, request.requestHeaders["Range"])
        }

        override fun onReceivedError(view: WebView, request: WebResourceRequest, error: WebResourceError) {
            reportIssue(
                PreviewIssue(
                    if (request.url.host == PREVIEW_HOST) PreviewIssueKind.MissingFile else PreviewIssueKind.Network,
                    error.description?.toString().orEmpty().ifBlank { "Resource request failed." },
                    request.url.toString(),
                ),
            )
        }

        override fun onReceivedHttpError(view: WebView, request: WebResourceRequest, response: WebResourceResponse) {
            if (response.statusCode < 400) return
            reportIssue(
                PreviewIssue(
                    if (request.url.host == PREVIEW_HOST) PreviewIssueKind.MissingFile else PreviewIssueKind.Network,
                    "HTTP ${response.statusCode} ${response.reasonPhrase.orEmpty()}".trim(),
                    request.url.toString(),
                ),
            )
        }
    }

    // ---- bridge -----------------------------------------------------------------------------------------

    private inner class InspectorBridge {
        @JavascriptInterface
        fun selection(payload: String) {
            runCatching { JSONArray(payload).map(::readSelectedElement) }
                .onSuccess { elements -> post { onSelectionChanged?.invoke(elements) } }
        }

        @JavascriptInterface
        fun candidates(payload: String) {
            runCatching { JSONArray(payload).map(::readElementCandidate) }
                .onSuccess { elements -> post { onCandidatesChanged?.invoke(elements) } }
        }

        @JavascriptInterface
        fun inspected(payload: String) {
            runCatching { JSONArray(payload).map(::readInspectedElement) }
                .onSuccess { elements -> if (elements.isNotEmpty()) post { onInspected?.invoke(elements) } }
        }

        @JavascriptInterface
        fun performanceResult(payload: String) {
            runCatching {
                val json = JSONObject(payload)
                Triple(json.optInt("runToken", -1), readMetrics(json), readFindings(json))
            }.onSuccess { (token, metrics, findings) ->
                if (token == performanceToken) post { onPerformanceResult?.invoke(metrics, findings) }
            }
        }
    }
}

// ---- JSON readers ---------------------------------------------------------------------------------------

private fun <T> JSONArray.map(read: (JSONObject) -> T): List<T> =
    (0 until length()).mapNotNull { index -> optJSONObject(index) }.map(read)

private fun JSONArray.strings(): List<String> = (0 until length()).map { optString(it) }.filter { it.isNotBlank() }

private fun JSONObject.properties(key: String): List<ElementProperty> =
    optJSONArray(key)?.map { ElementProperty(it.optString("name"), it.optString("value")) }
        ?.filter { it.name.isNotBlank() }
        .orEmpty()

private fun JSONObject.references(key: String): List<ElementReference> =
    optJSONArray(key)?.map {
        ElementReference(
            selector = it.optString("selector"),
            tag = it.optString("tag"),
            label = it.optString("label"),
            depth = it.optInt("depth").coerceAtLeast(0),
        )
    }?.filter { it.selector.isNotBlank() }.orEmpty()

private fun readSelectedElement(json: JSONObject) = SelectedElement(
    id = json.optInt("id"),
    selector = json.optString("selector"),
    tag = json.optString("tag"),
    label = json.optString("label"),
    depth = json.optInt("depth").coerceAtLeast(0),
    childCount = json.optInt("childCount").coerceAtLeast(0),
    width = json.optDouble("width", 0.0),
    height = json.optDouble("height", 0.0),
)

private fun readElementCandidate(json: JSONObject) = ElementCandidate(
    index = json.optInt("index"),
    selector = json.optString("selector"),
    tag = json.optString("tag"),
    label = json.optString("label"),
    depth = json.optInt("depth").coerceAtLeast(0),
    width = json.optDouble("width", 0.0),
    height = json.optDouble("height", 0.0),
    tier = json.optInt("tier").coerceIn(0, 2),
    decor = json.optBoolean("decor"),
    selected = json.optBoolean("selected"),
)

private fun readDeclarations(json: JSONObject, key: String): List<StyleDeclaration> =
    json.optJSONArray(key)?.map {
        StyleDeclaration(
            name = it.optString("name"),
            value = it.optString("value"),
            important = it.optBoolean("important"),
            winning = it.optBoolean("winning"),
        )
    }?.filter { it.name.isNotBlank() }.orEmpty()

private fun readRules(json: JSONObject): List<StyleRule> =
    json.optJSONArray("rules")?.map {
        StyleRule(
            path = it.optString("path"),
            selector = it.optString("selector"),
            specificity = it.optJSONArray("specificity")?.let { array ->
                (0 until array.length()).map { index -> array.optInt(index) }
            }.orEmpty(),
            condition = it.optString("condition"),
            ordinal = it.optInt("ordinal"),
            declarations = readDeclarations(it, "declarations"),
        )
    }?.filter { it.selector.isNotBlank() }.orEmpty()

private fun readInspectedElement(json: JSONObject) = InspectedElement(
    id = json.optInt("id"),
    selector = json.optString("selector"),
    elementId = json.optString("elementId"),
    classes = json.optJSONArray("classes")?.strings().orEmpty(),
    tag = json.optString("tag"),
    text = json.optString("text"),
    outerHtml = json.optString("outerHtml"),
    depth = json.optInt("depth").coerceAtLeast(0),
    childCount = json.optInt("childCount").coerceAtLeast(0),
    width = json.optDouble("width", 0.0),
    height = json.optDouble("height", 0.0),
    left = json.optDouble("left", 0.0),
    top = json.optDouble("top", 0.0),
    attributes = json.properties("attributes"),
    styles = json.properties("styles"),
    rules = readRules(json),
    inlineStyle = readDeclarations(json, "inlineStyle"),
    blockedSheets = json.optInt("blockedSheets"),
    openTag = json.optString("openTag"),
    parents = json.references("parents"),
    children = json.references("children"),
    siblings = json.references("siblings"),
)

private fun readMetrics(json: JSONObject) = RuntimeMetrics(
    responseStartMs = json.optDouble("responseStartMs", 0.0).coerceAtLeast(0.0),
    domContentLoadedMs = json.optDouble("domContentLoadedMs", 0.0).coerceAtLeast(0.0),
    loadMs = json.optDouble("loadMs", 0.0).coerceAtLeast(0.0),
    firstContentfulPaintMs = json.optDouble("firstContentfulPaintMs", -1.0).takeIf { it >= 0.0 },
    averageFrameMs = json.optDouble("averageFrameMs", 0.0).coerceAtLeast(0.0),
    worstFrameMs = json.optDouble("worstFrameMs", 0.0).coerceAtLeast(0.0),
    slowFramePercent = json.optDouble("slowFramePercent", 0.0).coerceIn(0.0, 100.0),
    domNodes = json.optInt("domNodes").coerceAtLeast(0),
    resourceCount = json.optInt("resourceCount").coerceAtLeast(0),
    decodedResourceBytes = json.optLong("decodedResourceBytes").coerceAtLeast(0L),
    scriptBytes = json.optLong("scriptBytes").coerceAtLeast(0L),
    layoutShiftScore = json.optDouble("layoutShiftScore", 0.0).coerceAtLeast(0.0),
    longTaskCount = json.optInt("longTaskCount").coerceAtLeast(0),
    longTaskTotalMs = json.optDouble("longTaskTotalMs", 0.0).coerceAtLeast(0.0),
)

private fun readFindings(json: JSONObject): List<PerformanceFinding> {
    val array = json.optJSONArray("findings") ?: return emptyList()
    return (0 until array.length()).mapNotNull { index ->
        val item = array.optJSONObject(index) ?: return@mapNotNull null
        val kind = PerformanceFindingKind.entries.firstOrNull { it.name == item.optString("kind") }
            ?: return@mapNotNull null
        PerformanceFinding(
            kind = kind,
            target = item.optString("target"),
            selector = item.optString("selector"),
            note = item.optString("note"),
            value = item.optDouble("value", 0.0),
            extra = item.optDouble("extra", 0.0),
        )
    }
}

/**
 * `isForMainFrame` is true for every resource requested by the main frame, not only for its document, so it
 * cannot be used on its own to tell a page navigation from a stylesheet, script, font or image. Chromium
 * reports the real destination through `Sec-Fetch-Dest`, and falls back to an `Accept` header that only
 * advertises `text/html` for document loads.
 */
private fun WebResourceRequest.isDocumentRequest(): Boolean {
    if (!isForMainFrame) return false
    val headers = requestHeaders.orEmpty()
    fun header(name: String) = headers.entries.firstOrNull { it.key.equals(name, ignoreCase = true) }?.value
    header("Sec-Fetch-Dest")?.let { return it.equals("document", ignoreCase = true) }
    val accept = header("Accept").orEmpty()
    return accept.isBlank() || accept.contains("text/html", ignoreCase = true)
}

private fun parseRange(value: String?, size: Long): LongRange? {
    if (value == null || !value.startsWith("bytes=") || size <= 0) return null
    val firstRange = value.removePrefix("bytes=").substringBefore(',')
    val start = firstRange.substringBefore('-').toLongOrNull() ?: return null
    val end = firstRange.substringAfter('-', "").toLongOrNull()?.coerceAtMost(size - 1) ?: (size - 1)
    return if (start in 0 until size && end >= start) start..end else null
}
