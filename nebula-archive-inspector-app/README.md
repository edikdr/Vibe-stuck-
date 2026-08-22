# Nebula Archive Inspector

An offline viewer and element inspector for static websites shipped as a ZIP or a single HTML file. Open an
archive on a phone or tablet, browse the site exactly as a browser would render it, tap any element, and get
back what you need to actually fix its layout — its box, its computed style, its attributes, and the source
files it most likely comes from.

Kotlin Multiplatform + Compose Multiplatform. Android is complete; desktop runs the same shared code and is
only missing a live page preview (see [ARCHITECTURE.md](ARCHITECTURE.md)).

Nothing is uploaded and nothing in the archive is ever modified.

## Inspecting elements

- **Picking hits what is under the finger.** The inspector builds the whole stack of layers under the point —
  the elements the browser hit, plus every layer `pointer-events: none` hides from hit testing — orders it by
  CSS painting order and takes the topmost. Decoration, overlays and pseudo layers are inspectable, and an
  ancestor is never substituted for the element that was tapped.
- **The stack is offered, not resolved.** When several layers overlap, they are listed topmost first and any
  of them can be picked by hand. Section backgrounds, full-viewport layers and layers that paint nothing
  under the point — a transparent pixel of an image — are listed last instead of first.
- **A hover outline shows the element a tap would pick**, before the tap, so a miss is visible immediately.
- **Multi-selection.** Tap an element to select it — it gets a numbered mark on the page. Tap more elements to
  build a selection. Tap a selected element again to inspect it.
- **ALL AT ONCE.** One button inspects the entire selection together and produces a single AI block for all of
  them, which is how a real layout change is usually described ("these three cards, same spacing").
- **What an inspection shows:** box size and position, the computed style that actually applied (display,
  position, colours, typography, flex/grid, spacing, border, radius, z-index, overflow, transform), every
  attribute, the parent/child/sibling tree with colour-coded DOM depth, the live DOM, and ranked candidate
  source files with excerpts.
- **Copy for AI** produces a compact block — one short line per fact, no fenced code, one excerpt for the best
  candidate and `path:line` for the rest:

```
NEBULA read-only inspection · my-site · index.html
[1] <button.cta>  body>main>section.hero>button.cta
    box 320 × 48 @12,340 · 2 children · depth 7
    css display:flex; color:rgb(255, 255, 255); background:rgb(10, 132, 255); padding:12px 24px; radius:8px
    attr data-action="subscribe" aria-label="Subscribe"
    text "Subscribe"
    html <button class="cta" data-action="subscribe">Subscribe</button>
    src index.html:42 ▸ .cta{padding:12px 24px;background:var(--accent)}
        assets/app.css:118 · assets/app.js:7
```

The inspector paints its outlines and marks on a canvas above the page, so inspecting a site never changes it.

## Previewing archives

- Opens ZIP archives and standalone HTML/HTM files through the system document picker; no storage permission.
- Keeps five recent documents and up to four open projects in switchable tabs.
- Extracts into the app cache instead of loading the archive into RAM; enforces Zip Slip, file-count,
  single-file and decompressed-size limits.
- Serves the site from an isolated HTTPS origin and resolves extensionless links, directory indexes, nested
  build folders, root-absolute paths, case-mismatched resources and SPA routes.
- Blocks external page navigation while allowing HTTPS API, CDN, font and media requests in `NET` mode.
- Emulates a phone (390 × 844), tablet (834 × 1194) or desktop (1280 × 800) viewport with a rotate control and
  a live CSS-size readout, next to the fit-to-screen mode. The page is laid out at the device's real CSS size
  and scaled natively, so taps and element picking stay aligned.
- Keeps a bounded diagnostics log of missing archive files, JavaScript errors, blocked resources and failed
  requests.
- Runs a runtime performance test that names concrete causes — the element behind a layout shift, the
  render-blocking script, the oversized image, the long JavaScript task — and can measure every page of the
  archive in one run, reporting the average and the worst page. A finding opens straight in the element
  inspector or in its source file, and copies to an assistant as one compact block.
- Simulates a slow network (`4G / 4G− / 3G`, Chrome DevTools values) while browsing and while measuring, by
  really holding bytes back. CPU throttling and a GPU breakdown are not possible in an embedded WebView, and
  the app says so rather than faking numbers.
- Compares two open archives: what was added, removed or changed between two builds, with a line diff for
  text files and a compact diff block for an assistant.
- Carries guarded pointer/touch fallbacks and layer repairs for modals, drawers and scroll-reveal animations
  that an Android WebView would otherwise leave broken. Every forced style is recorded and restored.
- Shows real child WebViews for `window.open` and `target="_blank"`, with native `alert`/`confirm`/`prompt`.

## Browsing files

Every archive file opens from a searchable library: formatted source for text and code, visual preview for
images, audio, video and fonts, and an online-backed viewer for GLB/GLTF models. SVG files are inlined into a
scaled stage with fit / 1:1 modes and a light/dark background toggle; a missing `viewBox` is reconstructed from
the intrinsic size, so an icon authored at 16 or 24 units fills the surface instead of hiding in a corner.

## Limits

ZIP 350 MB · standalone HTML 25 MB · extracted content 800 MB · one file 180 MB · 25,000 entries.
Server-side sites (PHP, Node.js, databases, backend APIs) cannot run offline; their built static output can.

## Build

Requirements: JDK 17 and Android SDK 35.

```bash
./gradlew :composeApp:assembleDebug     # composeApp/build/outputs/apk/debug/composeApp-debug.apk
./gradlew :composeApp:desktopRun -DmainClass=app.nebula.archive.DesktopMainKt
./gradlew :composeApp:desktopTest       # shared model, search, scoring, SVG and AI-context tests
```

The included keystore is a reproducible development key for test builds only. A Play Store or public release
needs a private release signing key owned by the publisher.
