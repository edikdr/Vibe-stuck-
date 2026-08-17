package app.nebula.archive

import androidx.compose.ui.unit.DpSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Window
import androidx.compose.ui.window.application
import androidx.compose.ui.window.rememberWindowState
import app.nebula.archive.ui.NebulaApp

fun main() = application {
    Window(
        onCloseRequest = ::exitApplication,
        title = "Nebula Archive Inspector",
        state = rememberWindowState(size = DpSize(1_280.dp, 840.dp)),
    ) {
        NebulaApp(DesktopPlatform())
    }
}
