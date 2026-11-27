#pragma once
// ============================================================================
//  Etappe 45: XP-Einstellfenster je Event-Befehl.
//
//  Rendert die portabel beschriebenen Felder (event_command_settings.h) als
//  passende Bedienelemente: nummerierte Listen 1..999 für Schalter/Variablen,
//  Datenbank-Auswahlen für Items/Helden/Truppen/..., Asset-Ordnerwahl für
//  Audio/Bilder, Combos für feste Auswahlen, An/Aus, geprüfte Zahlen, Text.
//
//  Serialisierung NICHT neu erfunden: beim Übernehmen werden die Werte in die
//  bestehenden F6-Formularcontrols geschrieben und der getestete Pfad
//  ev_insert_command()/ev_update_command() (event_command_prepare, verlustfreies
//  Mergen, Validierung, Undo) ausgelöst. So bleibt jeder geschriebene Befehl
//  für die echte Engine lesbar.
//
//  Eingebunden in editor_main.cpp innerhalb namespace rune::editor, NACHDEM
//  g_ev, EV_TYPE_LABELS, ev_cmd_visibility, ev_insert/update_command definiert
//  sind. Sicht-/DPI-Test nur auf Windows.
// ============================================================================

#include "editor/event_command_settings.h"
#include "editor/event_numbered_registry.h"
#include "editor/event_asset_browser_win.h"

enum {
    IDECS_FIELD_BASE = 8500,   // + feld*4: 0=control 1=name-edit 2=browse/name-button
    IDECS_OK         = 8600,
    IDECS_CANCEL     = 8601,
};

struct EcsFieldCtl {
    EcSetField spec;
    HWND control = nullptr;
    HWND name_edit = nullptr;   // nur Numbered*
    HWND button = nullptr;      // Asset browse / Numbered name
};

struct EcsState {
    HWND win = nullptr, ok = nullptr;
    int type = 0;
    bool is_update = false;
    EventCommandForm initial;
    EcsFieldCtl fields[EC_SET_MAX_FIELDS];
    int field_count = 0;
    NumberedRegistry switches, variables;
    JsonValue db;
    bool db_loaded = false;
    bool registry_dirty = false;
};
static EcsState g_ecs;

static HWND ecs_control(const wchar_t* cls, const wchar_t* text, DWORD style,
                        int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                             ui_px(x), ui_px(y), ui_px(w), ui_px(h), g_ecs.win,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(ui_dialog_font()), TRUE);
    return c;
}

static void ecs_load_db() {
    g_ecs.db_loaded = true;
    g_ecs.db = JsonValue::make_obj();
    std::string text;
    if (read_file(db_path(), text)) {
        JsonValue parsed = json_parse(text);
        if (parsed.is_obj()) g_ecs.db = std::move(parsed);
    }
    g_ecs.switches = NumberedRegistry::from_json(g_ecs.db.find("switches"));
    g_ecs.variables = NumberedRegistry::from_json(g_ecs.db.find("variables"));
}

static void ecs_save_registry() {
    if (!g_ecs.registry_dirty) return;
    g_ecs.db.set("switches", g_ecs.switches.to_json());
    g_ecs.db.set("variables", g_ecs.variables.to_json());
    write_file(db_path(), json_write(g_ecs.db));
    g_ecs.registry_dirty = false;
}

// Namen einer Datenbank-Sammlung für Ref-Auswahlen.
static std::vector<std::string> ecs_ref_names(const char* collection) {
    std::vector<std::string> out;
    const JsonValue* arr = g_ecs.db.find(collection ? collection : "");
    if (!arr || !arr->is_arr()) return out;
    for (size_t i = 0; i < arr->arr.size(); ++i) {
        const JsonValue* e = &arr->arr[i];
        const JsonValue* n = e->is_obj() ? e->find("name") : nullptr;
        out.push_back(n && n->is_str() ? n->str : std::to_string(i + 1));
    }
    return out;
}

static std::string ecs_slot_string(const EventCommandForm& f, int slot) {
    switch (slot) {
        case 0: return f.name; case 1: return f.val1; case 2: return f.val2;
        case 3: return f.val3; case 4: return f.text; default: return {};
    }
}

static void ecs_set_slot_string(EventCommandForm& f, int slot, const std::string& v) {
    switch (slot) {
        case 0: f.name = v; break; case 1: f.val1 = v; break; case 2: f.val2 = v; break;
        case 3: f.val3 = v; break; case 4: f.text = v; break; default: break;
    }
}

static void ecs_build() {
    using C = EcSetControl;
    int y = 12;
    for (int i = 0; i < g_ecs.field_count; ++i) {
        EcsFieldCtl& fc = g_ecs.fields[i];
        const EcSetField& s = fc.spec;
        const int base = IDECS_FIELD_BASE + i * 4;
        const std::wstring label = utf8_to_wide(s.label);
        ecs_control(L"Static", label.c_str(), 0, 12, y + 3, 150, 18, 0);
        const std::string cur = ecs_slot_string(g_ecs.initial, s.slot);
        const int cx = 168, cw = 300;
        switch (s.control) {
            case C::NumberedSwitch: case C::NumberedVariable: {
                NumberedRegistry& reg = s.control == C::NumberedSwitch ? g_ecs.switches : g_ecs.variables;
                fc.control = ecs_control(L"ComboBox", nullptr, CBS_DROPDOWNLIST | WS_VSCROLL,
                                         cx, y, 150, 400, base);
                for (int n = 1; n <= EC_NUMBERED_MAX; ++n)
                    SendMessageW(fc.control, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(utf8_to_wide(reg.label(n)).c_str()));
                int num = reg.number_of(cur);
                SendMessageW(fc.control, CB_SETCURSEL, num >= 1 ? num - 1 : 0, 0);
                fc.name_edit = ecs_control(L"Edit", utf8_to_wide(reg.name_of(num >= 1 ? num : 1)).c_str(),
                                           WS_BORDER | ES_AUTOHSCROLL, cx + 156, y, 144, 22, base + 1);
                y += 28;
                break;
            }
            case C::Ref: {
                fc.control = ecs_control(L"ComboBox", nullptr, CBS_DROPDOWNLIST | WS_VSCROLL, cx, y, cw, 400, base);
                std::vector<std::string> names = ecs_ref_names(s.collection);
                int sel = 0;
                for (size_t k = 0; k < names.size(); ++k) {
                    SendMessageW(fc.control, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(utf8_to_wide(names[k]).c_str()));
                    if (names[k] == cur) sel = static_cast<int>(k);
                }
                SendMessageW(fc.control, CB_SETCURSEL, names.empty() ? -1 : sel, 0);
                y += 28;
                break;
            }
            case C::Asset: {
                fc.control = ecs_control(L"ComboBox", utf8_to_wide(cur).c_str(), CBS_DROPDOWN | WS_VSCROLL, cx, y, cw - 96, 400, base);
                fc.button = ecs_control(L"Button", L"Durchsuchen…", BS_PUSHBUTTON, cx + cw - 90, y - 1, 90, 24, base + 2);
                y += 28;
                break;
            }
            case C::Combo: {
                fc.control = ecs_control(L"ComboBox", nullptr, CBS_DROPDOWNLIST | WS_VSCROLL, cx, y, cw, 400, base);
                if (s.slot == 5) {
                    for (const wchar_t* o : EV_OPS) SendMessageW(fc.control, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(o));
                    SendMessageW(fc.control, CB_SETCURSEL, g_ecs.initial.op, 0);
                } else if (s.slot == 7) {
                    for (const wchar_t* o : EV_SOURCES) SendMessageW(fc.control, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(o));
                    SendMessageW(fc.control, CB_SETCURSEL, g_ecs.initial.source, 0);
                } else {
                    int sel = 0;
                    for (int k = 0; k < s.option_count; ++k) {
                        SendMessageW(fc.control, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(utf8_to_wide(s.options[k]).c_str()));
                        if (cur == s.options[k]) sel = k;
                    }
                    SendMessageW(fc.control, CB_SETCURSEL, s.option_count ? sel : -1, 0);
                }
                y += 28;
                break;
            }
            case C::Bool: {
                fc.control = ecs_control(L"ComboBox", nullptr, CBS_DROPDOWNLIST, cx, y, 100, 200, base);
                SendMessageW(fc.control, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"an"));
                SendMessageW(fc.control, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"aus"));
                SendMessageW(fc.control, CB_SETCURSEL, g_ecs.initial.flag, 0);
                y += 28;
                break;
            }
            case C::Multiline: {
                fc.control = ecs_control(L"Edit", utf8_to_wide(cur).c_str(),
                                         WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, cx, y, cw, 84, base);
                y += 90;
                break;
            }
            default: { // Text / Int / Number
                fc.control = ecs_control(L"Edit", utf8_to_wide(cur).c_str(), WS_BORDER | ES_AUTOHSCROLL, cx, y, cw, 22, base);
                y += 28;
                break;
            }
        }
    }
    const int by = y + 8;
    g_ecs.ok = ecs_control(L"Button", L"Übernehmen", BS_PUSHBUTTON | WS_TABSTOP, 300, by, 90, 26, IDECS_OK);
    ecs_control(L"Button", L"Abbrechen", BS_PUSHBUTTON | WS_TABSTOP, 398, by, 90, 26, IDECS_CANCEL);
    // Fensterhöhe an Inhalt anpassen.
    RECT r{}; GetWindowRect(g_ecs.win, &r);
    const int want_h = ui_px(by + 26 + 40);
    SetWindowPos(g_ecs.win, nullptr, 0, 0, r.right - r.left, want_h, SWP_NOMOVE | SWP_NOZORDER);
}

// Nummerierte Auswahl: gewählte Nummer + Name aus dem Namensfeld.
static std::string ecs_numbered_value(EcsFieldCtl& fc) {
    const int sel = static_cast<int>(SendMessageW(fc.control, CB_GETCURSEL, 0, 0));
    const int num = sel + 1;
    wchar_t buf[256] = {};
    GetWindowTextW(fc.name_edit, buf, 256);
    std::string name = wide_to_utf8(buf);
    NumberedRegistry& reg = fc.spec.control == EcSetControl::NumberedSwitch ? g_ecs.switches : g_ecs.variables;
    if (name.empty()) {
        // Unbenannte Nummer bekommt einen sprechenden Default und wird registriert.
        char b[16]; std::snprintf(b, sizeof b, "%04d", num);
        name = (fc.spec.control == EcSetControl::NumberedSwitch ? "Schalter_" : "Variable_") + std::string(b);
    }
    if (reg.name_of(num) != name) { reg.set_name(num, name); g_ecs.registry_dirty = true; }
    return name;
}

static std::string ecs_control_string(EcsFieldCtl& fc) {
    using C = EcSetControl;
    switch (fc.spec.control) {
        case C::NumberedSwitch: case C::NumberedVariable: return ecs_numbered_value(fc);
        case C::Combo: {
            const int sel = static_cast<int>(SendMessageW(fc.control, CB_GETCURSEL, 0, 0));
            if (fc.spec.slot == 5 || fc.spec.slot == 7) return {};   // op/source separat
            if (sel >= 0 && sel < fc.spec.option_count) return fc.spec.options[sel];
            return {};
        }
        default: {
            wchar_t buf[4096] = {};
            GetWindowTextW(fc.control, buf, 4096);
            return wide_to_utf8(buf);
        }
    }
}

static void ecs_apply() {
    EventCommandForm result = g_ecs.initial;
    result.type = g_ecs.type;
    for (int i = 0; i < g_ecs.field_count; ++i) {
        EcsFieldCtl& fc = g_ecs.fields[i];
        if (fc.spec.slot == 5) { result.op = static_cast<int>(SendMessageW(fc.control, CB_GETCURSEL, 0, 0)); continue; }
        if (fc.spec.slot == 7) { result.source = static_cast<int>(SendMessageW(fc.control, CB_GETCURSEL, 0, 0)); continue; }
        if (fc.spec.control == EcSetControl::Bool) { result.flag = static_cast<int>(SendMessageW(fc.control, CB_GETCURSEL, 0, 0)); continue; }
        ecs_set_slot_string(result, fc.spec.slot, ecs_control_string(fc));
    }
    ecs_save_registry();

    // In die bestehenden F6-Controls schreiben und den getesteten Pfad auslösen.
    SendMessageW(g_ev.cb_ce_type, CB_SETCURSEL, g_ecs.type, 0);
    ev_cmd_visibility(true);
    SetWindowTextW(g_ev.ed_ce_name, utf8_to_wide(result.name).c_str());
    SetWindowTextW(g_ev.ed_ce_val1, utf8_to_wide(result.val1).c_str());
    SetWindowTextW(g_ev.ed_ce_val2, utf8_to_wide(result.val2).c_str());
    SetWindowTextW(g_ev.ed_ce_val3, utf8_to_wide(result.val3).c_str());
    SetWindowTextW(g_ev.ed_ce_text, utf8_to_wide(result.text).c_str());
    SendMessageW(g_ev.cb_ce_op, CB_SETCURSEL, result.op, 0);
    SendMessageW(g_ev.cb_ce_bool, CB_SETCURSEL, result.flag, 0);
    SendMessageW(g_ev.cb_ce_source, CB_SETCURSEL, result.source, 0);
    const bool update = g_ecs.is_update;
    DestroyWindow(g_ecs.win);
    if (update) ev_update_command(); else ev_insert_command();
}

static LRESULT CALLBACK ecs_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND: {
            const int id = LOWORD(wp), code = HIWORD(wp);
            // Nummerierte Liste: Namensfeld beim Wechsel aktualisieren.
            if (code == CBN_SELCHANGE) {
                for (int i = 0; i < g_ecs.field_count; ++i) {
                    EcsFieldCtl& fc = g_ecs.fields[i];
                    if (fc.control == reinterpret_cast<HWND>(lp) && fc.name_edit) {
                        const int sel = static_cast<int>(SendMessageW(fc.control, CB_GETCURSEL, 0, 0));
                        NumberedRegistry& reg = fc.spec.control == EcSetControl::NumberedSwitch ? g_ecs.switches : g_ecs.variables;
                        SetWindowTextW(fc.name_edit, utf8_to_wide(reg.name_of(sel + 1)).c_str());
                        return 0;
                    }
                }
                return 0;
            }
            if (code != BN_CLICKED) return 0;
            // Asset-Durchsuchen.
            for (int i = 0; i < g_ecs.field_count; ++i) {
                EcsFieldCtl& fc = g_ecs.fields[i];
                if (fc.button && id == IDECS_FIELD_BASE + i * 4 + 2 && fc.spec.control == EcSetControl::Asset) {
                    std::string key;
                    if (ec_browse_asset(h, fc.spec.collection, key))
                        SetWindowTextW(fc.control, utf8_to_wide(key).c_str());
                    return 0;
                }
            }
            if (id == IDECS_OK)     { ecs_apply(); return 0; }
            if (id == IDECS_CANCEL) { DestroyWindow(h); return 0; }
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            if (g_ecs.win == h) { g_ecs.win = nullptr; g_ecs.ok = nullptr; }
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// Öffnet das Einstellfenster. is_update=false => neuer Befehl (Einfügen).
void open_event_command_settings(int type, const EventCommandForm& initial, bool is_update) {
    if (g_ecs.win) { SetForegroundWindow(g_ecs.win); return; }
    WNDCLASSW wc{};
    wc.lpfnWndProc = ecs_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RuneEditorCmdSettings";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassW(&wc);

    g_ecs = EcsState{};
    g_ecs.type = type;
    g_ecs.is_update = is_update;
    g_ecs.initial = initial;
    ecs_load_db();
    g_ecs.field_count = ec_settings_fields(type, nullptr, 0);
    EcSetField specs[EC_SET_MAX_FIELDS];
    g_ecs.field_count = ec_settings_fields(type, specs, EC_SET_MAX_FIELDS);
    for (int i = 0; i < g_ecs.field_count; ++i) g_ecs.fields[i].spec = specs[i];

    const std::wstring title = std::wstring(L"Befehl einstellen: ") +
        (type >= 0 && type < ET_COUNT ? EV_TYPE_LABELS[type] : L"");
    g_ecs.win = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RuneEditorCmdSettings", title.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
        ui_px(500), ui_px(400), g_ev.win ? g_ev.win : g.main, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (!g_ecs.win) return;
    ecs_build();
    ShowWindow(g_ecs.win, SW_SHOW);
    SetForegroundWindow(g_ecs.win);
}
