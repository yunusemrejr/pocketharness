// PocketHarness - JSON parser/serializer implementation.
#include "json.h"

#include <cctype>
#include <cmath>
#include <cstdio>

namespace pocket {
namespace json {

namespace {
const Value kNull;
}

bool Value::asBool(bool def) const {
    if (isBool()) return std::get<bool>(data);
    return def;
}

double Value::asNum(double def) const {
    if (isNum()) return std::get<double>(data);
    return def;
}

long Value::asInt(long def) const {
    if (isNum()) return (long)std::get<double>(data);
    return def;
}

const std::string& Value::asStr() const {
    static const std::string kEmpty;
    if (isStr()) return std::get<std::string>(data);
    return kEmpty;
}

const Array& Value::asArr() const {
    static const Array kEmpty;
    if (isArr()) return std::get<Array>(data);
    return kEmpty;
}

const Object& Value::asObj() const {
    static const Object kEmpty;
    if (isObj()) return std::get<Object>(data);
    return kEmpty;
}

Array& Value::asArr() { return std::get<Array>(data); }
Object& Value::asObj() { return std::get<Object>(data); }

const Value& Value::at(std::string_view key) const {
    if (!isObj()) return kNull;
    auto it = std::get<Object>(data).find(std::string(key));
    if (it == std::get<Object>(data).end()) return kNull;
    return it->second;
}

const Value& Value::at(size_t idx) const {
    if (!isArr()) return kNull;
    const Array& a = std::get<Array>(data);
    if (idx >= a.size()) return kNull;
    return a[idx];
}

bool Value::has(std::string_view key) const {
    if (!isObj()) return false;
    return std::get<Object>(data).count(std::string(key)) > 0;
}

size_t Value::size() const {
    if (isArr()) return std::get<Array>(data).size();
    if (isObj()) return std::get<Object>(data).size();
    if (isStr()) return std::get<std::string>(data).size();
    return 0;
}

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------
struct Parser {
    std::string_view s;
    size_t pos = 0;
    std::string error;
    int depth = 0;

    bool eof() const { return pos >= s.size(); }
    char peek() const { return eof() ? '\0' : s[pos]; }

    void skipWs() {
        while (!eof()) {
            char c = s[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos;
            else break;
        }
    }

    bool expect(char c, const char* what) {
        if (peek() != c) {
            error = std::string("expected ") + what + " at offset " + std::to_string(pos);
            return false;
        }
        ++pos;
        return true;
    }

    bool consumeLit(const char* lit) {
        size_t i = 0;
        while (lit[i] && pos + i < s.size() && s[pos + i] == lit[i]) ++i;
        if (lit[i] != '\0') return false;
        pos += i;
        return true;
    }

    static void appendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out.push_back((char)cp);
        } else if (cp < 0x800) {
            out.push_back((char)(0xC0 | (cp >> 6)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back((char)(0xE0 | (cp >> 12)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        } else {
            out.push_back((char)(0xF0 | (cp >> 18)));
            out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        }
    }

    bool parseString(std::string& out) {
        if (!expect('"', "'\"'")) return false;
        out.clear();
        while (!eof()) {
            char c = s[pos++];
            if (c == '"') return true;
            if (c == '\\') {
                if (eof()) break;
                char e = s[pos++];
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
                        if (pos + 4 > s.size()) {
                            error = "bad \\u escape at offset " + std::to_string(pos);
                            return false;
                        }
                        unsigned cp = 0;
                        for (int k = 0; k < 4; ++k) {
                            char h = s[pos++];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= (unsigned)(h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
                            else {
                                error = "bad \\u escape at offset " + std::to_string(pos);
                                return false;
                            }
                        }
                        // Surrogate pairs.
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            if (pos + 6 <= s.size() && s[pos] == '\\' && s[pos + 1] == 'u') {
                                pos += 2;
                                unsigned lo = 0;
                                for (int k = 0; k < 4; ++k) {
                                    char h = s[pos++];
                                    lo <<= 4;
                                    if (h >= '0' && h <= '9') lo |= (unsigned)(h - '0');
                                    else if (h >= 'a' && h <= 'f') lo |= (unsigned)(h - 'a' + 10);
                                    else if (h >= 'A' && h <= 'F') lo |= (unsigned)(h - 'A' + 10);
                                    else {
                                        error = "bad low surrogate at offset " +
                                                std::to_string(pos);
                                        return false;
                                    }
                                }
                                if (lo >= 0xDC00 && lo <= 0xDFFF)
                                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                                else {
                                    error = "bad low surrogate at offset " + std::to_string(pos);
                                    return false;
                                }
                            } else {
                                error = "lone high surrogate at offset " + std::to_string(pos);
                                return false;
                            }
                        }
                        appendUtf8(out, cp);
                        break;
                    }
                    default:
                        error = std::string("bad escape \\") + e + " at offset " +
                                std::to_string(pos);
                        return false;
                }
            } else if ((unsigned char)c < 0x20) {
                error = "unescaped control char at offset " + std::to_string(pos);
                return false;
            } else {
                out.push_back(c);
            }
        }
        error = "unterminated string";
        return false;
    }

    bool parseNumber(double& out) {
        size_t start = pos;
        if (peek() == '-') ++pos;
        if (peek() == '0') {
            ++pos;
        } else if (peek() >= '1' && peek() <= '9') {
            while (peek() >= '0' && peek() <= '9') ++pos;
        } else {
            error = "bad number at offset " + std::to_string(pos);
            return false;
        }
        if (peek() == '.') {
            ++pos;
            if (!(peek() >= '0' && peek() <= '9')) {
                error = "bad number at offset " + std::to_string(pos);
                return false;
            }
            while (peek() >= '0' && peek() <= '9') ++pos;
        }
        if (peek() == 'e' || peek() == 'E') {
            ++pos;
            if (peek() == '+' || peek() == '-') ++pos;
            if (!(peek() >= '0' && peek() <= '9')) {
                error = "bad number at offset " + std::to_string(pos);
                return false;
            }
            while (peek() >= '0' && peek() <= '9') ++pos;
        }
        std::string tok(s.substr(start, pos - start));
        try {
            out = std::stod(tok);
        } catch (...) {
            error = "bad number at offset " + std::to_string(start);
            return false;
        }
        return true;
    }

    bool parseValue(Value& out) {
        if (++depth > 200) {
            error = "nesting too deep";
            return false;
        }
        skipWs();
        bool r = true;
        char c = peek();
        if (c == '{') {
            ++pos;
            Object o;
            skipWs();
            if (peek() == '}') {
                ++pos;
                out = Value(std::move(o));
            } else {
                for (;;) {
                    skipWs();
                    std::string key;
                    if (!parseString(key)) {
                        r = false;
                        break;
                    }
                    skipWs();
                    if (!expect(':', "':'")) {
                        r = false;
                        break;
                    }
                    Value v;
                    if (!parseValue(v)) {
                        r = false;
                        break;
                    }
                    o[std::move(key)] = std::move(v);
                    skipWs();
                    if (peek() == ',') {
                        ++pos;
                        continue;
                    }
                    if (!expect('}', "'}'")) {
                        r = false;
                        break;
                    }
                    break;
                }
                if (r) out = Value(std::move(o));
            }
        } else if (c == '[') {
            ++pos;
            Array a;
            skipWs();
            if (peek() == ']') {
                ++pos;
                out = Value(std::move(a));
            } else {
                for (;;) {
                    Value v;
                    if (!parseValue(v)) {
                        r = false;
                        break;
                    }
                    a.push_back(std::move(v));
                    skipWs();
                    if (peek() == ',') {
                        ++pos;
                        continue;
                    }
                    if (!expect(']', "']'")) {
                        r = false;
                        break;
                    }
                    break;
                }
                if (r) out = Value(std::move(a));
            }
        } else if (c == '"') {
            std::string str;
            r = parseString(str);
            if (r) out = Value(std::move(str));
        } else if (c == 't') {
            r = consumeLit("true");
            if (!r) error = "bad literal at offset " + std::to_string(pos);
            else out = Value(true);
        } else if (c == 'f') {
            r = consumeLit("false");
            if (!r) error = "bad literal at offset " + std::to_string(pos);
            else out = Value(false);
        } else if (c == 'n') {
            r = consumeLit("null");
            if (!r) error = "bad literal at offset " + std::to_string(pos);
            else out = Value(nullptr);
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            double d = 0;
            r = parseNumber(d);
            if (r) out = Value(d);
        } else if (eof()) {
            error = "unexpected end of input";
            r = false;
        } else {
            error = std::string("unexpected char '") + c + "' at offset " +
                    std::to_string(pos);
            r = false;
        }
        --depth;
        return r;
    }
};

Result<Value> parse(std::string_view text) {
    Parser p;
    p.s = text;
    Value v;
    if (!p.parseValue(v)) return Result<Value>::Err(p.error);
    p.skipWs();
    if (!p.eof())
        return Result<Value>::Err("trailing characters at offset " + std::to_string(p.pos));
    return Result<Value>::Ok(std::move(v));
}

// ---------------------------------------------------------------------------
// Serializer
// ---------------------------------------------------------------------------
void writeEscaped(std::string& out, std::string_view s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back((char)c);
                }
        }
    }
    out.push_back('"');
}

void writeValue(std::string& out, const Value& v, int indent, int cur) {
    if (v.isNull()) {
        out += "null";
    } else if (v.isBool()) {
        out += std::get<bool>(v.data) ? "true" : "false";
    } else if (v.isNum()) {
        double d = std::get<double>(v.data);
        if (std::isfinite(d) && d == (long long)d && d < 1e15 && d > -1e15) {
            out += std::to_string((long long)d);
        } else if (std::isfinite(d)) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.17g", d);
            out += buf;
        } else {
            out += "null";
        }
    } else if (v.isStr()) {
        writeEscaped(out, std::get<std::string>(v.data));
    } else if (v.isArr()) {
        const Array& a = std::get<Array>(v.data);
        out.push_back('[');
        for (size_t i = 0; i < a.size(); ++i) {
            if (i) out.push_back(',');
            if (indent >= 0) {
                out.push_back('\n');
                out.append((size_t)(cur + 1) * (size_t)indent, ' ');
            }
            writeValue(out, a[i], indent, cur + 1);
        }
        if (indent >= 0 && !a.empty()) {
            out.push_back('\n');
            out.append((size_t)cur * (size_t)indent, ' ');
        }
        out.push_back(']');
    } else {
        const Object& o = std::get<Object>(v.data);
        out.push_back('{');
        bool first = true;
        for (const auto& kv : o) {
            if (!first) out.push_back(',');
            first = false;
            if (indent >= 0) {
                out.push_back('\n');
                out.append((size_t)(cur + 1) * (size_t)indent, ' ');
            }
            writeEscaped(out, kv.first);
            out.push_back(':');
            if (indent >= 0) out.push_back(' ');
            writeValue(out, kv.second, indent, cur + 1);
        }
        if (indent >= 0 && !o.empty()) {
            out.push_back('\n');
            out.append((size_t)cur * (size_t)indent, ' ');
        }
        out.push_back('}');
    }
}

std::string stringify(const Value& v, bool pretty) {
    std::string out;
    writeValue(out, v, pretty ? 2 : -1, 0);
    return out;
}

}  // namespace json
}  // namespace pocket
