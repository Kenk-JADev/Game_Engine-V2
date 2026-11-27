# Etappe 20f — Checkliste (Bildschirm-Ebenen)

Diese Checkliste ist die Abhakliste für Etappe 20f. Stand: **Runde 33 — fertig**: F1–F3, F4 (Bilder 231–235), F5, F6; Register **61**. Offen: nur noch die Doku-Nachträge aus F7/F8 (Bilder in Handoff/Logik).

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

- [x] **F1 — Bildschirmton (XP 223):** `{"cmd":"screen_tone","r":-34,"g":0,"b":68,"gray":0,"frames":60}`;
      Overlay-Farbe über der Welt (`UI.screen_tone`), Werte −255..255 (XP-Farbton),
      `frames` = Übergangsdauer in XP-Frames (40/s), Dauer 0 = sofort; Modul
      `ScreenFx` mit `tone`, `update(dt)`, `draw` (Overlay über Welt und UI)
- [x] **F2 — Blitz (XP 224):** `{"cmd":"flash","color":"weiss","strength":255,"frames":20}`;
      Farbwahl weiß/rot/grün/blau/gelb (XP-Farbtöne), Stärke 0..255, kurzer
      Alpha-Verlauf; im selben Modul wie F1, `frames: 0` = ein Frame
- [x] **F3 — Wackeln (XP 225):** `{"cmd":"shake","power":4,"speed":5,"frames":40}`;
      umgesetzt als **Kamera-Versatz** (`Rune::Camera.shake`, in `camera.cpp`
      beim Bildaufbau auf Auge/Ziel addiert) — Picking bleibt unberührt,
      sinusförmiger Verlauf statt XP-Zufallssprünge
- [x] **F4 — Bilder (XP 231–235):** `show_picture`/`move_picture`/
      `rotate_picture`/`tint_picture`/`erase_picture`; Bildlader
      (`image_loader.rb`, PNG/BMP) meldet Pixel über `Rune::Gfx.image`, der
      Bild-Durchgang in `ui.cpp` zeichnet sie über der Welt und unter Fenstern.
      Reste: kein JPEG, Blend 1/2 zeichnen wie 0, Tint multipliziert
- [x] **F5 — Ergebnis-Codes 601–603:** `{"cmd":"if_battle_result","result":"sieg",
      "then":[],"else":[]}` — ein Befehl mit `result` (sieg/flucht/niederlage)
      statt drei Codes; ohne Kampf gilt der Sonst-Zweig
- [x] **F6 — Menü öffnen (XP 351):** `{"cmd":"menu"}` über `$game_temp.menu_calling`
      (die Calling-Flags aus 20d existieren schon)
- [x] **F7 — Tests:** `tools/screen_fx_test.rb` (Tone/Flash-Verlauf, Klemmen,
      Reihenfolge mehrerer Effekte), `tools/picture_test.rb` (Bilder über
      `$game_screen.pictures`), je Beispiel in `json_test`, `sync_test` grün
- [x] **F8 — Doku:** `architecture.md`, `XP_VERGLEICH.md` (223/224/225/231–235,
      351, 601–603), `LOGIK_PRUEFUNG.md` (AD.13), `HANDOFF`

## 2. Bekannte Eigenheiten

- Die UI-Schicht zeichnet Primitive (`Rune::Gfx.rect/text`) und seit 20f auch
  Bilder (`Rune::Gfx.image/picture`). `Bitmap` in `graphics.rb` bleibt ein
  Merker ohne Dateien — Bilddateien laufen ausschließlich über
  `ImageLoader` (PNG/BMP; JPEG bewusst nicht).
- Bilder werden **nach Nummer** gezeichnet (1 unten, 50 oben) und liegen unter
  HUD, Meldungen und Fenstern — wie XP.
- Ein Interpreter-Rahmen pro Update: Dauer-Effekte (`frames`) laufen deshalb
  über `ScreenFx.update(dt)` im Engine-Takt, nicht im Interpreter.
- XP-Frames sind 40/s (`Graphics.frame_rate`), Sekunden also `frames / 40.0`.
- Nach Editor-Änderungen ist `sync_test` rot, bis alle Registerstellen
  nachgezogen sind — erst Register, dann Tests.
