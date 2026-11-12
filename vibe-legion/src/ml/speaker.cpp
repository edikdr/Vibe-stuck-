#include "ml/speaker.hpp"

#include "core/voice.hpp"

namespace vl::ml {

bool SpeakerEmbedder::load(const std::filesystem::path& file, std::string* error) {
    // Two threads: a 3 s command is embedded in ~0.1 s, and only right after a gesture.
    InferenceConfig cfg;
    cfg.device = ComputeDevice::Cpu;
    cfg.threads = 2;
    return m_.open(file, cfg, error);
}

bool SpeakerEmbedder::embed(const std::vector<float>& pcm, std::vector<float>& out) {
    out.clear();
    if (!ok() || pcm.size() < kMinSamples) return false;
    int frames = 0;
    std::vector<float> feats = voice::fbank(pcm, &frames);
    if (frames < 20) return false;
    try {
        auto o = m_.run(feats.data(), {1, frames, voice::kMelBins});
        if (o.empty()) return false;
        const size_t n = o[0].GetTensorTypeAndShapeInfo().GetElementCount();
        const float* f = o[0].GetTensorData<float>();
        out.assign(f, f + n);
    } catch (const Ort::Exception&) {
        return false;
    }
    voice::l2normalize(out);
    return !out.empty();
}

}  // namespace vl::ml
