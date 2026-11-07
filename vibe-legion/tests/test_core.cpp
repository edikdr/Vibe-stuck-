// Portable unit tests for the core (no ONNX Runtime, no Windows).
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "core/detection.hpp"
#include "core/face_db.hpp"
#include "core/gestures.hpp"
#include "core/image.hpp"
#include "core/json.hpp"
#include "core/night_vision.hpp"
#include "core/settings.hpp"

using namespace vl;

static int g_failed = 0, g_checks = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) { ++g_failed; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)
#define NEAR(a, b, eps) CHECK(std::fabs((a) - (b)) <= (eps))

// ---------------------------------------------------------------- synthetic hand

// Right hand, palm towards a mirrored (selfie) camera, fingers up. Metres.
static HandLandmarks makeHand(bool thumb, bool idx, bool mid, bool ring, bool pinky, float scalePx = 1000.f,
                              Vec2 origin = {320, 300}) {
    HandLandmarks h;
    auto& w = h.world;
    w[WRIST] = {0, 0, 0};
    if (thumb) {
        w[THUMB_CMC] = {-0.03f, -0.03f, 0}; w[THUMB_MCP] = {-0.05f, -0.05f, 0};
        w[THUMB_IP] = {-0.065f, -0.07f, 0}; w[THUMB_TIP] = {-0.08f, -0.085f, 0};
    } else {
        w[THUMB_CMC] = {-0.03f, -0.03f, 0}; w[THUMB_MCP] = {-0.035f, -0.055f, -0.01f};
        w[THUMB_IP] = {-0.02f, -0.07f, -0.025f}; w[THUMB_TIP] = {-0.005f, -0.075f, -0.03f};
    }
    const bool ext[4] = {idx, mid, ring, pinky};
    const float xs[4] = {-0.03f, -0.01f, 0.01f, 0.03f};
    for (int f = 0; f < 4; ++f) {
        const int b = INDEX_MCP + 4 * f;
        const float x = xs[f];
        w[b] = {x, -0.09f, 0};
        if (ext[f]) {
            w[b + 1] = {x, -0.135f, 0}; w[b + 2] = {x, -0.16f, 0}; w[b + 3] = {x, -0.18f, 0};
        } else {
            w[b + 1] = {x, -0.12f, -0.02f}; w[b + 2] = {x, -0.10f, -0.035f}; w[b + 3] = {x, -0.085f, -0.025f};
        }
    }
    for (int i = 0; i < kHandLandmarks; ++i)
        h.image[i] = {origin.x + w[i].x * scalePx, origin.y + w[i].y * scalePx, w[i].z * scalePx};
    h.presence = 1.f;
    h.rightness = 1.f;
    return h;
}

static HandObservation observe(const HandLandmarks& lm) {
    HandObservation o;
    o.lm = lm;
    o.isRight = lm.rightness > 0.5f;
    analyzeHand(o);
    return o;
}

// ---------------------------------------------------------------- tests

static void testJson() {
    auto r = json::parse(R"({"a": 1.5, "b": [1, 2, 3], "s": "привет é \"q\"", "o": {"t": true, "n": null}})");
    CHECK(r.ok);
    NEAR(r.value["a"].asNumber(), 1.5, 1e-9);
    CHECK(r.value["b"].asArray().size() == 3);
    CHECK(r.value["s"].asString() == "привет \xC3\xA9 \"q\"");
    CHECK(r.value["o"]["t"].asBool());
    CHECK(r.value["o"]["n"].isNull());
    CHECK(r.value["missing"]["deep"].isNull());
    auto r2 = json::parse(r.value.dump());
    CHECK(r2.ok && r2.value.dump() == r.value.dump());
    CHECK(!json::parse("{\"a\": }").ok);
    CHECK(!json::parse("[1, 2").ok);
    // copy-on-write: modifying a copy must not alias
    json::Value a = r.value, b = a;
    b.set("a", 7);
    NEAR(a["a"].asNumber(), 1.5, 1e-9);
}

static void testGeometry() {
    RotatedRect r{{100, 100}, 50, kPi / 2};
    Affine m = roiToImage(r, 224);
    Vec2 c = m.apply({112, 112});
    NEAR(c.x, 100, 1e-3); NEAR(c.y, 100, 1e-3);
    // ROI "up" (0,-1) must point to image right for rotation +90°.
    Vec2 up = m.apply({112, 0}) - c;
    CHECK(up.x > 20 && std::fabs(up.y) < 1e-3);
    Affine inv = m.inverse();
    Vec2 back = inv.apply(m.apply({10, 20}));
    NEAR(back.x, 10, 1e-3); NEAR(back.y, 20, 1e-3);

    // Similarity estimation recovers a known transform.
    std::array<Vec2, 5> src = {{{10, 10}, {40, 12}, {25, 30}, {14, 50}, {38, 48}}};
    Affine t; t.a = 0.8f * std::cos(0.3f); t.b = -0.8f * std::sin(0.3f); t.c = 5;
    t.d = 0.8f * std::sin(0.3f); t.e = 0.8f * std::cos(0.3f); t.f = -3;
    std::array<Vec2, 5> dst;
    for (int i = 0; i < 5; ++i) dst[i] = t.apply(src[i]);
    Affine e = estimateSimilarity(src, dst);
    NEAR(e.a, t.a, 1e-4); NEAR(e.b, t.b, 1e-4); NEAR(e.c, t.c, 1e-3); NEAR(e.f, t.f, 1e-3);
}

static void testPalmDecoder() {
    PalmDecoder dec;
    CHECK(dec.anchors().size() == 2016);
    NEAR(dec.anchors()[0].x, 0.0208333f, 1e-6);
    NEAR(dec.anchors()[1152].x, 0.0416667f, 1e-6);  // first 12x12 anchor
    NEAR(dec.anchors()[2015].y, 0.9583333f, 1e-6);
    std::vector<float> boxes(2016 * 18, 0.f), scores(2016, -20.f);
    // One confident detection on anchor 500 (24x24 grid, x=10,y=10 -> 250th cell).
    const int a = 500;
    scores[a] = 4.f;
    scores[a + 1] = 3.f;  // its duplicate twin anchor -> suppressed by NMS
    boxes[a * 18 + 2] = 40; boxes[a * 18 + 3] = 40;
    boxes[(a + 1) * 18 + 2] = 41; boxes[(a + 1) * 18 + 3] = 41;
    Affine t;  // identity: tensor == source
    auto d = dec.decode(boxes.data(), scores.data(), t, 0.5f, 0.3f, 5);
    CHECK(d.size() == 1);
    if (!d.empty()) {
        const Vec2 anc = dec.anchors()[a] * 192.f;
        NEAR(d[0].box.center().x, anc.x, 1e-3);
        NEAR(d[0].box.w, 40, 1e-3);
        NEAR(d[0].score, sigmoid(4.f), 1e-5);
    }
}

static void testHandRoi() {
    // Palm pointing right: wrist (100,100), middle base (140,100) -> rotation +90°.
    PalmDetection p;
    p.keypoints = {{{100, 100}, {135, 90}, {140, 100}, {135, 110}, {130, 118}, {105, 85}, {115, 80}}};
    RotatedRect r = handRoiFromPalm(p);
    NEAR(r.rotation, kPi / 2, 1e-4);
    CHECK(r.center.x > 120);  // shifted toward the fingers
    CHECK(r.size > 80);

    HandLandmarks h = makeHand(true, true, true, true, true);
    RotatedRect t = handRoiFromLandmarks(h);
    NEAR(t.rotation, 0.f, 0.15f);  // fingers up (synthetic middle finger is slightly left)
    CHECK(t.size > 150 && t.size < 600);
}

static void testYuNet() {
    // 32x32 pad -> stride 8: 4x4 cells; put one face at cell (r=1,c=2).
    const int padW = 32, padH = 32;
    std::vector<float> cls8(16, 0), obj8(16, 0), bbox8(64, 0), kps8(160, 0);
    std::vector<float> cls16(4, 0), obj16(4, 0), bbox16(16, 0), kps16(40, 0);
    std::vector<float> cls32(1, 0), obj32(1, 0), bbox32(4, 0), kps32(10, 0);
    const int idx = 1 * 4 + 2;
    cls8[idx] = 0.9f; obj8[idx] = 0.9f;
    bbox8[idx * 4 + 0] = 0.5f; bbox8[idx * 4 + 1] = 0.5f;  // centre (2.5*8, 1.5*8) = (20,12)
    bbox8[idx * 4 + 2] = std::log(2.f); bbox8[idx * 4 + 3] = std::log(3.f);  // w 16, h 24
    std::array<YuNetLevel, 3> lv = {{{cls8.data(), obj8.data(), bbox8.data(), kps8.data(), 8},
                                      {cls16.data(), obj16.data(), bbox16.data(), kps16.data(), 16},
                                      {cls32.data(), obj32.data(), bbox32.data(), kps32.data(), 32}}};
    Affine t; t.a = 2; t.e = 2;  // tensor -> source x2
    auto f = decodeYuNet(lv, padW, padH, t, 0.6f, 0.3f, 5);
    CHECK(f.size() == 1);
    if (!f.empty()) {
        NEAR(f[0].box.center().x, 40, 1e-3);
        NEAR(f[0].box.center().y, 24, 1e-3);
        NEAR(f[0].box.w, 32, 1e-3);
        NEAR(f[0].score, 0.9f, 1e-5);
        NEAR(f[0].landmarks[0].x, 2 * 2 * 8, 1e-3);  // kps offset 0 at cell c=2
    }
    // Head pose: symmetric frontal face -> ~0 yaw.
    FaceDetection fd;
    fd.landmarks = {{{40, 50}, {60, 50}, {50, 60}, {42, 72}, {58, 72}}};
    HeadPose hp = estimateHeadPose(fd);
    NEAR(hp.yaw, 0, 1e-3);
    CHECK(std::fabs(hp.pitch) < 0.3f);
    fd.landmarks[2].x = 60;  // nose under the left eye -> turned
    CHECK(std::fabs(estimateHeadPose(fd).yaw) > 0.8f);
    // SFace alignment maps the template onto itself.
    FaceDetection tpl;
    tpl.landmarks = {{{38.2946f, 51.6963f}, {73.5318f, 51.5014f}, {56.0252f, 71.7366f}, {41.5493f, 92.3655f}, {70.7299f, 92.2041f}}};
    Affine al = sfaceAlignment(tpl);
    NEAR(al.a, 1, 1e-3); NEAR(al.c, 0, 1e-2);
}

static void testGesturesStatic() {
    CHECK(observe(makeHand(true, true, true, true, true)).pose == "open_palm");
    CHECK(observe(makeHand(false, false, false, false, false)).pose == "fist");
    CHECK(observe(makeHand(false, true, false, false, false)).pose == "point");
    CHECK(observe(makeHand(false, true, true, false, false)).pose == "victory");
    CHECK(observe(makeHand(false, true, true, true, false)).pose == "three");
    CHECK(observe(makeHand(true, false, false, false, true)).pose == "call");
    CHECK(observe(makeHand(false, true, false, false, true)).pose == "rock");
    auto o = observe(makeHand(true, true, true, true, true));
    CHECK(o.palmFacing);
    CHECK(o.fingers.count() == 5);
    // Back of the hand: flip x -> open palm no longer reported.
    HandLandmarks back = makeHand(true, true, true, true, true);
    for (auto& p : back.image) p.x = 640 - p.x;
    CHECK(observe(back).pose != "open_palm");
}

static void testTemplates() {
    HandLandmarks a = makeHand(false, true, true, false, false);
    HandLandmarks b = makeHand(false, true, true, false, false, 500.f, {100, 100});  // smaller, shifted
    auto ta = handTemplate(a, true, true, true), tb = handTemplate(b, true, true, true);
    CHECK(templateDistance(ta, tb) < 1e-3f);
    HandLandmarks c = makeHand(true, true, true, true, true);
    CHECK(templateDistance(ta, handTemplate(c, true, true, true)) > 0.15f);

    CustomGesture g;
    g.id = "abc";
    g.samples.push_back(ta);
    HandObservation o = observe(b);
    matchCustom(o, {g});
    CHECK(o.customId == "abc");
    HandObservation o2 = observe(c);
    matchCustom(o2, {g});
    CHECK(o2.customId.empty());

    // A recording: 200 frames of the V pose (slight jitter) + a few frames of an
    // open palm while the hand was entering. Outliers and duplicates go away.
    std::vector<std::array<float, 42>> raw;
    for (int i = 0; i < 200; ++i) {
        auto t = ta;
        t[16] += 0.002f * static_cast<float>(i % 20);  // index tip wiggles
        raw.push_back(t);
    }
    const auto tc = handTemplate(c, true, true, true);
    for (int i = 0; i < 6; ++i) raw.push_back(tc);
    auto kept = condenseSamples(raw, 64);
    CHECK(!kept.empty() && kept.size() <= 64);
    bool hasPalm = false;
    for (const auto& k : kept) hasPalm = hasPalm || templateDistance(k, tc) < 1e-3f;
    CHECK(!hasPalm);
    CHECK(condenseSamples({}, 64).empty());

    // One stray palm sample among many V samples must not make a palm match.
    CustomGesture noisy;
    noisy.id = "v";
    noisy.samples = {ta, ta, ta, ta, tc};
    HandObservation o3 = observe(c);
    matchCustom(o3, {noisy});
    CHECK(o3.customId.empty());
    HandObservation o4 = observe(b);
    matchCustom(o4, {noisy});
    CHECK(o4.customId == "v");
}

static void testEngine() {
    GestureRule r;
    r.id = "r1";
    r.gesture = "open_palm";
    r.holdMs = 300; r.cooldownMs = 1500;
    r.action.type = ActionType::MediaPlayPause;
    std::vector<GestureRule> rules = {r};
    GestureEngine e;
    FaceContext fc;
    fc.anyFace = true;
    auto palm = observe(makeHand(true, true, true, true, true));
    std::vector<HandObservation> hands = {palm};

    int fired = 0;
    for (int t = 0; t <= 290; t += 33) fired += static_cast<int>(e.update(hands, "", fc, rules, FaceGate::AnyFace, 1000 + t).size());
    CHECK(fired == 0);                       // not held long enough
    fired += static_cast<int>(e.update(hands, "", fc, rules, FaceGate::AnyFace, 1000 + 310).size());
    CHECK(fired == 1);
    for (int t = 340; t < 3000; t += 33) fired += static_cast<int>(e.update(hands, "", fc, rules, FaceGate::AnyFace, 1000 + t).size());
    CHECK(fired == 1);                       // held: no repeat
    // release, re-show inside cooldown -> no fire; after cooldown -> fire
    e.update({}, "", fc, rules, FaceGate::AnyFace, 4300);
    for (int t = 4300; t < 6000; t += 33) fired += static_cast<int>(e.update(hands, "", fc, rules, FaceGate::AnyFace, t).size());
    CHECK(fired == 2);

    // Face gate: no face -> no fire.
    GestureEngine e3;
    FaceContext none;
    int f3 = 0;
    for (int t = 0; t < 2000; t += 33) f3 += static_cast<int>(e3.update(hands, "", none, rules, FaceGate::AnyFace, t).size());
    CHECK(f3 == 0);

    // Brief dropout (grace period) does not reset the hold.
    GestureEngine e4;
    int f4 = 0;
    f4 += static_cast<int>(e4.update(hands, "", fc, rules, FaceGate::Off, 0).size());
    f4 += static_cast<int>(e4.update(hands, "", fc, rules, FaceGate::Off, 150).size());
    f4 += static_cast<int>(e4.update({}, "", fc, rules, FaceGate::Off, 200).size());
    f4 += static_cast<int>(e4.update(hands, "", fc, rules, FaceGate::Off, 320).size());
    CHECK(f4 == 1);
}

static void testSettings() {
    Settings s = Settings::defaults();
    // Palm example + swipe left/right -> previous/next window, all disabled.
    CHECK(s.rules.size() == 3 && !s.rules[0].enabled && !s.rules[1].enabled && !s.rules[2].enabled);
    CHECK(s.rules[1].gesture == "swipe_left" && s.rules[1].action.type == ActionType::PrevWindow);
    CHECK(s.rules[2].gesture == "swipe_right" && s.rules[2].action.type == ActionType::NextWindow);
    s.rules[0].action.type = ActionType::KeyCombo;
    s.rules[0].action.keys = {0x11, 0x10, 'M'};
    CustomGesture g;
    g.id = "x"; g.name = "Мой жест";
    g.samples.push_back({});
    s.customGestures.push_back(g);
    s.perf.device = ComputeDevice::Npu;
    s.camera.night = NightMode::Infrared;
    auto text = s.toJson().dump();
    auto p = json::parse(text);
    CHECK(p.ok);
    Settings t = Settings::fromJson(p.value);
    CHECK(t.rules.size() == 3);  // v2 file: no migration duplicates
    CHECK(t.rules[0].action.type == ActionType::KeyCombo);
    CHECK(t.rules[0].action.keys.size() == 3 && t.rules[0].action.keys[2] == 'M');
    CHECK(t.rules[0].name == s.rules[0].name);
    CHECK(t.customGestures.size() == 1 && t.customGestures[0].samples.size() == 1);
    CHECK(t.perf.device == ComputeDevice::Npu);
    CHECK(t.camera.night == NightMode::Infrared);
    CHECK(t.ui.hotkeyToggle == 0x0347);
    // Garbage values are clamped.
    auto bad = json::parse(R"({"performance": {"active_fps": 9999}, "rules": [{"gesture": "fist", "use_distance": true, "min_cm": 90}]})");
    Settings u = Settings::fromJson(bad.value);
    CHECK(u.perf.activeFps == 60);
    // v1 file: distance keys ignored, swipe rules offered once.
    CHECK(u.rules.size() == 3 && u.rules[0].gesture == "fist" && u.rules[2].gesture == "swipe_right");
    CHECK(Settings::fromJson(json::parse(u.toJson().dump()).value).rules.size() == 3);
    CHECK(actionFromId("next_window") == ActionType::NextWindow);
    CHECK(std::string(actionId(ActionType::ScreenshotFull)) == "screenshot_full");
    CHECK(actionFromId("media_next") == ActionType::MediaNext);
}

static void testFaceDb() {
    FaceDatabase db;
    Person p;
    p.id = "p1"; p.name = "Анна";
    Embedding e{};
    e[0] = 1; e[5] = 0.5f;
    l2normalize(e);
    p.rgb.push_back(e);
    db.people.push_back(p);
    auto blob = db.serialize();
    FaceDatabase db2;
    CHECK(db2.deserialize(blob));
    CHECK(db2.people.size() == 1 && db2.people[0].name == "Анна");
    auto m = db2.match(e, false, 0.4f);
    CHECK(m.person == 0 && m.similarity > 0.99f);
    Embedding other{};
    other[7] = 1;
    CHECK(db2.match(other, false, 0.4f).person == -1);
    blob[0] ^= 0xFF;
    CHECK(!db2.deserialize(blob));
    std::vector<uint8_t> trunc(blob.begin(), blob.begin() + 20);
    CHECK(!db2.deserialize(trunc));
}

static void testImage() {
    // NV12 4x2 frame, mid grey with neutral chroma -> RGB ~ (128,128,128)
    std::vector<uint8_t> nv12(4 * 2 + 4 * 1, 128);
    for (int i = 0; i < 8; ++i) nv12[i] = 126;
    Image rgb;
    convertFrame(nv12.data(), 4, 2, 4, PixelFormat::NV12, 1, false, rgb);
    CHECK(rgb.width == 4 && rgb.height == 2);
    NEAR(rgb.data[0], 128, 3);
    // Letterbox maps tensor centre to image centre.
    Image img(200, 100, 3);
    TensorSpec spec{192, 192, TensorLayout::NHWC, false, 1.f / 255.f, 0.f};
    std::vector<float> buf(192 * 192 * 3);
    Affine t = letterboxToTensor(img, spec, buf.data());
    Vec2 c = t.apply({96, 96});
    NEAR(c.x, 100, 1e-3); NEAR(c.y, 50, 1e-3);
}

static void testNight() {
    Image dark(160, 90, 3);
    for (size_t i = 0; i < dark.data.size(); ++i) dark.data[i] = static_cast<uint8_t>(10 + (i % 7));
    NightVision nv;
    NightStats st;
    nv.process(dark, NightParams{}, &st);
    CHECK(st.meanOut > st.meanIn * 2.f);
    CHECK(st.gamma < 1.f);
    // A bright frame is left mostly alone.
    Image bright(160, 90, 3);
    std::fill(bright.data.begin(), bright.data.end(), uint8_t{150});
    NightVision nv2;
    nv2.process(bright, NightParams{}, &st);
    NEAR(st.meanOut, st.meanIn, 0.12f);

    // IR alternating detector.
    IrFrameSelector sel;
    int accepted = 0;
    for (int i = 0; i < 40; ++i) {
        Image f(32, 32, 3);
        std::fill(f.data.begin(), f.data.end(), static_cast<uint8_t>(i % 2 ? 160 : 20));
        accepted += sel.accept(f, false);
    }
    CHECK(sel.alternating());
    CHECK(accepted < 30);
}

int main() {
    testJson();
    testGeometry();
    testPalmDecoder();
    testHandRoi();
    testYuNet();
    testGesturesStatic();
    testTemplates();
    testEngine();
    testSettings();
    testFaceDb();
    testImage();
    testNight();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
