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
    "task_view", "screenshot", "switch_window", "pause_recognition",
    "next_window", "prev_window", "minimize_window", "maximize_window", "close_window",
    "next_desktop", "prev_desktop", "start_menu", "search", "file_explorer",
    "browser_back", "browser_forward", "screenshot_full", "zoom_in", "zoom_out", "call_mute",
    "macro", "run_script", "voice_listen"};
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
const char* const kKind[] = {"one_hand", "two_hands", "motion", "stroke"};
const char* const kAxis[] = {"x", "y", "depth"};

double round4(float f) { return static_cast<double>(std::round(f * 10000.f) / 10000.f); }

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
    if (!a.label.empty()) o.set("label", a.label);
    if (!a.script.empty()) o.set("script", a.script);
    return o;
}

Action actionFromJson(const json::Value& v) {
    Action a;
    a.type = actionFromId(v["type"].asString());
    for (const auto& k : v["keys"].asArray()) a.keys.push_back(static_cast<uint16_t>(clampi(k.asInt(), 0, 255)));
    a.path = v["path"].asString();
    a.args = v["args"].asString();
    a.label = v["label"].asString();
    a.script = v["script"].asString();
    return a;
}

// A few everyday commands so the voice page is not empty; the user edits or removes them.
void addVoiceExamples(std::vector<VoiceCommand>& cmds) {
    auto add = [&](const char* phrase, ActionType t) {
        VoiceCommand c;
        c.id = newId();
        c.phrase = phrase;
        c.action.type = t;
        cmds.push_back(c);
    };
    add("пауза", ActionType::MediaPlayPause);
    add("следующий трек", ActionType::MediaNext);
    add("громче", ActionType::VolumeUp);
    add("тише", ActionType::VolumeDown);
    add("выключи микрофон", ActionType::MicMute);
    add("сверни окно", ActionType::MinimizeWindow);
}

// Swipe left/right -> previous/next window. Shipped disabled like the palm example.
void addSwipeRules(std::vector<GestureRule>& rules) {
    auto add = [&](const char* gesture, const char* name, ActionType t) {
        for (const auto& r : rules)
            if (r.gesture == gesture) return;  // the user already has a rule for it
        GestureRule r;
        r.id = newId();
        r.name = name;
        r.enabled = false;
        r.gesture = gesture;
        r.holdMs = 0;  // swipes are instantaneous
        r.cooldownMs = 700;
        r.action.type = t;
        rules.push_back(r);
    };
    add("swipe_left", "Взмах влево — предыдущее окно", ActionType::PrevWindow);
    add("swipe_right", "Взмах вправо — следующее окно", ActionType::NextWindow);
}

// Both hands apart / together -> zoom in / out (browsers, photos, documents). Disabled.
void addTwoHandRules(std::vector<GestureRule>& rules) {
    auto add = [&](const char* gesture, const char* name, ActionType t) {
        for (const auto& r : rules)
            if (r.gesture == gesture) return;
        GestureRule r;
        r.id = newId();
        r.name = name;
        r.enabled = false;
        r.gesture = gesture;
        r.holdMs = 0;
        r.cooldownMs = 600;
        r.action.type = t;
        rules.push_back(r);
    };
    add("two_spread", "Развести руки — увеличить", ActionType::ZoomIn);
    add("two_pinch", "Свести руки — уменьшить", ActionType::ZoomOut);
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
    // Examples, shipped *disabled*: gestures are meant to be set up in the app.
    GestureRule r;
    r.id = newId();
    r.name = "Ладонь — пауза / play";
    r.enabled = false;
    r.gesture = "open_palm";
    r.holdMs = 500;
    r.cooldownMs = 1700;
    r.action.type = ActionType::MediaPlayPause;
    s.rules.push_back(r);
    addSwipeRules(s.rules);
    addTwoHandRules(s.rules);
    addVoiceExamples(s.voice.commands);
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

    json::Value fc;
    fc.set("match_threshold", faces.matchThreshold);
    fc.set("looking_tolerance", faces.lookingTolerance);
    fc.set("recognize_every_ms", faces.recognizeEveryMs);
    fc.set("embed_model", faces.embedModel);
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

    json::Value vo;
    vo.set("owner_only", voice.ownerOnly);
    vo.set("match_threshold", voice.matchThreshold);
    vo.set("listen_ms", voice.listenMs);
    vo.set("unmute_to_listen", voice.unmuteToListen);
    json::Array vc;
    for (const auto& c : voice.commands) {
        json::Value o;
        o.set("id", c.id);
        o.set("phrase", c.phrase);
        o.set("enabled", c.enabled);
        o.set("action", actionToJson(c.action));
        vc.push_back(std::move(o));
    }
    vo.set("commands", std::move(vc));
    root.set("voice", std::move(vo));

    json::Array rs;
    for (const auto& r : rules) {
        json::Value o;
        o.set("id", r.id);
        o.set("name", r.name);
        o.set("enabled", r.enabled);
        o.set("gesture", r.gesture);
        o.set("hand", enumName(r.hand, kHand));
        o.set("hold_ms", r.holdMs);
        o.set("cooldown_ms", r.cooldownMs);
        o.set("repeat", r.repeat);
        o.set("repeat_ms", r.repeatMs);
        o.set("person", enumName(r.person, kPerson));
        o.set("person_id", r.personId);
        o.set("require_face", r.requireFace);
        o.set("require_looking", r.requireLooking);
        if (!r.apps.empty()) o.set("apps", r.apps);
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
        o.set("kind", enumName(g.kind, kKind));
        o.set("threshold", g.threshold);
        o.set("strictness", g.strictness);
        o.set("dynamic", g.dynamic);
        o.set("record_ms", g.recordMs);
        o.set("sample_budget", g.sampleBudget);
        if (g.kind == CustomKind::Motion || g.kind == CustomKind::Stroke) {
            o.set("motion_dir", g.motionDir);
            o.set("axis", enumName(g.axis, kAxis));
            o.set("motion_amp", g.motionAmp);
            o.set("motion_period_ms", g.motionPeriodMs);
        }
        json::Array rows;
        for (const auto& row : g.rows) {
            json::Array a;
            for (float f : row) a.push_back(round4(f));
            rows.push_back(std::move(a));
        }
        o.set("rows", std::move(rows));
        if (!g.samples.empty()) {  // legacy 2D outline, kept until re-recorded
            o.set("tolerance", g.tolerance);
            json::Array samples;
            for (const auto& smp : g.samples) {
                json::Array a;
                for (float f : smp) a.push_back(round4(f));
                samples.push_back(std::move(a));
            }
            o.set("samples", std::move(samples));
        }
        cg.push_back(std::move(o));
    }
    root.set("custom_gestures", std::move(cg));
    json::Array dis;
    for (const auto& id : disabledGestures) dis.push_back(id);
    root.set("disabled_gestures", std::move(dis));
    return root;
}

Settings Settings::fromJson(const json::Value& v) {
    Settings s;  // empty rules: a file with "rules": [] stays empty
    const int fileVersion = v["version"].asInt(1);
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
    // v1 "distance" section and per-rule "use_distance/min_cm/max_cm" are ignored.

    const auto& f = v["faces"];
    s.faces.matchThreshold = clampf(f["match_threshold"].asFloat(0.40f), 0.15f, 0.9f);
    s.faces.lookingTolerance = clampf(f["looking_tolerance"].asFloat(0.45f), 0.1f, 1.f);
    s.faces.recognizeEveryMs = clampi(f["recognize_every_ms"].asInt(1000), 250, 60000);
    s.faces.embedModel = f["embed_model"].asString(fileVersion < 4 ? "" : "fp32");
    if (s.faces.embedModel != "int8" && s.faces.embedModel != "fp32" && !s.faces.embedModel.empty()) s.faces.embedModel = "fp32";

    const auto& p = v["performance"];
    s.perf.profile = enumFrom(p["profile"], kPower, PowerProfile::Balanced);
    s.perf.activeFps = clampi(p["active_fps"].asInt(30), 5, 60);
    s.perf.idleFps = clampi(p["idle_fps"].asInt(8), 1, 30);
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

    const auto& vo = v["voice"];
    s.voice.ownerOnly = vo["owner_only"].asBool(true);
    s.voice.matchThreshold = clampf(vo["match_threshold"].asFloat(0.45f), 0.1f, 0.9f);
    s.voice.listenMs = clampi(vo["listen_ms"].asInt(5000), 2000, 10000);
    s.voice.unmuteToListen = vo["unmute_to_listen"].asBool(false);
    for (const auto& o : vo["commands"].asArray()) {
        VoiceCommand vc;
        vc.id = o["id"].asString(newId());
        vc.phrase = o["phrase"].asString();
        vc.enabled = o["enabled"].asBool(true);
        vc.action = actionFromJson(o["action"]);
        s.voice.commands.push_back(std::move(vc));
    }

    for (const auto& o : v["rules"].asArray()) {
        GestureRule r;
        r.id = o["id"].asString(newId());
        r.name = o["name"].asString();
        r.enabled = o["enabled"].asBool(true);
        r.gesture = o["gesture"].asString("open_palm");
        r.hand = enumFrom(o["hand"], kHand, HandSide::Any);
        r.holdMs = clampi(o["hold_ms"].asInt(320), 0, 5000);
        r.cooldownMs = clampi(o["cooldown_ms"].asInt(1700), 0, 30000);
        r.repeat = o["repeat"].asBool(false);
        r.repeatMs = clampi(o["repeat_ms"].asInt(250), 50, 5000);
        r.person = enumFrom(o["person"], kPerson, PersonFilter::Anyone);
        r.personId = o["person_id"].asString();
        r.requireFace = o["require_face"].asBool(false);
        r.requireLooking = o["require_looking"].asBool(false);
        r.apps = o["apps"].asString();
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
        g.kind = enumFrom(o["kind"], kKind, CustomKind::OneHand);
        g.threshold = clampf(o["threshold"].asFloat(0.f), 0.f, 2.f);
        g.strictness = clampf(o["strictness"].asFloat(1.f), 0.4f, 2.f);
        g.axis = enumFrom(o["axis"], kAxis, MotionAxis::X);
        g.motionAmp = clampf(o["motion_amp"].asFloat(0.f), 0.f, 50.f);
        g.motionPeriodMs = clampf(o["motion_period_ms"].asFloat(0.f), 0.f, 20000.f);
        g.motionDir = o["motion_dir"].asInt(1) < 0 ? -1 : 1;
        g.dynamic = o["dynamic"].asBool(g.kind == CustomKind::Motion || g.kind == CustomKind::Stroke);
        g.recordMs = clampi(o["record_ms"].asInt(9000), CustomGesture::kMinRecordMs, CustomGesture::kMaxRecordMs);
        g.sampleBudget = clampi(o["sample_budget"].asInt(48), 16, 128);
        for (const auto& row : o["rows"].asArray()) {
            std::vector<float> r2;
            for (const auto& x : row.asArray()) r2.push_back(x.asFloat());
            if (!r2.empty()) g.rows.push_back(std::move(r2));
        }
        s.customGestures.push_back(std::move(g));
    }
    for (const auto& d : v["disabled_gestures"].asArray())
        if (!d.asString().empty()) s.disabledGestures.push_back(d.asString());
    if (fileVersion < 2) addSwipeRules(s.rules);  // migrations: offer new example rules once
    if (fileVersion < 3) {
        addTwoHandRules(s.rules);
        if (s.perf.idleFps == 4) s.perf.idleFps = 8;  // old defaults -> new defaults
        if (s.faces.recognizeEveryMs == 2000) s.faces.recognizeEveryMs = 1000;
    }
    if (fileVersion < 4) {
        // The always-on "a face must be in the frame" gate becomes an opt-in per rule;
        // "known faces only" carries over to every rule that had no person filter.
        if (enumFrom(v["faces"]["gate"], kGate, FaceGate::AnyFace) == FaceGate::KnownOnly)
            for (auto& r : s.rules)
                if (r.person == PersonFilter::Anyone) r.person = PersonFilter::AnyKnown;
    }
    if (fileVersion < 5) addVoiceExamples(s.voice.commands);
    s.version = kVersion;
    return s;
}

bool Settings::gestureEnabled(const std::string& id) const {
    return std::find(disabledGestures.begin(), disabledGestures.end(), id) == disabledGestures.end();
}

}  // namespace vl
