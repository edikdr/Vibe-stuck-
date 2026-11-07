// Notification-area icon (next to the clock / volume) and procedurally drawn icons.
#pragma once

#include <cstdint>
#include <string>

#include "platform/win/win_common.hpp"

namespace vl::win {

enum class IconBadge { None, Active, Idle, Paused, Error, Night };

// Draws the Vibe Hand glyph (stylised palm with a scan arc) at `size` px.
//   tile = true : white glyph on an accent-coloured rounded tile (app/window icon)
//   tile = false: monochrome glyph for the tray, white or black for the taskbar theme
HICON makeIcon(int size, bool tile, bool darkGlyph, uint32_t accentRgb, IconBadge badge);

class TrayIcon {
public:
    bool create(HWND owner, UINT callbackMsg, HICON icon, const std::wstring& tip);
    void update(HICON icon, const std::wstring& tip);
    void notify(const std::wstring& title, const std::wstring& text);  // shows as a Windows toast
    void remove();
    bool recreate();  // after Explorer restarts ("TaskbarCreated")
    RECT anchorRect() const;  // icon position, for the flyout
    static UINT taskbarCreatedMsg();

private:
    HWND owner_ = nullptr;
    UINT msg_ = 0;
    HICON icon_ = nullptr;
    std::wstring tip_;
    bool added_ = false;
};

}  // namespace vl::win
