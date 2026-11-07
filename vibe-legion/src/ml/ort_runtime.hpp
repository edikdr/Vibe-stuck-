// ONNX Runtime plumbing: one Env, session options tuned for a background app
// (no spinning thread pools, sequential execution, small thread counts), model
// loading from memory with optional in-place patching, EP selection with fallback.
#pragma once

#include <onnxruntime_cxx_api.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "core/settings.hpp"

namespace vl::ml {

struct InferenceConfig {
    ComputeDevice device = ComputeDevice::Cpu;
    int threads = 1;
};

class OrtRuntime {
public:
    static OrtRuntime& instance();
    Ort::Env& env() { return env_; }

    // Creates a session from model bytes. `deviceUsed` receives a human label of
    // the execution provider that actually got attached (fallbacks included).
    std::unique_ptr<Ort::Session> createSession(const std::vector<uint8_t>& model, const InferenceConfig& cfg,
                                                std::string* deviceUsed, std::string* error);

    static bool readFile(const std::filesystem::path& p, std::vector<uint8_t>& out);

private:
    OrtRuntime();
    Ort::Env env_;
};

// Patches a fixed NCHW 1x3x640x640 input declaration (YuNet) to 1x3xHxW in place.
// Works because 640, H and W are all 2-byte protobuf varints (128..16383), so no
// length prefixes change. Returns false if the pattern was not found.
bool patchYuNetInputSize(std::vector<uint8_t>& model, int width, int height);

// Thin holder for a session with resolved I/O names.
struct ModelSession {
    std::unique_ptr<Ort::Session> session;
    std::vector<std::string> inputNames, outputNames;
    std::vector<std::vector<int64_t>> inputShapes, outputShapes;
    std::string device;
    double lastMs = 0.0;

    bool ok() const { return session != nullptr; }
    bool open(const std::filesystem::path& file, const InferenceConfig& cfg, std::string* error,
              std::vector<uint8_t>* patchedBytes = nullptr);
    int outputIndex(const std::string& name) const;
    std::vector<Ort::Value> run(float* input, const std::vector<int64_t>& shape);
};

}  // namespace vl::ml
