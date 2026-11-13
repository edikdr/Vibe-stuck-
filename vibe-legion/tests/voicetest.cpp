// Offline check of the voice chain on WAV files (16-bit PCM, any rate, mono or stereo):
//   vl_voicetest <speaker model.onnx> [intonation 0..1] a.wav b.wav ...
// Every file goes through the same path as the live microphone (end-pointing, phrase
// features, speaker embedding). Prints the DTW distance and the voice similarity of
// every pair: used to calibrate the thresholds in core/voice.cpp.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

#include "core/voice.hpp"
#include "ml/speaker.hpp"

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
    const double step = static_cast<double>(rate) / voice::kRate;  // linear resampling: fine for a test tool
    pcm16k.clear();
    for (double x = 0; x + 1 < static_cast<double>(n); x += step) {
        const size_t i = static_cast<size_t>(x);
        const float fr = static_cast<float>(x - static_cast<double>(i));
        pcm16k.push_back(mono[i] * (1 - fr) + mono[i + 1] * fr);
    }
    return !pcm16k.empty();
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: vl_voicetest <speaker model.onnx> [intonation] a.wav ...\n");
        return 2;
    }
    std::string err;
    ml::SpeakerEmbedder spk;
    auto toPath = [](const char* s) { return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(s))); };
    if (!spk.load(toPath(argv[1]), &err)) std::printf("speaker model: %s\n", err.c_str());
    int first = 2;
    float intonation = 0.5f;
    if (std::strchr(argv[2], '.') && !std::strstr(argv[2], ".wav")) intonation = static_cast<float>(std::atof(argv[first++]));

    std::vector<voice::VoiceTake> takes;
    std::vector<std::string> names;
    for (int i = first; i < argc; ++i) {
        std::vector<float> pcm;
        const auto path = toPath(argv[i]);
        if (!readWav(path, pcm)) {
            std::printf("%s: cannot read\n", argv[i]);
            continue;
        }
        pcm.insert(pcm.end(), voice::kRate, 0.f);  // trailing quiet so the phrase ends
        voice::Endpointer ep;
        for (size_t at = 0; at < pcm.size() && ep.state() != voice::Endpointer::State::Done; at += 320)
            ep.feed(pcm.data() + at, std::min<size_t>(320, pcm.size() - at));
        const std::vector<float> utt = ep.utterance();
        voice::VoiceTake t;
        t.feat = voice::phraseFeatures(utt, &t.frames);
        spk.embed(utt, t.emb);
        int voiced = 0;
        for (int k = 0; k < t.frames; ++k) voiced += t.feat[static_cast<size_t>(k) * voice::kTplDim + voice::kCep + 2] > 0.5f;
        std::printf("%2zu %-26s phrase %.2f s, %3d frames, voiced %2d %%, emb %zu\n", takes.size(), path.filename().string().c_str(),
                    utt.size() / 16000.0, t.frames, t.frames ? voiced * 100 / t.frames : 0, t.emb.size());
        takes.push_back(std::move(t));
        names.push_back(path.filename().string());
    }
    std::printf("\nDTW distance (intonation %.2f):\n", intonation);
    for (size_t i = 0; i < takes.size(); ++i) {
        for (size_t j = 0; j < takes.size(); ++j) {
            const float d = voice::dtwDistance(takes[i], takes[j], intonation);
            if (d > 90) std::printf("    -");
            else std::printf("%5.2f", d);
        }
        std::printf("  %2zu\n", i);
    }
    std::printf("\nvoice similarity:\n");
    for (size_t i = 0; i < takes.size(); ++i) {
        for (size_t j = 0; j < takes.size(); ++j) std::printf("%5.2f", voice::cosine(takes[i].emb, takes[j].emb));
        std::printf("  %2zu\n", i);
    }
    return 0;
}
