#include "ui/flyout.hpp"

#include "platform/win/win_common.hpp"

#include <shellapi.h>

#include <algorithm>

#include "app/actions.hpp"
#include "app/app.hpp"
#include "ui/main_window.hpp"

namespace vl::ui {

namespace {

// Work area of the monitor containing the point, and where the taskbar is.
RECT workArea(POINT pt) {
    MONITORINFO mi{sizeof mi};
    GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi);
    return mi.rcWork;
}

}  // namespace

// --------------------------------------------------------------------- flyout

int Flyout::backdropType() const { return win::kBackdropAcrylic; }

void Flyout::toggle(const RECT& anchor) {
    // A click on the tray icon first deactivates (hides) us; don't immediately reopen.
    if (visible()) {
        hide();
        return;
    }
    if (GetTickCount() - hiddenAt_ < 250) return;
    POINT pt{(anchor.left + anchor.right) / 2, (anchor.top + anchor.bottom) / 2};
    const RECT wa = workArea(pt);
    if (!hwnd_) {
        createWindow(L"VibeHandFlyout", L"Vibe Hand", WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_TOPMOST, 0, 0, 10, 10, nullptr);
    }
    const float s = GetDpiForWindow(hwnd_) / 96.f;
    const int w = static_cast<int>(372 * s), h = static_cast<int>(532 * s), m = static_cast<int>(12 * s);
    // Bottom-right above the taskbar (or next to it if the taskbar is elsewhere).
    int x = std::clamp(static_cast<int>(pt.x - w / 2), static_cast<int>(wa.left + m), static_cast<int>(wa.right - w - m));
    int y = wa.bottom - h - m;
    if (pt.y < (wa.top + wa.bottom) / 2) y = wa.top + m;  // taskbar at the top of the screen
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW);
    SetForegroundWindow(hwnd_);
    app_.setPreviewWanted(true);
    invalidate();
}

void Flyout::hide() {
    if (!hwnd_) return;
    ShowWindow(hwnd_, SW_HIDE);
    hiddenAt_ = GetTickCount();
    app_.setPreviewWanted(false);
}

LRESULT Flyout::extra(UINT msg, WPARAM wp, LPARAM lp, bool& handled) {
    handled = false;
    if (msg == WM_ACTIVATE && LOWORD(wp) == WA_INACTIVE) {
        hide();
        handled = true;
    } else if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
        hide();
        handled = true;
    }
    (void)lp;
    return 0;
}

void Flyout::frame(Gui& g, const RectF& client) {
    Canvas& c = g.canvas();
    const Palette& p = g.pal();
    const auto& s = app_.snapshot();
    Settings& S = app_.settings();
    const float pad = 16;

    if (!backdropActive_) c.fillRect(client, p.overlay);
    // Header.
    c.fillRound({pad, pad, 30, 30}, 8, p.accent);
    c.icon(glyph::Hand, {pad, pad, 30, 30}, p.onAccent, 15.f);
    c.text(L"Vibe Hand", {pad + 40, pad - 1, 200, 20}, p.text, 14.5f, Weight::Semibold);
    const std::wstring mode = app_.paused() ? app_.pauseText() : win::widen(modeTextRu(s.mode));
    c.fillCircle({pad + 45, pad + 25}, 3.5f, modeColor(p, static_cast<int>(app_.paused() ? PipelineMode::Paused : s.mode)));
    c.text(mode, {pad + 54, pad + 16, 240, 18}, p.textDim, 12.f);
    if (g.iconButton("open", {client.w - pad - 36, pad - 2, 36, 34}, glyph::Settings, L"Настройки")) {
        hide();
        app_.showMain();
    }

    // Preview strip.
    const RectF pv{pad, 64, client.w - 2 * pad, (client.w - 2 * pad) * 9.f / 16.f};
    drawPreview(g, app_, pv, true);

    // Live line.
    float y = pv.bottom() + 10;
    std::wstring live;
    if (s.hasHand) {
        live = s.hand.pose.empty() ? L"Рука в кадре" : win::widen(findBuiltin(s.hand.pose) ? findBuiltin(s.hand.pose)->nameRu : s.hand.pose.c_str());
        if (!s.lastDynamic.empty())
            if (const auto* b = findBuiltin(s.lastDynamic)) live += L" · " + win::widen(b->nameRu);
    } else {
        live = s.faces.empty() ? L"Никого не видно" : (s.faces[0].personId.empty() ? L"В кадре незнакомое лицо" : L"Привет, " + win::widen(s.faces[0].name));
    }
    c.text(live, {pad, y, client.w - 2 * pad, 20}, p.text, 13.5f, Weight::Semibold);
    y += 30;

    // Quick tiles (2x2), Windows quick-settings style.
    const float tw = (client.w - 2 * pad - 10) / 2, th = 64;
    struct Tile { const wchar_t* g; std::wstring t; std::wstring sub; bool on; };
    const bool night = S.camera.night != NightMode::Off;
    const bool eco = S.perf.profile == PowerProfile::Eco;
    const bool known = S.faces.gate == FaceGate::KnownOnly;
    Tile tiles[4] = {
        {app_.userPaused() ? glyph::Play : glyph::Pause, L"Распознавание", app_.userPaused() ? L"Выключено" : L"Включено", !app_.userPaused()},
        {glyph::Moon, L"Ночное зрение", night ? (S.camera.night == NightMode::Infrared ? L"ИК" : S.camera.night == NightMode::Auto ? L"Авто" : L"Всегда") : L"Выкл", night},
        {glyph::Leaf, L"Эко-режим", eco ? L"Минимум энергии" : L"Обычный", eco},
        {glyph::Contact, L"Только я", known ? L"Знакомые лица" : L"Любой человек", known},
    };
    for (int i = 0; i < 4; ++i) {
        const RectF r{pad + (i % 2) * (tw + 10), y + (i / 2) * (th + 10), tw, th};
        const bool h = g.hovered(r);
        if (h) g.setCursorHand();
        const Color bg = tiles[i].on ? (h ? p.accentHover : p.accent) : (h ? p.controlHover : p.control);
        c.fillRound(r, 8, bg);
        if (!tiles[i].on) c.strokeRound(r, 8, p.controlStroke);
        const Color fg = tiles[i].on ? p.onAccent : p.text;
        c.icon(tiles[i].g, {r.x + 14, r.y + 10, 18, 22}, fg, 15.f, Align::Start);
        c.text(tiles[i].t, {r.x + 14, r.y + 32, r.w - 20, 16}, fg, 12.5f, Weight::Semibold);
        c.text(tiles[i].sub, {r.x + 14, r.y + 47, r.w - 20, 14}, tiles[i].on ? fg.alpha(0.8f) : p.textDim, 11.f);
        if (h && g.input().released) {
            switch (i) {
                case 0: app_.setUserPaused(!app_.userPaused()); break;
                case 1: S.camera.night = night ? NightMode::Off : NightMode::Auto; app_.settingsChanged(); break;
                case 2: S.perf.profile = eco ? PowerProfile::Balanced : PowerProfile::Eco; app_.settingsChanged(); break;
                case 3:
                    if (app_.faces().people.empty() && !known) { hide(); app_.showMain(PageFaces); }
                    else { S.faces.gate = known ? FaceGate::AnyFace : FaceGate::KnownOnly; app_.settingsChanged(); }
                    break;
            }
        }
    }
    y += 2 * th + 18;

    // Footer.
    c.fillRect({0, client.h - 52, client.w, 52}, p.dark ? Color::hex(0x000000, 0.18f) : Color::hex(0x000000, 0.04f));
    const auto& st = app_.stats();
    wchar_t buf[96];
    swprintf(buf, 96, L"CPU %.1f %% · %.0f МБ · %ls", st.cpuPercent, st.workingSetMb, win::widen(s.device).c_str());
    c.text(buf, {pad, client.h - 52, client.w - 140, 52}, p.textMuted, 11.5f);
    if (g.button("main", {client.w - pad - 112, client.h - 42, 112, 32}, L"Открыть", ButtonKind::Accent)) {
        hide();
        app_.showMain();
    }
}

// --------------------------------------------------------------------- OSD

int Osd::backdropType() const { return win::kBackdropAcrylic; }

void Osd::ensure() {
    if (hwnd_) return;
    createWindow(L"VibeHandOsd", L"", WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
                 0, 0, 10, 10, nullptr);
}

void Osd::place() {
    POINT pt;
    GetCursorPos(&pt);
    // Primary monitor of the foreground window, else of the cursor.
    HMONITOR mon = GetForegroundWindow() ? MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTONEAREST)
                                         : MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof mi};
    GetMonitorInfoW(mon, &mi);
    const float s = GetDpiForWindow(hwnd_) / 96.f;
    const int w = static_cast<int>(320 * s), h = static_cast<int>(64 * s);
    const RECT& wa = mi.rcWork;
    SetWindowPos(hwnd_, HWND_TOPMOST, wa.left + (wa.right - wa.left - w) / 2, wa.bottom - h - static_cast<int>(24 * s), w, h,
                 SWP_SHOWWINDOW | SWP_NOACTIVATE);
}

void Osd::showFired(const std::wstring& glyphStr, const std::wstring& title, const std::wstring& detail, int holdMs) {
    ensure();
    glyph_ = glyphStr;
    title_ = title;
    detail_ = detail;
    fired_ = true;
    progress_ = -1.f;
    shownAt_ = GetTickCount();
    holdMs_ = static_cast<DWORD>(holdMs);
    place();
    SetTimer(hwnd_, 7, holdMs_, nullptr);
    invalidate();
}

void Osd::showProgress(const std::wstring& title, float progress) {
    if (fired_ && GetTickCount() - shownAt_ < holdMs_) return;  // let the confirmation stay
    ensure();
    fired_ = false;
    title_ = title;
    detail_ = L"Удерживайте жест…";
    progress_ = progress;
    if (!IsWindowVisible(hwnd_)) place();
    KillTimer(hwnd_, 7);
    invalidate();
}

void Osd::hideProgress() {
    if (hwnd_ && !fired_ && IsWindowVisible(hwnd_)) ShowWindow(hwnd_, SW_HIDE);
}

LRESULT Osd::extra(UINT msg, WPARAM wp, LPARAM lp, bool& handled) {
    handled = false;
    if (msg == WM_TIMER && wp == 7) {
        KillTimer(hwnd_, 7);
        fired_ = false;
        ShowWindow(hwnd_, SW_HIDE);
        handled = true;
    } else if (msg == WM_MOUSEACTIVATE) {
        handled = true;
        return MA_NOACTIVATE;
    }
    (void)lp;
    return 0;
}

void Osd::frame(Gui& g, const RectF& client) {
    Canvas& c = g.canvas();
    const Palette& p = g.pal();
    if (!backdropActive_) c.fillRect(client, p.overlay);
    const Vec2 ic{client.x + 34, client.h / 2};
    if (fired_) {
        c.fillCircle(ic, 18, p.accent);
        c.icon(glyph_.c_str(), {ic.x - 18, ic.y - 18, 36, 36}, p.onAccent, 15.f);
        // Countdown line along the bottom.
        const float t = std::clamp((GetTickCount() - shownAt_) / 1400.f, 0.f, 1.f);
        c.fillRect({0, client.h - 3, client.w * (1 - t), 3}, p.accent.alpha(0.7f));
        g.anim(1, t, 1.f);  // keep repainting while visible
    } else {
        g.progressRing(ic, 15, std::max(0.f, progress_), p.success, 4.f);
        c.icon(glyph::Hand, {ic.x - 18, ic.y - 18, 36, 36}, p.text, 13.f);
    }
    c.text(title_, {68, 11, client.w - 80, 22}, p.text, 14.f, Weight::Semibold);
    c.text(detail_, {68, 33, client.w - 80, 20}, p.textDim, 12.f);
}

}  // namespace vl::ui
