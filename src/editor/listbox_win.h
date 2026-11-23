#pragma once
// Eine Listbox-Zeile anhand ihres Originalindex auswählen. Bei gefilterten
// oder unvollständigen Listen ist die Zeilennummer nicht der Datenindex.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "core/win_path.h"

#include <string>

namespace rune::editor {

// Nur für Listboxen mit Strings. LB_GETTEXT kennt keine Puffergröße; den
// Unicode-Text deshalb anhand von LB_GETTEXTLEN vollständig holen.
inline bool listbox_text_utf8(HWND list, int row, std::string& out) {
    out.clear();
    if (!list || !IsWindow(list) || row < 0) return false;
    const LRESULT length = SendMessageW(list, LB_GETTEXTLEN, static_cast<WPARAM>(row), 0);
    if (length == LB_ERR || length < 0 || length > 32760) return false;
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    if (SendMessageW(list, LB_GETTEXT, static_cast<WPARAM>(row),
                     reinterpret_cast<LPARAM>(text.data())) != length) return false;
    if (length == 0) return true;
    text.resize(static_cast<std::size_t>(length));
    return core::win_path::to_utf8(text, out);
}

// Negative Werte bedeuten keine Auswahl; insbesondere ist -1 nie eine
// auswählbare Auslassungs-/Fehlerzeile. Nicht gefundene Werte werden gelöscht.
inline bool listbox_select_data(HWND list, LPARAM data) {
    if (!list || !IsWindow(list)) return false;
    const LRESULT count = SendMessageW(list, LB_GETCOUNT, 0, 0);
    if (count == LB_ERR) return false;
    LRESULT row = LB_ERR;
    if (data >= 0) {
        for (LRESULT i = 0; i < count; ++i) {
            if (SendMessageW(list, LB_GETITEMDATA, static_cast<WPARAM>(i), 0) == data) {
                row = i;
                break;
            }
        }
    }
    SendMessageW(list, LB_SETCURSEL, static_cast<WPARAM>(row), 0);
    return data < 0 || row != LB_ERR;
}

} // namespace rune::editor
#endif
