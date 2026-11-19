#pragma once
// Windows-Dateien erst vollständig neben dem Ziel schreiben und anschließend
// ersetzen. So kann ein Schreib-/Flushfehler das alte Ziel nicht verkürzen.
// Die vorherige Konfliktprüfung ist dadurch KEINE atomare Dateisperre.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <string>

namespace rune::editor {

inline bool editor_write_file_atomic(const std::wstring& path, const std::string& data,
                                     bool replace_existing = true) {
    static unsigned serial = 0;
    HANDLE file = INVALID_HANDLE_VALUE;
    std::wstring temp;
    for (int attempt = 0; attempt < 100; ++attempt) {
        temp = path + L".rune-" + std::to_wstring(GetCurrentProcessId()) +
               L"-" + std::to_wstring(++serial) + L".tmp";
        file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) break;
        const DWORD error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) return false;
    }
    if (file == INVALID_HANDLE_VALUE) return false;
    bool ok = true;
    size_t written = 0;
    while (written < data.size()) {
        const DWORD part = static_cast<DWORD>(std::min<size_t>(data.size() - written, 1024 * 1024));
        DWORD n = 0;
        if (!WriteFile(file, data.data() + written, part, &n, nullptr) || n == 0) { ok = false; break; }
        written += n;
    }
    if (ok && !FlushFileBuffers(file)) ok = false;
    if (!CloseHandle(file)) ok = false;
    const DWORD flags = MOVEFILE_WRITE_THROUGH | (replace_existing ? MOVEFILE_REPLACE_EXISTING : 0);
    if (ok && !MoveFileExW(temp.c_str(), path.c_str(), flags)) ok = false;
    if (!ok) DeleteFileW(temp.c_str());
    return ok;
}

} // namespace rune::editor
#endif
