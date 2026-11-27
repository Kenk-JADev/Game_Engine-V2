#pragma once
// Karten und F9: Beobachtung, Rückfrage und Commit mit einem gebundenen
// Projektverzeichnis. Kein fremdes Ziel durch ein zwischenzeitliches Rennen
// ersetzen oder löschen; bei blockierter Rücksetzung Sicherungsnamen melden.
#ifdef _WIN32
#include "core/project_assets_win.h"
#include "editor/json_validation.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace rune::editor {

// Der Attribut-Handle hält die beobachtete Dateiidentität während einer
// Rückfrage am Leben, erlaubt aber anderen Programmen Lesen/Schreiben/Rename.
// Eine nach dem Dialog neu angelegte Datei mit gleichen Bytes ist nicht die
// bestätigte Datei. Der Handle selbst wird niemals zum Schreiben benutzt.
struct DocumentSnapshot {
    bool present = false;
    std::string bytes;
    FILE_ID_INFO id{};
    assets::Handle identity;
};

struct DocumentRecovery {
    std::wstring backup; // alte Datei, falls der Rückweg blockiert ist
    std::wstring temp;   // eigene vorbereitete Datei, falls Löschen scheitert
};

// Nur für deterministische Windows-Rennentests; Hooks dürfen nicht werfen.
struct DocumentTestHooks {
    void (*after_compare)(void*) noexcept = nullptr;
    void (*before_publish)(void*) noexcept = nullptr;
    void* context = nullptr;
};

inline bool document_same_id(const FILE_ID_INFO& a, const FILE_ID_INFO& b) noexcept {
    return a.VolumeSerialNumber == b.VolumeSerialNumber &&
           std::memcmp(&a.FileId, &b.FileId, sizeof(a.FileId)) == 0;
}

inline bool document_file_id(HANDLE file, FILE_ID_INFO& id) noexcept {
    return GetFileInformationByHandleEx(file, FileIdInfo, &id, sizeof(id)) != 0;
}

// Ohne zweite Kopie der ganzen Datei unter Schreib-/Delete-Sperre vergleichen.
inline bool document_locked_equals(HANDLE file, const std::string& expected, bool& same) noexcept {
    same = false;
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(file, &length) || length.QuadPart < 0) return false;
    if (static_cast<unsigned long long>(length.QuadPart) != expected.size()) return true;
    char buffer[65536];
    std::size_t offset = 0;
    while (offset < expected.size()) {
        const DWORD part = static_cast<DWORD>(std::min<std::size_t>(sizeof(buffer), expected.size() - offset));
        DWORD count = 0;
        if (!ReadFile(file, buffer, part, &count, nullptr) || !count) return false;
        if (std::memcmp(buffer, expected.data() + offset, count) != 0) return true;
        offset += count;
    }
    same = true;
    return true;
}

class DocumentStore {
public:
    // Der Engine-Anker ist vertraut. Darunter werden wie beim Materialstore
    // die Verzeichnisse einzeln ohne Delete-Sharing gehalten; Reparsepunkte
    // und Hardlinks am Dokument sind nicht erlaubt. map=true wählt data/maps.
    assets::Result open(const std::string& root, const std::string& project,
                        bool map, const std::string& filename) {
        directory_ = INVALID_HANDLE_VALUE;
        maps_.reset();
        name_.clear();
        std::wstring wide_project;
        if (!assets::to_wide(project, wide_project) || !assets::component(wide_project) ||
            !assets::to_wide(filename, name_) || !assets::component(name_)) return assets::Result::Invalid;
        assets::Result result = project_.open(root, &wide_project);
        if (result != assets::Result::Ok) return result;
        if (map) {
            result = assets::open_directory(project_.data_directory(), L"maps", maps_);
            if (result != assets::Result::Ok) return result;
            directory_ = maps_.get();
        } else {
            directory_ = project_.data_directory();
        }
        return assets::Result::Ok;
    }

    // Fehlend ist ein gültiger beobachteter Zustand. Dateityp, Bytes und ID
    // werden bei gesperrten Schreib-/Rename-Zugriffen gemeinsam ermittelt.
    // Danach bleibt nur ein Attribut-Handle mit sämtlichen Sharing-Rechten.
    assets::Result observe(DocumentSnapshot& out) {
        out = DocumentSnapshot{};
        if (!valid()) return assets::Result::Invalid;
        assets::Handle reader;
        assets::Result result = assets::open_relative(directory(), name_, FILE_READ_DATA | FILE_READ_ATTRIBUTES,
                                                      FILE_SHARE_READ, FILE_OPEN, 0, reader);
        if (result == assets::Result::Missing) return assets::Result::Ok;
        if (result != assets::Result::Ok) return result;
        DocumentSnapshot snapshot;
        try {
            result = assets::read_locked(reader.get(), std::numeric_limits<std::size_t>::max(), snapshot.bytes);
        } catch (const std::bad_alloc&) {
            return assets::Result::TooLarge;
        } catch (const std::length_error&) {
            return assets::Result::TooLarge;
        }
        if (result != assets::Result::Ok) return result;
        if (!document_file_id(reader.get(), snapshot.id)) return assets::Result::Io;
        result = assets::open_relative(directory(), name_, FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN, 0, snapshot.identity);
        if (result != assets::Result::Ok) return result;
        FILE_ID_INFO pinned{};
        result = assets::inspect(snapshot.identity.get(), false);
        if (result != assets::Result::Ok || !document_file_id(snapshot.identity.get(), pinned))
            return result == assets::Result::Ok ? assets::Result::Io : result;
        if (!document_same_id(snapshot.id, pinned)) return assets::Result::Conflict;
        snapshot.present = true;
        out = std::move(snapshot);
        return assets::Result::Ok;
    }

    // Vergleich nach einer etwaigen Rückfrage erneut unter Schreib-/Delete-
    // Sperre. Die alte Datei wird per Handle gesichert, die neue nur ohne
    // Überschreiben eingehängt. Das ist KEIN atomarer Namensaustausch.
    assets::Result save(const DocumentSnapshot& expected, const std::string& bytes,
                        DocumentRecovery& recovery, const DocumentTestHooks* hooks = nullptr) {
        recovery = DocumentRecovery{};
        assets::Handle original;
        assets::Result result = lock_expected(expected, original);
        if (result != assets::Result::Ok) return result;
        if (hooks && hooks->after_compare) hooks->after_compare(hooks->context);

        assets::Handle staged;
        assets::DeleteUncommitted guard{staged};
        std::wstring temp;
        try {
            for (int i = 0; i < 100; ++i) {
                temp = assets::unique_name(L".tmp");
                result = assets::open_relative(directory(), temp, FILE_WRITE_DATA | FILE_READ_ATTRIBUTES | DELETE,
                                               0, FILE_CREATE, FILE_NON_DIRECTORY_FILE, staged);
                if (result != assets::Result::Conflict) break;
            }
        } catch (const std::bad_alloc&) {
            return assets::Result::TooLarge;
        } catch (const std::length_error&) {
            return assets::Result::TooLarge;
        }
        if (result != assets::Result::Ok) return result;
        if (assets::inspect(staged.get(), false) != assets::Result::Ok || !assets::write_locked(staged.get(), bytes)) {
            discard_staged(staged, guard, temp, recovery);
            return assets::Result::Io;
        }
        std::wstring backup;
        if (original.valid()) {
            result = backup_original(original.get(), backup);
            if (result != assets::Result::Ok) {
                discard_staged(staged, guard, temp, recovery);
                return result;
            }
        }
        // Ab hier keine allokierenden Operationen mehr, bis entweder das Ziel
        // veröffentlicht oder ein möglicher Rückweg dokumentiert ist.
        if (hooks && hooks->before_publish) hooks->before_publish(hooks->context);
        if (!assets::rename_relative_component(staged.get(), directory(), name_)) {
            const DWORD error = GetLastError();
            if (original.valid() && !assets::rename_relative_component(original.get(), directory(), name_))
                recovery.backup = std::move(backup);
            discard_staged(staged, guard, temp, recovery);
            return error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS
                       ? assets::Result::Conflict : assets::Result::Io;
        }
        guard.committed = true;
        if (original.valid() && !assets::delete_owned_handle(original.get()))
            recovery.backup = std::move(backup);
        return assets::Result::Ok;
    }

    // Erst die bestätigte, gesperrte Datei umbenennen; dann ausschließlich
    // dieses Handle löschen. Ein neu angelegtes Ziel wird nie gelöscht.
    assets::Result erase(const DocumentSnapshot& expected, DocumentRecovery& recovery,
                         const DocumentTestHooks* hooks = nullptr) {
        recovery = DocumentRecovery{};
        if (!expected.present) return assets::Result::Missing;
        assets::Handle original;
        assets::Result result = lock_expected(expected, original);
        if (result != assets::Result::Ok) return result;
        if (hooks && hooks->after_compare) hooks->after_compare(hooks->context);
        std::wstring backup;
        result = backup_original(original.get(), backup);
        if (result != assets::Result::Ok) return result;
        if (hooks && hooks->before_publish) hooks->before_publish(hooks->context);
        if (!assets::delete_owned_handle(original.get())) {
            if (!assets::rename_relative_component(original.get(), directory(), name_))
                recovery.backup = std::move(backup);
            return assets::Result::Io;
        }
        return assets::Result::Ok;
    }

private:
    HANDLE directory() const { return directory_; }
    bool valid() const { return !name_.empty() && directory_ && directory_ != INVALID_HANDLE_VALUE; }

    assets::Result lock_expected(const DocumentSnapshot& expected, assets::Handle& original) {
        if (!valid() || (expected.present && !expected.identity.valid())) return assets::Result::Invalid;
        assets::Result result = assets::open_relative(directory(), name_, FILE_READ_DATA | FILE_READ_ATTRIBUTES | DELETE,
                                                       FILE_SHARE_READ, FILE_OPEN, 0, original);
        if (result == assets::Result::Missing)
            return expected.present ? assets::Result::Conflict : assets::Result::Ok;
        if (result != assets::Result::Ok) return result;
        if (!expected.present) return assets::Result::Conflict;
        result = assets::inspect(original.get(), false);
        if (result != assets::Result::Ok) return result;
        FILE_ID_INFO actual{};
        if (!document_file_id(original.get(), actual)) return assets::Result::Io;
        if (!document_same_id(expected.id, actual)) return assets::Result::Conflict;
        bool equal = false;
        if (!document_locked_equals(original.get(), expected.bytes, equal)) return assets::Result::Io;
        return equal ? assets::Result::Ok : assets::Result::Conflict;
    }

    assets::Result backup_original(HANDLE original, std::wstring& backup) {
        try {
            for (int i = 0; i < 100; ++i) {
                backup = assets::unique_name(L".bak");
                if (assets::rename_relative_component(original, directory(), backup)) return assets::Result::Ok;
                const DWORD error = GetLastError();
                if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) return assets::Result::Io;
            }
        } catch (const std::bad_alloc&) {
            return assets::Result::TooLarge;
        } catch (const std::length_error&) {
            return assets::Result::TooLarge;
        }
        return assets::Result::Io;
    }

    static void discard_staged(assets::Handle& staged, assets::DeleteUncommitted& guard,
                               std::wstring& temp, DocumentRecovery& recovery) noexcept {
        if (!assets::delete_owned_handle(staged.get())) recovery.temp = std::move(temp);
        guard.committed = true;
        staged.reset();
    }

    assets::ProjectStore project_;
    assets::Handle maps_;
    HANDLE directory_ = INVALID_HANDLE_VALUE;
    std::wstring name_;
};

struct DocumentOutcome {
    assets::Result result = assets::Result::Io;
    bool cancelled = false;
};

// Derselbe Ablauf für Karten und F9, auch in den Windows-Tests: Die Rückfrage
// bezieht sich auf genau den beobachteten Stand. Ein Ja erlaubt nicht, einen
// während des Dialogs erneut veränderten Stand zu überschreiben.
template<class Confirm>
DocumentOutcome document_save_confirmed(DocumentStore& store, bool baseline_present,
                                        const std::string& baseline, const std::string& output,
                                        Confirm&& confirm, DocumentRecovery& recovery,
                                        const DocumentTestHooks* hooks = nullptr) {
    recovery = DocumentRecovery{};
    DocumentSnapshot current;
    const assets::Result observed = store.observe(current);
    if (observed != assets::Result::Ok) return {observed, false};
    if (editor_disk_changed(baseline_present, baseline, current.present, current.bytes) &&
        !std::forward<Confirm>(confirm)()) return {assets::Result::Ok, true};
    return {store.save(current, output, recovery, hooks), false};
}

template<class Confirm>
DocumentOutcome document_erase_confirmed(DocumentStore& store, bool baseline_present,
                                         const std::string& baseline, Confirm&& confirm,
                                         DocumentRecovery& recovery, const DocumentTestHooks* hooks = nullptr) {
    recovery = DocumentRecovery{};
    DocumentSnapshot current;
    const assets::Result observed = store.observe(current);
    if (observed != assets::Result::Ok) return {observed, false};
    if (!current.present) return {assets::Result::Missing, false};
    if (editor_disk_changed(baseline_present, baseline, true, current.bytes) &&
        !std::forward<Confirm>(confirm)()) return {assets::Result::Ok, true};
    return {store.erase(current, recovery, hooks), false};
}

} // namespace rune::editor
#endif
