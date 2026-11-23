#pragma once
// ============================================================================
//  Editor-Prüfung für Zufallskämpfe (Etappe 19l).
//  Ohne Win32-Abhängigkeit, damit dieselbe Prüfung im C++-Logiktest läuft.
//  Regeln und Schweregrade entsprechen MapCheck.pruefe_encounters in Ruby.
// ============================================================================

#include "editor/json.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace rune::editor {

template <typename Report>
void issue_scan_encounters(const JsonValue* enc, const std::string& wo, Report report) {
    // Fehlendes oder null-Feld ist optional, wie bei MapCheck.
    if (!enc || enc->is_null()) return;
    if (!enc->is_obj()) {
        report(2, wo, "ist kein Objekt");
        return;
    }

    if (const JsonValue* tr = enc->find("troops"); tr && !tr->is_null()) {
        if (!tr->is_arr()) {
            report(2, wo, "'troops' ist keine Liste");
        } else {
            for (int i = 0; i < static_cast<int>(tr->arr.size()); ++i) {
                const JsonValue& t = tr->arr[i];
                const bool leer = t.is_str() && std::all_of(
                    t.str.begin(), t.str.end(),
                    [](unsigned char c) { return c == '\0' || std::isspace(c); });
                if (!t.is_str() || leer) {
                    report(2, wo + " troops #" + std::to_string(i + 1),
                           "ist keine Truppen-ID (Text)");
                }
            }
        }
    }

    if (const JsonValue* st = enc->find("steps"); st && !st->is_null()) {
        const double n = st->number(0.0);
        if (!st->is_num() || !std::isfinite(n) || n < 1.0 || n != std::floor(n)) {
            report(1, wo, "'steps' muss eine ganze Zahl > 0 sein");
        }
    }
}

} // namespace rune::editor
