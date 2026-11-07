// Shared plumbing for our Direct2D windows: Win32 input -> Gui::Input, DPI,
// Mica/Acrylic backdrop, dark title bar, paint-on-demand with animation timer.
#pragma once

#include "ui/gui.hpp"

namespace vl {
class App;
}

namespace vl::ui {

class GuiWindow {
public:
    explicit GuiWindow(App& app) : app_(app) {}
    virtual ~GuiWindow();
    GuiWindow(const GuiWindow&) = delete;
    GuiWindow& operator=(const GuiWindow&) = delete;

    HWND hwnd() const { return hwnd_; }
    bool visible() const { return hwnd_ && IsWindowVisible(hwnd_); }
    void invalidate() { if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE); }
    void destroy();
    void refreshTheme();  // dark title bar / backdrop after a settings change

protected:
    bool createWindow(const wchar_t* cls, const wchar_t* title, DWORD style, DWORD exStyle, int x, int y, int w, int h,
                      HWND owner);
    virtual void frame(Gui& g, const RectF& client) = 0;
    virtual LRESULT extra(UINT msg, WPARAM wp, LPARAM lp, bool& handled) {
        handled = false;
        return 0;
    }
    virtual int backdropType() const = 0;  // win::kBackdrop*
    virtual void onDestroyed() {}

    App& app_;
    HWND hwnd_ = nullptr;
    Canvas canvas_;
    Gui gui_;
    Input input_;
    bool backdropActive_ = false;

private:
    static LRESULT CALLBACK proc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(HWND, UINT, WPARAM, LPARAM);
    void paint();
    void applyBackdrop();
    bool tracking_ = false;
    bool timer_ = false;
};

}  // namespace vl::ui
