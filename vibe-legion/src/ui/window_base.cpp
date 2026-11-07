#include "ui/window_base.hpp"

#include <dwmapi.h>
#include <windowsx.h>

#include <chrono>

#include "app/app.hpp"

namespace vl::ui {

namespace {
constexpr UINT_PTR kAnimTimer = 1;

double nowSec() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}
}  // namespace

GuiWindow::~GuiWindow() { destroy(); }

void GuiWindow::destroy() {
    if (hwnd_) DestroyWindow(hwnd_);
    hwnd_ = nullptr;
}

bool GuiWindow::createWindow(const wchar_t* cls, const wchar_t* title, DWORD style, DWORD exStyle, int x, int y, int w,
                             int h, HWND owner) {
    static bool registered[8] = {};
    static const wchar_t* names[8] = {};
    int slot = 0;
    for (; slot < 8 && names[slot] && wcscmp(names[slot], cls) != 0; ++slot) {}
    if (slot < 8 && !registered[slot]) {
        WNDCLASSEXW wc{sizeof wc};
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = proc;
        wc.hInstance = app_.instance();
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = app_.appIcon(true);
        wc.hIconSm = app_.appIcon(false);
        wc.lpszClassName = cls;
        RegisterClassExW(&wc);
        registered[slot] = true;
        names[slot] = cls;
    }
    hwnd_ = CreateWindowExW(exStyle, cls, title, style, x, y, w, h, owner, nullptr, app_.instance(), this);
    if (!hwnd_) return false;
    canvas_.attach(hwnd_);
    applyBackdrop();
    return true;
}

void GuiWindow::refreshTheme() {
    applyBackdrop();
    invalidate();
}

void GuiWindow::applyBackdrop() {
    if (!hwnd_) return;
    const BOOL dark = app_.palette().dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd_, win::kDwmUseImmersiveDarkMode, &dark, sizeof dark);
    int corner = win::kCornerRound;
    DwmSetWindowAttribute(hwnd_, win::kDwmWindowCornerPreference, &corner, sizeof corner);
    int type = backdropType();
    if (type == win::kBackdropMica && !app_.micaWanted()) type = win::kBackdropNone;
    backdropActive_ = SUCCEEDED(DwmSetWindowAttribute(hwnd_, win::kDwmSystemBackdropType, &type, sizeof type)) &&
                      type != win::kBackdropNone;
    MARGINS m = backdropActive_ ? MARGINS{-1, -1, -1, -1} : MARGINS{0, 0, 0, 0};
    DwmExtendFrameIntoClientArea(hwnd_, &m);
}

LRESULT CALLBACK GuiWindow::proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    GuiWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        self = static_cast<GuiWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = h;
    } else {
        self = reinterpret_cast<GuiWindow*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    }
    if (!self) return DefWindowProcW(h, msg, wp, lp);
    if (msg == WM_NCDESTROY) SetWindowLongPtrW(h, GWLP_USERDATA, 0);
    return self->handle(h, msg, wp, lp);
}

void GuiWindow::paint() {
    PAINTSTRUCT ps;
    BeginPaint(hwnd_, &ps);
    EndPaint(hwnd_, &ps);
    if (!canvas_.begin()) return;
    const Palette& p = app_.palette();
    canvas_.clear(backdropActive_ ? Color{0, 0, 0, 0} : p.window);
    input_.ctrl = GetKeyState(VK_CONTROL) < 0;
    input_.shift = GetKeyState(VK_SHIFT) < 0;
    input_.alt = GetKeyState(VK_MENU) < 0;
    input_.win = GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0;
    gui_.begin(canvas_, p, input_, nowSec());
    frame(gui_, {0, 0, canvas_.width(), canvas_.height()});
    const bool anim = gui_.end();
    canvas_.end();
    input_.endFrame();
    if (anim && !timer_) {
        SetTimer(hwnd_, kAnimTimer, 15, nullptr);
        timer_ = true;
    } else if (!anim && timer_) {
        KillTimer(hwnd_, kAnimTimer);
        timer_ = false;
    }
}

LRESULT GuiWindow::handle(HWND self, UINT msg, WPARAM wp, LPARAM lp) {
    bool handled = false;
    LRESULT r = extra(msg, wp, lp, handled);
    if (handled) return r;
    const float scale = GetDpiForWindow(self) / 96.f;
    auto mousePos = [&] { input_.mouse = {GET_X_LPARAM(lp) / scale, GET_Y_LPARAM(lp) / scale}; };
    switch (msg) {
        case WM_PAINT:
            paint();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            canvas_.resize();
            invalidate();
            return 0;
        case WM_DPICHANGED: {
            const RECT* rc = reinterpret_cast<RECT*>(lp);
            SetWindowPos(hwnd_, nullptr, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            invalidate();
            return 0;
        }
        case WM_TIMER:
            if (wp == kAnimTimer) {
                invalidate();
                return 0;
            }
            break;
        case WM_MOUSEMOVE:
            mousePos();
            if (!tracking_) {
                TRACKMOUSEEVENT t{sizeof t, TME_LEAVE, hwnd_, 0};
                TrackMouseEvent(&t);
                tracking_ = true;
            }
            invalidate();
            return 0;
        case WM_MOUSELEAVE:
            tracking_ = false;
            if (!input_.down) input_.mouse = {-1000, -1000};
            invalidate();
            return 0;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            mousePos();
            input_.down = true;
            input_.pressed = true;
            input_.doubleClick = msg == WM_LBUTTONDBLCLK;
            SetCapture(hwnd_);
            invalidate();
            return 0;
        case WM_LBUTTONUP:
            mousePos();
            input_.down = false;
            input_.released = true;
            ReleaseCapture();
            invalidate();
            return 0;
        case WM_RBUTTONDOWN:
            mousePos();
            input_.rightPressed = true;
            invalidate();
            return 0;
        case WM_MOUSEWHEEL:
            input_.wheel += GET_WHEEL_DELTA_WPARAM(wp) / static_cast<float>(WHEEL_DELTA);
            invalidate();
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            input_.keys.push_back(static_cast<uint32_t>(wp));
            invalidate();
            if (msg == WM_SYSKEYDOWN && !gui_.capturingKeys()) break;  // keep Alt+F4 etc.
            return 0;
        case WM_CHAR:
            input_.chars.push_back(static_cast<wchar_t>(wp));
            invalidate();
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {
                SetCursor(LoadCursorW(nullptr, gui_.wantsTextCursor() ? IDC_IBEAM
                                                : gui_.wantsHandCursor() ? IDC_HAND
                                                                          : IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_SETTINGCHANGE:
        case WM_DWMCOLORIZATIONCOLORCHANGED:
            applyBackdrop();
            invalidate();
            break;
        case WM_DESTROY:
            KillTimer(hwnd_, kAnimTimer);
            timer_ = false;
            canvas_.detach();
            hwnd_ = nullptr;
            onDestroyed();
            return 0;
        default:
            break;
    }
    return DefWindowProcW(self, msg, wp, lp);
}

}  // namespace vl::ui
