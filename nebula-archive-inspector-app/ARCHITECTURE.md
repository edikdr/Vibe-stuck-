# Architecture

The app is a Kotlin Multiplatform / Compose Multiplatform project with one rule: **anything that is not a
platform API lives in `commonMain`.** Android is currently the only complete target; desktop runs on the same
shared code and is missing only a live page preview.

```
composeApp/src/
├── commonMain/          shared model + shared UI (no platform APIs)
│   ├── core/            archive model, inspection contracts, scoring, search, formatting
│   ├── i18n/            AppStrings — every user-visible string, EN + RU
│   └── ui/              theme, components, panels, screens, WorkspaceState
├── jvmShared/           ZIP extraction and disk access shared by Android and desktop
├── androidMain/         Activity, platform services, WebView preview, in-page scripts
└── desktopMain/         window, platform services, desktop preview adapter
```

## Layers

**`core`** is pure Kotlin and fully testable.

| File | Responsibility |
| --- | --- |
| `ArchiveModel.kt` | entries, project, path resolution, Zip-Slip-safe normalisation |
| `Inspection.kt` | what an inspected element is, and `PreviewCommands` — what a preview must be able to do |
| `Performance.kt` | runtime score, findings and the simulated network profiles |
| `FindingText.kt` | localized wording for a finding — the page reports facts, Kotlin writes the sentence |
| `ArchiveDiff.kt` / `TextDiff.kt` | comparison of two archives, and the line diff for text files |
| `SourceSearch.kt` | element → candidate source files |
| `AiContext.kt` | the compact block behind "Copy for AI" |
| `Svg.kt` | makes a standalone SVG scalable |
| `PreviewDocuments.kt` | HTML wrappers for single-file previews |
| `SourceFormatter.kt` | re-indents minified HTML/CSS/JS/JSON for reading |

**`ui`** is Compose Multiplatform and platform independent. `WorkspaceState` is the single state holder for an
open project: it owns panels, overlays, the multi-selection, diagnostics and the device emulation, and it
enforces the rules that keep them consistent (panels are mutually exclusive; a page navigation invalidates
every inspection). Both the phone layout and the wide layout render the same state, so behaviour cannot drift
between them.

**Platform boundary** — a platform implements four things, in `ui/PlatformShell.kt`:

```kotlin
interface NebulaPlatform {
    val opener: ArchiveOpener                 // reference -> ArchiveProject
    val recentDocuments: RecentDocumentsStore
    val documentPicker: DocumentPicker        // system file picker
    val previewHost: PreviewHost              // @Composable surfaces + PreviewCommands
    fun copyToClipboard(text: String, confirmation: String)
}
```

`PreviewCommands` is the other half of the same boundary: the shared UI calls it, the platform preview
implements it. On Android that implementation is `ArchiveWebView`.

## Inspection flow

```
tap ──> InspectorScript.pickAt()
         ├── element not selected yet → adds it, draws a numbered mark, emits `selection`
         └── element already selected → emits `inspected` with the full detail
                                             │
NebulaInspector (JS bridge) ────────────────┘
         │  JSON
ArchiveWebView.InspectorBridge → WorkspaceState.onInspected()
         │
WorkspaceScreen: LaunchedEffect → findElementSources() on a background dispatcher
         │
InspectionPanel: box, computed style, attributes, DOM tree, live DOM, source candidates
         │
buildAiContext(): one compact block for one element or for the whole selection
```

The in-page inspector never mutates the page: outlines, selection marks and labels are painted on a single
fixed canvas above the document. Nothing has to be restored when inspection ends, and a site cannot be broken
by being inspected.

## Adding a platform

1. Implement `ArchiveOpener` (JVM targets can reuse `JvmArchiveExtractor` from `jvmShared`).
2. Implement `RecentDocumentsStore` and `DocumentPicker`.
3. Implement `PreviewHost`; return `supportsLivePreview = false` until a browser surface exists.
4. Call `NebulaApp(platform)` from the platform entry point.

Nothing else is platform specific — screens, panels, state, scoring, search and strings are already shared.

## Desktop status

`DesktopPlatform` implements the whole contract. Opening archives, the file library, the source viewer, the
inspector data model and the AI block all work. `DesktopPreviewHost.supportsLivePreview` is `false`: a live
preview needs an embedded browser (JCEF or a Compose WebView), and it is the single remaining piece. Raster
images already render natively through `ImageIO`.
