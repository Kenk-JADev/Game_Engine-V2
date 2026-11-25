# Etappe 20f — Checkliste (Bildschirm-Ebenen)

Diese Checkliste ist die Abhakliste für Etappe 20f. Stand: **Runde 31 —
geplant, noch nichts umgesetzt**.

Legende: `[x]` erledigt und committet · `[~]` erledigt, CI offen · `[ ]` offen

**Vorgänger:** Etappe 20e (Shop 302, Namenseingabe 303) ist **abgeschlossen**
(Register 51 Typen, CI grün, Commit `2e6f848`).

**Ziel:** Die XP-„Bildschirm“-Gruppe wird echt: Bildschirmton (223), Blitz
(224), Bildschirmwackeln (225), Bilder (231–235), die Ergebnis-Codes 601–603
und der Befehl „Menü öffnen“ (351).

## 0. Definition of Done (wie 20d/20e)

Register → `CMD_TYPES` → Interpreter → `ISSUE_CMD_TYPES` → `docs/befehle.json`
→ Formular (Lesen/Schreiben/Labels/Hinweise/`ev_cmd_label`) → MapCheck →
`samples` in `tools/json_test.cpp` → Zählungen → kopfloser Beweis-Test → Doku
(`architecture`, `XP_VERGLEICH`, `LOGIK_PRUEFUNG`, `HANDOFF`) → **CI grün**.

## 1. Plan in Scheiben

- [ ] **F1 — Bildschirmton (XP 223):** `{"cmd":"screen_tone","r":-34,"g":0,"b":68,"gray":0,"frames":60}`;
      Overlay-Farbe über der Welt (`UI.screen_tone`), Werte −255..255 (XP-Farbton),
      `frames` = Übergangsdauer in XP-Frames (40/s), Dauer 0 = sofort; Modul
      `ScreenFx` mit `tone`, `update(dt)`, `draw`
- [ ] **F2 — Blitz (XP 224):** `{"cmd":"flash","color":"weiss","strength":255,"frames":20}`;
      Farbwahl weiß/rot/grün/blau/gelb (XP-Farbtöne), Stärke 0..255, kurzer
      Alpha-Verlauf; im selben Modul wie F1
- [ ] **F3 — Wackeln (XP 225):** `{"cmd":"shake","power":4,"speed":5,"frames":40}`;
      Kamera-Offset in der UI-Schicht (oder dokumentiert als „ohne Kamera-Wackeln“,
      falls der Renderer das nicht hergibt) — **erst prüfen**, dann bauen
- [ ] **F4 — Bilder (XP 231–235):** `$game_screen.pictures` bekommt Wirkung:
      Bild zeigen/bewegen/drehen/färben/löschen als **Textur-Overlay** in der
      UI-Schicht. Voraussetzung: Bild-Lader für die UI (heute kann `Bitmap`
      keine Dateien laden — siehe `game/engine/graphics.rb` Kopf). Erst den
      Lader in `src/gfx/ui.cpp` + `src/ruby/vm.cpp` bauen, dann die Befehle
      (`picture_show/move/rotate/tone/erase`)
- [ ] **F5 — Ergebnis-Codes 601–603:** `if_shop_result`/`if_battle_result`-artige
      Zweige für Wahl, Kampf und Shop (XP: „Wenn Kampf gewonnen/verloren/geflohen“)
- [ ] **F6 — Menü öffnen (XP 351):** `{"cmd":"menu"}` über `$game_temp.menu_calling`
      (die Calling-Flags aus 20d existieren schon)
- [ ] **F7 — Tests:** `tools/screen_fx_test.rb` (Tone/Flash-Verlauf, Klemmen,
      Reihenfolge mehrerer Effekte), `tools/picture_test.rb` (Bilder über
      `$game_screen.pictures`), je Beispiel in `json_test`, `sync_test` grün
- [ ] **F8 — Doku:** `architecture.md`, `XP_VERGLEICH.md` (223/224/225/231–235,
      351, 601–603), `LOGIK_PRUEFUNG.md` (AD.13), `HANDOFF`

## 2. Bekannte Eigenheiten

- Die UI-Schicht (`UI.*`) zeichnet nur Primitive (`Rune::Gfx.rect/text`) — ein
  Bild-Lader fehlt noch; `Bitmap` in `graphics.rb` ist ein Merker ohne Dateien
  (Kopfkommentar dort lesen, bevor F4 beginnt).
- Ein Interpreter-Rahmen pro Update: Dauer-Effekte (`frames`) laufen deshalb
  über `ScreenFx.update(dt)` im Engine-Takt, nicht im Interpreter.
- XP-Frames sind 40/s (`Graphics.frame_rate`), Sekunden also `frames / 40.0`.
- Nach Editor-Änderungen ist `sync_test` rot, bis alle Registerstellen
  nachgezogen sind — erst Register, dann Tests.
