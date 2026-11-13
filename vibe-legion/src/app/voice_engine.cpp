#include "app/voice_engine.hpp"

#include <objbase.h>

#include <algorithm>
#include <chrono>

#include "ml/speaker.hpp"
#include "platform/win/voice_io.hpp"

namespace vl {

namespace {
constexpr wchar_t kSpeakerModel[] = L"voice_speaker_resnet34.onnx";
constexpr float kMinPhraseSec = 0.35f, kMaxPhraseSec = 5.2f;
constexpr uint64_t kRecordWaitMs = 8000;

float levelFromDb(float db) { return std::clamp((db + 60.f) / 50.f, 0.f, 1.f); }
}  // namespace

struct VoiceEngine::Models {
    ml::SpeakerEmbedder speaker;
};

VoiceEngine::VoiceEngine(std::filesystem::path modelsDir, Callbacks cb) : modelsDir_(std::move(modelsDir)), cb_(std::move(cb)) {
    thread_ = std::thread([this] { run(); });
}

VoiceEngine::~VoiceEngine() {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        quit_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
    delete models_;
}

void VoiceEngine::configure(const VoiceSettings& s, const voice::VoiceStore& store) {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        settings_ = s;
        store_ = store;
    }
    cv_.notify_all();
}

void VoiceEngine::setActive(bool on) {
    if (active_.exchange(on) != on) cv_.notify_all();
}

bool VoiceEngine::installed(std::string* missingRu) const {
    std::error_code ec;
    if (std::filesystem::exists(modelsDir_ / kSpeakerModel, ec)) return true;
    if (missingRu) *missingRu = "Нет модели сравнения голосов (models\\voice_speaker_resnet34.onnx). Запустите scripts\\fetch-deps.ps1 и пересоберите приложение.";
    return false;
}

void VoiceEngine::recordTake(const std::string& commandId) {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        recordId_ = commandId;
        recordDeadline_ = GetTickCount64() + kRecordWaitMs;
    }
    cv_.notify_all();
}

void VoiceEngine::cancelRecording() {
    std::lock_guard<std::mutex> lk(mutex_);
    recordId_.clear();
}

std::string VoiceEngine::recordingId() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return recordId_;
}

std::string VoiceEngine::errorText() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return error_;
}

void VoiceEngine::setState(State s) {
    if (state_.exchange(s) != s) notify(true);
}

void VoiceEngine::notify(bool force) {
    const uint64_t now = GetTickCount64();
    if (!force && now - lastNotify_ < 80) return;  // ~12 Hz is plenty for a level meter
    lastNotify_ = now;
    if (cb_.onState) cb_.onState();
}

void VoiceEngine::run() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    win::MicCapture mic;
    bool micOpen = false;
    voice::Endpointer ep;
    std::vector<float> block;
    auto closeMic = [&] {
        if (micOpen) mic.stop();
        micOpen = false;
        ep = voice::Endpointer();
        level_ = 0.f;
        speaking_ = false;
    };
    auto fail = [&](const std::string& why, int waitMs) {
        closeMic();
        {
            std::lock_guard<std::mutex> lk(mutex_);
            error_ = why;
            if (!recordId_.empty()) {  // tell the page why the recording did not start
                VoiceTakeResult r;
                r.commandId = recordId_;
                r.error = why;
                recordId_.clear();
                if (cb_.onTake) cb_.onTake(r);
            }
        }
        setState(State::Error);
        std::unique_lock<std::mutex> lk(mutex_);
        cv_.wait_for(lk, std::chrono::milliseconds(waitMs), [this] { return quit_; });
    };

    for (;;) {
        bool want = false, recording = false;
        {
            std::lock_guard<std::mutex> lk(mutex_);
            if (quit_) break;
            recording = !recordId_.empty();
            if (recording && GetTickCount64() > recordDeadline_) {
                VoiceTakeResult r;
                r.commandId = recordId_;
                r.error = "ничего не услышал — нажмите запись и сразу скажите фразу";
                recordId_.clear();
                recording = false;
                if (cb_.onTake) cb_.onTake(r);
            }
            bool anyReady = false;
            for (const auto& c : settings_.commands) {
                const voice::VoiceModel* m = store_.find(c.id);
                anyReady = anyReady || (c.enabled && m && m->ready());
            }
            want = recording || (settings_.enabled && active_ && anyReady);
        }
        if (!want) {
            closeMic();
            setState(State::Off);
            std::unique_lock<std::mutex> lk(mutex_);
            cv_.wait_for(lk, std::chrono::milliseconds(1000));
            continue;
        }
        if (!models_) {
            setState(State::Loading);
            auto* m = new Models;
            std::string err;
            if (!m->speaker.load(modelsDir_ / kSpeakerModel, &err)) {
                delete m;
                fail(err.empty() ? "модель сравнения голосов не загрузилась" : err, 10000);
                continue;
            }
            models_ = m;
        }
        if (!micOpen) {
            std::string err;
            if (!mic.start(&err)) {
                fail(err, 3000);
                continue;
            }
            micOpen = true;
            ep = voice::Endpointer();
            std::lock_guard<std::mutex> lk(mutex_);
            error_.clear();
        }
        setState(recording ? State::Recording : State::Listening);

        block.clear();
        if (!mic.read(block)) {
            fail("микрофон отключился", 2000);
            continue;
        }
        if (!block.empty()) {
            ep.feed(block.data(), block.size());
            level_ = levelFromDb(ep.lastDb());
            const bool sp = ep.state() != voice::Endpointer::State::Waiting;
            if (speaking_.exchange(sp) != sp) notify(true);
            if (ep.state() == voice::Endpointer::State::Done) {
                const std::vector<float> phrase = ep.utterance();
                ep.restart();
                speaking_ = false;
                handlePhrase(phrase);
            }
            // The level meter is only worth repainting while somebody is looking at it;
            // the app decides (it ignores the message when the window is closed).
            notify();
        }
        Sleep(20);
    }
    closeMic();
    CoUninitialize();
}

void VoiceEngine::handlePhrase(const std::vector<float>& pcm) {
    const float sec = static_cast<float>(pcm.size()) / voice::kRate;
    VoiceSettings cfg;
    voice::VoiceStore store;
    std::string recId;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        cfg = settings_;
        recId = recordId_;
        if (recId.empty()) store = store_;  // matching needs the takes; recording does not
    }
    voice::VoiceTake take;
    take.feat = voice::phraseFeatures(pcm, &take.frames);

    if (!recId.empty()) {
        VoiceTakeResult r;
        r.commandId = recId;
        r.seconds = sec;
        if (sec < kMinPhraseSec || take.frames < 12) r.error = "слишком коротко — это был щелчок или шум, повторите фразу";
        else if (sec > kMaxPhraseSec) r.error = "слишком длинно — команда должна быть короче 5 секунд";
        else if (!models_ || !models_->speaker.embed(pcm, take.emb)) r.error = "не удалось разобрать голос, повторите громче";
        else {
            r.ok = true;
            r.take = std::move(take);
        }
        {
            std::lock_guard<std::mutex> lk(mutex_);
            if (recordId_ == recId) recordId_.clear();
        }
        if (cb_.onTake) cb_.onTake(r);
        return;
    }

    if (sec < kMinPhraseSec || sec > kMaxPhraseSec || take.frames < 12) return;  // a click, or a long talk
    const voice::VoiceMatch m = voice::matchPhrase(store, cfg.commands, take, cfg.intonation);
    VoiceHeard h;
    h.seconds = sec;
    h.ratio = m.ratio;
    h.commandId = m.nearest;
    if (!m.id.empty()) {
        // Only now the (more expensive) voice check: ordinary talk never gets this far.
        std::vector<float> emb;
        const voice::VoiceModel* model = store.find(m.id);
        if (models_ && model && models_->speaker.embed(pcm, emb)) h.score = voice::ownerScore(*model, emb);
        h.kind = !cfg.ownerOnly || h.score >= cfg.matchThreshold ? VoiceHeard::Kind::Command : VoiceHeard::Kind::NotOwner;
    }
    if (cb_.onHeard) cb_.onHeard(h);
}

}  // namespace vl
