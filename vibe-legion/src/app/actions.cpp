#include "app/actions.hpp"

#include "platform/win/system.hpp"

#include <dwmapi.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

#include "core/macro.hpp"

namespace vl {

bool toggleSystemMicMute(bool* mutedNow) {
    // Mutes the capture endpoints themselves, like the mute box in Sound settings: every
    // app (Discord, Zoom, games) then receives silence. Hotkeys such as Win+Alt+K only
    // reach apps that implement the Windows 11 call-mute API, which Discord does not.
    using Microsoft::WRL::ComPtr;
    ComPtr<IMMDeviceEnumerator> en;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&en)))) return false;
    // The new state follows the default communications microphone (what calls use).
    BOOL curMuted = FALSE;
    {
        ComPtr<IMMDevice> def;
        if (FAILED(en->GetDefaultAudioEndpoint(eCapture, eCommunications, &def)) &&
            FAILED(en->GetDefaultAudioEndpoint(eCapture, eConsole, &def)))
            return false;
        ComPtr<IAudioEndpointVolume> vol;
        if (FAILED(def->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(vol.GetAddressOf()))))
            return false;
        vol->GetMute(&curMuted);
    }
    const BOOL target = curMuted ? FALSE : TRUE;
    ComPtr<IMMDeviceCollection> all;
    if (FAILED(en->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &all))) return false;
    UINT n = 0;
    all->GetCount(&n);
    bool any = false;
    for (UINT i = 0; i < n; ++i) {  // every microphone, so a headset and the built-in array agree
        ComPtr<IMMDevice> d;
        ComPtr<IAudioEndpointVolume> vol;
        if (FAILED(all->Item(i, &d)) ||
            FAILED(d->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(vol.GetAddressOf()))))
            continue;
        any = SUCCEEDED(vol->SetMute(target, nullptr)) || any;
    }
    if (mutedNow) *mutedNow = target != FALSE;
    return any;
}

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

bool typeText(const std::wstring& text) {
    std::vector<INPUT> v;
    for (wchar_t ch : text) {
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wScan = ch;
        in.ki.dwFlags = KEYEVENTF_UNICODE;
        v.push_back(in);
        in.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        v.push_back(in);
    }
    return !v.empty() && SendInput(static_cast<UINT>(v.size()), v.data(), sizeof(INPUT)) == v.size();
}

// Starts a program or script without a console window flashing up: PowerShell scripts
// through powershell.exe, batch files through cmd.exe, everything else (AutoHotkey,
// Python, .exe, documents, links) the way a double click would.
bool runCommand(const std::wstring& path, const std::wstring& args) {
    if (path.empty()) return false;
    std::wstring ext = std::filesystem::path(path).extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    std::wstring cmd;
    if (ext == L".ps1")
        cmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + path + L"\" " + args;
    else if (ext == L".bat" || ext == L".cmd")
        cmd = L"cmd.exe /c \"\"" + path + L"\" " + args + L"\"";
    if (cmd.empty()) {
        const auto r = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", path.c_str(), args.empty() ? nullptr : args.c_str(),
                                                               nullptr, SW_SHOWNORMAL));
        return r > 32;
    }
    STARTUPINFOW si{sizeof si};
    PROCESS_INFORMATION pi{};
    const std::wstring dir = std::filesystem::path(path).parent_path().wstring();
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        dir.empty() ? nullptr : dir.c_str(), &si, &pi))
        return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

// "C:\My scripts\a.ps1 -x 1", "\"C:\a b.exe\" arg": the program and its arguments.
void splitCommandLine(const std::wstring& line, std::wstring& path, std::wstring& args) {
    path.clear();
    args.clear();
    if (!line.empty() && line[0] == L'"') {
        const size_t q = line.find(L'"', 1);
        path = line.substr(1, q == std::wstring::npos ? std::wstring::npos : q - 1);
        if (q != std::wstring::npos && q + 1 < line.size()) args = line.substr(q + 1);
    } else {
        // The longest prefix that is an existing file wins (paths with spaces, no quotes);
        // otherwise the first word is the program.
        std::error_code ec;
        size_t cut = line.size();
        while (cut != std::wstring::npos && cut > 0 && !std::filesystem::exists(line.substr(0, cut), ec))
            cut = cut > 1 ? line.rfind(L' ', cut - 1) : std::wstring::npos;
        if (cut == std::wstring::npos || cut == 0) cut = std::min(line.find(L' '), line.size());
        path = line.substr(0, cut);
        if (cut < line.size()) args = line.substr(cut + 1);
    }
    while (!args.empty() && args.front() == L' ') args.erase(args.begin());
}

std::atomic<bool> g_macroBusy{false};

// Runs on its own thread: the pauses must not freeze the tray icon and the windows.
void runMacro(std::vector<MacroStep> steps) {
    if (g_macroBusy.exchange(true)) return;  // one macro at a time
    std::thread([steps = std::move(steps)] {
        const bool com = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
        for (const auto& s : steps) {
            switch (s.kind) {
                case MacroStep::Kind::Keys: comboVec(s.keys); break;
                case MacroStep::Kind::Wait: Sleep(static_cast<DWORD>(s.ms)); break;
                case MacroStep::Kind::Text: typeText(win::widen(s.text)); break;
                case MacroStep::Kind::Run: {
                    std::wstring p, a;
                    splitCommandLine(win::widen(s.text), p, a);
                    runCommand(p, a);
                    break;
                }
                case MacroStep::Kind::Builtin: {
                    Action a;
                    a.type = s.action;
                    executeAction(a);
                    break;
                }
            }
            if (s.kind != MacroStep::Kind::Wait) Sleep(40);  // let the target app take the input in order
        }
        if (com) CoUninitialize();
        g_macroBusy = false;
    }).detach();
}

std::wstring exeOfWindow(HWND w) {
    DWORD pid = 0;
    if (!w || !GetWindowThreadProcessId(w, &pid) || !pid) return {};
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!p) return {};
    wchar_t path[MAX_PATH * 2];
    DWORD n = static_cast<DWORD>(std::size(path));
    const bool ok = QueryFullProcessImageNameW(p, 0, path, &n) != FALSE;
    CloseHandle(p);
    return ok ? std::filesystem::path(std::wstring(path, n)).filename().wstring() : std::wstring();
}

}  // namespace

std::wstring foregroundAppExe() { return exeOfWindow(GetForegroundWindow()); }

std::vector<std::wstring> openWindowApps() {
    std::vector<std::wstring> out;
    for (HWND h : switchableWindows()) {
        std::wstring exe = exeOfWindow(h);
        std::transform(exe.begin(), exe.end(), exe.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
        if (!exe.empty() && std::find(out.begin(), out.end(), exe) == out.end()) out.push_back(exe);
    }
    std::sort(out.begin(), out.end());
    return out;
}

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
        case ActionType::MicMute: return toggleSystemMicMute(nullptr);
        case ActionType::CallMute: return combo({VK_LWIN, VK_MENU, 'K'});
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
        case ActionType::ZoomIn: return combo({VK_CONTROL, VK_OEM_PLUS});
        case ActionType::ZoomOut: return combo({VK_CONTROL, VK_OEM_MINUS});
        case ActionType::Macro: {
            std::vector<MacroStep> steps;
            if (!parseMacro(a.script, steps, nullptr)) return false;
            runMacro(std::move(steps));
            return true;
        }
        case ActionType::RunScript: return runCommand(win::widen(a.path), win::widen(a.args));
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
        case ActionType::MicMute: return L"Микрофон вкл/выкл (все приложения)";
        case ActionType::CallMute: return L"Микрофон в звонке (Win+Alt+K)";
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
        case ActionType::ZoomIn: return L"Увеличить масштаб (Ctrl +)";
        case ActionType::ZoomOut: return L"Уменьшить масштаб (Ctrl −)";
        case ActionType::Macro: return L"Макрос: несколько шагов подряд…";
        case ActionType::RunScript: return L"Запустить скрипт или программу скрыто…";
        case ActionType::kCount: break;
    }
    return L"";
}

std::wstring actionDescriptionRu(ActionType t) {
    switch (t) {
        case ActionType::None: return L"Ничего не делает: только подсказка на экране и запись в журнал. Удобно, чтобы проверить жест.";
        case ActionType::MediaPlayPause: return L"Клавиша «Воспроизведение/пауза»: музыка, YouTube, плееры — то, что сейчас играет.";
        case ActionType::MediaNext: return L"Следующий трек в текущем плеере (мультимедийная клавиша).";
        case ActionType::MediaPrev: return L"Предыдущий трек или начало текущего (мультимедийная клавиша).";
        case ActionType::MediaStop: return L"Останавливает воспроизведение (мультимедийная клавиша «Стоп»).";
        case ActionType::VolumeUp: return L"Громкость +2 %. Включите «Повторять», чтобы прибавлять, пока держите жест.";
        case ActionType::VolumeDown: return L"Громкость −2 %. Включите «Повторять», чтобы убавлять, пока держите жест.";
        case ActionType::VolumeMute: return L"Выключает и снова включает звук системы.";
        case ActionType::MicMute: return L"Выключает сами микрофоны в Windows: вас не слышит ни одно приложение — Discord, Zoom, игры. Повтор включает. Значок микрофона внутри приложения при этом не меняется.";
        case ActionType::CallMute: return L"Win+Alt+K: кнопка микрофона в Teams и других приложениях с поддержкой звонков Windows 11. Discord её не поддерживает — для него «Микрофон вкл/выкл».";
        case ActionType::KeyCombo: return L"Нажимает заданное сочетание в активном окне: например Ctrl+Shift+M или F5.";
        case ActionType::ScrollUp: return L"Колёсико вверх под курсором. С «Повторять» — непрерывная прокрутка.";
        case ActionType::ScrollDown: return L"Колёсико вниз под курсором. С «Повторять» — непрерывная прокрутка.";
        case ActionType::LaunchApp: return L"Открывает приложение из меню «Пуск» (и из Microsoft Store), файл, папку или ссылку.";
        case ActionType::LockPC: return L"Win+L: блокирует компьютер. Камера при этом выключается.";
        case ActionType::ShowDesktop: return L"Win+D: сворачивает все окна; повторно — возвращает их.";
        case ActionType::TaskView: return L"Win+Tab: все окна и виртуальные рабочие столы на одном экране.";
        case ActionType::Screenshot: return L"Win+Shift+S: выделите область — снимок попадёт в буфер обмена.";
        case ActionType::SwitchWindow: return L"Alt+Tab: переключает на предыдущее окно; повтор возвращает обратно.";
        case ActionType::PauseRecognition: return L"Ставит распознавание на паузу. Снять — горячей клавишей, из трея или в окне.";
        case ActionType::NextWindow: return L"Перебирает открытые окна по кругу без панели Alt+Tab: текущее уходит назад. Пара к «Взмах вправо».";
        case ActionType::PrevWindow: return L"Возвращает окна в обратном порядке. Пара к «Взмах влево».";
        case ActionType::MinimizeWindow: return L"Сворачивает активное окно в панель задач.";
        case ActionType::MaximizeWindow: return L"Разворачивает активное окно на весь экран или возвращает прежний размер.";
        case ActionType::CloseWindow: return L"Закрывает активное окно, как крестик (несохранённое приложение спросит).";
        case ActionType::NextDesktop: return L"Ctrl+Win+→: следующий виртуальный рабочий стол.";
        case ActionType::PrevDesktop: return L"Ctrl+Win+←: предыдущий виртуальный рабочий стол.";
        case ActionType::StartMenu: return L"Открывает или закрывает меню «Пуск» (клавиша Win).";
        case ActionType::Search: return L"Win+S: поиск Windows.";
        case ActionType::FileExplorer: return L"Win+E: новое окно проводника.";
        case ActionType::BrowserBack: return L"Клавиша «Назад»: браузеры, проводник, многие приложения.";
        case ActionType::BrowserForward: return L"Клавиша «Вперёд»: браузеры, проводник, многие приложения.";
        case ActionType::ScreenshotFull: return L"Win+PrtScn: весь экран сохраняется в «Изображения\\Снимки экрана».";
        case ActionType::ZoomIn: return L"Ctrl и «+»: крупнее страница в браузере, документ, фото. Пара к «Развести руки».";
        case ActionType::ZoomOut: return L"Ctrl и «−»: мельче. Пара к «Свести руки».";
        case ActionType::Macro: return L"Шаги через «;»: клавиши, пауза в мс, текст, запуск, действия. Пример: Ctrl+L; пауза 200; текст youtube.com; Enter";
        case ActionType::RunScript: return L"PowerShell (.ps1), .bat, AutoHotkey, Python или любая программа с аргументами. Окно консоли не показывается.";

        case ActionType::kCount: break;
    }
    return L"";
}
std::wstring actionGlyph(ActionType t) {
    switch (t) {
        case ActionType::Macro: return L"\uE8FD";       // bulleted list
        case ActionType::RunScript: return L"\uE756";   // command prompt
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
        case ActionType::ZoomIn: return L"";          // ZoomIn
        case ActionType::ZoomOut: return L"";         // ZoomOut
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
    if (a.type == ActionType::Macro) {
        std::vector<MacroStep> steps;
        if (a.script.empty()) return L"Макрос не задан";
        if (!parseMacro(a.script, steps, nullptr)) return L"Макрос с ошибкой";
        return L"Макрос: шагов — " + std::to_wstring(steps.size());
    }
    if (a.type == ActionType::RunScript) {
        if (a.path.empty()) return L"Скрипт не выбран";
        return L"Скрипт: " + std::filesystem::path(win::widen(a.path)).filename().wstring();
    }
    return actionTitleRu(a.type);
}

}  // namespace vl
