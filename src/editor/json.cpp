// ============================================================================
//  Minimaler JSON-Parser/-Schreiber für den RuneEditor (Etappe 17).
// ============================================================================

#include "editor/json.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace rune::editor {

namespace {

struct Parser {
    const char* start = nullptr;
    const char* p = nullptr;
    const char* end = nullptr;
    size_t line = 1;

    [[noreturn]] void fail(const char* what) {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "JSON-Fehler bei Zeichen %td: %s",
                      p ? p - start : 0, what);
        throw std::runtime_error(buf);
    }

    void skip_ws() {
        while (p < end) {
            const char c = *p;
            if (c == '\n') { ++line; ++p; }
            else if (c == ' ' || c == '\t' || c == '\r') { ++p; }
            else break;
        }
    }

    bool eat(char c) {
        if (p < end && *p == c) { ++p; return true; }
        return false;
    }

    void expect(char c) {
        if (!eat(c)) fail(std::string(std::string("erwartet '") + c + "'").c_str());
    }

    // UTF-16 Codepoint -> UTF-8 in out schieben.
    static void push_utf8(unsigned cp, std::string& out) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
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

    unsigned parse_hex4() {
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            if (p >= end) fail("\\u abgebrochen");
            const char c = *p++;
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
            else fail("ungültiges \\u");
        }
        return v;
    }

    std::string parse_string() {
        expect('"');
        std::string out;
        while (true) {
            if (p >= end) fail("String nicht abgeschlossen");
            const char c = *p++;
            if (c == '"') break;
            if (c != '\\') { out.push_back(c); continue; }
            if (p >= end) fail("Escape am Ende");
            const char e = *p++;
            switch (e) {
                case '"':  out.push_back('"');  break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/');  break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'u': {
                    unsigned cp = parse_hex4();
                    // Surrogatpaar (Event-Texte mit Sonderzeichen).
                    if (cp >= 0xD800 && cp <= 0xDBFF && p + 1 < end &&
                        p[0] == '\\' && p[1] == 'u') {
                        p += 2;
                        const unsigned lo = parse_hex4();
                        if (lo >= 0xDC00 && lo <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        }
                    }
                    push_utf8(cp, out);
                    break;
                }
                default: fail("unbekanntes Escape");
            }
        }
        return out;
    }

    JsonValue parse_value() {
        skip_ws();
        if (p >= end) fail("unerwartetes Ende");

        if (eat('{')) {
            JsonValue v; v.type = JsonValue::Type::Obj;
            skip_ws();
            if (eat('}')) return v;
            while (true) {
                skip_ws();
                std::string key = parse_string();
                skip_ws();
                expect(':');
                JsonValue val = parse_value();
                v.obj.emplace_back(std::move(key), std::move(val));
                skip_ws();
                if (eat(',')) continue;
                expect('}');
                break;
            }
            return v;
        }

        if (eat('[')) {
            JsonValue v; v.type = JsonValue::Type::Arr;
            skip_ws();
            if (eat(']')) return v;
            while (true) {
                v.arr.push_back(parse_value());
                skip_ws();
                if (eat(',')) continue;
                expect(']');
                break;
            }
            return v;
        }

        if (eat('"')) {
            --p; // parse_string erwartet das Anführungszeichen noch.
            JsonValue v; v.type = JsonValue::Type::Str;
            v.str = parse_string();
            return v;
        }

        if (eat('t')) { if (p + 2 < end && p[0] == 'r' && p[1] == 'u' && p[2] == 'e') { p += 3; return JsonValue::make_bool(true); } fail("bad true"); }
        if (eat('f')) { if (p + 3 < end && p[0] == 'a' && p[1] == 'l' && p[2] == 's' && p[3] == 'e') { p += 4; return JsonValue::make_bool(false); } fail("bad false"); }
        if (eat('n')) { if (p + 2 < end && p[0] == 'u' && p[1] == 'l' && p[2] == 'l') { p += 3; return JsonValue{}; } fail("bad null"); }

        // JSON-Zahlengrammatik strikt prüfen. Sonst würde der gespeicherte
        // Ganzzahl-Token z. B. '+1' oder '01' als ungültiges JSON ausgeben.
        {
            const char* first = p;
            eat('-');
            if (p >= end) fail("Zahl ohne Ziffern");
            if (eat('0')) {
                if (p < end && std::isdigit(static_cast<unsigned char>(*p)))
                    fail("führende Null in Zahl");
            } else if (*p >= '1' && *p <= '9') {
                while (p < end && std::isdigit(static_cast<unsigned char>(*p))) ++p;
            } else {
                fail("ungültige Zahl");
            }
            bool is_int = true;
            if (eat('.')) {
                is_int = false;
                if (p >= end || !std::isdigit(static_cast<unsigned char>(*p)))
                    fail("Nachkommaziffer fehlt");
                while (p < end && std::isdigit(static_cast<unsigned char>(*p))) ++p;
            }
            if (p < end && (*p == 'e' || *p == 'E')) {
                is_int = false;
                ++p;
                if (p < end && (*p == '+' || *p == '-')) ++p;
                if (p >= end || !std::isdigit(static_cast<unsigned char>(*p)))
                    fail("Exponentenziffer fehlt");
                while (p < end && std::isdigit(static_cast<unsigned char>(*p))) ++p;
            }
            const std::string token(first, p);
            if (is_int) return JsonValue::make_integer(token);
            return JsonValue::make_num(std::strtod(token.c_str(), nullptr), false);
        }
    }
};

void write_string(const std::string& s, std::string& out) {
    out.push_back('"');
    for (const unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c)); // UTF-8 durchreichen
                }
        }
    }
    out.push_back('"');
}

// Zahl schreiben.
//  - Ursprüngliche Ganzzahlen einschließlich sehr grosser Zahlen werden aus
//    ihrem Originaltoken geschrieben, nicht aus der double-Näherung.
//  - Neu als Ganzzahl erzeugte double-Werte werden ohne begrenzenden oder
//    undefinierten long-long-Cast in Festkomma geschrieben.
//  - Kommazahlen in der KÜRZESTEN Form, die den Wert exakt wiederherstellt
//    (1..17 signifikante Stellen, Vergleich per strtod). Damit schreibt der
//    Editor dieselben Bytes wie Ruby (JSON.pretty_generate). Der alte feste
//    Wert %.12g hat still Zahlen beschnitten: aus dem Startblickwinkel
//    3.141592653589793 wurde 3.14159265359, aus Spielständen wie
//    2.0040341041749343e-05 wurden 2.00403410417e-05 — jede gespeicherte Karte
//    unterschied sich danach vom Ruby-Ergebnis (Diff-Flut + Werteverlust).
//  - NaN/Unendlich sind kein gültiges JSON: sie werden zu null, damit die
//    Datei lesbar bleibt und der Karten-Scan (MapCheck/Probleme-Dialog) den
//    kaputten Wert meldet statt ihn zu verschlucken.
void write_number(const JsonValue& v, std::string& out) {
    if (v.is_int && !v.integer_text().empty()) {
        out += v.integer_text();
        return;
    }
    const double number = v.number();
    if (!std::isfinite(number)) {
        out += "null";
        return;
    }
    if (v.is_int) {
        if (number == 0.0) { out += "0"; return; }
        // DBL_MAX braucht höchstens 309 Stellen, plus Vorzeichen und NUL.
        char buf[std::numeric_limits<double>::max_exponent10 + 32];
        std::snprintf(buf, sizeof(buf), "%.0f", std::trunc(number));
        out += buf;
        return;
    }

    if (number == 0.0) {
        out += std::signbit(number) ? "-0.0" : "0.0";
        return;
    }

    // 1) Kürzeste Präzision suchen, die den Wert exakt wiederherstellt.
    char buf[64];
    int prec = 17;
    for (int p = 1; p <= 17; ++p) {
        std::snprintf(buf, sizeof(buf), "%.*g", p, number);
        if (std::strtod(buf, nullptr) == number) { prec = p; break; }
    }
    std::string text(buf);

    // 2) %g wechselt schon bei wenigen Stellen in die Exponentenschreibweise
    //    (240.0 -> "2.4e+02"), Ruby und Python bleiben bis Exponent 16 bei
    //    Festkomma (240.0). Für Karten- und Systemdaten ist Festkomma das,
    //    was beide Seiten schreiben — also bei Exponent [-4, 16) umformatieren,
    //    aber nur, wenn der Wert dabei exakt bleibt.
    const int exp10 = static_cast<int>(std::floor(std::log10(std::fabs(number))));
    if (exp10 >= -4 && exp10 < 16) {
        char fest[64];
        const int decimals = prec - 1 - exp10;
        std::snprintf(fest, sizeof(fest), "%.*f", decimals > 0 ? decimals : 0, number);
        if (std::strtod(fest, nullptr) == number) text = fest;
    }

    // Kommazahlen optisch als solche kennzeichnen (wie Ruby: 0.0 statt 0).
    if (text.find_first_of(".e") == std::string::npos) text += ".0";
    out += text;
}

void write_value(const JsonValue& v, int indent, std::string& out) {
    const std::string pad(static_cast<size_t>(indent) * 2, ' ');
    const std::string pad_in(static_cast<size_t>(indent + 1) * 2, ' ');

    switch (v.type) {
        case JsonValue::Type::Null: out += "null"; break;
        case JsonValue::Type::Bool: out += v.b ? "true" : "false"; break;
        case JsonValue::Type::Num:  write_number(v, out); break;
        case JsonValue::Type::Str:  write_string(v.str, out); break;

        case JsonValue::Type::Arr: {
            if (v.arr.empty()) { out += "[]"; break; }
            // Wie Ruby JSON.pretty_generate: jedes Element auf eigener Zeile,
            // damit Spiel und Editor sich beim Speichern nicht gegenseitig
            // das Format umwerfen (keine Diff-Flut in der Versionsverwaltung).
            out += "[\n";
            for (size_t i = 0; i < v.arr.size(); ++i) {
                if (i) out += ",\n";
                out += pad_in;
                write_value(v.arr[i], indent + 1, out);
            }
            out += '\n';
            out += pad;
            out.push_back(']');
            break;
        }

        case JsonValue::Type::Obj: {
            if (v.obj.empty()) { out += "{}"; break; }
            out += "{\n";
            for (size_t i = 0; i < v.obj.size(); ++i) {
                if (i) out += ",\n";
                out += pad_in;
                write_string(v.obj[i].first, out);
                out += ": ";
                write_value(v.obj[i].second, indent + 1, out);
            }
            out += '\n';
            out += pad;
            out.push_back('}');
            break;
        }
    }
}

} // namespace

JsonValue JsonValue::make_integer(std::string decimal) {
    const size_t first = !decimal.empty() && decimal[0] == '-' ? 1 : 0;
    if (first == decimal.size() || (decimal[first] == '0' && first + 1 != decimal.size()))
        throw std::runtime_error("ungültiger JSON-Ganzzahl-Token");
    for (size_t i = first; i < decimal.size(); ++i) {
        if (decimal[i] < '0' || decimal[i] > '9')
            throw std::runtime_error("ungültiger JSON-Ganzzahl-Token");
    }
    // Ruby JSON.parse('-0') liefert die Ganzzahl 0, nicht negative Null.
    if (decimal == "-0") decimal = "0";
    JsonValue value = make_num(std::strtod(decimal.c_str(), nullptr), true);
    value.integer_text_ = std::move(decimal);
    return value;
}

void JsonValue::increment() {
    if (!is_num()) return;
    if (!is_int || integer_text_.empty()) {
        *this = make_num(num_ + 1.0, is_int);
        return;
    }
    std::string text = integer_text_;
    if (text[0] != '-') {
        size_t i = text.size();
        while (i > 0 && text[i - 1] == '9') text[--i] = '0';
        if (i == 0) text.insert(text.begin(), '1');
        else ++text[i - 1];
    } else {
        // -N + 1 = -(N-1), danach ggf. eine führende Null entfernen.
        size_t i = text.size();
        while (i > 1 && text[i - 1] == '0') text[--i] = '9';
        --text[i - 1];
        if (text[1] == '0' && text.size() > 2) text.erase(1, 1);
        if (text == "-0") text = "0";
    }
    *this = make_integer(std::move(text));
}

JsonValue json_parse(const std::string& text) {
    Parser ps;
    ps.start = text.c_str();
    ps.p = ps.start;
    ps.end = ps.p + text.size();
    JsonValue v = ps.parse_value();
    ps.skip_ws();
    if (ps.p != ps.end) ps.fail("nachfolgende Zeichen");
    return v;
}

std::string json_write(const JsonValue& v) {
    std::string out;
    write_value(v, 0, out);
    out.push_back('\n');
    return out;
}

} // namespace rune::editor
