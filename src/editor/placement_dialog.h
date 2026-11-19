#pragma once
// ============================================================================
//  Etappe 19n: Platzier-Editor (Win32). Wird in editor_main.cpp innerhalb des
//  Editor-Namensraums eingebunden. Die Karte wird erst bei OK aus einer Kopie
//  übernommen; Abbrechen verwirft alle Änderungen dieser Dialogsitzung.
// ============================================================================

enum {
    IDPL_KIND = 8401, IDPL_LIST = 8402, IDPL_NEW = 8403, IDPL_DEL = 8404,
    IDPL_NAME = 8410, IDPL_TYPE = 8411, IDPL_X = 8412, IDPL_Y = 8413,
    IDPL_Z = 8414, IDPL_SPEED = 8415, IDPL_RADIUS = 8416, IDPL_ROT = 8417,
    IDPL_PATH = 8418, IDPL_TEXT = 8419, IDPL_APPLY = 8420,
    IDPL_OK = 8421, IDPL_CANCEL = 8422, IDPL_HINT = 8423, IDPL_CURSOR = 8424
};

struct PlacementDlgState {
    HWND win = nullptr;
    JsonValue work;
    PlacementKind kind = PlacementKind::Npc;
    int index = -1;                 // Index im jeweiligen Karten-Array
    std::vector<int> rows;          // sichtbare Listenzeile -> Kartenindex
    std::vector<std::string> types;
    PlacementForm before;
    bool name_editable = false;
    bool path_editable = false;
    bool text_editable = false;
    bool syncing = false;
};
static PlacementDlgState g_pl;

const char* pl_key(PlacementKind kind) {
    return kind == PlacementKind::Npc ? "npcs" : kind == PlacementKind::Sign ? "events" : "objects";
}

JsonValue* pl_array() {
    JsonValue* a = g_pl.work.find(pl_key(g_pl.kind));
    return a && a->is_arr() ? a : nullptr;
}

JsonValue* pl_current() {
    JsonValue* a = pl_array();
    if (!a || g_pl.index < 0 || g_pl.index >= static_cast<int>(a->arr.size())) return nullptr;
    return &a->arr[static_cast<size_t>(g_pl.index)];
}

std::string pl_text(int id) {
    HWND control = GetDlgItem(g_pl.win, id);
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<size_t>(std::max(0, length)) + 1, L'\0');
    const int copied = GetWindowTextW(control, text.data(), static_cast<int>(text.size()));
    text.resize(static_cast<size_t>(std::max(0, copied)));
    return wide_to_utf8(text);
}

void pl_set_text(int id, const std::string& text) {
    SetDlgItemTextW(g_pl.win, id, utf8_to_wide(text).c_str());
}

void pl_types(const std::string& current) {
    HWND combo = GetDlgItem(g_pl.win, IDPL_TYPE);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    g_pl.types.clear();
    for (const Prefab& p : g.prefabs) {
        g_pl.types.push_back(p.id);
        const std::wstring label = utf8_to_wide(p.name + " (" + p.id + ")");
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
    if (!current.empty() && std::find(g_pl.types.begin(), g_pl.types.end(), current) == g_pl.types.end()) {
        g_pl.types.push_back(current);
        const std::wstring label = utf8_to_wide(current + " (unbekannt)");
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
    const auto it = std::find(g_pl.types.begin(), g_pl.types.end(), current);
    const int sel = it == g_pl.types.end() ? -1 : static_cast<int>(it - g_pl.types.begin());
    SendMessageW(combo, CB_SETCURSEL, sel, 0);
}

PlacementForm pl_form() {
    PlacementForm f = g_pl.before;
    if (g_pl.name_editable) f.name = pl_text(IDPL_NAME);
    f.x = pl_text(IDPL_X);
    f.y = pl_text(IDPL_Y);
    f.z = pl_text(IDPL_Z);
    f.speed = pl_text(IDPL_SPEED);
    f.radius = pl_text(IDPL_RADIUS);
    f.rotation = pl_text(IDPL_ROT);
    if (g_pl.path_editable) f.path = pl_text(IDPL_PATH);
    if (g_pl.text_editable) f.text = pl_text(IDPL_TEXT);
    const int type = static_cast<int>(SendDlgItemMessageW(g_pl.win, IDPL_TYPE, CB_GETCURSEL, 0, 0));
    if (type >= 0 && type < static_cast<int>(g_pl.types.size())) f.type = g_pl.types[static_cast<size_t>(type)];
    return f;
}

void pl_refresh_list() {
    const bool old_sync = g_pl.syncing;
    g_pl.syncing = true;
    HWND list = GetDlgItem(g_pl.win, IDPL_LIST);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    g_pl.rows.clear();
    JsonValue* a = pl_array();
    if (a) {
        for (size_t i = 0; i < a->arr.size(); ++i) {
            const JsonValue& item = a->arr[i];
            if (g_pl.kind == PlacementKind::Sign) {
                const JsonValue* visual = item.is_obj() ? item.find("visual") : nullptr;
                if (!visual || !visual->is_str() || visual->str != "schild") continue;
            }
            std::string label = "#" + std::to_string(i + 1) + "  ";
            if (!item.is_obj()) label += "[ungültiger Eintrag]";
            else if (g_pl.kind == PlacementKind::Object) {
                const JsonValue* type = item.find("type");
                const std::string id = type && type->is_str() ? type->str : "?";
                const Prefab* prefab = prefab_of(id);
                label += prefab ? prefab->name : id;
            } else {
                const JsonValue* name = item.find("name");
                label += name && name->is_str() ? name->str : "[ohne Namen]";
            }
            const std::wstring w = utf8_to_wide(label);
            SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(w.c_str()));
            g_pl.rows.push_back(static_cast<int>(i));
        }
    }
    for (size_t row = 0; row < g_pl.rows.size(); ++row) {
        if (g_pl.rows[row] == g_pl.index) {
            SendMessageW(list, LB_SETCURSEL, static_cast<WPARAM>(row), 0);
            break;
        }
    }
    g_pl.syncing = old_sync;
}

void pl_load_fields() {
    g_pl.syncing = true;
    const JsonValue* item = pl_current();
    const bool valid = item && item->is_obj();
    g_pl.before = valid ? placement_form_read(*item, g_pl.kind) : PlacementForm{};
    const PlacementForm& f = g_pl.before;
    pl_set_text(IDPL_NAME, f.name);
    pl_set_text(IDPL_X, f.x);
    pl_set_text(IDPL_Y, f.y);
    pl_set_text(IDPL_Z, f.z);
    pl_set_text(IDPL_SPEED, f.speed);
    pl_set_text(IDPL_RADIUS, f.radius);
    pl_set_text(IDPL_ROT, f.rotation);
    pl_set_text(IDPL_PATH, f.path);
    pl_set_text(IDPL_TEXT, f.text);
    pl_types(f.type);
    const bool npc = g_pl.kind == PlacementKind::Npc;
    const bool sign = g_pl.kind == PlacementKind::Sign;
    const bool obj = g_pl.kind == PlacementKind::Object;
    // Alte Daten mit NUL, nicht darstellbarem UTF-8 oder zu langen Texten
    // dürfen durch Windows-Edit-Limits nicht still verkürzt werden.
    g_pl.name_editable = valid && !obj && pl_text(IDPL_NAME) == f.name;
    g_pl.path_editable = valid && npc && pl_text(IDPL_PATH) == f.path;
    g_pl.text_editable = valid && !obj && f.text_editable && pl_text(IDPL_TEXT) == f.text;
    ShowWindow(GetDlgItem(g_pl.win, IDPL_NAME), obj ? SW_HIDE : SW_SHOW);
    ShowWindow(GetDlgItem(g_pl.win, IDPL_TYPE), obj ? SW_SHOW : SW_HIDE);
    EnableWindow(GetDlgItem(g_pl.win, IDPL_NAME), g_pl.name_editable);
    EnableWindow(GetDlgItem(g_pl.win, IDPL_TYPE), valid && obj);
    for (int id : {IDPL_X, IDPL_Y, IDPL_Z, IDPL_APPLY}) EnableWindow(GetDlgItem(g_pl.win, id), valid);
    EnableWindow(GetDlgItem(g_pl.win, IDPL_CURSOR), valid && g.hover_valid);
    EnableWindow(GetDlgItem(g_pl.win, IDPL_SPEED), valid && npc);
    EnableWindow(GetDlgItem(g_pl.win, IDPL_RADIUS), valid && !obj);
    EnableWindow(GetDlgItem(g_pl.win, IDPL_ROT), valid && !npc);
    EnableWindow(GetDlgItem(g_pl.win, IDPL_PATH), g_pl.path_editable);
    EnableWindow(GetDlgItem(g_pl.win, IDPL_TEXT), g_pl.text_editable);
    EnableWindow(GetDlgItem(g_pl.win, IDPL_DEL), item != nullptr);
    const wchar_t* hint = !item ? L"Eintrag wählen oder mit Neu anlegen."
        : !valid ? L"Ungültiger Eintrag: unverändert lassen oder ausdrücklich löschen."
        : !obj && !g_pl.name_editable ? L"Name nicht darstellbar; er bleibt unverändert."
        : npc && !g_pl.path_editable ? L"Wegpunkte nicht darstellbar; sie bleiben unverändert."
        : npc && !f.path_valid ? L"Ungültige Wegpunkte: bleiben erhalten, bis sie bearbeitet werden."
        : !obj && !g_pl.text_editable ? L"Text nicht darstellbar oder komplex; er bleibt unverändert."
        : sign ? L"Schild ist ein normales Aktions-Event; weitere Befehle bleiben erhalten."
        : npc ? L"Ein Wegpunkt pro Zeile: X; Z. Weitere NPC-Befehle bleiben erhalten."
        : L"Y ist der Versatz zum Gelände. Die Einträge werden erst mit OK übernommen.";
    SetDlgItemTextW(g_pl.win, IDPL_HINT, hint);
    g_pl.syncing = false;
}

int pl_field_id(const std::string& field) {
    if (field == "name") return IDPL_NAME;
    if (field == "type") return IDPL_TYPE;
    if (field == "x") return IDPL_X;
    if (field == "y") return IDPL_Y;
    if (field == "z") return IDPL_Z;
    if (field == "speed") return IDPL_SPEED;
    if (field == "radius") return IDPL_RADIUS;
    if (field == "rot") return IDPL_ROT;
    if (field == "path") return IDPL_PATH;
    if (field == "text") return IDPL_TEXT;
    return IDPL_LIST;
}

// Validieren, bevor der Entwurf angefasst wird; Feldwechsel darf nicht
// stillschweigend ungültige Eingaben oder unbekannte JSON-Daten vernichten.
bool pl_apply() {
    JsonValue* current = pl_current();
    if (!current || !current->is_obj()) return true;
    const PlacementForm form = pl_form();
    PlacementEdit edit = placement_form_prepare(*current, g_pl.kind, g_pl.before, form);
    if (!edit.ok) {
        MessageBoxW(g_pl.win, utf8_to_wide(edit.error).c_str(), L"Platzier-Editor", MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(g_pl.win, pl_field_id(edit.field)));
        return false;
    }
    if (g_pl.kind != PlacementKind::Object && form.name != g_pl.before.name &&
        placement_name_used(g_pl.work, form.name, g_pl.kind, g_pl.index)) {
        MessageBoxW(g_pl.win, L"Dieser Name wird bereits von einem NPC oder Event verwendet.\n"
                              L"Bitte einen eindeutigen Namen wählen (Spielstände und Self-Switches).",
                    L"Platzier-Editor", MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(g_pl.win, IDPL_NAME));
        return false;
    }
    if (edit.replaces_invalid_path && MessageBoxW(g_pl.win,
        L"Die alte Wegpunkt-Liste ist ungültig. Durch die Bearbeitung wird sie ersetzt.\nFortfahren?",
        L"Platzier-Editor", MB_YESNO | MB_ICONWARNING) != IDYES) return false;
    if (edit.changed) *current = std::move(edit.value);
    pl_refresh_list();
    pl_load_fields();
    return true;
}

bool pl_ensure_array() {
    const char* key = pl_key(g_pl.kind);
    JsonValue* a = g_pl.work.find(key);
    if (a && a->is_arr()) return true;
    if (a && !a->is_null() && MessageBoxW(g_pl.win,
        L"Die vorhandene Liste ist ungültig. Soll sie durch eine leere Liste ersetzt werden?",
        L"Platzier-Editor", MB_YESNO | MB_ICONWARNING) != IDYES) return false;
    g_pl.work.set(key, JsonValue::make_arr());
    return true;
}

void pl_new() {
    if (!pl_apply() || !pl_ensure_array()) return;
    std::string prefab;
    if (g_pl.kind == PlacementKind::Object) {
        if (g.prefabs.empty()) {
            MessageBoxW(g_pl.win, L"Keine Objekt-Typen verfügbar.", L"Platzier-Editor", MB_OK | MB_ICONWARNING);
            return;
        }
        const int i = std::clamp(g.sel_prefab, 0, static_cast<int>(g.prefabs.size()) - 1);
        prefab = g.prefabs[static_cast<size_t>(i)].id;
    }
    JsonValue value = placement_new(g_pl.work, g_pl.kind, prefab);
    JsonValue* a = pl_array();
    a->arr.push_back(std::move(value));
    g_pl.index = static_cast<int>(a->arr.size()) - 1;
    pl_refresh_list();
    pl_load_fields();
    SetFocus(GetDlgItem(g_pl.win, g_pl.kind == PlacementKind::Object ? IDPL_TYPE : IDPL_NAME));
}

void pl_delete() {
    JsonValue* a = pl_array();
    if (!a || g_pl.index < 0 || g_pl.index >= static_cast<int>(a->arr.size())) return;
    if (MessageBoxW(g_pl.win, L"Diesen Eintrag einschließlich seiner Befehle löschen?\n"
                              L"Erst OK übernimmt die Änderung in die Karte.",
                    L"Platzier-Editor", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    a->arr.erase(a->arr.begin() + g_pl.index);
    g_pl.index = -1;
    pl_refresh_list();
    pl_load_fields();
}

bool pl_has_changes() {
    const JsonValue* item = pl_current();
    return json_write(g_pl.work) != json_write(g.map_root) ||
           (item && item->is_obj() && !placement_form_equal(g_pl.before, pl_form()));
}

void pl_cancel(HWND h) {
    if (pl_has_changes() && MessageBoxW(h, L"Alle Änderungen dieses Dialogs verwerfen?",
                                        L"Platzier-Editor", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    DestroyWindow(h);
}

void pl_accept(HWND h) {
    if (!pl_apply()) return;
    if (json_write(g_pl.work) != json_write(g.map_root)) {
        // Objektänderungen müssen im bestehenden Objekt-Undo liegen, damit
        // ältere Schnappschüsse sie nicht unbemerkt überschreiben können.
        const JsonValue* new_objects = g_pl.work.find("objects");
        if (g.objects && new_objects && json_write(*g.objects) != json_write(*new_objects)) push_undo();
        // F6 muss vor dem Öffnen geschlossen sein: seine Befehlszeiger würden
        // beim Ersetzen der Wurzel ungültig. Alle anderen Fenster sind gesperrt.
        cancel_object_drag();
        g.map_root = std::move(g_pl.work);
        g.objects = g.map_root.find("objects");
        g.sel_obj = -1;
        g.dirty = true;
        sync_object_list();
        sync_property_fields();
        issues_scan_map();
        update_title();
        status_mode(L"Platzierungen übernommen — mit Strg+S speichern");
        core::log_info("Platzierungen übernommen");
    }
    DestroyWindow(h);
}

LRESULT CALLBACK placement_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case DM_GETDEFID: return MAKELRESULT(IDPL_OK, DC_HASDEFID);
        case WM_COMMAND: {
            if (g_pl.syncing) return 0;
            const int id = LOWORD(wp), code = HIWORD(wp);
            if (id == IDPL_KIND && code == CBN_SELCHANGE) {
                const int choice = static_cast<int>(SendDlgItemMessageW(h, IDPL_KIND, CB_GETCURSEL, 0, 0));
                if (!pl_apply()) {
                    SendDlgItemMessageW(h, IDPL_KIND, CB_SETCURSEL, static_cast<WPARAM>(g_pl.kind), 0);
                    return 0;
                }
                g_pl.kind = choice == 1 ? PlacementKind::Sign : choice == 2 ? PlacementKind::Object : PlacementKind::Npc;
                g_pl.index = -1;
                pl_refresh_list();
                pl_load_fields();
                return 0;
            }
            if (id == IDPL_LIST && code == LBN_SELCHANGE) {
                const int row = static_cast<int>(SendDlgItemMessageW(h, IDPL_LIST, LB_GETCURSEL, 0, 0));
                const int target = row >= 0 && row < static_cast<int>(g_pl.rows.size()) ? g_pl.rows[static_cast<size_t>(row)] : -1;
                if (target == g_pl.index) return 0;
                if (!pl_apply()) { pl_refresh_list(); return 0; }
                g_pl.index = target;
                pl_refresh_list();
                pl_load_fields();
                return 0;
            }
            if (id == IDPL_NEW) { pl_new(); return 0; }
            if (id == IDPL_DEL) { pl_delete(); return 0; }
            if (id == IDPL_CURSOR && g.hover_valid && pl_current()) {
                pl_set_text(IDPL_X, placement_num_text(g.hover_pos.x));
                pl_set_text(IDPL_Z, placement_num_text(g.hover_pos.z));
                return 0;
            }
            if (id == IDPL_APPLY) { pl_apply(); return 0; }
            if (id == IDPL_OK || id == IDOK) { pl_accept(h); return 0; }
            if (id == IDPL_CANCEL || id == IDCANCEL) { pl_cancel(h); return 0; }
            break;
        }
        case WM_CLOSE: pl_cancel(h); return 0;
        case WM_DESTROY:
            if (g.props_dlg == h) g.props_dlg = nullptr;
            if (g_pl.win == h) g_pl.win = nullptr;
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

BOOL CALLBACK pl_collect_windows(HWND h, LPARAM param) {
    auto* windows = reinterpret_cast<std::vector<HWND>*>(param);
    if (h != g_pl.win && IsWindowEnabled(h) &&
        (h == g.main || GetWindow(h, GW_OWNER) == g.main)) windows->push_back(h);
    return TRUE;
}

void open_placement_dialog() {
    if (g.cur_map < 0 || !g.map_root.is_obj()) return;
    if (g.props_dlg) { SetForegroundWindow(g.props_dlg); return; }
    if (event_editor_open()) {
        MessageBoxW(g.main, L"Bitte zuerst den Event-Editor schließen.\n"
                            L"Übernommene Event-Änderungen bleiben in der Karte erhalten.",
                    L"Platzier-Editor", MB_OK | MB_ICONINFORMATION);
        return;
    }
    // F5 arbeitet auf einer Kartenkopie und kann beim Übernehmen die alte
    // Objektauswahl ersetzen. Drag beenden und Eingaben zuerst klären.
    cancel_object_drag();
    if (!resolve_object_edits()) return;
    g_pl = PlacementDlgState{};
    g_pl.work = g.map_root;
    WNDCLASSW wc{};
    wc.lpfnWndProc = placement_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RuneEditorPlacementDlg";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassW(&wc);
    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    const DWORD ex = WS_EX_DLGMODALFRAME;
    RECT frame{0, 0, 850, 510};
    const UINT dpi = GetDpiForWindow(g.main);
    if (!AdjustWindowRectExForDpi(&frame, style, FALSE, ex, dpi)) AdjustWindowRectEx(&frame, style, FALSE, ex);
    const int width = frame.right - frame.left, height = frame.bottom - frame.top;
    RECT owner{};
    GetWindowRect(g.main, &owner);
    int x = (owner.left + owner.right - width) / 2, y = (owner.top + owner.bottom - height) / 2;
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(MonitorFromWindow(g.main, MONITOR_DEFAULTTONEAREST), &mi)) {
        const int left = static_cast<int>(mi.rcWork.left), top = static_cast<int>(mi.rcWork.top);
        x = std::clamp(x, left, std::max(left, static_cast<int>(mi.rcWork.right) - width));
        y = std::clamp(y, top, std::max(top, static_cast<int>(mi.rcWork.bottom) - height));
    }
    g_pl.win = CreateWindowExW(ex, wc.lpszClassName, L"Platzier-Editor — NPCs, Schilder, Objekte",
                               style, x, y, width, height, g.main, nullptr, wc.hInstance, nullptr);
    if (!g_pl.win) return;
    g.props_dlg = g_pl.win;
    auto control = [&](const wchar_t* cls, const wchar_t* text, DWORD flags,
                       int cx, int cy, int cw, int ch, int id) {
        const bool interactive = wcscmp(cls, L"Static") != 0;
        HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | flags |
                                 (interactive ? WS_TABSTOP : 0), cx, cy, cw, ch, g_pl.win,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), wc.hInstance, nullptr);
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
        return c;
    };
    auto label = [&](const wchar_t* text, int cx, int cy, int w) {
        control(L"Static", text, 0, cx, cy, w, 20, 0);
    };
    label(L"Kategorie", 12, 10, 235);
    HWND kind = control(L"ComboBox", nullptr, CBS_DROPDOWNLIST | WS_VSCROLL, 12, 32, 235, 150, IDPL_KIND);
    for (const wchar_t* s : {L"NPCs", L"Schilder / Textpunkte", L"Objekte"})
        SendMessageW(kind, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));
    SendMessageW(kind, CB_SETCURSEL, 0, 0);
    control(L"ListBox", nullptr, WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS,
            12, 65, 235, 328, IDPL_LIST);
    control(L"Button", L"Neu", BS_PUSHBUTTON, 12, 402, 110, 30, IDPL_NEW);
    control(L"Button", L"Löschen", BS_PUSHBUTTON, 130, 402, 117, 30, IDPL_DEL);
    control(L"Button", L"Letzte Cursorposition", BS_PUSHBUTTON, 12, 440, 235, 30, IDPL_CURSOR);
    label(L"Name / Objekt-Typ", 270, 12, 240);
    control(L"Edit", nullptr, WS_BORDER | ES_AUTOHSCROLL, 270, 34, 560, 25, IDPL_NAME);
    control(L"ComboBox", nullptr, CBS_DROPDOWNLIST | WS_VSCROLL, 270, 34, 560, 180, IDPL_TYPE);
    label(L"X (m)", 270, 70, 90);
    label(L"Y: über Gelände (m)", 430, 70, 150);
    label(L"Z (m)", 630, 70, 90);
    control(L"Edit", nullptr, WS_BORDER | ES_AUTOHSCROLL, 270, 92, 130, 25, IDPL_X);
    control(L"Edit", nullptr, WS_BORDER | ES_AUTOHSCROLL, 430, 92, 150, 25, IDPL_Y);
    control(L"Edit", nullptr, WS_BORDER | ES_AUTOHSCROLL, 630, 92, 130, 25, IDPL_Z);
    label(L"Tempo (m/s)", 270, 125, 140);
    label(L"Radius (m)", 430, 125, 140);
    label(L"Drehung (Grad)", 630, 125, 150);
    control(L"Edit", nullptr, WS_BORDER | ES_AUTOHSCROLL, 270, 147, 130, 25, IDPL_SPEED);
    control(L"Edit", nullptr, WS_BORDER | ES_AUTOHSCROLL, 430, 147, 150, 25, IDPL_RADIUS);
    control(L"Edit", nullptr, WS_BORDER | ES_AUTOHSCROLL, 630, 147, 130, 25, IDPL_ROT);
    label(L"Wegpunkte: X; Z (eine Zeile pro Punkt, nur NPC)", 270, 182, 530);
    HWND path = control(L"Edit", nullptr, WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                        270, 205, 560, 86, IDPL_PATH);
    SendMessageW(path, EM_SETLIMITTEXT, 1000000, 0);
    label(L"Erster Text (eine Zeile pro Textzeile)", 270, 301, 520);
    HWND text = control(L"Edit", nullptr, WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                        270, 324, 560, 82, IDPL_TEXT);
    SendMessageW(text, EM_SETLIMITTEXT, 1000000, 0);
    control(L"Static", L"", 0, 270, 416, 560, 38, IDPL_HINT);
    control(L"Button", L"Eintrag übernehmen", BS_PUSHBUTTON, 270, 467, 190, 30, IDPL_APPLY);
    control(L"Button", L"OK", BS_DEFPUSHBUTTON, 624, 467, 98, 30, IDPL_OK);
    control(L"Button", L"Abbrechen", BS_PUSHBUTTON, 732, 467, 98, 30, IDPL_CANCEL);
    pl_refresh_list();
    pl_load_fields();

    // Modalität auch gegenüber bereits offenen moduslosen Nebenfenstern.
    // Nur vorher aktivierte Fenster werden beim Schließen wieder aktiviert.
    std::vector<HWND> disabled;
    EnumThreadWindows(GetCurrentThreadId(), pl_collect_windows, reinterpret_cast<LPARAM>(&disabled));
    for (HWND window : disabled) EnableWindow(window, FALSE);
    HWND dialog = g_pl.win;
    ShowWindow(dialog, SW_SHOW);
    SetForegroundWindow(dialog);
    SetFocus(kind);
    MSG msg{};
    bool quit = false;
    int quit_code = 0;
    while (IsWindow(dialog)) {
        const BOOL result = GetMessageW(&msg, nullptr, 0, 0);
        if (result <= 0) { quit = result == 0; quit_code = static_cast<int>(msg.wParam); break; }
        if (IsDialogMessageW(dialog, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (IsWindow(dialog)) DestroyWindow(dialog);
    for (HWND window : disabled) if (IsWindow(window)) EnableWindow(window, TRUE);
    if (IsWindow(g.main)) SetForegroundWindow(g.main);
    if (quit) PostQuitMessage(quit_code);
}
