// Windows-Dateitransaktionen für Karten/F9: echte Handles, Unicode-Pfade und
// deterministische Konkurrenz. Alle Dateien liegen in einem eigenen Tempbaum;
// kein interaktiver Editor- oder Hardwaretest.
#include "editor/document_store_win.h"
#include "editor/json_validation.h"

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
using rune::assets::Handle;
using rune::assets::Result;
using rune::editor::DocumentOutcome;
using rune::editor::DocumentRecovery;
using rune::editor::DocumentSnapshot;
using rune::editor::DocumentStore;
using rune::editor::DocumentTestHooks;
using rune::editor::document_erase_confirmed;
using rune::editor::document_save_confirmed;

int checks = 0, failures = 0;
void check(const char* name, bool ok) {
    const DWORD error = GetLastError();
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", name);
    if (!ok && std::getenv("GITHUB_ACTIONS"))
        std::printf("::error title=Dokument-Dateitest::%s (letzter Win32-Status %lu)\n", name,
                    static_cast<unsigned long>(error));
}

bool put(const std::wstring& path, const std::string& text, DWORD creation = CREATE_ALWAYS) {
    Handle file(CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, creation, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid()) return false;
    DWORD written = 0;
    return WriteFile(file.get(), text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
           written == text.size() && FlushFileBuffers(file.get());
}

bool get(const std::wstring& path, std::string& text) {
    Handle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid()) return false;
    text.clear();
    char buffer[4096];
    for (;;) {
        DWORD count = 0;
        if (!ReadFile(file.get(), buffer, sizeof(buffer), &count, nullptr)) return false;
        if (!count) return true;
        text.append(buffer, count);
    }
}

bool make_junction(const std::wstring& link, const std::wstring& target) {
    std::wstring command = L"cmd.exe /d /c mklink /J \"" + link + L"\" \"" + target + L"\"";
    STARTUPINFOW si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &si, &pi)) return false;
    const DWORD wait = WaitForSingleObject(pi.hProcess, 15000);
    if (wait != WAIT_OBJECT_0) { TerminateProcess(pi.hProcess, 1); WaitForSingleObject(pi.hProcess, 5000); }
    DWORD code = 1;
    const bool ok = wait == WAIT_OBJECT_0 && GetExitCodeProcess(pi.hProcess, &code) && code == 0;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return ok;
}

struct Fixture {
    std::wstring root, projects, project, data, maps, outside, moved_maps, other_project, other_data, junction;
    std::wstring name = L" Projekt_\u00e4_\u65e5_\U0001f680";
    std::wstring map_name = L"welt_\u754c_\U0001f680.json";
    std::wstring map, db, sentinel, moved, other_cwd, cwd_sentinel;
    std::string root8, name8, map8;
    std::vector<std::wstring> dirs;
    bool owned = false;

    bool mkdir(const std::wstring& path) {
        if (!CreateDirectoryW(path.c_str(), nullptr)) return false;
        dirs.push_back(path);
        return true;
    }
    bool create() {
        wchar_t temp[MAX_PATH + 1]{};
        const DWORD n = GetTempPathW(MAX_PATH, temp);
        if (!n || n >= MAX_PATH) return false;
        for (int i = 0; i < 100; ++i) {
            root = std::wstring(temp, n) + L"rune-doc-\u00e4-\u65e5-\U0001f680-" +
                   std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(i);
            if (mkdir(root)) { owned = true; break; }
            if (GetLastError() != ERROR_ALREADY_EXISTS) return false;
        }
        if (!owned) return false;
        projects = root + L"\\projekte"; project = projects + L"\\" + name;
        data = project + L"\\data"; maps = data + L"\\maps"; outside = root + L"\\outside";
        moved_maps = data + L"\\maps-moved"; moved = maps + L"\\vorher.json";
        map = maps + L"\\" + map_name; db = data + L"\\database.json";
        sentinel = outside + L"\\sentinel.json"; other_cwd = root + L"\\cwd";
        cwd_sentinel = other_cwd + L"\\database.json";
        other_project = projects + L"\\J"; other_data = other_project + L"\\data";
        junction = other_data + L"\\maps";
        return rune::assets::to_utf8(root, root8) && rune::assets::to_utf8(name, name8) &&
               rune::assets::to_utf8(map_name, map8) && mkdir(projects) && mkdir(project) &&
               mkdir(data) && mkdir(maps) && mkdir(outside) && mkdir(other_cwd) &&
               mkdir(other_project) && mkdir(other_data);
    }

    // Nur selbst erzeugte Dateien und .rune-Reste innerhalb dieser exklusiven
    // Testwurzel; eine Junction wird nie durchlaufen.
    bool cleanup() {
        if (!owned) return false;
        bool ok = true;
        const auto erase = [&](const std::wstring& path) {
            if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES && !DeleteFileW(path.c_str())) ok = false;
        };
        erase(map); erase(db); erase(sentinel); erase(moved); erase(cwd_sentinel);
        for (const auto& dir : {maps, data}) {
            WIN32_FIND_DATAW fd{};
            HANDLE h = FindFirstFileW((dir + L"\\.rune-*").c_str(), &fd);
            if (h != INVALID_HANDLE_VALUE) {
                do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) erase(dir + L"\\" + fd.cFileName); }
                while (FindNextFileW(h, &fd));
                FindClose(h);
            }
        }
        if (GetFileAttributesW(junction.c_str()) != INVALID_FILE_ATTRIBUTES && !RemoveDirectoryW(junction.c_str())) ok = false;
        for (auto it = dirs.rbegin(); it != dirs.rend(); ++it)
            if (!RemoveDirectoryW(it->c_str())) ok = false;
        owned = false;
        return ok;
    }
};

struct Race {
    const wchar_t* target = nullptr;
    const wchar_t* moved = nullptr;
    bool invoked = false;
    bool writer_blocked = false;
    bool delete_blocked = false;
    bool rename_blocked = false;
    bool created = false;
};

void try_locked_changes(void* context) noexcept {
    auto& race = *static_cast<Race*>(context);
    race.invoked = true;
    HANDLE writer = CreateFileW(race.target, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    race.writer_blocked = writer == INVALID_HANDLE_VALUE;
    if (writer != INVALID_HANDLE_VALUE) CloseHandle(writer);
    race.delete_blocked = DeleteFileW(race.target) == 0;
    race.rename_blocked = MoveFileW(race.target, race.moved) == 0;
}

void competing_create(void* context) noexcept {
    auto& race = *static_cast<Race*>(context);
    race.invoked = true;
    HANDLE file = CreateFileW(race.target, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    constexpr char content[] = "FREMD";
    DWORD written = 0;
    race.created = WriteFile(file, content, sizeof(content) - 1, &written, nullptr) && written == sizeof(content) - 1;
    CloseHandle(file);
}

int rune_files(const std::wstring& directory) {
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((directory + L"\\.rune-*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    int count = 0;
    do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) ++count; }
    while (FindNextFileW(h, &fd));
    FindClose(h);
    return count;
}

void run_tests(Fixture& f) {
    std::string text;
    std::wstring cwd;
    const DWORD n = GetCurrentDirectoryW(0, nullptr);
    if (n) {
        cwd.resize(n);
        const DWORD written = GetCurrentDirectoryW(n, cwd.data());
        if (written && written < n) cwd.resize(written); else cwd.clear();
    }
    const bool cwd_set = !cwd.empty() && put(f.cwd_sentinel, "CWD BLEIBT", CREATE_NEW) &&
                         SetCurrentDirectoryW(f.other_cwd.c_str());
    check("fremdes Arbeitsverzeichnis mit gleichnamiger Datei", cwd_set);

    {
        DocumentStore map, db;
        check("Unicode-Projekt und Kartenordner verankert", map.open(f.root8, f.name8, true, f.map8) == Result::Ok);
        check("F9-Dokument im selben Projekt verankert", db.open(f.root8, f.name8, false, "database.json") == Result::Ok);
        check("gehaltener Kartenordner kann nicht ausgetauscht werden", !MoveFileW(f.maps.c_str(), f.moved_maps.c_str()));
        check("Pfadflucht als Dokumentname abgewiesen", [&] {
            DocumentStore invalid;
            return invalid.open(f.root8, f.name8, true, "../fremd.json") == Result::Invalid;
        }());

        DocumentRecovery recovery;
        {
            DocumentSnapshot missing;
            check("fehlende Karte wird als fehlend beobachtet", map.observe(missing) == Result::Ok && !missing.present);
            check("neue Karte ohne Überschreiben angelegt", map.save(missing, "ALT", recovery) == Result::Ok &&
                  recovery.backup.empty() && recovery.temp.empty() && get(f.map, text) && text == "ALT");
        }
        {
            int questions = 0;
            const DocumentOutcome same = document_save_confirmed(map, true, "ALT", "ALT", [&] {
                ++questions; return true;
            }, recovery);
            check("unveränderter Stand speichert ohne Konfliktrückfrage", same.result == Result::Ok &&
                  !same.cancelled && questions == 0 && get(f.map, text) && text == "ALT");
            const DocumentOutcome denied = document_save_confirmed(map, true, "VERALTET", "EDITOR", [&] {
                ++questions; return false;
            }, recovery);
            check("Nein im Konfliktdialog erhält die Datei", denied.result == Result::Ok && denied.cancelled &&
                  questions == 1 && get(f.map, text) && text == "ALT");
            bool changed = false;
            const DocumentOutcome raced = document_save_confirmed(map, true, "VERALTET", "EDITOR", [&] {
                ++questions; changed = put(f.map, "WAEHREND RUECKFRAGE"); return true;
            }, recovery);
            check("Ja bestätigt keine späteren Änderungen während des Dialogs", changed && questions == 2 &&
                  !raced.cancelled && raced.result == Result::Conflict && get(f.map, text) && text == "WAEHREND RUECKFRAGE");
            check("Dialog-Rennentest zurückgesetzt", put(f.map, "ALT"));
        }
        {
            DocumentSnapshot seen;
            const bool observed = map.observe(seen) == Result::Ok && seen.present && seen.bytes == "ALT";
            check("Attribut-Handle erlaubt externes Schreiben nach der Beobachtung", observed && put(f.map, "EXTERN"));
            check("Änderung nach Beobachtung wird nicht überschrieben", observed &&
                  map.save(seen, "EDITOR", recovery) == Result::Conflict && get(f.map, text) && text == "EXTERN" &&
                  recovery.backup.empty());
        }
        {
            DocumentSnapshot seen;
            const bool observed = map.observe(seen) == Result::Ok && seen.present && seen.bytes == "EXTERN";
            check("erkannter Konflikt wäre rückfragepflichtig", observed &&
                  rune::editor::editor_disk_changed(true, "ALT", seen.present, seen.bytes));
            check("ausdrücklich bestätigter aktueller Stand kann gespeichert werden", observed &&
                  map.save(seen, "EDITOR", recovery) == Result::Ok && recovery.backup.empty() &&
                  get(f.map, text) && text == "EDITOR");
        }
        {
            DocumentSnapshot seen;
            const bool observed = map.observe(seen) == Result::Ok && seen.bytes == "EDITOR";
            Race race{f.map.c_str(), f.moved.c_str()};
            const DocumentTestHooks hooks{try_locked_changes, nullptr, &race};
            const Result result = observed ? map.save(seen, "GESICHERT", recovery, &hooks) : Result::Io;
            check("während Vergleich und Commit sind Schreiben, Löschen und Rename gesperrt",
                  result == Result::Ok && race.invoked && race.writer_blocked && race.delete_blocked && race.rename_blocked &&
                  get(f.map, text) && text == "GESICHERT");
        }
        {
            DocumentSnapshot seen;
            const bool observed = map.observe(seen) == Result::Ok && seen.bytes == "GESICHERT";
            const bool replaced = observed && MoveFileW(f.map.c_str(), f.moved.c_str()) &&
                                  put(f.map, "GESICHERT", CREATE_NEW);
            check("Dateiidentität erkennt Ersatz mit gleichen Bytes", replaced &&
                  map.save(seen, "EDITOR", recovery) == Result::Conflict && get(f.map, text) && text == "GESICHERT" &&
                  get(f.moved, text) && text == "GESICHERT");
        }
        check("Identitätstest-Dateien nur im Testprojekt zurückgesetzt",
              DeleteFileW(f.map.c_str()) && MoveFileW(f.moved.c_str(), f.map.c_str()));
        {
            DocumentSnapshot seen;
            const bool observed = map.observe(seen) == Result::Ok && seen.bytes == "GESICHERT";
            Race race{f.map.c_str(), nullptr};
            const DocumentTestHooks hooks{nullptr, competing_create, &race};
            const Result result = observed ? map.save(seen, "EDITOR", recovery, &hooks) : Result::Io;
            const std::wstring backup = f.maps + L"\\" + recovery.backup;
            check("Neuanleger zwischen Backup und Commit wird nicht überschrieben", race.created &&
                  result == Result::Conflict && get(f.map, text) && text == "FREMD");
            check("verdrängter Originalstand bleibt als benannte Sicherung erhalten", !recovery.backup.empty() &&
                  get(backup, text) && text == "GESICHERT");
            check("Test stellt eigene Sicherung wieder her", DeleteFileW(f.map.c_str()) &&
                  MoveFileW(backup.c_str(), f.map.c_str()));
        }
        {
            DocumentSnapshot seen;
            const bool observed = map.observe(seen) == Result::Ok && seen.bytes == "GESICHERT";
            check("externe Änderung vor Karten-Löschen vorbereitet", observed && put(f.map, "NICHT LOESCHEN"));
            check("Löschen mit veraltetem Stand entfernt keine fremden Bytes", observed &&
                  map.erase(seen, recovery) == Result::Conflict && get(f.map, text) && text == "NICHT LOESCHEN");
        }
        {
            int questions = 0;
            const DocumentOutcome denied = document_erase_confirmed(map, true, "ALT", [&] {
                ++questions; return false;
            }, recovery);
            check("Nein beim Konflikt-Löschen erhält die Karte", denied.cancelled && questions == 1 &&
                  get(f.map, text) && text == "NICHT LOESCHEN");
            bool changed = false;
            const DocumentOutcome raced = document_erase_confirmed(map, true, "ALT", [&] {
                ++questions; changed = put(f.map, "NEUER STAND"); return true;
            }, recovery);
            check("Ja beim Löschen gilt nicht für Änderungen während der Rückfrage", changed && questions == 2 &&
                  raced.result == Result::Conflict && !raced.cancelled && get(f.map, text) && text == "NEUER STAND");
            check("Lösch-Dialogtest zurückgesetzt", put(f.map, "NICHT LOESCHEN"));
        }
        {
            DocumentSnapshot seen;
            const bool observed = map.observe(seen) == Result::Ok && seen.bytes == "NICHT LOESCHEN";
            Race race{f.map.c_str(), nullptr};
            const DocumentTestHooks hooks{nullptr, competing_create, &race};
            const Result result = observed ? map.erase(seen, recovery, &hooks) : Result::Io;
            check("Löschen betrifft nur das bestätigte Handle, nicht einen Neuanleger", result == Result::Ok &&
                  race.created && recovery.backup.empty() && get(f.map, text) && text == "FREMD");
        }
        {
            Handle reader(CreateFileW(f.map.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            DocumentSnapshot seen;
            const bool observed = reader.valid() && map.observe(seen) == Result::Ok && seen.bytes == "FREMD";
            check("Fremdleser ohne Delete-Sharing blockiert Ersetzen sicher", observed &&
                  map.save(seen, "EDITOR", recovery) == Result::Io && get(f.map, text) && text == "FREMD");
        }
        {
            std::string big(131101, 'x');
            big[65536] = '\0';
            big.back() = 'y';
            check("große binäre Testdatei vorbereitet", put(f.map, big));
            DocumentSnapshot seen;
            const bool observed = map.observe(seen) == Result::Ok && seen.bytes == big;
            check("Bytevergleich liest über mehrere Puffer einschließlich NUL", observed &&
                  map.save(seen, "NACH GROSS", recovery) == Result::Ok && get(f.map, text) && text == "NACH GROSS");
        }
        {
            // unique_name wird im Commit einmal für .tmp und dann für .bak
            // benutzt. Die erste Sicherung absichtlich belegen und erhalten.
            const std::wstring seed = rune::assets::unique_name(L".bak");
            const std::size_t dash = seed.find_last_of(L'-');
            const int serial = std::stoi(seed.substr(dash + 1));
            const std::wstring collision = f.maps + L"\\.rune-" + std::to_wstring(GetCurrentProcessId()) +
                                           L"-" + std::to_wstring(serial + 2) + L".bak";
            DocumentSnapshot seen;
            const bool ready = map.observe(seen) == Result::Ok && seen.bytes == "NACH GROSS" &&
                               put(collision, "FREMDE SICHERUNG", CREATE_NEW);
            check("Kollision eines Sicherungsnamens wird nicht ersetzt", ready &&
                  map.save(seen, "NACH KOLLISION", recovery) == Result::Ok && recovery.backup.empty() &&
                  get(collision, text) && text == "FREMDE SICHERUNG" &&
                  get(f.map, text) && text == "NACH KOLLISION");
            check("eigene Kollisionsfixture entfernt", DeleteFileW(collision.c_str()));
        }
        {
            const bool removed = DeleteFileW(f.map.c_str()) != 0;
            int questions = 0;
            const DocumentOutcome denied = document_save_confirmed(map, true, "NACH KOLLISION", "EDITOR", [&] {
                ++questions; return false;
            }, recovery);
            check("entfernte Karte fragt und Nein legt sie nicht neu an", removed && denied.cancelled && questions == 1 &&
                  GetFileAttributesW(f.map.c_str()) == INVALID_FILE_ATTRIBUTES);
            bool created = false;
            const DocumentOutcome raced = document_save_confirmed(map, true, "NACH KOLLISION", "EDITOR", [&] {
                ++questions; created = put(f.map, "NEU ANGELEGT", CREATE_NEW); return true;
            }, recovery);
            check("Ja zu fehlender Datei überschreibt keinen Neuanleger im Dialog", created && questions == 2 &&
                  raced.result == Result::Conflict && get(f.map, text) && text == "NEU ANGELEGT");
            check("Test-Neuanleger entfernt", DeleteFileW(f.map.c_str()));
        }
        {
            DocumentSnapshot missing;
            Race race{f.map.c_str(), nullptr};
            const DocumentTestHooks hooks{competing_create, nullptr, &race};
            const Result result = map.save(missing, "EDITOR", recovery, &hooks);
            check("Neuanleger nach Fehlend-Vergleich gewinnt ohne Überschreiben", race.invoked && race.created &&
                  result == Result::Conflict && recovery.backup.empty() && recovery.temp.empty() &&
                  get(f.map, text) && text == "FREMD");
            check("zweiter Test-Neuanleger entfernt", DeleteFileW(f.map.c_str()));
            int questions = 0;
            const DocumentOutcome recreated = document_save_confirmed(map, true, "ALT", "NEU", [&] {
                ++questions; return true;
            }, recovery);
            check("fehlende Karte nach ausdrücklichem Ja neu angelegt", recreated.result == Result::Ok &&
                  !recreated.cancelled && questions == 1 && get(f.map, text) && text == "NEU");
        }
        check("keine temporären Karten-/Backupreste nach Erfolg und Wiederherstellung", rune_files(f.maps) == 0);

        {
            DocumentSnapshot missing;
            check("fehlende Datenbank beobachtet", db.observe(missing) == Result::Ok && !missing.present);
            check("externe Datenbank nach Beobachtung angelegt", put(f.db, "ANDERE DB", CREATE_NEW));
            check("fehlende F9-Datei überschreibt keinen Neuanleger", db.save(missing, "DB", recovery) == Result::Conflict &&
                  get(f.db, text) && text == "ANDERE DB");
        }
        {
            DocumentSnapshot seen;
            const bool observed = db.observe(seen) == Result::Ok && seen.bytes == "ANDERE DB";
            check("F9 speichert bestätigte Bytes über dieselbe gesicherte Transaktion", observed &&
                  db.save(seen, "DB GESPEICHERT", recovery) == Result::Ok && recovery.backup.empty() &&
                  get(f.db, text) && text == "DB GESPEICHERT");
        }
        {
            int questions = 0;
            bool changed = false;
            const DocumentOutcome raced = document_save_confirmed(db, true, "ANDERE DB", "EDITOR DB", [&] {
                ++questions; changed = put(f.db, "F9 WAEHREND DIALOG"); return true;
            }, recovery);
            check("F9 bestätigt nur den vor der Rückfrage gelesenen Stand", changed && questions == 1 &&
                  raced.result == Result::Conflict && !raced.cancelled && get(f.db, text) && text == "F9 WAEHREND DIALOG");
            check("F9-Rennentest zurückgesetzt", put(f.db, "DB GESPEICHERT"));
        }
        {
            Handle writer(CreateFileW(f.db.c_str(), GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            DocumentSnapshot seen;
            check("offener Fremd-Schreiber blockiert stabile Beobachtung", writer.valid() && db.observe(seen) != Result::Ok);
        }
        check("gesperrte Datenbank unverändert", get(f.db, text) && text == "DB GESPEICHERT");
        check("gleichnamige Datei im fremden CWD unverändert", cwd_set && get(f.cwd_sentinel, text) && text == "CWD BLEIBT");
    }

    check("externe Testdatei erstellt", put(f.sentinel, "AUSSEN", CREATE_NEW));
    const std::wstring hard = f.data + L"\\hard.json";
    const bool hardlink = CreateHardLinkW(hard.c_str(), f.sentinel.c_str(), nullptr) != 0;
    {
        DocumentStore target;
        DocumentSnapshot seen;
        check("Hardlink-Dokument wird vor dem Schreiben abgewiesen", hardlink &&
              target.open(f.root8, f.name8, false, "hard.json") == Result::Ok && target.observe(seen) == Result::Invalid);
    }
    check("Datei hinter Hardlink bleibt unverändert", get(f.sentinel, text) && text == "AUSSEN");
    if (hardlink) DeleteFileW(hard.c_str());
    const std::wstring file_junction = f.data + L"\\link.json";
    const bool target_link = make_junction(file_junction, f.outside);
    {
        DocumentStore target;
        DocumentSnapshot seen;
        check("Reparsepunkt als Dokumentziel wird abgewiesen", target_link &&
              target.open(f.root8, f.name8, false, "link.json") == Result::Ok && target.observe(seen) == Result::Reparse);
    }
    check("Dokument-Junction entfernt", !target_link || RemoveDirectoryW(file_junction.c_str()));
    const bool linked = make_junction(f.junction, f.outside);
    {
        DocumentStore target;
        check("Kartenordner als Junction wird abgewiesen", linked && target.open(f.root8, "J", true, "sentinel.json") == Result::Reparse);
    }
    check("Inhalt hinter Junction bleibt unverändert", get(f.sentinel, text) && text == "AUSSEN");
    check("Arbeitsverzeichnis zurückgesetzt", !cwd_set || SetCurrentDirectoryW(cwd.c_str()));
}
} // namespace

int main() {
    std::puts("== Windows: transaktionale Karten-/F9-Dateien ==");
    Fixture fixture;
    if (!fixture.create()) {
        check("isolierter Unicode-Tempbaum", false);
        if (fixture.owned) fixture.cleanup();
        return 1;
    }
    check("isolierter Unicode-Tempbaum", true);
    run_tests(fixture);
    check("nur eigene Testdateien vollständig entfernt", fixture.cleanup());
    std::printf("DOKUMENT-DATEITEST: %d Prüfungen, %d Fehler\n", checks, failures);
    if (std::getenv("GITHUB_ACTIONS"))
        std::printf("::notice title=Dokument-Dateitest::%d Prüfungen, %d Fehler\n", checks, failures);
    return failures ? 1 : 0;
}
