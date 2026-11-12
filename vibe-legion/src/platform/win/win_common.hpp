// Common Windows includes, COM smart pointer, string conversion and constants that
// older SDKs / MinGW headers do not define (values from the Windows 11 SDK).
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wrl/client.h>

#include <filesystem>
#include <string>
#include <string_view>

// Thread power throttling (EcoQoS) is missing from some header sets.
#ifndef THREAD_POWER_THROTTLING_CURRENT_VERSION
#define THREAD_POWER_THROTTLING_CURRENT_VERSION 1
#define THREAD_POWER_THROTTLING_EXECUTION_SPEED 0x1
typedef struct _THREAD_POWER_THROTTLING_STATE {
    ULONG Version;
    ULONG ControlMask;
    ULONG StateMask;
} THREAD_POWER_THROTTLING_STATE;
#endif

namespace vl::win {

using Microsoft::WRL::ComPtr;

std::wstring widen(std::string_view utf8);
std::string narrow(std::wstring_view wide);

// DWM attributes (dwmapi.h, Windows 11 22H2+). Plain ints so they compile everywhere.
constexpr DWORD kDwmUseImmersiveDarkMode = 20;
constexpr DWORD kDwmWindowCornerPreference = 33;
constexpr DWORD kDwmBorderColor = 34;
constexpr DWORD kDwmCaptionColor = 35;
constexpr DWORD kDwmSystemBackdropType = 38;
constexpr int kCornerRound = 2, kCornerRoundSmall = 3;
constexpr int kBackdropNone = 1, kBackdropMica = 2, kBackdropAcrylic = 3, kBackdropMicaAlt = 4;

// Media Foundation / KS GUIDs not present in every header set.
// KSCATEGORY_SENSOR_CAMERA: IR / depth "sensor" cameras (Windows Hello).
inline constexpr GUID kCategorySensorCamera = {0x24e552d7, 0x6523, 0x47f7, {0xa6, 0x47, 0xd3, 0x46, 0x5b, 0xf1, 0xf5, 0xca}};
// GUID_POWER_SAVING_STATUS: battery saver / energy saver on/off.
inline constexpr GUID kPowerSavingStatus = {0xe00958c0, 0xc213, 0x4ace, {0xac, 0x77, 0xfe, 0xcc, 0xed, 0x2e, 0xee, 0xa5}};
// GUID_ACDC_POWER_SOURCE.
inline constexpr GUID kAcDcPowerSource = {0x5d3e9a59, 0xe9d5, 0x4b00, {0xa6, 0xbd, 0xff, 0x34, 0xff, 0x51, 0x65, 0x48}};
// GUID_CONSOLE_DISPLAY_STATE.
inline constexpr GUID kConsoleDisplayState = {0x6fe69556, 0x704a, 0x47a0, {0x8f, 0x24, 0xc2, 0x8d, 0x93, 0x6f, 0xda, 0x47}};

// Private window messages shared by app / UI.
enum : UINT {
    WM_VL_TRAY = WM_APP + 1,
    WM_VL_SNAPSHOT,       // pipeline produced a new snapshot
    WM_VL_GESTURE,        // lParam: heap GestureEvent*
    WM_VL_TASKDONE,       // lParam: heap std::function<void()>* to run on the UI thread
    WM_VL_SHOWMAIN,       // second instance asked us to show up
    WM_VL_VOICE,          // voice engine changed state / level: repaint
};

}  // namespace vl::win
