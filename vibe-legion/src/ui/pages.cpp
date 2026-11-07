// Page content of the main window. Immediate-mode: every widget edits the
// live Settings directly and calls App::settingsChanged(), which debounces the
// save and pushes the new configuration to the camera and pipeline.
#include "platform/win/win_common.hpp"

#include <commdlg.h>

#include <algorithm>
#include <cmath>
#include <ctime>

#include "app/actions.hpp"
#include "app/app.hpp"
#include "platform/win/system.hpp"
#include "ui/main_window.hpp"

namespace vl::ui {

using win::widen;
using win::narrow;

namespace {

double nowSec() { return GetTickCount64() / 1000.0; }

std::wstring fmtInt(float v, const wchar_t* unit) { return std::to_wstring(static_cast<int>(std::lround(v))) + unit; }
std::wstring fmtMs(float v) {
    if (v >= 1000.f) {
        wchar_t b[32];
        swprintf(b, 32, L"%.1f с", v / 1000.f);
        return b;
    }
    return fmtInt(v, L" мс");
}
std::wstring fmt1(double v, const wchar_t* unit) {
    wchar_t b[48];
    swprintf(b, 48, L"%.1f%ls", v, unit);
    return b;
}

std::unordered_map<std::string, float> g_contentHeight;

struct PageScope {
    Gui& g;
    std::string key;
    Column col;
    RectF view;
    PageScope(Gui& gg, const std::string& k, const RectF& area, const std::wstring& title, const std::wstring& subtitle)
        : g(gg), key(k) {
        const float pad = 32.f;
        const float w = std::min(area.w - 2 * pad, 920.f);
        view = {area.x, area.y, area.w, area.h};
        g.beginScroll(key, view, g_contentHeight[key]);
        col = Column{area.x + pad, area.y + 26, w, 8};
        g.label(title, col.next(36), 26.f, nullptr, Weight::Semibold);
        if (!subtitle.empty()) {
            const Color& m = g.pal().textDim;
            g.label(subtitle, col.next(20), 13.5f, &m);
        }
        col.space(10);
    }
    ~PageScope() {
        g_contentHeight[key] = col.y - view.y + 28;
        g.endScroll();
    }
};

void section(Gui& g, Column& col, const std::wstring& text) {
    col.space(10);
    g.label(text, col.next(22), 14.f, nullptr, Weight::Semibold);
}

// Windows-11-settings-style row: icon, title, description, control on the right.
RectF settingRow(Gui& g, Column& col, const wchar_t* glyphStr, const std::wstring& title, const std::wstring& desc,
                 float controlW = 300.f, float h = 0.f) {
    Canvas& c = g.canvas();
    const Palette& p = g.pal();
    const float textW = col.w - controlW - 90;
    const float descH = desc.empty() ? 0.f : c.measure(desc, 12.5f, Weight::Regular, textW, true).y;
    if (h <= 0) h = std::max(64.f, 30.f + descH + 16.f);
    const RectF r = col.next(h);
    g.card(r, 7);
    if (glyphStr) c.icon(glyphStr, {r.x + 18, r.y, 22, r.h}, p.text, 17.f);
    const float tx = r.x + (glyphStr ? 56 : 20);
    const float ty = r.y + (r.h - (20 + (desc.empty() ? 0 : descH + 2))) * 0.5f;
    c.text(title, {tx, ty, textW, 20}, p.text, 14.f);
    if (!desc.empty()) c.text(desc, {tx, ty + 21, textW, descH + 2}, p.textDim, 12.5f, Weight::Regular, Align::Start, Align::Start, true);
    return {r.right() - controlW - 16, r.y + (r.h - 34) * 0.5f, controlW, 34};
}

std::wstring gestureName(const App& app, const std::string& id) {
    if (const auto* b = findBuiltin(id)) return widen(b->nameRu);
    if (id.rfind("custom:", 0) == 0)
        for (const auto& cg : const_cast<App&>(app).settings().customGestures)
            if (cg.id == id.substr(7)) return widen(cg.name);
    return L"—";
}

const std::vector<ActionType>& actionOrder() {
    // Own actions first (app, keys), then windows, media, system.
    static const std::vector<ActionType> v = {
        ActionType::LaunchApp, ActionType::KeyCombo,
        ActionType::NextWindow, ActionType::PrevWindow, ActionType::SwitchWindow, ActionType::TaskView,
        ActionType::MinimizeWindow, ActionType::MaximizeWindow, ActionType::CloseWindow, ActionType::ShowDesktop,
        ActionType::NextDesktop, ActionType::PrevDesktop,
        ActionType::MediaPlayPause, ActionType::MediaNext, ActionType::MediaPrev, ActionType::MediaStop,
        ActionType::VolumeUp, ActionType::VolumeDown, ActionType::VolumeMute, ActionType::MicMute,
        ActionType::ScrollUp, ActionType::ScrollDown, ActionType::BrowserBack, ActionType::BrowserForward,
        ActionType::Screenshot, ActionType::ScreenshotFull, ActionType::StartMenu, ActionType::Search,
        ActionType::FileExplorer, ActionType::LockPC, ActionType::PauseRecognition, ActionType::None};
    return v;
}

std::wstring handName(bool right) { return right ? L"правая" : L"левая"; }

std::wstring browseExe(HWND owner) {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn{sizeof ofn};
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Программы\0*.exe;*.lnk;*.bat;*.cmd\0Все файлы\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&ofn) ? std::wstring(file) : std::wstring();
}

bool confirmButton(Gui& g, UiState& st, const std::string& key, const RectF& r, const std::wstring& text) {
    const bool armed = st.confirmDelete == key && nowSec() < st.confirmUntil;
    if (g.button("del" + key, r, armed ? L"Точно удалить?" : text, ButtonKind::Danger, glyph::Delete)) {
        if (armed) {
            st.confirmDelete.clear();
            return true;
        }
        st.confirmDelete = key;
        st.confirmUntil = nowSec() + 3.0;
    }
    return false;
}

}  // namespace

Color modeColor(const Palette& p, int mode) {
    switch (static_cast<PipelineMode>(mode)) {
        case PipelineMode::Active: return p.success;
        case PipelineMode::Idle: return p.info;
        case PipelineMode::Sleep: return p.night;
        case PipelineMode::Paused: return p.textMuted;
        case PipelineMode::Starting: return p.info;
        default: return p.danger;
    }
}

// --------------------------------------------------------------------- preview

void drawPreview(Gui& g, App& app, const RectF& r, bool overlays) {
    Canvas& c = g.canvas();
    const Palette& p = g.pal();
    const auto& s = app.snapshot();
    static std::unordered_map<const Canvas*, uint64_t> seen;
    c.fillRound(r, 8, Color::hex(0x050607));
    if (!s.previewBgra.empty() && seen[&c] != s.previewSeq) {
        c.updateBitmap(s.previewBgra.data(), s.previewW, s.previewH);
        seen[&c] = s.previewSeq;
    }
    if (!c.hasBitmap() || s.previewW == 0 || app.paused()) {
        c.icon(app.paused() ? glyph::Pause : glyph::Camera, {r.x, r.y + r.h / 2 - 30, r.w, 30}, p.textMuted, 26.f);
        const std::wstring t = app.paused() ? app.pauseText() : (s.mode == PipelineMode::ModelError ? widen(s.reason) : widen(s.camera.empty() ? "Ожидание камеры…" : s.camera));
        c.text(t, {r.x + 20, r.y + r.h / 2 + 6, r.w - 40, 40}, p.textMuted, 12.5f, Weight::Regular, Align::Center, Align::Start, true);
        return;
    }
    const float ar = static_cast<float>(s.previewW) / s.previewH;
    RectF d = r;
    if (r.w / r.h > ar) { d.w = r.h * ar; d.x = r.x + (r.w - d.w) / 2; }
    else { d.h = r.w / ar; d.y = r.y + (r.h - d.h) / 2; }
    const bool flip = !app.settings().camera.mirrorPreview;  // frames arrive mirrored
    c.pushRoundClip(r, 8);
    c.drawBitmap(d, 1.f, flip);
    if (overlays && s.frameW > 0) {
        const float sx = d.w / s.frameW, sy = d.h / s.frameH;
        auto P = [&](float x, float y) -> Vec2 { return {flip ? d.right() - x * sx : d.x + x * sx, d.y + y * sy}; };
        for (const auto& f : s.faces) {
            const Vec2 a = P(f.det.box.x, f.det.box.y), b = P(f.det.box.right(), f.det.box.bottom());
            const RectF fr{std::min(a.x, b.x), a.y, std::fabs(b.x - a.x), b.y - a.y};
            const Color col = f.personId.empty() ? p.info : p.success;
            c.strokeRound(fr, 6, col, 2.f);
            std::wstring name = f.personId.empty() ? L"Лицо" : widen(f.name);
            if (!f.looking) name += L" · не смотрит";
            const float tw = c.measure(name, 12.f, Weight::Semibold).x + 16;
            c.fillRound({fr.x, fr.y - 22, tw, 20}, 5, col);
            c.text(name, {fr.x + 8, fr.y - 22, tw - 8, 20}, Color::hex(0x0B0D10), 12.f, Weight::Semibold);
        }
        static const int bones[][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {0, 5}, {5, 6}, {6, 7}, {7, 8},
                                       {5, 9}, {9, 10}, {10, 11}, {11, 12}, {9, 13}, {13, 14}, {14, 15},
                                       {15, 16}, {13, 17}, {17, 18}, {18, 19}, {19, 20}, {0, 17}};
        auto drawHand = [&](const HandObservation& h, const Color& boneCol) {
            const auto& lm = h.lm.image;
            for (const auto& b : bones) c.line(P(lm[b[0]].x, lm[b[0]].y), P(lm[b[1]].x, lm[b[1]].y), boneCol, 2.5f);
            for (int i = 0; i < kHandLandmarks; ++i) c.fillCircle(P(lm[i].x, lm[i].y), i % 4 == 0 ? 4.f : 3.f, Color::hex(0xFFFFFF));
            // Label at the wrist.
            const std::wstring lbl = (h.pose.empty() ? L"рука" : gestureName(app, h.pose)) + L" · " + handName(h.isRight);
            const Vec2 w = P(lm[0].x, lm[0].y);
            const float tw = c.measure(lbl, 12.f, Weight::Semibold).x + 16;
            c.fillRound({w.x - tw / 2, w.y + 10, tw, 22}, 6, Color::hex(0x000000, 0.65f));
            c.text(lbl, {w.x - tw / 2, w.y + 10, tw, 22}, Color::hex(0xFFFFFF), 12.f, Weight::Semibold, Align::Center);
        };
        if (s.hasSecondHand) drawHand(s.hand2, p.info);
        if (s.hasHand) {
            drawHand(s.hand, p.accent);
            // Hold progress of the rule that is charging.
            float prog = 0;
            for (const auto& [id, v] : s.ruleProgress) prog = std::max(prog, v);
            const auto& lm = s.hand.lm.image;
            if (prog > 0.02f && prog < 1.f) g.progressRing(P(lm[9].x, lm[9].y), 18, prog, p.success, 4.f);
        }
        // Last movement, top right of the preview.
        if (!s.lastDynamic.empty()) {
            const std::wstring t = gestureName(app, s.lastDynamic);
            const float tw = c.measure(t, 12.f, Weight::Semibold).x + 20;
            c.fillRound({r.right() - tw - 10, r.y + 10, tw, 24}, 12, p.success);
            c.text(t, {r.right() - tw - 10, r.y + 10, tw, 24}, Color::hex(0x0B0D10), 12.f, Weight::Semibold, Align::Center);
        }
    }
    // Night / IR badge.
    if (s.nightActive) {
        const std::wstring t = s.infrared ? (s.irAlternating ? L"ИК · без засветки" : L"ИК-камера") : L"Ночное усиление";
        const float tw = c.measure(t, 11.5f, Weight::Semibold).x + 30;
        c.fillRound({r.x + 10, r.y + 10, tw, 22}, 11, Color::hex(0x000000, 0.6f));
        c.icon(glyph::Moon, {r.x + 16, r.y + 10, 14, 22}, p.night, 11.f);
        c.text(t, {r.x + 34, r.y + 10, tw - 30, 22}, Color::hex(0xFFFFFF), 11.5f, Weight::Semibold);
    }
    c.popRoundClip();
}

// --------------------------------------------------------------------- overview

void drawOverview(Gui& g, App& app, UiState& st, const RectF& area) {
    PageScope ps(g, "overview", area, L"Обзор", L"Что видит камера прямо сейчас и как работает распознавание");
    Column& col = ps.col;
    Canvas& c = g.canvas();
    const Palette& p = g.pal();
    const auto& s = app.snapshot();

    if (s.mode == PipelineMode::ModelError) {
        const RectF r = col.next(84);
        g.card(r, 8);
        c.icon(glyph::Warning, {r.x + 16, r.y, 24, r.h}, p.warning, 20.f);
        c.text(L"Не найдены модели ИИ", {r.x + 54, r.y + 14, r.w - 70, 22}, p.text, 14.f, Weight::Semibold);
        c.text(L"Запустите scripts\\fetch-deps.ps1 или положите .onnx-файлы в папку models рядом с программой. " + widen(s.reason),
               {r.x + 54, r.y + 36, r.w - 70, 40}, p.textDim, 12.5f, Weight::Regular, Align::Start, Align::Start, true);
    }

    // Hero: preview + live status.
    const float heroH = 330;
    const RectF hero = col.next(heroH);
    g.card(hero, 10);
    const float pvW = std::min(hero.w * 0.56f, (heroH - 24) * 16.f / 9.f);
    drawPreview(g, app, {hero.x + 12, hero.y + 12, pvW, heroH - 24}, true);
    const float rx = hero.x + pvW + 32, rw = hero.right() - rx - 20;
    float y = hero.y + 20;
    const int mode = static_cast<int>(app.paused() ? PipelineMode::Paused : s.mode);
    const std::wstring modeText = app.paused() ? L"Пауза" : widen(modeTextRu(s.mode));
    g.pill({rx, y, c.measure(modeText, 12.f, Weight::Semibold).x + 28, 24}, modeText, modeColor(p, mode));
    bool on = !app.userPaused();
    if (g.toggle("ov_on", {hero.right() - 120, y - 4, 104, 32}, on)) app.setUserPaused(!on);
    y += 40;

    auto line = [&](const wchar_t* gl, const std::wstring& title, const std::wstring& value, const Color& vc) {
        c.icon(gl, {rx, y, 20, 40}, p.textDim, 15.f);
        c.text(title, {rx + 30, y, rw - 30, 18}, p.textMuted, 12.f);
        c.text(value, {rx + 30, y + 17, rw - 30, 22}, vc, 15.f, Weight::Semibold);
        y += 50;
    };
    if (s.hasHand) {
        auto describe = [&](const HandObservation& h) {
            std::wstring v = handName(h.isRight) + L": " + (h.pose.empty() ? L"поза не распознана" : gestureName(app, h.pose));
            if (!h.customId.empty()) v += L" · свой «" + gestureName(app, "custom:" + h.customId) + L"»";
            return v;
        };
        std::wstring v = describe(s.hand);
        if (s.hasSecondHand) v += L"; " + describe(s.hand2);
        if (!s.twoHandPose.empty()) v = gestureName(app, s.twoHandPose) + L" (" + v + L")";
        line(glyph::Hand, s.hasSecondHand ? L"Руки" : L"Рука", v, p.text);
    } else {
        line(glyph::Hand, L"Рука", s.lastDynamic.empty() ? L"не видно" : gestureName(app, s.lastDynamic), p.textDim);
    }
    if (!s.faces.empty()) {
        std::wstring v;
        for (size_t i = 0; i < s.faces.size(); ++i) {
            const auto& f = s.faces[i];
            if (i) v += L", ";
            v += f.personId.empty() ? L"незнакомое лицо" : widen(f.name);
        }
        line(glyph::Contact, L"Лица", v, p.text);
    } else {
        line(glyph::Contact, L"Лица", L"нет в кадре", p.textDim);
    }
    const bool anyRule = std::any_of(app.settings().rules.begin(), app.settings().rules.end(), [](const GestureRule& r) { return r.enabled; });
    line(glyph::Info, L"Правило", anyRule ? (s.whyNot.empty() ? L"—" : widen(s.whyNot)) : L"нет включённых правил", p.text);
    line(glyph::Swap, L"Движение", s.lastDynamic.empty() ? L"взмахов нет" : gestureName(app, s.lastDynamic),
         s.lastDynamic.empty() ? p.textDim : p.success);

    // Tiles.
    col.space(4);
    const RectF tiles = col.next(92);
    const float tw = (tiles.w - 3 * 10) / 4;
    struct Tile { const wchar_t* g; std::wstring t; std::wstring v; std::wstring sub; };
    const auto& ps2 = app.stats();
    const Tile tl[4] = {
        {glyph::Speed, L"Кадров в секунду", s.fps > 0 ? fmt1(s.fps, L"") : L"—", fmt1(s.msFrame, L" мс на кадр")},
        {glyph::Chip, L"Процессор", fmt1(ps2.cpuPercent, L" %"), L"всего ядер системы"},
        {glyph::Memory, L"Память", fmt1(ps2.workingSetMb, L" МБ"), L"рабочий набор"},
        {glyph::Rocket, L"Вычисления", s.device.empty() ? L"—" : widen(s.device), s.nightActive ? L"ночной режим" : L"дневной режим"},
    };
    for (int i = 0; i < 4; ++i) {
        const RectF r{tiles.x + i * (tw + 10), tiles.y, tw, tiles.h};
        g.card(r, 8);
        c.icon(tl[i].g, {r.x + 14, r.y + 12, 18, 18}, p.accent, 14.f, Align::Start);
        c.text(tl[i].t, {r.x + 38, r.y + 11, r.w - 44, 20}, p.textDim, 12.f);
        c.text(tl[i].v, {r.x + 14, r.y + 34, r.w - 24, 30}, p.text, 21.f, Weight::Semibold);
        c.text(tl[i].sub, {r.x + 14, r.y + 64, r.w - 24, 18}, p.textMuted, 11.5f);
    }

    // Empty state.
    if (!anyRule) {
        const RectF r = settingRow(g, col, glyph::Hand, L"Жесты ещё не настроены",
                                   L"Добавьте правило: жест, дистанция, кто может его подавать и что сделать. "
                                   L"Пример «ладонь на 60–70 см → пауза» уже лежит выключенным.", 180);
        if (g.button("goGest", r, L"Открыть жесты", ButtonKind::Accent)) st.page = PageGestures;
    }

    section(g, col, L"Последние срабатывания");
    if (app.log().empty()) {
        const Color& m = p.textMuted;
        g.label(L"Пока ничего не срабатывало.", col.next(24), 13.f, &m);
    }
    for (const auto& e : app.log()) {
        const RectF r = col.next(50);
        g.card(r, 7);
        c.icon(e.glyph.c_str(), {r.x + 14, r.y, 22, r.h}, p.accent, 15.f);
        c.text(e.title, {r.x + 50, r.y + 7, r.w - 180, 18}, p.text, 13.5f, Weight::Semibold);
        c.text(e.detail, {r.x + 50, r.y + 25, r.w - 180, 18}, p.textDim, 12.f);
        const time_t t = static_cast<time_t>(e.unixMs / 1000);
        tm lt{};
        localtime_s(&lt, &t);
        wchar_t b[16];
        wcsftime(b, 16, L"%H:%M:%S", &lt);
        c.text(b, {r.right() - 110, r.y, 96, r.h}, p.textMuted, 12.f, Weight::Regular, Align::End);
    }
}

// --------------------------------------------------------------------- gestures

namespace {

void ruleEditor(Gui& g, App& app, UiState& st, Column& col, GestureRule& r, size_t index, bool& removed) {
    Settings& S = app.settings();
    bool ch = false;

    auto& nameBuf = st.text["rule:" + r.id];
    if (nameBuf.empty() && !r.name.empty()) nameBuf = widen(r.name);
    RectF rr = settingRow(g, col, glyph::Edit, L"Название", L"Как правило будет подписано в журнале и подсказках");
    if (g.textField("name", rr, nameBuf, L"Например: Пауза ладонью", 48)) { r.name = narrow(nameBuf); ch = true; }

    // Gesture.
    // Own gestures first, then poses, movements and two-hand gestures.
    std::vector<std::wstring> gnames;
    std::vector<std::string> gids;
    for (const auto& cg : S.customGestures) {
        gnames.push_back(L"Свой: " + widen(cg.name) + (!cg.recorded() ? L" (не записан)" : cg.legacy() ? L" (старая запись)" : L""));
        gids.push_back("custom:" + cg.id);
    }
    for (const auto& b : builtinGestures()) {
        if (!S.gestureEnabled(b.id) && r.gesture != b.id) continue;  // switched-off built-ins are hidden
        gnames.push_back(widen(b.nameRu) + (S.gestureEnabled(b.id) ? L"" : L" (отключён)"));
        gids.push_back(b.id);
    }
    int gsel = static_cast<int>(std::find(gids.begin(), gids.end(), r.gesture) - gids.begin());
    if (gsel >= static_cast<int>(gids.size())) gsel = 0;
    rr = settingRow(g, col, glyph::Hand, L"Жест", L"Свои жесты, позы, движения одной рукой и жесты двумя руками");
    if (g.combo("gesture", rr, gsel, gnames)) { r.gesture = gids[gsel]; ch = true; }

    const auto* builtin = findBuiltin(r.gesture);
    const bool motion = builtin && builtin->dynamic;
    const bool twoHands = builtin && builtin->twoHands;
    if (!twoHands) {
        int hand = static_cast<int>(r.hand);
        rr = settingRow(g, col, glyph::Swap, L"Рука", L"");
        if (g.segmented("hand", rr, hand, {L"Любая", L"Левая", L"Правая"})) { r.hand = static_cast<HandSide>(hand); ch = true; }
    }

    if (motion) {
        const Color& m = g.pal().textDim;
        const std::string& id = r.gesture;
        const wchar_t* how = id == "wave" ? L"Помашите открытой ладонью из стороны в сторону 2–3 раза (≈1 секунда)."
                             : id == "two_spread" ? L"Поднимите обе ладони рядом и быстро разведите их в стороны."
                             : id == "two_pinch" ? L"Поднимите обе ладони по сторонам и быстро сведите их вместе."
                                                 : L"Взмах срабатывает сразу: быстро проведите открытой ладонью на 20–30 см в нужную сторону.";
        g.label(how, col.next(22), 12.5f, &m);
    } else {
        if (twoHands) {
            const Color& m = g.pal().textDim;
            g.label(L"Нужны обе руки в кадре, каждая в этой позе.", col.next(22), 12.5f, &m);
        }
        float hold = static_cast<float>(r.holdMs);
        rr = settingRow(g, col, glyph::Record, L"Удержание", L"Сколько держать жест до срабатывания (защита от случайностей)");
        if (g.slider("hold", rr, hold, 0, 2000, 20, fmtMs)) { r.holdMs = static_cast<int>(hold); ch = true; }
    }

    float cd = static_cast<float>(r.cooldownMs);
    rr = settingRow(g, col, glyph::Refresh, L"Пауза между срабатываниями", L"Кулдаун после выполнения действия");
    if (g.slider("cooldown", rr, cd, 0, 10000, 100, fmtMs)) { r.cooldownMs = static_cast<int>(cd); ch = true; }

    if (!motion) {
        rr = settingRow(g, col, glyph::Play, L"Повторять, пока жест удерживается", L"Удобно для громкости и прокрутки");
        if (g.toggle("repeat", rr, r.repeat)) ch = true;
    }
    if (r.repeat && !motion) {
        float rp = static_cast<float>(r.repeatMs);
        rr = settingRow(g, col, nullptr, L"Интервал повтора", L"");
        if (g.slider("repeatms", rr, rp, 50, 2000, 10, fmtMs)) { r.repeatMs = static_cast<int>(rp); ch = true; }
    }

    // Who.
    std::vector<std::wstring> who = {L"Любой человек", L"Любое знакомое лицо"};
    for (const auto& pp : app.faces().people) who.push_back(L"Только " + widen(pp.name));
    int wsel = r.person == PersonFilter::Anyone ? 0 : r.person == PersonFilter::AnyKnown ? 1 : 0;
    if (r.person == PersonFilter::Specific)
        for (size_t i = 0; i < app.faces().people.size(); ++i)
            if (app.faces().people[i].id == r.personId) wsel = static_cast<int>(i) + 2;
    rr = settingRow(g, col, glyph::People, L"Кто может подавать жест", L"Нужны добавленные лица (раздел «Лица»)");
    if (g.combo("who", rr, wsel, who)) {
        if (wsel == 0) r.person = PersonFilter::Anyone;
        else if (wsel == 1) r.person = PersonFilter::AnyKnown;
        else { r.person = PersonFilter::Specific; r.personId = app.faces().people[wsel - 2].id; }
        ch = true;
    }
    rr = settingRow(g, col, glyph::Contact, L"Только когда в кадре есть лицо",
                    L"Отсекает срабатывания, когда вы отошли от компьютера. Выключено — лицо не нужно");
    if (g.toggle("face", rr, r.requireFace)) ch = true;
    rr = settingRow(g, col, glyph::Eye, L"Только когда смотрю на экран", L"Голова повёрнута к камере");
    if (g.toggle("look", rr, r.requireLooking)) ch = true;

    // Action.
    const auto& ao = actionOrder();
    std::vector<std::wstring> anames;
    for (auto t : ao) anames.push_back(actionTitleRu(t));
    int asel = static_cast<int>(std::find(ao.begin(), ao.end(), r.action.type) - ao.begin());
    if (asel >= static_cast<int>(ao.size())) asel = 0;
    rr = settingRow(g, col, actionGlyph(r.action.type).c_str(), L"Действие", actionDescriptionRu(r.action.type), 360);
    if (g.combo("action", rr, asel, anames)) { r.action.type = ao[asel]; ch = true; }
    if (r.action.type == ActionType::KeyCombo) {
        rr = settingRow(g, col, glyph::Keyboard, L"Сочетание клавиш", L"Щёлкните и нажмите нужные клавиши");
        if (g.keysField("keys", rr, r.action.keys)) ch = true;
    }
    if (r.action.type == ActionType::LaunchApp) {
        // Start menu ("All apps"): desktop programs and Store apps, loaded once.
        static std::vector<InstalledApp> apps;
        static bool appsLoaded = false;
        if (!appsLoaded) {
            apps = installedApps();
            appsLoaded = true;
        }
        std::vector<std::wstring> names = {L"— выберите —"};
        int asel2 = 0;
        for (size_t i = 0; i < apps.size(); ++i) {
            names.push_back(apps[i].name);
            if (_wcsicmp(apps[i].path.c_str(), widen(r.action.path).c_str()) == 0) asel2 = static_cast<int>(i) + 1;
        }
        auto& pathBuf = st.text["path:" + r.id];
        rr = settingRow(g, col, glyph::Apps, L"Приложение из меню «Пуск»",
                        apps.empty() ? L"Список не загрузился — укажите файл ниже" : std::to_wstring(apps.size()) + L" приложений, включая Microsoft Store", 420);
        if (g.combo("app", {rr.x, rr.y, rr.w - 44, rr.h}, asel2, names) && asel2 > 0) {
            r.action.path = narrow(apps[asel2 - 1].path);
            r.action.label = narrow(apps[asel2 - 1].name);
            pathBuf.clear();
            ch = true;
        }
        if (g.iconButton("appsrefresh", {rr.right() - 36, rr.y, 36, rr.h}, glyph::Refresh, L"Обновить список")) appsLoaded = false;

        const bool fromList = r.action.path.rfind("shell:AppsFolder\\", 0) == 0;
        if (pathBuf.empty() && !r.action.path.empty() && !fromList) pathBuf = widen(r.action.path);
        rr = settingRow(g, col, glyph::Open, L"…или любой файл, программа, папка, ссылка", L"", 420);
        if (g.textField("path", {rr.x, rr.y, rr.w - 96, rr.h}, pathBuf, L"C:\\Path\\app.exe или https://…", 260)) {
            r.action.path = narrow(pathBuf);
            r.action.label.clear();
            ch = true;
        }
        if (g.button("browse", {rr.right() - 88, rr.y, 88, rr.h}, L"Обзор…")) {
            const auto f = browseExe(GetActiveWindow());
            if (!f.empty()) { pathBuf = f; r.action.path = narrow(f); r.action.label.clear(); ch = true; }
        }
    }

    const RectF btns = col.next(40);
    if (g.button("test", {btns.x, btns.y, 190, 34}, L"Проверить действие", ButtonKind::Standard, glyph::Play)) app.testAction(r.action);
    if (confirmButton(g, st, "rule" + r.id, {btns.right() - 190, btns.y, 190, 34}, L"Удалить правило")) {
        S.rules.erase(S.rules.begin() + static_cast<long>(index));
        removed = true;
        ch = true;
    }
    col.space(8);
    if (ch) app.settingsChanged();
}

}  // namespace

void drawGestures(Gui& g, App& app, UiState& st, const RectF& area) {
    PageScope ps(g, "gestures", area, L"Жесты", L"Правила «жест → действие». Всё настраивается здесь, без файлов");
    Column& col = ps.col;
    Canvas& c = g.canvas();
    const Palette& p = g.pal();
    Settings& S = app.settings();
    const auto& s = app.snapshot();

    // Live readout.
    {
        const RectF r = col.next(64);
        g.card(r, 8);
        c.icon(glyph::Eye, {r.x + 16, r.y, 22, r.h}, p.accent, 17.f);
        std::wstring now;
        if (s.hasHand) {
            auto one = [&](const HandObservation& h) {
                std::wstring v = handName(h.isRight) + L": " + (h.pose.empty() ? L"поза не распознана" : gestureName(app, h.pose));
                if (!h.customId.empty()) v += L" · свой «" + gestureName(app, "custom:" + h.customId) + L"»";
                return v;
            };
            now = one(s.hand);
            if (s.hasSecondHand) now += L"; " + one(s.hand2);
            if (!s.twoHandPose.empty()) now = gestureName(app, s.twoHandPose) + L" · " + now;
            if (!s.lastDynamic.empty()) now += L" · " + gestureName(app, s.lastDynamic);
        } else {
            now = s.lastDynamic.empty() ? L"рука не видна" : gestureName(app, s.lastDynamic);
        }
        c.text(L"Сейчас вижу", {r.x + 54, r.y + 11, 200, 18}, p.textMuted, 12.f);
        c.text(now, {r.x + 54, r.y + 29, r.w - 300, 22}, p.text, 14.5f, Weight::Semibold);
        c.text(s.whyNot.empty() ? L"" : L"Первое правило: " + widen(s.whyNot), {r.right() - 300, r.y, 284, r.h}, p.textDim, 12.5f,
               Weight::Regular, Align::End);
    }

    section(g, col, L"Правила");
    for (size_t i = 0; i < S.rules.size(); ++i) {
        GestureRule& r = S.rules[i];
        g.pushId(r.id);
        const bool open = st.expandedRule == r.id;
        const RectF row = col.next(68);
        g.card(row, 8, true);
        // Toggle on the left.
        if (g.toggle("en", {row.x + 12, row.y, 44, row.h}, r.enabled, false)) app.settingsChanged();
        const std::wstring title = r.name.empty() ? gestureName(app, r.gesture) + L" → " + actionTitleRu(r.action.type) : widen(r.name);
        std::wstring sub = gestureName(app, r.gesture);
        const auto* bg = findBuiltin(r.gesture);
        if (r.holdMs > 0 && !(bg && bg->dynamic)) sub += L" · " + fmtMs(static_cast<float>(r.holdMs));
        if (r.requireFace) sub += L" · с лицом";
        if (r.person == PersonFilter::AnyKnown) sub += L" · знакомые";
        if (r.person == PersonFilter::Specific) sub += L" · один человек";
        sub += L" → " + describeAction(r.action);
        c.icon(actionGlyph(r.action.type).c_str(), {row.x + 66, row.y, 22, row.h}, r.enabled ? p.accent : p.textMuted, 16.f);
        c.text(title, {row.x + 100, row.y + 13, row.w - 220, 20}, r.enabled ? p.text : p.textDim, 14.f, Weight::Semibold);
        c.text(sub, {row.x + 100, row.y + 34, row.w - 220, 20}, p.textDim, 12.5f);
        for (const auto& [id, v] : s.ruleProgress)
            if (id == r.id && v > 0.f) g.progressRing({row.right() - 80, row.y + row.h / 2}, 11, v, p.success, 3.f);
        if (g.iconButton("exp", {row.right() - 48, row.y + 16, 36, 36}, open ? glyph::ChevronUp : glyph::ChevronDown, L"Настроить"))
            st.expandedRule = open ? "" : r.id;
        bool removed = false;
        if (open) {
            Column inner{col.x + 24, col.y, col.w - 24, 6};
            ruleEditor(g, app, st, inner, r, i, removed);
            col.y = inner.y + 4;
        }
        g.popId();
        if (removed) break;
    }
    {
        const RectF r = col.next(40);
        if (g.button("addrule", {r.x, r.y, 200, 36}, L"Новое правило", ButtonKind::Accent, glyph::Add)) {
            GestureRule nr;
            nr.id = newId();
            nr.gesture = "open_palm";
            nr.holdMs = 500;
            nr.action.type = ActionType::MediaPlayPause;
            S.rules.push_back(nr);
            st.expandedRule = nr.id;
            app.settingsChanged();
        }
    }

    section(g, col, L"Свои жесты");
    {
        const Color& m = p.textDim;
        g.label(L"Запись идёт 10 секунд. Приложение само поймёт, что вы показали: позу одной рукой, позу двумя руками "
                L"(держите обе в кадре всё время) или позу с движением (повторяйте движение всю запись — сработает только "
                L"с ним). Слегка меняйте наклон кисти, чтобы жест узнавался под разными углами.",
                col.next(54), 12.5f, &m, Weight::Regular, Align::Start, Align::Start, true);
    }
    for (size_t i = 0; i < S.customGestures.size(); ++i) {
        CustomGesture& cg = S.customGestures[i];
        g.pushId("cg" + cg.id);
        const RectF row = col.next(64);
        g.card(row, 8);
        c.icon(cg.kind == CustomKind::Motion ? glyph::Swap : glyph::Record, {row.x + 16, row.y, 22, row.h}, p.accent, 16.f);
        c.text(widen(cg.name), {row.x + 54, row.y + 12, 300, 20}, p.text, 14.f, Weight::Semibold);
        std::wstring kind;
        if (!cg.recorded()) kind = L"не записан";
        else if (cg.legacy()) kind = L"старая 2D-запись — нажмите «Заново»";
        else if (cg.kind == CustomKind::TwoHands) kind = L"две руки";
        else if (cg.kind == CustomKind::Motion)
            kind = cg.axis == MotionAxis::Depth ? L"движение к камере и обратно" : cg.axis == MotionAxis::X ? L"движение влево-вправо" : L"движение вверх-вниз";
        else kind = L"одна рука";
        if (cg.recorded()) kind += L" · " + std::to_wstring(cg.legacy() ? cg.samples.size() : cg.rows.size()) + L" обр.";
        c.text(kind, {row.x + 54, row.y + 33, 300, 18}, cg.legacy() ? p.warning : p.textDim, 12.f);
        // Strictness: left = stricter (fewer false triggers), right = looser.
        const RectF sr{row.x + 370, row.y + 15, std::max(120.f, row.w - 740), 34};
        if (cg.legacy()) {
            if (g.slider("tol", sr, cg.tolerance, 0.08f, 0.5f, 0.01f, [](float v) { return fmt1(v * 100.f, L" %"); }))
                app.settingsChanged();
        } else if (cg.recorded()) {
            if (g.slider("strict", sr, cg.strictness, 0.6f, 1.6f, 0.05f,
                         [](float v) { return v < 0.95f ? std::wstring(L"строже") : v > 1.05f ? std::wstring(L"мягче") : std::wstring(L"обычно"); }))
                app.settingsChanged();
            g.tooltip(sr, L"Левее — строже: меньше ложных срабатываний, но жест нужно показывать точнее");
        }
        const bool live = (s.hasHand && s.hand.customId == cg.id) || (s.hasSecondHand && s.hand2.customId == cg.id) ||
                          s.lastDynamic == "custom:" + cg.id;
        if (live) g.pill({row.right() - 300, row.y + 21, 70, 22}, L"сейчас", p.success);
        const bool recording = st.recordingGesture == cg.id && s.recordProgress >= 0.f;
        if (recording) g.progressRing({row.right() - 220, row.y + row.h / 2}, 11, s.recordProgress, p.accent);
        if (g.button("rerec", {row.right() - 206, row.y + 15, 92, 34}, L"Заново", ButtonKind::Standard, nullptr, !recording)) {
            st.recordingGesture = cg.id;
            app.startRecording(cg.id);
        }
        if (confirmButton(g, st, "cg" + cg.id, {row.right() - 106, row.y + 15, 94, 34}, L"Удалить")) {
            const std::string gid = "custom:" + cg.id;
            S.customGestures.erase(S.customGestures.begin() + static_cast<long>(i));
            for (auto& r : S.rules)
                if (r.gesture == gid) r.enabled = false;  // keep the rule, but it can't match any more
            app.settingsChanged();
            g.popId();
            break;
        }
        g.popId();
    }
    {
        const RectF row = col.next(64);
        g.card(row, 8);
        g.textField("newgest", {row.x + 16, row.y + 15, 280, 34}, st.newGestureName, L"Название, например «Колодец»", 40);
        const bool busy = !st.recordingGesture.empty() && s.recordProgress >= 0.f;
        if (g.button("rec", {row.x + 308, row.y + 15, 200, 34}, L"Записать 10 секунд", ButtonKind::Accent, glyph::Record,
                     !busy && !st.newGestureName.empty())) {
            CustomGesture ng;
            ng.id = newId();
            ng.name = narrow(st.newGestureName);
            S.customGestures.push_back(ng);
            st.recordingGesture = ng.id;
            st.newGestureName.clear();
            app.settingsChanged();
            app.startRecording(ng.id);
        }
        if (busy) {
            g.progressRing({row.x + 540, row.y + row.h / 2}, 12, s.recordProgress, p.accent);
            c.text(widen(s.taskHint), {row.x + 562, row.y, row.w - 580, row.h}, p.textDim, 12.5f, Weight::Regular, Align::Start,
                   Align::Center, true);
        } else if (!app.lastTaskMessage().empty()) {
            c.text(app.lastTaskMessage(), {row.x + 524, row.y, row.w - 540, row.h}, p.textDim, 12.5f, Weight::Regular,
                   Align::Start, Align::Center, true);
        }
    }

    // Built-in gestures: the user can switch off the ones that misfire for them.
    section(g, col, L"Встроенные жесты");
    {
        const Color& m = p.textDim;
        g.label(L"Выключенный жест не распознаётся и исчезает из списка «Жест». Правила с ним перестают срабатывать.",
                col.next(20), 12.5f, &m);
    }
    {
        const auto& all = builtinGestures();
        const float cellW = (col.w - 10) / 2, cellH = 44;
        const size_t rows = (all.size() + 1) / 2;
        const RectF box = col.next(rows * (cellH + 6));
        for (size_t i = 0; i < all.size(); ++i) {
            const auto& b = all[i];
            const RectF cell{box.x + (i % 2) * (cellW + 10), box.y + (i / 2) * (cellH + 6), cellW, cellH};
            g.card(cell, 7);
            bool on = S.gestureEnabled(b.id);
            c.text(widen(b.nameRu), {cell.x + 16, cell.y, cell.w - 90, cell.h}, on ? p.text : p.textMuted, 13.5f);
            if (b.twoHands || b.dynamic)
                c.text(b.twoHands ? L"2 руки" : L"движение", {cell.right() - 150, cell.y, 70, cell.h}, p.textMuted, 11.5f,
                       Weight::Regular, Align::End);
            if (g.toggle(std::string("bi_") + b.id, {cell.right() - 60, cell.y, 48, cell.h}, on, false)) {
                auto& d = S.disabledGestures;
                if (on) d.erase(std::remove(d.begin(), d.end(), std::string(b.id)), d.end());
                else d.push_back(b.id);
                app.settingsChanged();
            }
        }
    }
    if (!S.disabledGestures.empty()) {
        const RectF r = col.next(40);
        if (g.button("bi_restore", {r.x, r.y, 220, 36}, L"Вернуть все встроенные", ButtonKind::Standard, glyph::Refresh)) {
            S.disabledGestures.clear();
            app.settingsChanged();
        }
    }
}

// --------------------------------------------------------------------- faces

void drawFaces(Gui& g, App& app, UiState& st, const RectF& area) {
    PageScope ps(g, "faces", area, L"Лица", L"Кому разрешено управлять жестами и когда камера считает, что вы за компьютером");
    Column& col = ps.col;
    Canvas& c = g.canvas();
    const Palette& p = g.pal();
    Settings& S = app.settings();
    const auto& s = app.snapshot();
    FaceDatabase& db = app.faces();

    // Preview + current recognition.
    {
        const RectF r = col.next(250);
        g.card(r, 10);
        const float pw = (r.h - 24) * 16.f / 9.f;
        drawPreview(g, app, {r.x + 12, r.y + 12, pw, r.h - 24}, true);
        float y = r.y + 22;
        const float x = r.x + pw + 32, w = r.right() - x - 20;
        c.text(L"В кадре", {x, y, w, 18}, p.textMuted, 12.f);
        y += 20;
        if (s.faces.empty()) {
            c.text(L"Лиц не видно", {x, y, w, 24}, p.textDim, 15.f, Weight::Semibold);
        }
        for (const auto& f : s.faces) {
            const std::wstring n = f.personId.empty() ? L"Незнакомое лицо" : widen(f.name);
            c.text(n, {x, y, w, 24}, f.personId.empty() ? p.text : p.success, 15.f, Weight::Semibold);
            y += 24;
            c.text(L"сходство " + fmt1(std::max(0.f, f.similarity) * 100.f, L" %") + (f.looking ? L" · смотрит на экран" : L" · отвернулся"),
                   {x, y, w, 18}, p.textDim, 12.5f);
            y += 28;
            if (y > r.bottom() - 30) break;
        }
    }

    if (S.faces.embedModel == "int8" && !db.people.empty()) {
        const RectF r = settingRow(g, col, glyph::Warning, L"Лица записаны старой моделью",
                                   L"Новая модель узнавания точнее и в 7 раз быстрее, но старые образцы ей не подходят. "
                                   L"Переход сотрёт образцы (имена останутся) — затем нажмите «Дописать» у каждого человека.", 220);
        if (confirmButton(g, st, "embed_fp32", r, L"Перейти на точную")) {
            for (auto& pp : db.people) { pp.rgb.clear(); pp.ir.clear(); }
            S.faces.embedModel = "fp32";
            app.facesChanged();
            app.settingsChanged();
        }
    }    section(g, col, L"Узнавание");
    {
        const Color& m = p.textDim;
        g.label(L"Лицо для жестов не обязательно. Нужно ли оно, выбирается в каждом правиле: «Только когда в кадре есть лицо», «Кто может подавать жест».",
                col.next(36), 12.5f, &m, Weight::Regular, Align::Start, Align::Start, true);
    }
    RectF rr = settingRow(g, col, glyph::Contact, L"Порог узнавания",
                    L"Выше — строже (меньше ложных узнаваний, но иногда «не узнаёт»). Рекомендуется 36–45 %.");
    if (g.slider("thr", rr, S.faces.matchThreshold, 0.25f, 0.7f, 0.01f, [](float v) { return fmt1(v * 100, L" %"); })) app.settingsChanged();
    rr = settingRow(g, col, glyph::Eye, L"Допуск «смотрит на экран»", L"Насколько можно повернуть голову");
    if (g.slider("look", rr, S.faces.lookingTolerance, 0.15f, 0.9f, 0.01f, [](float v) { return fmt1(v * 100, L" %"); })) app.settingsChanged();

    section(g, col, L"Люди");
    for (size_t i = 0; i < db.people.size(); ++i) {
        Person& pr = db.people[i];
        g.pushId("p" + pr.id);
        const RectF row = col.next(68);
        g.card(row, 8);
        const std::wstring nm = widen(pr.name);
        c.fillCircle({row.x + 36, row.y + row.h / 2}, 20, p.accent.alpha(0.22f));
        c.text(nm.empty() ? L"?" : nm.substr(0, 1), {row.x + 16, row.y, 40, row.h}, p.accent, 17.f, Weight::Semibold, Align::Center);
        c.text(nm, {row.x + 70, row.y + 13, 300, 20}, p.text, 14.5f, Weight::Semibold);
        c.text(std::to_wstring(pr.rgb.size()) + L" образцов (камера) · " + std::to_wstring(pr.ir.size()) + L" (ИК)",
               {row.x + 70, row.y + 35, 360, 18}, p.textDim, 12.f);
        const bool enrolling = st.enrollingPerson == pr.id && s.enrollProgress >= 0.f;
        if (enrolling) {
            g.progressRing({row.right() - 260, row.y + row.h / 2}, 12, s.enrollProgress, p.accent);
            c.text(widen(s.taskHint), {row.right() - 520, row.y, 240, row.h}, p.textDim, 12.f, Weight::Regular, Align::End);
        }
        if (g.button("more", {row.right() - 216, row.y + 17, 104, 34}, L"Дописать", ButtonKind::Standard, glyph::Add, !enrolling)) {
            st.enrollingPerson = pr.id;
            app.startEnrollment(pr.id, App::kEnrollMoreMs);
        }
        if (confirmButton(g, st, "p" + pr.id, {row.right() - 104, row.y + 17, 92, 34}, L"Удалить")) {
            const std::string pid = pr.id;
            db.remove(pid);
            for (auto& r : S.rules)
                if (r.person == PersonFilter::Specific && r.personId == pid) r.person = PersonFilter::AnyKnown;
            app.facesChanged();
            app.settingsChanged();
            g.popId();
            break;
        }
        g.popId();
    }
    {
        const RectF row = col.next(64);
        g.card(row, 8);
        g.textField("newname", {row.x + 16, row.y + 15, 260, 34}, st.newPersonName, L"Имя, например «Анна»", 32);
        const bool busy = !st.enrollingPerson.empty() && s.enrollProgress >= 0.f;
        if (g.button("addp", {row.x + 288, row.y + 15, 220, 34}, L"Добавить и запомнить", ButtonKind::Accent, glyph::Add,
                     !busy && !st.newPersonName.empty())) {
            Person np;
            np.id = newId();
            np.name = narrow(st.newPersonName);
            np.createdUnix = static_cast<int64_t>(time(nullptr));
            db.people.push_back(np);
            st.enrollingPerson = np.id;
            st.newPersonName.clear();
            app.facesChanged();
            app.startEnrollment(np.id, App::kEnrollNewMs);
        }
        c.text(busy ? widen(s.taskHint) : L"Сядьте как обычно и ~12 секунд медленно поворачивайте голову: влево-вправо, вверх-вниз",
               {row.x + 524, row.y, row.w - 540, row.h}, p.textDim, 12.5f, Weight::Regular, Align::Start, Align::Center, true);
    }
    {
        const Color& m = p.textMuted;
        g.label(L"Фотографии не сохраняются: хранятся только числовые отпечатки лица (128 чисел на снимок), "
                L"зашифрованные Windows DPAPI под вашей учётной записью. ИК-камера даёт отдельные отпечатки — "
                L"запишите себя ещё раз в темноте, чтобы узнавание работало и ночью.",
                col.next(56), 12.f, &m, Weight::Regular, Align::Start, Align::Start, true);
    }
}

// --------------------------------------------------------------------- camera

void drawCamera(Gui& g, App& app, UiState& st, const RectF& area) {
    PageScope ps(g, "camera", area, L"Камера и ночное зрение", L"Источник, разрешение, работа в темноте и калибровка дистанции");
    Column& col = ps.col;
    Canvas& c = g.canvas();
    const Palette& p = g.pal();
    Settings& S = app.settings();
    const auto& s = app.snapshot();

    {
        const RectF r = col.next(300);
        g.card(r, 10);
        const float pw = std::min(r.w * 0.62f, (r.h - 24) * 16.f / 9.f);
        drawPreview(g, app, {r.x + 12, r.y + 12, pw, r.h - 24}, false);
        const float x = r.x + pw + 32, w = r.right() - x - 20;
        float y = r.y + 22;
        c.text(L"Камера", {x, y, w, 18}, p.textMuted, 12.f);
        c.text(widen(s.camera.empty() ? "—" : s.camera), {x, y + 18, w, 40}, p.text, 13.5f, Weight::Semibold, Align::Start, Align::Start, true);
        y += 66;
        c.text(L"Освещённость", {x, y, w, 18}, p.textMuted, 12.f);
        const RectF bar{x, y + 24, w, 8};
        c.fillRound(bar, 4, p.textMuted.alpha(0.25f));
        c.fillRound({bar.x, bar.y, bar.w * std::clamp(s.light / 0.7f, 0.f, 1.f), 8}, 4, s.nightActive ? p.night : p.warning);
        const float tx = bar.x + bar.w * std::clamp(S.camera.nightThreshold / 0.7f, 0.f, 1.f);
        c.fillRect({tx - 1, bar.y - 5, 2, 18}, p.text);
        y += 44;
        c.text(s.nightActive ? (s.infrared ? L"Ночь: ИК-сенсор Windows Hello" : L"Ночь: программное усиление") : L"День",
               {x, y, w, 20}, s.nightActive ? p.night : p.text, 14.f, Weight::Semibold);
        y += 30;
        bool mirror = S.camera.mirrorPreview;
        c.text(L"Зеркальный просмотр", {x, y, w - 60, 32}, p.text, 13.5f);
        if (g.toggle("mirror", {x + w - 60, y, 60, 32}, mirror, false)) { S.camera.mirrorPreview = mirror; app.settingsChanged(); }
    }

    section(g, col, L"Источник");
    const auto& cams = app.cameras();
    std::vector<std::wstring> names = {L"Автоматически"};
    std::vector<std::wstring> links = {L""};
    bool haveIr = false;
    for (const auto& d : cams) {
        if (d.infrared) { haveIr = true; continue; }
        names.push_back(d.name);
        links.push_back(d.symlink);
    }
    int sel = 0;
    for (size_t i = 1; i < links.size(); ++i)
        if (_wcsicmp(links[i].c_str(), widen(S.camera.deviceId).c_str()) == 0) sel = static_cast<int>(i);
    RectF rr = settingRow(g, col, glyph::Camera, L"Камера", haveIr ? L"Найдена ИК-камера Windows Hello — будет использоваться ночью" : L"ИК-камера не найдена", 360);
    if (g.combo("cam", {rr.x, rr.y, rr.w - 44, rr.h}, sel, names)) { S.camera.deviceId = narrow(links[sel]); app.settingsChanged(); }
    if (g.iconButton("camrefresh", {rr.right() - 36, rr.y, 36, rr.h}, glyph::Refresh, L"Обновить список")) app.refreshCameras();

    int res = S.camera.width >= 1280 ? 2 : S.camera.width >= 640 ? 1 : 0;
    rr = settingRow(g, col, glyph::Speed, L"Разрешение захвата",
                    L"Распознаванию хватает 640×360: выше — только больше нагрузки на USB и ISP", 360);
    if (g.segmented("res", rr, res, {L"424×240", L"640×360", L"1280×720"})) {
        static const int W[3] = {424, 640, 1280}, H[3] = {240, 360, 720};
        S.camera.width = W[res];
        S.camera.height = H[res];
        app.settingsChanged();
    }

    section(g, col, L"Ночное зрение");
    int nm = static_cast<int>(S.camera.night);
    rr = settingRow(g, col, glyph::Moon, L"Режим",
                    L"Авто: при темноте включает усиление, а если темно долго — ИК-камеру с подсветкой. Программно: всегда усиливать. ИК: всегда ИК-камера.", 400);
    if (g.segmented("night", rr, nm, {L"Выкл", L"Авто", L"Программно", L"ИК"})) { S.camera.night = static_cast<NightMode>(nm); app.settingsChanged(); }
    rr = settingRow(g, col, glyph::Brightness, L"Порог темноты", L"Белая метка на шкале освещённости выше");
    if (g.slider("nthr", rr, S.camera.nightThreshold, 0.05f, 0.5f, 0.01f, [](float v) { return fmt1(v * 100, L" %"); })) app.settingsChanged();
    rr = settingRow(g, col, glyph::Flash, L"Сила усиления", L"Адаптивная гамма + локальный контраст");
    if (g.slider("nstr", rr, S.camera.nightStrength, 0.f, 1.f, 0.05f, [](float v) { return fmt1(v * 100, L" %"); })) app.settingsChanged();
    rr = settingRow(g, col, nullptr, L"Локальный контраст (CLAHE)", L"Вытягивает руку из полутени, не пересвечивая лампу в кадре");
    if (g.toggle("clahe", rr, S.camera.nightLocalContrast)) app.settingsChanged();
    rr = settingRow(g, col, nullptr, L"Шумоподавление", L"Сглаживает зерно в неподвижных зонах, не размывая движение руки");
    if (g.toggle("dn", rr, S.camera.nightDenoise)) app.settingsChanged();
    rr = settingRow(g, col, nullptr, L"Аппаратное усиление камеры", L"Поднимает усиление и компенсацию подсветки сенсора в темноте");
    if (g.toggle("hwll", rr, S.camera.hardwareLowLight)) app.settingsChanged();
    rr = settingRow(g, col, nullptr, L"ИК-подсветка", L"Просить драйвер включать ИК-излучатель Windows Hello");
    if (g.toggle("torch", rr, S.camera.irTorch)) app.settingsChanged();
    rr = settingRow(g, col, nullptr, L"Подавление засветки в ИК", L"Вычитает кадр без подсветки: солнце и лампы не мешают");
    if (g.toggle("amb", rr, S.camera.irAmbientSuppression)) app.settingsChanged();

    section(g, col, L"Руки");
    rr = settingRow(g, col, glyph::Swap, L"Поменять левую и правую руку",
                    L"Если в «Обзоре» ваша правая рука подписана как левая");
    if (g.toggle("swap", rr, S.camera.swapHandedness)) app.settingsChanged();
    col.space(4);
    if (g.button("privacy", col.next(36), L"Параметры конфиденциальности камеры", ButtonKind::Subtle, glyph::Lock))
        win::openUri(L"ms-settings:privacy-webcam");
}

// --------------------------------------------------------------------- power

void drawPower(Gui& g, App& app, UiState& st, const RectF& area) {
    PageScope ps(g, "power", area, L"Питание и производительность",
                 L"Ryzen AI Max+ 392 · Radeon 8060S · NPU XDNA 2 — сколько ресурсов отдавать распознаванию");
    Column& col = ps.col;
    Canvas& c = g.canvas();
    const Palette& p = g.pal();
    Settings& S = app.settings();
    const auto& s = app.snapshot();
    const auto& stt = app.stats();

    {
        const RectF r = col.next(128);
        g.card(r, 10);
        const float w = (r.w - 40) / 5;
        struct M { std::wstring t, v; };
        const M m[5] = {{L"Процессор", fmt1(stt.cpuPercent, L" %")},
                        {L"Память", fmt1(stt.workingSetMb, L" МБ")},
                        {L"Кадров/с", fmt1(s.fps, L"")},
                        {L"Кадр целиком", fmt1(s.msFrame, L" мс")},
                        {L"Устройство", s.device.empty() ? L"—" : widen(s.device)}};
        for (int i = 0; i < 5; ++i) {
            c.text(m[i].t, {r.x + 20 + i * w, r.y + 16, w - 10, 18}, p.textMuted, 12.f);
            c.text(m[i].v, {r.x + 20 + i * w, r.y + 36, w - 10, 30}, p.text, i == 4 ? 15.f : 21.f, Weight::Semibold);
        }
        // Stage timing bars.
        const double stages[4] = {s.msPalm, s.msHand, s.msFace, s.msEmbed};
        const wchar_t* names[4] = {L"ладонь", L"кисть", L"лицо", L"узнавание"};
        const float bw = (r.w - 40) / 4;
        for (int i = 0; i < 4; ++i) {
            const float x = r.x + 20 + i * bw;
            c.text(std::wstring(names[i]) + L" " + fmt1(stages[i], L" мс"), {x, r.y + 78, bw - 12, 16}, p.textDim, 11.5f);
            c.fillRound({x, r.y + 100, bw - 16, 6}, 3, p.textMuted.alpha(0.25f));
            c.fillRound({x, r.y + 100, (bw - 16) * static_cast<float>(std::min(1.0, stages[i] / 20.0)), 6}, 3, p.accent);
        }
    }

    section(g, col, L"Профиль");
    int prof = static_cast<int>(S.perf.profile);
    RectF rr = settingRow(g, col, glyph::Leaf, L"Режим работы",
                          L"Эко: EcoQoS, ≤15 к/с, 1 поток. Баланс: EcoQoS, частота по настройкам. Отклик: без троттлинга и с повышенным приоритетом — для игр.", 360);
    if (g.segmented("prof", rr, prof, {L"Эко", L"Баланс", L"Отклик"})) { S.perf.profile = static_cast<PowerProfile>(prof); app.settingsChanged(); }
    float af = static_cast<float>(S.perf.activeFps);
    rr = settingRow(g, col, glyph::Speed, L"Частота, когда рука в кадре", L"Выше — точнее удержание и взмахи");
    if (g.slider("afps", rr, af, 10, 60, 1, [](float v) { return fmtInt(v, L" к/с"); })) { S.perf.activeFps = static_cast<int>(af); app.settingsChanged(); }
    float idf = static_cast<float>(S.perf.idleFps);
    rr = settingRow(g, col, nullptr, L"Частота в ожидании", L"Пока руки нет: ищем её реже, почти без нагрузки");
    if (g.slider("ifps", rr, idf, 1, 15, 1, [](float v) { return fmtInt(v, L" к/с"); })) { S.perf.idleFps = static_cast<int>(idf); app.settingsChanged(); }
    float sl = static_cast<float>(S.perf.sleepAfterSec);
    rr = settingRow(g, col, glyph::Moon, L"Засыпать, если никого нет", L"Нет ни лица, ни руки — 1 кадр в секунду");
    if (g.slider("sleep", rr, sl, 10, 600, 10, [](float v) { return v >= 60 ? fmt1(v / 60.f, L" мин") : fmtInt(v, L" с"); })) {
        S.perf.sleepAfterSec = static_cast<int>(sl);
        app.settingsChanged();
    }

    section(g, col, L"Автоматика");
    rr = settingRow(g, col, glyph::Battery, L"На батарее — режим «Эко»", app.power().onBattery ? L"Сейчас: от батареи" : L"Сейчас: от сети");
    if (g.toggle("ecobat", rr, S.perf.ecoOnBattery)) app.settingsChanged();
    rr = settingRow(g, col, glyph::Game, L"Пауза в полноэкранных играх и презентациях", L"Камера освобождается, CPU не тратится");
    if (g.toggle("fs", rr, S.perf.pauseInFullscreen)) app.settingsChanged();
    rr = settingRow(g, col, glyph::Lock, L"Выключать камеру при блокировке ПК", L"Индикатор камеры гаснет; возобновится после входа");
    if (g.toggle("lock", rr, S.perf.pauseWhenLocked)) app.settingsChanged();

    section(g, col, L"Вычисления");
    int dev = static_cast<int>(S.perf.device);
    rr = settingRow(g, col, glyph::Chip, L"Где считать нейросети",
                    L"Процессор: модели крошечные, 1 ядро Zen 5 справляется за 2–5 мс — это самый экономичный вариант. "
                    L"Видеокарта (DirectML) будит iGPU — имеет смысл только для 2+ потоков. NPU требует сборку ONNX Runtime с VitisAI (Ryzen AI SDK).", 360);
    if (g.segmented("dev", rr, dev, {L"Процессор", L"Radeon iGPU", L"NPU XDNA"})) { S.perf.device = static_cast<ComputeDevice>(dev); app.settingsChanged(); }
    float th = static_cast<float>(S.perf.threads);
    rr = settingRow(g, col, nullptr, L"Потоков на модель", L"1 — минимум энергии; 2 — меньше задержка");
    if (g.slider("thr", rr, th, 1, 4, 1, [](float v) { return fmtInt(v, L""); })) { S.perf.threads = static_cast<int>(th); app.settingsChanged(); }
    rr = settingRow(g, col, nullptr, L"Квантованные модели (int8)", L"Экономит ~5 МБ, но в наших тестах модель кисти int8 была в 4 раза медленнее и чаще ошибалась. Не рекомендуется.");
    if (g.toggle("int8", rr, S.perf.int8Models)) app.settingsChanged();
    (void)st;
}

// --------------------------------------------------------------------- system

void drawSystem(Gui& g, App& app, UiState& st, const RectF& area) {
    PageScope ps(g, "system", area, L"Система", L"Автозапуск, значок у часов, горячие клавиши и оформление");
    Column& col = ps.col;
    Canvas& c = g.canvas();
    const Palette& p = g.pal();
    Settings& S = app.settings();

    section(g, col, L"Запуск");
    RectF rr = settingRow(g, col, glyph::Power, L"Запускать вместе с Windows", L"Сразу в трей, без окна");
    if (g.toggle("auto", rr, S.ui.autostart)) app.settingsChanged();
    rr = settingRow(g, col, glyph::Taskbar, L"Значок рядом с часами",
                    L"Windows 11 прячет новые значки в «^». Включите Vibe Hand в «Другие значки в области уведомлений».", 300);
    if (g.button("tb", rr, L"Параметры панели задач", ButtonKind::Standard, glyph::Open)) win::openUri(L"ms-settings:taskbar");

    section(g, col, L"Горячие клавиши");
    rr = settingRow(g, col, glyph::Keyboard, L"Пауза / возобновление распознавания", L"Работает в любом приложении");
    if (g.hotkeyField("hk1", rr, S.ui.hotkeyToggle)) app.settingsChanged();
    rr = settingRow(g, col, glyph::Keyboard, L"Открыть Vibe Hand", L"");
    if (g.hotkeyField("hk2", rr, S.ui.hotkeyOpen)) app.settingsChanged();

    section(g, col, L"Уведомления");
    rr = settingRow(g, col, glyph::Notification, L"Подсказка на экране при срабатывании", L"Небольшая плашка внизу экрана и кольцо удержания");
    if (g.toggle("osd", rr, S.ui.osd)) app.settingsChanged();
    rr = settingRow(g, col, glyph::Notification, L"Системные уведомления", L"Пауза, проблемы с камерой, окончание записи лица");
    if (g.toggle("notif", rr, S.ui.notifications)) app.settingsChanged();

    section(g, col, L"Оформление");
    int th = static_cast<int>(S.ui.theme);
    rr = settingRow(g, col, glyph::Color, L"Тема", L"", 360);
    if (g.segmented("theme", rr, th, {L"Тёмная", L"Светлая", L"Как в Windows"})) { S.ui.theme = static_cast<Theme>(th); app.settingsChanged(); }
    {
        rr = settingRow(g, col, glyph::Color, L"Акцентный цвет", accentPresets()[std::clamp(S.ui.accent, 0, static_cast<int>(accentPresets().size()) - 1)].name, 360);
        const auto& pr = accentPresets();
        for (size_t i = 0; i < pr.size(); ++i) {
            const Color col2 = Color::hex(pr[i].rgb ? pr[i].rgb : win::systemAccentColor());
            if (g.swatch("acc" + std::to_string(i), {rr.right() - 14 - (pr.size() - 1 - i) * 40.f, rr.y + rr.h / 2}, 13, col2,
                         S.ui.accent == static_cast<int>(i))) {
                S.ui.accent = static_cast<int>(i);
                app.settingsChanged();
            }
        }
    }
    rr = settingRow(g, col, nullptr, L"Материал Mica", L"Полупрозрачный фон окна, как в приложениях Windows 11");
    if (g.toggle("mica", rr, S.ui.mica)) app.settingsChanged();

    section(g, col, L"Данные");
    {
        const RectF r = col.next(40);
        if (g.button("data", {r.x, r.y, 220, 36}, L"Открыть папку данных", ButtonKind::Standard, glyph::Folder)) app.openDataFolder();
        if (confirmButton(g, st, "reset", {r.x + 232, r.y, 220, 36}, L"Сбросить настройки")) app.resetSettings();
    }

    section(g, col, L"О программе");
    {
        const RectF r = col.next(132);
        g.card(r, 8);
        c.text(L"Vibe Hand 0.3 — фоновое управление жестами для Lenovo Legion 7 (15ASH11)", {r.x + 18, r.y + 14, r.w - 36, 20}, p.text, 14.f, Weight::Semibold);
        c.text(L"Нейросети: MediaPipe Palm Detection + Hand Landmark (Apache-2.0), YuNet (MIT), SFace (Apache-2.0) — "
               L"в сборке OpenCV Zoo. Инференс: ONNX Runtime (MIT). Интерфейс: Direct2D. Всё работает локально, "
               L"ничего не отправляется в сеть.",
               {r.x + 18, r.y + 40, r.w - 36, 80}, p.textDim, 12.5f, Weight::Regular, Align::Start, Align::Start, true);
    }
}

}  // namespace vl::ui
