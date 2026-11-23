// Windows-Integration für dieselben Pfad-/Scan-/Datei- und F4-Helfer wie
// Editor und Spiel. Eigene Temp-Daten, echte Win32-W-APIs und Kindprozesse;
// keine interaktive Editor-/Spiel- oder Hardware-GPU-Prüfung.
#include "core/project_assets_win.h"
#include "core/win_path.h"
#include "core/win_project_cli.h"
#include "editor/atomic_file_win.h"
#include "editor/database_file_win.h"
#include "editor/database_form.h"
#include "editor/listbox_win.h"
#include "editor/map_document.h"
#include "editor/playtest_win.h"
#include "editor/project_scan_win.h"

#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace paths = rune::core::win_path;
namespace cli = rune::core::win_project_cli;
using namespace rune::editor;

int checks = 0, failures = 0;
void check(const char* name, bool ok) {
    const unsigned long error = GetLastError();
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", name);
    if (!ok && std::getenv("GITHUB_ACTIONS"))
        std::printf("::error title=Windows-Pfadtest::%s (letzter Win32-Status %lu)\n", name, error);
}

std::wstring project_name() { return L" Projekt \u00e4 \u65e5 \U0001f680"; }

bool same_path(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()),
                                b.c_str(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}

std::wstring extended_path(const std::wstring& path) {
    if (path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\')
        return L"\\\\?\\UNC\\" + path.substr(2);
    return L"\\\\?\\" + path;
}

struct Fixture {
    std::wstring root;
    std::vector<std::wstring> directories, files;

    bool create_root() {
        wchar_t temp[MAX_PATH + 1]{};
        const DWORD n = GetTempPathW(MAX_PATH, temp);
        if (!n || n >= MAX_PATH) return false;
        for (int i = 0; i < 100; ++i) {
            root = std::wstring(temp, n) + L"rune-path-\u00e4-\u65e5-\U0001f680-" +
                   std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(i);
            if (CreateDirectoryW(root.c_str(), nullptr)) { directories.push_back(root); return true; }
            if (GetLastError() != ERROR_ALREADY_EXISTS) return false;
        }
        return false;
    }

    bool mkdir(const std::wstring& path) {
        if (!CreateDirectoryW(path.c_str(), nullptr)) return false;
        directories.push_back(path);
        return true;
    }

    bool put(const std::wstring& path, const std::string& data) {
        HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;
        files.push_back(path);
        DWORD written = 0;
        const bool ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) &&
                        written == data.size();
        return CloseHandle(h) && ok;
    }

    bool copy(const std::wstring& source, const std::wstring& dest) {
        if (!CopyFileW(source.c_str(), dest.c_str(), TRUE)) return false;
        files.push_back(dest);
        return true;
    }

    bool cleanup() {
        bool ok = true;
        for (auto it = files.rbegin(); it != files.rend(); ++it) {
            if (GetFileAttributesW(it->c_str()) != INVALID_FILE_ATTRIBUTES && !DeleteFileW(it->c_str())) ok = false;
        }
        for (auto it = directories.rbegin(); it != directories.rend(); ++it) {
            if (!RemoveDirectoryW(it->c_str())) ok = false;
        }
        files.clear(); directories.clear();
        return ok;
    }
};

cli::ProjectArgument parse(std::vector<std::wstring> args, std::string& result) {
    std::vector<wchar_t*> pointers;
    for (auto& arg : args) pointers.push_back(arg.data());
    return cli::parse_project(static_cast<int>(pointers.size()), pointers.data(), result);
}

// Dieses Programm wird als Rune3D.exe kopiert und über den echten F4-Helfer
// mit einem Unicode-Projekt und eigenem CWD gestartet. Es bestätigt dieselbe
// Windows-Argument-/Umgebungsgrenze wie main.cpp, startet aber KEINE GUI.
int child() {
    std::string project;
    std::wstring wide, cwd, resolved;
    if (cli::current_project(project) != cli::ProjectArgument::Present ||
        !paths::to_wide(project, wide) || wide != project_name() ||
        !cli::set_project_environment(project)) return 2;
    wchar_t env[512]{};
    if (!GetEnvironmentVariableW(L"RUNE_PROJECT", env, 512) || std::wstring(env) != project_name()) return 3;
    if (!paths::current_directory(cwd) || !paths::find_engine_root(false, project) ||
        !paths::to_wide(project, resolved) || !same_path(cwd, resolved)) return 4;
    if (!paths::find_engine_root(true, project) || !paths::to_wide(project, resolved) ||
        !same_path(cwd, resolved)) return 5;
    return 0;
}

bool run_playtest_child(const std::string& root, const std::string& project, bool nested) {
    std::wstring exe, command, cwd;
    if (!playtest_command(root, project, exe, command, cwd)) return false;
    const bool selected_nested = exe.find(L"\\build\\Rune3D.exe") != std::wstring::npos;
    if (selected_nested != nested) return false;
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, cwd.c_str(), &si, &pi)) return false;
    const DWORD wait = WaitForSingleObject(pi.hProcess, 15000);
    if (wait != WAIT_OBJECT_0) { TerminateProcess(pi.hProcess, 1); WaitForSingleObject(pi.hProcess, 5000); }
    DWORD exit_code = 1;
    const bool ok = wait == WAIT_OBJECT_0 && GetExitCodeProcess(pi.hProcess, &exit_code) && exit_code == 0;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::strcmp(argv[1], "--project") == 0) return child();
    std::puts("== Windows: UTF-8/UTF-16-Projektpfade ==");
    std::printf("  ... Windows-Systemcodepage: %u\n", static_cast<unsigned>(GetACP()));

    const std::wstring sample = L"\u00e4 \u65e5 \U0001f680";
    std::string utf8;
    std::wstring roundtrip;
    check("Umlaut, CJK und ergänzendes Unicode ohne Verlust konvertiert",
          paths::to_utf8(sample, utf8) && paths::to_wide(utf8, roundtrip) && roundtrip == sample);
    // UTF-8 und GB18030 können alle Unicode-Zeichen darstellen. Bei üblichen
    // älteren ACPs ausdrücklich nachweisen, dass dieser Test darüber hinausgeht.
    if (GetACP() != CP_UTF8 && GetACP() != 54936) {
        BOOL substituted = FALSE;
        const int count = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, sample.data(),
                                              static_cast<int>(sample.size()), nullptr, 0, nullptr, &substituted);
        check("Testnamen enthalten Zeichen außerhalb der aktiven Systemcodepage", count > 0 && substituted);
    }
    roundtrip = L"vorher";
    check("ungültiges UTF-8 wird ohne Ersatzzeichen abgewiesen",
          !paths::to_wide(std::string("\xc0\xaf", 2), roundtrip) && roundtrip.empty());
    check("eingebettetes NUL im Dateipfad wird abgewiesen",
          !paths::to_wide(std::string("a\0b", 3), roundtrip));
    utf8 = "vorher";
    const std::wstring unpaired(1, static_cast<wchar_t>(0xd800));
    check("vereinzeltes UTF-16-Surrogat wird abgewiesen",
          !paths::to_utf8(unpaired, utf8) && utf8.empty());
    const std::wstring long_project(140, L'\u754c');
    check("Projektkomponente nach UTF-16 statt UTF-8-Bytelänge begrenzt",
          paths::to_utf8(long_project, utf8) && utf8.size() > 255 && rune::assets::component(long_project));
    check("Projektpfadflucht und Windows-Gerätenamen bleiben abgewiesen",
          !rune::assets::component(L"..") && !rune::assets::component(L"A\\B") &&
          !rune::assets::component(L"CON.txt") && !rune::assets::component(L"COM\u00b9"));

    std::string project_utf8;
    paths::to_utf8(project_name(), project_utf8);
    std::string parsed;
    check("Unicode-/Leerzeichen-Projektargument wird exakt gelesen",
          parse({L"Rune3D.exe", L"--project", project_name()}, parsed) == cli::ProjectArgument::Present &&
          parsed == project_utf8);
    check("fehlendes und doppeltes Projektargument werden abgewiesen",
          parse({L"Rune3D.exe", L"--project"}, parsed) == cli::ProjectArgument::Invalid &&
          parse({L"Rune3D.exe", L"--project", L"A", L"--project", L"B"}, parsed) == cli::ProjectArgument::Invalid);
    check("ungültiges UTF-16-Projektargument wird abgewiesen",
          parse({L"Rune3D.exe", L"--project", unpaired}, parsed) == cli::ProjectArgument::Invalid);
    check("ohne Projektargument bleibt die Option unbelegt",
          parse({L"Rune3D.exe"}, parsed) == cli::ProjectArgument::Absent && parsed.empty());

    HWND list = CreateWindowExW(0, L"LISTBOX", L"", WS_POPUP | LBS_HASSTRINGS,
                                0, 0, 100, 100, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    const std::wstring label = long_project + L"\U0001f680";
    std::string expected_label, label_read;
    const bool list_ok = list && paths::to_utf8(label, expected_label) &&
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str())) == 0 &&
        listbox_text_utf8(list, 0, label_read) && label_read == expected_label;
    check("Projektliste liest auch mehr als 127 UTF-16-Zeichen vollständig", list_ok);
    if (list) DestroyWindow(list);

    Fixture f;
    if (!f.create_root()) { check("eigenes Temp-Verzeichnis", false); return 1; }
    const std::wstring game = f.root + L"\\game", build = f.root + L"\\build", log = f.root + L"\\log";
    const std::wstring outside = f.root + L"\\fremdes-cwd", projects = f.root + L"\\projekte";
    const std::wstring project = projects + L"\\" + project_name(), data = project + L"\\data";
    const std::wstring maps = data + L"\\maps", other = projects + L"\\B", other_data = other + L"\\data";
    const std::wstring map_file = maps + L"\\welt_\u57ce_\U0001f680.json";
    const std::wstring db_file = data + L"\\database.json";
    bool setup = f.mkdir(game) && f.mkdir(build) && f.mkdir(log) && f.mkdir(outside) && f.mkdir(projects) &&
                 f.mkdir(projects + L"\\OhneDaten") &&
                 f.mkdir(project) && f.mkdir(data) && f.mkdir(maps) && f.mkdir(other) && f.mkdir(other_data) &&
                 f.mkdir(maps + L"\\ordner.json") &&
                 f.put(data + L"\\system.json", R"({"name":"A"})") &&
                 f.put(other_data + L"\\system.json", R"({"name":"B"})") &&
                 f.put(map_file, R"({"name":"Alt","objects":[],"future":{"keep":7}})") &&
                 f.put(db_file, R"({"gold":4,"future":{"keep":9}})") &&
                 f.put(maps + L"\\keine-karte.txt", "fremd");
    check("isolierte Unicode-Installation mit zwei Projekten angelegt", setup);
    if (!setup) {
        check("unvollständige Testinstallation entfernt", f.cleanup());
        return 1;
    }

    std::string root_utf8, data_utf8, map_utf8, db_utf8, root_found;
    std::wstring original_cwd, selected;
    const bool have_original = paths::current_directory(original_cwd);
    const bool root_ok = have_original && paths::to_utf8(f.root, root_utf8) && paths::to_utf8(data, data_utf8) &&
                         paths::to_utf8(map_file, map_utf8) && paths::to_utf8(db_file, db_utf8);
    check("Unicode-Pfade und ursprüngliches CWD verfügbar", root_ok);
    const bool cwd_changed = root_ok && paths::set_current_directory(root_utf8);
    check("Spiel-Wurzelsuche findet Unicode-CWD mit game/",
          cwd_changed && paths::find_engine_root(true, root_found) && paths::to_wide(root_found, selected) &&
          same_path(selected, f.root));
    const auto names = project_names_on_disk(root_utf8);
    std::vector<std::string> expected_names = {project_utf8, "B"};
    std::sort(expected_names.begin(), expected_names.end());
    check("Editor-Projektscan liefert exakte Unicode-Namen", names == expected_names);
    const auto found_maps = map_files_on_disk(data_utf8);
    std::string expected_map;
    paths::to_utf8(L"welt_\u57ce_\U0001f680.json", expected_map);
    check("Editor-Kartenscan findet Unicode-Datei und ignoriert Verzeichnis/TXT",
          found_maps.size() == 1 && found_maps[0] == expected_map);

    std::string disk;
    const std::wstring marker_wide = log + L"\\last_project.txt";
    f.files.push_back(marker_wide);
    const std::string marker = root_utf8 + "\\log\\last_project.txt";
    check("Projektmerker unter Unicode-Wurzel wird unverändert als UTF-8 gelesen und geschrieben",
          paths::write_file(marker, project_utf8) && paths::read_file(marker, disk) && disk == project_utf8);
    check("ungültiger Dateipfad erzeugt keine Datei und verändert den Merker nicht",
          !paths::write_file(root_utf8 + "\\log\\" + std::string("\xff", 1), "falsch") &&
          paths::read_file(marker, disk) && disk == project_utf8);
    JsonValue map;
    const bool map_loaded = paths::read_file(map_utf8, disk) && map_parse_document(disk, map) == MapParseStatus::Ok;
    if (map_loaded) map.set("note", JsonValue::make_str("neu"));
    std::wstring map_wide;
    check("Karte unter Unicode-Pfad über echte Editor-Lese-/Schreibhilfen bearbeiten",
          map_loaded && paths::to_wide(map_utf8, map_wide) && editor_write_file_atomic(map_wide, json_write(map)) &&
          paths::read_file(map_utf8, disk) && map_parse_document(disk, map) == MapParseStatus::Ok &&
          map.find("note") && map.find("note")->str == "neu" && map.find("future"));

    std::wstring db_wide;
    JsonValue db;
    const bool db_loaded = paths::to_wide(db_utf8, db_wide) &&
        db_read_disk(db_wide, disk) == DatabaseDiskState::Ready && database_parse_document(disk, db) == DatabaseParseStatus::Ok;
    if (db_loaded) db.set("gold", JsonValue::make_num(8, true));
    check("F9-Dateileser und atomare Schreibhilfe nutzen denselben Unicode-Pfad",
          db_loaded && editor_write_file_atomic(db_wide, json_write(db)) &&
          db_read_disk(db_wide, disk) == DatabaseDiskState::Ready && database_parse_document(disk, db) == DatabaseParseStatus::Ok &&
          db.find("gold") && db.find("gold")->number() == 8 && db.find("future"));
    check("F9 unterscheidet fehlende Datei und fehlenden Elternordner",
          db_read_disk(data + L"\\fehlt.json", disk) == DatabaseDiskState::Missing &&
          db_read_disk(data + L"\\fehlt\\database.json", disk) == DatabaseDiskState::Error);

    // Mit \\?\-Präfix das Dateikomponentenlimit getrennt vom MAX_PATH-Test
    // prüfen: Die Temp-Datei darf nicht an den ohnehin langen Namen angehängt werden.
    const std::wstring long_file = extended_path(maps + L"\\" + std::wstring(245, L'a') + L".json");
    f.files.push_back(long_file);
    std::string long_utf8;
    check("Temp-Datei für langen gültigen Kartennamen bleibt kurz",
          editor_write_file_atomic(long_file, "LANG", false) && paths::to_utf8(long_file, long_utf8) &&
          paths::read_file(long_utf8, disk) && disk == "LANG");

    std::wstring current_exe(32768, L'\0');
    const DWORD exe_length = GetModuleFileNameW(nullptr, current_exe.data(), static_cast<DWORD>(current_exe.size()));
    if (exe_length && exe_length < current_exe.size()) current_exe.resize(exe_length);
    else current_exe.clear();
    const std::wstring nested = build + L"\\Rune3D.exe", adjacent = f.root + L"\\Rune3D.exe";
    const bool copied_nested = !current_exe.empty() && f.copy(current_exe, nested);
    // CWD absichtlich ändern: playtest_command muss dem Kind die Wurzel setzen.
    const bool outside_set = have_original && SetCurrentDirectoryW(outside.c_str()) != 0;
    check("F4-Kommando startet Kindprozess im build/-Layout mit exakt einem Projektargument",
          copied_nested && outside_set && run_playtest_child(root_utf8, project_utf8, true));
    const bool removed_nested = copied_nested && DeleteFileW(nested.c_str()) != 0;
    const bool copied_adjacent = removed_nested && f.copy(current_exe, adjacent);
    check("F4-Kommando unterstützt auch EXE neben game/", copied_adjacent && run_playtest_child(root_utf8, project_utf8, false));
    std::wstring ignored_exe, ignored_cmd, ignored_cwd;
    check("F4 lehnt nicht zitierbare Projektpfade ab",
          !playtest_command(root_utf8, "../anderes-projekt", ignored_exe, ignored_cmd, ignored_cwd));

    const bool restored = (!cwd_changed && !outside_set) ||
                          (have_original && SetCurrentDirectoryW(original_cwd.c_str()));
    check("ursprüngliches Arbeitsverzeichnis wiederhergestellt", restored);
    check("nur eigene Testdateien vollständig entfernt", restored && f.cleanup());
    std::printf("WINDOWS-PFADTEST: %d Prüfungen, %d Fehler\n", checks, failures);
    if (std::getenv("GITHUB_ACTIONS"))
        std::printf("::notice title=Windows-Pfadtest::%d Prüfungen, %d Fehler\n", checks, failures);
    return failures ? 1 : 0;
}
