#include "core/face_db.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace vl {

void l2normalize(Embedding& e) {
    double s = 0;
    for (float v : e) s += static_cast<double>(v) * v;
    const float n = s > 1e-12 ? static_cast<float>(1.0 / std::sqrt(s)) : 0.f;
    for (float& v : e) v *= n;
}

float cosine(const Embedding& a, const Embedding& b) {
    float s = 0.f;
    for (size_t i = 0; i < a.size(); ++i) s += a[i] * b[i];
    return s;
}

FaceMatch FaceDatabase::match(const Embedding& e, bool ir, float threshold) const {
    FaceMatch best;
    for (size_t p = 0; p < people.size(); ++p) {
        const auto& primary = ir ? people[p].ir : people[p].rgb;
        const auto& fallback = ir ? people[p].rgb : people[p].ir;
        const auto& samples = primary.empty() ? fallback : primary;
        // Cross-modality matching is weaker: demand a little more.
        const float penalty = primary.empty() ? 0.06f : 0.f;
        float s = -1.f;
        for (const auto& smp : samples) s = std::max(s, cosine(e, smp));
        s -= penalty;
        if (s > best.similarity) {
            best.similarity = s;
            best.person = static_cast<int>(p);
        }
    }
    if (best.similarity < threshold) best.person = -1;
    return best;
}

Person* FaceDatabase::find(const std::string& id) {
    for (auto& p : people)
        if (p.id == id) return &p;
    return nullptr;
}

void FaceDatabase::remove(const std::string& id) {
    people.erase(std::remove_if(people.begin(), people.end(), [&](const Person& p) { return p.id == id; }),
                 people.end());
}

namespace {

constexpr uint32_t kMagic = 0x464C4756;  // "VGLF"
constexpr uint32_t kVersion = 1;

struct Writer {
    std::vector<uint8_t> b;
    void u32(uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i))); }
    void i64(int64_t v) { for (int i = 0; i < 8; ++i) b.push_back(static_cast<uint8_t>(static_cast<uint64_t>(v) >> (8 * i))); }
    void str(const std::string& s) { u32(static_cast<uint32_t>(s.size())); b.insert(b.end(), s.begin(), s.end()); }
    void emb(const Embedding& e) {
        const size_t o = b.size();
        b.resize(o + sizeof(float) * e.size());
        std::memcpy(b.data() + o, e.data(), sizeof(float) * e.size());
    }
};

struct Reader {
    const std::vector<uint8_t>& b;
    size_t i = 0;
    bool ok = true;
    bool need(size_t n) { if (i + n > b.size()) ok = false; return ok; }
    uint32_t u32() {
        if (!need(4)) return 0;
        uint32_t v = 0;
        for (int k = 0; k < 4; ++k) v |= static_cast<uint32_t>(b[i++]) << (8 * k);
        return v;
    }
    int64_t i64() {
        if (!need(8)) return 0;
        uint64_t v = 0;
        for (int k = 0; k < 8; ++k) v |= static_cast<uint64_t>(b[i++]) << (8 * k);
        return static_cast<int64_t>(v);
    }
    std::string str() {
        const uint32_t n = u32();
        if (n > 4096 || !need(n)) { ok = false; return {}; }
        std::string s(reinterpret_cast<const char*>(b.data() + i), n);
        i += n;
        return s;
    }
    bool emb(Embedding& e) {
        if (!need(sizeof(float) * e.size())) return false;
        std::memcpy(e.data(), b.data() + i, sizeof(float) * e.size());
        i += sizeof(float) * e.size();
        return true;
    }
};

}  // namespace

std::vector<uint8_t> FaceDatabase::serialize() const {
    Writer w;
    w.u32(kMagic);
    w.u32(kVersion);
    w.u32(static_cast<uint32_t>(people.size()));
    for (const auto& p : people) {
        w.str(p.id);
        w.str(p.name);
        w.i64(p.createdUnix);
        w.u32(static_cast<uint32_t>(p.rgb.size()));
        for (const auto& e : p.rgb) w.emb(e);
        w.u32(static_cast<uint32_t>(p.ir.size()));
        for (const auto& e : p.ir) w.emb(e);
    }
    return w.b;
}

bool FaceDatabase::deserialize(const std::vector<uint8_t>& blob) {
    Reader r{blob};
    if (r.u32() != kMagic || r.u32() != kVersion) return false;
    const uint32_t n = r.u32();
    if (n > 1000) return false;
    std::vector<Person> out;
    for (uint32_t k = 0; k < n && r.ok; ++k) {
        Person p;
        p.id = r.str();
        p.name = r.str();
        p.createdUnix = r.i64();
        for (auto* list : {&p.rgb, &p.ir}) {
            const uint32_t m = r.u32();
            if (m > 1000) return false;
            for (uint32_t j = 0; j < m && r.ok; ++j) {
                Embedding e;
                if (r.emb(e)) list->push_back(e);
            }
        }
        out.push_back(std::move(p));
    }
    if (!r.ok) return false;
    people = std::move(out);
    return true;
}

}  // namespace vl
