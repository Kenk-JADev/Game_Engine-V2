# Etappe 22 — Checkliste (Ton-Reste: ME, SE-Stop, Merken, MP3)

Diese Checkliste ist die Abhakliste für Etappe 22. Stand: **Runde 40 —
abgeschlossen** (CI grün: Lauf 45603462533, alle vier Jobs).

Legende: `[x]` erledigt und committet · `[~]` erledigt, CI offen · `[ ]` offen

**Vorgänger:** Etappe 21 (Ton aus Dateien) ist **abgeschlossen** — WAV, OGG,
Pitch, Fade über Zeit, Karten-BGM/BGS (`MapAudio`, F2-Formular) und die
Event-Befehle 241/242/245/246; Register **65 Typen**, CI grün, letzter Commit
`1b62897`.

**Ziel:** Nach Etappe 21 klingen Dateien, aber die restliche Audio-API von XP
ist noch Schale: `Audio.se_stop` ist ein No-Op, „ME abspielen" (XP 249) kennt
kein eigenes Verhalten, `$game_system.bgm_memorize`/`bgm_restore` (XP 247/248)
fehlen, und MP3 wird weiter abgelehnt. Etappe 22 schließt diese Lücken, damit
das Thema Ton in `docs/XP_VERGLEICH.md` bis auf bewusste Ausnahmen ✅ ist.

## 1. Aufgaben

- [x] **T1 — SE-Stop und ME (XP 249/251):** `Audio.se_play` merkt sich, welche
  Einmal-Sounds laufen; `Audio.se_stop` stoppt sie wirklich (Datei und
  prozedural). Neues `Audio.me_stop`. `Audio.me_play` verhält sich wie RGSS:
  ein ME verstummt die laufende Musik (der ME liegt auf dem BGM-Kanal), die
  Musik wird **nicht** automatisch fortgesetzt. Kandidatenordner kommen dazu:
  `audio/bgs/`, `audio/me/`.
- [x] **T2 — Musik merken und zurückholen (XP 247/248):**
  `Game_System#bgm_memorize`/`bgm_restore`, `bgs_memorize`/`bgs_restore`,
  `playing_bgm`/`playing_bgs` sowie XP-Regel „`bgm_play(nil)` bzw. leerer Name
  stoppt". Merker sind flüchtig (kein Spielstand), wie in XP.
- [x] **T3 — Kampf-Musik:** Vor dem Kampf BGM/BGS merken, im Kampf
  `$game_system.battle_bgm` spielen (wenn gesetzt) und nach dem Kampf
  wiederherstellen; bei Sieg `battle_end_me` spielen, wenn gesetzt. Guards
  (`defined?($game_system)`) halten kopflose Tests ohne RGSS-Schicht ruhig.
- [x] **T4 — MP3 (Lettern-Ersatz):** `minimp3` (CC0, single header) vendoren,
  Wrapper `src/core/mp3.h` (parallel zu `ogg.h`), `sound::load_audio` erkennt
  MP3 an `ID3`/Frame-Sync, `AudioFiles` nimmt `.mp3` an, MapCheck warnt nicht
  mehr. Test `tools/mp3_test.cpp` mit eingebetteter Fixture.
- [x] **T5 — Tests:** `tools/rgss_test.rb` (SE-Stop, ME, Merken/Zurückholen),
  `tools/battle_test.rb` (Memorize/Restore nur mit `$game_system`),
  `tools/audio_files_test.rb` (Kandidatenordner me/bgs), `tools/mp3_test.cpp`,
  `json_test` bleibt unverändert (keine neuen Befehle).
- [x] **T6 — Doku:** `architecture.md` (Ton-Abschnitt), `XP_VERGLEICH.md`
  (247/248/249/251 von ❌ auf ✅), `LOGIK_PRUEFUNG.md` (AD-Eintrag),
  `HANDOFF` (Commit-Tabelle, nächste Etappe).

### Ergebnis je Aufgabe

- **T1:** `Audio` führt zwei kurze Listen laufender Einmal-Sounds
  (`@se`, `@me`, je max. 24 Einträge). `Audio.se_stop` und das neue
  `Audio.me_stop` stoppen Datei-Klänge über `AudioFiles.stop(name)` und
  prozedurale über `Rune::Sound.stop(basisname)`; ein zweites `se_stop` ist
  still. `Audio.me_play` verstummt zuerst die laufende Musik (RGSS: ME liegt
  auf dem BGM-Kanal) und setzt sie **nicht** fort, sondern vergisst sie nur in
  `MapAudio` (`MapAudio.vergiss(:bgm)` – der BGS-Teppich läuft weiter).
  `AudioFiles::UNTERORDNER` kennt jetzt `audio/bgs/` und `audio/me/`.
- **T2:** `GameSystem` hat `bgm_memorize`/`bgm_restore`, `bgs_memorize`/
  `bgs_restore`, `playing_bgm`/`playing_bgs`, `me_stop`; `bgm_play(nil)` und
  ein leerer Name stoppen (XP-Regel, über `leer?`). `bgm_fade`/`bgs_fade`
  rechnen jetzt **Sekunden → Millisekunden** (XP: `time * 1000`; der
  Interpreter ruft weiterhin `Audio.*` direkt mit Millisekunden). Alle vier
  Merker sind flüchtig und stehen nicht im Spielstand.
- **T3:** `Battle.musik_start` (vor dem Kampf: `bgm_memorize` + `battle_bgm`)
  und `Battle.musik_ende(sieg:)` (ME, dann `bgm_restore` – Reihenfolge wie
  `Scene_Battle#battle_end`); `start` ruft `musik_start`, `ende` und `abort!`
  rufen `musik_ende`, den ME nur bei `:sieg`. Ohne `$game_system`/`Audio`
  (kopflose Tests) bleiben beide Aufrufe still.
- **T4:** `src/core/minimp3.h` ist minimp3 (CC0, 1948 Zeilen, unverändert),
  `src/core/minimp3.cpp` schaltet die Implementierung ein (`-Werror`-sauber),
  `src/core/mp3.h` ist die Hülle: `ist_mp3` (ID3v2-Kennung oder Frame-Sync),
  `id3_laenge` (ID3v2-Kopf inkl. Fußzeile überspringen) und `parse`
  (Rahmenweise über `mp3dec_decode_frame`, mono-Mittelwert, `wav::resample`
  auf 22050 Hz, 60-s-Grenze). `sound::load_audio` erkennt das Format über
  `ogg::ist_ogg` → `mp3::ist_mp3` → `wav::parse`; `AudioFiles` und MapCheck
  nehmen `.mp3` an (MapCheck warnt nur noch vor fremden Endungen wie `.mid`).
  Gebaut wird `minimp3.cpp` über den `src/**/*.cpp`-Glob, in `build.ps1` steht
  es zusätzlich in der `embedded_texture_test`-Quellenliste, `mp3_test` ist in
  `build.ps1 -Test` und in `ci.yml` eingetragen.
- **T5:** `tools/mp3_test.cpp` (15 Checks, eingebettete 0,25-s-Stereo-Fixture:
  Erkennung, Dekodieren, Stereo→Mono, Resampling, ID3, Müll/Abschnitt);
  `rgss_test` +18 Checks (SE-Stop/ME/Merken/`playing_bgm`/Fade-Sekunden),
  `battle_test` +3 (Memorize → `battle_bgm`, ME+Restore nach Sieg, kein ME
  nach Abbruch), `map_audio_test` +1 (`vergiss`), `audio_files_test` +1
  (Ordner bgs/me) und MP3-Check umgestellt. `json_test` unverändert (860).
- **T6:** dieser Abschnitt, `architecture.md`, `XP_VERGLEICH.md` (249/251 ✅,
  247/248 ⚠️ – siehe unten), `LOGIK_PRUEFUNG.md` (AD.18/AD.19, Fazit Ton),
  `HANDOFF` mit Commit-Tabelle.

### Bewusst offen (Scope-Entscheidung von T6)

Die XP-Event-Befehle **243/244** (BGM sichern/zurückholen) und **247/248**
(BGM+BGS merken/wiederherstellen) gibt es im Register weiterhin nicht: Etappe
22 füllt die RGSS-Schicht (`GameSystem#bgm_memorize/restore`), die Kampfmusik
nutzt sie bereits. Die Befehle selbst brauchen Interpreter, MapCheck,
F6-Formular und Register-Pflege (65 → 69 Typen); sie kamen als eigene kleine
Etappe, damit dieser Schritt prüfbar bleibt. **In Etappe 23 umgesetzt**
(`save_bgm`/`resume_bgm`/`memorize_audio`/`restore_audio`, siehe
`docs/ETAPPE_23_CHECKLISTE.md`); `XP_VERGLEICH.md` führt 247/248 seitdem auf ✅.

## 2. Definition of Done

Ein Punkt ist fertig, wenn er im Kopf kommentiert ist, einen kopflosen Test
hat (Ruby über `tools/test_all.rb`, portables C++ über CI), im Register nichts
kaputt macht und die CI auf allen vier Jobs grün ist — inklusive
Windows-Build und WARP-Test.

## 3. Bekannte Eigenheiten (aus 21, für 22 ergänzt)

- Prozedurale Klänge bleiben Rückfall; ein Datei-Klang gleichen Namens hat
  Vorrang.
- `Audio.bgm_play` ersetzt eine laufende Musik; `MapAudio` startet dieselbe
  Karte+Datei nicht neu.
- `Audio.bgm_fade` rechnet in RGSS-Millisekunden, die Event-Befehle liefern
  Sekunden; `GameSystem#bgm_fade` nimmt Sekunden und rechnet selbst um.
- `Audio.me_play` verstummt die BGM und setzt sie nicht fort (RGSS-Verhalten).
  Wer nach einem ME die alte Musik will, ruft `bgm_restore` – genau das macht
  `Battle.musik_ende`.
- `se_stop`/`me_stop` arbeiten über den gemerkten Namen der Engine
  (`datei:<pfad>` bzw. Basisname). Eine Stimme, die von Hand über
  `Rune::Sound.play` gestartet wurde, kennt die Liste nicht.
- MP3 wird wie WAV/OGG auf **60 s gekürzt**; XP-Musik ist länger, läuft hier
  aber ohnehin in Schleife.

## 4. Reihenfolge

T1 (SE/ME) → T2 (Merken) → T3 (Kampf, baut auf T2) → T4 (MP3 zuletzt, weil
Vendoring dazukommt) → T5/T6 mit jedem Schritt.
