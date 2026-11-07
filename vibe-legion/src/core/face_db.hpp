// Enrolled people: name + a handful of 128-d SFace embeddings per modality.
// Only vectors are stored (never images); on Windows the blob is DPAPI-encrypted.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace vl {

using Embedding = std::array<float, 128>;

void l2normalize(Embedding& e);
float cosine(const Embedding& a, const Embedding& b);  // inputs must be normalised

struct Person {
    std::string id;
    std::string name;
    int64_t createdUnix = 0;
    std::vector<Embedding> rgb;  // colour camera samples
    std::vector<Embedding> ir;   // IR sensor samples (different modality)
};

struct FaceMatch {
    int person = -1;      // index into people, -1 = unknown
    float similarity = 0.f;
};

class FaceDatabase {
public:
    std::vector<Person> people;

    // Best person for an embedding. Uses samples of the same modality when the
    // person has any, otherwise falls back to the other modality.
    FaceMatch match(const Embedding& e, bool ir, float threshold) const;

    Person* find(const std::string& id);
    void remove(const std::string& id);

    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& blob);

    static constexpr size_t kMaxSamplesPerModality = 48;
};

}  // namespace vl
