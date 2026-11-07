#include "core/json.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace vl::json {

namespace {
const std::string kEmptyString;
const Array kEmptyArray;
const Object kEmptyObject;
const Value kNull;
}  // namespace

Value::Type Value::type() const {
    switch (data_.index()) {
        case 0: return Type::Null;
        case 1: return Type::Bool;
        case 2: return Type::Number;
        case 3: return Type::String;
        case 4: return Type::Array;
        default: return Type::Object;
    }
}

bool Value::asBool(bool def) const {
    if (auto p = std::get_if<bool>(&data_)) return *p;
    if (auto n = std::get_if<double>(&data_)) return *n != 0.0;
    return def;
}

double Value::asNumber(double def) const {
    if (auto p = std::get_if<double>(&data_)) return std::isfinite(*p) ? *p : def;
    if (auto b = std::get_if<bool>(&data_)) return *b ? 1.0 : 0.0;
    return def;
}

const std::string& Value::asString() const {
    if (auto p = std::get_if<std::string>(&data_)) return *p;
    return kEmptyString;
}

std::string Value::asString(const std::string& def) const {
    if (auto p = std::get_if<std::string>(&data_)) return *p;
    return def;
}

const Array& Value::asArray() const {
    if (auto p = std::get_if<std::shared_ptr<Array>>(&data_)) return **p;
    return kEmptyArray;
}

const Object& Value::asObject() const {
    if (auto p = std::get_if<std::shared_ptr<Object>>(&data_)) return **p;
    return kEmptyObject;
}

Array& Value::makeArray() {
    auto p = std::get_if<std::shared_ptr<Array>>(&data_);
    if (!p) {
        data_ = std::make_shared<Array>();
        p = std::get_if<std::shared_ptr<Array>>(&data_);
    } else if (p->use_count() > 1) {
        *p = std::make_shared<Array>(**p);  // copy-on-write
    }
    return **p;
}

Object& Value::makeObject() {
    auto p = std::get_if<std::shared_ptr<Object>>(&data_);
    if (!p) {
        data_ = std::make_shared<Object>();
        p = std::get_if<std::shared_ptr<Object>>(&data_);
    } else if (p->use_count() > 1) {
        *p = std::make_shared<Object>(**p);
    }
    return **p;
}

const Value& Value::operator[](std::string_view key) const {
    for (const auto& [k, v] : asObject())
        if (k == key) return v;
    return kNull;
}

bool Value::has(std::string_view key) const {
    for (const auto& kv : asObject())
        if (kv.first == key) return true;
    return false;
}

void Value::set(std::string key, Value v) {
    auto& obj = makeObject();
    for (auto& kv : obj) {
        if (kv.first == key) {
            kv.second = std::move(v);
            return;
        }
    }
    obj.emplace_back(std::move(key), std::move(v));
}

namespace {

void escapeTo(std::string& out, const std::string& s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
}

void newline(std::string& out, int indent, int depth) {
    if (indent <= 0) return;
    out.push_back('\n');
    out.append(static_cast<size_t>(indent * depth), ' ');
}

}  // namespace

void Value::dumpTo(std::string& out, int indent, int depth) const {
    switch (type()) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += std::get<bool>(data_) ? "true" : "false"; break;
        case Type::Number: {
            double d = std::get<double>(data_);
            if (!std::isfinite(d)) d = 0;
            char buf[32];
            if (d == std::floor(d) && std::fabs(d) < 1e15)
                std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(d));
            else
                std::snprintf(buf, sizeof buf, "%.6g", d);
            out += buf;
            break;
        }
        case Type::String: escapeTo(out, std::get<std::string>(data_)); break;
        case Type::Array: {
            const auto& a = asArray();
            if (a.empty()) { out += "[]"; break; }
            // Short numeric arrays stay on one line (landmark templates, colours).
            bool flat = a.size() <= 64;
            for (const auto& v : a) flat = flat && (v.isNumber() || v.isBool());
            out.push_back('[');
            for (size_t i = 0; i < a.size(); ++i) {
                if (i) out.push_back(',');
                if (flat) { if (i && indent > 0) out.push_back(' '); }
                else newline(out, indent, depth + 1);
                a[i].dumpTo(out, indent, depth + 1);
            }
            if (!flat) newline(out, indent, depth);
            out.push_back(']');
            break;
        }
        case Type::Object: {
            const auto& o = asObject();
            if (o.empty()) { out += "{}"; break; }
            out.push_back('{');
            for (size_t i = 0; i < o.size(); ++i) {
                if (i) out.push_back(',');
                newline(out, indent, depth + 1);
                escapeTo(out, o[i].first);
                out += indent > 0 ? ": " : ":";
                o[i].second.dumpTo(out, indent, depth + 1);
            }
            newline(out, indent, depth);
            out.push_back('}');
            break;
        }
    }
}

std::string Value::dump(int indent) const {
    std::string out;
    dumpTo(out, indent, 0);
    return out;
}

// ---------------------------------------------------------------- parser

namespace {

struct Parser {
    std::string_view s;
    size_t i = 0;
    std::string err;
    int depth = 0;

    void ws() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
    }
    bool fail(const char* m) {
        if (err.empty()) err = std::string(m) + " at offset " + std::to_string(i);
        return false;
    }
    bool lit(std::string_view w) {
        if (s.substr(i, w.size()) == w) { i += w.size(); return true; }
        return false;
    }

    static void utf8(std::string& out, uint32_t cp) {
        if (cp < 0x80) out.push_back(static_cast<char>(cp));
        else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool hex4(uint32_t& v) {
        if (i + 4 > s.size()) return fail("bad \\u escape");
        v = 0;
        for (int k = 0; k < 4; ++k) {
            char c = s[i++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else return fail("bad hex digit");
        }
        return true;
    }

    bool str(std::string& out) {
        if (i >= s.size() || s[i] != '"') return fail("expected string");
        ++i;
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return true;
            if (c != '\\') { out.push_back(c); continue; }
            if (i >= s.size()) break;
            char e = s[i++];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    uint32_t cp = 0;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF && lit("\\u")) {
                        uint32_t lo = 0;
                        if (!hex4(lo)) return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    utf8(out, cp);
                    break;
                }
                default: return fail("bad escape");
            }
        }
        return fail("unterminated string");
    }

    bool value(Value& out) {
        if (++depth > 64) return fail("nesting too deep");
        ws();
        if (i >= s.size()) return fail("unexpected end");
        char c = s[i];
        bool ok = true;
        if (c == '{') {
            ++i;
            Object obj;
            ws();
            if (i < s.size() && s[i] == '}') ++i;
            else {
                while (true) {
                    ws();
                    std::string key;
                    if (!str(key)) return false;
                    ws();
                    if (i >= s.size() || s[i] != ':') return fail("expected ':'");
                    ++i;
                    Value v;
                    if (!value(v)) return false;
                    obj.emplace_back(std::move(key), std::move(v));
                    ws();
                    if (i < s.size() && s[i] == ',') { ++i; continue; }
                    if (i < s.size() && s[i] == '}') { ++i; break; }
                    return fail("expected ',' or '}'");
                }
            }
            out = Value(std::move(obj));
        } else if (c == '[') {
            ++i;
            Array arr;
            ws();
            if (i < s.size() && s[i] == ']') ++i;
            else {
                while (true) {
                    Value v;
                    if (!value(v)) return false;
                    arr.push_back(std::move(v));
                    ws();
                    if (i < s.size() && s[i] == ',') { ++i; continue; }
                    if (i < s.size() && s[i] == ']') { ++i; break; }
                    return fail("expected ',' or ']'");
                }
            }
            out = Value(std::move(arr));
        } else if (c == '"') {
            std::string v;
            if (!str(v)) return false;
            out = Value(std::move(v));
        } else if (lit("true")) out = Value(true);
        else if (lit("false")) out = Value(false);
        else if (lit("null")) out = Value();
        else if (c == '-' || (c >= '0' && c <= '9')) {
            size_t start = i;
            ++i;
            while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.' || s[i] == 'e' ||
                                    s[i] == 'E' || s[i] == '+' || s[i] == '-'))
                ++i;
            std::string num(s.substr(start, i - start));
            char* end = nullptr;
            double d = std::strtod(num.c_str(), &end);
            if (!end || *end) return fail("bad number");
            out = Value(d);
        } else {
            ok = fail("unexpected character");
        }
        --depth;
        return ok;
    }
};

}  // namespace

ParseResult parse(std::string_view text) {
    ParseResult r;
    // Skip UTF-8 BOM written by Notepad.
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF)
        text.remove_prefix(3);
    Parser p;
    p.s = text;
    if (p.value(r.value)) {
        p.ws();
        if (p.i != text.size()) p.fail("trailing characters");
    }
    r.ok = p.err.empty();
    r.error = p.err;
    if (!r.ok) r.value = Value();
    return r;
}

}  // namespace vl::json
