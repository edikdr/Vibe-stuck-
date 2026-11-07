#include "ml/ort_runtime.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>

#ifdef VL_WITH_DIRECTML
#include <dml_provider_factory.h>
#endif

namespace vl::ml {

OrtRuntime& OrtRuntime::instance() {
    static OrtRuntime rt;
    return rt;
}

OrtRuntime::OrtRuntime() : env_(ORT_LOGGING_LEVEL_ERROR, "VibeLegion") {}

bool OrtRuntime::readFile(const std::filesystem::path& p, std::vector<uint8_t>& out) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const auto size = f.tellg();
    if (size <= 0 || size > (512ll << 20)) return false;
    out.resize(static_cast<size_t>(size));
    f.seekg(0);
    return static_cast<bool>(f.read(reinterpret_cast<char*>(out.data()), size));
}

std::unique_ptr<Ort::Session> OrtRuntime::createSession(const std::vector<uint8_t>& model, const InferenceConfig& cfg,
                                                        std::string* deviceUsed, std::string* error) {
    auto makeOptions = [&](bool cpuOnly, std::string& label) {
        Ort::SessionOptions so;
        so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        so.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
        so.SetIntraOpNumThreads(std::max(1, cfg.threads));
        so.SetInterOpNumThreads(1);
        // The single most important knob for a background app: ORT's pool threads
        // busy-wait between runs by default, which shows up as constant CPU load.
        so.AddConfigEntry("session.intra_op.allow_spinning", "0");
        so.AddConfigEntry("session.inter_op.allow_spinning", "0");
        // Our tensors are tiny and shapes fixed: the BFC arena would only hoard
        // memory (tens of MB). Plain malloc keeps the resident set small.
        so.DisableCpuMemArena();
        label = "CPU";
        if (cpuOnly) return so;
        if (cfg.device == ComputeDevice::Gpu) {
#ifdef VL_WITH_DIRECTML
            so.DisableMemPattern();  // required by DirectML
            Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(so, 0));
            label = "GPU (DirectML)";
#else
            label = "CPU (сборка без DirectML)";
#endif
        } else if (cfg.device == ComputeDevice::Npu) {
            std::unordered_map<std::string, std::string> opts;
            if (const char* c = std::getenv("VL_VITISAI_CONFIG")) opts["config_file"] = c;
            so.AppendExecutionProvider("VitisAI", opts);  // throws if this ORT build lacks it
            label = "NPU (VitisAI)";
        }
        return so;
    };

    for (int attempt = 0; attempt < 2; ++attempt) {
        std::string label;
        try {
            Ort::SessionOptions so = makeOptions(attempt == 1 || cfg.device == ComputeDevice::Cpu, label);
            auto s = std::make_unique<Ort::Session>(env_, model.data(), model.size(), so);
            if (deviceUsed) *deviceUsed = attempt == 1 ? "CPU (резерв)" : label;
            return s;
        } catch (const Ort::Exception& e) {
            if (error) *error = e.what();
            if (cfg.device == ComputeDevice::Cpu) break;  // nothing to fall back to
        }
    }
    return nullptr;
}

bool patchYuNetInputSize(std::vector<uint8_t>& m, int width, int height) {
    if (width < 128 || height < 128 || width > 16383 || height > 16383) return false;
    // TensorShapeProto dims: {1, 3, 640, 640} as nested Dimension messages.
    static const uint8_t pat[] = {0x0A, 2, 8, 1, 0x0A, 2, 8, 3, 0x0A, 3, 8, 0x80, 0x05, 0x0A, 3, 8, 0x80, 0x05};
    for (size_t i = 0; i + sizeof(pat) <= m.size(); ++i) {
        if (std::memcmp(m.data() + i, pat, sizeof(pat)) != 0) continue;
        m[i + 11] = static_cast<uint8_t>(0x80 | (height & 0x7F));
        m[i + 12] = static_cast<uint8_t>(height >> 7);
        m[i + 16] = static_cast<uint8_t>(0x80 | (width & 0x7F));
        m[i + 17] = static_cast<uint8_t>(width >> 7);
        return true;
    }
    return false;
}

bool ModelSession::open(const std::filesystem::path& file, const InferenceConfig& cfg, std::string* error,
                        std::vector<uint8_t>* patched) {
    session.reset();
    inputNames.clear(); outputNames.clear(); inputShapes.clear(); outputShapes.clear();
    std::vector<uint8_t> bytes;
    if (patched) bytes = std::move(*patched);
    else if (!OrtRuntime::readFile(file, bytes)) {
        if (error) *error = "не найден файл модели: " + file.filename().string();
        return false;
    }
    session = OrtRuntime::instance().createSession(bytes, cfg, &device, error);
    if (!session) return false;
    Ort::AllocatorWithDefaultOptions alloc;
    for (size_t i = 0; i < session->GetInputCount(); ++i) {
        inputNames.emplace_back(session->GetInputNameAllocated(i, alloc).get());
        inputShapes.push_back(session->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
    }
    for (size_t i = 0; i < session->GetOutputCount(); ++i) {
        outputNames.emplace_back(session->GetOutputNameAllocated(i, alloc).get());
        outputShapes.push_back(session->GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape());
    }
    return !inputNames.empty() && !outputNames.empty();
}

int ModelSession::outputIndex(const std::string& name) const {
    for (size_t i = 0; i < outputNames.size(); ++i)
        if (outputNames[i] == name) return static_cast<int>(i);
    return -1;
}

std::vector<Ort::Value> ModelSession::run(float* input, const std::vector<int64_t>& shape) {
    static const Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    size_t count = 1;
    for (auto d : shape) count *= static_cast<size_t>(d);
    Ort::Value in = Ort::Value::CreateTensor<float>(mem, input, count, shape.data(), shape.size());
    std::vector<const char*> outs;
    for (auto& n : outputNames) outs.push_back(n.c_str());
    const char* inName = inputNames[0].c_str();
    const auto t0 = std::chrono::steady_clock::now();
    auto r = session->Run(Ort::RunOptions{nullptr}, &inName, &in, 1, outs.data(), outs.size());
    lastMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    return r;
}

}  // namespace vl::ml
