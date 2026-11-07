#include "ui/gfx.hpp"

#include <algorithm>
#include <cmath>

namespace vl::ui {

namespace {

bool hasFamily(IDWriteFactory* f, const wchar_t* name) {
    ComPtr<IDWriteFontCollection> col;
    if (FAILED(f->GetSystemFontCollection(&col))) return false;
    UINT32 idx = 0;
    BOOL exists = FALSE;
    col->FindFamilyName(name, &idx, &exists);
    return exists != FALSE;
}

inline D2D1_RECT_F toD2(const RectF& r) { return D2D1::RectF(r.x, r.y, r.x + r.w, r.y + r.h); }

}  // namespace

Gfx& Gfx::get() {
    static Gfx g;
    return g;
}

Gfx::Gfx() {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d_.GetAddressOf());
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown**>(dw_.GetAddressOf()));
    textFamily_ = hasFamily(dw_.Get(), L"Segoe UI Variable Text") ? L"Segoe UI Variable Text" : L"Segoe UI";
    iconFamily_ = hasFamily(dw_.Get(), L"Segoe Fluent Icons") ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets";
}

IDWriteTextFormat* Gfx::format(float size, Weight w, bool icon, bool wrap, Align h, Align v) {
    const auto key = std::make_tuple(static_cast<int>(size * 4), static_cast<int>(w), icon, wrap, static_cast<int>(h),
                                     static_cast<int>(v));
    auto it = formats_.find(key);
    if (it != formats_.end()) return it->second.Get();
    ComPtr<IDWriteTextFormat> f;
    const wchar_t* fam = icon ? iconFamily_.c_str() : (size >= 20 && !icon ? L"Segoe UI Variable Display" : textFamily_.c_str());
    if (!icon && size >= 20 && !hasFamily(dw_.Get(), fam)) fam = textFamily_.c_str();
    dw_->CreateTextFormat(fam, nullptr, static_cast<DWRITE_FONT_WEIGHT>(w), DWRITE_FONT_STYLE_NORMAL,
                          DWRITE_FONT_STRETCH_NORMAL, size, L"ru-ru", &f);
    if (!f) return nullptr;
    f->SetWordWrapping(wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
    f->SetTextAlignment(h == Align::Start ? DWRITE_TEXT_ALIGNMENT_LEADING
                        : h == Align::Center ? DWRITE_TEXT_ALIGNMENT_CENTER
                                             : DWRITE_TEXT_ALIGNMENT_TRAILING);
    f->SetParagraphAlignment(v == Align::Start ? DWRITE_PARAGRAPH_ALIGNMENT_NEAR
                             : v == Align::Center ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER
                                                  : DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    if (!wrap) {
        // Ellipsis instead of clipping mid-glyph.
        ComPtr<IDWriteInlineObject> ell;
        dw_->CreateEllipsisTrimmingSign(f.Get(), &ell);
        DWRITE_TRIMMING tr{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
        f->SetTrimming(&tr, ell.Get());
    }
    formats_[key] = f;
    return f.Get();
}

bool Canvas::attach(HWND hwnd) {
    hwnd_ = hwnd;
    return true;
}

void Canvas::detach() {
    bmp_.Reset();
    brush_.Reset();
    layers_.clear();
    rt_.Reset();
    hwnd_ = nullptr;
}

void Canvas::resize() {
    if (!rt_ || !hwnd_) return;
    RECT rc;
    GetClientRect(hwnd_, &rc);
    rt_->Resize(D2D1::SizeU(std::max<LONG>(1, rc.right), std::max<LONG>(1, rc.bottom)));
}

bool Canvas::begin() {
    if (!hwnd_) return false;
    RECT rc;
    GetClientRect(hwnd_, &rc);
    dpi_ = static_cast<float>(GetDpiForWindow(hwnd_));
    if (!rt_) {
        const auto props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi_, dpi_);
        const auto hprops = D2D1::HwndRenderTargetProperties(
            hwnd_, D2D1::SizeU(std::max<LONG>(1, rc.right), std::max<LONG>(1, rc.bottom)), D2D1_PRESENT_OPTIONS_NONE);
        if (FAILED(Gfx::get().d2d()->CreateHwndRenderTarget(props, hprops, &rt_))) return false;
        rt_->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 1), &brush_);
        rt_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);  // required on transparent targets
        bmp_.Reset();
        bmpW_ = bmpH_ = 0;
    }
    rt_->SetDpi(dpi_, dpi_);
    w_ = rc.right * 96.f / dpi_;
    h_ = rc.bottom * 96.f / dpi_;
    rt_->BeginDraw();
    rt_->SetTransform(D2D1::Matrix3x2F::Identity());
    return true;
}

void Canvas::end() {
    if (!rt_) return;
    if (rt_->EndDraw() == static_cast<HRESULT>(D2DERR_RECREATE_TARGET)) {
        bmp_.Reset();
        brush_.Reset();
        layers_.clear();
        rt_.Reset();
    }
}

void Canvas::brush(const Color& c) { brush_->SetColor(D2D1::ColorF(c.r, c.g, c.b, c.a)); }

void Canvas::clear(const Color& c) { rt_->Clear(D2D1::ColorF(c.r * c.a, c.g * c.a, c.b * c.a, c.a)); }

void Canvas::fillRect(const RectF& r, const Color& c) {
    brush(c);
    rt_->FillRectangle(toD2(r), brush_.Get());
}

void Canvas::fillRound(const RectF& r, float rad, const Color& c) {
    brush(c);
    rt_->FillRoundedRectangle(D2D1::RoundedRect(toD2(r), rad, rad), brush_.Get());
}

void Canvas::strokeRound(const RectF& r, float rad, const Color& c, float width) {
    brush(c);
    const float h = width * 0.5f;
    rt_->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(r.x + h, r.y + h, r.x + r.w - h, r.y + r.h - h), rad, rad),
                              brush_.Get(), width);
}

void Canvas::fillCircle(Vec2 c, float rad, const Color& col) {
    brush(col);
    rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(c.x, c.y), rad, rad), brush_.Get());
}

void Canvas::strokeCircle(Vec2 c, float rad, const Color& col, float width) {
    brush(col);
    rt_->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(c.x, c.y), rad, rad), brush_.Get(), width);
}

void Canvas::line(Vec2 a, Vec2 b, const Color& c, float width) {
    brush(c);
    rt_->DrawLine(D2D1::Point2F(a.x, a.y), D2D1::Point2F(b.x, b.y), brush_.Get(), width);
}

void Canvas::arc(Vec2 c, float r, float start, float sweep, const Color& col, float width) {
    if (std::fabs(sweep) < 1e-3f) return;
    if (std::fabs(sweep) >= 2 * kPi - 1e-3f) {
        strokeCircle(c, r, col, width);
        return;
    }
    ComPtr<ID2D1PathGeometry> geo;
    Gfx::get().d2d()->CreatePathGeometry(&geo);
    ComPtr<ID2D1GeometrySink> sink;
    geo->Open(&sink);
    const D2D1_POINT_2F p0 = D2D1::Point2F(c.x + r * std::cos(start), c.y + r * std::sin(start));
    const float e = start + sweep;
    const D2D1_POINT_2F p1 = D2D1::Point2F(c.x + r * std::cos(e), c.y + r * std::sin(e));
    sink->BeginFigure(p0, D2D1_FIGURE_BEGIN_HOLLOW);
    sink->AddArc(D2D1::ArcSegment(p1, D2D1::SizeF(r, r), 0.f,
                                  sweep > 0 ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
                                  std::fabs(sweep) > kPi ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    sink->Close();
    brush(col);
    ComPtr<ID2D1StrokeStyle> style;
    D2D1_STROKE_STYLE_PROPERTIES sp = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND);
    Gfx::get().d2d()->CreateStrokeStyle(sp, nullptr, 0, &style);
    rt_->DrawGeometry(geo.Get(), brush_.Get(), width, style.Get());
}

void Canvas::text(const std::wstring& s, const RectF& r, const Color& c, float size, Weight w, Align h, Align v,
                  bool wrap) {
    if (s.empty()) return;
    auto* f = Gfx::get().format(size, w, false, wrap, h, v);
    if (!f) return;
    brush(c);
    rt_->DrawText(s.c_str(), static_cast<UINT32>(s.size()), f, toD2(r), brush_.Get(),
                  D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void Canvas::icon(const wchar_t* g, const RectF& r, const Color& c, float size, Align h) {
    auto* f = Gfx::get().format(size, Weight::Regular, true, false, h, Align::Center);
    if (!f) return;
    brush(c);
    rt_->DrawText(g, static_cast<UINT32>(wcslen(g)), f, toD2(r), brush_.Get());
}

Vec2 Canvas::measure(const std::wstring& s, float size, Weight w, float maxWidth, bool wrap) {
    auto* f = Gfx::get().format(size, w, false, wrap, Align::Start, Align::Start);
    ComPtr<IDWriteTextLayout> layout;
    if (!f || FAILED(Gfx::get().dwrite()->CreateTextLayout(s.c_str(), static_cast<UINT32>(s.size()), f, maxWidth, 1000.f,
                                                            &layout)))
        return {0, 0};
    DWRITE_TEXT_METRICS m{};
    layout->GetMetrics(&m);
    return {m.widthIncludingTrailingWhitespace, m.height};
}

void Canvas::updateBitmap(const uint8_t* bgra, int w, int h) {
    if (!rt_ || !bgra || w <= 0 || h <= 0) return;
    if (!bmp_ || w != bmpW_ || h != bmpH_) {
        bmp_.Reset();
        const auto props = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
        if (FAILED(rt_->CreateBitmap(D2D1::SizeU(w, h), props, &bmp_))) return;
        bmpW_ = w;
        bmpH_ = h;
    }
    bmp_->CopyFromMemory(nullptr, bgra, static_cast<UINT32>(w * 4));
}

void Canvas::drawBitmap(const RectF& dst, float opacity, bool flipX) {
    if (!bmp_) return;
    D2D1_MATRIX_3X2_F old;
    rt_->GetTransform(&old);
    if (flipX) {
        const float cx = dst.x + dst.w * 0.5f;
        rt_->SetTransform(D2D1::Matrix3x2F::Scale(-1.f, 1.f, D2D1::Point2F(cx, 0)) * old);
    }
    rt_->DrawBitmap(bmp_.Get(), toD2(dst), opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    rt_->SetTransform(old);
}

void Canvas::pushClip(const RectF& r) { rt_->PushAxisAlignedClip(toD2(r), D2D1_ANTIALIAS_MODE_ALIASED); }
void Canvas::popClip() { rt_->PopAxisAlignedClip(); }

void Canvas::pushRoundClip(const RectF& r, float rad) {
    ComPtr<ID2D1RoundedRectangleGeometry> geo;
    Gfx::get().d2d()->CreateRoundedRectangleGeometry(D2D1::RoundedRect(toD2(r), rad, rad), &geo);
    ComPtr<ID2D1Layer> layer;
    rt_->CreateLayer(&layer);
    rt_->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), geo.Get()), layer.Get());
    layers_.push_back(layer);
}

void Canvas::popRoundClip() {
    rt_->PopLayer();
    if (!layers_.empty()) layers_.pop_back();
}

}  // namespace vl::ui
