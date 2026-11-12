// Offline check of the voice chain on WAV files (16-bit PCM, any rate, mono or stereo):
//   vl_voicetest <models dir> <vosk dll dir> "<phrase 1>|<phrase 2>|..." a.wav b.wav ...
// Prints what the recogniser heard in each file (closed list of phrases) and the
// similarity of every pair of voices.
#include <cstdio>
#include <cstring>
#include <fstream>

#include "core/voice.hpp"
#include "ml/speaker.hpp"
#include "platform/win/voice_io.hpp"

#include <objbase.h>

using namespace vl;

static bool readWav(const std::filesystem::path& p, std::vector<float>& pcm16k) {
    std::ifstream f(p, std::ios::binary);
    std::vector<char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (d.size() < 44 || std::memcmp(d.data(), "RIFF", 4) != 0) return false;
    int ch = 1, rate = 16000, bits = 16;
    size_t at = 12;
    const char* data = nullptr;
    size_t dataLen = 0;
    while (at + 8 <= d.size()) {
        uint32_t len = 0;
        std::memcpy(&len, d.data() + at + 4, 4);
        if (std::memcmp(d.data() + at, "fmt ", 4) == 0) {
            uint16_t c = 0, b = 0;
            uint32_t r = 0;
            std::memcpy(&c, d.data() + at + 10, 2);
            std::memcpy(&r, d.data() + at + 12, 4);
            std::memcpy(&b, d.data() + at + 22, 2);
            ch = c; rate = static_cast<int>(r); bits = b;
        } else if (std::memcmp(d.data() + at, "data", 4) == 0) {
            data = d.data() + at + 8;
            dataLen = std::min<size_t>(len, d.size() - at - 8);
            break;
        }
        at += 8 + len + (len & 1);
    }
    if (!data || bits != 16 || ch < 1) return false;
    const size_t n = dataLen / 2 / static_cast<size_t>(ch);
    std::vector<float> mono(n);
    const auto* s = reinterpret_cast<const int16_t*>(data);
    for (size_t i = 0; i < n; ++i) {
        int sum = 0;
        for (int c = 0; c < ch; ++c) sum += s[i * static_cast<size_t>(ch) + static_cast<size_t>(c)];
        mono[i] = static_cast<float>(sum) / static_cast<float>(ch) / 32768.f;
    }
    // Linear resampling is enough for a test tool.
    const double step = static_cast<double>(rate) / voice::kRate;
    pcm16k.clear();
    for (double x = 0; x + 1 < static_cast<double>(n); x += step) {
        const size_t i = static_cast<size_t>(x);
        const float fr = static_cast<float>(x - static_cast<double>(i));
        pcm16k.push_back(mono[i] * (1 - fr) + mono[i + 1] * fr);
    }
    return !pcm16k.empty();
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 5) {
        std::printf("usage: vl_voicetest <models dir> <vosk dir> \"phrase|phrase\" a.wav ...\n");
        return 2;
    }
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const std::filesystem::path models = argv[1], voskDir = argv[2];
    std::vector<VoiceCommand> cmds;
    {
        const std::string all = win::narrow(argv[3]);
        size_t pos = 0;
        while (pos <= all.size()) {
            size_t bar = all.find('|', pos);
            if (bar == std::string::npos) bar = all.size();
            VoiceCommand c;
            c.phrase = all.substr(pos, bar - pos);
            if (!c.phrase.empty()) cmds.push_back(c);
            pos = bar + 1;
        }
    }
    std::string err;
    win::Vosk vosk;
    if (!vosk.load(voskDir, models / L"vosk-model-small-ru", &err)) std::printf("vosk: %s\n", err.c_str());
    ml::SpeakerEmbedder spk;
    if (!spk.load(models / L"voice_speaker_resnet34.onnx", &err)) std::printf("speaker: %s\n", err.c_str());
    const std::string grammar = voice::grammarJson(cmds);
    std::printf("grammar: %s\n", grammar.c_str());
    for (const auto& c : cmds)
        for (const auto& w : voice::splitWords(voice::normalizePhrase(c.phrase)))
            if (vosk.ok() && !vosk.hasWord(w)) std::printf("  not in dictionary: %s\n", w.c_str());

    std::vector<std::vector<float>> embs;
    std::vector<std::string> names;
    for (int i = 4; i < argc; ++i) {
        std::vector<float> pcm;
        if (!readWav(argv[i], pcm)) {
            std::printf("%ls: cannot read\n", argv[i]);
            continue;
        }
        // The same path as the app: end-pointing, then recognition and embedding of the utterance.
        voice::Endpointer ep;
        for (size_t at = 0; at < pcm.size() && ep.state() != voice::Endpointer::State::Done; at += 480)
            ep.feed(pcm.data() + at, std::min<size_t>(480, pcm.size() - at));
        const std::vector<float> utt = ep.utterance();
        const std::string text = vosk.recognize(utt.empty() ? pcm : utt, grammar);
        std::vector<float> e;
        const bool ok = spk.embed(utt.empty() ? pcm : utt, e);
        std::printf("%-28ls %5.2f s, speech %5.2f s  heard: \"%s\"  cmd: %d  emb: %s (%zu, %.0f ms)\n",
                    std::filesystem::path(argv[i]).filename().c_str(), pcm.size() / 16000.0, utt.size() / 16000.0, text.c_str(),
                    voice::matchCommand(cmds, text), ok ? "ok" : "FAIL", e.size(), spk.lastMs());
        if (ok) {
            embs.push_back(e);
            names.push_back(win::narrow(std::filesystem::path(argv[i]).filename().wstring()));
        }
    }
    std::printf("\nsimilarity:\n");
    for (size_t i = 0; i < embs.size(); ++i) {
        for (size_t j = 0; j < embs.size(); ++j) std::printf("%6.2f", voice::cosine(embs[i], embs[j]));
        std::printf("   %s\n", names[i].c_str());
    }
    return 0;
}
