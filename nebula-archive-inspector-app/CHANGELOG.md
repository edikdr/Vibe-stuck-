# Changelog

## 0.15.0 — picking that shows the real element, and the rule behind it

### Inspector

- **Picking hits what is under the finger.** The stack under the point is built from `elementsFromPoint`
  plus every `pointer-events:none` layer whose own box holds the point, ordered by CSS painting order. The
  topmost wins, an ancestor is never substituted for it, and the rest of the stack is offered so a
  decoration lying over a section background can be reached by hand.
- **Only the element being aimed at is drawn**, as its real box model — content, padding and margin, each in
  its own tint, with a label naming it and its size. Outlining every visible node at once, which is what the
  overlay did before, buried the page it was meant to explain: on a real site it turned the screen into a
  mesh nobody could read.
- **The page still scrolls while the inspector is on.** A press aims, a drag past the touch slop scrolls and
  drops the highlight, a tap picks. Before, the gesture was swallowed whole and nothing below the fold could
  be reached.
- **The cascade is reported, not guessed.** The preview is same-origin, so the page's own CSSOM is read:
  every rule that applied, heaviest first, with the archive file and line it was written on, the query that
  gated it, its specificity, and each declaration marked as winning or overridden.

### Copy for AI

- The block is roughly half as long for the same element. No sentences, no labels that repeat the value: the
  rule that won with its `path:line`, the computed values no rule already states, attributes, text, and the
  markup excerpt.

### Performance test

- Findings about the same file are folded into one, carrying how many times it appeared — six oversized
  copies of one image is one problem, not six.
- The frames right after load are no longer counted as jank; they are layout and paint finishing, and they
  used to make every page look janky.

## 0.14.0 — concrete performance causes, per-page testing, network throttling, archive diff

### Performance test

- The test now names **what** slows the page down instead of printing generic advice. Findings come with the
  element or file behind them: layout shifts attributed to the node that moved, long JavaScript tasks,
  render-blocking scripts, oversized images (with decoded vs painted size and wasted pixels), images without
  `width`/`height`, the heaviest downloads, expensive compositing properties, DOM size and script weight.
- A finding with a selector opens **straight in the element inspector**; one that maps to an archive file
  opens in the source viewer. The diagnosis and the tool that explains it are finally connected.
- **Every page can be measured in one run.** The test walks all HTML pages of the archive (up to 20, entry
  point first), shows `N/M` progress with a Stop button, reports an average plus the worst page, and returns
  the user to the page they started from.
- **Fixed a real scoring bug**: JavaScript weight was summed over the whole archive, so a multi-page site with
  code splitting was penalised for bundles the measured page never executed. Weight now comes from the
  resources the page actually loaded, and `evaluateRuntimePerformance` no longer takes the project at all.
- Layout shift and long-task time now affect the score.
- Three AI blocks, all compact and fence-free: the whole report, the problems of one page, or a single
  problem with its selector and target.

### Network throttling

- `FULL / 4G / 4G− / 3G` presets (Chrome DevTools values) cycle from the preview toolbar, so a site can simply
  be browsed under Slow 3G to watch skeletons and lazy loading behave.
- Bytes are really held back while serving archive files — the page loads slowly instead of being reported as
  slow — and latency is applied per request on Chromium's worker thread, never on the UI thread.
- Throttling never changes the score; the report is stamped with the profile it was measured under, so two
  runs cannot be compared by accident.
- Honest limits, stated in the UI: CPU throttling and a GPU/paint/composite breakdown are not available to an
  embedded WebView. The compositing-cost finding is the closest honest signal, named for what it is.

### Archive diff

- Two open archives can be compared: added, removed and modified files, byte delta, unchanged count. Equal
  size is not treated as equal content — same-size files are hashed.
- Text files open in a **line diff** with shared prefix/suffix trimmed and distant context dropped, so a
  one-line edit prints a few lines instead of the whole file. A rewrite too large to match line by line
  degrades to a labelled replacement block rather than stalling the device.
- One compact `Diff for AI` block summarises what changed between two builds.

### Internals

- `ThrottledInputStream` and `LimitedInputStream` moved to `jvmShared`, where a desktop preview will reuse
  them — and where they are unit tested. Testing them caught a real bug: `0` was used as "not started yet",
  which restarted the pacing budget on every read.
- 53 shared tests now cover scoring, findings, all three performance AI blocks, the diff, the line diff and
  the throttling maths. The injected performance script is exercised against a DOM stub with faked
  `PerformanceObserver` entries.

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
