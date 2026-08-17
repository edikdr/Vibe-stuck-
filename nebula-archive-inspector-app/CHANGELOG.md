# Changelog

## 0.13.0 — multi-selection, shared core, mobile rendering fixes

### Inspector

- **Multi-selection.** Tapping an element selects it and marks it with a number on the page; tapping the same
  element again inspects it. Any number of elements can be collected this way.
- **ALL AT ONCE** inspects the whole selection together and produces one AI block for all of it. Individual
  members can be removed, focused or cleared from the selection strip.
- Inspections now report the **box** (size and position), the **computed style** that actually applied
  (display, position, colours, typography, flex/grid, spacing, border, radius, z-index, overflow, transform,
  opacity, box-sizing) and every **attribute** — the facts a layout problem is diagnosed from, none of which
  existed before.
- **Copy for AI** was rewritten to be compact: one short line per fact, no fenced code blocks, a single
  one-line excerpt for the best source candidate and `path:line` for the rest. A typical element block is
  around ten lines instead of a page of Markdown.
- The in-page inspector no longer mutates the page. Outlines, marks and labels are painted on one canvas above
  the document instead of writing `!important` inline styles onto up to 2,000 elements — faster, and a site
  can no longer be altered by being inspected.
- Source ranking treats `.cta` in a stylesheet and `class="cta"` in markup as the same evidence, so markup is
  no longer pushed below CSS for the same class.

### Rendering on phones and tablets

- **Fixed the black block that appeared under the page while scrolling down.** Three causes: the Android 12+
  stretch overscroll dragged the WebView's dark background into view, `LAYER_TYPE_HARDWARE` forced long pages
  into a single texture that exceeded the maximum size, and the navigation bar contrast scrim painted over the
  app background. Overscroll is off, the forced layer is gone, system bars are transparent with the contrast
  scrim disabled, and the window background matches the app surface.
- Removed a full-view invalidation on every scroll frame.
- Rotating a phone or tablet no longer destroys the open projects and their extracted cache.
- The keyboard resizes the workspace instead of covering it.
- Added a desktop viewport (1280 × 800) to the phone/tablet/fit device emulation.

### SVG

- The root tag is parsed with quote awareness, so a `>` inside an attribute no longer truncates the markup.
- The XML prolog, doctype and leading comments are skipped; `xmlns` is added when missing.
- `viewBox` is reconstructed from intrinsic sizes carrying units (`px`, `pt`, `mm`, `cm`, `in`, `em`, `rem`);
  percentage sizes are correctly left alone.
- The viewer gained a **fit / 1:1** switch next to the light/dark background toggle.

### Architecture

- Split `MainActivity.kt` (1,647 lines) and `ArchiveWebView.kt` (925 lines) into a layered tree:
  `core` (model, inspection contracts, scoring, search, AI context, SVG, formatting), `i18n`, `ui`
  (theme, components, panels, screens) — all in `commonMain` — plus thin platform adapters.
- Introduced `WorkspaceState`, one state holder that owns panels, overlays, selection, diagnostics and device
  emulation, and enforces their rules. The phone and wide layouts now render the same state instead of
  duplicating ~55 lines of callbacks each.
- Defined the platform boundary as four interfaces (`ArchiveOpener`, `RecentDocumentsStore`, `DocumentPicker`,
  `PreviewHost`) plus `PreviewCommands`. No `expect`/`actual` and no shared code is overridden per platform.
- ZIP extraction moved to a new `jvmShared` source set shared by Android and desktop.
- **Desktop** is no longer a stub: it opens archives, browses the file library, reads formatted sources,
  previews raster images and runs the whole inspector data model. Only the live page preview is left, and
  `DesktopPreviewHost` is where it plugs in.
- Removed dead code: `auditArchive`/`ArchiveAudit`, nine unused `AppStrings` fields, three per-version
  changelog files.
- Tests cover paths, entry points, SPA route resolution, source ranking, performance scoring, SVG preparation
  and the compact AI block. The injected inspector script is syntax-checked and its selection flow is
  exercised against a DOM stub.

## 0.12.0 and earlier

Render and device preview, element picker with DOM map, file library, page diagnostics, runtime performance
test, RU/EN switch, popup windows and page dialogs, interaction compatibility repairs.
