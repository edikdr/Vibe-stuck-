#include "vision/pipeline.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace vl {

namespace {

int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

float median(std::vector<float> v) {
    if (v.empty()) return 0.f;
    std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
    return v[v.size() / 2];
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
    for (auto& f : lmFilter_) f = OneEuro(2.0f, 0.03f, 1.0f);
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

void VisionPipeline::startEnrollment(const std::string& personId, int samples) {
    std::lock_guard<std::mutex> lk(taskMutex_);
    enroll_ = std::make_unique<Enroll>();
    enroll_->person = personId;
    enroll_->want = std::clamp(samples, 3, static_cast<int>(FaceDatabase::kMaxSamplesPerModality));
}

void VisionPipeline::startCalibration(float knownCm) {
    std::lock_guard<std::mutex> lk(taskMutex_);
    calib_ = std::make_unique<Calib>();
    calib_->knownCm = knownCm;
}

void VisionPipeline::startRecording(const std::string& id, bool rot, bool mir, int ms) {
    std::lock_guard<std::mutex> lk(taskMutex_);
    record_ = std::make_unique<Record>();
    record_->id = id;
    record_->rot = rot;
    record_->mir = mir;
    record_->ms = std::clamp(ms, 500, 10000);
}

void VisionPipeline::cancelTasks() {
    std::lock_guard<std::mutex> lk(taskMutex_);
    enroll_.reset();
    calib_.reset();
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
    dist_.intrinsics.hfovDeg = s.distance.hfovDeg;
    dist_.setCalibration(s.distance.calibration);

    double msPalm = 0, msHand = 0, msFace = 0, msEmbed = 0;

    // ---- faces
    const bool handActive = now - lastHandMs_ < s.perf.idleAfterMs;
    bool needRecognition = false;
    if (!db->people.empty()) {
        needRecognition = s.faces.gate == FaceGate::KnownOnly || previewWanted_;
        for (const auto& r : s.rules)
            if (r.enabled && r.person != PersonFilter::Anyone) needRecognition = true;
    }
    std::unique_lock<std::mutex> tlk(taskMutex_);
    const bool enrolling = enroll_ != nullptr;
    tlk.unlock();
    const bool needFaces = s.faces.gate != FaceGate::Off || needRecognition || enrolling || previewWanted_ ||
                           std::any_of(s.rules.begin(), s.rules.end(), [](const GestureRule& r) {
                               return r.enabled && (r.requireLooking || r.person != PersonFilter::Anyone);
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
            fi.distanceCm = dist_.faceRawCm(d, rgb.width);
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
                auto p = modelsDir_ / "face_recognition_sface_2021dec_int8.onnx";
                if (!std::filesystem::exists(p)) p = modelsDir_ / "face_recognition_sface_2021dec.onnx";
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
                    const bool enrollThis = enroll_ && k == largest && now - enroll_->last >= 150;
                    tlk.unlock();
                    const bool stale = now - faceRecognizedMs_[k] >= s.faces.recognizeEveryMs || faces_[k].personId.empty();
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
                            enroll_->got.push_back(e);
                            enroll_->ir = ir;
                            enroll_->last = now;
                            if (static_cast<int>(enroll_->got.size()) >= enroll_->want) {
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

    // ---- hand
    HandLandmarks raw;
    bool found = false;
    if (palm_.ok() && hand_.ok()) {
        if (tracking_) {
            found = hand_.run(rgb, handRoiFromLandmarks(lastLm_), raw);
            msHand += hand_.lastMs();
        }
        if (!found) {
            auto palms = palm_.detect(rgb, 0.5f, 1);
            msPalm = palm_.lastMs();
            if (!palms.empty()) {
                found = hand_.run(rgb, handRoiFromPalm(palms[0]), raw);
                msHand += hand_.lastMs();
            }
        }
    }
    tracking_ = found;

    std::vector<HandObservation> hands;
    std::string dynamic;
    HandObservation obs;
    if (found) {
        lastLm_ = raw;
        lastHandMs_ = now;
        obs.lm = raw;
        for (int i = 0; i < kHandLandmarks; ++i) {
            obs.lm.image[i].x = lmFilter_[2 * i].filter(raw.image[i].x, dt);
            obs.lm.image[i].y = lmFilter_[2 * i + 1].filter(raw.image[i].y, dt);
        }
        obs.isRight = (raw.rightness > 0.5f) != s.camera.swapHandedness;
        const float rawCm = dist_.handRawCm(raw, rgb.width);
        obs.distanceCm = dist_.smoothHand(rawCm);
        analyzeHand(obs);
        matchCustom(obs, s.customGestures);
        dynamic = swipe_.update(obs, now);
        if (!dynamic.empty()) { lastDynamic_ = dynamic; lastDynamicMs_ = now; }
        hands.push_back(obs);

        tlk.lock();
        if (calib_) {
            if (calib_->start == 0) calib_->start = now;
            if (obs.pose == "open_palm" && rawCm > 0) calib_->raw.push_back(rawCm / dist_.calibration());
            if (now - calib_->start > 2000 && calib_->raw.size() >= 12) {
                const float scale = DistanceEstimator::solveCalibration(calib_->knownCm, median(calib_->raw));
                calib_.reset();
                tlk.unlock();
                dist_.resetHand();
                if (cb_.onCalibrated) cb_.onCalibrated(scale);
                tlk.lock();
            }
        }
        if (record_) {
            if (record_->start == 0) record_->start = now;
            record_->got.push_back(handTemplate(obs.lm, obs.isRight, record_->rot, record_->mir));
            if (now - record_->start >= record_->ms) {
                auto done = std::move(record_);
                tlk.unlock();
                // Keep at most 24 evenly spaced samples.
                std::vector<std::array<float, 42>> keep;
                const size_t n = done->got.size(), m = std::min<size_t>(24, n);
                for (size_t i = 0; i < m; ++i) keep.push_back(done->got[i * n / m]);
                if (cb_.onRecorded) cb_.onRecorded(done->id, std::move(keep));
                tlk.lock();
            }
        }
        tlk.unlock();
    } else {
        dist_.resetHand();
        swipe_.reset();
        for (auto& f : lmFilter_) f.reset();
    }

    // ---- rules
    FaceContext fc;
    fc.anyFace = !faces_.empty();
    for (const auto& f : faces_) {
        fc.looking = fc.looking || f.looking;
        if (!f.personId.empty()) fc.people.push_back(f.personId);
    }
    auto events = engine_.update(hands, dynamic, fc, s.rules, s.faces.gate, now);
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
        if (!found) sn.whyNot = "рука не найдена";
        else if (!GestureEngine::handMatches(r, obs, &sn.whyNot)) {}
        else if (s.faces.gate == FaceGate::AnyFace && !fc.anyFace) sn.whyNot = "нет лица в кадре";
        else if (s.faces.gate == FaceGate::KnownOnly && fc.people.empty()) sn.whyNot = "лицо не распознано";
        else sn.whyNot = "условия выполнены";
        break;
    }
    tlk.lock();
    if (enroll_) { sn.enrollProgress = static_cast<float>(enroll_->got.size()) / enroll_->want; sn.taskHint = faces_.empty() ? "Посмотрите в камеру" : "Медленно поворачивайте голову"; }
    if (calib_) { sn.calibProgress = std::min(1.f, calib_->raw.size() / 12.f); sn.taskHint = found ? "Держите ладонь неподвижно" : "Покажите открытую ладонь"; }
    if (record_) { sn.recordProgress = record_->start ? std::min(1.f, (now - record_->start) / static_cast<float>(record_->ms)) : 0.f; sn.taskHint = found ? "Держите жест" : "Покажите руку"; }
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
            tracking_ = false;
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
