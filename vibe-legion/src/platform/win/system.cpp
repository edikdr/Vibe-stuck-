#include "platform/win/system.hpp"

#include <dpapi.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wincrypt.h>

#include <fstream>
#include <iterator>
#include <sstream>

namespace vl::win {

std::wstring widen(std::string_view s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string narrow(std::wstring_view w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

std::filesystem::path appDataDir() {
    PWSTR p = nullptr;
    std::filesystem::path dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p))) dir = std::filesystem::path(p) / L"VibeLegion";
    CoTaskMemFree(p);
    if (dir.empty()) dir = exeDir() / L"data";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::filesystem::path exeDir() {
    wchar_t buf[MAX_PATH * 2];
    const DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
    return std::filesystem::path(std::wstring(buf, n)).parent_path();
}

namespace {

bool readAll(const std::filesystem::path& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool writeAtomic(const std::filesystem::path& p, const void* data, size_t size) {
    std::filesystem::path tmp = p;
    tmp += L".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        if (!f) return false;
    }
    return MoveFileExW(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

}  // namespace

Settings loadSettings() {
    std::string text;
    const auto p = appDataDir() / L"settings.json";
    if (!readAll(p, text)) return Settings::defaults();
    auto r = json::parse(text);
    if (!r.ok) {
        // Keep the broken file for the user, start from defaults.
        std::error_code ec;
        std::filesystem::copy_file(p, appDataDir() / L"settings.broken.json",
                                   std::filesystem::copy_options::overwrite_existing, ec);
        return Settings::defaults();
    }
    return Settings::fromJson(r.value);
}

bool saveSettings(const Settings& s) {
    const std::string text = s.toJson().dump(2);
    return writeAtomic(appDataDir() / L"settings.json", text.data(), text.size());
}

FaceDatabase loadFaces() {
    FaceDatabase db;
    std::string blob;
    if (!readAll(appDataDir() / L"faces.bin", blob) || blob.empty()) return db;
    DATA_BLOB in{static_cast<DWORD>(blob.size()), reinterpret_cast<BYTE*>(blob.data())}, out{};
    if (CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        std::vector<uint8_t> plain(out.pbData, out.pbData + out.cbData);
        SecureZeroMemory(out.pbData, out.cbData);
        LocalFree(out.pbData);
        db.deserialize(plain);
    }
    return db;
}

bool saveFaces(const FaceDatabase& db) {
    auto plain = db.serialize();
    DATA_BLOB in{static_cast<DWORD>(plain.size()), plain.data()}, out{};
    if (!CryptProtectData(&in, L"Vibe Legion faces", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        return false;
    const bool ok = writeAtomic(appDataDir() / L"faces.bin", out.pbData, out.cbData);
    LocalFree(out.pbData);
    return ok;
}

namespace {
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"VibeLegion";
}  // namespace

bool setAutostart(bool on) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return false;
    LONG r;
    if (on) {
        wchar_t exe[MAX_PATH * 2];
        GetModuleFileNameW(nullptr, exe, static_cast<DWORD>(std::size(exe)));
        const std::wstring cmd = L"\"" + std::wstring(exe) + L"\" --minimized";
        r = RegSetValueExW(k, kRunValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(cmd.c_str()),
                           static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        r = RegDeleteValueW(k, kRunValue);
        if (r == ERROR_FILE_NOT_FOUND) r = ERROR_SUCCESS;
    }
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

bool autostartEnabled() {
    return RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, RRF_RT_REG_SZ, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
}

void applyProcessPolicy(PowerProfile p) {
    PROCESS_POWER_THROTTLING_STATE st{};
    st.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    st.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    st.StateMask = p == PowerProfile::Responsive ? 0 : PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &st, sizeof st);
    SetPriorityClass(GetCurrentProcess(), p == PowerProfile::Responsive ? ABOVE_NORMAL_PRIORITY_CLASS
                                          : p == PowerProfile::Eco      ? BELOW_NORMAL_PRIORITY_CLASS
                                                                        : NORMAL_PRIORITY_CLASS);
}

PowerState queryPower() {
    PowerState s;
    SYSTEM_POWER_STATUS ps{};
    if (GetSystemPowerStatus(&ps)) {
        s.onBattery = ps.ACLineStatus == 0;
        s.saver = ps.SystemStatusFlag != 0;  // battery saver on
    }
    return s;
}

bool fullscreenAppRunning() {
    QUERY_USER_NOTIFICATION_STATE q;
    if (FAILED(SHQueryUserNotificationState(&q))) return false;
    return q == QUNS_RUNNING_D3D_FULL_SCREEN || q == QUNS_PRESENTATION_MODE || q == QUNS_BUSY;
}

ProcessStats ProcessMeter::sample() {
    FILETIME c, e, k, u, now;
    GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
    GetSystemTimeAsFileTime(&now);
    auto q = [](FILETIME f) { return (static_cast<uint64_t>(f.dwHighDateTime) << 32) | f.dwLowDateTime; };
    const uint64_t proc = q(k) + q(u), wall = q(now);
    if (lastWall_ && wall > lastWall_) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        last_.cpuPercent = 100.0 * static_cast<double>(proc - lastProc_) / static_cast<double>(wall - lastWall_) /
                           std::max<DWORD>(1, si.dwNumberOfProcessors);
    }
    lastProc_ = proc;
    lastWall_ = wall;
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof pmc)) {
        last_.workingSetMb = pmc.WorkingSetSize / 1048576.0;
        last_.privateMb = pmc.PrivateUsage / 1048576.0;
    }
    return last_;
}

void trimWorkingSet() { SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1)); }

bool systemUsesLightTheme() {
    DWORD v = 0, sz = sizeof v;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz) == ERROR_SUCCESS)
        return v != 0;
    return false;
}

uint32_t systemAccentColor() {
    DWORD v = 0, sz = sizeof v;
    // AccentColor is ABGR.
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", L"AccentColor", RRF_RT_REG_DWORD, nullptr,
                     &v, &sz) == ERROR_SUCCESS)
        return ((v & 0xFF) << 16) | (v & 0xFF00) | ((v >> 16) & 0xFF);
    return 0x0078D4;
}

void enableDarkMenus(bool dark) {
    // SetPreferredAppMode (#135) and FlushMenuThemes (#136): undocumented but stable
    // since 1903; used by Notepad++, Windows Terminal, etc. Missing -> light menus.
    HMODULE ux = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!ux) return;
    using SetMode = int(WINAPI*)(int);
    using Flush = void(WINAPI*)();
    auto set = reinterpret_cast<SetMode>(reinterpret_cast<void*>(GetProcAddress(ux, MAKEINTRESOURCEA(135))));
    auto flush = reinterpret_cast<Flush>(reinterpret_cast<void*>(GetProcAddress(ux, MAKEINTRESOURCEA(136))));
    if (set) set(dark ? 2 /*ForceDark*/ : 3 /*ForceLight*/);
    if (flush) flush();
}

void openUri(const wchar_t* uri) { ShellExecuteW(nullptr, L"open", uri, nullptr, nullptr, SW_SHOWNORMAL); }

std::wstring vkName(uint32_t vk) {
    switch (vk) {
        case VK_SPACE: return L"Space";
        case VK_RETURN: return L"Enter";
        case VK_TAB: return L"Tab";
        case VK_ESCAPE: return L"Esc";
        case VK_LEFT: return L"←";
        case VK_RIGHT: return L"→";
        case VK_UP: return L"↑";
        case VK_DOWN: return L"↓";
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return L"Ctrl";
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return L"Shift";
        case VK_MENU: case VK_LMENU: case VK_RMENU: return L"Alt";
        case VK_LWIN: case VK_RWIN: return L"Win";
        case VK_MEDIA_PLAY_PAUSE: return L"Play/Pause";
        case VK_VOLUME_MUTE: return L"Mute";
        default: break;
    }
    if (vk >= VK_F1 && vk <= VK_F24) return L"F" + std::to_wstring(vk - VK_F1 + 1);
    const UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    wchar_t name[64] = {};
    LONG lp = static_cast<LONG>(sc << 16);
    if (vk == VK_INSERT || vk == VK_DELETE || vk == VK_HOME || vk == VK_END || vk == VK_PRIOR || vk == VK_NEXT)
        lp |= 1 << 24;  // extended
    if (GetKeyNameTextW(lp, name, 64) > 0) return name;
    return L"0x" + std::to_wstring(vk);
}

std::wstring hotkeyText(uint32_t packed) {
    if (!packed) return L"—";
    const uint32_t mods = hotkeyMods(packed), vk = hotkeyVk(packed);
    std::wstring s;
    if (mods & MOD_CONTROL) s += L"Ctrl + ";
    if (mods & MOD_ALT) s += L"Alt + ";
    if (mods & MOD_SHIFT) s += L"Shift + ";
    if (mods & MOD_WIN) s += L"Win + ";
    return s + vkName(vk);
}

}  // namespace vl::win
