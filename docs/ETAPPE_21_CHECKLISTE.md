# Etappe 21 — Checkliste (Ton: Dateien statt Prozedur-Sounds)

Diese Checkliste ist die Abhakliste für Etappe 21. Stand: **Runde 39 — T1
(WAV), T2 (OGG), T3 (Karten-BGM/BGS mit F2-Formular), T4 (Befehle
241/242/245/246) und T5 (Pitch + Fade über Zeit) sind fertig**.

Legende: `[x]` erledigt und committet · `[~]` erledigt, CI offen · `[ ]` offen

**Vorgänger:** Etappe 20f (Bildschirm-Ebenen) ist **abgeschlossen** — Ton 223,
Blitz 224, Wackeln 225, Bilder 231–235, Ergebniszweige 601–603, Menü-Befehl
351; Register **61 Typen**, CI grün, Commit `47124b1`.

**Ziel:** XP spielt Musik und Geräusche aus **Dateien** (`Audio/BGM/*.ogg`).
Rune3D hat die RGSS-Schicht (`Audio.bgm_play`/`se_play`), aber
`src/core/sound.cpp` kennt nur **prozedural erzeugte** Klänge, die beim Start
angemeldet werden. Damit sind `bgm`/`se`-Befehle und Karten-BGM heute
substanzlos. Etappe 21 baut den Datei-Lader und hängt die XP-Befehle 241–246,
Pitch und `bgm_fade` daran.

## 0. Definition of Done (wie 20d–20f)

Register → `CMD_TYPES` → Interpreter → `ISSUE_CMD_TYPES` → `docs/befehle.json`
→ Formular (Lesen/Schreiben/Labels/Hinweise/`ev_cmd_label`) → MapCheck →
`samples` in `tools/json_test.cpp` → Zählungen → kopfloser Beweis-Test → Doku
(`architecture`, `XP_VERGLEICH`, `LOGIK_PRUEFUNG`, `HANDOFF`) → **CI grün**.

Dateizugriffe laufen über die **sichere Assets-Brücke**
(`Rune::Assets.read_texture` bzw. ein Gegenstück für Binärdateien), nie direkt
über `File.binread` im Spielpfad — die Tests dürfen Bytes einbetten.

## 1. Plan in Scheiben

- [x] **T1 — WAV-Lader in `src/core/sound.cpp`:** PCM (8/16 Bit, mono/stereo,
  11–48 kHz) parsen, auf das WaveOut-Format (22,05 kHz, 16 Bit, mono)
  resampeln, als benannten Klang anmelden (`Rune::Sound.load(name, bytes)`).
  Rückgabe `true/false`; Fehler landen als Warnung im Log und in `Issues`.
  Obergrenze: 60 s bzw. 8 MB pro Datei (XP-BGM-Längen reichen).
- [x] **T2 — OGG-Vorbis:** `stb_vorbis.c` (single header, public domain) unter
  `src/core/` einbinden, dekodieren und über denselben Resampler schicken.
  Umgesetzt (Runde 38): `src/core/stb_vorbis.cpp` (v1.25, Vendoring-Hinweis im
  Kopf, Lizenz am Dateiende) und `src/core/ogg.h` (`ist_ogg`, `parse` →
  `wav::Clip` mono/16 Bit/22050 Hz über `wav::resample`); `sound.cpp` erkennt
  das Format an den ersten Bytes (`load_wav` heißt jetzt `load_audio`), Ruby
  reicht OGG unverändert weiter, MP3 bleibt mit Warnung draußen. Test:
  `tools/ogg_test.cpp` (11 Checks, eingebettete 0,25-s-Stereo-Fixture) in CI
  und `build.ps1 -Test`.
- [x] **T3 — Karten-BGM/BGS:** Karten-JSON bekommt `bgm`/`bgs`
  (Datei + Lautstärke + Pitch). Beim Kartenwechsel startet das BGM, beim
  Verlassen stoppt es (`Audio.bgm_play`/`bgm_stop`), `bgs` läuft wie XP
  weiter. MapCheck: Dateiendung, Lautstärke 0–100, Pitch 50–150.
  Umgesetzt (Runde 37): `game/engine/map_audio.rb` (`enter`/`bgm`/`bgs`/
  `reset`), Aufruf in `Game.enter_map` nach `Weather.load_map`, MapCheck
  `pruefe_ton` (Objekt, Name, OGG/MP3-Warnung, Grenzen), `tools/map_audio_test.rb`.
  Dazu das Formular im F2-Dialog „Karten-Eigenschaften" (Gruppe „Ton
  (Kartenmusik)"): Name, Lautstärke und Pitch für BGM und BGS, leerer Name
  löscht den Eintrag, unbekannte Zusatzschlüssel im Objekt bleiben erhalten;
  `tools/editor_workflow_test.cpp` prüft das Modell (9 Checks).
- [x] **T4 — Event-Befehle 241/242/245/246:**
  `{"cmd":"play_bgm","name":"bgm/dorf.ogg","volume":100,"pitch":100}`,
  `{"cmd":"fadeout_bgm","seconds":2}`,
  `{"cmd":"play_bgs","name":"bgs/regen.ogg","volume":80}`,
  `{"cmd":"fadeout_bgs","seconds":1}`. Register/Formular/MapCheck wie immer;
  `bgm_fade` läuft über einen eigenen Takt (nicht im Interpreter blockierend).
  Umgesetzt: 65 Typen in `CMD_TYPES`/Register/`ISSUE_CMD_TYPES`/`EV_TYPES`,
  `ton_abspielen`/`ton_ausblenden` im Interpreter, MapCheck (Name, Lautstärke
  0–100, Pitch 50–150, `seconds >= 0`), F6-Felder Name + Wert 1/2 und je ein
  `json_test`-Beispiel pro Typ.
- [x] **T5 — Pitch und `bgm_fade` über Zeit:** Resampling-Faktor aus Pitch
  (50–150 %) über `wav.h::rate_fuer_pitch`; Fade über `AudioFiles.fade`/
  `fade_ms` + `AudioFiles.update(dt)` (nicht blockierend, Engine-Takt),
  pro Frame `Rune::Sound.volume(name, vol)`; danach Stopp und Kanal frei.
  `Audio.bgm_fade`/`bgs_fade` rechnen RGSS-Millisekunden in Sekunden um.
- [x] **T6 — Tests:** `tools/audio_test.rb` prüft den WAV-Parser an
  eingebetteten Bytes (8/16 Bit, mono/stereo, kaputte Header, zu lang),
  Pitch-/Fade-Rechnung als reine Funktionen und die Befehle über den
  Interpreter (mit `Rune::Sound`-Attrappe). `json_test` bekommt die vier
  Beispiele und je einen Grenzfall (Lautstärke 101, Pitch 20, `seconds` < 0).
  Umgesetzt: `tools/wav_test.cpp` (18), `tools/ogg_test.cpp` (11),
  `tools/audio_files_test.rb` (Fade/OGG), `tools/map_audio_test.rb` (12),
  `tools/event_test.rb` (+6 Ton-Befehle) und `json_test` (+10 Grenzfälle,
  860 Checks); ein eigener `tools/audio_test.rb` wurde bewusst nicht
  angelegt, weil die WAV-/OGG-Prüfungen portabel in C++ liegen (CI ohne Ruby
  schneller) und die Ruby-Seite in `audio_files_test` steckt.
- [x] **T7 — Doku:** `architecture.md` (Abschnitt „Ton“), `XP_VERGLEICH.md`
  (241–246 von ⚠️/❌ auf ✅), `LOGIK_PRUEFUNG.md` (neuer AD-Eintrag),
  `HANDOFF` (Commit-Tabelle, nächste Etappe).
  Umgesetzt: Abschnitt „Ton aus Dateien" in `architecture.md` (WAV, OGG,
  Fade, Befehle, Kartenmusik), 241–246 in `XP_VERGLEICH.md` auf ✅/⚠️,
  `LOGIK_PRUEFUNG.md` (AD.18 neu geschrieben, AD.19, Audit-Tabelle),
  `HANDOFF` mit Commit-Tabelle 21-1 … 21-7.

## 2. Stand (Runde 34)

Umgesetzt: `src/core/wav.h` (portabel) + `sound.cpp::load_audio` +
`Rune::Sound.load/known?`, `game/engine/audio_files.rb`, Anbindung in
`Audio.bgm_play`/`bgs_play`/`se_play`/`me_play` mit Rückfall auf die
prozeduralen Klänge, Pitch 50–150 % über Resampling, CI- und
`build.ps1`-Prüfung des WAV-Tests.

Fade über Zeit (Teil von T5): `sound::set_volume`/`volume_of`,
`Rune::Sound.volume(name, vol = nil)`, `AudioFiles.fade`/`fade_ms`/`update`/
`fade_aktiv?`/`stoppe_klang`, `AudioFiles.update(dt)` in `game/main.rb`, echte
`Audio.bgm_fade`/`bgs_fade` (RGSS-Millisekunden, ohne Datei sofortiger Stopp).
Ein neuer `loop`/`play` auf denselben Klang bricht einen laufenden Fade ab.

Befehle (T4): `play_bgm`/`fadeout_bgm`/`play_bgs`/`fadeout_bgs` in Register,
`CMD_TYPES`, `ISSUE_CMD_TYPES`, `EV_TYPES`, Interpreter, MapCheck und
F6-Formular; `Audio.bgm_play` ersetzt eine laufende Musik (XP 241);
`json_test` hat je Typ ein Beispiel und zehn Grenzfälle zu den Ton-Befehlen
(860 Checks), `tools/event_test.rb` prüft
Weiterreichen und Klemmung der Ton-Befehle (6 Checks).

Kartenmusik (T3, Runde 37): `map_audio.rb` startet `bgm`/`bgs` beim
Kartenwechsel, lässt die bisherige Musik weiterlaufen, wenn die Karte nichts
nennt, und startet dieselbe Datei nicht neu; MapCheck prüft die Kartenfelder,
`tools/map_audio_test.rb` deckt Start/Weiterlaufen/Klemmung ab.

OGG (T2, Runde 38): `src/core/stb_vorbis.cpp` (vendored) + `src/core/ogg.h`
dekodieren Vorbis nach mono/16 Bit/22050 Hz; `sound::load_audio` erkennt das
Format selbst, `tools/ogg_test.cpp` prüft Dekodieren, Stereo→Mono, Resampling
und Fehlerfälle.

**Etappe 21 ist damit fertig** — T1 bis T7 sind abgehakt, alle CI-Jobs grün
(Windows-Build inklusive). Bewusst offen geblieben (nicht Teil der Etappe):
MP3 (kein Dekoder), `me_play` (XP 249) und `se_stop` (251), BGS merken/
wiederherstellen (247/248).

## 3. Bekannte Eigenheiten

- Prozedurale Klänge (`musik`, `schritt`, `landung`, …) bleiben als Fallback
  registriert; ein Datei-Klang gleichen Namens ersetzt sie.
- WaveOut läuft mit **einem** Gerät und vier Puffern (`kBuffers`); lange BGMs
  müssen deshalb geloopt gemischt werden, nicht in einen Puffer kopiert.
- `Audio.bgm_play` übergibt heute nur den Basisnamen — T3/T4 müssen den
  Dateipfad mitgeben, damit der Lader die Datei findet.
- `bgm_fade` blockiert in XP nicht; die Dauer läuft im Engine-Takt
  (`Graphics.frame_rate`, 40/s), nicht im Interpreter — deshalb
  `AudioFiles.update(dt)` im Frame und nicht im Event-Befehl.
- `Audio.bgm_fade(time)` erwartet wie RGSS **Millisekunden** (das
  XP-Defaultscript ruft `time * 1000`); die Editor-Befehle 242/246 liefern
  Sekunden. Ein Fade auf einen prozeduralen Klang bleibt ein sofortiger Stopp.
- Nach einem Fade bleibt `@bgm`/`@bgs` gesetzt (XP räumt den Slot erst beim
  nächsten `bgm_play`/`bgm_stop`); der Klang selbst ist gestoppt.

## 3. Reihenfolge

T1 → T5 (Resampling/Pitch ist die Grundlage) → T6 (Parser-Tests) → T3 → T4 →
T2 (OGG) → T7.
