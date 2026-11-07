// OS integration helpers: paths, persistent storage, autostart, power / session
// awareness, process QoS, resource stats, theme queries.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/face_db.hpp"
#include "core/settings.hpp"
#include "platform/win/win_common.hpp"

namespace vl::win {

std::filesystem::path appDataDir();   // %LOCALAPPDATA%\VibeLegion (created)
std::filesystem::path exeDir();

// Settings (plain JSON, atomic replace) and faces (DPAPI, current user only).
Settings loadSettings();
bool saveSettings(const Settings& s);
FaceDatabase loadFaces();
bool saveFaces(const FaceDatabase& db);

// HKCU\...\Run entry with --minimized.
bool setAutostart(bool on);
bool autostartEnabled();

// Process-wide power policy for the chosen profile:
//   Eco/Balanced -> EcoQoS (efficiency mode, low clocks), normal priority
//   Responsive   -> no throttling, ABOVE_NORMAL priority (games won't starve us)
void applyProcessPolicy(PowerProfile p);

struct PowerState {
    bool onBattery = false;
    bool saver = false;  // battery saver / energy saver active
};
PowerState queryPower();

// True while a full-screen D3D app / presentation mode / "busy" state is active.
bool fullscreenAppRunning();

struct ProcessStats {
    double cpuPercent = 0;  // of the whole machine, like Task Manager
    double workingSetMb = 0;
    double privateMb = 0;
};
class ProcessMeter {
public:
    ProcessStats sample();  // call ~1 Hz

private:
    uint64_t lastProc_ = 0, lastWall_ = 0;
    ProcessStats last_;
};
void trimWorkingSet();

// Theme.
bool systemUsesLightTheme();
uint32_t systemAccentColor();  // 0xRRGGBB
void enableDarkMenus(bool dark);  // uxtheme ordinals (Win10 1903+), no-op if missing

void openUri(const wchar_t* uri);  // ms-settings:..., https://...
std::wstring vkName(uint32_t vk);
std::wstring hotkeyText(uint32_t packed);

}  // namespace vl::win
