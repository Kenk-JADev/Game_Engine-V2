# Etappe 22 — Checkliste (Ton-Reste: ME, SE-Stop, Merken, MP3)

Diese Checkliste ist die Abhakliste für Etappe 22. Stand: **Runde 40 — Plan
steht, Umsetzung beginnt**.

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

- [ ] **T1 — SE-Stop und ME (XP 249/251):** `Audio.se_play` merkt sich, welche
  Einmal-Sounds laufen; `Audio.se_stop` stoppt sie wirklich (Datei und
  prozedural). Neues `Audio.me_stop`. `Audio.me_play` verhält sich wie RGSS:
  ein ME verstummt die laufende Musik (der ME liegt auf dem BGM-Kanal), die
  Musik wird **nicht** automatisch fortgesetzt. Kandidatenordner kommen dazu:
  `audio/bgs/`, `audio/me/`.
- [ ] **T2 — Musik merken und zurückholen (XP 247/248):**
  `Game_System#bgm_memorize`/`bgm_restore`, `bgs_memorize`/`bgs_restore`,
  `playing_bgm`/`playing_bgs` sowie XP-Regel „`bgm_play(nil)` bzw. leerer Name
  stoppt". Merker sind flüchtig (kein Spielstand), wie in XP.
- [ ] **T3 — Kampf-Musik:** Vor dem Kampf BGM/BGS merken, im Kampf
  `$game_system.battle_bgm` spielen (wenn gesetzt) und nach dem Kampf
  wiederherstellen; bei Sieg `battle_end_me` spielen, wenn gesetzt. Guards
  (`defined?($game_system)`) halten kopflose Tests ohne RGSS-Schicht ruhig.
- [ ] **T4 — MP3 (Lettern-Ersatz):** `minimp3` (CC0, single header) vendoren,
  Wrapper `src/core/mp3.h` (parallel zu `ogg.h`), `sound::load_audio` erkennt
  MP3 an `ID3`/Frame-Sync, `AudioFiles` nimmt `.mp3` an, MapCheck warnt nicht
  mehr. Test `tools/mp3_test.cpp` mit eingebetteter Fixture.
- [ ] **T5 — Tests:** `tools/rgss_test.rb` (SE-Stop, ME, Merken/Zurückholen),
  `tools/battle_test.rb` (Memorize/Restore nur mit `$game_system`),
  `tools/audio_files_test.rb` (Kandidatenordner me/bgs), `tools/mp3_test.cpp`,
  `json_test` bleibt unverändert (keine neuen Befehle).
- [ ] **T6 — Doku:** `architecture.md` (Ton-Abschnitt), `XP_VERGLEICH.md`
  (247/248/249/251 von ❌ auf ✅), `LOGIK_PRUEFUNG.md` (AD-Eintrag),
  `HANDOFF` (Commit-Tabelle, nächste Etappe).

## 2. Definition of Done

Ein Punkt ist fertig, wenn er im Kopf kommentiert ist, einen kopflosen Test
hat (Ruby über `tools/test_all.rb`, portables C++ über CI), im Register nichts
kaputt macht und die CI auf allen vier Jobs grün ist — inklusive
Windows-Build und WARP-Test.

## 3. Bekannte Eigenheiten (aus 21)

- Prozedurale Klänge bleiben Rückfall; ein Datei-Klang gleichen Namens hat
  Vorrang.
- `Audio.bgm_play` ersetzt eine laufende Musik; `MapAudio` startet dieselbe
  Karte+Datei nicht neu.
- `Audio.bgm_fade` rechnet in RGSS-Millisekunden, die Event-Befehle liefern
  Sekunden.

## 4. Reihenfolge

T1 (SE/ME) → T2 (Merken) → T3 (Kampf, baut auf T2) → T4 (MP3 zuletzt, weil
Vendoring dazukommt) → T5/T6 mit jedem Schritt.
