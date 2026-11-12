// Speaker embedding model (WeSpeaker ResNet34, ONNX): turns a piece of speech into a
// vector that is close for the same voice and far for different voices.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "ml/ort_runtime.hpp"

namespace vl::ml {

class SpeakerEmbedder {
public:
    bool load(const std::filesystem::path& file, std::string* error);
    void unload() { m_ = ModelSession(); }
    bool ok() const { return m_.ok(); }
    // pcm: 16 kHz mono, -1..1, at least ~0.4 s. out: L2-normalised embedding.
    bool embed(const std::vector<float>& pcm, std::vector<float>& out);
    double lastMs() const { return m_.lastMs; }

    static constexpr size_t kMinSamples = 16000 * 4 / 10;

private:
    ModelSession m_;
};

}  // namespace vl::ml
