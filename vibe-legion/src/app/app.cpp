#include "app/app.hpp"

#include <shellapi.h>
#include <wtsapi32.h>

#include <atomic>
#include <chrono>
#include <ctime>

#include "app/actions.hpp"
#include "ui/flyout.hpp"
#include "ui/main_window.hpp"

namespace vl {

using namespace win;

namespace {

constexpr UINT_PTR kTimerTick = 1, kTimerSave = 2, kTimerTest = 3;
constexpr int kHotkeyToggle = 1, kHotkeyOpen = 2;
enum MenuId : UINT { IdOpen = 100, IdPause, IdNightOff, IdNightAuto, IdNightSoft, IdNightIr, IdEco, IdBalanced, IdResponsive, IdQuit };

std::atomic<bool> g_snapshotPending{false};

bool taskbarLight() {
    DWORD v = 0, sz = sizeof v;
    return RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                        L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz) == ERROR_SUCCESS && v != 0;
}

int64_t unixMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

}  // namespace

App::App() = default;

App::~App() {
    if (pipeline_) pipeline_->stop();
    if (camera_) camera_->stop();
    for (HICON i : {trayIcon_, iconSmall_, iconLarge_})
        if (i) DestroyIcon(i);
}

// --------------------------------------------------------------------- startup

int App::run(HINSTANCE inst, bool startMinimized) {
    inst_ = inst;
    const bool firstRun = !std::filesystem::exists(appDataDir() / L"settings.json");
    settings_ = loadSettings();
    faces_ = loadFaces();
    // Pre-v4 files do not say which SFace build enrolled the faces: it was int8.
    if (settings_.faces.embedModel.empty()) {
        settings_.faces.embedModel = faces_.people.empty() ? "fp32" : "int8";
        settingsDirty_ = true;
    }
    palette_ = ui::paletteFor(settings_.ui);
    enableDarkMenus(palette_.dark);

    const uint32_t accent = ui::accentFor(settings_.ui);
    iconSmall_ = makeIcon(GetSystemMetrics(SM_CXSMICON) * 2, true, false, accent, IconBadge::None);
    iconLarge_ = makeIcon(GetSystemMetrics(SM_CXICON) * 2, true, false, accent, IconBadge::None);

    WNDCLASSEXW wc{sizeof wc};
    wc.lpfnWndProc = msgProc;
    wc.hInstance = inst;
    wc.lpszClassName = L"VibeHandMsg";
    RegisterClassExW(&wc);
    // A hidden top-level window (not HWND_MESSAGE): it must receive the
    // "TaskbarCreated" broadcast to restore the tray icon after Explorer restarts.
    msgWnd_ = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"Vibe Hand", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
                              inst, this);
    if (!msgWnd_) return 1;

    camera_ = std::make_unique<MfCamera>(mailbox_);
    camera_->configure(settings_.camera);

    PipelineCallbacks cb;
    cb.onGesture = [this](const GestureEvent& e) { PostMessageW(msgWnd_, WM_VL_GESTURE, 0, reinterpret_cast<LPARAM>(new GestureEvent(e))); };
    cb.onSnapshot = [this] {
        if (!g_snapshotPending.exchange(true)) PostMessageW(msgWnd_, WM_VL_SNAPSHOT, 0, 0);
    };
    cb.onEnrolled = [this](const std::string& id, std::vector<Embedding> embs, bool ir) {
        postToUi([this, id, embs = std::move(embs), ir]() mutable {
            Person* p = faces_.find(id);
            if (!p) return;
            auto& list = ir ? p->ir : p->rgb;
            list.insert(list.end(), embs.begin(), embs.end());
            if (list.size() > FaceDatabase::kMaxSamplesPerModality)
                list.erase(list.begin(), list.end() - static_cast<long>(FaceDatabase::kMaxSamplesPerModality));
            facesChanged();
            taskMessage_ = L"Лицо «" + widen(p->name) + L"» запомнено: " + std::to_wstring(embs.size()) + L" снимков" +
                           (ir ? L" (ИК)" : L"");
            if (settings_.ui.notifications) tray_.notify(L"Vibe Hand", taskMessage_);
            if (main_ && main_->hwnd()) main_->state().enrollingPerson.clear();
        });
    };
    cb.onRecorded = [this](const std::string& id, bool ok, CustomGesture learned, std::string report) {
        postToUi([this, id, ok, learned = std::move(learned), report = std::move(report)] {
            for (auto& g : settings_.customGestures) {
                if (g.id != id) continue;
                if (!ok) {
                    taskMessage_ = L"«" + widen(g.name) + L"» не записан: " + widen(report);
                } else {
                    // Keep the name, id and the user's strictness; take everything learned.
                    const float strictness = g.strictness;
                    const std::string name = g.name;
                    g = learned;
                    g.id = id;
                    g.name = name;
                    g.strictness = strictness;
                    taskMessage_ = L"«" + widen(g.name) + L"» записан: " + widen(report) + L". В правиле: «Свой: " +
                                   widen(g.name) + L"»";
                }
            }
            if (main_ && main_->hwnd()) main_->state().recordingGesture.clear();
            settingsChanged();
        });
    };    pipeline_ = std::make_unique<VisionPipeline>(exeDir() / L"models", mailbox_, camera_.get(), std::move(cb));
    pipeline_->setSettings(std::make_shared<Settings>(settings_));
    pipeline_->setFaceDb(std::make_shared<FaceDatabase>(faces_));

    trayDark_ = taskbarLight();
    trayIcon_ = makeIcon(GetSystemMetrics(SM_CXSMICON), false, trayDark_, accent, IconBadge::Idle);
    trayBadge_ = IconBadge::Idle;
    tray_.create(msgWnd_, WM_VL_TRAY, trayIcon_, L"Vibe Hand");

    powerNotify_[0] = RegisterPowerSettingNotification(msgWnd_, &kAcDcPowerSource, DEVICE_NOTIFY_WINDOW_HANDLE);
    powerNotify_[1] = RegisterPowerSettingNotification(msgWnd_, &kPowerSavingStatus, DEVICE_NOTIFY_WINDOW_HANDLE);
    powerNotify_[2] = RegisterPowerSettingNotification(msgWnd_, &kConsoleDisplayState, DEVICE_NOTIFY_WINDOW_HANDLE);
    WTSRegisterSessionNotification(msgWnd_, NOTIFY_FOR_THIS_SESSION);

    flyout_ = std::make_unique<ui::Flyout>(*this);
    osd_ = std::make_unique<ui::Osd>(*this);
    main_ = std::make_unique<ui::MainWindow>(*this);

    power_ = queryPower();
    pipeline_->setEco(power_.onBattery || power_.saver);
    applySettings(true);
    registerHotkeys();
    camera_->start();
    pipeline_->start();
    updatePauseState();
    SetTimer(msgWnd_, kTimerTick, 1000, nullptr);

    // Re-register on every start: the Run entry must point at *this* exe (the app may
    // have been rebuilt or moved), and the pre-rename "VibeLegion" entry goes away.
    setAutostart(settings_.ui.autostart);
    if (firstRun) {
        saveSettings(settings_);
        showMain(ui::PageOverview);
        tray_.notify(L"Vibe Hand работает", L"Значок рядом с часами: щелчок — быстрые настройки, правый щелчок — меню.");
    } else if (!startMinimized && !settings_.ui.startMinimized) {
        showMain();
    }

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    // Shutdown.
    KillTimer(msgWnd_, kTimerTick);
    if (settingsDirty_) saveSettings(settings_);
    pipeline_->stop();
    camera_->stop();
    tray_.remove();
    for (auto h : powerNotify_)
        if (h) UnregisterPowerSettingNotification(h);
    WTSUnRegisterSessionNotification(msgWnd_);
    main_.reset();
    flyout_.reset();
    osd_.reset();
    return 0;
}

// --------------------------------------------------------------------- settings

void App::settingsChanged() {
    settingsDirty_ = true;
    applySettings(false);
    SetTimer(msgWnd_, kTimerSave, 500, nullptr);
}

void App::applySettings(bool full) {
    const ui::Palette old = palette_;
    palette_ = ui::paletteFor(settings_.ui);
    pipeline_->setSettings(std::make_shared<Settings>(settings_));
    camera_->configure(settings_.camera);
    const PowerProfile prof = (power_.onBattery || power_.saver) && settings_.perf.ecoOnBattery ? PowerProfile::Eco : settings_.perf.profile;
    if (!profileApplied_ || prof != appliedProfile_) {
        applyProcessPolicy(prof);
        appliedProfile_ = prof;
        profileApplied_ = true;
    }
    const bool themeChanged = old.dark != palette_.dark || old.accent.r != palette_.accent.r || old.accent.g != palette_.accent.g ||
                              old.accent.b != palette_.accent.b;
    if (themeChanged || full) {
        enableDarkMenus(palette_.dark);
        if (main_) main_->refreshTheme();
        if (flyout_) flyout_->refreshTheme();
        if (osd_) osd_->refreshTheme();
        trayBadge_ = IconBadge::None;  // force the tray icon to redraw with the new accent
        updateTrayIcon();
    } else if (main_ && main_->hwnd()) {
        main_->refreshTheme();  // Mica toggle
    }
    updatePauseState();
}

void App::facesChanged() {
    saveFaces(faces_);
    pipeline_->setFaceDb(std::make_shared<FaceDatabase>(faces_));
}

void App::resetSettings() {
    settings_ = Settings::defaults();
    settingsChanged();
    registerHotkeys();
}

void App::registerHotkeys() {
    UnregisterHotKey(msgWnd_, kHotkeyToggle);
    UnregisterHotKey(msgWnd_, kHotkeyOpen);
    auto reg = [&](int id, uint32_t hk) {
        if (hk && !RegisterHotKey(msgWnd_, id, hotkeyMods(hk) | MOD_NOREPEAT, hotkeyVk(hk)) && settings_.ui.notifications)
            tray_.notify(L"Горячая клавиша занята", hotkeyText(hk) + L" уже использует другое приложение.");
    };
    reg(kHotkeyToggle, settings_.ui.hotkeyToggle);
    reg(kHotkeyOpen, settings_.ui.hotkeyOpen);
}

const std::vector<CameraDevice>& App::cameras() {
    if (!camerasLoaded_) refreshCameras();
    return cameraList_;
}

void App::refreshCameras() {
    cameraList_ = MfCamera::enumerate();
    camerasLoaded_ = true;
}

// --------------------------------------------------------------------- pause logic

bool App::paused() const {
    return userPaused_ || !settings_.enabled || (locked_ && settings_.perf.pauseWhenLocked) || displayOff_ ||
           (fullscreen_ && settings_.perf.pauseInFullscreen);
}

std::wstring App::pauseText() const {
    if (userPaused_ || !settings_.enabled) return L"Распознавание выключено";
    if (locked_ && settings_.perf.pauseWhenLocked) return L"ПК заблокирован — камера выключена";
    if (displayOff_) return L"Экран выключен";
    if (fullscreen_ && settings_.perf.pauseInFullscreen) return L"Полноэкранная игра или презентация";
    return L"";
}

void App::setUserPaused(bool p) {
    userPaused_ = p;
    updatePauseState();
}

void App::updatePauseState() {
    if (!pipeline_) return;
    const bool p = paused();
    pipeline_->setPaused(p, narrow(pauseText()));
    camera_->setStreaming(!p);  // release the camera: LED off, sensor and USB idle
    if (p != lastPaused_) {
        lastPaused_ = p;
        updateTrayIcon();
        if (main_) main_->invalidate();
        if (flyout_) flyout_->invalidate();
    }
}

void App::updateTrayIcon() {
    IconBadge b = IconBadge::Idle;
    if (paused()) b = IconBadge::Paused;
    else if (snap_.mode == PipelineMode::ModelError || snap_.mode == PipelineMode::NoCamera || snap_.mode == PipelineMode::Covered)
        b = IconBadge::Error;
    else if (snap_.mode == PipelineMode::Active) b = IconBadge::Active;
    else if (snap_.nightActive) b = IconBadge::Night;
    const bool dark = taskbarLight();
    std::wstring tip = L"Vibe Hand — " + (paused() ? pauseText() : widen(modeTextRu(snap_.mode)));
    if (b != trayBadge_ || dark != trayDark_) {
        HICON old = trayIcon_;
        trayIcon_ = makeIcon(GetSystemMetrics(SM_CXSMICON), false, dark, ui::accentFor(settings_.ui), b);
        trayBadge_ = b;
        trayDark_ = dark;
        tray_.update(trayIcon_, tip);
        if (old) DestroyIcon(old);
    } else {
        tray_.update(trayIcon_, tip);
    }
}

// --------------------------------------------------------------------- UI glue

void App::setPreviewWanted(bool on) {
    const bool want = on || (main_ && main_->visible() && !IsIconic(main_->hwnd())) || (flyout_ && flyout_->visible());
    pipeline_->setPreviewWanted(want);
}

void App::startEnrollment(const std::string& id, int durationMs) {
    taskMessage_.clear();
    pipeline_->startEnrollment(id, durationMs);
}
void App::startRecording(const std::string& id) {
    taskMessage_.clear();
    for (const auto& g : settings_.customGestures)
        if (g.id == id) pipeline_->startRecording(id, g.rotationInvariant, g.mirrorInvariant, CustomGesture::kRecordMs);
}
void App::cancelTasks() { if (pipeline_) pipeline_->cancelTasks(); }

void App::testAction(const Action& a) {
    // Give the user a moment to focus the target window (key combos go to the foreground app).
    static Action pending;
    pending = a;
    SetPropW(msgWnd_, L"vl.test", reinterpret_cast<HANDLE>(&pending));
    SetTimer(msgWnd_, kTimerTest, 1500, nullptr);
    if (osd_) osd_->showFired(actionGlyph(a.type), describeAction(a), L"Выполню через 1,5 с — переключитесь в нужное окно");
}

void App::showMain(int page) {
    if (flyout_) flyout_->hide();
    main_->show(page);
}

void App::toggleFlyout() { flyout_->toggle(tray_.anchorRect()); }

void App::quit() {
    quitting_ = true;
    if (main_) main_->destroy();
    if (flyout_) flyout_->destroy();
    if (osd_) osd_->destroy();
    DestroyWindow(msgWnd_);
}

void App::openDataFolder() { ShellExecuteW(nullptr, L"open", appDataDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL); }

void App::postToUi(std::function<void()> fn) {
    PostMessageW(msgWnd_, WM_VL_TASKDONE, 0, reinterpret_cast<LPARAM>(new std::function<void()>(std::move(fn))));
}

void App::addLog(LogEntry e) {
    log_.push_front(std::move(e));
    while (log_.size() > 8) log_.pop_back();
}

void App::onGesture(const GestureEvent& e) {
    if (e.action.type == ActionType::PauseRecognition) setUserPaused(true);
    else executeAction(e.action);
    std::wstring gname = findBuiltin(e.gesture) ? widen(findBuiltin(e.gesture)->nameRu) : L"Свой жест";
    if (e.gesture.rfind("custom:", 0) == 0)
        for (const auto& g : settings_.customGestures)
            if ("custom:" + g.id == e.gesture) gname = widen(g.name);
    const std::wstring detail = gname;
    const std::wstring title = e.ruleName.empty() ? describeAction(e.action) : widen(e.ruleName);
    addLog({title, detail, actionGlyph(e.action.type), unixMs()});
    if (settings_.ui.osd) osd_->showFired(actionGlyph(e.action.type), describeAction(e.action), detail);
    if (main_) main_->invalidate();
}

void App::onSnapshot() {
    g_snapshotPending = false;
    snap_ = pipeline_->snapshot();
    if (main_ && main_->visible()) main_->invalidate();
    if (flyout_ && flyout_->visible()) flyout_->invalidate();
    // OSD hold ring for rules that take a noticeable time to charge.
    if (settings_.ui.osd && !paused()) {
        float best = 0.f;
        std::string rid;
        for (const auto& [id, v] : snap_.ruleProgress)
            if (v > best) { best = v; rid = id; }
        const GestureRule* r = nullptr;
        for (const auto& x : settings_.rules)
            if (x.id == rid) r = &x;
        if (r && r->holdMs >= 250 && best > 0.12f && best < 1.f)
            osd_->showProgress(r->name.empty() ? describeAction(r->action) : widen(r->name), best);
        else
            osd_->hideProgress();
    }
    // Surface camera problems once.
    const std::string hint = snap_.mode == PipelineMode::Covered ? "covered" : snap_.mode == PipelineMode::NoCamera ? "nocam" : "";
    if (hint != lastHint_) {
        lastHint_ = hint;
        updateTrayIcon();
        if (!hint.empty() && settings_.ui.notifications)
            tray_.notify(L"Vibe Hand", hint == "covered" ? L"Камера закрыта шторкой или выключена клавишей — жесты недоступны."
                                                           : widen(snap_.camera.empty() ? "Камера недоступна" : snap_.camera));
    } else if ((snap_.mode == PipelineMode::Active) != (trayBadge_ == IconBadge::Active)) {
        updateTrayIcon();
    }
}

void App::showTrayMenu() {
    HMENU m = CreatePopupMenu();
    HMENU night = CreatePopupMenu(), prof = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, IdOpen, L"Открыть Vibe Hand");
    SetMenuDefaultItem(m, IdOpen, FALSE);
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING | (userPaused_ ? MF_CHECKED : 0), IdPause,
                (L"Пауза распознавания\t" + hotkeyText(settings_.ui.hotkeyToggle)).c_str());
    const auto nm = settings_.camera.night;
    AppendMenuW(night, MF_STRING | (nm == NightMode::Off ? MF_CHECKED : 0), IdNightOff, L"Выключено");
    AppendMenuW(night, MF_STRING | (nm == NightMode::Auto ? MF_CHECKED : 0), IdNightAuto, L"Автоматически");
    AppendMenuW(night, MF_STRING | (nm == NightMode::Software ? MF_CHECKED : 0), IdNightSoft, L"Программное усиление");
    AppendMenuW(night, MF_STRING | (nm == NightMode::Infrared ? MF_CHECKED : 0), IdNightIr, L"ИК-камера");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(night), L"Ночное зрение");
    const auto pr = settings_.perf.profile;
    AppendMenuW(prof, MF_STRING | (pr == PowerProfile::Eco ? MF_CHECKED : 0), IdEco, L"Эко");
    AppendMenuW(prof, MF_STRING | (pr == PowerProfile::Balanced ? MF_CHECKED : 0), IdBalanced, L"Баланс");
    AppendMenuW(prof, MF_STRING | (pr == PowerProfile::Responsive ? MF_CHECKED : 0), IdResponsive, L"Отклик");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(prof), L"Профиль питания");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, IdQuit, L"Выход");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(msgWnd_);
    const UINT cmd = TrackPopupMenuEx(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, msgWnd_, nullptr);
    PostMessageW(msgWnd_, WM_NULL, 0, 0);
    DestroyMenu(m);  // destroys submenus too
    switch (cmd) {
        case IdOpen: showMain(); break;
        case IdPause: setUserPaused(!userPaused_); break;
        case IdNightOff: settings_.camera.night = NightMode::Off; settingsChanged(); break;
        case IdNightAuto: settings_.camera.night = NightMode::Auto; settingsChanged(); break;
        case IdNightSoft: settings_.camera.night = NightMode::Software; settingsChanged(); break;
        case IdNightIr: settings_.camera.night = NightMode::Infrared; settingsChanged(); break;
        case IdEco: settings_.perf.profile = PowerProfile::Eco; settingsChanged(); break;
        case IdBalanced: settings_.perf.profile = PowerProfile::Balanced; settingsChanged(); break;
        case IdResponsive: settings_.perf.profile = PowerProfile::Responsive; settingsChanged(); break;
        case IdQuit: quit(); break;
        default: break;
    }
}

// --------------------------------------------------------------------- messages

LRESULT CALLBACK App::msgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
        return DefWindowProcW(h, msg, wp, lp);
    }
    auto* self = reinterpret_cast<App*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (!self || !self->msgWnd_) return DefWindowProcW(h, msg, wp, lp);
    return self->handle(msg, wp, lp);
}

LRESULT App::handle(UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == TrayIcon::taskbarCreatedMsg()) {
        tray_.recreate();
        return 0;
    }
    switch (msg) {
        case WM_VL_TRAY:
            switch (LOWORD(lp)) {
                case NIN_SELECT:
                case NIN_KEYSELECT: toggleFlyout(); break;
                case WM_CONTEXTMENU: showTrayMenu(); break;
                default: break;
            }
            return 0;
        case WM_VL_SNAPSHOT:
            onSnapshot();
            return 0;
        case WM_VL_GESTURE: {
            std::unique_ptr<GestureEvent> e(reinterpret_cast<GestureEvent*>(lp));
            if (e && !paused()) onGesture(*e);
            return 0;
        }
        case WM_VL_TASKDONE: {
            std::unique_ptr<std::function<void()>> fn(reinterpret_cast<std::function<void()>*>(lp));
            if (fn && *fn) (*fn)();
            if (main_) main_->invalidate();
            return 0;
        }
        case WM_VL_SHOWMAIN:
            showMain();
            return 0;
        case WM_HOTKEY:
            if (wp == kHotkeyToggle) {
                setUserPaused(!userPaused_);
                if (settings_.ui.osd)
                    osd_->showFired(userPaused_ ? L"" : L"", userPaused_ ? L"Распознавание на паузе" : L"Распознавание включено",
                                    hotkeyText(settings_.ui.hotkeyToggle));
            } else if (wp == kHotkeyOpen) {
                showMain();
            }
            return 0;
        case WM_TIMER:
            if (wp == kTimerTick) {
                stats_ = meter_.sample();
                fullscreen_ = settings_.perf.pauseInFullscreen && fullscreenAppRunning();
                const PowerState ps = queryPower();
                if (ps.onBattery != power_.onBattery || ps.saver != power_.saver) {
                    power_ = ps;
                    pipeline_->setEco(ps.onBattery || ps.saver);
                    applySettings(false);
                }
                updatePauseState();
                if (main_ && main_->visible()) main_->invalidate();
            } else if (wp == kTimerSave) {
                KillTimer(msgWnd_, kTimerSave);
                if (settingsDirty_) {
                    saveSettings(settings_);
                    settingsDirty_ = false;
                    if (autostartEnabled() != settings_.ui.autostart) setAutostart(settings_.ui.autostart);
                    registerHotkeys();
                }
            } else if (wp == kTimerTest) {
                KillTimer(msgWnd_, kTimerTest);
                if (auto* a = reinterpret_cast<Action*>(GetPropW(msgWnd_, L"vl.test"))) executeAction(*a);
                RemovePropW(msgWnd_, L"vl.test");
            }
            return 0;
        case WM_POWERBROADCAST:
            if (wp == PBT_POWERSETTINGCHANGE) {
                const auto* s = reinterpret_cast<const POWERBROADCAST_SETTING*>(lp);
                if (s->PowerSetting == kConsoleDisplayState) displayOff_ = s->DataLength >= 4 && s->Data[0] == 0;
                else {
                    power_ = queryPower();
                    pipeline_->setEco(power_.onBattery || power_.saver);
                    applySettings(false);
                }
                updatePauseState();
            } else if (wp == PBT_APMSUSPEND) {
                camera_->setStreaming(false);
            } else if (wp == PBT_APMRESUMEAUTOMATIC) {
                updatePauseState();
            }
            return TRUE;
        case WM_WTSSESSION_CHANGE:
            if (wp == WTS_SESSION_LOCK) locked_ = true;
            else if (wp == WTS_SESSION_UNLOCK) locked_ = false;
            updatePauseState();
            return 0;
        case WM_SETTINGCHANGE:
            if (lp && wcscmp(reinterpret_cast<const wchar_t*>(lp), L"ImmersiveColorSet") == 0) applySettings(true);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(msgWnd_, msg, wp, lp);
}

}  // namespace vl
