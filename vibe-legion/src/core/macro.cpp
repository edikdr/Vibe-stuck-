#include "core/macro.hpp"

#include <algorithm>
#include <cstdlib>

namespace vl {

namespace {

// Decodes one UTF-8 code point starting at s[i]; advances i. Invalid bytes pass through.
uint32_t nextCp(const std::string& s, size_t& i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    auto cont = [&](size_t k) { return i + k < s.size() && (static_cast<unsigned char>(s[i + k]) & 0xC0) == 0x80; };
    auto b = [&](size_t k) { return static_cast<uint32_t>(static_cast<unsigned char>(s[i + k]) & 0x3Fu); };
    uint32_t cp = c;
    size_t len = 1;
    if ((c & 0xE0) == 0xC0 && cont(1)) {
        cp = ((c & 0x1Fu) << 6) | b(1);
        len = 2;
    } else if ((c & 0xF0) == 0xE0 && cont(1) && cont(2)) {
        cp = ((c & 0x0Fu) << 12) | (b(1) << 6) | b(2);
        len = 3;
    } else if ((c & 0xF8) == 0xF0 && cont(1) && cont(2) && cont(3)) {
        cp = ((c & 0x07u) << 18) | (b(1) << 12) | (b(2) << 6) | b(3);
        len = 4;
    }
    i += len;
    return cp;
}

void putCp(std::string& o, uint32_t cp) {
    if (cp < 0x80) {
        o.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        o.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        o.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        o.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        o.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        o.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        o.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        o.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        o.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        o.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

struct KeyName { const char* name; uint16_t vk; };
const KeyName kKeys[] = {
    {"ctrl", 0x11}, {"control", 0x11}, {"shift", 0x10}, {"alt", 0x12}, {"win", 0x5B}, {"windows", 0x5B},
    {"enter", 0x0D}, {"return", 0x0D}, {"esc", 0x1B}, {"escape", 0x1B}, {"space", 0x20}, {"пробел", 0x20},
    {"tab", 0x09}, {"backspace", 0x08}, {"del", 0x2E}, {"delete", 0x2E}, {"ins", 0x2D}, {"insert", 0x2D},
    {"home", 0x24}, {"end", 0x23}, {"pgup", 0x21}, {"pageup", 0x21}, {"pgdn", 0x22}, {"pagedown", 0x22},
    {"left", 0x25}, {"up", 0x26}, {"right", 0x27}, {"down", 0x28},
    {"влево", 0x25}, {"вверх", 0x26}, {"вправо", 0x27}, {"вниз", 0x28},
    {"printscreen", 0x2C}, {"prtsc", 0x2C}, {"pause", 0x13}, {"capslock", 0x14}, {"menu", 0x5D},
    {"plus", 0xBB}, {"minus", 0xBD}, {"comma", 0xBC}, {"period", 0xBE}, {"slash", 0xBF}, {"backslash", 0xDC},
    {"semicolon", 0xBA}, {"quote", 0xDE}, {"tilde", 0xC0}, {"lbracket", 0xDB}, {"rbracket", 0xDD},
    {"playpause", 0xB3}, {"nexttrack", 0xB0}, {"prevtrack", 0xB1}, {"stop", 0xB2},
    {"volup", 0xAF}, {"voldown", 0xAE}, {"mute", 0xAD},
    {"-", 0xBD}, {"=", 0xBB}, {",", 0xBC}, {".", 0xBE}, {"/", 0xBF}, {"[", 0xDB}, {"]", 0xDD},
};

bool isModifier(uint16_t vk) { return vk == 0x11 || vk == 0x10 || vk == 0x12 || vk == 0x5B; }

bool keyFromName(const std::string& n, uint16_t& vk) {
    if (n.size() == 1) {
        const char c = n[0];
        if (c >= 'a' && c <= 'z') { vk = static_cast<uint16_t>(c - 'a' + 'A'); return true; }
        if (c >= '0' && c <= '9') { vk = static_cast<uint16_t>(c); return true; }
    }
    if (n.size() >= 2 && n.size() <= 3 && n[0] == 'f') {  // F1..F24
        const int k = std::atoi(n.c_str() + 1);
        if (k >= 1 && k <= 24 && std::to_string(k) == n.substr(1)) { vk = static_cast<uint16_t>(0x70 + k - 1); return true; }
    }
    if (n.size() == 4 && n.compare(0, 3, "num") == 0 && n[3] >= '0' && n[3] <= '9') {
        vk = static_cast<uint16_t>(0x60 + (n[3] - '0'));
        return true;
    }
    for (const auto& k : kKeys)
        if (n == k.name) { vk = k.vk; return true; }
    return false;
}

// "wait 300" -> word "wait", rest "300".
void splitWord(const std::string& s, std::string& word, std::string& rest) {
    const size_t sp = s.find_first_of(" \t");
    word = lowerUtf8(s.substr(0, sp));
    rest = sp == std::string::npos ? std::string() : trimCopy(s.substr(sp + 1));
}

bool builtinAllowed(ActionType t) {
    switch (t) {
        case ActionType::None: case ActionType::KeyCombo: case ActionType::LaunchApp: case ActionType::Macro:
        case ActionType::RunScript: case ActionType::VoiceListen: case ActionType::PauseRecognition:
        case ActionType::kCount:
            return false;
        default:
            return true;
    }
}

}  // namespace

std::string lowerUtf8(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        uint32_t cp = nextCp(s, i);
        if (cp >= 'A' && cp <= 'Z') cp += 32;
        else if (cp >= 0x410 && cp <= 0x42F) cp += 0x20;  // А..Я
        else if (cp == 0x401) cp = 0x451;                  // Ё
        putCp(o, cp);
    }
    return o;
}

std::string trimCopy(const std::string& s) {
    size_t a = 0, b = s.size();
    auto ws = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (a < b && ws(s[a])) ++a;
    while (b > a && ws(s[b - 1])) --b;
    return s.substr(a, b - a);
}

bool parseKeyCombo(const std::string& text, std::vector<uint16_t>& keys) {
    keys.clear();
    std::vector<uint16_t> mods, rest;
    const std::string s = lowerUtf8(text);
    size_t pos = 0;
    while (pos <= s.size()) {
        size_t plus = s.find('+', pos);
        if (plus == std::string::npos) plus = s.size();
        const std::string name = trimCopy(s.substr(pos, plus - pos));
        uint16_t vk = 0;
        if (name.empty() || !keyFromName(name, vk)) return false;
        (isModifier(vk) ? mods : rest).push_back(vk);
        pos = plus + 1;
    }
    if (mods.size() + rest.size() == 0 || mods.size() + rest.size() > 6) return false;
    keys = mods;
    keys.insert(keys.end(), rest.begin(), rest.end());
    return true;
}

bool parseMacro(const std::string& script, std::vector<MacroStep>& out, std::string* errorRu) {
    out.clear();
    auto fail = [&](size_t n, const std::string& why) {
        if (errorRu) *errorRu = "шаг " + std::to_string(n) + ": " + why;
        out.clear();
        return false;
    };
    size_t pos = 0, n = 0;
    while (pos <= script.size()) {
        size_t end = script.find_first_of(";\n", pos);
        if (end == std::string::npos) end = script.size();
        const std::string raw = trimCopy(script.substr(pos, end - pos));
        pos = end + 1;
        if (raw.empty()) continue;
        ++n;
        if (n > kMaxMacroSteps) return fail(n, "слишком много шагов (можно до " + std::to_string(kMaxMacroSteps) + ")");
        std::string word, rest;
        splitWord(raw, word, rest);
        MacroStep st;
        if (word == "wait" || word == "sleep" || word == "пауза" || word == "ждать") {
            char* e = nullptr;
            const long ms = std::strtol(rest.c_str(), &e, 10);
            if (rest.empty() || (e && *e) || ms < 0 || ms > 10000)
                return fail(n, "после «пауза» нужно число миллисекунд от 0 до 10000");
            st.kind = MacroStep::Kind::Wait;
            st.ms = static_cast<int>(ms);
        } else if (word == "text" || word == "type" || word == "текст") {
            if (rest.empty()) return fail(n, "после «текст» нужен сам текст");
            st.kind = MacroStep::Kind::Text;
            st.text = rest;
        } else if (word == "run" || word == "запуск" || word == "запустить") {
            if (rest.empty()) return fail(n, "после «запуск» нужен путь к программе или скрипту");
            st.kind = MacroStep::Kind::Run;
            st.text = rest;
        } else if (rest.empty() && raw.find('_') != std::string::npos && actionFromId(word) != ActionType::None) {
            if (!builtinAllowed(actionFromId(word))) return fail(n, "действие «" + word + "» в макросе не поддерживается");
            st.kind = MacroStep::Kind::Builtin;
            st.action = actionFromId(word);
        } else if (parseKeyCombo(raw, st.keys)) {
            st.kind = MacroStep::Kind::Keys;
        } else {
            return fail(n, "не понял «" + raw + "»: это не клавиши, не «пауза», «текст», «запуск» и не действие");
        }
        out.push_back(std::move(st));
    }
    if (out.empty()) {
        if (errorRu) *errorRu = "макрос пуст";
        return false;
    }
    return true;
}

bool appListMatches(const std::string& list, const std::string& exe) {
    auto stem = [](const std::string& in) {
        std::string s = lowerUtf8(trimCopy(in));
        if (s.size() > 4 && s.compare(s.size() - 4, 4, ".exe") == 0) s.resize(s.size() - 4);
        return s;
    };
    const std::string want = stem(exe);
    bool any = false;
    size_t pos = 0;
    while (pos <= list.size()) {
        size_t end = list.find_first_of(",;\n", pos);
        if (end == std::string::npos) end = list.size();
        const std::string item = stem(list.substr(pos, end - pos));
        pos = end + 1;
        if (item.empty()) continue;
        any = true;
        if (!want.empty() && item == want) return true;
    }
    return !any;
}

}  // namespace vl
