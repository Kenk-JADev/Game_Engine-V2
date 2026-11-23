#pragma once
// Projekt-Materialdateien unter einer gewählten Engine-Wurzel. Alle Kinder
// werden einzeln relativ zu bereits geöffneten Verzeichnishandles geöffnet;
// Reparsepunkte werden am geöffneten Objekt abgewiesen. Die Engine-Wurzel
// selbst ist der ausdrücklich vertraute Anker. Kein Fallback auf Pfad-I/O.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winternl.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/win_path.h"
#include "editor/texture_path.h"
#include "world/texture_limits.h"

namespace rune::assets {

enum class Result { Ok, Missing, Conflict, Invalid, TooLarge, Reparse, Io };

inline const char* result_text(Result r) {
    switch (r) {
        case Result::Ok: return "ok";
        case Result::Missing: return "Datei oder Verzeichnis fehlt";
        case Result::Conflict: return "Dateikonflikt";
        case Result::Invalid: return "ungültiger Pfad oder Dateityp";
        case Result::TooLarge: return "Datei zu gross";
        case Result::Reparse: return "Reparsepunkt nicht erlaubt";
        case Result::Io: return "Dateizugriff fehlgeschlagen oder gesperrt";
    }
    return "Dateizugriff fehlgeschlagen";
}

class Handle {
public:
    Handle() = default;
    explicit Handle(HANDLE h) : h_(h) {}
    ~Handle() { reset(); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : h_(other.h_) { other.h_ = INVALID_HANDLE_VALUE; }
    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) { reset(); h_ = other.h_; other.h_ = INVALID_HANDLE_VALUE; }
        return *this;
    }
    HANDLE get() const { return h_; }
    bool valid() const { return h_ != INVALID_HANDLE_VALUE && h_ != nullptr; }
    void reset(HANDLE h = INVALID_HANDLE_VALUE) {
        if (valid()) CloseHandle(h_);
        h_ = h;
    }
private:
    HANDLE h_ = INVALID_HANDLE_VALUE;
};

inline bool to_wide(const std::string& input, std::wstring& output) {
    return core::win_path::to_wide(input, output);
}

inline bool to_utf8(const std::wstring& input, std::string& output) {
    return core::win_path::to_utf8(input, output);
}

inline bool absolute_path(const std::wstring& path, std::wstring& full) {
    const DWORD n = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (!n || n > 32760) return false;
    std::wstring result(static_cast<std::size_t>(n), L'\0');
    const DWORD copied = GetFullPathNameW(path.c_str(), n, result.data(), nullptr);
    if (!copied || copied >= n) return false;
    result.resize(copied);
    std::replace(result.begin(), result.end(), L'/', L'\\');
    while (result.size() > 3 && result.back() == L'\\') result.pop_back();
    full = std::move(result);
    return true;
}

inline bool component(const std::wstring& name) {
    if (name.empty() || name.size() > 255 || name == L"." || name == L".." ||
        name.back() == L'.' || name.back() == L' ') return false;
    for (wchar_t c : name) {
        if (c < 0x20 || c == 0x7f || c == L'/' || c == L'\\' || c == L':' ||
            c == L'*' || c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|') return false;
    }
    std::string utf8;
    return to_utf8(name, utf8) && !editor::windows_device_name(utf8);
}

// Die Ruby-Datenwurzel darf nur exakt der gewählten Projekt- bzw. Demo-
// Wurzel entsprechen; geöffnet wird anschließend trotzdem handle-relativ.
inline bool matches_data_root(const std::string& engine_root, const std::string& data_root,
                              const std::wstring* project) {
    std::wstring engine, requested, expected, actual;
    if (!to_wide(engine_root, engine) || !to_wide(data_root, requested) ||
        (project && !component(*project))) return false;
    expected = engine + (project ? L"\\projekte\\" + *project + L"\\data" : L"\\game\\data");
    if (!absolute_path(expected, expected) || !absolute_path(requested, actual)) return false;
    return CompareStringOrdinal(expected.c_str(), static_cast<int>(expected.size()),
                                actual.c_str(), static_cast<int>(actual.size()), TRUE) == CSTR_EQUAL;
}

inline Result inspect(HANDLE h, bool directory) {
    BY_HANDLE_FILE_INFORMATION info{};
    if (GetFileType(h) != FILE_TYPE_DISK || !GetFileInformationByHandle(h, &info)) return Result::Io;
    if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) return Result::Reparse;
    if (!!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != directory) return Result::Invalid;
    // Auch ein Hardlink kann auf Inhalte ausserhalb des Projektbaums zeigen.
    // Bei vorhandenen Dateien daher nur einen Verzeichniseintrag akzeptieren
    // (neue Dateien werden ohnehin exklusiv angelegt).
    if (!directory && info.nNumberOfLinks != 1) return Result::Invalid;
    return Result::Ok;
}

// NtCreateFile mit genau EINEM Namen und verifiziertem RootDirectory. Ein
// abschließender Reparsepunkt wird als solcher geöffnet und danach abgewiesen.
inline Result open_relative(HANDLE parent, const std::wstring& name, ACCESS_MASK access,
                            ULONG sharing, ULONG disposition, ULONG options, Handle& out) {
    if (!component(name)) return Result::Invalid;
    using NtCreate = NTSTATUS (NTAPI *)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK,
                                        PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
    using NtError = ULONG (WINAPI *)(NTSTATUS);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return Result::Io;
    const auto create = reinterpret_cast<NtCreate>(GetProcAddress(ntdll, "NtCreateFile"));
    const auto to_error = reinterpret_cast<NtError>(GetProcAddress(ntdll, "RtlNtStatusToDosError"));
    if (!create || !to_error) return Result::Io;
    UNICODE_STRING text{};
    text.Length = static_cast<USHORT>(name.size() * sizeof(wchar_t));
    text.MaximumLength = text.Length;
    text.Buffer = const_cast<PWSTR>(name.data());
    OBJECT_ATTRIBUTES attrs{};
    attrs.Length = sizeof(attrs);
    attrs.RootDirectory = parent;
    attrs.ObjectName = &text;
    attrs.Attributes = 0x40; // OBJ_CASE_INSENSITIVE
    IO_STATUS_BLOCK ios{};
    HANDLE file = INVALID_HANDLE_VALUE;
    const NTSTATUS status = create(&file, access | SYNCHRONIZE, &attrs, &ios, nullptr,
                                   FILE_ATTRIBUTE_NORMAL, sharing, disposition,
                                   options | FILE_OPEN_REPARSE_POINT | FILE_SYNCHRONOUS_IO_NONALERT,
                                   nullptr, 0);
    if (status < 0) {
        const ULONG error = to_error(status);
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return Result::Missing;
        if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS) return Result::Conflict;
        return Result::Io;
    }
    out.reset(file);
    return Result::Ok;
}

inline Result open_directory(HANDLE parent, const std::wstring& name, Handle& out) {
    Result r = open_relative(parent, name, FILE_LIST_DIRECTORY | FILE_TRAVERSE | FILE_READ_ATTRIBUTES,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN, 0, out);
    if (r != Result::Ok) return r;
    r = inspect(out.get(), true);
    if (r != Result::Ok) out.reset();
    return r;
}

inline Result read_locked(HANDLE file, std::size_t maximum, std::string& out) {
    out.clear();
    const Result type = inspect(file, false);
    if (type != Result::Ok) return type;
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(file, &length) || length.QuadPart < 0) return Result::Io;
    if (static_cast<unsigned long long>(length.QuadPart) > maximum) return Result::TooLarge;
    std::string data(static_cast<std::size_t>(length.QuadPart), '\0');
    std::size_t pos = 0;
    while (pos < data.size()) {
        const DWORD part = static_cast<DWORD>(std::min<std::size_t>(data.size() - pos, 1024u * 1024u));
        DWORD n = 0;
        if (!ReadFile(file, data.data() + pos, part, &n, nullptr) || !n) return Result::Io;
        pos += n;
    }
    // Die Datei wurde ohne Schreib-/Delete-Sharing geöffnet. Damit bleibt
    // Größe und Inhalt während des Lesens und Vergleichs stabil.
    out = std::move(data);
    return Result::Ok;
}

inline bool write_locked(HANDLE file, const std::string& data) {
    std::size_t pos = 0;
    while (pos < data.size()) {
        const DWORD part = static_cast<DWORD>(std::min<std::size_t>(data.size() - pos, 1024u * 1024u));
        DWORD n = 0;
        if (!WriteFile(file, data.data() + pos, part, &n, nullptr) || !n) return false;
        pos += n;
    }
    return FlushFileBuffers(file) != 0;
}

// Einen bereits validierten Einzel-Namen relativ zum gehaltenen Verzeichnis
// setzen. Niemals ein vorhandenes Ziel ersetzen und keinen Pfad-Fallback
// verwenden. Nach einem Backup-Rename darf diese Funktion nicht allokieren.
inline bool rename_relative_component(HANDLE file, HANDLE directory, std::wstring_view target) noexcept {
    if (target.empty() || target.size() > 255 || target == L"." || target == L".." ||
        target.back() == L'.' || target.back() == L' ') {
        SetLastError(ERROR_INVALID_NAME);
        return false;
    }
    for (wchar_t c : target) {
        if (c < 0x20 || c == 0x7f || c == L'/' || c == L'\\' || c == L':' ||
            c == L'*' || c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|') {
            SetLastError(ERROR_INVALID_NAME);
            return false;
        }
    }
    using NtSet = NTSTATUS (NTAPI *)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS);
    using NtError = ULONG (WINAPI *)(NTSTATUS);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) { SetLastError(ERROR_MOD_NOT_FOUND); return false; }
    const auto set = reinterpret_cast<NtSet>(GetProcAddress(ntdll, "NtSetInformationFile"));
    const auto to_error = reinterpret_cast<NtError>(GetProcAddress(ntdll, "RtlNtStatusToDosError"));
    if (!set || !to_error) { SetLastError(ERROR_PROC_NOT_FOUND); return false; }

    // FILE_RENAME_INFORMATION aus ntifs.h; die nutzbare Layoutdefinition ist
    // nicht in allen Win32-SDKs öffentlich. Der native Aufruf erhält genau ein
    // einzelnes Zielsegment und das bereits geprüfte Verzeichnishandle.
    // SetFileInformationByHandle löst einfache Namen dagegen gegen das
    // Arbeitsverzeichnis auf bzw. akzeptiert RootDirectory nicht zuverlässig.
    struct NativeRenameInfo {
        BOOLEAN replace_if_exists;
        HANDLE root_directory;
        ULONG name_bytes;
        WCHAR name[1];
    };
    const std::size_t bytes = target.size() * sizeof(wchar_t);
    const std::size_t length = sizeof(NativeRenameInfo) + bytes;
    alignas(NativeRenameInfo) unsigned char storage[sizeof(NativeRenameInfo) + 255 * sizeof(wchar_t)]{};
    auto* info = reinterpret_cast<NativeRenameInfo*>(storage);
    info->replace_if_exists = FALSE;
    info->root_directory = directory;
    info->name_bytes = static_cast<ULONG>(bytes);
    std::memcpy(info->name, target.data(), bytes);
    IO_STATUS_BLOCK ios{};
    const NTSTATUS status = set(file, &ios, info, static_cast<ULONG>(length), FileRenameInformation);
    if (status < 0) { SetLastError(to_error(status)); return false; }
    return true;
}

// Der Materialstore darf weiterhin nur das feste Manifest und eigene
// Sicherungsnamen ansteuern. Andere Aufrufer validieren ihre Zielkomponente
// VOR dem ersten Rename über component().
inline bool rename_same_directory(HANDLE file, HANDLE directory, std::wstring_view target) noexcept {
    if (target != L"textures.json") {
        if (!target.starts_with(L".rune-") || !target.ends_with(L".bak")) {
            SetLastError(ERROR_INVALID_NAME);
            return false;
        }
        for (std::size_t i = 6; i + 4 < target.size(); ++i) {
            if ((target[i] < L'0' || target[i] > L'9') && target[i] != L'-') {
                SetLastError(ERROR_INVALID_NAME);
                return false;
            }
        }
    }
    return rename_relative_component(file, directory, target);
}

inline bool delete_owned_handle(HANDLE file) {
    FILE_DISPOSITION_INFO info{};
    info.DeleteFile = TRUE;
    return SetFileInformationByHandle(file, FileDispositionInfo, &info, sizeof(info)) != 0;
}

// Auch bei einer C++-Ausnahme nach CREATE_NEW nur die eigene Datei entfernen.
struct DeleteUncommitted {
    Handle& file;
    bool committed = false;
    ~DeleteUncommitted() {
        if (!committed && file.valid()) delete_owned_handle(file.get());
    }
};

inline std::wstring unique_name(const wchar_t* suffix) {
    static volatile LONG serial = 0;
    return L".rune-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
           std::to_wstring(InterlockedIncrement(&serial)) + suffix;
}

class ProjectStore {
public:
    // root_path ist der bestehende, vertrauenswürdige Engine-Installationsordner
    // in UTF-8; project ist ein UTF-16-Einzelname. project == nullptr
    // wählt die Demo-Daten unter game/data statt projekte/<name>/data.
    Result open(const std::string& root_path, const std::wstring* project) {
        texture_dir_.reset(); data_.reset(); project_.reset(); container_.reset(); root_.reset();
        std::wstring root;
        if (!to_wide(root_path, root)) return Result::Invalid;
        std::wstring full;
        if (!absolute_path(root, full)) return Result::Invalid;
        root_.reset(CreateFileW(full.c_str(), FILE_LIST_DIRECTORY | FILE_TRAVERSE | FILE_READ_ATTRIBUTES,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (!root_.valid()) return Result::Io;
        Result r = inspect(root_.get(), true);
        if (r != Result::Ok) return r;
        r = open_directory(root_.get(), project ? L"projekte" : L"game", container_);
        if (r != Result::Ok) return r;
        HANDLE parent = container_.get();
        if (project) {
            r = open_directory(parent, *project, project_);
            if (r != Result::Ok) return r;
            parent = project_.get();
        }
        return open_directory(parent, L"data", data_);
    }

    // Nicht besitzendes Handle; bleibt nur gültig, solange dieser Store lebt.
    // Karten-/Datenbanktransaktionen können so denselben geprüften Projekt-
    // anker verwenden, ohne eine absolute Pfadauflösung nachzuholen.
    HANDLE data_directory() const { return data_.get(); }

    Result read_manifest(std::string& out) {
        return read_child(data_.get(), L"textures.json", world::TEXTURE_MAX_MANIFEST_BYTES, out);
    }

    Result read_texture(const std::string& filename, std::size_t expected, std::string& out) {
        if (!editor::texture_file_is_flat(filename) || expected > world::TEXTURE_MAX_BYTES) return Result::Invalid;
        std::wstring name;
        if (!to_wide(filename, name)) return Result::Invalid;
        Result r = texture_directory(false);
        if (r != Result::Ok) return r;
        r = read_child(texture_dir_.get(), name, expected, out);
        if (r == Result::Ok && out.size() != expected) { out.clear(); return Result::Invalid; }
        return r;
    }

    // Erstellt immer eine neue Datei. Bereits vorhandene Dateien, Hardlinks
    // und Reparsepunkte werden niemals überschrieben. Das Handle bleibt bis
    // zum erfolgreichen Metadaten-Commit gesperrt und gehört dem Aufrufer.
    Result create_texture(const std::string& preferred, const std::string& data,
                          std::string& actual, Handle& created) {
        if (!editor::texture_file_is_flat(preferred) || data.empty() || data.size() > world::TEXTURE_MAX_BYTES)
            return Result::Invalid;
        actual.clear();
        Result r = texture_directory(true);
        if (r != Result::Ok) return r;
        const std::size_t dot = preferred.rfind('.');
        const std::string stem = dot == std::string::npos ? preferred : preferred.substr(0, dot);
        const std::string ext = dot == std::string::npos ? "" : preferred.substr(dot);
        for (unsigned i = 1; i <= 10000; ++i) {
            const std::string candidate = i == 1 ? preferred : stem + "_" + std::to_string(i) + ext;
            std::wstring name;
            if (!editor::texture_file_is_flat(candidate) || !to_wide(candidate, name)) return Result::Invalid;
            actual = candidate; // mögliche Allokation vor dem exklusiven Anlegen
            r = open_relative(texture_dir_.get(), name, FILE_WRITE_DATA | FILE_READ_ATTRIBUTES | DELETE,
                              0, FILE_CREATE, FILE_NON_DIRECTORY_FILE, created);
            if (r == Result::Conflict) continue;
            if (r != Result::Ok) return r;
            if (inspect(created.get(), false) != Result::Ok || !write_locked(created.get(), data)) {
                delete_owned_handle(created.get());
                created.reset();
                return Result::Io;
            }
            return Result::Ok;
        }
        return Result::Conflict;
    }

    // Optimistischer Bytevergleich unter einem exklusiven Schreib-/Rename-
    // Schutz. Altes Ziel über dessen Handle in eine einzigartige Sicherung
    // verschieben; danach die fertige neue Datei ohne Ersetzen einhängen.
    // Ein konkurrierender Neuanleger gewinnt niemals ein Überschreiben. Falls
    // der Rückweg blockiert ist, bleibt recovery als Wiederherstellung erhalten.
    // Zwischen beiden Renames kann der Name kurz fehlen; ein Crash kann eine
    // .rune-*.bak hinterlassen. Das ist kein atomarer Namensaustausch.
    Result save_manifest(bool was_present, const std::string& was, const std::string& data,
                         std::wstring& recovery, void (*before_promote)(void*) = nullptr, void* context = nullptr) {
        recovery.clear();
        if (data.size() > world::TEXTURE_MAX_MANIFEST_BYTES) return Result::TooLarge;
        Handle original;
        Result r = open_relative(data_.get(), L"textures.json", FILE_READ_DATA | FILE_READ_ATTRIBUTES | DELETE,
                                 FILE_SHARE_READ, FILE_OPEN, 0, original);
        if (r == Result::Missing && was_present) return Result::Conflict;
        if (r != Result::Ok && r != Result::Missing) return r;
        if (r == Result::Ok && !was_present) return Result::Conflict;
        if (r == Result::Ok) {
            std::string current;
            r = read_locked(original.get(), world::TEXTURE_MAX_MANIFEST_BYTES, current);
            if (r != Result::Ok) return r;
            if (current != was) return Result::Conflict;
        }
        Handle staged;
        DeleteUncommitted staged_guard{staged};
        std::wstring temp;
        for (int i = 0; i < 100; ++i) {
            temp = unique_name(L".tmp");
            r = open_relative(data_.get(), temp, FILE_WRITE_DATA | FILE_READ_ATTRIBUTES | DELETE,
                              0, FILE_CREATE, FILE_NON_DIRECTORY_FILE, staged);
            if (r != Result::Conflict) break;
        }
        if (r != Result::Ok) return r;
        if (inspect(staged.get(), false) != Result::Ok || !write_locked(staged.get(), data)) {
            if (!delete_owned_handle(staged.get())) recovery = temp;
            return Result::Io;
        }
        std::wstring backup;
        if (original.valid()) {
            bool renamed = false;
            for (int i = 0; i < 100; ++i) {
                backup = unique_name(L".bak");
                if (rename_same_directory(original.get(), data_.get(), backup)) { renamed = true; break; }
                const DWORD error = GetLastError();
                if (error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS) break;
            }
            if (!renamed) {
                if (!delete_owned_handle(staged.get())) recovery = temp;
                return Result::Io;
            }
        }
        if (before_promote) before_promote(context); // deterministischer Konflikttest
        if (!rename_same_directory(staged.get(), data_.get(), L"textures.json")) {
            const bool restored = !original.valid() || rename_same_directory(original.get(), data_.get(), L"textures.json");
            if (!restored) recovery = std::move(backup);
            if (!delete_owned_handle(staged.get()) && recovery.empty()) recovery = std::move(temp);
            return Result::Conflict;
        }
        staged_guard.committed = true; // staged trägt jetzt den gültigen Zielnamen
        if (original.valid() && !delete_owned_handle(original.get())) recovery = std::move(backup);
        return Result::Ok;
    }

private:
    Result read_child(HANDLE parent, const std::wstring& name, std::size_t maximum, std::string& out) {
        out.clear();
        if (!parent || parent == INVALID_HANDLE_VALUE) return Result::Io;
        Handle file;
        const Result r = open_relative(parent, name, FILE_READ_DATA | FILE_READ_ATTRIBUTES,
                                       FILE_SHARE_READ, FILE_OPEN, 0, file);
        return r == Result::Ok ? read_locked(file.get(), maximum, out) : r;
    }

    Result texture_directory(bool create) {
        if (texture_dir_.valid()) return Result::Ok;
        Result r = open_directory(data_.get(), L"textures", texture_dir_);
        if (r != Result::Missing || !create) return r;
        r = open_relative(data_.get(), L"textures", FILE_LIST_DIRECTORY | FILE_TRAVERSE | FILE_READ_ATTRIBUTES,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_CREATE, FILE_DIRECTORY_FILE, texture_dir_);
        if (r == Result::Conflict) return open_directory(data_.get(), L"textures", texture_dir_);
        if (r != Result::Ok) return r;
        r = inspect(texture_dir_.get(), true);
        if (r != Result::Ok) texture_dir_.reset();
        return r;
    }

    Handle root_, container_, project_, data_, texture_dir_;
};

} // namespace rune::assets
#endif
