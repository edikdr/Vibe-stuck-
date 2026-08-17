package app.nebula.archive

import android.graphics.Color
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import app.nebula.archive.ui.NebulaApp

/**
 * The whole Android entry point: draw edge to edge, build the platform, run the shared app.
 *
 * System bars are transparent and their contrast scrim is switched off, so the app's own background reaches
 * every edge instead of leaving a dark band under the navigation bar. Insets are applied by the shared UI.
 */
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge(
            statusBarStyle = SystemBarStyle.dark(Color.TRANSPARENT),
            navigationBarStyle = SystemBarStyle.dark(Color.TRANSPARENT),
        )
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            window.isNavigationBarContrastEnforced = false
        }
        val platform = AndroidPlatform(this, AndroidDocumentPicker(this))
        setContent { NebulaApp(platform) }
    }
}
