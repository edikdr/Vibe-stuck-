#include "app/actions.hpp"

#include "platform/win/system.hpp"

#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <vector>

namespace vl {

namespace {

bool isExtended(WORD vk) {
    switch (vk) {
        case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN: case VK_HOME: case VK_END:
        case VK_PRIOR: case VK_NEXT: case VK_INSERT: case VK_DELETE: case VK_RMENU: case VK_RCONTROL:
        case VK_LWIN: case VK_RWIN: case VK_APPS: case VK_DIVIDE: case VK_NUMLOCK: case VK_SNAPSHOT:
        case VK_MEDIA_PLAY_PAUSE: case VK_MEDIA_NEXT_TRACK: case VK_MEDIA_PREV_TRACK: case VK_MEDIA_STOP:
        case VK_VOLUME_UP: case VK_VOLUME_DOWN: case VK_VOLUME_MUTE:
        case VK_BROWSER_BACK: case VK_BROWSER_FORWARD:
            return true;
        default:
            return false;
    }
}

INPUT key(WORD vk, bool up) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    in.ki.dwFlags = (up ? KEYEVENTF_KEYUP : 0) | (isExtended(vk) ? KEYEVENTF_EXTENDEDKEY : 0);
    return in;
}

bool combo(std::initializer_list<WORD> keys) {
    std::vector<INPUT> v;
    for (WORD k : keys) v.push_back(key(k, false));
    for (auto it = keys.end(); it != keys.begin();) v.push_back(key(*--it, true));
    return SendInput(static_cast<UINT>(v.size()), v.data(), sizeof(INPUT)) == v.size();
}

bool comboVec(const std::vector<uint16_t>& keys) {
    if (keys.empty()) return false;
    std::vector<INPUT> v;
    for (auto k : keys) v.push_back(key(k, false));
    for (auto it = keys.rbegin(); it != keys.rend(); ++it) v.push_back(key(*it, true));
    return SendInput(static_cast<UINT>(v.size()), v.data(), sizeof(INPUT)) == v.size();
}

bool wheel(int delta) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_WHEEL;
    in.mi.mouseData = static_cast<DWORD>(delta);
    return SendInput(1, &in, sizeof in) == 1;
}

// ---- window switching (swipes): cycles windows without the Alt+Tab overlay

// A window the user would see in Alt+Tab: visible, not minimised, not a tool or
// owned popup, not cloaked (other virtual desktop, suspended Store app), titled,
// not always-on-top and not one of ours.
bool switchable(HWND h) {
    if (!IsWindowVisible(h) || IsIconic(h) || GetWindow(h, GW_OWNER)) return false;
    const LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
    if (ex & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST)) return false;
    BOOL cloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(h, DWMWA_CLOAKED, &cloaked, sizeof cloaked)) && cloaked) return false;
    if (GetWindowTextLengthW(h) == 0) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid == GetCurrentProcessId()) return false;
    RECT r{};
    GetWindowRect(h, &r);
    return r.right - r.left > 1 && r.bottom - r.top > 1;
}

std::vector<HWND> switchableWindows() {  // top of the z-order first
    std::vector<HWND> v;
    EnumWindows([](HWND h, LPARAM lp) -> BOOL {
        if (switchable(h)) reinterpret_cast<std::vector<HWND>*>(lp)->push_back(h);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&v));
    return v;
}

// A background process may not take the foreground (focus-stealing protection).
// The lock is lifted while Alt is held, which is what window switchers rely on.
// The extra unassigned key (0x88) keeps the target app from treating the lone
// Alt press as "open the menu bar".
bool activateWindow(HWND h) {
    INPUT down[3] = {key(VK_MENU, false), key(0x88, false), key(0x88, true)};
    SendInput(3, down, sizeof(INPUT));
    BOOL ok = SetForegroundWindow(h);
    INPUT up = key(VK_MENU, true);
    SendInput(1, &up, sizeof(INPUT));
    if (!ok) SwitchToThisWindow(h, TRUE);
    return true;
}

bool cycleWindows(bool forward) {
    const auto wins = switchableWindows();
    if (wins.empty()) return false;
    const HWND fg = GetForegroundWindow();
    const bool fgListed = !wins.empty() && wins.front() == fg;
    if (forward) {
        // Send the current window to the back and bring up the next one: repeated
        // swipes walk through every window, and "back" returns in reverse order.
        if (!fgListed) return activateWindow(wins.front());
        if (wins.size() < 2) return false;
        SetWindowPos(fg, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        return activateWindow(wins[1]);
    }
    const HWND last = wins.back();
    if (fgListed && last == fg) return false;
    return activateWindow(last);
}

bool foregroundCommand(int cmd) {  // SC_MINIMIZE / SC_MAXIMIZE / SC_RESTORE / SC_CLOSE
    const HWND fg = GetForegroundWindow();
    if (!fg || !switchable(fg)) return false;  // never the desktop or the taskbar
    if (cmd == SC_MAXIMIZE && IsZoomed(fg)) cmd = SC_RESTORE;
    return PostMessageW(fg, WM_SYSCOMMAND, static_cast<WPARAM>(cmd), 0) != FALSE;
}

}  // namespace

std::vector<InstalledApp> installedApps() {
    using Microsoft::WRL::ComPtr;
    std::vector<InstalledApp> out;
    ComPtr<IShellItem> folder;
    if (FAILED(SHGetKnownFolderItem(FOLDERID_AppsFolder, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS(&folder)))) return out;
    ComPtr<IEnumShellItems> items;
    if (FAILED(folder->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&items)))) return out;
    ComPtr<IShellItem> it;
    while (items->Next(1, &it, nullptr) == S_OK) {
        PWSTR name = nullptr, parsing = nullptr;
        if (SUCCEEDED(it->GetDisplayName(SIGDN_NORMALDISPLAY, &name)) &&
            SUCCEEDED(it->GetDisplayName(SIGDN_PARENTRELATIVEPARSING, &parsing)) && name && parsing && *name) {
            out.push_back({name, std::wstring(L"shell:AppsFolder\\") + parsing});
        }
        CoTaskMemFree(name);
        CoTaskMemFree(parsing);
        it.Reset();
    }
    std::sort(out.begin(), out.end(), [](const InstalledApp& a, const InstalledApp& b) {
        return CompareStringEx(LOCALE_NAME_USER_DEFAULT, NORM_IGNORECASE, a.name.c_str(), -1, b.name.c_str(), -1, nullptr,
                               nullptr, 0) == CSTR_LESS_THAN;
    });
    return out;
}

bool executeAction(const Action& a) {
    switch (a.type) {
        case ActionType::None: return true;
        case ActionType::MediaPlayPause: return combo({VK_MEDIA_PLAY_PAUSE});
        case ActionType::MediaNext: return combo({VK_MEDIA_NEXT_TRACK});
        case ActionType::MediaPrev: return combo({VK_MEDIA_PREV_TRACK});
        case ActionType::MediaStop: return combo({VK_MEDIA_STOP});
        case ActionType::VolumeUp: return combo({VK_VOLUME_UP});
        case ActionType::VolumeDown: return combo({VK_VOLUME_DOWN});
        case ActionType::VolumeMute: return combo({VK_VOLUME_MUTE});
        case ActionType::MicMute: return combo({VK_LWIN, VK_MENU, 'K'});
        case ActionType::KeyCombo: return comboVec(a.keys);
        case ActionType::ScrollUp: return wheel(WHEEL_DELTA);
        case ActionType::ScrollDown: return wheel(-WHEEL_DELTA);
        case ActionType::LaunchApp: {
            if (a.path.empty()) return false;
            const auto r = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", win::widen(a.path).c_str(),
                                                                   a.args.empty() ? nullptr : win::widen(a.args).c_str(),
                                                                   nullptr, SW_SHOWNORMAL));
            return r > 32;
        }
        case ActionType::LockPC: return LockWorkStation() != FALSE;
        case ActionType::ShowDesktop: return combo({VK_LWIN, 'D'});
        case ActionType::TaskView: return combo({VK_LWIN, VK_TAB});
        case ActionType::Screenshot: return combo({VK_LWIN, VK_SHIFT, 'S'});
        case ActionType::SwitchWindow: return combo({VK_MENU, VK_TAB});
        case ActionType::PauseRecognition: return true;  // handled by the app
        case ActionType::NextWindow: return cycleWindows(true);
        case ActionType::PrevWindow: return cycleWindows(false);
        case ActionType::MinimizeWindow: return foregroundCommand(SC_MINIMIZE);
        case ActionType::MaximizeWindow: return foregroundCommand(SC_MAXIMIZE);
        case ActionType::CloseWindow: return foregroundCommand(SC_CLOSE);
        case ActionType::NextDesktop: return combo({VK_LCONTROL, VK_LWIN, VK_RIGHT});
        case ActionType::PrevDesktop: return combo({VK_LCONTROL, VK_LWIN, VK_LEFT});
        case ActionType::StartMenu: return combo({VK_LWIN});
        case ActionType::Search: return combo({VK_LWIN, 'S'});
        case ActionType::FileExplorer: return combo({VK_LWIN, 'E'});
        case ActionType::BrowserBack: return combo({VK_BROWSER_BACK});
        case ActionType::BrowserForward: return combo({VK_BROWSER_FORWARD});
        case ActionType::ScreenshotFull: return combo({VK_LWIN, VK_SNAPSHOT});
        case ActionType::kCount: break;
    }
    return false;
}

std::wstring actionTitleRu(ActionType t) {
    switch (t) {
        case ActionType::None: return L"Ничего (только подсказка)";
        case ActionType::MediaPlayPause: return L"Пауза / воспроизведение";
        case ActionType::MediaNext: return L"Следующий трек";
        case ActionType::MediaPrev: return L"Предыдущий трек";
        case ActionType::MediaStop: return L"Стоп";
        case ActionType::VolumeUp: return L"Громче";
        case ActionType::VolumeDown: return L"Тише";
        case ActionType::VolumeMute: return L"Выключить звук";
        case ActionType::MicMute: return L"Микрофон вкл/выкл (Win+Alt+K)";
        case ActionType::KeyCombo: return L"Сочетание клавиш…";
        case ActionType::ScrollUp: return L"Прокрутка вверх";
        case ActionType::ScrollDown: return L"Прокрутка вниз";
        case ActionType::LaunchApp: return L"Запустить приложение или файл…";
        case ActionType::LockPC: return L"Заблокировать ПК";
        case ActionType::ShowDesktop: return L"Показать рабочий стол";
        case ActionType::TaskView: return L"Представление задач";
        case ActionType::Screenshot: return L"Снимок области (Win+Shift+S)";
        case ActionType::SwitchWindow: return L"Последнее окно (Alt+Tab)";
        case ActionType::PauseRecognition: return L"Пауза распознавания";
        case ActionType::NextWindow: return L"Следующее окно";
        case ActionType::PrevWindow: return L"Предыдущее окно";
        case ActionType::MinimizeWindow: return L"Свернуть окно";
        case ActionType::MaximizeWindow: return L"Развернуть / восстановить окно";
        case ActionType::CloseWindow: return L"Закрыть окно";
        case ActionType::NextDesktop: return L"Следующий рабочий стол";
        case ActionType::PrevDesktop: return L"Предыдущий рабочий стол";
        case ActionType::StartMenu: return L"Меню «Пуск»";
        case ActionType::Search: return L"Поиск Windows";
        case ActionType::FileExplorer: return L"Проводник";
        case ActionType::BrowserBack: return L"Назад (браузер, проводник)";
        case ActionType::BrowserForward: return L"Вперёд (браузер, проводник)";
        case ActionType::ScreenshotFull: return L"Снимок всего экрана в «Изображения»";
        case ActionType::kCount: break;
    }
    return L"";
}

std::wstring actionGlyph(ActionType t) {
    switch (t) {
        case ActionType::MediaPlayPause: return L"";
        case ActionType::MediaNext: return L"";
        case ActionType::MediaPrev: return L"";
        case ActionType::MediaStop: return L"";
        case ActionType::VolumeUp: return L"";
        case ActionType::VolumeDown: return L"";
        case ActionType::VolumeMute: return L"";
        case ActionType::MicMute: return L"";
        case ActionType::KeyCombo: return L"";
        case ActionType::ScrollUp: return L"";
        case ActionType::ScrollDown: return L"";
        case ActionType::LaunchApp: return L"";
        case ActionType::LockPC: return L"";
        case ActionType::ShowDesktop: return L"";
        case ActionType::TaskView: return L"";
        case ActionType::Screenshot: return L"";
        case ActionType::SwitchWindow: return L"";
        case ActionType::PauseRecognition: return L"";
        case ActionType::NextWindow: return L"";       // ChevronRight
        case ActionType::PrevWindow: return L"";       // ChevronLeft
        case ActionType::MinimizeWindow: return L"";   // ChromeMinimize
        case ActionType::MaximizeWindow: return L"";   // ChromeMaximize
        case ActionType::CloseWindow: return L"";      // ChromeClose
        case ActionType::NextDesktop: return L"";      // Forward
        case ActionType::PrevDesktop: return L"";      // Back
        case ActionType::StartMenu: return L"";        // Home
        case ActionType::Search: return L"";           // Search
        case ActionType::FileExplorer: return L"";     // Folder
        case ActionType::BrowserBack: return L"";
        case ActionType::BrowserForward: return L"";
        case ActionType::ScreenshotFull: return L"";   // TVMonitor
        default: return L"";
    }
}

std::wstring describeAction(const Action& a) {
    if (a.type == ActionType::KeyCombo) {
        std::wstring s;
        for (size_t i = 0; i < a.keys.size(); ++i) s += (i ? L" + " : L"") + win::vkName(a.keys[i]);
        return s.empty() ? L"Сочетание не задано" : s;
    }
    if (a.type == ActionType::LaunchApp) {
        if (a.path.empty()) return L"Приложение не выбрано";
        if (!a.label.empty()) return L"Запуск: " + win::widen(a.label);
        const auto p = std::filesystem::path(win::widen(a.path));
        return L"Запуск: " + p.filename().wstring();
    }
    return actionTitleRu(a.type);
}

}  // namespace vl
