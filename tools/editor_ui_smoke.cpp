// Windows-Integration: echte Editor-EXE mit WARP in einer isolierten Kopie.
// Synthetische Win32-Nachrichten prüfen Arbeitsabläufe, keine physische Maus,
// Lesbarkeit von Pixeln, echte DPI-Wechsel oder Hardware-GPU. Der Aufrufer
// stellt eine vollständig kopierte Engine-Wurzel bereit; Fixtures werden nur
// in einem eigens neu angelegten Projekt unter dieser Wurzel geschrieben.
#include <windows.h>
#include <shellapi.h>

#include "editor/json.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

using rune::editor::JsonValue;
using rune::editor::json_parse;

namespace {
int checks = 0, failures = 0;
DWORD child_id = 0;
HANDLE child_process = nullptr;

bool check(const char* name, bool ok) {
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", name);
    if (!ok && std::getenv("GITHUB_ACTIONS"))
        std::printf("::error title=Editor-UI-Smoke::%s\n", name);
    return ok;
}

bool alive() {
    return child_process && WaitForSingleObject(child_process, 0) == WAIT_TIMEOUT;
}

struct Search {
    const wchar_t* cls;
    const wchar_t* title;
    HWND found = nullptr;
};

BOOL CALLBACK find_owned(HWND h, LPARAM lp) {
    auto& search = *reinterpret_cast<Search*>(lp);
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != child_id) return TRUE;
    wchar_t cls[128]{};
    if (!GetClassNameW(h, cls, 128) || std::wstring(cls) != search.cls) return TRUE;
    if (search.title) {
        wchar_t title[256]{};
        GetWindowTextW(h, title, 256);
        if (std::wstring(title) != search.title) return TRUE;
    }
    search.found = h;
    return FALSE;
}

HWND find_window(const wchar_t* cls, const wchar_t* title = nullptr) {
    Search search{cls, title, nullptr};
    EnumWindows(find_owned, reinterpret_cast<LPARAM>(&search));
    return search.found;
}

template<class Predicate>
bool wait_until(Predicate predicate, DWORD timeout = 12000) {
    const ULONGLONG end = GetTickCount64() + timeout;
    do {
        if (predicate()) return true;
        if (!alive()) return false;
        Sleep(30);
    } while (GetTickCount64() < end);
    return predicate();
}

HWND wait_window(const wchar_t* cls, const wchar_t* title = nullptr, DWORD timeout = 12000) {
    HWND result = nullptr;
    wait_until([&] { result = find_window(cls, title); return result != nullptr; }, timeout);
    return result;
}

bool send(HWND h, UINT msg, WPARAM wp = 0, LPARAM lp = 0, DWORD timeout = 5000) {
    if (!h) return false;
    DWORD_PTR result = 0;
    return SendMessageTimeoutW(h, msg, wp, lp, SMTO_ABORTIFHUNG | SMTO_BLOCK,
                               timeout, &result) != 0;
}

std::wstring text(HWND h) {
    if (!h) return {};
    DWORD_PTR length = 0;
    if (!SendMessageTimeoutW(h, WM_GETTEXTLENGTH, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 5000, &length) ||
        length > 8 * 1024 * 1024) return {};
    std::wstring out(static_cast<size_t>(length) + 1, L'\0');
    DWORD_PTR copied = 0;
    if (!SendMessageTimeoutW(h, WM_GETTEXT, out.size(), reinterpret_cast<LPARAM>(out.data()),
                             SMTO_ABORTIFHUNG | SMTO_BLOCK, 5000, &copied)) return {};
    out.resize(static_cast<size_t>(copied));
    return out;
}

bool set_text(HWND h, const std::wstring& value) {
    return send(h, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(value.c_str())) && text(h) == value;
}

bool post_command(HWND h, int id, int code = BN_CLICKED, HWND sender = nullptr) {
    return h && PostMessageW(h, WM_COMMAND, MAKEWPARAM(id, code), reinterpret_cast<LPARAM>(sender)) != 0;
}

struct TextSearch { const wchar_t* needle; bool found = false; };
BOOL CALLBACK find_child_text(HWND h, LPARAM lp) {
    auto& search = *reinterpret_cast<TextSearch*>(lp);
    if (text(h).find(search.needle) != std::wstring::npos) { search.found = true; return FALSE; }
    return TRUE;
}

bool respond(const wchar_t* title, int id, const wchar_t* expected_text = nullptr) {
    HWND dialog = wait_window(L"#32770", title);
    if (!dialog) return false;
    HWND button = nullptr;
    if (!wait_until([&] { button = GetDlgItem(dialog, id); return button != nullptr; })) return false;
    if (expected_text) {
        TextSearch search{expected_text, false};
        EnumChildWindows(dialog, find_child_text, reinterpret_cast<LPARAM>(&search));
        if (!search.found) return false;
    }
    if (!PostMessageW(button, BM_CLICK, 0, 0)) return false;
    return wait_until([&] { return !IsWindow(dialog); });
}

bool create_dir(const std::wstring& path) {
    return CreateDirectoryW(path.c_str(), nullptr) != 0;
}

bool write_file(const std::wstring& path, const std::string& bytes, bool create_new) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                              create_new ? CREATE_NEW : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
                    written == bytes.size();
    CloseHandle(file);
    return ok;
}

std::string read_file(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > 1024 * 1024) {
        CloseHandle(file);
        return {};
    }
    std::string result(static_cast<size_t>(size.QuadPart), '\0');
    DWORD got = 0;
    const bool ok = ReadFile(file, result.data(), static_cast<DWORD>(result.size()), &got, nullptr) && got == result.size();
    CloseHandle(file);
    return ok ? result : std::string();
}

bool make_project(const std::wstring& root, std::wstring& project, std::wstring& data) {
    project = L"rune_ui_smoke_" + std::to_wstring(GetCurrentProcessId());
    const std::wstring base = root + L"\\projekte\\" + project;
    data = base + L"\\data";
    if (!create_dir(base) || !create_dir(data) || !create_dir(data + L"\\maps")) return false;
    return write_file(data + L"\\system.json",
        R"({"name":"UI-Test","start":{"map":"karte","x":-3,"z":4,"yaw":0.75,"custom":"bleibt"},"extra":17})", true) &&
        write_file(data + L"\\maps\\karte.json",
        R"({"name":"UI-Karte","terrain":{"size":80,"resolution":16,"amplitude":0},"objects":[],"events":[{"id":1,"name":"Event A","x":2.5,"z":0,"commands":[]},{"id":2,"name":"Event B","x":-2.5,"z":0,"commands":[]}],"npcs":[{"name":"NPC Test","x":0,"z":-3,"commands":[]}],"start":{"x":0,"z":0}})", true) &&
        write_file(data + L"\\scripts.json",
        R"({"custom":"bleibt","scripts":[{"name":"erstes","code":"wert = 1\nwert += 2\n","extra":17},{"name":"zweites","code":"# zweites\n"}]})", true);
}

bool launch(const std::wstring& root, const std::wstring& project) {
    const std::wstring exe = root + L"\\RuneEditor.exe";
    std::wstring command = L"\"" + exe + L"\" --project \"" + project + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        root.c_str(), &si, &pi)) return false;
    child_id = pi.dwProcessId;
    child_process = pi.hProcess;
    CloseHandle(pi.hThread);
    return true;
}

POINT projection(HWND view, double x, double y, double z) {
    RECT rc{};
    GetClientRect(view, &rc);
    const double width = static_cast<double>(rc.right), height = static_cast<double>(rc.bottom);
    // Anfangskamera bei 80 m flachem Terrain (load_map): (0,16,14.4),
    // yaw PI, pitch -0.5; 60 Grad vertikales Sichtfeld.
    const double dy = y - 16.0, dz = z - 14.4;
    const double f = -std::sin(0.5) * dy - std::cos(0.5) * dz;
    const double up = std::cos(0.5) * dy - std::sin(0.5) * dz;
    const double tan_half = std::tan(3.141592653589793 / 6.0);
    const double nx = -x / (f * tan_half * width / height);
    const double ny = up / (f * tan_half);
    return {static_cast<LONG>(std::lround((nx + 1) * width * 0.5)),
            static_cast<LONG>(std::lround((1 - ny) * height * 0.5))};
}

LPARAM point_lparam(POINT p) { return MAKELPARAM(static_cast<WORD>(p.x), static_cast<WORD>(p.y)); }

bool left_click(HWND view, POINT p) {
    return PostMessageW(view, WM_LBUTTONDOWN, MK_LBUTTON, point_lparam(p)) &&
           PostMessageW(view, WM_LBUTTONUP, 0, point_lparam(p));
}

bool double_click_tail(HWND view, POINT p) {
    return PostMessageW(view, WM_LBUTTONDBLCLK, MK_LBUTTON, point_lparam(p)) &&
           PostMessageW(view, WM_LBUTTONUP, 0, point_lparam(p));
}

bool test_markers(HWND main) {
    HWND view = GetDlgItem(main, 3015);
    if (!check("Viewport und Windows-Doppelklickklasse", view && (GetClassLongPtrW(view, GCL_STYLE) & CS_DBLCLKS))) return false;
    RECT r{}; GetClientRect(view, &r);
    const POINT a = projection(view, 2.5, 0.7, 0), b = projection(view, -2.5, 0.7, 0);
    const POINT npc = projection(view, 0, 0.9, -3);
    const auto inside = [&](POINT p) { return p.x >= 0 && p.y >= 0 && p.x < r.right && p.y < r.bottom; };
    if (!check("Testmarker liegen im sichtbaren Viewport", inside(a) && inside(b) && inside(npc))) return false;
    SetForegroundWindow(main);
    left_click(view, a);
    Sleep(250);
    if (!check("Einzelklick öffnet keinen Event- oder NPC-Dialog",
               !find_window(L"RuneEditorEventDlg") && !find_window(L"RuneEditorPlacementDlg"))) return false;
    double_click_tail(view, b);
    Sleep(250);
    if (!check("zweiter Klick auf anderes Event öffnet keinen Dialog", !find_window(L"RuneEditorEventDlg"))) return false;
    left_click(view, b);
    double_click_tail(view, b);
    HWND ev = nullptr;
    const bool event_selected = wait_until([&] {
        ev = find_window(L"RuneEditorEventDlg");
        return ev && text(GetDlgItem(ev, 5011)) == L"Event B";
    });
    if (!check("Doppelklick öffnet das gezielte Event B", event_selected)) return false;
    // F6 ist moduslos. Ein offener Evententwurf darf beim Wechsel zum NPC
    // weder still verschwinden noch F5 auf einer veralteten Karte öffnen.
    HWND event_name = GetDlgItem(ev, 5011);
    if (!set_text(event_name, L"Event B Entwurf")) return false;
    left_click(view, npc);
    double_click_tail(view, npc);
    if (!check("NPC-Wechsel kann bei offenem Evententwurf abgebrochen werden",
               respond(L"Event-Editor", IDCANCEL) && IsWindow(ev) &&
               !find_window(L"RuneEditorPlacementDlg") && text(event_name) == L"Event B Entwurf")) return false;
    left_click(view, npc);
    double_click_tail(view, npc);
    if (!check("Evententwurf wird erst nach ausdrücklichem Nein verworfen", respond(L"Event-Editor", IDNO))) return false;
    HWND pl = nullptr;
    const bool npc_selected = wait_until([&] {
        pl = find_window(L"RuneEditorPlacementDlg");
        return pl && !find_window(L"RuneEditorEventDlg") && text(GetDlgItem(pl, 8410)) == L"NPC Test";
    });
    if (!check("NPC-Doppelklick öffnet vorausgewählten NPC und klärt F6", npc_selected)) return false;
    post_command(pl, 8422);
    return check("NPC-Dialog lässt sich abbrechen", wait_until([&] { return !IsWindow(pl); }));
}

bool test_project_start(HWND main, const std::wstring& data) {
    HWND view = GetDlgItem(main, 3015);
    if (!view || !wait_until([&] { return IsWindowEnabled(main) != FALSE; })) return false;
    SetForegroundWindow(main);
    RECT rc{}; GetClientRect(view, &rc);
    const POINT point{rc.right / 2, rc.bottom * 4 / 5};
    const std::wstring path = data + L"\\system.json";
    const std::string before = read_file(path);
    if (!send(view, WM_RBUTTONDOWN, MK_RBUTTON, point_lparam(point))) return false;
    GUITHREADINFO info{}; info.cbSize = sizeof(info);
    const DWORD thread = GetWindowThreadProcessId(view, nullptr);
    const bool captured = GetGUIThreadInfo(thread, &info) && info.hwndCapture == view;
    POINT drag{point.x + 25, point.y + 15};
    send(view, WM_MOUSEMOVE, MK_RBUTTON, point_lparam(drag));
    send(view, WM_RBUTTONUP, 0, point_lparam(drag));
    Sleep(150);
    if (!check("Rechtsziehen hält Capture und öffnet kein Kontextmenü",
               captured && !find_window(L"#32768") && read_file(path) == before)) return false;
    // Kamera nach dem Drag synchron auf die Testposition zurücksetzen.
    if (!send(main, WM_COMMAND, 102, 0)) return false;
    PostMessageW(view, WM_RBUTTONDOWN, MK_RBUTTON, point_lparam(point));
    PostMessageW(view, WM_RBUTTONUP, 0, point_lparam(point));
    HWND menu = wait_window(L"#32768");
    if (!check("Rechtsklick öffnet das Karten-Kontextmenü", menu != nullptr)) return false;
    // Der Spielerstart ist der erste Menüeintrag. System-Menüschleife über
    // dieselben Tastennachrichten bedienen wie mit Pfeil-unten und Enter.
    PostMessageW(menu, WM_KEYDOWN, VK_DOWN, 0);
    PostMessageW(menu, WM_KEYUP, VK_DOWN, 0);
    PostMessageW(menu, WM_KEYDOWN, VK_RETURN, 0);
    PostMessageW(menu, WM_KEYUP, VK_RETURN, 0);
    JsonValue system;
    const bool saved = wait_until([&] {
        try {
            system = json_parse(read_file(path));
            const JsonValue* start = system.find("start");
            const JsonValue* flag = start ? start->find("use_project_position") : nullptr;
            return flag && flag->type == JsonValue::Type::Bool && flag->b;
        } catch (...) { return false; }
    });
    const JsonValue* start = system.find("start");
    const double nx = 2.0 * point.x / rc.right - 1.0, ny = 1.0 - 2.0 * point.y / rc.bottom;
    const double scale = std::tan(3.141592653589793 / 6.0);
    const double dx = -nx * scale * rc.right / rc.bottom;
    const double dy = -std::sin(0.5) + std::cos(0.5) * ny * scale;
    const double dz = -std::cos(0.5) - std::sin(0.5) * ny * scale;
    const double expected_x = std::round(dx * -16.0 / dy);
    const double expected_z = std::round(14.4 + dz * -16.0 / dy);
    const JsonValue* x = start ? start->find("x") : nullptr;
    const JsonValue* z = start ? start->find("z") : nullptr;
    const JsonValue* yaw = start ? start->find("yaw") : nullptr;
    const JsonValue* map = start ? start->find("map") : nullptr;
    return check("Kontextmenü speichert den gerasterten Treffer und erhält fremde Felder",
                 saved && x && z && x->is_num() && z->is_num() && x->number() == expected_x &&
                 z->number() == expected_z && yaw && yaw->number() == 0.75 && map && map->str == "karte" &&
                 start->find("custom") && system.find("extra"));
}

bool select_script(HWND dialog, HWND list, int index, const wchar_t* expected) {
    if (!send(list, LB_SETCURSEL, static_cast<WPARAM>(index), 0) ||
        !post_command(dialog, 6001, LBN_SELCHANGE, list)) return false;
    return wait_until([&] { return text(GetDlgItem(dialog, 6010)) == expected; });
}

bool test_project_switch(HWND main, const std::wstring& root, const std::wstring& data,
                         const std::wstring& project) {
    post_command(main, 108, 0);
    HWND sc = wait_window(L"RuneEditorScriptDlg");
    HWND code = nullptr;
    if (!sc || !wait_until([&] { code = GetDlgItem(sc, 6011); return code && IsWindowEnabled(code); }) ||
        !set_text(code, L"$game_variables[7] = 123\r\n")) return false;
    const std::wstring path = data + L"\\scripts.json";
    const std::wstring other = root + L"\\projekte\\dorf\\data\\scripts.json";
    const std::string untouched = read_file(other);
    post_command(main, 114, 0);
    HWND dialog = wait_window(L"RuneEditorProjectDlg");
    HWND list = nullptr;
    if (!dialog || !wait_until([&] { list = GetDlgItem(dialog, 4201); return list != nullptr; })) return false;
    DWORD_PTR row = 0;
    if (!SendMessageTimeoutW(list, LB_FINDSTRINGEXACT, static_cast<WPARAM>(-1),
                             reinterpret_cast<LPARAM>(L"dorf"), SMTO_ABORTIFHUNG | SMTO_BLOCK, 5000, &row) ||
        static_cast<LRESULT>(row) == LB_ERR || !send(list, LB_SETCURSEL, row, 0)) return false;
    post_command(dialog, 4203);
    if (!check("Abbrechen beim Projektwechsel erhält Projekt und F11-Entwurf",
               respond(L"RGSS Script-Editor", IDCANCEL) && IsWindow(sc) &&
               text(code).find(L"123") != std::wstring::npos && text(main).find(project) != std::wstring::npos)) return false;
    post_command(dialog, 4203);
    if (!check("Projektwechsel speichert den F11-Entwurf im alten Projekt",
               respond(L"RGSS Script-Editor", IDYES) &&
               wait_until([&] { return !IsWindow(sc) && !IsWindow(dialog) && text(main).find(L"dorf") != std::wstring::npos; }) &&
               read_file(path).find("$game_variables[7] = 123") != std::string::npos && read_file(other) == untouched)) return false;
    return true;
}

bool test_scripts(HWND main, const std::wstring& data) {
    post_command(main, 108, 0);
    HWND sc = wait_window(L"RuneEditorScriptDlg");
    if (!check("F11 öffnet den echten RGSS-Editor", sc != nullptr)) return false;
    HWND list = nullptr, code = nullptr, gutter = nullptr, name = nullptr, cursor = nullptr;
    const bool controls_ready = wait_until([&] {
        list = GetDlgItem(sc, 6001); code = GetDlgItem(sc, 6011); gutter = GetDlgItem(sc, 6019);
        name = GetDlgItem(sc, 6010); cursor = GetDlgItem(sc, 6020);
        return list && code && gutter && name && cursor && IsWindowEnabled(code) && text(name) == L"erstes";
    });
    if (!check("Code, Zeilenspalte und Projektliste sind vorhanden", controls_ready)) return false;
    RECT gc{}, ec{}; GetWindowRect(gutter, &gc); GetWindowRect(code, &ec);
    if (!check("Zeilenspalte liegt direkt links neben dem Code",
               gc.right == ec.left && gc.top == ec.top && gc.bottom == ec.bottom && gc.right > gc.left)) return false;
    std::wstring long_code;
    for (int i = 0; i < 79; ++i) long_code += L"# Zeile\r\n";
    long_code += L"$game_variables[7] = 42";
    if (!check("mehrzeiliger RGSS-Code kann eingegeben werden", set_text(code, long_code))) return false;
    send(code, EM_SETSEL, static_cast<WPARAM>(long_code.size()), static_cast<LPARAM>(long_code.size()));
    send(code, EM_SCROLLCARET);
    PostMessageW(code, WM_KEYUP, VK_END, 0);
    const bool line_ok = wait_until([&] { return text(cursor).find(L"Zeile 80") != std::wstring::npos; });
    DWORD_PTR lines = 0, first = 0;
    SendMessageTimeoutW(code, EM_GETLINECOUNT, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 5000, &lines);
    SendMessageTimeoutW(code, EM_GETFIRSTVISIBLELINE, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 5000, &first);
    if (!check("Zeilenzahl, Cursor und vertikales Scrollen stimmen", line_ok && lines == 80 && first > 0)) return false;
    const std::wstring edited = L"$game_variables[7] = 42\r\n# \u00c4nderung\r\n";
    if (!set_text(code, edited)) return false;
    send(code, EM_SETSEL, 0, 0);
    if (!send(code, WM_CHAR, L'\t', 0)) return false;
    const std::wstring indented = L"  " + edited;
    if (!check("Tab rückt Ruby-Code ein", text(code) == indented)) return false;
    if (!check("Auswahlwechsel übernimmt den Entwurf und zeigt das zweite Script",
               select_script(sc, list, 1, L"zweites"))) return false;
    if (!check("Rückwechsel erhält den ungespeicherten Code",
               select_script(sc, list, 0, L"erstes") && text(code) == indented)) return false;
    post_command(sc, 6018); // Syntax prüfen, ohne Ausführung
    if (!check("gültige Ruby-Syntax wird in einer Meldung bestätigt",
               respond(L"Ruby-Syntax", IDOK, L"Ruby-Syntax gültig"))) return false;
    if (!set_text(code, L"if\r\n")) return false;
    post_command(sc, 6018);
    if (!check("Syntaxfehler wird mit Dateiname und Zeile angezeigt",
               respond(L"Ruby-Syntax", IDOK, L"F11:"))) return false;
    if (!set_text(code, indented)) return false;
    HWND autoload = GetDlgItem(sc, 6017);
    send(autoload, BM_SETCHECK, BST_CHECKED, 0);
    post_command(sc, 6017, BN_CLICKED, autoload);
    post_command(sc, 6013);
    const std::wstring path = data + L"\\scripts.json";
    bool saved = wait_until([&] { return read_file(path).find("$game_variables[7] = 42") != std::string::npos; });
    JsonValue doc;
    try { doc = json_parse(read_file(path)); } catch (...) { saved = false; }
    const JsonValue* scripts = doc.find("scripts");
    const JsonValue* item = scripts && scripts->is_arr() && !scripts->arr.empty() ? &scripts->arr[0] : nullptr;
    const JsonValue* flag = item ? item->find("autoload") : nullptr;
    if (!check("Speichern erhält Zusatzdaten und aktiviert RGSS-Autoload",
               saved && doc.find("custom") && item && item->find("extra") && flag && flag->b)) return false;
    const std::string baseline = read_file(path);
    const std::wstring draft = L"$game_variables[7] = 99\r\n";
    if (!set_text(code, draft)) return false;
    const std::string external = R"({"scripts":[{"name":"extern","code":"# extern"}],"external":true})";
    if (!write_file(path, external, false)) return false;
    post_command(sc, 6013);
    if (!check("externer Dateikonflikt kann abgelehnt werden", respond(L"Script-Dateikonflikt", IDNO) &&
               read_file(path) == external && text(code) == draft)) return false;
    PostMessageW(sc, WM_CLOSE, 0, 0);
    if (!check("Abbrechen beim Schließen erhält Fenster und Entwurf",
               respond(L"RGSS Script-Editor", IDCANCEL) && IsWindow(sc) && text(code) == draft)) return false;
    if (!write_file(path, "{", false)) return false;
    post_command(sc, 6014);
    if (!check("Reload fragt vor Verwerfen und meldet beschädigtes JSON",
               respond(L"Scripts neu laden", IDYES) && respond(L"Scripts nicht geladen", IDOK) &&
               text(code) == draft && read_file(path) == "{")) return false;
    if (!write_file(path, baseline, false)) return false;
    PostMessageW(sc, WM_CLOSE, 0, 0);
    if (!check("Schließen mit Ja speichert den erhaltenen Entwurf",
               respond(L"RGSS Script-Editor", IDYES) && wait_until([&] { return !IsWindow(sc); }) &&
               read_file(path).find("$game_variables[7] = 99") != std::string::npos)) return false;
    return true;
}

} // namespace

int main() {
    std::puts("== Windows: echter Editor, synthetische UI-Arbeitsabläufe ==");
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc != 2) {
        if (argv) LocalFree(argv);
        check("isolierte Paketwurzel übergeben", false);
        return 1;
    }
    const std::wstring root = argv[1];
    LocalFree(argv);
    std::wstring project, data;
    if (!check("eigenes Projekt in der Paketkopie angelegt", make_project(root, project, data)) ||
        !check("Editor-Prozess gestartet", launch(root, project))) return 1;
    HWND main = wait_window(L"RuneEditorMain", nullptr, 90000);
    const bool ready = main && wait_until([&] {
        return IsWindowVisible(main) && text(main).find(project) != std::wstring::npos;
    }, 90000);
    if (check("Editor startet mit dem isolierten Testprojekt", ready)) {
        if (check("Markerablauf vollständig", test_markers(main)) &&
            check("Spielerstart-Ablauf vollständig", test_project_start(main, data)) &&
            check("F11-Ablauf vollständig", test_scripts(main, data)))
            check("Projektwechsel-Ablauf vollständig", test_project_switch(main, root, data, project));
    }
    if (alive()) {
        PostMessageW(main, WM_CLOSE, 0, 0);
        if (WaitForSingleObject(child_process, 10000) != WAIT_OBJECT_0) TerminateProcess(child_process, 1);
        WaitForSingleObject(child_process, 10000);
    }
    CloseHandle(child_process);
    std::printf("EDITOR-UI-SMOKE: %d Prüfungen, %d Fehler\n", checks, failures);
    return failures ? 1 : 0;
}
