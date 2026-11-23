#pragma once
// Dateipfade werden innerhalb der Engine als gültiges UTF-8 übergeben und
// ausschließlich an der Windows-Grenze in UTF-16 umgewandelt. Keine ACP- oder
// verlustbehaftete Ersatzzeichen-Konvertierung für Dateizugriffe.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rune::core::win_path {

inline bool to_wide(std::string_view utf8, std::wstring& out) {
    out.clear();
    if (utf8.empty() || utf8.size() > 131040 || utf8.find('\0') != std::string_view::npos) return false;
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                                      static_cast<int>(utf8.size()), nullptr, 0);
    if (n <= 0 || n > 32760) return false;
    std::wstring result(static_cast<std::size_t>(n), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                            static_cast<int>(utf8.size()), result.data(), n) != n) return false;
    out = std::move(result);
    return true;
}

inline bool to_utf8(std::wstring_view wide, std::string& out) {
    out.clear();
    if (wide.empty() || wide.size() > 32760 || wide.find(L'\0') != std::wstring_view::npos) return false;
    const int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(),
                                      static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return false;
    std::string result(static_cast<std::size_t>(n), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(),
                            static_cast<int>(wide.size()), result.data(), n, nullptr, nullptr) != n) return false;
    out = std::move(result);
    return true;
}

inline bool full_path(const std::wstring& path, std::wstring& out) {
    const DWORD n = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (!n || n > 32760) return false;
    std::wstring result(static_cast<std::size_t>(n), L'\0');
    const DWORD written = GetFullPathNameW(path.c_str(), n, result.data(), nullptr);
    if (!written || written >= n) return false;
    result.resize(written);
    out = std::move(result);
    return true;
}

inline bool current_directory(std::wstring& out) {
    const DWORD n = GetCurrentDirectoryW(0, nullptr);
    if (!n || n > 32760) return false;
    std::wstring result(static_cast<std::size_t>(n), L'\0');
    const DWORD written = GetCurrentDirectoryW(n, result.data());
    if (!written || written >= n) return false;
    result.resize(written);
    out = std::move(result);
    return true;
}

inline bool executable_directory(std::wstring& out) {
    for (DWORD size = 256; size <= 32768; size *= 2) {
        std::wstring path(static_cast<std::size_t>(size), L'\0');
        const DWORD n = GetModuleFileNameW(nullptr, path.data(), size);
        if (!n) return false;
        if (n >= size) continue;
        path.resize(n);
        const std::size_t slash = path.find_last_of(L"\\/");
        if (slash == std::wstring::npos) return false;
        // Bei C:\\programm.exe muss C:\\ und nicht C: erhalten bleiben.
        path.resize(slash == 2 && path.size() > 2 && path[1] == L':' ? 3 : slash);
        if (path.empty()) return false;
        out = std::move(path);
        return true;
    }
    return false;
}

inline bool directory_exists(const std::wstring& path) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// Spiel bevorzugt die aktuelle Installation, Editor den Ort seiner EXE.
// Ein relativer ".."-Anteil wird vor der Rückgabe normalisiert.
inline bool find_engine_root(bool prefer_current, std::string& root) {
    root.clear();
    std::wstring cwd, exe, selected, normalized;
    const bool has_cwd = current_directory(cwd);
    const bool has_exe = executable_directory(exe);
    if (prefer_current && has_cwd && directory_exists(cwd + L"\\game")) selected = cwd;
    else if (has_exe && directory_exists(exe + L"\\game")) selected = exe;
    else if (has_exe && directory_exists(exe + L"\\..\\game")) selected = exe + L"\\..";
    else if (has_cwd) selected = cwd;
    else return false;
    return full_path(selected, normalized) && to_utf8(normalized, root);
}

inline bool set_current_directory(const std::string& path) {
    std::wstring wide;
    return to_wide(path, wide) && SetCurrentDirectoryW(wide.c_str());
}

inline bool path_exists(const std::string& path) {
    std::wstring wide;
    return to_wide(path, wide) && GetFileAttributesW(wide.c_str()) != INVALID_FILE_ATTRIBUTES;
}

inline bool create_directory(const std::string& path) {
    std::wstring wide;
    return to_wide(path, wide) && CreateDirectoryW(wide.c_str(), nullptr);
}

inline bool read_file(const std::string& path, std::string& out) {
    out.clear();
    std::wstring wide;
    if (!to_wide(path, wide)) return false;
    FILE* file = nullptr;
    if (_wfopen_s(&file, wide.c_str(), L"rb") != 0 || !file) return false;
    std::string data;
    char buf[65536];
    std::size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), file)) > 0) data.append(buf, n);
    const bool read_ok = std::ferror(file) == 0;
    const bool close_ok = std::fclose(file) == 0;
    if (!read_ok || !close_ok) return false;
    out = std::move(data);
    return true;
}

inline bool write_file(const std::string& path, const std::string& data) {
    std::wstring wide;
    if (!to_wide(path, wide)) return false;
    FILE* file = nullptr;
    if (_wfopen_s(&file, wide.c_str(), L"wb") != 0 || !file) return false;
    const std::size_t n = std::fwrite(data.data(), 1, data.size(), file);
    const bool close_ok = std::fclose(file) == 0;
    return n == data.size() && close_ok;
}

struct DirectoryEntry {
    std::string name;
    DWORD attributes = 0;
};

// Suchmuster wie "...\\maps\\*.json"; Einträge mit nicht darstellbarem
// UTF-16 werden übersprungen statt unter einem Ersatznamen zu erscheinen.
inline bool list(const std::string& pattern, std::vector<DirectoryEntry>& out) {
    out.clear();
    std::wstring wide;
    if (!to_wide(pattern, wide)) return false;
    WIN32_FIND_DATAW fd{};
    HANDLE handle = FindFirstFileW(wide.c_str(), &fd);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    }
    struct FindGuard { HANDLE h; ~FindGuard() { FindClose(h); } } guard{handle};
    do {
        if (fd.cFileName[0] == L'.') continue;
        std::string name;
        if (to_utf8(fd.cFileName, name)) out.push_back({std::move(name), fd.dwFileAttributes});
    } while (FindNextFileW(handle, &fd));
    if (GetLastError() != ERROR_NO_MORE_FILES) { out.clear(); return false; }
    return true;
}

} // namespace rune::core::win_path
#endif
