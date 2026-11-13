// Voice commands, portable part. The user records every command with their own voice
// (a few takes each); a spoken phrase is then compared with those recordings directly:
//   * what was said and how: dynamic time warping over spectral shape, loudness contour
//     and pitch contour (intonation) - any language, any words, no dictionary;
//   * who said it: a speaker embedding compared with the embeddings of the same
//     command's recordings (same words, same voice - the strongest kind of check).
// 16 kHz mono throughout.
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

// Finds where a spoken phrase starts and ends in a live stream. Feed it blocks as they
// arrive; it keeps the audio of the current phrase only.
//   * the noise floor follows the quiet parts, speech is >= 9 dB above it (and above -52 dBFS);
//   * speech starts after 60 ms of it and ends after `hangMs` of quiet;
//   * 300 ms before the start are kept, so the first consonant is not cut off.
class Endpointer {
public:
    enum class State { Waiting, Speaking, Done };
    explicit Endpointer(int hangMs = 450, int maxSpeechMs = 5000) : hangMs_(hangMs), maxSpeechMs_(maxSpeechMs) {}
    State feed(const float* pcm, size_t n);
    State state() const { return state_; }
    // The phrase (pre-roll + speech + a short tail). Valid once speech has started.
    std::vector<float> utterance() const;
    // Ready for the next phrase; the learned noise floor is kept.
    void restart();
    float lastDb() const { return lastDb_; }
    int speechMs() const { return speechMs_; }

private:
    static constexpr size_t kFrame = kRate / 50;  // 20 ms
    int hangMs_, maxSpeechMs_;
    State state_ = State::Waiting;
    std::vector<float> buf_;
    size_t done_ = 0;              // samples of buf_ already analysed
    size_t start_ = 0, end_ = 0;   // phrase bounds in buf_
    float noiseDb_ = -60.f;
    bool noiseInit_ = false;
    int loud_ = 0, quietMs_ = 0, speechMs_ = 0;
    float lastDb_ = -120.f;
};

// Speech-only parts of a recording: the quiet gaps are cut out.
std::vector<float> speechOnly(const std::vector<float>& pcm);

// ------------------------------------------------------------------ recorded commands

// Per 20 ms frame: 12 cepstral coefficients (spectral shape = which sounds), the loudness
// contour, the pitch contour in octaves around the phrase's median (intonation) and a
// voiced / unvoiced flag. Normalised per phrase, so distance to the microphone, its
// colouring and how high the voice is in general do not matter.
constexpr int kCep = 12;
constexpr int kTplDim = kCep + 3;
std::vector<float> phraseFeatures(const std::vector<float>& pcm, int* frames);

// One take of a command.
struct VoiceTake {
    int frames = 0;
    std::vector<float> feat;   // frames * kTplDim
    std::vector<float> emb;    // speaker embedding of the take (L2-normalised)
};

// Dynamic time warping: average per-step distance along the best alignment (a 35 % band:
// the phrase may be said somewhat faster or slower, not twice as fast). `intonation`
// 0..1 is how much the pitch contour counts next to the sounds themselves.
float dtwDistance(const VoiceTake& a, const VoiceTake& b, float intonation);

struct VoiceModel {
    std::string id;                 // VoiceCommand::id
    std::vector<VoiceTake> takes;
    float threshold = 0.f;          // acceptance distance learned from how alike the takes are
    static constexpr size_t kMinTakes = 3, kMaxTakes = 8;
    bool ready() const { return takes.size() >= kMinTakes; }
};
// Sets m.threshold from the takes' own spread.
void learnThreshold(VoiceModel& m, float intonation);
// How far a new take is from the existing ones, relative to what is normal (1 = typical,
// about 2 and more = a different phrase or a slip of the tongue). 0 with no takes yet.
float takeOddness(const VoiceModel& m, const VoiceTake& t, float intonation);

struct VoiceStore {
    std::vector<VoiceModel> commands;
    VoiceModel* find(const std::string& id);
    const VoiceModel* find(const std::string& id) const;
    VoiceModel& get(const std::string& id);  // creates
    void remove(const std::string& id);
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& blob);
};

struct VoiceMatch {
    std::string id;        // accepted command ("" = none)
    std::string nearest;   // closest command, accepted or not
    float ratio = 99.f;    // nearest: distance / its allowed distance (< 1 = close enough)
    float margin = 0.f;    // runner-up ratio / best ratio (how unambiguous)
};
// Compares a phrase with every enabled, ready command. A command's distance is the mean
// of its 2 nearest takes. Accepted when within the command's threshold (scaled by its
// strictness) and clearly closer than the next command.
VoiceMatch matchPhrase(const VoiceStore& store, const std::vector<VoiceCommand>& commands, const VoiceTake& heard,
                       float intonation);

void l2normalize(std::vector<float>& v);
float cosine(const std::vector<float>& a, const std::vector<float>& b);
// How much `emb` sounds like the person who recorded the command: mean of the 2 best
// cosine similarities to the takes' embeddings. -1 when there is nothing to compare with.
float ownerScore(const VoiceModel& m, const std::vector<float>& emb);

}  // namespace vl::voice
