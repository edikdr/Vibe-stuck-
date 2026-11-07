// Minimal JSON value + parser + pretty printer.
// Keeps object key order (settings files stay readable and diff-friendly).
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace vl::json {

class Value;
using Array = std::vector<Value>;
using Object = std::vector<std::pair<std::string, Value>>;

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool b) : data_(b) {}
    Value(int v) : data_(static_cast<double>(v)) {}
    Value(int64_t v) : data_(static_cast<double>(v)) {}
    Value(uint32_t v) : data_(static_cast<double>(v)) {}
    Value(float v) : data_(static_cast<double>(v)) {}
    Value(double v) : data_(v) {}
    Value(const char* s) : data_(std::string(s)) {}
    Value(std::string s) : data_(std::move(s)) {}
    Value(Array a) : data_(std::make_shared<Array>(std::move(a))) {}
    Value(Object o) : data_(std::make_shared<Object>(std::move(o))) {}

    Type type() const;
    bool isNull() const { return type() == Type::Null; }
    bool isObject() const { return type() == Type::Object; }
    bool isArray() const { return type() == Type::Array; }
    bool isNumber() const { return type() == Type::Number; }
    bool isString() const { return type() == Type::String; }
    bool isBool() const { return type() == Type::Bool; }

    // Lenient accessors: wrong type -> fallback.
    bool asBool(bool def = false) const;
    double asNumber(double def = 0.0) const;
    float asFloat(float def = 0.f) const { return static_cast<float>(asNumber(def)); }
    int asInt(int def = 0) const { return static_cast<int>(asNumber(def)); }
    const std::string& asString() const;
    std::string asString(const std::string& def) const;

    const Array& asArray() const;
    const Object& asObject() const;
    Array& makeArray();
    Object& makeObject();

    // Object helpers.
    const Value& operator[](std::string_view key) const;  // Null if missing
    bool has(std::string_view key) const;
    void set(std::string key, Value v);  // object only (converts Null to Object)

    std::string dump(int indent = 2) const;

private:
    void dumpTo(std::string& out, int indent, int depth) const;
    std::variant<std::monostate, bool, double, std::string, std::shared_ptr<Array>, std::shared_ptr<Object>> data_;
};

struct ParseResult {
    Value value;
    bool ok = false;
    std::string error;
};

ParseResult parse(std::string_view text);

}  // namespace vl::json
