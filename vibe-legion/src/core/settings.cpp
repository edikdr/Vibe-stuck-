#include "core/settings.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>

namespace vl {

namespace {

const char* const kActionIds[] = {
    "none", "media_play_pause", "media_next", "media_prev", "media_stop",
    "volume_up", "volume_down", "volume_mute", "mic_mute", "key_combo",
    "scroll_up", "scroll_down", "launch_app", "lock_pc", "show_desktop",
    "task_view", "screenshot", "switch_window", "pause_recognition"};
static_assert(sizeof(kActionIds) / sizeof(kActionIds[0]) == static_cast<size_t>(ActionType::kCount));

template <typename E, size_t N>
E enumFrom(const json::Value& v, const char* const (&names)[N], E def) {
    const std::string& s = v.asString();
    for (size_t i = 0; i < N; ++i)
        if (s == names[i]) return static_cast<E>(i);
    return def;
}

template <typename E, size_t N>
const char* enumName(E e, const char* const (&names)[N]) {
    const size_t i = static_cast<size_t>(e);
    return i < N ? names[i] : names[0];
}

const char* const kHand[] = {"any", "left", "right"};
const char* const kPerson[] = {"anyone", "any_known", "specific"};
const char* const kNight[] = {"off", "auto", "software", "infrared"};
const char* const kPower[] = {"eco", "balanced", "responsive"};
const char* const kDevice[] = {"cpu", "gpu", "npu"};
const char* const kGate[] = {"off", "any_face", "known_only"};
const char* const kTheme[] = {"dark", "light", "system"};

float clampf(float v, float lo, float hi) { return std::min(hi, std::max(lo, v)); }
int clampi(int v, int lo, int hi) { return std::min(hi, std::max(lo, v)); }

json::Value actionToJson(const Action& a) {
    json::Value o;
    o.set("type", actionId(a.type));
    if (!a.keys.empty()) {
        json::Array k;
        for (auto vk : a.keys) k.push_back(static_cast<int>(vk));
        o.set("keys", std::move(k));
    }
    if (!a.path.empty()) o.set("path", a.path);
    if (!a.args.empty()) o.set("args", a.args);
    return o;
}

Action actionFromJson(const json::Value& v) {
    Action a;
    a.type = actionFromId(v["type"].asString());
    for (const auto& k : v["keys"].asArray()) a.keys.push_back(static_cast<uint16_t>(clampi(k.asInt(), 0, 255)));
    a.path = v["path"].asString();
    a.args = v["args"].asString();
    return a;
}

}  // namespace

const char* actionId(ActionType t) { return enumName(t, kActionIds); }

ActionType actionFromId(const std::string& id) {
    json::Value v(id);
    return enumFrom(v, kActionIds, ActionType::None);
}

std::string newId() {
    static std::mt19937_64 rng(static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()) ^
                               std::random_device{}());
    static const char* alphabet = "abcdefghijkmnpqrstuvwxyz23456789";
    std::string s;
    for (int i = 0; i < 10; ++i) s.push_back(alphabet[rng() % 32]);
    return s;
}

Settings Settings::defaults() {
    Settings s;
    // The example from the brief: open palm held at 60-70 cm toggles media.
    // Shipped *disabled*: gestures are meant to be set up in the app.
    GestureRule r;
    r.id = newId();
    r.name = "Ладонь — пауза / play";
    r.enabled = false;
    r.gesture = "open_palm";
    r.minCm = 60.f;
    r.maxCm = 70.f;
    r.holdMs = 320;
    r.cooldownMs = 1700;
    r.action.type = ActionType::MediaPlayPause;
    s.rules.push_back(r);
    return s;
}

json::Value Settings::toJson() const {
    json::Value root;
    root.set("version", version);
    root.set("enabled", enabled);

    json::Value cam;
    cam.set("device_id", camera.deviceId);
    cam.set("ir_device_id", camera.irDeviceId);
    cam.set("width", camera.width);
    cam.set("height", camera.height);
    cam.set("mirror_preview", camera.mirrorPreview);
    cam.set("swap_handedness", camera.swapHandedness);
    cam.set("night", enumName(camera.night, kNight));
    cam.set("night_threshold", camera.nightThreshold);
    cam.set("night_strength", camera.nightStrength);
    cam.set("night_local_contrast", camera.nightLocalContrast);
    cam.set("night_denoise", camera.nightDenoise);
    cam.set("ir_ambient_suppression", camera.irAmbientSuppression);
    cam.set("ir_torch", camera.irTorch);
    cam.set("hardware_low_light", camera.hardwareLowLight);
    root.set("camera", std::move(cam));

    json::Value dist;
    dist.set("hfov_deg", distance.hfovDeg);
    dist.set("calibration", distance.calibration);
    root.set("distance", std::move(dist));

    json::Value fc;
    fc.set("gate", enumName(faces.gate, kGate));
    fc.set("match_threshold", faces.matchThreshold);
    fc.set("looking_tolerance", faces.lookingTolerance);
    fc.set("recognize_every_ms", faces.recognizeEveryMs);
    root.set("faces", std::move(fc));

    json::Value pf;
    pf.set("profile", enumName(perf.profile, kPower));
    pf.set("active_fps", perf.activeFps);
    pf.set("idle_fps", perf.idleFps);
    pf.set("sleep_fps", perf.sleepFps);
    pf.set("idle_after_ms", perf.idleAfterMs);
    pf.set("sleep_after_sec", perf.sleepAfterSec);
    pf.set("eco_on_battery", perf.ecoOnBattery);
    pf.set("pause_in_fullscreen", perf.pauseInFullscreen);
    pf.set("pause_when_locked", perf.pauseWhenLocked);
    pf.set("device", enumName(perf.device, kDevice));
    pf.set("threads", perf.threads);
    pf.set("int8_models", perf.int8Models);
    root.set("performance", std::move(pf));

    json::Value ui_;
    ui_.set("theme", enumName(ui.theme, kTheme));
    ui_.set("accent", ui.accent);
    ui_.set("mica", ui.mica);
    ui_.set("osd", ui.osd);
    ui_.set("notifications", ui.notifications);
    ui_.set("autostart", ui.autostart);
    ui_.set("start_minimized", ui.startMinimized);
    ui_.set("hotkey_toggle", ui.hotkeyToggle);
    ui_.set("hotkey_open", ui.hotkeyOpen);
    root.set("ui", std::move(ui_));

    json::Array rs;
    for (const auto& r : rules) {
        json::Value o;
        o.set("id", r.id);
        o.set("name", r.name);
        o.set("enabled", r.enabled);
        o.set("gesture", r.gesture);
        o.set("hand", enumName(r.hand, kHand));
        o.set("use_distance", r.useDistance);
        o.set("min_cm", r.minCm);
        o.set("max_cm", r.maxCm);
        o.set("hold_ms", r.holdMs);
        o.set("cooldown_ms", r.cooldownMs);
        o.set("repeat", r.repeat);
        o.set("repeat_ms", r.repeatMs);
        o.set("person", enumName(r.person, kPerson));
        o.set("person_id", r.personId);
        o.set("require_looking", r.requireLooking);
        o.set("action", actionToJson(r.action));
        rs.push_back(std::move(o));
    }
    root.set("rules", std::move(rs));

    json::Array cg;
    for (const auto& g : customGestures) {
        json::Value o;
        o.set("id", g.id);
        o.set("name", g.name);
        o.set("rotation_invariant", g.rotationInvariant);
        o.set("mirror_invariant", g.mirrorInvariant);
        o.set("tolerance", g.tolerance);
        json::Array samples;
        for (const auto& smp : g.samples) {
            json::Array a;
            for (float f : smp) a.push_back(static_cast<double>(std::round(f * 10000.f) / 10000.f));
            samples.push_back(std::move(a));
        }
        o.set("samples", std::move(samples));
        cg.push_back(std::move(o));
    }
    root.set("custom_gestures", std::move(cg));
    return root;
}

Settings Settings::fromJson(const json::Value& v) {
    Settings s;  // empty rules: a file with "rules": [] stays empty
    s.version = v["version"].asInt(1);
    s.enabled = v["enabled"].asBool(true);

    const auto& c = v["camera"];
    s.camera.deviceId = c["device_id"].asString();
    s.camera.irDeviceId = c["ir_device_id"].asString();
    s.camera.width = clampi(c["width"].asInt(640), 160, 1920);
    s.camera.height = clampi(c["height"].asInt(360), 120, 1080);
    s.camera.mirrorPreview = c["mirror_preview"].asBool(true);
    s.camera.swapHandedness = c["swap_handedness"].asBool(false);
    s.camera.night = enumFrom(c["night"], kNight, NightMode::Auto);
    s.camera.nightThreshold = clampf(c["night_threshold"].asFloat(0.22f), 0.02f, 0.6f);
    s.camera.nightStrength = clampf(c["night_strength"].asFloat(0.7f), 0.f, 1.f);
    s.camera.nightLocalContrast = c["night_local_contrast"].asBool(true);
    s.camera.nightDenoise = c["night_denoise"].asBool(true);
    s.camera.irAmbientSuppression = c["ir_ambient_suppression"].asBool(true);
    s.camera.irTorch = c["ir_torch"].asBool(true);
    s.camera.hardwareLowLight = c["hardware_low_light"].asBool(true);

    const auto& d = v["distance"];
    s.distance.hfovDeg = clampf(d["hfov_deg"].asFloat(74.f), 30.f, 140.f);
    s.distance.calibration = clampf(d["calibration"].asFloat(1.f), 0.1f, 10.f);

    const auto& f = v["faces"];
    s.faces.gate = enumFrom(f["gate"], kGate, FaceGate::AnyFace);
    s.faces.matchThreshold = clampf(f["match_threshold"].asFloat(0.40f), 0.15f, 0.9f);
    s.faces.lookingTolerance = clampf(f["looking_tolerance"].asFloat(0.45f), 0.1f, 1.f);
    s.faces.recognizeEveryMs = clampi(f["recognize_every_ms"].asInt(2000), 250, 60000);

    const auto& p = v["performance"];
    s.perf.profile = enumFrom(p["profile"], kPower, PowerProfile::Balanced);
    s.perf.activeFps = clampi(p["active_fps"].asInt(30), 5, 60);
    s.perf.idleFps = clampi(p["idle_fps"].asInt(4), 1, 30);
    s.perf.sleepFps = clampi(p["sleep_fps"].asInt(1), 1, 10);
    s.perf.idleAfterMs = clampi(p["idle_after_ms"].asInt(1500), 200, 60000);
    s.perf.sleepAfterSec = clampi(p["sleep_after_sec"].asInt(90), 5, 3600);
    s.perf.ecoOnBattery = p["eco_on_battery"].asBool(true);
    s.perf.pauseInFullscreen = p["pause_in_fullscreen"].asBool(true);
    s.perf.pauseWhenLocked = p["pause_when_locked"].asBool(true);
    s.perf.device = enumFrom(p["device"], kDevice, ComputeDevice::Cpu);
    s.perf.threads = clampi(p["threads"].asInt(1), 1, 8);
    s.perf.int8Models = p["int8_models"].asBool(false);

    const auto& u = v["ui"];
    s.ui.theme = enumFrom(u["theme"], kTheme, Theme::Dark);
    s.ui.accent = clampi(u["accent"].asInt(0), 0, 16);
    s.ui.mica = u["mica"].asBool(true);
    s.ui.osd = u["osd"].asBool(true);
    s.ui.notifications = u["notifications"].asBool(true);
    s.ui.autostart = u["autostart"].asBool(true);
    s.ui.startMinimized = u["start_minimized"].asBool(true);
    s.ui.hotkeyToggle = static_cast<uint32_t>(u["hotkey_toggle"].asNumber(0x0347));
    s.ui.hotkeyOpen = static_cast<uint32_t>(u["hotkey_open"].asNumber(0x0356));

    for (const auto& o : v["rules"].asArray()) {
        GestureRule r;
        r.id = o["id"].asString(newId());
        r.name = o["name"].asString();
        r.enabled = o["enabled"].asBool(true);
        r.gesture = o["gesture"].asString("open_palm");
        r.hand = enumFrom(o["hand"], kHand, HandSide::Any);
        r.useDistance = o["use_distance"].asBool(true);
        r.minCm = clampf(o["min_cm"].asFloat(60.f), 10.f, 300.f);
        r.maxCm = clampf(o["max_cm"].asFloat(70.f), r.minCm, 300.f);
        r.holdMs = clampi(o["hold_ms"].asInt(320), 0, 5000);
        r.cooldownMs = clampi(o["cooldown_ms"].asInt(1700), 0, 30000);
        r.repeat = o["repeat"].asBool(false);
        r.repeatMs = clampi(o["repeat_ms"].asInt(250), 50, 5000);
        r.person = enumFrom(o["person"], kPerson, PersonFilter::Anyone);
        r.personId = o["person_id"].asString();
        r.requireLooking = o["require_looking"].asBool(false);
        r.action = actionFromJson(o["action"]);
        s.rules.push_back(std::move(r));
    }
    for (const auto& o : v["custom_gestures"].asArray()) {
        CustomGesture g;
        g.id = o["id"].asString(newId());
        g.name = o["name"].asString();
        g.rotationInvariant = o["rotation_invariant"].asBool(true);
        g.mirrorInvariant = o["mirror_invariant"].asBool(true);
        g.tolerance = clampf(o["tolerance"].asFloat(0.22f), 0.05f, 1.f);
        for (const auto& smp : o["samples"].asArray()) {
            const auto& a = smp.asArray();
            if (a.size() != 42) continue;
            std::array<float, 42> f42{};
            for (size_t i = 0; i < 42; ++i) f42[i] = a[i].asFloat();
            g.samples.push_back(f42);
        }
        s.customGestures.push_back(std::move(g));
    }
    return s;
}

}  // namespace vl
