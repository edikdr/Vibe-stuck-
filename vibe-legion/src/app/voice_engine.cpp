#include "app/voice_engine.hpp"

#include <objbase.h>

#include <algorithm>
#include <chrono>

#include "ml/speaker.hpp"
#include "platform/win/voice_io.hpp"

namespace vl {

namespace {
constexpr auto kUnloadAfter = std::chrono::minutes(5);
constexpr wchar_t kSpeakerModel[] = L"voice_speaker_resnet34.onnx";
constexpr wchar_t kSpeechModel[] = L"vosk-model-small-ru";

float levelFromDb(float db) { return std::clamp((db + 60.f) / 50.f, 0.f, 1.f); }
}  // namespace

struct VoiceEngine::Models {
    win::Vosk vosk;
    ml::SpeakerEmbedder speaker;
};

VoiceEngine::VoiceEngine(std::filesystem::path modelsDir, std::filesystem::path voskDir, Callbacks cb)
    : modelsDir_(std::move(modelsDir)), voskDir_(std::move(voskDir)), cb_(std::move(cb)) {
    thread_ = std::thread([this] { run(); });
}

VoiceEngine::~VoiceEngine() {
    cancel_ = true;
    {
        std::lock_guard<std::mutex> lk(jobMutex_);
        job_ = Job::Quit;
    }
    jobCv_.notify_all();
    if (thread_.joinable()) thread_.join();
    delete models_;
}

void VoiceEngine::configure(const VoiceSettings& s, const voice::VoicePrint& print) {
    std::lock_guard<std::mutex> lk(cfgMutex_);
    settings_ = s;
    print_ = print;
}

bool VoiceEngine::installed(std::string* missingRu) const {
    std::error_code ec;
    const char* what = nullptr;
    if (!std::filesystem::exists(voskDir_ / L"libvosk.dll", ec)) what = "библиотеки распознавания речи (vosk\\libvosk.dll)";
    else if (!std::filesystem::exists(modelsDir_ / kSpeechModel / L"am" / L"final.mdl", ec)) what = "модели русской речи (models\\vosk-model-small-ru)";
    else if (!std::filesystem::exists(modelsDir_ / kSpeakerModel, ec)) what = "модели сравнения голосов (models\\voice_speaker_resnet34.onnx)";
    if (!what) return true;
    if (missingRu) *missingRu = std::string("Нет ") + what + ". Запустите scripts\\fetch-deps.ps1 и пересоберите приложение.";
    return false;
}

bool VoiceEngine::listen() {
    std::lock_guard<std::mutex> lk(jobMutex_);
    if (job_ != Job::None || state_ != State::Idle) return false;
    cancel_ = false;
    job_ = Job::Listen;
    jobCv_.notify_all();
    return true;
}

bool VoiceEngine::enroll(int ms) {
    std::lock_guard<std::mutex> lk(jobMutex_);
    if (job_ != Job::None || state_ != State::Idle) return false;
    cancel_ = false;
    enrollMs_ = std::clamp(ms, 5000, 60000);
    job_ = Job::Enroll;
    jobCv_.notify_all();
    return true;
}

void VoiceEngine::cancel() { cancel_ = true; }

void VoiceEngine::warmUp() {
    if (loaded_ || warmFailed_) return;
    std::lock_guard<std::mutex> lk(jobMutex_);
    if (job_ != Job::None || state_ != State::Idle) return;
    job_ = Job::Warm;
    jobCv_.notify_all();
}

bool VoiceEngine::unknownWords(const std::string& phrase, std::vector<std::string>& missing) {
    missing.clear();
    std::unique_lock<std::mutex> lk(modelMutex_, std::try_to_lock);
    if (!lk.owns_lock() || !models_ || !models_->vosk.ok()) return false;
    for (const auto& w : voice::splitWords(voice::normalizePhrase(phrase)))
        if (!models_->vosk.hasWord(w)) missing.push_back(w);
    return true;
}

void VoiceEngine::setState(State s) {
    state_ = s;
    lastNotify_ = 0;
    notify();
}

void VoiceEngine::notify() {
    const uint64_t now = GetTickCount64();
    if (now - lastNotify_ < 60) return;  // ~15 Hz is plenty for a level meter
    lastNotify_ = now;
    if (cb_.onState) cb_.onState();
}

bool VoiceEngine::ensureModels(std::string* error) {
    std::lock_guard<std::mutex> lk(modelMutex_);
    if (models_ && models_->vosk.ok() && models_->speaker.ok()) return true;
    delete models_;
    loaded_ = false;
    models_ = new Models;
    std::string err;
    if (!models_->vosk.load(voskDir_, modelsDir_ / kSpeechModel, &err) ||
        !models_->speaker.load(modelsDir_ / kSpeakerModel, &err)) {
        if (error) *error = err.empty() ? "модели голоса не загрузились" : err;
        delete models_;
        models_ = nullptr;
        warmFailed_ = true;
        return false;
    }
    loaded_ = true;
    warmFailed_ = false;
    return true;
}

void VoiceEngine::run() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lk(jobMutex_);
            if (!jobCv_.wait_for(lk, kUnloadAfter, [this] { return job_ != Job::None; })) {
                // Idle for a while: give the memory back (the speech model is ~100 MB).
                std::lock_guard<std::mutex> ml(modelMutex_);
                delete models_;
                models_ = nullptr;
                loaded_ = false;
                continue;
            }
            job = job_;
        }
        if (job == Job::Quit) break;
        if (job == Job::Listen) doListen();
        else if (job == Job::Enroll) doEnroll();
        else if (job == Job::Warm) {
            setState(State::Loading);
            ensureModels(nullptr);
        }
        level_ = 0.f;
        progress_ = 0.f;
        {
            std::lock_guard<std::mutex> lk(jobMutex_);
            if (job_ != Job::Quit) job_ = Job::None;
        }
        setState(State::Idle);
    }
    CoUninitialize();
}

void VoiceEngine::doListen() {
    VoiceSettings cfg;
    voice::VoicePrint print;
    {
        std::lock_guard<std::mutex> lk(cfgMutex_);
        cfg = settings_;
        print = print_;
    }
    VoiceOutcome out;
    auto finish = [&](VoiceOutcome::Kind k) {
        out.kind = k;
        if (cb_.onOutcome) cb_.onOutcome(out);
    };
    if (cfg.ownerOnly && print.empty()) return finish(VoiceOutcome::Kind::NoVoicePrint);

    // Open the microphone first: it buffers while the models load, so a command spoken
    // straight after the gesture is not lost.
    setState(State::Loading);
    win::MicCapture mic;
    if (!mic.start(&out.error)) return finish(VoiceOutcome::Kind::Error);
    if (!ensureModels(&out.error)) return finish(VoiceOutcome::Kind::Error);

    setState(State::Listening);
    voice::Endpointer ep(700, 6000);
    std::vector<float> block;
    while (ep.state() != voice::Endpointer::State::Done) {
        if (cancel_) return finish(VoiceOutcome::Kind::Cancelled);
        block.clear();
        if (!mic.read(block)) {
            out.error = "микрофон отключился";
            return finish(VoiceOutcome::Kind::Error);
        }
        if (!block.empty()) ep.feed(block.data(), block.size());
        level_ = levelFromDb(ep.lastDb());
        if (ep.state() == voice::Endpointer::State::Waiting) {
            progress_ = std::min(1.f, static_cast<float>(ep.waitedMs()) / static_cast<float>(cfg.listenMs));
            if (ep.waitedMs() >= cfg.listenMs) return finish(VoiceOutcome::Kind::NoSpeech);
        }
        notify();
        Sleep(20);
    }
    mic.stop();
    const std::vector<float> utt = ep.utterance();

    setState(State::Recognizing);
    {
        std::lock_guard<std::mutex> lk(modelMutex_);
        if (!models_) {
            out.error = "модели голоса выгружены";
            return finish(VoiceOutcome::Kind::Error);
        }
        out.heard = models_->vosk.recognize(utt, voice::grammarJson(cfg.commands));
        // The voice is compared on the speech itself, without the pauses around it.
        std::vector<float> speech = voice::speechOnly(utt);
        if (speech.size() < ml::SpeakerEmbedder::kMinSamples) speech = utt;
        std::vector<float> emb;
        if (!print.empty() && models_->speaker.embed(speech, emb)) out.score = voice::ownerScore(print, emb);
    }
    if (out.heard == "[unk]") out.heard.clear();
    const int idx = voice::matchCommand(cfg.commands, out.heard);
    if (idx < 0) return finish(VoiceOutcome::Kind::NotUnderstood);
    // With a short list of phrases the recogniser squeezes any long talk into the nearest
    // command. A real command lasts about as long as its letters take to say.
    {
        const std::string norm = voice::normalizePhrase(out.heard);
        size_t letters = 0;
        for (unsigned char ch : norm) letters += (ch & 0xC0) != 0x80 && ch != ' ';
        const float speechSec = static_cast<float>(voice::speechOnly(utt).size()) / voice::kRate;
        if (speechSec > 0.8f + 0.22f * static_cast<float>(letters)) return finish(VoiceOutcome::Kind::NotUnderstood);
    }
    if (cfg.ownerOnly && out.score < cfg.matchThreshold) return finish(VoiceOutcome::Kind::NotOwner);
    out.commandId = cfg.commands[static_cast<size_t>(idx)].id;
    finish(VoiceOutcome::Kind::Command);
}

void VoiceEngine::doEnroll() {
    VoiceEnrollOutcome out;
    auto finish = [&] {
        if (cb_.onEnrolled) cb_.onEnrolled(out);
    };
    setState(State::Loading);
    win::MicCapture mic;
    if (!mic.start(&out.error)) return finish();
    if (!ensureModels(&out.error)) return finish();

    setState(State::Enrolling);
    std::vector<float> pcm, block;
    const size_t want = static_cast<size_t>(enrollMs_) * voice::kRate / 1000;
    while (pcm.size() < want) {
        if (cancel_) {
            out.error = "запись отменена";
            return finish();
        }
        block.clear();
        if (!mic.read(block)) {
            out.error = "микрофон отключился";
            return finish();
        }
        if (!block.empty()) {
            const size_t tail = std::min<size_t>(block.size(), voice::kRate / 20);
            level_ = levelFromDb(voice::levelDb(block.data() + block.size() - tail, tail));
            pcm.insert(pcm.end(), block.begin(), block.end());
        }
        progress_ = std::min(1.f, static_cast<float>(pcm.size()) / static_cast<float>(want));
        notify();
        Sleep(20);
    }
    mic.stop();

    setState(State::Recognizing);
    const std::vector<float> speech = voice::speechOnly(pcm);
    out.speechSec = static_cast<float>(speech.size()) / voice::kRate;
    if (out.speechSec < 5.f) {
        out.error = "слишком мало речи (" + std::to_string(static_cast<int>(out.speechSec)) +
                    " с из нужных 5): читайте текст вслух всё время записи, ближе к микрофону";
        return finish();
    }
    {
        std::lock_guard<std::mutex> lk(modelMutex_);
        if (!models_) {
            out.error = "модели голоса выгружены";
            return finish();
        }
        // Pieces the length of real commands (1.5 s) and longer ones (3 s): short phrases
        // are compared with short samples, which is where speaker models are weakest.
        struct Cut { float len, hop; };
        for (const Cut c : {Cut{1.5f, 1.0f}, Cut{3.0f, 1.5f}}) {
            const size_t len = static_cast<size_t>(c.len * voice::kRate), hop = static_cast<size_t>(c.hop * voice::kRate);
            for (size_t at = 0; at + len <= speech.size() && out.samples.size() < voice::VoicePrint::kMaxSamples / 2; at += hop) {
                std::vector<float> piece(speech.begin() + static_cast<std::ptrdiff_t>(at), speech.begin() + static_cast<std::ptrdiff_t>(at + len));
                std::vector<float> emb;
                if (models_->speaker.embed(piece, emb)) out.samples.push_back(std::move(emb));
            }
        }
    }
    if (out.samples.size() < 4) {
        out.error = "не удалось получить образцы голоса";
        return finish();
    }
    // Leave-one-out: what the owner's own voice scores against the rest of the samples.
    double sum = 0;
    for (size_t i = 0; i < out.samples.size(); ++i) {
        voice::VoicePrint rest;
        for (size_t j = 0; j < out.samples.size(); ++j)
            if (j != i) rest.samples.push_back(out.samples[j]);
        sum += voice::ownerScore(rest, out.samples[i]);
    }
    out.consistency = static_cast<float>(sum / static_cast<double>(out.samples.size()));
    out.ok = true;
    finish();
}

}  // namespace vl
