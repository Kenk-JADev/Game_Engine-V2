#pragma once
// RGSS-/Ruby-Projektscripts. Innerhalb des Editor-Namensraums eingebunden.
// Das Spiel führt nur explizit markierte Startscripts automatisch aus; Code
// wird beim Syntaxprüfen nie ausgeführt. Die Projektdatei ist keine Sandbox.

enum {
    IDSC_LIST = 6001, IDSC_NEW = 6002, IDSC_DEL = 6003,
    IDSC_NAME = 6010, IDSC_CODE = 6011, IDSC_APPLY = 6012,
    IDSC_SAVE = 6013, IDSC_RELOAD = 6014, IDSC_CLOSE = 6015,
    IDSC_STATUS = 6016, IDSC_AUTOLOAD = 6017, IDSC_CHECK = 6018,
    IDSC_GUTTER = 6019, IDSC_CURSOR = 6020, IDSC_LIST_LABEL = 6021,
    IDSC_NAME_LABEL = 6022, IDSC_CODE_LABEL = 6023,
    IDSC_FIND = 6024, IDSC_FIND_NEXT = 6025, IDSC_FIND_LABEL = 6026,
    IDSC_REPLACE = 6027, IDSC_REPLACE_ONE = 6028, IDSC_REPLACE_ALL = 6029,
    IDSC_REPLACE_LABEL = 6030   // Etappe 36: Suchen/Ersetzen wie XP
};
constexpr UINT WM_SCRIPT_SELECT = WM_APP + 54;
constexpr UINT WM_SCRIPT_CURSOR = WM_APP + 55;
constexpr std::size_t SC_TEXT_LIMIT = 8u * 1024u * 1024u;

struct ScriptDlgState {
    HWND win = nullptr, list = nullptr, name = nullptr, code = nullptr, gutter = nullptr;
    HWND status = nullptr, cursor = nullptr, autoload = nullptr;
    HFONT ui_font = nullptr, code_font = nullptr;
    unsigned dpi = 96;
    std::string project, baseline;
    std::uint64_t project_epoch = 0;
    JsonValue root;
    ScriptForm before;
    int sel = -1, last_gutter_width = 0;
    bool loaded = false, baseline_present = false, dirty = false, touched = false;
    bool syncing = false, ready = false, select_queued = false, select_delayed = false;
};
static ScriptDlgState g_sc;

struct ScriptSyncGuard {
    bool previous;
    ScriptSyncGuard() : previous(g_sc.syncing) { g_sc.syncing = true; }
    ~ScriptSyncGuard() { g_sc.syncing = previous; }
};

void sc_layout();
void sc_cursor_update();
bool sc_apply();
bool sc_save();
bool sc_reconcile_selection();

void sc_status(const std::string& text) {
    if (g_sc.status) ev_set_text(g_sc.status, text);
}

void sc_title() {
    if (!g_sc.win) return;
    std::wstring title = L"RGSS Script-Editor — " + utf8_to_wide(g_sc.project);
    if (g_sc.dirty || g_sc.touched) title += L" *";
    SetWindowTextW(g_sc.win, title.c_str());
}

JsonValue* sc_list(bool create = false) {
    return g_sc.loaded ? script_document_list(g_sc.root, create) : nullptr;
}

JsonValue* sc_item() {
    JsonValue* list = sc_list();
    if (!list || g_sc.sel < 0 || static_cast<size_t>(g_sc.sel) >= list->arr.size()) return nullptr;
    return &list->arr[static_cast<size_t>(g_sc.sel)];
}

bool sc_project_matches() {
    return g_sc.project_epoch == g.project_epoch && g_sc.project == g.project_name && !g_sc.project.empty();
}

// Erst erfolgreich und vollständig einlesen, dann austauschen. Fehlerhafte
// Dateien und ein vorhandener Entwurf bleiben bei fehlgeschlagenem Reload da.
bool sc_load_file() {
    if (!sc_project_matches()) return false;
    DocumentStore store;
    DocumentSnapshot snapshot;
    assets::Result result = store.open(g.root, g_sc.project, false, "scripts.json");
    if (result == assets::Result::Ok) result = store.observe(snapshot);
    if (result != assets::Result::Ok) {
        document_result_notice(g_sc.win, L"Scripts nicht geladen", result, {}, g.project_dir);
        return false;
    }
    JsonValue root;
    if (!snapshot.present) root = JsonValue::make_obj();
    else if (script_document_parse(snapshot.bytes, root) != ScriptDocumentStatus::Ok) {
        data_prompt(g_sc.win, L"scripts.json ist beschädigt, mehrdeutig oder kein gültiges Scriptdokument. "
                              L"Die Datei und ein vorhandener Entwurf bleiben unverändert. Bitte extern prüfen.",
                    L"Scripts nicht geladen", MB_OK | MB_ICONERROR);
        return false;
    }
    g_sc.root = std::move(root);
    g_sc.baseline = std::move(snapshot.bytes);
    g_sc.baseline_present = snapshot.present;
    g_sc.loaded = true;
    g_sc.dirty = false;
    g_sc.touched = false;
    const JsonValue* list = sc_list();
    g_sc.sel = list && !list->arr.empty() ? 0 : -1;
    return true;
}

void sc_refresh_list() {
    if (!g_sc.list) return;
    ScriptSyncGuard guard;
    SendMessageW(g_sc.list, LB_RESETCONTENT, 0, 0);
    const JsonValue* list = sc_list();
    if (list) for (size_t i = 0; i < list->arr.size(); ++i) {
        const JsonValue& item = list->arr[i];
        const JsonValue* name = item.is_obj() ? item.find("name") : nullptr;
        std::string label = name && name->is_str() && !name->str.empty()
            ? name->str : "[ungültiger oder namenloser Eintrag]";
        const JsonValue* flag = item.is_obj() ? item.find("autoload") : nullptr;
        if (flag && flag->type == JsonValue::Type::Bool && flag->b) label = "▶ " + label;
        // Etappe 36: XP zeigt die Scriptliste fortlaufend nummeriert.
        label = script_index_label(i) + " " + label;
        const std::wstring wide = utf8_to_wide(label);
        const LRESULT row = SendMessageW(g_sc.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str()));
        if (row != LB_ERR && row != LB_ERRSPACE)
            SendMessageW(g_sc.list, LB_SETITEMDATA, static_cast<WPARAM>(row), static_cast<LPARAM>(i));
    }
    if (!list || g_sc.sel < 0 || static_cast<size_t>(g_sc.sel) >= list->arr.size()) g_sc.sel = -1;
    listbox_select_data(g_sc.list, static_cast<LPARAM>(g_sc.sel));
    EnableWindow(GetDlgItem(g_sc.win, IDSC_NEW), g_sc.loaded);
    EnableWindow(GetDlgItem(g_sc.win, IDSC_SAVE), g_sc.loaded);
    EnableWindow(GetDlgItem(g_sc.win, IDSC_DEL), g_sc.sel >= 0);
}

// Win32-Multiline erwartet CRLF. Ein alleinstehendes CR oder NUL ist nicht
// zuverlässig editierbar und bleibt daher unverändert geschützt.
bool sc_display_text(HWND control, const std::string& original, bool editable, bool multiline) {
    std::string displayed;
    if (editable && original.size() <= SC_TEXT_LIMIT && original.find('\0') == std::string::npos &&
        editor_valid_utf8(original)) {
        displayed.reserve(original.size());
        for (size_t i = 0; i < original.size(); ++i) {
            if (original[i] == '\r' && (i + 1 == original.size() || original[i + 1] != '\n')) {
                editable = false;
                break;
            }
            if (original[i] == '\n' && (i == 0 || original[i - 1] != '\r')) displayed.push_back('\r');
            displayed.push_back(original[i]);
        }
    } else editable = false;
    if (!multiline && !editor_single_line(original)) editable = false;
    std::string roundtrip;
    if (editable) {
        ev_set_text(control, displayed);
        editable = control_text_strict(control, roundtrip) &&
            (multiline ? script_normalize_lines(roundtrip) == script_normalize_lines(original) : roundtrip == original);
    }
    if (!editable) SetWindowTextW(control, L"[nicht vollständig darstellbar oder mehrdeutig — geschützt]");
    EnableWindow(control, editable);
    return editable;
}

void sc_refresh_fields() {
    if (!g_sc.name || !g_sc.code) return;
    ScriptSyncGuard guard;
    const JsonValue* item = sc_item();
    const bool valid = item && item->is_obj();
    g_sc.before = valid ? script_form_read(*item) : ScriptForm{};
    if (valid) {
        g_sc.before.name_editable = sc_display_text(g_sc.name, g_sc.before.name, g_sc.before.name_editable, false);
        g_sc.before.code_editable = sc_display_text(g_sc.code, g_sc.before.code, g_sc.before.code_editable, true);
    } else {
        SetWindowTextW(g_sc.name, item ? L"[ungültiger Eintrag — geschützt]" : L"");
        SetWindowTextW(g_sc.code, L"");
        EnableWindow(g_sc.name, FALSE);
        EnableWindow(g_sc.code, FALSE);
    }
    SendMessageW(g_sc.autoload, BM_SETCHECK, g_sc.before.autoload ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(g_sc.autoload, valid && g_sc.before.autoload_editable);
    EnableWindow(GetDlgItem(g_sc.win, IDSC_APPLY), valid);
    EnableWindow(GetDlgItem(g_sc.win, IDSC_CHECK), valid && g_sc.before.code_editable);
    g_sc.touched = false;
    sc_title();
    sc_layout();
    sc_cursor_update();
}

bool sc_read_form(ScriptForm& result) {
    result = g_sc.before;
    if (result.name_editable && !control_text_strict(g_sc.name, result.name)) return false;
    if (result.code_editable && !control_text_strict(g_sc.code, result.code)) return false;
    if (result.autoload_editable)
        result.autoload = SendMessageW(g_sc.autoload, BM_GETCHECK, 0, 0) == BST_CHECKED;
    return true;
}

bool sc_has_pending() {
    if (!sc_item() || !sc_item()->is_obj()) return false;
    ScriptForm now;
    if (!sc_read_form(now)) return true;
    return now.name != g_sc.before.name ||
        script_normalize_lines(now.code) != script_normalize_lines(g_sc.before.code) ||
        now.autoload != g_sc.before.autoload;
}

bool sc_apply() {
    JsonValue* item = sc_item();
    if (!item || !item->is_obj()) { g_sc.touched = false; sc_title(); return true; }
    ScriptForm input;
    if (!sc_read_form(input)) {
        data_prompt(g_sc.win, L"Das Eingabefeld enthält ungültiges UTF-16 und wird nicht übernommen.",
                    L"Script nicht übernommen", MB_OK | MB_ICONWARNING);
        return false;
    }
    ScriptEdit edit = script_form_prepare(*item, g_sc.before, input);
    if (!edit.ok) {
        data_prompt(g_sc.win, utf8_to_wide(edit.error).c_str(), L"Script nicht übernommen", MB_OK | MB_ICONWARNING);
        return false;
    }
    if ((input.name != g_sc.before.name || (input.autoload && !g_sc.before.autoload)) &&
        script_name_used(g_sc.root, input.name, static_cast<size_t>(g_sc.sel))) {
        data_prompt(g_sc.win, L"Dieser Scriptname ist bereits belegt. Bitte einen eindeutigen Namen wählen.",
                    L"Script nicht übernommen", MB_OK | MB_ICONWARNING);
        SetFocus(g_sc.name);
        return false;
    }
    if (edit.changed) {
        const bool name_editable = g_sc.before.name_editable, code_editable = g_sc.before.code_editable;
        const bool auto_editable = g_sc.before.autoload_editable;
        *item = std::move(edit.value);
        g_sc.before = script_form_read(*item);
        g_sc.before.name_editable = name_editable;
        g_sc.before.code_editable = code_editable;
        g_sc.before.autoload_editable = auto_editable;
        g_sc.dirty = true;
        sc_refresh_list();
        sc_status("Änderungen übernommen — noch nicht gespeichert");
    }
    g_sc.touched = false;
    sc_title();
    return true;
}

bool sc_reconcile_selection() {
    g_sc.select_queued = false;
    g_sc.select_delayed = false;
    if (!g_sc.list || !g_sc.loaded) return true;
    const LRESULT row = SendMessageW(g_sc.list, LB_GETCURSEL, 0, 0);
    const LRESULT data = row == LB_ERR ? LB_ERR : SendMessageW(g_sc.list, LB_GETITEMDATA, static_cast<WPARAM>(row), 0);
    const int next = data == LB_ERR ? -1 : static_cast<int>(data);
    if (next == g_sc.sel) return true;
    if (!sc_apply()) { listbox_select_data(g_sc.list, static_cast<LPARAM>(g_sc.sel)); return false; }
    const JsonValue* list = sc_list();
    if (next >= 0 && (!list || static_cast<size_t>(next) >= list->arr.size())) return false;
    g_sc.sel = next;
    sc_refresh_list();
    sc_refresh_fields();
    return true;
}

bool sc_flush_selection() {
    return !(g_sc.select_queued || g_sc.select_delayed) || sc_reconcile_selection();
}

void sc_queue_selection() {
    if (g_sc.select_queued || !g_sc.win) return;
    g_sc.select_queued = PostMessageW(g_sc.win, WM_SCRIPT_SELECT, 0, 0) != 0;
    if (!g_sc.select_queued) listbox_select_data(g_sc.list, static_cast<LPARAM>(g_sc.sel));
}

void script_requeue_selection() {
    if (!g_sc.win || !g_sc.select_delayed) return;
    g_sc.select_delayed = false;
    sc_queue_selection();
}

void sc_add() {
    if (!g_sc.loaded || !sc_flush_selection() || !sc_apply()) return;
    JsonValue* list = sc_list(true);
    if (!list) return;
    std::string name;
    for (int i = 1; ; ++i) {
        name = "script_" + std::to_string(i);
        if (!script_name_used(g_sc.root, name, static_cast<size_t>(-1))) break;
    }
    JsonValue item = JsonValue::make_obj();
    item.set("name", JsonValue::make_str(name));
    item.set("code", JsonValue::make_str("# Ruby / RGSS\n# Als Startscript markieren oder aus einem Event aufrufen.\n"));
    // Etappe 36 (wie RPG Maker XP „Einfügen“): über dem markierten Eintrag;
    // ohne Auswahl über „089 - Main“, damit eigene Scripts vor Main laden.
    const size_t pos = script_insert_position(*list, g_sc.sel);
    list->arr.insert(list->arr.begin() + static_cast<std::ptrdiff_t>(pos), std::move(item));
    g_sc.sel = static_cast<int>(pos);
    g_sc.dirty = true;
    sc_refresh_list();
    sc_refresh_fields();
    sc_status("Neues Script angelegt — Speichern nicht vergessen");
    SetFocus(g_sc.name);
}

void sc_delete() {
    if (!sc_flush_selection() || !sc_item()) return;
    if (data_prompt(g_sc.win, L"Dieses Script einschließlich seiner Zusatzdaten löschen?\r\n"
                              L"Nach dem Speichern ist die Löschung nicht rückgängig.",
                    L"Script löschen", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
    JsonValue* list = sc_list();
    if (!list || g_sc.sel < 0 || static_cast<size_t>(g_sc.sel) >= list->arr.size()) return;
    list->arr.erase(list->arr.begin() + g_sc.sel);
    if (g_sc.sel >= static_cast<int>(list->arr.size())) g_sc.sel = static_cast<int>(list->arr.size()) - 1;
    g_sc.dirty = true;
    sc_refresh_list();
    sc_refresh_fields();
    sc_status("Script gelöscht — Speichern nicht vergessen");
}

bool sc_save() {
    if (!g_sc.loaded || !sc_project_matches() || !sc_flush_selection() || !sc_apply()) return false;
    if (!g_sc.dirty) { sc_status("Keine ungespeicherten Scriptänderungen"); return true; }
    if (!editor_json_utf8(g_sc.root) || !editor_json_serializable(g_sc.root)) {
        data_prompt(g_sc.win, L"Das Scriptdokument enthält ungültige Daten und wird nicht überschrieben.",
                    L"Scripts nicht gespeichert", MB_OK | MB_ICONERROR);
        return false;
    }
    const std::string output = json_write(g_sc.root);
    DocumentStore store;
    DocumentRecovery recovery;
    assets::Result result = store.open(g.root, g_sc.project, false, "scripts.json");
    if (result == assets::Result::Ok) {
        const DocumentOutcome outcome = document_save_confirmed(store, g_sc.baseline_present, g_sc.baseline,
            output, [] {
                return data_prompt(g_sc.win, L"scripts.json wurde seit dem Laden extern geändert oder entfernt. "
                    L"Den externen Stand ausdrücklich mit dem Editorentwurf überschreiben?",
                    L"Script-Dateikonflikt", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES;
            }, recovery);
        if (outcome.cancelled) return false;
        result = outcome.result;
    }
    if (result != assets::Result::Ok) {
        document_result_notice(g_sc.win, L"Scripts nicht gespeichert", result, recovery, g.project_dir);
        return false;
    }
    g_sc.baseline = output;
    g_sc.baseline_present = true;
    g_sc.dirty = false;
    g_sc.touched = false;
    sc_title();
    sc_status("scripts.json gespeichert — F5 im Spiel lädt die Änderungen");
    document_result_notice(g_sc.win, L"Scripts gespeichert", result, recovery, g.project_dir);
    return true;
}

void sc_reload() {
    if (!sc_flush_selection()) return;
    if ((g_sc.dirty || sc_has_pending()) && data_prompt(g_sc.win,
        L"Ungespeicherte Scriptänderungen verwerfen und die Datei neu laden?",
        L"Scripts neu laden", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
    if (!sc_load_file()) return;
    sc_refresh_list();
    sc_refresh_fields();
    sc_status("scripts.json vom Datenträger geladen");
}

bool script_request_close() {
    if (!g_sc.win || !IsWindow(g_sc.win)) return true;
    if (g.data_prompt_active || !sc_flush_selection()) return false;
    if (g_sc.dirty || sc_has_pending()) {
        const int answer = data_prompt(g_sc.win,
            L"Ungespeicherte Scriptänderungen speichern?\r\n\r\n"
            L"Ja: speichern   Nein: verwerfen   Abbrechen: zurück",
            L"RGSS Script-Editor", MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON3);
        if (answer == IDYES) { if (!sc_save()) return false; }
        else if (answer != IDNO) return false;
    }
    DestroyWindow(g_sc.win);
    return true;
}

void sc_check_syntax() {
    if (!g_sc.before.code_editable) return;
    std::string code, message;
    if (!control_text_strict(g_sc.code, code)) {
        data_prompt(g_sc.win, L"Der Text enthält ungültiges UTF-16.", L"Ruby-Syntax", MB_OK | MB_ICONWARNING);
        return;
    }
    if (rune::ruby::check_syntax(script_normalize_lines(code), message)) {
        sc_status("Ruby-Syntax gültig — Code wurde nicht ausgeführt");
        data_prompt(g_sc.win, L"Ruby-Syntax gültig. Der Code wurde nur geprüft, nicht ausgeführt.\r\n"
                              L"Dies garantiert keine vollständige RPG-Maker-XP-Kompatibilität.",
                    L"Ruby-Syntax", MB_OK | MB_ICONINFORMATION);
    } else {
        if (message.size() > 12000) message.resize(12000);
        sc_status("Ruby-Syntaxprüfung meldet einen Fehler");
        const std::wstring text = L"Prüfung ohne Ausführung:\r\n\r\n" + utf8_to_wide(message);
        data_prompt(g_sc.win, text.c_str(), L"Ruby-Syntax", MB_OK | MB_ICONWARNING);
    }
}

int sc_gutter_width() {
    const int lines = g_sc.code ? std::max(1, static_cast<int>(SendMessageW(g_sc.code, EM_GETLINECOUNT, 0, 0))) : 1;
    int digits = 1;
    for (int n = lines; n >= 10; n /= 10) ++digits;
    HDC dc = g_sc.code ? GetDC(g_sc.code) : nullptr;
    TEXTMETRICW metrics{};
    if (dc) {
        HFONT old = reinterpret_cast<HFONT>(SelectObject(dc, g_sc.code_font ? g_sc.code_font : g.font));
        GetTextMetricsW(dc, &metrics);
        SelectObject(dc, old);
        ReleaseDC(g_sc.code, dc);
    }
    const int digit_width = std::max(main_layout_scale(8, g_sc.dpi), static_cast<int>(metrics.tmAveCharWidth));
    return std::max(main_layout_scale(44, g_sc.dpi), (digits + 2) * digit_width);
}

void sc_cursor_update() {
    if (!g_sc.code || !g_sc.cursor) return;
    DWORD start = 0, end = 0;
    SendMessageW(g_sc.code, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    const int line = static_cast<int>(SendMessageW(g_sc.code, EM_LINEFROMCHAR, end, 0));
    const LRESULT index = SendMessageW(g_sc.code, EM_LINEINDEX, static_cast<WPARAM>(line), 0);
    wchar_t text[96];
    std::swprintf(text, 96, L"Zeile %d  ·  Spalte %lu", std::max(0, line) + 1,
                  static_cast<unsigned long>(index < 0 || end < static_cast<DWORD>(index) ? 1 : end - index + 1));
    SetWindowTextW(g_sc.cursor, text);
    if (g_sc.gutter) InvalidateRect(g_sc.gutter, nullptr, FALSE);
}

LRESULT CALLBACK sc_gutter_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(h, &paint);
        RECT rc{}; GetClientRect(h, &rc);
        HBRUSH background = CreateSolidBrush(RGB(239, 242, 246));
        FillRect(dc, &rc, background ? background : GetSysColorBrush(COLOR_BTNFACE));
        if (background) DeleteObject(background);
        if (g_sc.code) {
            HFONT old = reinterpret_cast<HFONT>(SelectObject(dc, g_sc.code_font ? g_sc.code_font : g.font));
            TEXTMETRICW metrics{}; GetTextMetricsW(dc, &metrics);
            const int height = std::max(1, static_cast<int>(metrics.tmHeight + metrics.tmExternalLeading));
            const int first = std::max(0, static_cast<int>(SendMessageW(g_sc.code, EM_GETFIRSTVISIBLELINE, 0, 0)));
            const int lines = std::max(1, static_cast<int>(SendMessageW(g_sc.code, EM_GETLINECOUNT, 0, 0)));
            DWORD start = 0, end = 0;
            SendMessageW(g_sc.code, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
            const int active = static_cast<int>(SendMessageW(g_sc.code, EM_LINEFROMCHAR, end, 0));
            SetBkMode(dc, TRANSPARENT);
            for (int line = first; line < lines && line < first + rc.bottom / height + 3; ++line) {
                const LRESULT index = SendMessageW(g_sc.code, EM_LINEINDEX, static_cast<WPARAM>(line), 0);
                const LRESULT pos = index < 0 ? -1 : SendMessageW(g_sc.code, EM_POSFROMCHAR, static_cast<WPARAM>(index), 0);
                int y = main_layout_scale(2, g_sc.dpi) + (line - first) * height;
                if (pos != -1) {
                    // EM_POSFROMCHAR liefert Edit-Clientkoordinaten. Die eigene
                    // Randspalte beginnt außerhalb des Edit-Rahmens.
                    POINT origin{0, static_cast<short>(HIWORD(pos))};
                    MapWindowPoints(g_sc.code, h, &origin, 1);
                    y = origin.y;
                }
                if (y >= rc.bottom) break;
                RECT row{0, y, rc.right - main_layout_scale(7, g_sc.dpi), y + height};
                SetTextColor(dc, line == active ? RGB(42, 77, 120) : RGB(115, 125, 138));
                wchar_t label[32]; std::swprintf(label, 32, L"%d", line + 1);
                DrawTextW(dc, label, -1, &row, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP);
            }
            SelectObject(dc, old);
        }
        EndPaint(h, &paint);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void sc_indent_selection(bool outdent) {
    if (!g_sc.code || !g_sc.before.code_editable) return;
    const int length = GetWindowTextLengthW(g_sc.code);
    if (length < 0 || static_cast<size_t>(length) > SC_TEXT_LIMIT) return;
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(g_sc.code, text.data(), length + 1);
    if (copied != length) return;
    text.resize(static_cast<size_t>(length));
    DWORD start = 0, end = 0;
    SendMessageW(g_sc.code, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    const ScriptIndent edit = script_indent(text, start, end, outdent);
    if (!edit.changed || text.size() - (edit.end - edit.begin) + edit.replacement.size() > SC_TEXT_LIMIT) return;
    SendMessageW(g_sc.code, EM_SETSEL, static_cast<WPARAM>(edit.begin), static_cast<LPARAM>(edit.end));
    SendMessageW(g_sc.code, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(edit.replacement.c_str()));
    SendMessageW(g_sc.code, EM_SETSEL, static_cast<WPARAM>(edit.selection_begin), static_cast<LPARAM>(edit.selection_end));
    sc_cursor_update();
}

// Etappe 36: nächste Fundstelle im aktuellen Script (ab Cursor, mit Umlauf).
void sc_find_next() {
    if (!g_sc.code) return;
    const HWND find = GetDlgItem(g_sc.win, IDSC_FIND);
    const int nlen = find ? GetWindowTextLengthW(find) : 0;
    if (nlen <= 0) { if (find) SetFocus(find); sc_status("Suchbegriff eingeben (Strg+F)"); return; }
    std::wstring needle(static_cast<size_t>(nlen) + 1, L'\0');
    GetWindowTextW(find, needle.data(), nlen + 1);
    needle.resize(static_cast<size_t>(nlen));
    const int length = GetWindowTextLengthW(g_sc.code);
    if (length < 0 || static_cast<size_t>(length) > SC_TEXT_LIMIT) return;
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(g_sc.code, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    DWORD start = 0, end = 0;
    SendMessageW(g_sc.code, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    const std::size_t pos = script_find_text(text, needle, end);
    if (pos == std::wstring::npos) {
        sc_status("„" + wide_to_utf8(needle) + "“ in diesem Script nicht gefunden");
        return;
    }
    SendMessageW(g_sc.code, EM_SETSEL, static_cast<WPARAM>(pos), static_cast<LPARAM>(pos + needle.size()));
    SendMessageW(g_sc.code, EM_SCROLLCARET, 0, 0);
    sc_cursor_update();
    sc_status(pos < end ? "Gefunden (von vorn weitergesucht) — F3 = weiter" : "Gefunden — F3 = weiter");
}

// Etappe 36: Ersetzen im aktuellen Script. „Alle“ ohne Umlauf; ein Treffer
// ab der Cursorposition. Der Edit-Undo-Pfad bleibt über EM_REPLACESEL erhalten.
void sc_replace(bool all) {
    if (!g_sc.code) return;
    auto edit_text = [](HWND h) {
        std::wstring value;
        if (!h) return value;
        const int n = GetWindowTextLengthW(h);
        if (n <= 0) return value;
        value.resize(static_cast<size_t>(n));
        GetWindowTextW(h, value.data(), n + 1);
        return value;
    };
    const std::wstring needle = edit_text(GetDlgItem(g_sc.win, IDSC_FIND));
    const std::wstring replacement = edit_text(GetDlgItem(g_sc.win, IDSC_REPLACE));
    if (needle.empty()) {
        if (HWND find = GetDlgItem(g_sc.win, IDSC_FIND)) SetFocus(find);
        sc_status("Suchbegriff eingeben (Strg+F)");
        return;
    }
    const int length = GetWindowTextLengthW(g_sc.code);
    if (length < 0 || static_cast<size_t>(length) > SC_TEXT_LIMIT) return;
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(g_sc.code, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    if (all) {
        std::wstring changed = text;
        const std::size_t count = script_replace_all(changed, needle, replacement);
        if (!count) { sc_status("„" + wide_to_utf8(needle) + "“ nicht gefunden"); return; }
        if (changed.size() > SC_TEXT_LIMIT) {
            sc_status("Ersetzen würde das Script zu groß machen — nichts geändert");
            return;
        }
        SetWindowTextW(g_sc.code, changed.c_str());
        sc_cursor_update();
        sc_status(std::to_string(count) + " Stelle(n) ersetzt — Übernehmen/Speichern nicht vergessen");
        return;
    }
    DWORD start = 0, end = 0;
    SendMessageW(g_sc.code, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    const std::size_t pos = script_find_text(text, needle, end);
    if (pos == std::wstring::npos) {
        sc_status("„" + wide_to_utf8(needle) + "“ in diesem Script nicht gefunden");
        return;
    }
    if (text.size() - needle.size() + replacement.size() > SC_TEXT_LIMIT) {
        sc_status("Ersetzen würde das Script zu groß machen — nichts geändert");
        return;
    }
    SendMessageW(g_sc.code, EM_SETSEL, static_cast<WPARAM>(pos),
                 static_cast<LPARAM>(pos + needle.size()));
    SendMessageW(g_sc.code, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(replacement.c_str()));
    const std::size_t after = pos + replacement.size();
    SendMessageW(g_sc.code, EM_SETSEL, static_cast<WPARAM>(after), static_cast<LPARAM>(after));
    SendMessageW(g_sc.code, EM_SCROLLCARET, 0, 0);
    sc_cursor_update();
    sc_status(pos < end ? "Ersetzt (mit Umlauf) — Strg+H = weiter" : "Ersetzt — Strg+H = weiter");
}

// Suchfeld: Enter/F3 = weiter suchen, Strg+Enter = alle ersetzen, Esc = Code.
LRESULT CALLBACK sc_find_subclass(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_GETDLGCODE && lp) {
        const MSG* m = reinterpret_cast<const MSG*>(lp);
        if (m->message == WM_KEYDOWN && (m->wParam == VK_RETURN || m->wParam == VK_ESCAPE))
            return DLGC_WANTMESSAGE;
    }
    if (msg == WM_KEYDOWN && wp == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000)) {
        sc_replace(true); return 0;
    }
    if (msg == WM_KEYDOWN && (wp == VK_RETURN || wp == VK_F3)) { sc_find_next(); return 0; }
    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) { if (g_sc.code) SetFocus(g_sc.code); return 0; }
    if (msg == WM_CHAR && (wp == L'\r' || wp == 27)) return 0;   // kein Warnton
    if (msg == WM_NCDESTROY) RemoveWindowSubclass(h, sc_find_subclass, 2);
    return DefSubclassProc(h, msg, wp, lp);
}

// Ersetzen-Feld: Enter/F3 = nächstes Vorkommen ersetzen, Strg+Enter = alle.
LRESULT CALLBACK sc_replace_subclass(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_GETDLGCODE && lp) {
        const MSG* m = reinterpret_cast<const MSG*>(lp);
        if (m->message == WM_KEYDOWN && (m->wParam == VK_RETURN || m->wParam == VK_ESCAPE))
            return DLGC_WANTMESSAGE;
    }
    if (msg == WM_KEYDOWN && wp == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000)) {
        sc_replace(true); return 0;
    }
    if (msg == WM_KEYDOWN && (wp == VK_RETURN || wp == VK_F3)) { sc_replace(false); return 0; }
    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) { if (g_sc.code) SetFocus(g_sc.code); return 0; }
    if (msg == WM_CHAR && (wp == L'\r' || wp == 27)) return 0;
    if (msg == WM_NCDESTROY) RemoveWindowSubclass(h, sc_replace_subclass, 3);
    return DefSubclassProc(h, msg, wp, lp);
}

LRESULT CALLBACK sc_code_subclass(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_GETDLGCODE) return DefSubclassProc(h, msg, wp, lp) | DLGC_WANTTAB | DLGC_WANTALLKEYS;
    // Etappe 36: Strg+F = Suchfeld, F3 = weiter suchen (wie XP-Script-Editor).
    if (msg == WM_KEYDOWN && wp == 'F' && (GetKeyState(VK_CONTROL) & 0x8000)) {
        if (const HWND find = GetDlgItem(g_sc.win, IDSC_FIND)) {
            SetFocus(find);
            SendMessageW(find, EM_SETSEL, 0, -1);
        }
        return 0;
    }
    if (msg == WM_KEYDOWN && wp == 'H' && (GetKeyState(VK_CONTROL) & 0x8000)) {
        if (const HWND replace = GetDlgItem(g_sc.win, IDSC_REPLACE)) {
            SetFocus(replace);
            SendMessageW(replace, EM_SETSEL, 0, -1);
        }
        return 0;
    }
    if (msg == WM_KEYDOWN && wp == VK_F3) { sc_find_next(); return 0; }
    if (msg == WM_CHAR && ((wp == 6 && (GetKeyState(VK_CONTROL) & 0x8000)) ||
                           (wp == 8 && (GetKeyState(VK_CONTROL) & 0x8000)))) return 0;   // Strg+F/H ohne Zeichen
    if (msg == WM_CHAR && wp == L'\t') {
        sc_indent_selection((GetKeyState(VK_SHIFT) & 0x8000) != 0);
        return 0;
    }
    if (msg == WM_CHAR && wp == 19 && (GetKeyState(VK_CONTROL) & 0x8000)) return 0;
    const LRESULT result = DefSubclassProc(h, msg, wp, lp);
    if ((msg == WM_CHAR || msg == WM_KEYUP || msg == WM_LBUTTONUP || msg == WM_MOUSEWHEEL ||
         msg == WM_VSCROLL || msg == WM_HSCROLL || msg == WM_SIZE || msg == WM_SETTEXT) && g_sc.win)
        PostMessageW(g_sc.win, WM_SCRIPT_CURSOR, 0, 0);
    if (msg == WM_NCDESTROY) RemoveWindowSubclass(h, sc_code_subclass, 1);
    return result;
}

void sc_layout() {
    if (!g_sc.win || !g_sc.ready) return;
    RECT rc{}; GetClientRect(g_sc.win, &rc);
    const auto s = [](int n) { return main_layout_scale(n, g_sc.dpi); };
    const int width = std::max(1, static_cast<int>(rc.right));
    const int height = std::max(1, static_cast<int>(rc.bottom));
    const int margin = s(12), gap = s(12);
    const int left = std::clamp(width / 4, s(150), s(260));
    const int right_x = margin + left + gap;
    const int right_w = std::max(s(80), width - right_x - margin);
    const auto move = [](HWND h, int x, int y, int w, int hgt) {
        if (h) MoveWindow(h, x, y, std::max(1, w), std::max(1, hgt), TRUE);
    };
    move(GetDlgItem(g_sc.win, IDSC_LIST_LABEL), margin, s(10), left, s(21));
    move(g_sc.list, margin, s(34), left, std::max(s(30), height - s(150)));
    const int left_y = height - s(108);
    const int half = (left - s(8)) / 2;
    move(GetDlgItem(g_sc.win, IDSC_NEW), margin, left_y, half, s(30));
    move(GetDlgItem(g_sc.win, IDSC_DEL), margin + half + s(8), left_y, half, s(30));
    move(GetDlgItem(g_sc.win, IDSC_CLOSE), margin, height - s(69), left, s(30));
    move(GetDlgItem(g_sc.win, IDSC_NAME_LABEL), right_x, s(12), s(56), s(21));
    move(g_sc.name, right_x + s(58), s(8), std::max(s(80), right_w - s(58)), s(27));
    move(g_sc.autoload, right_x, s(43), right_w, s(25));
    move(GetDlgItem(g_sc.win, IDSC_CODE_LABEL), right_x, s(75), right_w, s(21));
    const int code_y = s(99), code_h = std::max(s(25), height - code_y - s(162));
    g_sc.last_gutter_width = sc_gutter_width();
    const int gutter = std::min(g_sc.last_gutter_width, std::max(1, right_w / 3));
    move(g_sc.gutter, right_x, code_y, gutter, code_h);
    move(g_sc.code, right_x + gutter, code_y, right_w - gutter, code_h);
    // Etappe 36: XP-artige Suche/Ersetzen direkt unter dem Codefenster.
    const int find_y = height - s(160), replace_y = height - s(130);
    move(GetDlgItem(g_sc.win, IDSC_FIND_LABEL), right_x, find_y + s(3), s(50), s(21));
    move(GetDlgItem(g_sc.win, IDSC_FIND), right_x + s(52), find_y,
         std::max(s(50), right_w - s(190)), s(25));
    move(GetDlgItem(g_sc.win, IDSC_FIND_NEXT), right_x + right_w - s(130), find_y, s(130), s(25));
    move(GetDlgItem(g_sc.win, IDSC_REPLACE_LABEL), right_x, replace_y + s(3), s(70), s(21));
    move(GetDlgItem(g_sc.win, IDSC_REPLACE), right_x + s(72), replace_y,
         std::max(s(50), right_w - s(270)), s(25));
    move(GetDlgItem(g_sc.win, IDSC_REPLACE_ONE), right_x + right_w - s(190), replace_y, s(90), s(25));
    move(GetDlgItem(g_sc.win, IDSC_REPLACE_ALL), right_x + right_w - s(94), replace_y, s(86), s(25));
    const int status_y = height - s(94);
    move(g_sc.status, right_x, status_y, std::max(s(50), right_w - s(165)), s(21));
    move(g_sc.cursor, right_x + std::max(0, right_w - s(160)), status_y, std::min(right_w, s(160)), s(21));
    const int button_y = height - s(65), bw = std::max(1, (right_w - s(18)) / 4);
    move(GetDlgItem(g_sc.win, IDSC_APPLY), right_x, button_y, bw, s(32));
    move(GetDlgItem(g_sc.win, IDSC_CHECK), right_x + bw + s(6), button_y, bw, s(32));
    move(GetDlgItem(g_sc.win, IDSC_SAVE), right_x + 2 * (bw + s(6)), button_y, bw, s(32));
    move(GetDlgItem(g_sc.win, IDSC_RELOAD), right_x + 3 * (bw + s(6)), button_y, bw, s(32));
    InvalidateRect(g_sc.gutter, nullptr, FALSE);
}

BOOL CALLBACK sc_update_child_font(HWND child, LPARAM) {
    HFONT font = child == g_sc.code || child == g_sc.gutter ? g_sc.code_font : g_sc.ui_font;
    SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font ? font : g.font), TRUE);
    return TRUE;
}

void sc_set_fonts(unsigned dpi) {
    g_sc.dpi = dpi ? dpi : 96;
    HFONT old_ui = g_sc.ui_font, old_code = g_sc.code_font;
    g_sc.ui_font = event_view_make_font(g_sc.dpi);
    g_sc.code_font = CreateFontW(-std::max(1, main_layout_scale(15, g_sc.dpi)), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    if (g_sc.win) EnumChildWindows(g_sc.win, sc_update_child_font, 0);
    if (old_ui) DeleteObject(old_ui);
    if (old_code) DeleteObject(old_code);
}

HWND sc_control(const wchar_t* cls, const wchar_t* text, DWORD style, int id) {
    HWND control = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
        0, 0, 1, 1, g_sc.win, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_sc.ui_font ? g_sc.ui_font : g.font), TRUE);
    return control;
}

LRESULT CALLBACK script_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (g.data_prompt_active || (g.props_dlg && g.dlg_mode != 1)) {
        if (msg == WM_SCRIPT_SELECT && g_sc.select_queued) {
            g_sc.select_queued = false; g_sc.select_delayed = true; return 0;
        }
        if (msg == WM_CLOSE) return 0;
        if (msg == WM_COMMAND) {
            if (LOWORD(wp) == IDSC_LIST && HIWORD(wp) == LBN_SELCHANGE && g_sc.list)
                listbox_select_data(g_sc.list, static_cast<LPARAM>(g_sc.sel));
            return 0;
        }
    }
    switch (msg) {
        case WM_SCRIPT_SELECT: if (g_sc.select_queued) sc_reconcile_selection(); return 0;
        case WM_SCRIPT_CURSOR: sc_cursor_update(); return 0;
        case WM_SIZE: sc_layout(); return 0;
        case WM_CONTEXTMENU: {
            // Etappe 36: Rechtsklick in der Liste wie XP (Einfügen/Löschen).
            if (reinterpret_cast<HWND>(wp) != g_sc.list || !g_sc.loaded) break;
            POINT screen{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (screen.x == -1 && screen.y == -1) {
                RECT rc{}; GetWindowRect(g_sc.list, &rc);
                screen = {rc.left + main_layout_scale(10, g_sc.dpi),
                          rc.top + main_layout_scale(10, g_sc.dpi)};
            }
            POINT local = screen;
            ScreenToClient(g_sc.list, &local);
            const LRESULT hit = SendMessageW(g_sc.list, LB_ITEMFROMPOINT, 0,
                                             MAKELPARAM(local.x, local.y));
            if (HIWORD(hit) == 0) {
                if (!sc_flush_selection()) return 0;
                const int row = static_cast<int>(LOWORD(hit));
                SendMessageW(g_sc.list, LB_SETCURSEL, static_cast<WPARAM>(row), 0);
                g_sc.sel = row;
                sc_refresh_fields();
            }
            if (HMENU menu = CreatePopupMenu()) {
                AppendMenuW(menu, MF_STRING, IDSC_NEW, L"Einfügen (vor Auswahl)");
                AppendMenuW(menu, MF_STRING, IDSC_DEL, L"Löschen");
                EnableMenuItem(menu, IDSC_DEL, g_sc.sel >= 0 ? MF_ENABLED : MF_GRAYED);
                TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                               screen.x, screen.y, 0, h, nullptr);
                DestroyMenu(menu);
            }
            return 0;
        }
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lp);
            EventLayoutRect work{};
            const int w = main_layout_scale(720, g_sc.dpi), hgt = main_layout_scale(440, g_sc.dpi);
            if (event_view_work_area(h, nullptr, work)) {
                info->ptMinTrackSize.x = std::min(w, work.right - work.left);
                info->ptMinTrackSize.y = std::min(hgt, work.bottom - work.top);
            } else { info->ptMinTrackSize.x = w; info->ptMinTrackSize.y = hgt; }
            return 0;
        }
        case WM_DPICHANGED: {
            sc_set_fonts(LOWORD(wp));
            const RECT* suggested = reinterpret_cast<const RECT*>(lp);
            EventLayoutRect target = event_view_rect(*suggested), work{};
            if (event_view_work_area(h, suggested, work)) target = event_layout_fit(work, target);
            SetWindowPos(h, nullptr, target.left, target.top, target.right - target.left,
                         target.bottom - target.top, SWP_NOZORDER | SWP_NOACTIVATE);
            sc_layout(); sc_cursor_update();
            return 0;
        }
        case WM_COMMAND: {
            if (g_sc.syncing || (g.props_dlg && g.dlg_mode != 1)) return 0;
            const int id = LOWORD(wp), code = HIWORD(wp);
            if (id == IDSC_LIST && code == LBN_SELCHANGE) { sc_queue_selection(); return 0; }
            if ((id == IDSC_NAME || id == IDSC_CODE) && code == EN_CHANGE) {
                g_sc.touched = true; sc_title();
                if (id == IDSC_CODE) {
                    // Nicht bei jedem Tastendruck sämtliche Controls bewegen;
                    // nur ein Ziffernwechsel der Zeilenzahl ändert das Layout.
                    if (sc_gutter_width() != g_sc.last_gutter_width) sc_layout();
                    sc_cursor_update();
                }
                return 0;
            }
            if (id == IDSC_CODE && (code == EN_VSCROLL || code == EN_HSCROLL)) { sc_cursor_update(); return 0; }
            if (id == IDSC_AUTOLOAD && code == BN_CLICKED) { g_sc.touched = true; sc_title(); return 0; }
            if (code != BN_CLICKED) return 0;
            if (id == IDSC_NEW) sc_add();
            else if (id == IDSC_DEL) sc_delete();
            else if (id == IDSC_FIND_NEXT) sc_find_next();
            else if (id == IDSC_REPLACE_ONE) sc_replace(false);
            else if (id == IDSC_REPLACE_ALL) sc_replace(true);
            else if (id == IDSC_APPLY) { if (sc_flush_selection()) sc_apply(); }
            else if (id == IDSC_CHECK) { if (sc_flush_selection()) sc_check_syntax(); }
            else if (id == IDSC_SAVE) sc_save();
            else if (id == IDSC_RELOAD) sc_reload();
            else if (id == IDSC_CLOSE || id == IDCANCEL) script_request_close();
            return 0;
        }
        case WM_CLOSE: script_request_close(); return 0;
        case WM_NCDESTROY:
            if (g_sc.win == h) {
                HFONT ui = g_sc.ui_font, code = g_sc.code_font;
                g_sc = ScriptDlgState{};
                if (ui) DeleteObject(ui);
                if (code) DeleteObject(code);
            }
            break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void open_script_editor() {
    if (g_sc.win && IsWindow(g_sc.win)) {
        event_view_fit_existing(g_sc.win);
        SetForegroundWindow(g_sc.win);
        return;
    }
    if (g.project_name.empty() || g.data_prompt_active || g.props_dlg) return;
    g_sc = ScriptDlgState{};
    g_sc.project = g.project_name;
    g_sc.project_epoch = g.project_epoch;
    g_sc.dpi = GetDpiForWindow(g.main);
    if (!g_sc.dpi) g_sc.dpi = 96;
    g_sc.syncing = true;

    WNDCLASSW wc{};
    wc.lpfnWndProc = script_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RuneEditorScriptDlg";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassW(&wc);
    WNDCLASSW gutter{};
    gutter.lpfnWndProc = sc_gutter_proc;
    gutter.hInstance = wc.hInstance;
    gutter.lpszClassName = L"RuneEditorScriptGutter";
    gutter.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&gutter);

    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_CLIPCHILDREN;
    const DWORD ex = WS_EX_DLGMODALFRAME;
    RECT frame{0, 0, main_layout_scale(1080, g_sc.dpi), main_layout_scale(700, g_sc.dpi)};
    if (!AdjustWindowRectExForDpi(&frame, style, FALSE, ex, g_sc.dpi)) AdjustWindowRectEx(&frame, style, FALSE, ex);
    EventLayoutRect work{};
    if (!event_view_work_area(g.main, nullptr, work))
        work = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    RECT owner{}; GetWindowRect(g.main, &owner);
    const EventLayoutRect bounds = event_layout_center(work, event_view_rect(owner), frame.right - frame.left, frame.bottom - frame.top);
    g_sc.win = CreateWindowExW(ex, wc.lpszClassName, L"RGSS Script-Editor", style,
        bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top,
        g.main, nullptr, wc.hInstance, nullptr);
    if (!g_sc.win) { g_sc = ScriptDlgState{}; return; }
    sc_set_fonts(g_sc.dpi);
    sc_control(L"Static", L"PROJEKT-SCRIPTS", 0, IDSC_LIST_LABEL);
    g_sc.list = sc_control(L"ListBox", nullptr, WS_TABSTOP | WS_VSCROLL | WS_BORDER | LBS_NOTIFY | LBS_HASSTRINGS, IDSC_LIST);
    sc_control(L"Button", L"Neu", WS_TABSTOP | BS_PUSHBUTTON, IDSC_NEW);
    sc_control(L"Button", L"Löschen", WS_TABSTOP | BS_PUSHBUTTON, IDSC_DEL);
    sc_control(L"Button", L"Schließen", WS_TABSTOP | BS_PUSHBUTTON, IDSC_CLOSE);
    sc_control(L"Static", L"Name", 0, IDSC_NAME_LABEL);
    g_sc.name = sc_control(L"Edit", nullptr, WS_TABSTOP | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL, IDSC_NAME);
    g_sc.autoload = sc_control(L"Button",
        L"Als RGSS-Startscript ausführen (XP-Defaultscripts und 089 - Main bleiben geschützt)",
        WS_TABSTOP | BS_AUTOCHECKBOX, IDSC_AUTOLOAD);
    sc_control(L"Static", L"Ruby / RGSS   ·   Strg+F: suchen   ·   Strg+H: ersetzen   ·   F3: weiter   ·   Tab: ein-/ausrücken   ·   Strg+S: speichern", 0, IDSC_CODE_LABEL);
    g_sc.gutter = sc_control(gutter.lpszClassName, nullptr, 0, IDSC_GUTTER);
    g_sc.code = sc_control(L"Edit", nullptr, WS_TABSTOP | WS_BORDER | WS_VSCROLL | WS_HSCROLL |
        ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN, IDSC_CODE);
    sc_control(L"Static", L"Suchen", 0, IDSC_FIND_LABEL);
    HWND find_edit = sc_control(L"Edit", nullptr,
        WS_TABSTOP | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL, IDSC_FIND);
    sc_control(L"Button", L"Weiter (F3)", WS_TABSTOP | BS_PUSHBUTTON, IDSC_FIND_NEXT);
    sc_control(L"Static", L"Ersetzen", 0, IDSC_REPLACE_LABEL);
    HWND replace_edit = sc_control(L"Edit", nullptr,
        WS_TABSTOP | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL, IDSC_REPLACE);
    sc_control(L"Button", L"Ersetzen", WS_TABSTOP | BS_PUSHBUTTON, IDSC_REPLACE_ONE);
    sc_control(L"Button", L"Alle", WS_TABSTOP | BS_PUSHBUTTON, IDSC_REPLACE_ALL);
    g_sc.status = sc_control(L"Static", L"", 0, IDSC_STATUS);
    g_sc.cursor = sc_control(L"Static", L"", SS_RIGHT, IDSC_CURSOR);
    sc_control(L"Button", L"Übernehmen", WS_TABSTOP | BS_PUSHBUTTON, IDSC_APPLY);
    sc_control(L"Button", L"Syntax prüfen", WS_TABSTOP | BS_PUSHBUTTON, IDSC_CHECK);
    sc_control(L"Button", L"Speichern", WS_TABSTOP | BS_PUSHBUTTON, IDSC_SAVE);
    sc_control(L"Button", L"Neu laden", WS_TABSTOP | BS_PUSHBUTTON, IDSC_RELOAD);
    if (!g_sc.list || !g_sc.name || !g_sc.code || !g_sc.gutter || !g_sc.status || !g_sc.cursor || !g_sc.autoload ||
        !find_edit || !replace_edit || !SetWindowSubclass(g_sc.code, sc_code_subclass, 1, 0) ||
        !SetWindowSubclass(find_edit, sc_find_subclass, 2, 0) ||
        !SetWindowSubclass(replace_edit, sc_replace_subclass, 3, 0)) {
        DestroyWindow(g_sc.win);
        data_prompt(g.main, L"Der Script-Editor konnte seine Steuerelemente nicht anlegen.",
                    L"Script-Editor", MB_OK | MB_ICONERROR);
        return;
    }
    SendMessageW(g_sc.name, EM_SETLIMITTEXT, static_cast<WPARAM>(SC_TEXT_LIMIT), 0);
    SendMessageW(g_sc.code, EM_SETLIMITTEXT, static_cast<WPARAM>(SC_TEXT_LIMIT), 0);
    SendMessageW(g_sc.code, WM_SETFONT, reinterpret_cast<WPARAM>(g_sc.code_font ? g_sc.code_font : g.font), TRUE);
    g_sc.ready = true;
    const bool loaded = sc_load_file();
    sc_refresh_list();
    sc_refresh_fields();
    g_sc.syncing = false;
    sc_layout();
    sc_status(loaded ? "Projekt-Scripts geladen" : "Datei nicht geladen — nach externer Prüfung neu laden");
    ShowWindow(g_sc.win, SW_SHOW);
    SetForegroundWindow(g_sc.win);
    if (g_sc.sel >= 0) SetFocus(g_sc.code); else SetFocus(GetDlgItem(g_sc.win, IDSC_NEW));
}

bool script_editor_focused() {
    if (!g_sc.win) return false;
    const HWND foreground = GetForegroundWindow();
    return foreground == g_sc.win || (foreground && IsChild(g_sc.win, foreground));
}
