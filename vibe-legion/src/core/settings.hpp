// All user-tunable settings. Serialised to %LOCALAPPDATA%\VibeHand\settings.json.
// Unknown/missing keys fall back to defaults, so old files keep working.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/json.hpp"

namespace vl {

// ------------------------------------------------------------------ actions

enum class ActionType {
    None,
    MediaPlayPause, MediaNext, MediaPrev, MediaStop,
    VolumeUp, VolumeDown, VolumeMute,
    MicMute,        // Win+Alt+K (Windows 11 system mic toggle for supported apps)
    KeyCombo,       // arbitrary keys, e.g. Ctrl+Shift+M
    ScrollUp, ScrollDown,
    LaunchApp,      // path + args
    LockPC,
    ShowDesktop,    // Win+D
    TaskView,       // Win+Tab
    Screenshot,     // Win+Shift+S
    SwitchWindow,   // Alt+Tab
    PauseRecognition,
    // Added in settings v2 (appended: ids are stored by name, order is free).
    NextWindow,     // cycle open windows forward (swipe-friendly, no Alt+Tab UI)
    PrevWindow,     // ... and back
    MinimizeWindow,
    MaximizeWindow, // maximise / restore the foreground window
    CloseWindow,    // Alt+F4
    NextDesktop,    // Ctrl+Win+Right
    PrevDesktop,    // Ctrl+Win+Left
    StartMenu,      // Win
    Search,         // Win+S
    FileExplorer,   // Win+E
    BrowserBack,
    BrowserForward,
    ScreenshotFull, // Win+PrtScn: whole screen saved to Pictures\Screenshots
    // v3
    ZoomIn,         // Ctrl + '+'
    ZoomOut,        // Ctrl + '-'
    kCount
};

const char* actionId(ActionType t);
ActionType actionFromId(const std::string& id);

struct Action {
    ActionType type = ActionType::None;
    std::vector<uint16_t> keys;  // virtual-key codes, modifiers first (KeyCombo)
    std::string path;            // LaunchApp: file, or "shell:AppsFolder\<AppUserModelID>" for Start menu apps
    std::string args;
    std::string label;           // LaunchApp: display name picked from the Start menu list
};

// ------------------------------------------------------------------ gestures

enum class HandSide { Any, Left, Right };
enum class PersonFilter { Anyone, AnyKnown, Specific };

struct GestureRule {
    std::string id;          // stable random id
    std::string name;        // user label
    bool enabled = true;
    std::string gesture = "open_palm";  // built-in id or "custom:<id>"
    HandSide hand = HandSide::Any;
    int holdMs = 320;
    int cooldownMs = 1700;
    bool repeat = false;     // fire repeatedly while held (volume, scroll)
    int repeatMs = 250;
    PersonFilter person = PersonFilter::Anyone;
    std::string personId;    // for Specific
    bool requireLooking = false;
    Action action;
};

struct CustomGesture {
    std::string id;
    std::string name;
    bool rotationInvariant = true;
    bool mirrorInvariant = true;   // left and right hands match the same template
    float tolerance = 0.22f;       // RMS distance in palm-length units
    std::vector<std::array<float, 42>> samples;

    static constexpr int kRecordMs = 8000;     // how long a recording lasts
    static constexpr size_t kMaxSamples = 64;  // kept after de-duplication
};

// ------------------------------------------------------------------ subsystems

enum class NightMode { Off, Auto, Software, Infrared };
enum class PowerProfile { Eco, Balanced, Responsive };
enum class ComputeDevice { Cpu, Gpu, Npu };  // CPU EP / DirectML (Radeon iGPU) / VitisAI (XDNA NPU)
enum class FaceGate { Off, AnyFace, KnownOnly };
enum class Theme { Dark, Light, System };

struct CameraSettings {
    std::string deviceId;      // symbolic link; empty = first colour camera
    std::string irDeviceId;    // empty = auto-detect Windows Hello sensor
    int width = 640, height = 360;
    bool mirrorPreview = true;
    bool swapHandedness = false;   // if the model reports left/right the other way round
    NightMode night = NightMode::Auto;
    float nightThreshold = 0.22f;  // mean luminance below which Auto engages
    float nightStrength = 0.7f;
    bool nightLocalContrast = true;
    bool nightDenoise = true;
    bool irAmbientSuppression = true;
    bool irTorch = true;           // ask driver to light the IR emitter
    bool hardwareLowLight = true;  // raise gain/backlight compensation in the dark
};

struct FaceSettings {
    FaceGate gate = FaceGate::AnyFace;
    float matchThreshold = 0.40f;  // cosine similarity (SFace reference: 0.363)
    float lookingTolerance = 0.45f;  // |yaw|,|pitch| below this = looking at screen
    int recognizeEveryMs = 1000;   // v3: was 2000 (fp32 SFace is 7x cheaper)
};

struct PerformanceSettings {
    PowerProfile profile = PowerProfile::Balanced;
    int activeFps = 30;
    int idleFps = 8;           // v3: was 4; a raised hand is picked up within ~125 ms
    int sleepFps = 1;
    int idleAfterMs = 1500;    // no hand -> idle rate
    int sleepAfterSec = 90;    // no face and no hand -> sleep rate
    bool ecoOnBattery = true;
    bool pauseInFullscreen = true;
    bool pauseWhenLocked = true;  // releases the camera (LED off)
    ComputeDevice device = ComputeDevice::Cpu;
    int threads = 1;
    bool int8Models = false;
};

struct UiSettings {
    Theme theme = Theme::Dark;
    int accent = 0;          // 0 = Windows accent, 1.. = presets
    bool mica = true;
    bool osd = true;         // on-screen confirmation when a gesture fires
    bool notifications = true;
    bool autostart = true;
    bool startMinimized = true;
    uint32_t hotkeyToggle = 0x0347;   // packed (mods << 8) | vk; MOD_CONTROL|MOD_ALT + G
    uint32_t hotkeyOpen = 0x0356;     // Ctrl+Alt+V
};

struct Settings {
    // 2: no distance window, swipe rules, window actions; 3: two-hand rules, zoom, faster idle rate
    static constexpr int kVersion = 3;
    int version = kVersion;
    bool enabled = true;  // global recognition on/off
    CameraSettings camera;
    FaceSettings faces;
    PerformanceSettings perf;
    UiSettings ui;
    std::vector<GestureRule> rules;
    std::vector<CustomGesture> customGestures;

    static Settings defaults();
    json::Value toJson() const;
    static Settings fromJson(const json::Value& v);
};

// Hotkey packing helpers (mods = MOD_* bits from WinUser.h).
inline uint32_t packHotkey(uint32_t mods, uint32_t vk) { return (mods << 8) | (vk & 0xFF); }
inline uint32_t hotkeyMods(uint32_t hk) { return hk >> 8; }
inline uint32_t hotkeyVk(uint32_t hk) { return hk & 0xFF; }

std::string newId();  // short random id for rules/people/templates

}  // namespace vl
