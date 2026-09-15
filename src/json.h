// PocketHarness - tiny self-contained JSON parser/serializer (no dependencies).
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "common.h"

namespace pocket {
namespace json {

struct Value;
using Array = std::vector<Value>;
using Object = std::map<std::string, Value>;  // map: deterministic key order

struct Value {
    using Storage =
        std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;
    Storage data = nullptr;

    Value() = default;
    Value(std::nullptr_t) : data(nullptr) {}
    Value(bool b) : data(b) {}
    Value(int i) : data((double)i) {}
    Value(long i) : data((double)i) {}
    Value(double d) : data(d) {}
    Value(const char* s) : data(std::string(s)) {}
    Value(std::string_view s) : data(std::string(s)) {}
    Value(std::string s) : data(std::move(s)) {}
    Value(Array a) : data(std::move(a)) {}
    Value(Object o) : data(std::move(o)) {}

    bool isNull() const { return std::holds_alternative<std::nullptr_t>(data); }
    bool isBool() const { return std::holds_alternative<bool>(data); }
    bool isNum() const { return std::holds_alternative<double>(data); }
    bool isStr() const { return std::holds_alternative<std::string>(data); }
    bool isArr() const { return std::holds_alternative<Array>(data); }
    bool isObj() const { return std::holds_alternative<Object>(data); }

    bool asBool(bool def = false) const;
    double asNum(double def = 0) const;
    long asInt(long def = 0) const;
    const std::string& asStr() const;
    const Array& asArr() const;
    const Object& asObj() const;
    Array& asArr();
    Object& asObj();

    // Object lookup. Returns null value when missing/wrong type.
    const Value& at(std::string_view key) const;
    // Array index. Returns null value when out of range/wrong type.
    const Value& at(size_t idx) const;
    bool has(std::string_view key) const;
    size_t size() const;
};

// Parse one complete JSON document. Trailing garbage is an error.
Result<Value> parse(std::string_view text);

// Serialize. pretty=true emits indented JSON.
std::string stringify(const Value& v, bool pretty = false);

// Helpers to build objects without ceremony.
inline Object obj() { return Object{}; }

}  // namespace json
}  // namespace pocket
