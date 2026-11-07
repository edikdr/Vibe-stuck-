#include "platform/win/tray.hpp"

#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace vl::win {

namespace {

constexpr UINT kTrayId = 1;

float sdCapsule(float px, float py, float ax, float ay, float bx, float by, float r) {
    const float pax = px - ax, pay = py - ay, bax = bx - ax, bay = by - ay;
    const float h = std::clamp((pax * bax + pay * bay) / (bax * bax + bay * bay), 0.f, 1.f);
    const float dx = pax - bax * h, dy = pay - bay * h;
    return std::sqrt(dx * dx + dy * dy) - r;
}

float sdRoundBox(float px, float py, float cx, float cy, float hw, float hh, float r) {
    const float qx = std::fabs(px - cx) - hw + r, qy = std::fabs(py - cy) - hh + r;
    const float ox = std::max(qx, 0.f), oy = std::max(qy, 0.f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.f) - r;
}

// Glyph in unit space (0..1): palm + four fingers + thumb + scan arc above.
float sdGlyph(float x, float y) {
    float d = sdRoundBox(x, y, 0.50f, 0.68f, 0.20f, 0.18f, 0.10f);
    const float fx[4] = {0.335f, 0.447f, 0.559f, 0.668f}, top[4] = {0.31f, 0.25f, 0.28f, 0.37f};
    for (int i = 0; i < 4; ++i) d = std::min(d, sdCapsule(x, y, fx[i], top[i], fx[i], 0.62f, 0.036f));
    d = std::min(d, sdCapsule(x, y, 0.33f, 0.72f, 0.19f, 0.56f, 0.045f));
    // Scan arc: ring segment above the fingers.
    const float cx = 0.5f, cy = 0.62f, R = 0.52f;
    const float rr = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
    const float ang = std::atan2(y - cy, x - cx);  // -pi/2 = straight up
    const float ring = std::fabs(rr - R) - 0.034f;
    const float span = std::fabs(ang + 1.5708f) - 0.62f;  // +-35° around "up"
    d = std::min(d, std::max(ring, span * R));
    return d;
}

uint32_t badgeColor(IconBadge b) {
    switch (b) {
        case IconBadge::Active: return 0x3DDC84;
        case IconBadge::Idle: return 0x5BB0FF;
        case IconBadge::Paused: return 0x9AA0A6;
        case IconBadge::Error: return 0xFF5A5A;
        case IconBadge::Night: return 0xB18CFF;
        default: return 0;
    }
}

}  // namespace

HICON makeIcon(int size, bool tile, bool darkGlyph, uint32_t accent, IconBadge badge) {
    BITMAPV5HEADER bi{};
    bi.bV5Size = sizeof bi;
    bi.bV5Width = size;
    bi.bV5Height = -size;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;
    void* bits = nullptr;
    HDC dc = GetDC(nullptr);
    HBITMAP color = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, dc);
    if (!color) return nullptr;
    auto* px = static_cast<uint32_t*>(bits);

    const float inv = 1.f / size;
    const uint32_t bc = badgeColor(badge);
    const float glyphScale = tile ? 0.78f : 1.0f, glyphOff = (1.f - glyphScale) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float u = (x + 0.5f) * inv, v = (y + 0.5f) * inv;
            float r = 0, g = 0, b = 0, a = 0;
            auto over = [&](float cr, float cg, float cb, float ca) {
                r = cr * ca + r * (1 - ca);
                g = cg * ca + g * (1 - ca);
                b = cb * ca + b * (1 - ca);
                a = ca + a * (1 - ca);
            };
            if (tile) {
                const float dt = sdRoundBox(u, v, 0.5f, 0.5f, 0.47f, 0.47f, 0.22f) * size;
                const float ta = std::clamp(0.5f - dt, 0.f, 1.f);
                // Accent gradient, slightly lighter at the top.
                const float k = 1.15f - 0.3f * v;
                over(std::min(1.f, ((accent >> 16) & 0xFF) / 255.f * k), std::min(1.f, ((accent >> 8) & 0xFF) / 255.f * k),
                     std::min(1.f, (accent & 0xFF) / 255.f * k), ta);
            }
            const float gu = (u - glyphOff) / glyphScale, gv = (v - glyphOff) / glyphScale;
            const float dg = sdGlyph(gu, gv) * size * glyphScale;
            float ga = std::clamp(0.5f - dg, 0.f, 1.f);
            float bd = 1e9f;
            if (bc) {
                const float cx = 0.80f, cy = 0.80f, R = 0.17f;
                bd = (std::sqrt((u - cx) * (u - cx) + (v - cy) * (v - cy)) - R) * size;
                // Cut a gap around the badge so it reads at 16 px.
                ga *= std::clamp(bd - 1.f + 0.5f, 0.f, 1.f);
            }
            const float gcol = darkGlyph && !tile ? 0.1f : 1.f;
            over(gcol, gcol, gcol, ga);
            if (bc) over(((bc >> 16) & 0xFF) / 255.f, ((bc >> 8) & 0xFF) / 255.f, (bc & 0xFF) / 255.f,
                         std::clamp(0.5f - bd, 0.f, 1.f));
            const auto c8 = [](float c) { return static_cast<uint32_t>(std::clamp(c, 0.f, 1.f) * 255.f + 0.5f); };
            // Straight (non-premultiplied) alpha for CreateIconIndirect.
            const float ia = a > 1e-4f ? 1.f / a : 0.f;
            px[y * size + x] = (c8(a) << 24) | (c8(r * ia) << 16) | (c8(g * ia) << 8) | c8(b * ia);
        }
    }
    std::vector<uint8_t> maskBits(static_cast<size_t>((size + 15) / 16 * 2) * size, 0);
    HBITMAP mask = CreateBitmap(size, size, 1, 1, maskBits.data());
    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmColor = color;
    ii.hbmMask = mask;
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(color);
    DeleteObject(mask);
    return icon;
}

UINT TrayIcon::taskbarCreatedMsg() {
    static const UINT m = RegisterWindowMessageW(L"TaskbarCreated");
    return m;
}

bool TrayIcon::create(HWND owner, UINT msg, HICON icon, const std::wstring& tip) {
    owner_ = owner;
    msg_ = msg;
    icon_ = icon;
    tip_ = tip;
    return recreate();
}

bool TrayIcon::recreate() {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof nid;
    nid.hWnd = owner_;
    nid.uID = kTrayId;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = msg_;
    nid.hIcon = icon_;
    wcsncpy_s(nid.szTip, tip_.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_DELETE, &nid);
    added_ = Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
    if (added_) {
        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid);
    }
    return added_;
}

void TrayIcon::update(HICON icon, const std::wstring& tip) {
    icon_ = icon;
    tip_ = tip;
    if (!added_) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof nid;
    nid.hWnd = owner_;
    nid.uID = kTrayId;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid.hIcon = icon;
    wcsncpy_s(nid.szTip, tip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayIcon::notify(const std::wstring& title, const std::wstring& text) {
    if (!added_) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof nid;
    nid.hWnd = owner_;
    nid.uID = kTrayId;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON | NIIF_RESPECT_QUIET_TIME;
    nid.hBalloonIcon = icon_;
    wcsncpy_s(nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid.szInfo, text.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayIcon::remove() {
    if (!added_) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof nid;
    nid.hWnd = owner_;
    nid.uID = kTrayId;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    added_ = false;
}

RECT TrayIcon::anchorRect() const {
    NOTIFYICONIDENTIFIER id{};
    id.cbSize = sizeof id;
    id.hWnd = owner_;
    id.uID = kTrayId;
    RECT r{};
    if (FAILED(Shell_NotifyIconGetRect(&id, &r))) {
        POINT p;
        GetCursorPos(&p);
        r = {p.x - 8, p.y - 8, p.x + 8, p.y + 8};
    }
    return r;
}

}  // namespace vl::win
