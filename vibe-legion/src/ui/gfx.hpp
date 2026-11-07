// Direct2D / DirectWrite drawing surface for one window. All coordinates are DIPs.
#pragma once

#include <d2d1.h>
#include <dwrite.h>

#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "core/geometry.hpp"
#include "platform/win/win_common.hpp"
#include "ui/theme.hpp"

namespace vl::ui {

using win::ComPtr;

enum class Align { Start, Center, End };
enum class Weight { Regular = 400, Medium = 500, Semibold = 600, Bold = 700 };

class Gfx {
public:
    static Gfx& get();
    ID2D1Factory* d2d() const { return d2d_.Get(); }
    IDWriteFactory* dwrite() const { return dw_.Get(); }
    IDWriteTextFormat* format(float size, Weight w, bool icon, bool wrap, Align h, Align v);
    const wchar_t* iconFamily() const { return iconFamily_.c_str(); }
    const wchar_t* textFamily() const { return textFamily_.c_str(); }

private:
    Gfx();
    ComPtr<ID2D1Factory> d2d_;
    ComPtr<IDWriteFactory> dw_;
    std::wstring textFamily_, iconFamily_;
    std::map<std::tuple<int, int, bool, bool, int, int>, ComPtr<IDWriteTextFormat>> formats_;
};

class Canvas {
public:
    bool attach(HWND hwnd);
    void detach();
    void resize();
    bool begin();      // false if the target could not be created
    void end();        // presents; recreates the target on device loss
    float dpiScale() const { return dpi_ / 96.f; }
    float width() const { return w_; }
    float height() const { return h_; }
    ID2D1RenderTarget* rt() const { return rt_.Get(); }

    void clear(const Color& c);
    void fillRect(const RectF& r, const Color& c);
    void fillRound(const RectF& r, float radius, const Color& c);
    void strokeRound(const RectF& r, float radius, const Color& c, float width = 1.f);
    void fillCircle(Vec2 c, float radius, const Color& col);
    void strokeCircle(Vec2 c, float radius, const Color& col, float width);
    void line(Vec2 a, Vec2 b, const Color& c, float width = 1.f);
    void arc(Vec2 c, float radius, float startRad, float sweepRad, const Color& col, float width);
    void text(const std::wstring& s, const RectF& r, const Color& c, float size = 14.f, Weight w = Weight::Regular,
              Align h = Align::Start, Align v = Align::Center, bool wrap = false);
    void icon(const wchar_t* glyph, const RectF& r, const Color& c, float size = 16.f, Align h = Align::Center);
    Vec2 measure(const std::wstring& s, float size, Weight w = Weight::Regular, float maxWidth = 10000.f, bool wrap = false);

    // Camera preview bitmap (BGRA, opaque). Recreated only when the size changes.
    void updateBitmap(const uint8_t* bgra, int w, int h);
    void drawBitmap(const RectF& dst, float opacity = 1.f, bool flipX = false);
    bool hasBitmap() const { return bmp_ != nullptr; }

    void pushClip(const RectF& r);
    void popClip();
    void pushRoundClip(const RectF& r, float radius);
    void popRoundClip();

private:
    void brush(const Color& c);
    HWND hwnd_ = nullptr;
    ComPtr<ID2D1HwndRenderTarget> rt_;
    ComPtr<ID2D1SolidColorBrush> brush_;
    ComPtr<ID2D1Bitmap> bmp_;
    int bmpW_ = 0, bmpH_ = 0;
    std::vector<ComPtr<ID2D1Layer>> layers_;
    float dpi_ = 96.f, w_ = 0, h_ = 0;
};

// Segoe Fluent Icons code points used across the UI.
namespace glyph {
inline constexpr const wchar_t* Home = L"";
inline constexpr const wchar_t* Hand = L"";  // "Touch"
inline constexpr const wchar_t* People = L"";
inline constexpr const wchar_t* Camera = L"";
inline constexpr const wchar_t* Speed = L"";
inline constexpr const wchar_t* Settings = L"";
inline constexpr const wchar_t* Info = L"";
inline constexpr const wchar_t* Add = L"";
inline constexpr const wchar_t* Delete = L"";
inline constexpr const wchar_t* Edit = L"";
inline constexpr const wchar_t* Play = L"";
inline constexpr const wchar_t* Pause = L"";
inline constexpr const wchar_t* ChevronDown = L"";
inline constexpr const wchar_t* ChevronUp = L"";
inline constexpr const wchar_t* ChevronRight = L"";
inline constexpr const wchar_t* Moon = L"";
inline constexpr const wchar_t* Sun = L"";
inline constexpr const wchar_t* Leaf = L"";      // "Eco" (power saving)
inline constexpr const wchar_t* Power = L"";
inline constexpr const wchar_t* Record = L"";
inline constexpr const wchar_t* Contact = L"";
inline constexpr const wchar_t* Shield = L"";
inline constexpr const wchar_t* Keyboard = L"";
inline constexpr const wchar_t* Volume = L"";
inline constexpr const wchar_t* Ruler = L"";
inline constexpr const wchar_t* Eye = L"";
inline constexpr const wchar_t* Chip = L"";      // processor
inline constexpr const wchar_t* Battery = L"";
inline constexpr const wchar_t* Game = L"";
inline constexpr const wchar_t* Lock = L"";
inline constexpr const wchar_t* Folder = L"";
inline constexpr const wchar_t* Refresh = L"";
inline constexpr const wchar_t* Open = L"";
inline constexpr const wchar_t* Checkmark = L"";
inline constexpr const wchar_t* Brightness = L"";
inline constexpr const wchar_t* Color = L"";
inline constexpr const wchar_t* Notification = L"";
inline constexpr const wchar_t* Rocket = L"";    // speed
inline constexpr const wchar_t* Swap = L"";
inline constexpr const wchar_t* Taskbar = L"";
inline constexpr const wchar_t* Memory = L"";
inline constexpr const wchar_t* Warning = L"";
inline constexpr const wchar_t* Flash = L"";
}  // namespace glyph

}  // namespace vl::ui
