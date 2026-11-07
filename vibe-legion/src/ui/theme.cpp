#include "ui/theme.hpp"

#include <algorithm>

#include "platform/win/system.hpp"

namespace vl::ui {

const std::vector<AccentPreset>& accentPresets() {
    static const std::vector<AccentPreset> p = {
        {L"Как в Windows", 0},
        {L"Legion Blue", 0x3B82F6},
        {L"Циан", 0x22D3EE},
        {L"Янтарь", 0xF59E0B},
        {L"Фиолетовый", 0xA78BFA},
        {L"Лайм", 0x84CC16},
        {L"Малиновый", 0xF43F5E},
    };
    return p;
}

Palette makePalette(bool dark, uint32_t accent) {
    Palette p;
    p.dark = dark;
    Color a = Color::hex(accent);
    // Keep the accent readable on both backgrounds (Windows accents can be very dark).
    const float lum = 0.2126f * a.r + 0.7152f * a.g + 0.0722f * a.b;
    if (dark && lum < 0.35f) a = a.mix(Color{1, 1, 1, 1}, 0.35f);
    if (!dark && lum > 0.6f) a = a.mix(Color{0, 0, 0, 1}, 0.3f);
    p.accent = a;
    p.accentHover = a.mix(dark ? Color{1, 1, 1, 1} : Color{0, 0, 0, 1}, 0.12f);
    const float alum = 0.2126f * a.r + 0.7152f * a.g + 0.0722f * a.b;
    p.onAccent = alum > 0.55f ? Color::hex(0x0B0D10) : Color::hex(0xFFFFFF);
    if (dark) {
        p.window = Color::hex(0x0D0F13);
        p.sidebar = Color::hex(0x000000, 0.18f);
        p.card = Color::hex(0xFFFFFF, 0.045f);
        p.cardHover = Color::hex(0xFFFFFF, 0.07f);
        p.cardStroke = Color::hex(0xFFFFFF, 0.07f);
        p.control = Color::hex(0xFFFFFF, 0.06f);
        p.controlHover = Color::hex(0xFFFFFF, 0.09f);
        p.controlPressed = Color::hex(0xFFFFFF, 0.04f);
        p.controlStroke = Color::hex(0xFFFFFF, 0.09f);
        p.text = Color::hex(0xF3F5F8);
        p.textDim = Color::hex(0xB4BAC4);
        p.textMuted = Color::hex(0x7D8591);
        p.divider = Color::hex(0xFFFFFF, 0.08f);
        p.overlay = Color::hex(0x1B1F26);
    } else {
        p.window = Color::hex(0xF3F4F7);
        p.sidebar = Color::hex(0xFFFFFF, 0.25f);
        p.card = Color::hex(0xFFFFFF, 0.72f);
        p.cardHover = Color::hex(0xFFFFFF, 0.9f);
        p.cardStroke = Color::hex(0x000000, 0.06f);
        p.control = Color::hex(0xFFFFFF, 0.8f);
        p.controlHover = Color::hex(0xF8F9FB, 1.f);
        p.controlPressed = Color::hex(0xEEF0F3, 1.f);
        p.controlStroke = Color::hex(0x000000, 0.1f);
        p.text = Color::hex(0x15181D);
        p.textDim = Color::hex(0x4A515C);
        p.textMuted = Color::hex(0x7A828E);
        p.divider = Color::hex(0x000000, 0.08f);
        p.overlay = Color::hex(0xFBFBFC);
    }
    p.success = Color::hex(dark ? 0x3DDC84 : 0x14A44D);
    p.warning = Color::hex(dark ? 0xFFC14D : 0xC27C00);
    p.danger = Color::hex(dark ? 0xFF6B6B : 0xD13438);
    p.info = Color::hex(dark ? 0x5BB0FF : 0x0F6CBD);
    p.night = Color::hex(dark ? 0xB18CFF : 0x7B4FE0);
    return p;
}

bool isDark(const UiSettings& ui) {
    if (ui.theme == Theme::System) return !win::systemUsesLightTheme();
    return ui.theme == Theme::Dark;
}

uint32_t accentFor(const UiSettings& ui) {
    const auto& pr = accentPresets();
    const int i = std::clamp(ui.accent, 0, static_cast<int>(pr.size()) - 1);
    return pr[i].rgb ? pr[i].rgb : win::systemAccentColor();
}

Palette paletteFor(const UiSettings& ui) { return makePalette(isDark(ui), accentFor(ui)); }

}  // namespace vl::ui
