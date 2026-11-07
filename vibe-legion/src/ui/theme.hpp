// Colour tokens. Dark theme is the default and is tuned to sit next to Lenovo's
// gaming software and Windows 11 dark mode: near-black surfaces, soft card layers,
// one accent colour (Windows accent or a preset).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/settings.hpp"

namespace vl::ui {

struct Color {
    float r = 0, g = 0, b = 0, a = 1;
    static Color hex(uint32_t rgb, float a = 1.f) {
        return {((rgb >> 16) & 0xFF) / 255.f, ((rgb >> 8) & 0xFF) / 255.f, (rgb & 0xFF) / 255.f, a};
    }
    Color alpha(float na) const { return {r, g, b, na}; }
    Color mix(const Color& o, float t) const {
        return {r + (o.r - r) * t, g + (o.g - g) * t, b + (o.b - b) * t, a + (o.a - a) * t};
    }
};

struct Palette {
    bool dark = true;
    Color window;       // solid background when Mica is off
    Color sidebar;      // navigation pane tint
    Color card;         // card fill
    Color cardHover;
    Color cardStroke;
    Color control;      // buttons / inputs
    Color controlHover;
    Color controlPressed;
    Color controlStroke;
    Color text;
    Color textDim;
    Color textMuted;
    Color accent;
    Color accentHover;
    Color onAccent;
    Color success, warning, danger, info, night;
    Color divider;
    Color overlay;      // popups
};

struct AccentPreset {
    const wchar_t* name;
    uint32_t rgb;  // 0 = Windows accent
};
const std::vector<AccentPreset>& accentPresets();

Palette makePalette(bool dark, uint32_t accentRgb);
// Resolves the Settings (theme + accent index) against the current system state.
Palette paletteFor(const UiSettings& ui);
bool isDark(const UiSettings& ui);
uint32_t accentFor(const UiSettings& ui);

}  // namespace vl::ui
