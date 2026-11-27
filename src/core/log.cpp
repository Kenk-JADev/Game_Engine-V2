#include "core/log.h"

#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>
#include <share.h>

#include <windows.h>

namespace rune::core {
namespace {

std::mutex g_mutex;
std::FILE* g_file = nullptr;

const char* level_tag(int lvl) {
    switch (lvl) {
        case 0: return "INFO";
        case 1: return "WARN";
        default: return "ERROR";
    }
}

void write_line(int lvl, const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_mutex);

    std::time_t t = std::time(nullptr);
    std::tm tmv{};
    localtime_s(&tmv, &t);

    char stamp[32];
    std::snprintf(stamp, sizeof(stamp), "%02d:%02d:%02d", tmv.tm_hour, tmv.tm_min, tmv.tm_sec);

    std::string line = std::string("[") + stamp + "][" + level_tag(lvl) + "] " + msg;

    std::fprintf(stdout, "%s\n", line.c_str());
    std::fflush(stdout);

    OutputDebugStringA(line.c_str());
    OutputDebugStringA("\n");

    if (g_file) {
        std::fprintf(g_file, "%s\n", line.c_str());
        std::fflush(g_file);
    }
}

} // namespace

void log_open(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) {
        std::fclose(g_file);
        g_file = nullptr;
    }
    // Etappe 18j: _wfsopen mit _SH_DENYNO statt fopen_s("w") - fopen_s
    // sperrt die Datei exklusiv, weshalb Live-Tests das Log während des
    // Editor-Laufs nicht lesen konnten (alle bisherigen Live-Skripte mussten
    // den Editor erst beenden). So bleibt die Datei für Leser offen.
    std::wstring wide(path.size() + 1, L'\0');
    const int n = MultiByteToWideChar(CP_UTF8, 0, path.c_str(),
                                      static_cast<int>(path.size()), &wide[0],
                                      static_cast<int>(path.size()));
    wide.resize(n > 0 ? n : 0);
    // (MinGW-Signatur: _wfsopen liefert FILE*, anders als MSVCs errno_t-Variante)
    if (!(g_file = _wfsopen(wide.c_str(), L"w", _SH_DENYNO))) {
        std::fprintf(stderr, "Logdatei konnte nicht geöffnet werden: %s\n", path.c_str());
    }
}

void log_close() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) {
        std::fclose(g_file);
        g_file = nullptr;
    }
}

void log_info(const std::string& msg) { write_line(0, msg); }
void log_warn(const std::string& msg) { write_line(1, msg); }
void log_error(const std::string& msg) { write_line(2, msg); }

} // namespace rune::core
