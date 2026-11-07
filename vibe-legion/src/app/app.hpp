// Application root: owns camera, vision pipeline, tray icon, windows and settings,
// and translates OS events (lock, power, full-screen games, hotkeys) into
// pipeline state. Everything here runs on the UI thread.
#pragma once

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "core/face_db.hpp"
#include "core/settings.hpp"
#include "platform/win/camera_mf.hpp"
#include "platform/win/system.hpp"
#include "platform/win/tray.hpp"
#include "ui/theme.hpp"
#include "vision/pipeline.hpp"

namespace vl {

namespace ui {
class MainWindow;
class Flyout;
class Osd;
}  // namespace ui

struct LogEntry {
    std::wstring title;
    std::wstring detail;
    std::wstring glyph;
    int64_t unixMs = 0;
};

class App {
public:
    App();
    ~App();
    int run(HINSTANCE inst, bool startMinimized);

    // ---- used by the UI
    Settings& settings() { return settings_; }
    void settingsChanged();               // debounced save + apply
    FaceDatabase& faces() { return faces_; }
    void facesChanged();
    const PipelineSnapshot& snapshot() const { return snap_; }
    const win::ProcessStats& stats() const { return stats_; }
    const std::vector<win::CameraDevice>& cameras();
    void refreshCameras();
    const std::deque<LogEntry>& log() const { return log_; }
    const ui::Palette& palette() const { return palette_; }

    bool paused() const;
    std::wstring pauseText() const;
    void setUserPaused(bool p);
    bool userPaused() const { return userPaused_; }

    void setPreviewWanted(bool on);
    void startEnrollment(const std::string& personId, int samples = 12);
    void startCalibration(float knownCm);
    void startRecording(const std::string& gestureId);
    void cancelTasks();
    std::wstring lastTaskMessage() const { return taskMessage_; }

    void testAction(const Action& a);
    void showMain(int page = -1);
    void toggleFlyout();
    void quit();
    void openDataFolder();
    void resetSettings();
    HICON appIcon(bool large) const { return large ? iconLarge_ : iconSmall_; }
    HINSTANCE instance() const { return inst_; }
    HWND messageWindow() const { return msgWnd_; }

    // Live info for the UI.
    win::PowerState power() const { return power_; }
    bool micaWanted() const { return settings_.ui.mica; }

private:
    static LRESULT CALLBACK msgProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(UINT, WPARAM, LPARAM);
    void applySettings(bool full);
    void updatePauseState();
    void updateTrayIcon();
    void registerHotkeys();
    void onGesture(const GestureEvent& e);
    void onSnapshot();
    void showTrayMenu();
    void postToUi(std::function<void()> fn);
    void addLog(LogEntry e);

    HINSTANCE inst_ = nullptr;
    HWND msgWnd_ = nullptr;

    Settings settings_;
    FaceDatabase faces_;
    ui::Palette palette_;

    FrameMailbox mailbox_;
    std::unique_ptr<win::MfCamera> camera_;
    std::unique_ptr<VisionPipeline> pipeline_;
    PipelineSnapshot snap_;

    win::TrayIcon tray_;
    HICON trayIcon_ = nullptr, iconSmall_ = nullptr, iconLarge_ = nullptr;
    win::IconBadge trayBadge_ = win::IconBadge::None;
    bool trayDark_ = false;

    std::unique_ptr<ui::MainWindow> main_;
    std::unique_ptr<ui::Flyout> flyout_;
    std::unique_ptr<ui::Osd> osd_;

    win::ProcessMeter meter_;
    win::ProcessStats stats_;
    win::PowerState power_;
    std::vector<win::CameraDevice> cameraList_;
    bool camerasLoaded_ = false;

    bool userPaused_ = false, locked_ = false, fullscreen_ = false, displayOff_ = false;
    bool lastPaused_ = false;
    PowerProfile appliedProfile_ = PowerProfile::Balanced;
    bool profileApplied_ = false;
    std::deque<LogEntry> log_;
    std::wstring taskMessage_;
    HPOWERNOTIFY powerNotify_[3] = {};
    bool quitting_ = false;
    bool settingsDirty_ = false;
    std::string lastHint_;
};

}  // namespace vl
