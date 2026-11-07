// Small immediate-mode widget set drawn with Direct2D in the Windows 11 Fluent
// style (toggle switches, sliders, segmented controls, combo boxes, text fields,
// hotkey capture, navigation items, cards). Each window rebuilds its UI per paint;
// widget state that must persist (animations, focus, open popup) lives here.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ui/gfx.hpp"

namespace vl::ui {

struct Input {
    Vec2 mouse{-1000, -1000};
    bool down = false, pressed = false, released = false, rightPressed = false;
    bool doubleClick = false;
    float wheel = 0.f;
    std::wstring chars;           // WM_CHAR text typed this frame
    std::vector<uint32_t> keys;   // WM_KEYDOWN virtual keys this frame
    bool ctrl = false, shift = false, alt = false, win = false;
    void endFrame() {
        pressed = released = rightPressed = doubleClick = false;
        wheel = 0.f;
        chars.clear();
        keys.clear();
    }
};

enum class ButtonKind { Standard, Accent, Subtle, Danger };

class Gui {
public:
    void begin(Canvas& c, const Palette& p, Input& in, double timeSec);
    // Returns true while something is animating (caller schedules another frame).
    bool end();

    Canvas& canvas() { return *c_; }
    const Palette& pal() const { return *p_; }
    Input& input() { return *in_; }

    // ---- ids
    uint32_t id(const std::string& s) const;
    void pushId(const std::string& s);
    void pushId(int i) { pushId(std::to_string(i)); }
    void popId();

    // ---- helpers
    bool hovered(const RectF& r) const;
    float anim(uint32_t id, float target, float speed = 14.f);  // exponential approach
    void setCursorHand() { wantHand_ = true; }
    bool wantsHandCursor() const { return wantHand_; }
    bool wantsTextCursor() const { return wantText_; }
    void tooltip(const RectF& r, const std::wstring& text);
    bool anyPopupOpen() const { return popup_ != 0; }
    bool capturingKeys() const { return focus_ != 0; }

    // ---- widgets
    void card(const RectF& r, float radius = 8.f, bool hoverable = false);
    void label(const std::wstring& s, const RectF& r, float size = 14.f, const Color* col = nullptr,
               Weight w = Weight::Regular, Align h = Align::Start, Align v = Align::Center, bool wrap = false);
    bool button(const std::string& id, const RectF& r, const std::wstring& text, ButtonKind k = ButtonKind::Standard,
                const wchar_t* glyph = nullptr, bool enabled = true);
    bool iconButton(const std::string& id, const RectF& r, const wchar_t* glyph, const std::wstring& tip = L"",
                    bool active = false);
    bool toggle(const std::string& id, const RectF& r, bool& v, bool showText = true);
    bool slider(const std::string& id, const RectF& r, float& v, float lo, float hi, float step,
                const std::function<std::wstring(float)>& fmt = {});
    bool rangeSlider(const std::string& id, const RectF& r, float& a, float& b, float lo, float hi, float step,
                     float marker = -1.f);
    bool segmented(const std::string& id, const RectF& r, int& sel, const std::vector<std::wstring>& items);
    bool combo(const std::string& id, const RectF& r, int& sel, const std::vector<std::wstring>& items);
    bool textField(const std::string& id, const RectF& r, std::wstring& text, const std::wstring& placeholder = L"",
                   size_t maxLen = 64);
    bool hotkeyField(const std::string& id, const RectF& r, uint32_t& packed);
    bool keysField(const std::string& id, const RectF& r, std::vector<uint16_t>& keys);
    bool navItem(const std::string& id, const RectF& r, const wchar_t* glyph, const std::wstring& text, bool selected);
    bool swatch(const std::string& id, Vec2 c, float radius, const Color& col, bool selected);
    void progressRing(Vec2 c, float r, float value, const Color& col, float width = 3.f);
    void spinner(Vec2 c, float r, const Color& col);
    void pill(const RectF& r, const std::wstring& text, const Color& col, bool filled = false);

    // ---- vertical scrolling region
    void beginScroll(const std::string& id, const RectF& viewport, float contentHeight);
    void endScroll();
    float scrollOffset(const std::string& id) const;

private:
    void drawPopups();
    RectF popupRect() const;
    void releaseFocusIfClickedOutside();

    Canvas* c_ = nullptr;
    const Palette* p_ = nullptr;
    Input* in_ = nullptr;
    double t_ = 0, dt_ = 0.016;
    std::vector<uint32_t> idStack_;
    std::unordered_map<uint32_t, float> anims_;
    bool animating_ = false;
    bool wantHand_ = false, wantText_ = false;

    uint32_t active_ = 0;   // widget being dragged
    uint32_t focus_ = 0;    // text / hotkey field with keyboard focus
    bool focusClaimed_ = false;
    size_t caret_ = 0;

    // Combo popup (drawn last, on top).
    static constexpr float kPopupItemH = 34.f;
    uint32_t popup_ = 0;
    RectF popupAnchor_;
    std::vector<std::wstring> popupItems_;
    int popupSel_ = -1;
    int popupResult_ = -1;
    uint32_t popupResultId_ = 0;
    bool popupJustOpened_ = false;
    uint32_t popupJustClosed_ = 0;  // combo whose popup the current click closed: don't reopen it
    float popupScroll_ = 0.f;        // long lists (gestures, actions, apps) scroll inside the popup

    // Tooltip.
    std::wstring tipText_;
    RectF tipRect_;
    uint32_t tipId_ = 0;
    double tipSince_ = 0;

    // Scroll.
    struct Scroll { float offset = 0, target = 0, content = 0, view = 0; };
    std::unordered_map<uint32_t, Scroll> scrolls_;
    std::vector<uint32_t> scrollStack_;
    std::vector<RectF> scrollViews_;
};

// Vertical layout cursor.
struct Column {
    float x, y, w, gap = 8.f;
    RectF next(float h) {
        RectF r{x, y, w, h};
        y += h + gap;
        return r;
    }
    void space(float h) { y += h; }
};

}  // namespace vl::ui
