package app.nebula.archive.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.saveable.Saver
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import app.nebula.archive.AppLocale
import app.nebula.archive.AppStrings
import app.nebula.archive.ArchiveProject
import app.nebula.archive.RecentDocument
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

/** Four projects fit in the tab strip of a phone; beyond that the strip stops being readable. */
private const val MAX_OPEN_PROJECTS = 4

private val LocaleSaver = Saver<AppLocale, Int>(save = { it.ordinal }, restore = { AppLocale.entries[it] })

/**
 * Root of the application: owns the open projects and the locale, and hands everything else to the screens.
 * The platform passes itself in, so this composable is identical on every target.
 */
@Composable
fun NebulaApp(platform: NebulaPlatform) {
    val scope = rememberCoroutineScope()
    var locale by rememberSaveable(stateSaver = LocaleSaver) { mutableStateOf(AppLocale.English) }
    val strings = AppStrings.forLocale(locale)

    var recentDocuments by remember { mutableStateOf(platform.recentDocuments.load()) }
    var projects by remember { mutableStateOf<List<ArchiveProject>>(emptyList()) }
    var activeProjectId by remember { mutableStateOf<Long?>(null) }
    var loading by remember { mutableStateOf(false) }
    var error by remember { mutableStateOf<String?>(null) }

    val openReferences: (List<String>) -> Unit = { references ->
        scope.launch {
            loading = true
            error = null
            val available = (MAX_OPEN_PROJECTS - projects.size).coerceAtLeast(0)
            if (available == 0) {
                error = strings.closeTabFirst
            } else {
                references.distinct().take(available).forEach { reference ->
                    runCatching { platform.opener.open(reference) }
                        .onSuccess { project ->
                            projects = projects + project
                            activeProjectId = project.openedAtMs
                            recentDocuments = platform.recentDocuments.record(reference, project.name)
                        }
                        .onFailure { caught -> error = caught.message ?: strings.openFailed }
                }
                if (references.size > available) error = strings.tooManyProjects
            }
            loading = false
        }
    }
    val chooseDocument = { platform.documentPicker.pick(openReferences) }

    // Extracted archives live in a cache directory that has to be released when the app goes away.
    val latestProjects by rememberUpdatedState(projects)
    DisposableEffect(Unit) {
        onDispose { latestProjects.forEach { runCatching { it.close() } } }
    }

    NebulaTheme {
        CompositionLocalProvider(LocalCopyText provides { text -> platform.copyToClipboard(text, strings.copied) }) {
            Surface(color = Space, modifier = Modifier.fillMaxSize()) {
                Box(Modifier.fillMaxSize()) {
                    val activeProject = projects.firstOrNull { it.openedAtMs == activeProjectId } ?: projects.lastOrNull()
                    if (activeProject == null) {
                        LandingScreen(
                            strings = strings,
                            loading = loading,
                            error = error,
                            recentDocuments = recentDocuments,
                            onChooseDocument = chooseDocument,
                            onOpenRecent = { item: RecentDocument -> openReferences(listOf(item.reference)) },
                            onToggleLocale = { locale = locale.other() },
                        )
                    } else {
                        // A new project must not inherit the previous one's inspector, panels or measurements.
                        key(activeProject.openedAtMs) {
                            WorkspaceScreen(
                                project = activeProject,
                                projects = projects,
                                locale = locale,
                                platform = platform,
                                onSelectProject = { activeProjectId = it.openedAtMs },
                                onOpenAnother = chooseDocument,
                                onToggleLocale = { locale = locale.other() },
                                onClose = {
                                    val remaining = projects.filterNot { it.openedAtMs == activeProject.openedAtMs }
                                    projects = remaining
                                    activeProjectId = remaining.lastOrNull()?.openedAtMs
                                    scope.launch(Dispatchers.Default) { runCatching { activeProject.close() } }
                                },
                            )
                        }
                    }
                }
            }
        }
    }
}

private fun AppLocale.other() = if (this == AppLocale.English) AppLocale.Russian else AppLocale.English
