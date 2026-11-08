#include "vision/pipeline.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace vl {

namespace {

RectF handBox(const HandLandmarks& lm) {
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (const auto& p : lm.image) {
        x0 = std::min(x0, p.x); y0 = std::min(y0, p.y);
        x1 = std::max(x1, p.x); y1 = std::max(y1, p.y);
    }
    return {x0, y0, x1 - x0, y1 - y0};
}

int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace

const char* modeTextRu(PipelineMode m) {
    switch (m) {
        case PipelineMode::Starting: return "Запуск";
        case PipelineMode::Paused: return "Пауза";
        case PipelineMode::Sleep: return "Сон";
        case PipelineMode::Idle: return "Ожидание";
        case PipelineMode::Active: return "Активно";
        case PipelineMode::NoCamera: return "Нет камеры";
        case PipelineMode::Covered: return "Камера закрыта";
        case PipelineMode::ModelError: return "Нет моделей";
    }
    return "";
}

VisionPipeline::VisionPipeline(std::filesystem::path modelsDir, FrameMailbox& mailbox, CameraControl* camera,
                               PipelineCallbacks cb)
    : modelsDir_(std::move(modelsDir)), mailbox_(mailbox), camera_(camera), cb_(std::move(cb)) {
    settings_ = std::make_shared<Settings>(Settings::defaults());
    faceDb_ = std::make_shared<FaceDatabase>();
    for (auto& tr : tracks_) resetTrack(tr);
}

void VisionPipeline::resetTrack(Track& t) {
    t.active = false;
    for (auto& f : t.filter) f = OneEuro(2.0f, 0.03f, 1.0f);
    t.motion.reset();
    t.poses.clear();
    t.path.clear();
    t.customs.clear();
    t.vel = {};
}

VisionPipeline::~VisionPipeline() { stop(); }

void VisionPipeline::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread([this] { run(); });
}

void VisionPipeline::stop() {
    if (!running_) return;
    running_ = false;
    mailbox_.wake();
    if (thread_.joinable()) thread_.join();
}

void VisionPipeline::setSettings(std::shared_ptr<const Settings> s) {
    std::lock_guard<std::mutex> lk(cfgMutex_);
    settings_ = std::move(s);
}

void VisionPipeline::setFaceDb(std::shared_ptr<const FaceDatabase> db) {
    std::lock_guard<std::mutex> lk(cfgMutex_);
    faceDb_ = std::move(db);
}

void VisionPipeline::setPaused(bool paused, std::string reason) {
    {
        std::lock_guard<std::mutex> lk(cfgMutex_);
        pauseReason_ = std::move(reason);
    }
    paused_ = paused;
    mailbox_.wake();
}

void VisionPipeline::setEco(bool eco) { eco_ = eco; }

void VisionPipeline::startEnrollment(const std::string& personId, int durationMs) {
    std::lock_guard<std::mutex> lk(taskMutex_);
    enroll_ = std::make_unique<Enroll>();
    enroll_->person = personId;
    enroll_->durationMs = std::clamp(durationMs, 2000, 30000);
}

void VisionPipeline::startRecording(const std::string& id, bool rot, bool mir, int ms, bool dynamic, int sampleBudget) {
    std::lock_guard<std::mutex> lk(taskMutex_);
    record_ = std::make_unique<Record>();
    record_->id = id;
    record_->rot = rot;
    record_->mir = mir;
    record_->ms = std::clamp(ms, CustomGesture::kMinRecordMs, CustomGesture::kMaxRecordMs);
    record_->dynamic = dynamic;
    record_->budget = std::clamp(sampleBudget, 16, 128);
}

void VisionPipeline::cancelTasks() {
    std::lock_guard<std::mutex> lk(taskMutex_);
    enroll_.reset();
    record_.reset();
}

PipelineSnapshot VisionPipeline::snapshot() const {
    std::lock_guard<std::mutex> lk(snapMutex_);
    return snap_;
}

int VisionPipeline::targetFps(const Settings& s) const {
    const auto& p = s.perf;
    PowerProfile prof = eco_ && p.ecoOnBattery ? PowerProfile::Eco : p.profile;
    int active = p.activeFps, idle = p.idleFps, sleep = p.sleepFps;
    if (prof == PowerProfile::Eco) {
        active = std::min(active, 15);
        idle = std::min(idle, 2);
        sleep = 1;
    } else if (prof == PowerProfile::Responsive) {
        idle = std::max(idle, 8);
    }
    // Two tracked hands cost two landmark runs per frame: cap at 2/3 of the active rate
    // (30 -> 20 fps), which keeps the peak load within ~+30 % of the one-hand peak.
    if (twoTracked_) active = std::max(std::min(active, 12), active * 2 / 3);
    // Hold timers need decent temporal resolution only while a hand is up.
    const int64_t t = nowMs();
    if (t - lastHandMs_ < p.idleAfterMs) return active;
    if (t - std::max(lastHandMs_, lastFaceSeenMs_) > static_cast<int64_t>(p.sleepAfterSec) * 1000) return sleep;
    return idle;
}

void VisionPipeline::loadModels(const Settings& s) {
    ml::InferenceConfig cfg;
    cfg.device = s.perf.device;
    cfg.threads = s.perf.profile == PowerProfile::Eco ? 1 : s.perf.threads;
    const std::string sfx = s.perf.int8Models ? "_int8" : "";
    auto pick = [&](const std::string& base, const std::string& suffix) {
        auto p = modelsDir_ / (base + suffix + ".onnx");
        if (!std::filesystem::exists(p)) p = modelsDir_ / (base + ".onnx");
        return p;
    };
    std::string err;
    modelError_.clear();
    if (!palm_.load(pick("palm_detection_mediapipe_2023feb", sfx), cfg, &err)) modelError_ = err;
    else if (!hand_.load(pick("handpose_estimation_mediapipe_2023feb", sfx), cfg, &err)) modelError_ = err;
    // YuNet is tiny; always fp32. 224x128 covers a 16:9 frame (faces at desk distance are large).
    else if (!face_.load(modelsDir_ / "face_detection_yunet_2023mar.onnx", cfg, 224, 128, &err)) modelError_ = err;
    embedder_.unload();  // reloaded lazily with the new config
    loadedKey_ = std::to_string(static_cast<int>(cfg.device)) + "/" + std::to_string(cfg.threads) + "/" + sfx;
    std::lock_guard<std::mutex> lk(snapMutex_);
    snap_.device = palm_.ok() ? palm_.device() : "";
}

void VisionPipeline::updateNight(float light, bool ir, int64_t now, const Settings& s) {
    const auto& c = s.camera;
    if (!ir) lightEma_ = lightEma_ + 0.15f * (light - lightEma_);
    const bool dark = lightEma_ < c.nightThreshold;
    const bool bright = lightEma_ > c.nightThreshold + 0.05f;

    if (!ir) {
        if (dark) { if (darkSinceMs_ < 0) darkSinceMs_ = now; brightSinceMs_ = -1; }
        else if (bright) { if (brightSinceMs_ < 0) brightSinceMs_ = now; darkSinceMs_ = -1; }
    }

    switch (c.night) {
        case NightMode::Off: nightOn_ = false; break;
        case NightMode::Software: nightOn_ = true; break;
        case NightMode::Infrared:
        case NightMode::Auto:
            if (!ir) {
                if (dark && darkSinceMs_ >= 0 && now - darkSinceMs_ > 1500) nightOn_ = true;
                if (bright && brightSinceMs_ >= 0 && now - brightSinceMs_ > 1500) nightOn_ = false;
            }
            break;
    }
    if (c.night == NightMode::Infrared) nightOn_ = true;

    if (camera_ && camera_->irAvailable()) {
        bool wantIr = false;
        if (c.night == NightMode::Infrared) wantIr = true;
        else if (c.night == NightMode::Auto) {
            if (!ir) wantIr = dark && darkSinceMs_ >= 0 && now - darkSinceMs_ > 4000;
            else {
                // On IR we cannot see room light; probe the colour camera every 90 s.
                if (irSinceMs_ < 0) irSinceMs_ = now;
                wantIr = now - irSinceMs_ < 90000;
                if (!wantIr) { darkSinceMs_ = -1; lightEma_ = c.nightThreshold + 0.02f; }
            }
        }
        if (!ir) irSinceMs_ = -1;
        if (wantIr != camera_->usingIr()) camera_->requestIr(wantIr);
    }
    const bool hw = nightOn_ && !ir && c.hardwareLowLight;
    if (camera_ && hw != lowLightHw_) {
        camera_->setLowLightBoost(hw);
        lowLightHw_ = hw;
    }
}

void VisionPipeline::processForTest(const Image& rgb, int64_t now) {
    std::shared_ptr<const Settings> s;
    {
        std::lock_guard<std::mutex> lk(cfgMutex_);
        s = settings_;
    }
    const std::string key = std::to_string(static_cast<int>(s->perf.device)) + "/" +
                            std::to_string(s->perf.profile == PowerProfile::Eco ? 1 : s->perf.threads) + "/" +
                            (s->perf.int8Models ? "_int8" : "");
    if (key != loadedKey_) loadModels(*s);
    Image copy = rgb;
    processFrame(copy, false, now, *s);
}

void VisionPipeline::processFrame(Image& rgb, bool ir, int64_t now, const Settings& s) {
    const auto t0 = std::chrono::steady_clock::now();
    const float dt = lastFrameMs_ ? std::max(1, static_cast<int>(now - lastFrameMs_)) / 1000.f : 0.033f;
    lastFrameMs_ = now;

    std::shared_ptr<const FaceDatabase> db;
    {
        std::lock_guard<std::mutex> lk(cfgMutex_);
        db = faceDb_;
    }

    // ---- light / night vision
    const float light = meanLuma(rgb, 8);
    if (!ir && light < 0.012f) { if (coveredSinceMs_ < 0) coveredSinceMs_ = now; }
    else coveredSinceMs_ = -1;
    const bool covered = coveredSinceMs_ >= 0 && now - coveredSinceMs_ > 1500 && !nightOn_;
    updateNight(light, ir, now, s);
    if (ir) {
        if (!irSel_.accept(rgb, s.camera.irAmbientSuppression)) return;  // unlit frame of an alternating pair
        NightParams np{s.camera.nightStrength * 0.5f, 0.45f, s.camera.nightLocalContrast, s.camera.nightDenoise, 2.0f};
        night_.process(rgb, np);
    } else if (nightOn_) {
        NightParams np{s.camera.nightStrength, 0.42f, s.camera.nightLocalContrast, s.camera.nightDenoise, 2.5f};
        night_.process(rgb, np);
    } else {
        night_.reset();
    }

    double msPalm = 0, msHand = 0, msFace = 0, msEmbed = 0;

    // ---- faces
    const bool handActive = now - lastHandMs_ < s.perf.idleAfterMs;
    bool needRecognition = false;
    if (!db->people.empty()) {
        needRecognition = previewWanted_;
        for (const auto& r : s.rules)
            if (r.enabled && r.person != PersonFilter::Anyone) needRecognition = true;
    }
    std::unique_lock<std::mutex> tlk(taskMutex_);
    const bool enrolling = enroll_ != nullptr;
    tlk.unlock();
    // Faces are only looked for when a rule asks for them (or the window shows them):
    // no rule with a face condition = no face model running at all.
    const bool needFaces = needRecognition || enrolling || previewWanted_ ||
                           std::any_of(s.rules.begin(), s.rules.end(), [](const GestureRule& r) {
                               return r.enabled && (r.requireFace || r.requireLooking || r.person != PersonFilter::Anyone);
                           });
    if (needFaces && face_.ok() && now - lastFaceRunMs_ >= (handActive ? 300 : 0)) {
        lastFaceRunMs_ = now;
        auto dets = face_.detect(rgb, 0.7f, 4);
        msFace = face_.lastMs();
        std::vector<FaceInfo> next;
        std::vector<int64_t> nextRec;
        for (const auto& d : dets) {
            FaceInfo fi;
            fi.det = d;
            int64_t rec = -1000000;
            // Carry identity from the overlapping face of the previous run.
            for (size_t k = 0; k < faces_.size(); ++k) {
                if (iou(faces_[k].det.box, d.box) > 0.3f) {
                    fi.personId = faces_[k].personId;
                    fi.name = faces_[k].name;
                    fi.similarity = faces_[k].similarity;
                    rec = k < faceRecognizedMs_.size() ? faceRecognizedMs_[k] : rec;
                    break;
                }
            }
            const HeadPose hp = estimateHeadPose(d);
            fi.looking = std::fabs(hp.yaw) < s.faces.lookingTolerance && std::fabs(hp.pitch) < s.faces.lookingTolerance;
            next.push_back(fi);
            nextRec.push_back(rec);
        }
        faces_ = std::move(next);
        faceRecognizedMs_ = std::move(nextRec);
        if (!faces_.empty()) lastFaceSeenMs_ = now;

        if ((needRecognition || enrolling) && !faces_.empty()) {
            if (!embedder_.ok()) {
                ml::InferenceConfig cfg{s.perf.device, 1};
                std::string err;
                // fp32 first: on Zen 5 it runs in ~15 ms vs ~110 ms for the int8 build
                // (no fast int8 path for this graph in the CPU EP) and is more accurate.
                const bool int8 = s.faces.embedModel == "int8";  // must match how the faces were enrolled
                const char* first = int8 ? "face_recognition_sface_2021dec_int8.onnx" : "face_recognition_sface_2021dec.onnx";
                const char* second = int8 ? "face_recognition_sface_2021dec.onnx" : "face_recognition_sface_2021dec_int8.onnx";
                auto p = modelsDir_ / first;
                if (!std::filesystem::exists(p)) p = modelsDir_ / second;
                embedder_.load(p, cfg, &err);
            }
            if (embedder_.ok()) {
                embedderLastUse_ = now;
                // Largest face is the enrollment subject.
                size_t largest = 0;
                for (size_t k = 1; k < faces_.size(); ++k)
                    if (faces_[k].det.box.area() > faces_[largest].det.box.area()) largest = k;
                for (size_t k = 0; k < faces_.size() && k < 3; ++k) {
                    tlk.lock();
                    if (enroll_ && enroll_->start == 0) enroll_->start = now;  // the clock starts with a face
                    // One sample every 300 ms over the whole duration: different head angles,
                    // expressions and lighting instead of 14 near-identical frames.
                    const bool enrollThis = enroll_ && k == largest && now - enroll_->last >= 300;
                    tlk.unlock();
                    // Known faces are re-checked every recognizeEveryMs; unknown ones sooner,
                    // but not on every frame (that used to run SFace at the full frame rate
                    // whenever a face was not recognised).
                    const int64_t every = faces_[k].personId.empty() ? std::min(600, s.faces.recognizeEveryMs)
                                                                     : s.faces.recognizeEveryMs;
                    const bool stale = now - faceRecognizedMs_[k] >= every;
                    if (!(enrollThis || (needRecognition && stale))) continue;
                    Embedding e;
                    if (!embedder_.embed(rgb, faces_[k].det, e)) continue;
                    msEmbed += embedder_.lastMs();
                    faceRecognizedMs_[k] = now;
                    const FaceMatch m = db->match(e, ir, s.faces.matchThreshold);
                    faces_[k].similarity = m.similarity;
                    if (m.person >= 0) {
                        faces_[k].personId = db->people[m.person].id;
                        faces_[k].name = db->people[m.person].name;
                    } else {
                        faces_[k].personId.clear();
                        faces_[k].name.clear();
                    }
                    if (enrollThis) {
                        tlk.lock();
                        if (enroll_) {
                            bool dup = false;  // skip frames that add nothing new
                            for (const auto& g : enroll_->got)
                                if (cosine(e, g) > 0.985f) { dup = true; break; }
                            if (!dup) enroll_->got.push_back(e);
                            enroll_->ir = ir;
                            enroll_->last = now;
                            const bool timeUp = now - enroll_->start >= enroll_->durationMs && enroll_->got.size() >= 5;
                            if (timeUp || enroll_->got.size() >= FaceDatabase::kMaxSamplesPerModality) {
                                auto done = std::move(enroll_);
                                tlk.unlock();
                                if (cb_.onEnrolled) cb_.onEnrolled(done->person, std::move(done->got), done->ir);
                                tlk.lock();
                            }
                        }
                        tlk.unlock();
                    }
                }
            }
        }
    } else if (!needFaces) {
        faces_.clear();
        faceRecognizedMs_.clear();
    }
    if (embedder_.ok() && !needRecognition && !enrolling && now - embedderLastUse_ > 120000) embedder_.unload();
    if (embedder_.ok() && embedModelLoaded_ != s.faces.embedModel) embedder_.unload();  // switched build
    embedModelLoaded_ = s.faces.embedModel;

    // ---- hands (up to two)
    std::string dynamic;
    if (palm_.ok() && hand_.ok()) {
        for (auto& tr : tracks_) {
            if (!tr.active) continue;
            HandLandmarks raw;
            // Fast movements: look where the hand is going, not where it was. The ROI moves
            // by the last frame-to-frame displacement and grows with the speed, so a swipe
            // or a shake stays inside it instead of being lost and re-detected (which drops
            // the frames that carry the movement).
            RotatedRect roi = handRoiFromLandmarks(tr.lastLm);
            const float step = length(tr.vel);
            roi.center = roi.center + tr.vel;
            roi.size *= 1.f + std::min(0.6f, step / std::max(1.f, roi.size));
            bool ok = hand_.run(rgb, roi, raw);
            msHand += hand_.lastMs();
            if (!ok && step > 0.f) {  // prediction overshot (the hand stopped / turned back): retry in place
                ok = hand_.run(rgb, handRoiFromLandmarks(tr.lastLm), raw);
                msHand += hand_.lastMs();
            }
            if (ok) {
                const Vec2 prev = (tr.lastLm.image[WRIST].xy() + tr.lastLm.image[MIDDLE_MCP].xy()) * 0.5f;
                const Vec2 cur = (raw.image[WRIST].xy() + raw.image[MIDDLE_MCP].xy()) * 0.5f;
                tr.vel = (cur - prev) * 0.8f;  // a little under, overshooting is worse than lagging
                tr.lastLm = raw;
            } else {
                // Lost: a swipe that carried the hand out of the frame still counts.
                if (auto g = tr.motion.lost(now); !g.empty() && dynamic.empty()) dynamic = g;
                resetTrack(tr);
            }
        }
        const int active = static_cast<int>(tracks_[0].active) + static_cast<int>(tracks_[1].active);
        // The palm detector is the expensive part: every frame while no hand is
        // tracked, every 250 ms while one is (to pick up a second hand), never with two.
        if (active < 2 && (active == 0 || now - lastPalmScanMs_ >= 250)) {
            lastPalmScanMs_ = now;
            const auto palms = palm_.detect(rgb, 0.45f, 2);
            msPalm = palm_.lastMs();
            for (const auto& pd : palms) {
                const Vec2 pc{pd.box.x + pd.box.w * 0.5f, pd.box.y + pd.box.h * 0.5f};
                bool known = false;
                for (const auto& tr : tracks_)
                    if (tr.active && handBox(tr.lastLm).contains(pc)) known = true;
                if (known) continue;
                Track* slot = !tracks_[0].active ? &tracks_[0] : !tracks_[1].active ? &tracks_[1] : nullptr;
                if (!slot) break;
                HandLandmarks raw;
                const bool ok = hand_.run(rgb, handRoiFromPalm(pd), raw);
                msHand += hand_.lastMs();
                if (!ok) continue;
                resetTrack(*slot);
                slot->active = true;
                slot->lastLm = raw;
            }
        }
        // Two tracks may lock onto the same hand (after crossing): keep one.
        if (tracks_[0].active && tracks_[1].active && iou(handBox(tracks_[0].lastLm), handBox(tracks_[1].lastLm)) > 0.35f)
            resetTrack(tracks_[1]);
    }

    std::vector<HandObservation> hands;
    for (size_t ti = 0; ti < tracks_.size(); ++ti) {
        auto& tr = tracks_[ti];
        if (!tr.active) continue;
        HandObservation o;
        o.track = static_cast<int>(ti);
        const HandLandmarks& raw = tr.lastLm;
        o.lm = raw;
        for (int i = 0; i < kHandLandmarks; ++i) {
            o.lm.image[i].x = tr.filter[2 * i].filter(raw.image[i].x, dt);
            o.lm.image[i].y = tr.filter[2 * i + 1].filter(raw.image[i].y, dt);
        }
        o.isRight = (raw.rightness > 0.5f) != s.camera.swapHandedness;
        analyzeHand(o);
        // Majority of the last 3 frames: one misread frame no longer breaks a hold.
        tr.poses.push_back(o.pose);
        while (tr.poses.size() > 3) tr.poses.pop_front();
        o.pose.clear();
        for (const auto& cand : tr.poses)
            if (!cand.empty() && std::count(tr.poses.begin(), tr.poses.end(), cand) >= 2) { o.pose = cand; break; }
        if (!o.pose.empty() && !s.gestureEnabled(o.pose)) o.pose.clear();  // switched off by the user

        // Context: how sure the model is, whether the whole hand is visible, how fast it moves.
        o.presence = raw.presence;
        const float mx = rgb.width * 0.01f, my = rgb.height * 0.01f;
        for (const auto& p : raw.image)
            if (p.x < mx || p.y < my || p.x > rgb.width - mx || p.y > rgb.height - my) o.inFrame = false;
        const Vec2 c = palmCentre(o);
        tr.path.push_back({now, c});
        while (tr.path.size() > 2 && now - tr.path.front().first > 300) tr.path.pop_front();
        if (tr.path.size() >= 2 && now > tr.path.front().first) {
            const float dtS = static_cast<float>(now - tr.path.front().first) / 1000.f;
            o.speed = length(c - tr.path.front().second) / std::max(1.f, o.palmPx) / dtS;
        }
        o.steady = o.speed < 1.5f;  // palm lengths per second: talking hands move faster
        o.feat = handFeatures(raw, o.isRight, false);
        o.featMirror = o.isRight ? o.feat : handFeatures(raw, o.isRight, true);
        hands.push_back(std::move(o));
    }
    // Larger (closer) hand first: it is "the" hand for one-hand displays.
    if (hands.size() == 2 && hands[1].palmPx > hands[0].palmPx) std::swap(hands[0], hands[1]);
    // Two hands right next to each other are holding something or typing: their
    // finger shapes (a "V" around a phone) are not gestures.
    if (hands.size() == 2) {
        const float palm = std::max(1.f, 0.5f * (hands[0].palmPx + hands[1].palmPx));
        if (length(palmCentre(hands[0]) - palmCentre(hands[1])) < 2.0f * palm) hands[0].busy = hands[1].busy = true;
    }
    const bool found = !hands.empty();
    twoTracked_ = hands.size() >= 2;
    if (found) lastHandMs_ = now;

    // Custom gestures (one hand, two hands, with movement).
    const std::string customMove = custom_.update(hands, s.customGestures, now);
    // Like poses: a custom match must hold in 2 of the last 3 frames of that hand.
    for (auto& h : hands) {
        if (h.track < 0) continue;
        auto& hist = tracks_[static_cast<size_t>(h.track)].customs;
        hist.push_back(h.customId);
        while (hist.size() > 3) hist.pop_front();
        if (!h.customId.empty() && std::count(hist.begin(), hist.end(), h.customId) < 2) h.customId.clear();
    }

    // Swipes are held back briefly only if some rule needs to tell them from waving.
    bool oscillation = false;
    for (const auto& r : s.rules) {
        if (!r.enabled) continue;
        if (r.gesture == "wave") oscillation = true;
        if (r.gesture.rfind("custom:", 0) == 0)
            for (const auto& cg : s.customGestures)
                if ("custom:" + cg.id == r.gesture && cg.kind == CustomKind::Motion) oscillation = true;
    }
    for (auto& tr : tracks_) tr.motion.setOscillationAware(oscillation);

    // Movements. With two hands up, their joint movement wins (spreading the hands
    // would otherwise also read as a left swipe plus a right swipe).
    const HandObservation* h0 = hands.size() >= 1 ? &hands[0] : nullptr;
    const HandObservation* h1 = hands.size() >= 2 ? &hands[1] : nullptr;
    std::string twoPose = twoHands_.staticPose(h0, h1);
    if (!twoPose.empty() && !s.gestureEnabled(twoPose)) twoPose.clear();
    if (auto g = twoHands_.update(h0, h1, now); !g.empty()) {
        dynamic = g;
        for (auto& tr : tracks_) tr.motion.reset();
    } else if (hands.size() == 1) {
        for (auto& tr : tracks_)
            if (tr.active)
                if (auto m = tr.motion.update(hands[0], now); !m.empty() && dynamic.empty()) dynamic = m;
    } else if (hands.size() == 2) {
        for (auto& tr : tracks_) tr.motion.reset();
    }
    if (!dynamic.empty() && !s.gestureEnabled(dynamic)) dynamic.clear();
    if (!customMove.empty()) dynamic = customMove;  // a recorded movement beats a generic one
    if (!dynamic.empty()) { lastDynamic_ = dynamic; lastDynamicMs_ = now; }

    // ---- recording a custom gesture: every frame, all hands, for the analysis at the end
    tlk.lock();
    if (record_) {
        if (found && record_->start == 0) record_->start = now + CustomGesture::kPrerollMs;  // get ready first
        if (record_->start != 0 && now >= record_->start) {
            RecordedFrame fr;
            fr.t = now;
            for (const auto& h : hands) {
                if (!h.reliable()) continue;
                fr.hands.push_back({h.feat, h.featMirror, palmCentre(h), h.palmPx, h.speed, h.pose});
            }
            std::sort(fr.hands.begin(), fr.hands.end(),
                      [](const RecordedHand& a, const RecordedHand& b) { return a.centre.x < b.centre.x; });
            record_->frames.push_back(std::move(fr));
        }
    }
    const HandObservation obs = found ? hands[0] : HandObservation{};
    // Finishes on time even if the hand left the frame.
    if (record_ && record_->start != 0 && now - record_->start >= record_->ms) {
        auto done = std::move(record_);
        tlk.unlock();
        CustomGesture learned;
        learned.mirrorInvariant = done->mir;
        learned.rotationInvariant = done->rot;
        learned.dynamic = done->dynamic;
        learned.sampleBudget = done->budget;
        learned.recordMs = done->ms;
        std::string report;
        const bool ok = learnCustomGesture(done->frames, learned, &report);
        if (cb_.onRecorded) cb_.onRecorded(done->id, ok, std::move(learned), std::move(report));
    } else {
        tlk.unlock();
    }

    // ---- rules
    FaceContext fc;
    fc.anyFace = !faces_.empty();
    for (const auto& f : faces_) {
        fc.looking = fc.looking || f.looking;
        if (!f.personId.empty()) fc.people.push_back(f.personId);
    }
    auto events = engine_.update(hands, dynamic, twoPose, fc, s.rules, now);
    for (const auto& e : events)
        if (cb_.onGesture) cb_.onGesture(e);

    // ---- snapshot
    const double msFrame = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    frameTimes_.push_back(now);
    while (!frameTimes_.empty() && now - frameTimes_.front() > 2000) frameTimes_.erase(frameTimes_.begin());

    PipelineSnapshot sn;
    sn.mode = covered ? PipelineMode::Covered
              : found || now - lastHandMs_ < s.perf.idleAfterMs ? PipelineMode::Active
              : now - std::max(lastHandMs_, lastFaceSeenMs_) > static_cast<int64_t>(s.perf.sleepAfterSec) * 1000
                  ? PipelineMode::Sleep
                  : PipelineMode::Idle;
    if (covered) sn.reason = "Шторка камеры закрыта или камера выключена клавишей";
    sn.fps = frameTimes_.size() > 1 ? 1000.f * (frameTimes_.size() - 1) / std::max<int64_t>(1, now - frameTimes_.front()) : 0.f;
    sn.msFrame = msFrame; sn.msPalm = msPalm; sn.msHand = msHand; sn.msFace = msFace; sn.msEmbed = msEmbed;
    sn.frameW = rgb.width; sn.frameH = rgb.height;
    sn.hasHand = found;
    if (found) sn.hand = obs;
    if (hands.size() >= 2) { sn.hasSecondHand = true; sn.hand2 = hands[1]; }
    sn.twoHandPose = twoPose;
    if (now - lastDynamicMs_ < 1000) sn.lastDynamic = lastDynamic_;
    sn.faces = faces_;
    sn.light = ir ? light : lightEma_;
    sn.nightActive = nightOn_ || ir;
    sn.infrared = ir;
    sn.irAlternating = ir && irSel_.alternating();
    sn.device = palm_.ok() ? palm_.device() : "";
    sn.camera = camera_ ? camera_->statusText() : "";
    for (const auto& r : s.rules)
        if (r.enabled) sn.ruleProgress.emplace_back(r.id, engine_.progress(r.id));
    for (const auto& r : s.rules) {
        if (!r.enabled) continue;
        const auto* rb = findBuiltin(r.gesture);
        if (!found) sn.whyNot = "рука не найдена";
        else if (rb && rb->twoHands) {
            if (hands.size() < 2) sn.whyNot = "нужны две руки";
            else if (!rb->dynamic && twoPose != r.gesture) sn.whyNot = "другой жест";
            else sn.whyNot = "условия выполнены";
        }
        else if (!GestureEngine::handMatches(r, obs, &sn.whyNot)) {}
        else if (r.requireFace && !fc.anyFace) sn.whyNot = "нет лица в кадре";
        else if (r.person != PersonFilter::Anyone && fc.people.empty()) sn.whyNot = "лицо не распознано";
        else sn.whyNot = "условия выполнены";
        break;
    }
    tlk.lock();
    if (enroll_) {
        sn.enrollProgress = enroll_->start ? std::min(1.f, (now - enroll_->start) / static_cast<float>(enroll_->durationMs)) : 0.f;
        sn.taskHint = faces_.empty() ? "Посмотрите в камеру"
                                     : "Медленно поворачивайте голову влево-вправо, вверх-вниз · " + std::to_string(enroll_->got.size()) + " снимков";
    }
    if (record_) {
        const bool started = record_->start != 0 && now >= record_->start;
        sn.recordProgress = started ? std::min(1.f, (now - record_->start) / static_cast<float>(record_->ms)) : 0.f;
        const int left = started ? std::max(0, static_cast<int>((record_->ms - (now - record_->start) + 999) / 1000)) : record_->ms / 1000;
        if (record_->start == 0)
            sn.taskHint = "Покажите руку — запись начнётся после отсчёта";
        else if (!started)
            sn.taskHint = "Приготовьтесь… " + std::to_string((record_->start - now + 999) / 1000);
        else if (record_->dynamic)
            sn.taskHint = "Повторяйте движение · " + std::to_string(left) + " с";
        else
            sn.taskHint = "Держите знак, слегка меняя наклон кисти · " + std::to_string(left) + " с";
    }
    tlk.unlock();

    if (previewWanted_) {
        Image thumb;
        const int tw = std::min(480, rgb.width), th = rgb.height * tw / std::max(1, rgb.width);
        resizeBilinear(rgb, thumb, tw, th);
        if (ir) tintNightPreview(thumb);
        sn.previewW = tw;
        sn.previewH = th;
        sn.previewBgra.resize(static_cast<size_t>(tw) * th * 4);
        for (size_t i = 0, j = 0; i < thumb.data.size(); i += 3, j += 4) {
            sn.previewBgra[j] = thumb.data[i + 2];
            sn.previewBgra[j + 1] = thumb.data[i + 1];
            sn.previewBgra[j + 2] = thumb.data[i];
            sn.previewBgra[j + 3] = 255;
        }
        sn.previewSeq = ++previewSeq_;
    }
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        snap_ = std::move(sn);
    }
    const int notifyMs = previewWanted_ ? 66 : 500;
    if (cb_.onSnapshot && now - lastSnapshotNotify_ >= notifyMs) {
        lastSnapshotNotify_ = now;
        cb_.onSnapshot();
    }
}

void VisionPipeline::run() {
    uint64_t lastSeq = 0;
    int64_t nextDue = 0, lastFrameAt = nowMs(), lastLoadTry = 0;
    RawFrame frame;
    Image rgb;
    while (running_) {
        std::shared_ptr<const Settings> s;
        std::string reason;
        {
            std::lock_guard<std::mutex> lk(cfgMutex_);
            s = settings_;
            reason = pauseReason_;
        }
        const int64_t now = nowMs();
        const std::string key = std::to_string(static_cast<int>(s->perf.device)) + "/" +
                                std::to_string(s->perf.profile == PowerProfile::Eco ? 1 : s->perf.threads) + "/" +
                                (s->perf.int8Models ? "_int8" : "");
        if (key != loadedKey_ || (!modelError_.empty() && now - lastLoadTry > 10000)) {
            lastLoadTry = now;
            loadModels(*s);
        }

        auto publishMode = [&](PipelineMode m, const std::string& why) {
            {
                std::lock_guard<std::mutex> lk(snapMutex_);
                snap_.mode = m;
                snap_.reason = why;
                snap_.hasHand = false;
                snap_.faces.clear();
                snap_.fps = 0;
                snap_.camera = camera_ ? camera_->statusText() : "";
            }
            if (cb_.onSnapshot && now - lastSnapshotNotify_ >= 500) {
                lastSnapshotNotify_ = now;
                cb_.onSnapshot();
            }
        };

        if (paused_ || !s->enabled) {
            for (auto& tr : tracks_) resetTrack(tr);
            twoHands_.reset();
            custom_.reset();
            engine_.reset();
            mailbox_.clear();
            publishMode(PipelineMode::Paused, s->enabled ? reason : "Распознавание выключено");
            mailbox_.take(frame, lastSeq, 250);  // also serves as an interruptible sleep
            lastFrameAt = nowMs();
            continue;
        }
        if (!modelError_.empty()) {
            publishMode(PipelineMode::ModelError, modelError_);
            mailbox_.take(frame, lastSeq, 500);
            continue;
        }

        const int fps = targetFps(*s);
        const int interval = 1000 / std::max(1, fps);
        if (now < nextDue) {
            // Interruptible wait; frames that arrive meanwhile simply overwrite the slot.
            std::this_thread::sleep_for(std::chrono::milliseconds(std::min<int64_t>(nextDue - now, 100)));
            continue;
        }
        if (!mailbox_.take(frame, lastSeq, 400)) {
            if (nowMs() - lastFrameAt > 3000) publishMode(PipelineMode::NoCamera, camera_ ? camera_->statusText() : "");
            continue;
        }
        lastSeq = frame.seq;
        lastFrameAt = nowMs();
        nextDue = now + interval;

        const int down = std::max(1, frame.width / 640);
        convertFrame(frame.data.data(), frame.width, frame.height, frame.stride, frame.format, down, true, rgb,
                     frame.uvOffset);
        processFrame(rgb, frame.infrared, nowMs(), *s);
    }
}

}  // namespace vl
