#pragma once
// Eine Listbox-Zeile anhand ihres Originalindex auswählen. Bei gefilterten
// oder unvollständigen Listen ist die Zeilennummer nicht der Datenindex.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace rune::editor {

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
