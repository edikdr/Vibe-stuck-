#include "ui/main_window.hpp"

#include "app/app.hpp"
#include "platform/win/system.hpp"

namespace vl::ui {

void MainWindow::show(int page) {
    if (page >= 0) state_.page = page;
    if (!hwnd_) {
        // ~1120x740 DIP, centred on the monitor with the cursor.
        POINT pt;
        GetCursorPos(&pt);
        HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{sizeof mi};
        GetMonitorInfoW(mon, &mi);
        UINT dpiX = 96, dpiY = 96;
        using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
        if (HMODULE sh = LoadLibraryExW(L"shcore.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32))
            if (auto f = reinterpret_cast<GetDpiForMonitorFn>(reinterpret_cast<void*>(GetProcAddress(sh, "GetDpiForMonitor"))))
                f(mon, 0, &dpiX, &dpiY);
        const float s = dpiX / 96.f;
        const int w = static_cast<int>(1120 * s), h = static_cast<int>(740 * s);
        const RECT& wa = mi.rcWork;
        const int x = wa.left + (wa.right - wa.left - w) / 2, y = wa.top + (wa.bottom - wa.top - h) / 2;
        createWindow(L"VibeHandMain", L"Vibe Hand", WS_OVERLAPPEDWINDOW, 0, x, y, w, h, nullptr);
    }
    ShowWindow(hwnd_, IsIconic(hwnd_) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(hwnd_);
    invalidate();
    app_.setPreviewWanted(true);
}

int MainWindow::backdropType() const { return win::kBackdropMica; }

void MainWindow::onDestroyed() {
    app_.setPreviewWanted(false);
    app_.cancelTasks();
    state_.enrollingPerson.clear();
    state_.recordingGesture.clear();
    win::trimWorkingSet();  // give back the UI's memory while we live in the tray
}

LRESULT MainWindow::extra(UINT msg, WPARAM wp, LPARAM lp, bool& handled) {
    handled = false;
    switch (msg) {
        case WM_GETMINMAXINFO: {
            const float s = GetDpiForWindow(hwnd_) / 96.f;
            auto* mm = reinterpret_cast<MINMAXINFO*>(lp);
            mm->ptMinTrackSize = {static_cast<LONG>(880 * s), static_cast<LONG>(600 * s)};
            handled = true;
            return 0;
        }
        case WM_CLOSE:
            // Closing hides to the tray: the window is destroyed to free its memory.
            DestroyWindow(hwnd_);
            handled = true;
            return 0;
        case WM_SIZE:
            app_.setPreviewWanted(wp != SIZE_MINIMIZED);
            break;
        default:
            break;
    }
    return 0;
}

void MainWindow::frame(Gui& g, const RectF& client) {
    const Palette& p = g.pal();
    Canvas& c = g.canvas();

    // ---- navigation pane
    const float navW = 248.f;
    if (!backdropActive_) c.fillRect({0, 0, navW, client.h}, p.dark ? Color::hex(0x0A0C0F) : Color::hex(0xEDEFF3));
    // Brand.
    c.fillRound({18, 18, 34, 34}, 9, p.accent);
    c.icon(glyph::Hand, {18, 18, 34, 34}, p.onAccent, 17.f);
    c.text(L"Vibe Hand", {62, 16, 180, 22}, p.text, 16.f, Weight::Semibold);
    c.text(L"Жесты, голос, лица · Legion 7", {62, 37, 180, 16}, p.textMuted, 12.f);

    struct Nav { const wchar_t* g; const wchar_t* t; };
    static const Nav items[kPageCount] = {
        {glyph::Home, L"Обзор"},          {glyph::Hand, L"Жесты"},
        {glyph::Mic, L"Голос"},
        {glyph::People, L"Лица"},          {glyph::Camera, L"Камера и ночь"},
        {glyph::Leaf, L"Питание"},         {glyph::Settings, L"Система"},
    };
    float y = 76;
    for (int i = 0; i < kPageCount; ++i) {
        if (g.navItem("nav" + std::to_string(i), {8, y, navW - 16, 40}, items[i].g, items[i].t, state_.page == i))
            state_.page = i;
        y += 44;
    }

    // Status card at the bottom of the pane.
    const PipelineSnapshot& s = app_.snapshot();
    const RectF sc{12, client.h - 96, navW - 24, 82};
    g.card(sc, 8);
    const Color mc = modeColor(p, static_cast<int>(app_.paused() ? PipelineMode::Paused : s.mode));
    c.fillCircle({sc.x + 18, sc.y + 22}, 5, mc);
    c.text(app_.paused() ? L"Пауза" : win::widen(modeTextRu(s.mode)), {sc.x + 32, sc.y + 10, sc.w - 80, 24}, p.text,
           13.5f, Weight::Semibold);
    bool on = !app_.userPaused();
    if (g.toggle("navpause", {sc.right() - 54, sc.y + 8, 46, 28}, on, false)) app_.setUserPaused(!on);
    std::wstring sub = app_.paused() ? app_.pauseText() : (s.fps > 0 ? std::to_wstring(static_cast<int>(s.fps + 0.5f)) + L" к/с · " : L"") + win::widen(s.device);
    c.text(sub, {sc.x + 14, sc.y + 38, sc.w - 28, 34}, p.textMuted, 12.f, Weight::Regular, Align::Start, Align::Start, true);

    // ---- page
    const RectF area{navW, 0, client.w - navW, client.h};
    if (backdropActive_) c.fillRound({navW, 0, client.w - navW + 12, client.h + 12}, 8, p.dark ? Color::hex(0x000000, 0.22f) : Color::hex(0xFFFFFF, 0.35f));
    switch (state_.page) {
        case PageOverview: drawOverview(g, app_, state_, area); break;
        case PageGestures: drawGestures(g, app_, state_, area); break;
        case PageVoice: drawVoice(g, app_, state_, area); break;
        case PageFaces: drawFaces(g, app_, state_, area); break;
        case PageCamera: drawCamera(g, app_, state_, area); break;
        case PagePower: drawPower(g, app_, state_, area); break;
        default: drawSystem(g, app_, state_, area); break;
    }
}

}  // namespace vl::ui
