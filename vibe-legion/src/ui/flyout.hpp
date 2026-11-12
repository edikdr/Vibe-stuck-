// Tray flyout (left click on the icon next to the clock): Windows-11 quick-settings
// style panel with live status, the big on/off switch and four quick tiles.
#pragma once

#include "ui/window_base.hpp"

namespace vl::ui {

class Flyout final : public GuiWindow {
public:
    using GuiWindow::GuiWindow;
    void toggle(const RECT& anchor);
    void hide();

protected:
    void frame(Gui& g, const RectF& client) override;
    LRESULT extra(UINT msg, WPARAM wp, LPARAM lp, bool& handled) override;
    int backdropType() const override;

private:
    DWORD hiddenAt_ = 0;
};

// Small on-screen confirmation above the taskbar ("✓ Пауза / воспроизведение")
// and a hold-progress ring while a gesture is charging.
class Osd final : public GuiWindow {
public:
    using GuiWindow::GuiWindow;
    // holdMs: how long it stays (longer for "listening for a voice command").
    void showFired(const std::wstring& glyph, const std::wstring& title, const std::wstring& detail, int holdMs = 1400);
    void showProgress(const std::wstring& title, float progress);
    void hideProgress();

protected:
    void frame(Gui& g, const RectF& client) override;
    LRESULT extra(UINT msg, WPARAM wp, LPARAM lp, bool& handled) override;
    int backdropType() const override;

private:
    void ensure();
    void place();
    std::wstring glyph_, title_, detail_;
    float progress_ = -1.f;
    bool fired_ = false;
    DWORD shownAt_ = 0;
    DWORD holdMs_ = 1400;
};

}  // namespace vl::ui
