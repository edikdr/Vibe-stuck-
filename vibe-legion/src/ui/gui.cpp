#include "ui/gui.hpp"

#include <algorithm>
#include <cmath>

#include "platform/win/system.hpp"

namespace vl::ui {

namespace {

uint32_t fnv1a(const std::string& s, uint32_t seed) {
    uint32_t h = seed ^ 2166136261u;
    for (unsigned char ch : s) {
        h ^= ch;
        h *= 16777619u;
    }
    return h ? h : 1;
}

std::wstring clipboardText() {
    std::wstring out;
    if (!OpenClipboard(nullptr)) return out;
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        if (auto* p = static_cast<const wchar_t*>(GlobalLock(h))) {
            out = p;
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return out;
}

bool isModifier(uint32_t vk) {
    return vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN || vk == VK_LCONTROL ||
           vk == VK_RCONTROL || vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_LMENU || vk == VK_RMENU;
}

}  // namespace

void Gui::begin(Canvas& c, const Palette& p, Input& in, double t) {
    c_ = &c;
    p_ = &p;
    in_ = &in;
    dt_ = t_ > 0 ? std::clamp(t - t_, 0.001, 0.1) : 0.016;
    t_ = t;
    idStack_.clear();
    animating_ = false;
    wantHand_ = wantText_ = false;
    focusClaimed_ = false;
    tipText_.clear();
    // Note: `active_` must survive from the button-down frame to the button-up
    // frame (Windows repaints in between); it is cleared in end().
}

bool Gui::end() {
    drawPopups();
    if (in_->pressed && !focusClaimed_) focus_ = 0;
    if (!in_->down) active_ = 0;  // after widgets have seen this frame's release
    if (in_->released) popupJustClosed_ = 0;
    // Tooltip after a short dwell.
    if (!tipText_.empty()) {
        if (tipSince_ == 0) tipSince_ = t_;
        if (t_ - tipSince_ > 0.55) {
            const Vec2 sz = c_->measure(tipText_, 12.f, Weight::Regular, 320.f, true);
            RectF r{in_->mouse.x + 12, in_->mouse.y + 18, sz.x + 18, sz.y + 10};
            if (r.right() > c_->width() - 4) r.x = c_->width() - 4 - r.w;
            if (r.bottom() > c_->height() - 4) r.y = in_->mouse.y - r.h - 6;
            c_->fillRound(r, 5, p_->overlay);
            c_->strokeRound(r, 5, p_->controlStroke);
            c_->text(tipText_, {r.x + 9, r.y + 5, r.w - 18, r.h - 10}, p_->text, 12.f, Weight::Regular, Align::Start,
                     Align::Center, true);
        } else {
            animating_ = true;
        }
    } else {
        tipSince_ = 0;
    }
    if (focus_) animating_ = true;  // caret blink
    return animating_;
}

uint32_t Gui::id(const std::string& s) const { return fnv1a(s, idStack_.empty() ? 0 : idStack_.back()); }
void Gui::pushId(const std::string& s) { idStack_.push_back(id(s)); }
void Gui::popId() { if (!idStack_.empty()) idStack_.pop_back(); }

bool Gui::hovered(const RectF& r) const {
    Vec2 m = in_->mouse;
    if (!scrollViews_.empty() && !scrollViews_.back().contains(m)) return false;
    if (popup_ && popupRect().contains(m)) return false;  // widgets under an open popup are inert
    m.y += scrollOffset("");                                // content space inside a scroll region
    return r.contains(m);
}

float Gui::anim(uint32_t key, float target, float speed) {
    auto it = anims_.find(key);
    if (it == anims_.end()) {
        anims_[key] = target;
        return target;
    }
    float& v = it->second;
    v += (target - v) * static_cast<float>(1.0 - std::exp(-speed * dt_));
    if (std::fabs(target - v) > 0.002f) animating_ = true;
    else v = target;
    return v;
}

void Gui::tooltip(const RectF& r, const std::wstring& text) {
    if (!text.empty() && hovered(r)) tipText_ = text;
}

float Gui::scrollOffset(const std::string&) const {
    if (scrollStack_.empty()) return 0.f;
    auto it = scrolls_.find(scrollStack_.back());
    return it == scrolls_.end() ? 0.f : it->second.offset;
}

void Gui::beginScroll(const std::string& sid, const RectF& view, float content) {
    const uint32_t k = id(sid);
    Scroll& s = scrolls_[k];
    s.content = content;
    s.view = view.h;
    const float maxOff = std::max(0.f, content - view.h);
    if (view.contains(in_->mouse) && in_->wheel != 0.f && !popup_) s.target -= in_->wheel * 90.f;
    s.target = std::clamp(s.target, 0.f, maxOff);
    s.offset += (s.target - s.offset) * static_cast<float>(1.0 - std::exp(-18.0 * dt_));
    if (std::fabs(s.target - s.offset) > 0.3f) animating_ = true;
    else s.offset = s.target;
    s.offset = std::clamp(s.offset, 0.f, maxOff);
    c_->pushClip(view);
    c_->rt()->SetTransform(D2D1::Matrix3x2F::Translation(0, -s.offset));
    scrollStack_.push_back(k);
    scrollViews_.push_back(view);
}

void Gui::endScroll() {
    if (scrollStack_.empty()) return;
    const Scroll& s = scrolls_[scrollStack_.back()];
    const RectF view = scrollViews_.back();
    c_->rt()->SetTransform(D2D1::Matrix3x2F::Identity());
    c_->popClip();
    scrollStack_.pop_back();
    scrollViews_.pop_back();
    if (s.content > s.view + 1) {
        const float frac = s.view / s.content;
        const float barH = std::max(30.f, view.h * frac);
        const float y = view.y + (view.h - barH) * (s.offset / (s.content - s.view));
        const float w = view.contains(in_->mouse) ? 4.f : 2.f;
        c_->fillRound({view.right() - w - 3, y + 2, w, barH - 4}, w / 2, p_->textMuted.alpha(0.6f));
    }
}

// --------------------------------------------------------------------- widgets

void Gui::card(const RectF& r, float radius, bool hoverable) {
    const bool h = hoverable && hovered(r);
    c_->fillRound(r, radius, h ? p_->cardHover : p_->card);
    c_->strokeRound(r, radius, p_->cardStroke);
}

void Gui::label(const std::wstring& s, const RectF& r, float size, const Color* col, Weight w, Align h, Align v,
                bool wrap) {
    c_->text(s, r, col ? *col : p_->text, size, w, h, v, wrap);
}

bool Gui::button(const std::string& sid, const RectF& r, const std::wstring& text, ButtonKind k, const wchar_t* g,
                 bool enabled) {
    const uint32_t k_ = id(sid);
    const bool h = enabled && hovered(r);
    if (h) wantHand_ = true;
    if (h && in_->pressed) { active_ = k_; focusClaimed_ = true; }
    const bool pressedNow = active_ == k_ && in_->down && h;
    const bool clicked = enabled && h && in_->released && active_ == k_;
    Color bg, fg = p_->text, stroke = p_->controlStroke;
    switch (k) {
        case ButtonKind::Accent:
            bg = pressedNow ? p_->accent.alpha(0.85f) : h ? p_->accentHover : p_->accent;
            fg = p_->onAccent;
            stroke = Color{0, 0, 0, 0};
            break;
        case ButtonKind::Subtle:
            bg = pressedNow ? p_->controlPressed : h ? p_->controlHover : Color{0, 0, 0, 0};
            stroke = Color{0, 0, 0, 0};
            break;
        case ButtonKind::Danger:
            bg = pressedNow ? p_->controlPressed : h ? p_->danger.alpha(0.16f) : p_->control;
            fg = p_->danger;
            break;
        default:
            bg = pressedNow ? p_->controlPressed : h ? p_->controlHover : p_->control;
    }
    if (!enabled) fg = p_->textMuted;
    c_->fillRound(r, 5, bg);
    if (stroke.a > 0) c_->strokeRound(r, 5, stroke);
    float tw = c_->measure(text, 13.5f, Weight::Medium).x;
    float gw = g ? 22.f : 0.f;
    float x = r.x + (r.w - tw - gw) * 0.5f;
    if (g) c_->icon(g, {x, r.y, 16, r.h}, fg, 14.f);
    c_->text(text, {x + gw, r.y, tw + 2, r.h}, fg, 13.5f, Weight::Medium, Align::Start, Align::Center);
    return clicked;
}

bool Gui::iconButton(const std::string& sid, const RectF& r, const wchar_t* g, const std::wstring& tip, bool activeLook) {
    const uint32_t k = id(sid);
    const bool h = hovered(r);
    if (h) wantHand_ = true;
    if (h && in_->pressed) { active_ = k; focusClaimed_ = true; }
    const bool clicked = h && in_->released && active_ == k;
    const float a = anim(k, h ? 1.f : 0.f, 20.f);
    if (activeLook) c_->fillRound(r, 5, p_->accent.alpha(0.18f));
    if (a > 0.01f) c_->fillRound(r, 5, p_->controlHover.alpha(p_->controlHover.a * a * 1.4f));
    c_->icon(g, r, activeLook ? p_->accent : p_->text, 15.f);
    tooltip(r, tip);
    return clicked;
}

bool Gui::toggle(const std::string& sid, const RectF& r, bool& v, bool showText) {
    const uint32_t k = id(sid);
    const RectF sw{r.right() - 40, r.y + (r.h - 20) * 0.5f, 40, 20};
    const RectF hit = showText ? RectF{r.right() - 100, r.y, 100, r.h} : sw;
    const bool h = hovered(hit);
    if (h) wantHand_ = true;
    bool changed = false;
    if (h && in_->pressed) { active_ = k; focusClaimed_ = true; }
    if (h && in_->released && active_ == k) {
        v = !v;
        changed = true;
    }
    const float t = anim(k, v ? 1.f : 0.f, 18.f);
    const Color fill = v ? (h ? p_->accentHover : p_->accent) : (h ? p_->controlHover : Color{0, 0, 0, 0});
    c_->fillRound(sw, 10, fill);
    if (!v) c_->strokeRound(sw, 10, p_->textDim, 1.f);
    const float knobR = h ? 7.f : 6.f;
    const float kx = sw.x + 10 + t * 20;
    c_->fillCircle({kx, sw.y + 10}, knobR, v ? p_->onAccent : p_->textDim);
    if (showText)
        c_->text(v ? L"Вкл." : L"Откл.", {sw.x - 56, r.y, 48, r.h}, p_->text, 13.5f, Weight::Regular, Align::End);
    return changed;
}

bool Gui::slider(const std::string& sid, const RectF& r, float& v, float lo, float hi, float step,
                 const std::function<std::wstring(float)>& fmt) {
    const uint32_t k = id(sid);
    const float valueW = fmt ? 72.f : 0.f;
    const RectF track{r.x + 8, r.y + r.h * 0.5f - 2, r.w - 16 - valueW, 4};
    const RectF hit{r.x, r.y, r.w - valueW, r.h};
    const bool h = hovered(hit);
    if (h) wantHand_ = true;
    if (h && in_->pressed) { active_ = k; focusClaimed_ = true; }
    bool changed = false;
    if (active_ == k && in_->down) {
        float t = std::clamp((in_->mouse.x - track.x) / track.w, 0.f, 1.f);
        float nv = lo + t * (hi - lo);
        if (step > 0) nv = std::round(nv / step) * step;
        nv = std::clamp(nv, lo, hi);
        if (nv != v) { v = nv; changed = true; }
    }
    if (h && in_->wheel != 0.f && step > 0 && scrollStack_.empty()) {
        v = std::clamp(v + (in_->wheel > 0 ? step : -step), lo, hi);
        changed = true;
    }
    const float t = hi > lo ? std::clamp((v - lo) / (hi - lo), 0.f, 1.f) : 0.f;
    c_->fillRound(track, 2, p_->textMuted.alpha(0.45f));
    c_->fillRound({track.x, track.y, track.w * t, 4}, 2, p_->accent);
    const Vec2 th{track.x + track.w * t, track.y + 2};
    c_->fillCircle(th, 10, p_->dark ? Color::hex(0x454A53) : Color::hex(0xFFFFFF));
    c_->strokeCircle(th, 10, p_->controlStroke, 1);
    const float inner = anim(k, active_ == k ? 5.f : (h ? 7.f : 6.f), 20.f);
    c_->fillCircle(th, inner, p_->accent);
    if (fmt) c_->text(fmt(v), {r.right() - valueW, r.y, valueW, r.h}, p_->textDim, 13.f, Weight::Regular, Align::End);
    return changed;
}

bool Gui::rangeSlider(const std::string& sid, const RectF& r, float& a, float& b, float lo, float hi, float step,
                      float marker) {
    const uint32_t ka = id(sid + "#a"), kb = id(sid + "#b");
    const RectF track{r.x + 10, r.y + r.h * 0.5f - 2, r.w - 20, 4};
    auto toX = [&](float v) { return track.x + track.w * std::clamp((v - lo) / (hi - lo), 0.f, 1.f); };
    const bool h = hovered(r);
    if (h) wantHand_ = true;
    if (h && in_->pressed) {
        focusClaimed_ = true;
        active_ = std::fabs(in_->mouse.x - toX(a)) <= std::fabs(in_->mouse.x - toX(b)) ? ka : kb;
    }
    bool changed = false;
    if ((active_ == ka || active_ == kb) && in_->down) {
        float t = std::clamp((in_->mouse.x - track.x) / track.w, 0.f, 1.f);
        float nv = lo + t * (hi - lo);
        if (step > 0) nv = std::round(nv / step) * step;
        if (active_ == ka) nv = std::min(nv, b - step);
        else nv = std::max(nv, a + step);
        float& tgt = active_ == ka ? a : b;
        if (nv != tgt) { tgt = std::clamp(nv, lo, hi); changed = true; }
    }
    c_->fillRound(track, 2, p_->textMuted.alpha(0.45f));
    c_->fillRound({toX(a), track.y, toX(b) - toX(a), 4}, 2, p_->accent);
    if (marker >= lo && marker <= hi) {
        const float mx = toX(marker);
        const bool inside = marker >= a && marker <= b;
        c_->fillRound({mx - 1.5f, track.y - 9, 3, 22}, 1.5f, inside ? p_->success : p_->warning);
    }
    for (int i = 0; i < 2; ++i) {
        const Vec2 th{toX(i ? b : a), track.y + 2};
        c_->fillCircle(th, 10, p_->dark ? Color::hex(0x454A53) : Color::hex(0xFFFFFF));
        c_->strokeCircle(th, 10, p_->controlStroke, 1);
        c_->fillCircle(th, active_ == (i ? kb : ka) ? 5.f : 6.f, p_->accent);
    }
    return changed;
}

bool Gui::segmented(const std::string& sid, const RectF& r, int& sel, const std::vector<std::wstring>& items) {
    if (items.empty()) return false;
    const uint32_t k = id(sid);
    c_->fillRound(r, 6, p_->control);
    c_->strokeRound(r, 6, p_->controlStroke);
    const float w = r.w / items.size();
    const float pos = anim(k, static_cast<float>(std::clamp(sel, 0, static_cast<int>(items.size()) - 1)), 16.f);
    c_->fillRound({r.x + 3 + pos * w, r.y + 3, w - 6, r.h - 6}, 4, p_->accent);
    bool changed = false;
    for (size_t i = 0; i < items.size(); ++i) {
        const RectF cell{r.x + i * w, r.y, w, r.h};
        const bool h = hovered(cell);
        if (h) wantHand_ = true;
        if (h && in_->pressed) { active_ = id(sid + "#" + std::to_string(i)); focusClaimed_ = true; }
        if (h && in_->released && active_ == id(sid + "#" + std::to_string(i)) && sel != static_cast<int>(i)) {
            sel = static_cast<int>(i);
            changed = true;
        }
        const bool on = std::fabs(pos - i) < 0.5f;
        if (h && !on) c_->fillRound({cell.x + 3, cell.y + 3, cell.w - 6, cell.h - 6}, 4, p_->controlHover);
        c_->text(items[i], {cell.x + 4, cell.y, cell.w - 8, cell.h}, on ? p_->onAccent : p_->text, 13.f,
                 on ? Weight::Semibold : Weight::Regular, Align::Center);
    }
    return changed;
}

bool Gui::combo(const std::string& sid, const RectF& r, int& sel, const std::vector<std::wstring>& items) {
    const uint32_t k = id(sid);
    bool changed = false;
    if (popupResultId_ == k) {
        if (popupResult_ >= 0 && popupResult_ != sel) {
            sel = popupResult_;
            changed = true;
        }
        popupResultId_ = 0;
        popupResult_ = -1;
    }
    const bool h = hovered(r);
    if (h) wantHand_ = true;
    if (h && in_->pressed) { active_ = k; focusClaimed_ = true; }
    if (h && in_->released && active_ == k && popup_ != k && popupJustClosed_ != k) {
        popup_ = k;
        // Popup position in window coordinates.
        popupAnchor_ = {r.x, r.y - scrollOffset(""), r.w, r.h};
        popupItems_ = items;
        popupSel_ = sel;
        popupJustOpened_ = true;
        // Open scrolled so that the current choice is visible (centred when possible).
        const RectF pr = popupRect();
        const float maxScroll = std::max(0.f, items.size() * kPopupItemH + 8.f - pr.h);
        popupScroll_ = std::clamp(sel * kPopupItemH - (pr.h - kPopupItemH) * 0.5f, 0.f, maxScroll);
    }
    c_->fillRound(r, 5, h ? p_->controlHover : p_->control);
    c_->strokeRound(r, 5, p_->controlStroke);
    const std::wstring cur = sel >= 0 && sel < static_cast<int>(items.size()) ? items[sel] : L"—";
    c_->text(cur, {r.x + 11, r.y, r.w - 40, r.h}, p_->text, 13.5f);
    c_->icon(glyph::ChevronDown, {r.right() - 30, r.y, 20, r.h}, p_->textDim, 11.f);
    return changed;
}

RectF Gui::popupRect() const {
    // As tall as the list, but never taller than the window: below or above the
    // combo, whichever has more room. Longer lists scroll (popupScroll_).
    const float full = popupItems_.size() * kPopupItemH + 8.f;
    const float winH = c_ ? c_->height() : 740.f;
    const float below = winH - popupAnchor_.bottom() - 12.f, above = popupAnchor_.y - 12.f;
    const float cap = std::min(420.f, std::max(below, above));
    const float h = std::max(std::min(full, cap), std::min(full, 3 * kPopupItemH + 8.f));
    RectF pr{popupAnchor_.x, popupAnchor_.bottom() + 4, std::max(popupAnchor_.w, 180.f), h};
    if (pr.bottom() > winH - 8) pr.y = popupAnchor_.y - h - 4;  // open upwards
    if (pr.y < 4) pr.y = 4;
    return pr;
}

void Gui::drawPopups() {
    if (!popup_) return;
    const float itemH = kPopupItemH;
    const RectF pr = popupRect();
    const float maxScroll = std::max(0.f, popupItems_.size() * itemH + 8.f - pr.h);
    if (pr.contains(in_->mouse) && in_->wheel != 0.f) popupScroll_ -= in_->wheel * 3.f * itemH;
    popupScroll_ = std::clamp(popupScroll_, 0.f, maxScroll);
    c_->fillRound({pr.x, pr.y + 2, pr.w, pr.h}, 8, Color{0, 0, 0, 0.25f});  // soft shadow
    c_->fillRound(pr, 8, p_->overlay);
    c_->strokeRound(pr, 8, p_->controlStroke);
    c_->pushClip({pr.x + 1, pr.y + 3, pr.w - 2, pr.h - 6});
    const size_t first = static_cast<size_t>(popupScroll_ / itemH);
    for (size_t i = first; i < popupItems_.size(); ++i) {
        const RectF it{pr.x + 4, pr.y + 4 + i * itemH - popupScroll_, pr.w - 8 - (maxScroll > 0 ? 6 : 0), itemH - 2};
        if (it.y > pr.bottom()) break;
        const bool h = it.contains(in_->mouse) && pr.contains(in_->mouse);
        if (h) wantHand_ = true;
        if (h || static_cast<int>(i) == popupSel_) c_->fillRound(it, 5, h ? p_->controlHover : p_->control);
        if (static_cast<int>(i) == popupSel_) c_->fillRound({it.x, it.y + 9, 3, it.h - 18}, 1.5f, p_->accent);
        c_->text(popupItems_[i], {it.x + 12, it.y, it.w - 16, it.h}, p_->text, 13.5f);
        if (h && in_->released && !popupJustOpened_) {
            popupResult_ = static_cast<int>(i);
            popupResultId_ = popup_;
            popup_ = 0;
            animating_ = true;  // apply the result on the next frame
            focusClaimed_ = true;
        }
    }
    c_->popClip();
    if (maxScroll > 0) {  // scroll bar
        const float barH = std::max(24.f, (pr.h - 8) * (pr.h / (popupItems_.size() * itemH + 8.f)));
        const float y = pr.y + 4 + (pr.h - 8 - barH) * (popupScroll_ / maxScroll);
        c_->fillRound({pr.right() - 7, y, 3, barH}, 1.5f, p_->textMuted.alpha(0.7f));
    }
    if (popup_ && !popupJustOpened_ && in_->pressed && !pr.contains(in_->mouse)) {
        popupJustClosed_ = popup_;  // a click on the combo itself closes, not re-opens
        popup_ = 0;
        focusClaimed_ = true;
    }
    if (popup_ && !popupJustOpened_)
        for (auto k : in_->keys)
            if (k == VK_ESCAPE) popup_ = 0;
    if (in_->released) popupJustOpened_ = false;
    if (popup_) focusClaimed_ = focusClaimed_ || in_->pressed;
}

bool Gui::textField(const std::string& sid, const RectF& r, std::wstring& text, const std::wstring& placeholder,
                    size_t maxLen) {
    const uint32_t k = id(sid);
    const bool h = hovered(r);
    if (h) wantText_ = true;
    if (h && in_->pressed) {
        focus_ = k;
        focusClaimed_ = true;
    }
    bool changed = false;
    const bool focused = focus_ == k;
    if (focused) {
        if (in_->pressed && h) focusClaimed_ = true;
        for (wchar_t ch : in_->chars) {
            if (ch >= 32 && ch != 127 && text.size() < maxLen) {
                text.push_back(ch);
                changed = true;
            }
        }
        for (auto vk : in_->keys) {
            if (vk == VK_BACK && !text.empty()) {
                if (in_->ctrl) text.clear();
                else text.pop_back();
                changed = true;
            } else if (vk == VK_RETURN || vk == VK_ESCAPE || vk == VK_TAB) {
                focus_ = 0;
            } else if (vk == 'V' && in_->ctrl) {
                for (wchar_t ch : clipboardText())
                    if (ch >= 32 && text.size() < maxLen) text.push_back(ch);
                changed = true;
            }
        }
    }
    c_->fillRound(r, 5, focused ? (p_->dark ? Color::hex(0x1E2128) : Color::hex(0xFFFFFF)) : (h ? p_->controlHover : p_->control));
    c_->strokeRound(r, 5, p_->controlStroke);
    if (focused) c_->fillRound({r.x + 1, r.bottom() - 2, r.w - 2, 2}, 1, p_->accent);
    const RectF tr{r.x + 11, r.y, r.w - 22, r.h};
    if (text.empty() && !focused) c_->text(placeholder, tr, p_->textMuted, 13.5f);
    else c_->text(text, tr, p_->text, 13.5f);
    if (focused && std::fmod(t_, 1.0) < 0.55) {
        const float tw = std::min(c_->measure(text, 13.5f).x, tr.w - 2);
        c_->fillRect({tr.x + tw + 1, r.y + 8, 1.2f, r.h - 16}, p_->text);
    }
    return changed;
}

bool Gui::hotkeyField(const std::string& sid, const RectF& r, uint32_t& packed) {
    const uint32_t k = id(sid);
    const bool h = hovered(r);
    if (h) wantHand_ = true;
    if (h && in_->pressed) {
        focus_ = k;
        focusClaimed_ = true;
    }
    bool changed = false;
    const bool focused = focus_ == k;
    if (focused) {
        for (auto vk : in_->keys) {
            if (vk == VK_ESCAPE) { focus_ = 0; break; }
            if (vk == VK_BACK || vk == VK_DELETE) { packed = 0; changed = true; focus_ = 0; break; }
            if (isModifier(vk)) continue;
            uint32_t mods = 0;
            if (in_->ctrl) mods |= MOD_CONTROL;
            if (in_->alt) mods |= MOD_ALT;
            if (in_->shift) mods |= MOD_SHIFT;
            if (in_->win) mods |= MOD_WIN;
            if (!mods && !(vk >= VK_F1 && vk <= VK_F24)) continue;  // require a modifier for letter keys
            packed = packHotkey(mods, vk);
            changed = true;
            focus_ = 0;
            break;
        }
    }
    c_->fillRound(r, 5, focused ? p_->accent.alpha(0.18f) : (h ? p_->controlHover : p_->control));
    c_->strokeRound(r, 5, focused ? p_->accent : p_->controlStroke);
    c_->icon(glyph::Keyboard, {r.x + 8, r.y, 18, r.h}, p_->textDim, 13.f);
    c_->text(focused ? L"Нажмите сочетание… (Esc — отмена)" : win::hotkeyText(packed), {r.x + 32, r.y, r.w - 40, r.h},
             focused ? p_->accent : p_->text, 13.5f);
    return changed;
}

bool Gui::keysField(const std::string& sid, const RectF& r, std::vector<uint16_t>& keys) {
    const uint32_t k = id(sid);
    const bool h = hovered(r);
    if (h) wantHand_ = true;
    if (h && in_->pressed) {
        focus_ = k;
        focusClaimed_ = true;
    }
    bool changed = false;
    const bool focused = focus_ == k;
    if (focused) {
        for (auto vk : in_->keys) {
            if (vk == VK_ESCAPE && !in_->ctrl && !in_->alt && !in_->shift) { focus_ = 0; break; }
            if (isModifier(vk)) continue;
            keys.clear();
            if (in_->ctrl) keys.push_back(VK_CONTROL);
            if (in_->alt) keys.push_back(VK_MENU);
            if (in_->shift) keys.push_back(VK_SHIFT);
            if (in_->win) keys.push_back(VK_LWIN);
            keys.push_back(static_cast<uint16_t>(vk));
            changed = true;
            focus_ = 0;
            break;
        }
    }
    std::wstring s;
    for (size_t i = 0; i < keys.size(); ++i) s += (i ? L" + " : L"") + win::vkName(keys[i]);
    if (s.empty()) s = L"Не задано";
    c_->fillRound(r, 5, focused ? p_->accent.alpha(0.18f) : (h ? p_->controlHover : p_->control));
    c_->strokeRound(r, 5, focused ? p_->accent : p_->controlStroke);
    c_->icon(glyph::Keyboard, {r.x + 8, r.y, 18, r.h}, p_->textDim, 13.f);
    c_->text(focused ? L"Нажмите клавиши…" : s, {r.x + 32, r.y, r.w - 40, r.h}, focused ? p_->accent : p_->text, 13.5f);
    return changed;
}

bool Gui::navItem(const std::string& sid, const RectF& r, const wchar_t* g, const std::wstring& text, bool selected) {
    const uint32_t k = id(sid);
    const bool h = hovered(r);
    if (h) wantHand_ = true;
    if (h && in_->pressed) { active_ = k; focusClaimed_ = true; }
    const bool clicked = h && in_->released && active_ == k;
    const float a = anim(k, selected ? 1.f : (h ? 0.6f : 0.f), 18.f);
    if (a > 0.01f) c_->fillRound(r, 6, p_->controlHover.alpha(p_->controlHover.a * std::max(a, 0.6f) * (selected ? 1.3f : a)));
    const float bar = anim(id(sid + "#bar"), selected ? 16.f : 0.f, 16.f);
    if (bar > 0.5f) c_->fillRound({r.x + 1, r.y + (r.h - bar) * 0.5f, 3, bar}, 1.5f, p_->accent);
    c_->icon(g, {r.x + 12, r.y, 20, r.h}, selected ? p_->accent : p_->text, 16.f);
    c_->text(text, {r.x + 44, r.y, r.w - 48, r.h}, p_->text, 14.f, selected ? Weight::Semibold : Weight::Regular);
    return clicked;
}

bool Gui::swatch(const std::string& sid, Vec2 c, float radius, const Color& col, bool selected) {
    const uint32_t k = id(sid);
    const RectF r{c.x - radius, c.y - radius, radius * 2, radius * 2};
    const bool h = hovered(r);
    if (h) wantHand_ = true;
    if (h && in_->pressed) { active_ = k; focusClaimed_ = true; }
    const bool clicked = h && in_->released && active_ == k;
    if (selected) c_->strokeCircle(c, radius + 3.5f, p_->text, 2.f);
    c_->fillCircle(c, h ? radius + 1 : radius, col);
    return clicked;
}

void Gui::progressRing(Vec2 c, float r, float v, const Color& col, float width) {
    c_->strokeCircle(c, r, p_->textMuted.alpha(0.3f), width);
    if (v > 0.f) c_->arc(c, r, -kPi / 2, 2 * kPi * std::clamp(v, 0.f, 1.f), col, width);
}

void Gui::spinner(Vec2 c, float r, const Color& col) {
    animating_ = true;
    const float s = static_cast<float>(std::fmod(t_ * 5.0, 2 * kPi));
    const float sweep = 1.2f + 0.9f * static_cast<float>(std::sin(t_ * 2.3));
    c_->arc(c, r, s, sweep, col, 2.5f);
}

void Gui::pill(const RectF& r, const std::wstring& text, const Color& col, bool filled) {
    c_->fillRound(r, r.h / 2, filled ? col : col.alpha(0.16f));
    c_->text(text, r, filled ? p_->onAccent : col, 12.f, Weight::Semibold, Align::Center);
}

}  // namespace vl::ui
