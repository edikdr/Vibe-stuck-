package app.nebula.archive

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.requiredSize
import androidx.compose.foundation.layout.widthIn
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TextField
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.window.Dialog
import app.nebula.archive.ui.Border
import app.nebula.archive.ui.MonoText
import app.nebula.archive.ui.Nebula
import app.nebula.archive.ui.PanelRaised
import app.nebula.archive.ui.PreviewHost
import app.nebula.archive.ui.SectionLabel
import app.nebula.archive.ui.SmallAction
import app.nebula.archive.ui.Space
import app.nebula.archive.ui.TextMain
import app.nebula.archive.ui.TextMuted
import app.nebula.archive.ui.Violet
import app.nebula.archive.ui.WorkspaceState
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

private const val MAX_IMAGE_PIXELS = 4096

/** Renders archived pages and files with an Android `WebView`. */
object AndroidPreviewHost : PreviewHost {
    override val supportsLivePreview = true

    @Composable
    override fun Preview(state: WorkspaceState, modifier: Modifier) {
        val context = LocalContext.current
        val webView = remember(state.project.openedAtMs) { ArchiveWebView(context) }
        var popup by remember { mutableStateOf<ArchiveWebView?>(null) }
        var dialog by remember { mutableStateOf<PageDialogRequest?>(null) }

        DisposableEffect(webView) {
            state.preview = webView
            onDispose {
                state.preview = PreviewCommands.Unavailable
                releaseWebView(webView)
            }
        }
        DisposableEffect(popup) {
            val current = popup
            onDispose { current?.let(::releaseWebView) }
        }

        DevicePreview(state, webView, modifier) { view ->
            view.onInspected = state::onInspected
            view.onSelectionChanged = state::onSelectionChanged
            view.onNavigationStateChanged = state::onNavigation
            view.onPerformanceResult = state::onPerformanceResult
            view.onIssue = state::onIssue
            view.onPopupCreated = { child -> popup = child }
            view.onPageDialog = { request -> dialog = request }
        }
        popup?.let { child ->
            PopupPreview(state, child, onPopup = { popup = it }, onClose = { popup = null })
        }
        dialog?.let { request -> PageDialog(state, request) { dialog = null } }
    }

    @Composable
    override fun AssetPreview(state: WorkspaceState, entry: ArchiveEntry, modifier: Modifier) {
        val context = LocalContext.current
        val project = state.project
        val viewer = remember(project.openedAtMs, entry.path) { ArchiveWebView(context) }
        var dialog by remember(viewer) { mutableStateOf<PageDialogRequest?>(null) }
        var decoded by remember(project.openedAtMs, entry.path) { mutableStateOf<Bitmap?>(null) }
        var decodeFinished by remember(project.openedAtMs, entry.path) { mutableStateOf(entry.kind != ArchiveKind.Image) }
        var svgMarkup by remember(project.openedAtMs, entry.path) { mutableStateOf<String?>(null) }

        LaunchedEffect(project.openedAtMs, entry.path) {
            if (entry.kind != ArchiveKind.Image) return@LaunchedEffect
            withContext(Dispatchers.IO) {
                if (entry.isSvg) svgMarkup = project.text(entry.path) else decoded = decodeArchiveImage(project, entry.path)
            }
            decodeFinished = true
        }
        DisposableEffect(viewer) {
            onDispose { releaseWebView(viewer) }
        }

        val image = decoded
        when {
            image != null -> Box(modifier.background(Color.Black)) {
                Image(image.asImageBitmap(), entry.path, Modifier.fillMaxSize(), contentScale = ContentScale.Fit)
            }
            !decodeFinished -> Box(modifier, contentAlignment = Alignment.Center) {
                Text(state.strings.loadingImage, color = TextMuted)
            }
            else -> AndroidView(
                factory = {
                    (viewer.parent as? ViewGroup)?.removeView(viewer)
                    viewer
                },
                update = { view ->
                    view.setOnlineResourcesEnabled(true)
                    view.onPageDialog = { request -> dialog = request }
                    // Reloading only when the request actually changes keeps toggling an option cheap.
                    val marker = "${project.openedAtMs}:${entry.path}:${state.svgLightBackground}:${state.svgFitToStage}"
                    if (view.tag != marker) {
                        view.tag = marker
                        when {
                            entry.isSvg -> view.showSvg(project, entry, svgMarkup, state.svgLightBackground, state.svgFitToStage)
                            entry.kind == ArchiveKind.Image -> view.showRawFile(project, entry)
                            else -> view.showAsset(project, entry, state.strings.modelOnlineNotice)
                        }
                    }
                },
                modifier = modifier,
            )
        }
        dialog?.let { request -> PageDialog(state, request) { dialog = null } }
    }
}

/**
 * Hosts the preview WebView, optionally inside an emulated device viewport.
 *
 * The WebView is laid out at the device's real CSS size and shrunk with a native view scale rather than a
 * Compose `graphicsLayer` transform. Compose forwards pointer events to an `AndroidView` through an interop
 * filter that only compensates for translation, so a Compose-side scale would leave every tap offset from
 * what the user touched. A scaled child of a `FrameLayout` gets its touches mapped back through the inverse
 * matrix by the platform, which keeps both page interaction and element picking accurate.
 */
@Composable
private fun DevicePreview(
    state: WorkspaceState,
    webView: ArchiveWebView,
    modifier: Modifier,
    bind: (ArchiveWebView) -> Unit,
) {
    val density = LocalDensity.current
    val device = state.device
    val landscape = state.landscape
    BoxWithConstraints(
        modifier.background(if (device.emulated) Color(0xFF070B14) else Color.Transparent),
        contentAlignment = Alignment.Center,
    ) {
        val viewportWidth = if (device.emulated) device.viewportWidthDp(landscape).dp else maxWidth
        val viewportHeight = if (device.emulated) device.viewportHeightDp(landscape).dp else maxHeight
        val margin = if (device.emulated) 16.dp else 0.dp
        val scale = if (!device.emulated) {
            1f
        } else {
            minOf((maxWidth - margin) / viewportWidth, (maxHeight - margin) / viewportHeight).coerceIn(0.05f, 1f)
        }
        val widthPx = if (device.emulated) with(density) { viewportWidth.roundToPx() } else ViewGroup.LayoutParams.MATCH_PARENT
        val heightPx = if (device.emulated) with(density) { viewportHeight.roundToPx() } else ViewGroup.LayoutParams.MATCH_PARENT

        Box(
            Modifier.requiredSize(viewportWidth * scale, viewportHeight * scale)
                .then(if (device.emulated) Modifier.border(1.dp, Color(0xFF32436A)) else Modifier),
        ) {
            AndroidView(
                modifier = Modifier.fillMaxSize(),
                factory = { context -> FrameLayout(context).apply { clipChildren = true } },
                update = { frame ->
                    if (webView.parent !== frame) {
                        (webView.parent as? ViewGroup)?.removeView(webView)
                        frame.removeAllViews()
                        frame.addView(webView, FrameLayout.LayoutParams(widthPx, heightPx))
                    } else {
                        val params = webView.layoutParams as FrameLayout.LayoutParams
                        if (params.width != widthPx || params.height != heightPx) {
                            params.width = widthPx
                            params.height = heightPx
                            webView.layoutParams = params
                        }
                    }
                    webView.pivotX = 0f
                    webView.pivotY = 0f
                    webView.scaleX = scale
                    webView.scaleY = scale
                    bind(webView)
                    webView.setOnlineResourcesEnabled(state.onlineResources)
                    webView.setNetworkProfile(state.networkProfile)
                    if (webView.tag != state.project.openedAtMs) {
                        webView.tag = state.project.openedAtMs
                        webView.show(state.project)
                    }
                    webView.setInspectorEnabled(state.inspecting)
                },
            )
        }
    }
}

/** A real child WebView for `window.open` and `target="_blank"`, shown as an in-app window. */
@Composable
private fun PopupPreview(
    state: WorkspaceState,
    webView: ArchiveWebView,
    onPopup: (ArchiveWebView) -> Unit,
    onClose: () -> Unit,
) {
    var dialog by remember(webView) { mutableStateOf<PageDialogRequest?>(null) }
    DisposableEffect(webView) {
        webView.onCloseRequested = onClose
        onDispose { webView.onCloseRequested = null }
    }
    Column(Modifier.fillMaxSize().background(Color(0xF7050810)).border(1.dp, Nebula).padding(6.dp)) {
        Row(
            Modifier.fillMaxWidth().height(44.dp).background(PanelRaised).padding(horizontal = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text("▣", color = Violet, fontWeight = FontWeight.Black)
            Column(Modifier.weight(1f)) {
                SectionLabel(state.strings.siteWindow)
                MonoText(webView.url.orEmpty().ifBlank { "about:blank" }, size = 8)
            }
            SmallAction("×", onClose)
        }
        AndroidView(
            factory = {
                // Compose can only attach a view that has no parent; an adopted popup still belongs to Chromium.
                (webView.parent as? ViewGroup)?.removeView(webView)
                webView
            },
            update = { view ->
                view.onPopupCreated = onPopup
                view.onCloseRequested = onClose
                view.onPageDialog = { request -> dialog = request }
            },
            modifier = Modifier.fillMaxSize().background(Color.White),
        )
    }
    dialog?.let { request -> PageDialog(state, request) { dialog = null } }
}

/** `alert`, `confirm` and `prompt` as lifecycle-safe Compose dialogs instead of raw system dialogs. */
@Composable
private fun PageDialog(state: WorkspaceState, request: PageDialogRequest, onDismiss: () -> Unit) {
    val strings = state.strings
    var promptText by remember(request) { mutableStateOf((request as? PageDialogRequest.Prompt)?.defaultValue.orEmpty()) }
    var resolved by remember(request) { mutableStateOf(false) }
    DisposableEffect(request) {
        onDispose { if (!resolved) runCatching { request.cancel() } }
    }
    val cancel = {
        if (!resolved) {
            resolved = true
            runCatching { request.cancel() }
        }
        onDismiss()
    }
    Dialog(onDismissRequest = cancel) {
        Column(
            Modifier.fillMaxWidth().widthIn(max = 520.dp).clip(MaterialTheme.shapes.large).background(PanelRaised)
                .border(1.dp, Nebula, MaterialTheme.shapes.large).padding(18.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp),
        ) {
            Text(request.message, color = TextMain, fontSize = 13.sp, lineHeight = 19.sp)
            if (request is PageDialogRequest.Prompt) {
                TextField(
                    value = promptText,
                    onValueChange = { promptText = it },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                    colors = TextFieldDefaults.colors(
                        focusedContainerColor = Space,
                        unfocusedContainerColor = Space,
                        focusedTextColor = TextMain,
                        unfocusedTextColor = TextMain,
                        focusedIndicatorColor = Nebula,
                        unfocusedIndicatorColor = TextMuted,
                    ),
                )
            }
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                if (request !is PageDialogRequest.Alert) {
                    TextButton(onClick = cancel) { Text(strings.cancel, color = TextMuted) }
                }
                TextButton(onClick = {
                    if (!resolved) {
                        resolved = true
                        when (request) {
                            is PageDialogRequest.Alert -> request.result.confirm()
                            is PageDialogRequest.Confirm -> request.result.confirm()
                            is PageDialogRequest.Prompt -> request.result.confirm(promptText)
                        }
                    }
                    onDismiss()
                }) { Text(strings.ok, color = Nebula) }
            }
        }
    }
}

/** Large images are sampled down first: a full-size decode of a 12000px asset would exhaust the heap. */
private fun decodeArchiveImage(project: ArchiveProject, path: String): Bitmap? {
    val diskFile = (project.content as? DiskArchiveSource)?.file(path)
    val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
    if (diskFile != null) {
        BitmapFactory.decodeFile(diskFile.absolutePath, bounds)
    } else {
        project.bytes(path)?.let { BitmapFactory.decodeByteArray(it, 0, it.size, bounds) }
    }
    var sample = 1
    while (bounds.outWidth / sample > MAX_IMAGE_PIXELS || bounds.outHeight / sample > MAX_IMAGE_PIXELS) sample *= 2
    val options = BitmapFactory.Options().apply { inSampleSize = sample }
    return if (diskFile != null) {
        BitmapFactory.decodeFile(diskFile.absolutePath, options)
    } else {
        project.bytes(path)?.let { BitmapFactory.decodeByteArray(it, 0, it.size, options) }
    }
}

/** A WebView must leave the view hierarchy before it is destroyed. */
private fun releaseWebView(webView: ArchiveWebView) {
    webView.stopLoading()
    webView.scaleX = 1f
    webView.scaleY = 1f
    (webView.parent as? ViewGroup)?.removeView(webView)
    webView.destroy()
}
