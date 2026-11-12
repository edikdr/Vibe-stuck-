// Voice commands, portable part: audio features for the speaker model, speech
// end-pointing, the owner's voice print and phrase matching. 16 kHz mono throughout.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/settings.hpp"

namespace vl::voice {

constexpr int kRate = 16000;
constexpr int kMelBins = 80;

// Kaldi-style log-mel filterbank the speaker model was trained on: 25 ms Hamming window,
// 10 ms shift, pre-emphasis 0.97, 80 bins from 20 Hz to 8 kHz, samples scaled to the
// int16 range, then the mean of every bin over the utterance is removed.
// pcm: -1..1. Returns frames * kMelBins floats (row = frame); *frames = number of frames.
std::vector<float> fbank(const std::vector<float>& pcm, int* frames);

// Level of a block in dBFS (-120 for silence).
float levelDb(const float* pcm, size_t n);

// Finds where a spoken command starts and ends in a live stream. Feed it blocks as they
// arrive; it keeps the audio itself.
//   * the noise floor follows the quiet parts, speech is >= 9 dB above it (and above -52 dBFS);
//   * speech starts after 60 ms of it and ends after `hangMs` of quiet;
//   * 300 ms before the start are kept, so the first consonant is not cut off.
class Endpointer {
public:
    enum class State { Waiting, Speaking, Done };
    explicit Endpointer(int hangMs = 700, int maxSpeechMs = 6000) : hangMs_(hangMs), maxSpeechMs_(maxSpeechMs) {}
    State feed(const float* pcm, size_t n);
    State state() const { return state_; }
    // The utterance (pre-roll + speech + a short tail). Valid once speech has started.
    std::vector<float> utterance() const;
    float lastDb() const { return lastDb_; }
    int speechMs() const { return speechMs_; }
    int waitedMs() const { return static_cast<int>(total_ * 1000 / kRate); }

private:
    static constexpr size_t kFrame = kRate / 50;  // 20 ms
    int hangMs_, maxSpeechMs_;
    State state_ = State::Waiting;
    std::vector<float> buf_;       // everything fed so far
    size_t done_ = 0;              // samples already analysed
    size_t total_ = 0;
    size_t start_ = 0, end_ = 0;   // utterance bounds in buf_
    float noiseDb_ = -60.f;
    bool noiseInit_ = false;
    int loud_ = 0, quietMs_ = 0, speechMs_ = 0;
    float lastDb_ = -120.f;
};

// Speech-only parts of a recording (for enrolling the owner's voice): the quiet gaps
// between phrases are cut out.
std::vector<float> speechOnly(const std::vector<float>& pcm);

// The owner's voice: speaker-model embeddings (L2-normalised) of pieces of the enrollment.
struct VoicePrint {
    std::vector<std::vector<float>> samples;
    bool empty() const { return samples.empty(); }
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& blob);
    static constexpr size_t kMaxSamples = 64;
};
void l2normalize(std::vector<float>& v);
float cosine(const std::vector<float>& a, const std::vector<float>& b);
// How much `emb` sounds like the owner: mean of the 3 best cosine similarities (short
// commands vary, so one lucky or unlucky sample should not decide). -1 for an empty print.
float ownerScore(const VoicePrint& p, const std::vector<float>& emb);

// "Выключи  микрофон!" -> "выключи микрофон": lower case, ё -> е, letters / digits / single spaces.
std::string normalizePhrase(const std::string& utf8);
std::vector<std::string> splitWords(const std::string& normalized);
// Index of the enabled command whose phrase equals `heard` (both normalised); -1 if none.
int matchCommand(const std::vector<VoiceCommand>& commands, const std::string& heard);
// JSON array for the recogniser: every enabled phrase + "[unk]" (anything else).
std::string grammarJson(const std::vector<VoiceCommand>& commands);

}  // namespace vl::voice
