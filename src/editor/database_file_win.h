#pragma once
// Dateizustand für den F9-Dialog. Fehler sind nicht mit einer fehlenden Datei
// gleichzusetzen, damit ein nicht lesbares Original nicht überschrieben wird.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#include <string>
#include <utility>

namespace rune::editor {

enum class DatabaseDiskState { Missing, Ready, Error };

inline DatabaseDiskState db_read_disk(const std::wstring& path, std::string& out) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        if (GetLastError() != ERROR_FILE_NOT_FOUND) return DatabaseDiskState::Error;
        out.clear();
        return DatabaseDiskState::Missing;
    }
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) return DatabaseDiskState::Error;
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f) return DatabaseDiskState::Error;
    std::string text;
    char buf[65536];
    std::size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
    const bool read_ok = std::ferror(f) == 0;
    const bool close_ok = std::fclose(f) == 0;
    if (!read_ok || !close_ok) return DatabaseDiskState::Error;
    out = std::move(text);
    return DatabaseDiskState::Ready;
}

} // namespace rune::editor
#endif
