// Verlustfreies Materialmodell und echte Windows-Dateihandles ohne GUI.
#include "editor/material_document.h"
#include "editor/texture_preview.h"
#include "world/texture_limits.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#ifdef _WIN32
#include "core/project_assets_win.h"
#endif

using namespace rune::editor;
namespace {
int failures = 0, checks = 0;
void check(const char* name, bool value) {
#ifdef _WIN32
    const unsigned long last_error = GetLastError();
#endif
    ++checks;
    if (!value) ++failures;
    std::printf("  %s  %s\n", value ? "ok  " : "FAIL", name);
    if (!value && std::getenv("GITHUB_ACTIONS")) {
#ifdef _WIN32
        std::printf("::error title=Materialtest::%s (letzter Win32-Status %lu)\n", name, last_error);
#else
        std::printf("::error title=Materialtest::%s\n", name);
#endif
    }
}

void portable_tests() {
    std::puts("== Materialdokument ==");
    const std::string raw = R"({"future":{"keep":9007199254740993},"textures":[false,{"name":"ok","file":"ok.rgba","w":4,"h":2,"tile":1.23456789,"extra":{"sicher":true}},{"name":"schlecht","file":"../a","w":1e300},42]})";
    JsonValue doc;
    check("gültiger Baum mit unbekannten und ungültigen Einträgen", material_parse(raw, doc) == MaterialParse::Ok);
    auto entries = material_entries(doc);
    check("nur sicherer Eintrag sichtbar, Originalindex erhalten", entries.size() == 1 && entries[0].document_index == 1 && entries[0].w == 4);
    const std::string before = json_write(doc);
    JsonValue edited = doc;
    check("Tile gezielt ändern", material_set_tile(edited, 1, 3.25));
    const JsonValue* list = material_array(edited);
    check("unbekannte Wurzel, falsche Einträge und Zusatzfelder erhalten",
          list && list->arr.size() == 4 && list->arr[0].type_of() == JsonValue::Type::Bool && list->arr[2].is_obj() &&
          list->arr[3].is_num() && list->arr[1].find("extra") && edited.find("future") &&
          json_write(*edited.find("future")) == json_write(*doc.find("future")));
    check("Originalbaum beim Bearbeiten unverändert", json_write(doc) == before);
    check("ungültiger Tilewert nicht übernommen", !material_set_tile(edited, 1, 0) && !material_set_tile(edited, 1, 1e100));
    check("gezieltes Entfernen löscht keine Nachbareinträge", material_remove(edited, 1) &&
          material_array(edited)->arr.size() == 3 && material_array(edited)->arr[1].find("name"));

    JsonValue replacement = doc;
    MaterialInfo m = entries[0];
    m.file = "ok_2.rgba";
    m.w = m.h = 1;
    m.source = "neu";
    check("Ersatz bearbeitet nur bekannte Bildfelder", material_upsert(replacement, m));
    const JsonValue& replaced = material_array(replacement)->arr[1];
    check("Ersatz behält unbekannte Felder und präzises Tile", replaced.find("extra") &&
          replaced.find("tile")->number() == 1.23456789 && replaced.find("file")->str == "ok_2.rgba");
    MaterialInfo invalid_old = m;
    invalid_old.name = "schlecht";
    check("Import ersetzt keinen ungültigen Alteintrag", !material_upsert(replacement, invalid_old));
    m.name = "neu";
    check("neuer Eintrag wird angehängt und Altmaterial erhalten", material_upsert(replacement, m) &&
          material_array(replacement)->arr.size() == 5 && material_array(replacement)->arr[2].find("name"));

    JsonValue untouched = json_parse("{\"vorher\":true}");
    const std::string marker = json_write(untouched);
    check("doppelte JSON-Schlüssel abgewiesen", material_parse("{\"textures\":[],\"textures\":[]}", untouched) == MaterialParse::Duplicate && json_write(untouched) == marker);
    check("doppelte Namen abgewiesen", material_parse(R"({"textures":[{"name":"x"},{"name":"x"}]})", untouched) == MaterialParse::Duplicate);
    check("nicht endliche unbekannte Zahl abgewiesen", material_parse(R"({"textures":[],"extra":1e400})", untouched) == MaterialParse::Nonfinite);
    check("falscher Listentyp nicht ersetzt", material_parse(R"({"textures":false,"extra":1})", untouched) == MaterialParse::InvalidRoot);
    check("ungültiges JSON nicht zu leerer Datei", material_parse("{", untouched) != MaterialParse::Ok && json_write(untouched) == marker);
    check("rohes Steuerzeichen abgewiesen", material_parse(std::string("{\"x\":\"a\n\"}"), untouched) == MaterialParse::InvalidJson);
    check("ungültiges UTF-8 abgewiesen", material_parse(std::string("{\"x\":\"") + char(0xff) + "\"}", untouched) == MaterialParse::InvalidUtf8);
    const std::string deep(101, '[');
    check("vor Parser auf Verschachtelung begrenzt", material_parse(deep + "0" + std::string(101, ']'), untouched) == MaterialParse::TooDeep);
    check("Klammern in Strings zählen nicht", material_nesting_safe(R"({"x":"[[[\\\"]]]"})"));
    check("Dateilimit vor Parser", material_parse(std::string(rune::world::TEXTURE_MAX_MANIFEST_BYTES + 1, ' '), untouched) == MaterialParse::TooLarge);
    JsonValue legacy;
    check("Legacy-Array unterstützt", material_parse(R"([{"name":"x","file":"x.rgba"},false])", legacy) == MaterialParse::Ok &&
          material_entries(legacy).size() == 1 && material_entries(legacy)[0].w == 64);
    const JsonValue bad = json_parse(R"([{"name":"x","file":"x.rgba","w":999999999999999999999999999999999},{"name":"y","file":"y.rgba","w":1.5},{"name":"z","file":"CON.rgba"},{"name":"q","file":"../q"}])");
    check("extreme und gebrochene Dimensionen sowie Geräte-/Pfadnamen abgewiesen", material_entries(bad).empty());
    std::size_t bytes = 0;
    check("RGBA-Rechnung vor Multiplikation begrenzt", rune::world::texture_byte_count(4096, 4096, bytes) && bytes == 64u * 1024u * 1024u &&
          !rune::world::texture_byte_count(4097, 1, bytes) && !rune::world::texture_byte_count(-1, 1, bytes));
    const std::uint8_t rgba[] = {255, 0, 0, 255, 0, 0, 255, 127};
    std::uint8_t bgra[sizeof(rgba)]{};
    texture_rgba_to_bgra(rgba, bgra, 2);
    check("F7-DIB vertauscht Rot/Blau korrekt, Alpha bleibt erhalten",
          bgra[0] == 0 && bgra[2] == 255 && bgra[3] == 255 &&
          bgra[4] == 255 && bgra[6] == 0 && bgra[7] == 127);
}

#ifdef _WIN32
using rune::assets::Handle;
using rune::assets::ProjectStore;
using rune::assets::Result;

bool put(const std::wstring& path, const std::string& data, DWORD creation = CREATE_ALWAYS) {
    Handle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, creation, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid()) return false;
    DWORD n = 0;
    return WriteFile(file.get(), data.data(), static_cast<DWORD>(data.size()), &n, nullptr) && n == data.size();
}

bool get(const std::wstring& path, std::string& out) {
    Handle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid()) return false;
    char buf[4096];
    DWORD n = 0;
    if (!ReadFile(file.get(), buf, sizeof(buf), &n, nullptr)) return false;
    out.assign(buf, n);
    return true;
}

bool make_junction(const std::wstring& link, const std::wstring& target) {
    std::wstring command = L"cmd.exe /d /c mklink /J \"" + link + L"\" \"" + target + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &si, &pi)) return false;
    const DWORD wait = WaitForSingleObject(pi.hProcess, 15000);
    if (wait != WAIT_OBJECT_0) TerminateProcess(pi.hProcess, 1);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return wait == WAIT_OBJECT_0 && code == 0;
}

struct Race { std::wstring path; bool created = false; };
void competing_create(void* ctx) {
    auto& race = *static_cast<Race*>(ctx);
    race.created = put(race.path, "EXTERN", CREATE_NEW);
}

void windows_tests() {
    std::puts("== Windows: Materialdateien und Reparsepunkte ==");
    wchar_t tmp[MAX_PATH + 1]{};
    const DWORD n = GetTempPathW(MAX_PATH, tmp);
    if (!n || n >= MAX_PATH) { check("Temp-Verzeichnis", false); return; }
    const std::wstring wroot = std::wstring(tmp, n) + L"rune-material-\u00e4-\u65e5-\U0001f680-" +
                               std::to_wstring(GetCurrentProcessId());
    std::string root;
    if (!rune::assets::to_utf8(wroot, root) || !CreateDirectoryW(wroot.c_str(), nullptr)) {
        check("eigenes Unicode-Temp-Verzeichnis", false); return;
    }
    const std::wstring projects = wroot + L"\\projekte";
    const std::wstring project_a = L"A_\u00e4_\u65e5_\U0001f680";
    const std::wstring project = projects + L"\\" + project_a;
    const std::wstring data = project + L"\\data";
    const std::wstring texdir = data + L"\\textures";
    const std::wstring json = data + L"\\textures.json";
    const std::wstring outside = wroot + L"\\outside";
    bool dirs = CreateDirectoryW(projects.c_str(), nullptr) && CreateDirectoryW(project.c_str(), nullptr) &&
                CreateDirectoryW(data.c_str(), nullptr) && CreateDirectoryW(outside.c_str(), nullptr);
    check("isolierte Projektordner", dirs);
    if (!dirs) return;
    // Das Arbeitsverzeichnis ist absichtlich NICHT der Projekt-Datenordner.
    // So darf ein fälschlich gegen CWD aufgelöster Rename weder hier landen
    // noch eine gleichnamige fremde Datei verändern.
    const DWORD cwd_size = GetCurrentDirectoryW(0, nullptr);
    std::wstring old_cwd(cwd_size, L'\0');
    const DWORD cwd_length = cwd_size ? GetCurrentDirectoryW(cwd_size, old_cwd.data()) : 0;
    const bool cwd_ok = cwd_length && cwd_length < cwd_size;
    if (cwd_ok) old_cwd.resize(cwd_length);
    const std::wstring cwd_sentinel = outside + L"\\textures.json";
    const bool cwd_changed = cwd_ok && put(cwd_sentinel, "CWD UNBERUEHRT", CREATE_NEW) &&
                             SetCurrentDirectoryW(outside.c_str());
    check("abweichendes Arbeitsverzeichnis mit fremdem Manifest", cwd_changed);
    ProjectStore store;
    check("verankerte Projektwurzel öffnen", store.open(root, &project_a) == Result::Ok);
    const std::wstring moved_data = project + L"\\data-moved";
    check("offenes Projektverzeichnis kann nicht ausgetauscht werden",
          !MoveFileW(data.c_str(), moved_data.c_str()) && GetFileAttributesW(data.c_str()) != INVALID_FILE_ATTRIBUTES);
    std::string data_utf8;
    check("Ruby-Datenwurzel muss exakt zum gewählten Projekt passen", rune::assets::to_utf8(data, data_utf8) &&
          rune::assets::matches_data_root(root, data_utf8, &project_a) &&
          !rune::assets::matches_data_root(root, data_utf8 + "\\..\\..\\B\\data", &project_a));
    std::string read;
    check("fehlendes Manifest wird erkannt", store.read_manifest(read) == Result::Missing);
    std::wstring recovery;
    const std::string first = "{\"textures\":[]}";
    check("Manifest ohne Überschreiben neu anlegen", store.save_manifest(false, "", first, recovery) == Result::Ok &&
          recovery.empty() && store.read_manifest(read) == Result::Ok && read == first);
    check("zweite Neuanlage überschreibt nichts", store.save_manifest(false, "", "NEU", recovery) == Result::Conflict &&
          store.read_manifest(read) == Result::Ok && read == first);
    check("externe Änderung vorbereiten", put(json, "EXTERN"));
    check("Bytekonflikt bricht vor jeder Änderung ab", store.save_manifest(true, first, "NEU", recovery) == Result::Conflict &&
          store.read_manifest(read) == Result::Ok && read == "EXTERN");
    Handle lock(CreateFileW(json.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    const bool locked = lock.valid() && store.save_manifest(true, "EXTERN", "NEU", recovery) != Result::Ok;
    lock.reset();
    check("Schreibsperre schützt bestehende Bytes", locked && store.read_manifest(read) == Result::Ok && read == "EXTERN");
    const std::string saved = "{\"textures\":[42]}";
    check("gültiger Vergleich und gesicherter Commit", store.save_manifest(true, "EXTERN", saved, recovery) == Result::Ok &&
          recovery.empty() && store.read_manifest(read) == Result::Ok && read == saved);
    Race race{json};
    const Result race_result = store.save_manifest(true, saved, "NEU", recovery, competing_create, &race);
    const std::wstring backup = data + L"\\" + recovery;
    check("Rennen: fremdes neues Ziel wird nicht überschrieben", race.created && race_result == Result::Conflict &&
          get(json, read) && read == "EXTERN");
    check("Rennen: ursprünglicher Inhalt bleibt in benannter Sicherung", !recovery.empty() && get(backup, read) && read == saved);
    const bool restored = DeleteFileW(json.c_str()) && MoveFileW(backup.c_str(), json.c_str());
    check("Sicherung im Test wiederhergestellt", restored && get(json, read) && read == saved);
    check("Manifest im fremden Arbeitsverzeichnis bleibt unverändert",
          cwd_changed && get(cwd_sentinel, read) && read == "CWD UNBERUEHRT");

    Handle first_texture;
    std::string first_name;
    check("Textur exklusiv neu anlegen", store.create_texture("stein.rgba", "ABCD", first_name, first_texture) == Result::Ok && first_name == "stein.rgba");
    first_texture.reset();
    Handle second_texture;
    std::string second_name;
    check("Dateikollision bekommt neuen Namen", store.create_texture("stein.rgba", "EFGH", second_name, second_texture) == Result::Ok && second_name != first_name);
    second_texture.reset();
    check("beide RGBA-Dateien unverändert lesbar", store.read_texture(first_name, 4, read) == Result::Ok && read == "ABCD" &&
          store.read_texture(second_name, 4, read) == Result::Ok && read == "EFGH");
    check("abweichende RGBA-Länge abgewiesen", store.read_texture(first_name, 3, read) == Result::TooLarge &&
          store.read_texture(first_name, 5, read) == Result::Invalid);
    check("Pfadflucht und Gerätenamen abgewiesen", store.read_texture("../outside", 4, read) == Result::Invalid &&
          store.read_texture("NUL.rgba", 4, read) == Result::Invalid);

    const std::wstring sentinel = outside + L"\\sentinel.rgba";
    check("externe Testdatei", put(sentinel, "KEEP"));
    const std::wstring hard = texdir + L"\\hard.rgba";
    const bool hardlink = CreateHardLinkW(hard.c_str(), sentinel.c_str(), nullptr) != 0;
    check("bestehender Hardlink wird nicht als Projekttextur gelesen", hardlink &&
          store.read_texture("hard.rgba", 4, read) == Result::Invalid);
    Handle new_hard;
    std::string hard_name;
    check("Hardlink-Kollision wird nicht überschrieben", hardlink &&
          store.create_texture("hard.rgba", "NEW!", hard_name, new_hard) == Result::Ok && hard_name != "hard.rgba");
    new_hard.reset();
    check("Hardlink-Zieldatei bleibt unverändert", get(sentinel, read) && read == "KEEP");

    const std::wstring bp = projects + L"\\B", bd = bp + L"\\data", blink = bd + L"\\textures";
    const std::wstring cp = projects + L"\\C", cd = cp + L"\\data", clink = cd + L"\\textures.json";
    const std::wstring dlink = projects + L"\\D";
    const bool tree = CreateDirectoryW(bp.c_str(), nullptr) && CreateDirectoryW(bd.c_str(), nullptr) &&
                      CreateDirectoryW(cp.c_str(), nullptr) && CreateDirectoryW(cd.c_str(), nullptr);
    const bool links = tree && make_junction(blink, outside) && make_junction(clink, outside) && make_junction(dlink, outside);
    check("drei echte Junctions im Testverzeichnis angelegt", links);
    const std::wstring pb = L"B", pc = L"C", pd = L"D";
    ProjectStore b, c, d;
    check("textures-Verzeichnis als Junction abgewiesen", links && b.open(root, &pb) == Result::Ok && b.read_texture("sentinel.rgba", 4, read) == Result::Reparse);
    check("textures.json als Junction abgewiesen", links && c.open(root, &pc) == Result::Ok && c.read_manifest(read) == Result::Reparse);
    check("Projektverzeichnis als Junction abgewiesen", links && d.open(root, &pd) == Result::Reparse);
    check("Datei hinter Junctions bleibt unverändert", get(sentinel, read) && read == "KEEP");

    // Erst alle verankerten Verzeichnishandles schließen, dann nur eigene
    // Testdateien/-junctions entfernen, niemals den Junction-Inhalt löschen.
    store = ProjectStore{}; b = ProjectStore{}; c = ProjectStore{}; d = ProjectStore{};
    DeleteFileW((texdir + L"\\stein.rgba").c_str());
    std::wstring second_wide, hard_wide;
    rune::assets::to_wide(second_name, second_wide);
    rune::assets::to_wide(hard_name, hard_wide);
    if (!second_wide.empty()) DeleteFileW((texdir + L"\\" + second_wide).c_str());
    if (!hard_wide.empty()) DeleteFileW((texdir + L"\\" + hard_wide).c_str());
    DeleteFileW(hard.c_str());
    DeleteFileW(json.c_str());
    RemoveDirectoryW(texdir.c_str());
    RemoveDirectoryW(data.c_str());
    RemoveDirectoryW(project.c_str());
    RemoveDirectoryW(blink.c_str()); RemoveDirectoryW(bd.c_str()); RemoveDirectoryW(bp.c_str());
    RemoveDirectoryW(clink.c_str()); RemoveDirectoryW(cd.c_str()); RemoveDirectoryW(cp.c_str());
    RemoveDirectoryW(dlink.c_str());
    check("Arbeitsverzeichnis zurückgesetzt", !cwd_changed || SetCurrentDirectoryW(old_cwd.c_str()));
    DeleteFileW(cwd_sentinel.c_str());
    DeleteFileW(sentinel.c_str()); RemoveDirectoryW(outside.c_str());
    RemoveDirectoryW(projects.c_str());
    check("Testverzeichnis vollständig entfernt", RemoveDirectoryW(wroot.c_str()) != 0);
}
#endif
} // namespace

int main() {
    portable_tests();
#ifdef _WIN32
    windows_tests();
#endif
    std::printf("MATERIAL-TEST: %d Prüfungen, %d Fehler\n", checks, failures);
    if (std::getenv("GITHUB_ACTIONS"))
        std::printf("::notice title=Materialtest::%d Prüfungen, %d Fehler\n", checks, failures);
    return failures ? 1 : 0;
}
