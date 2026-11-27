# Etappe 35 – Himmel, Wolken mit Schatten, Tag/Nacht, schärfere Schatten

Status: **Code geschrieben, noch nicht gebaut/getestet.**

## XP-Vorbild (Recherche)
Quellen: RPGA „RPG Maker XP Kurs Teil 2“ (Fogs), GDU „Lesson 3 – Adding
Atmosphere“, Steam-Guide (Nacht per Fog/Tileset), RPG-Maker-Blog „The Basics
of Tints & Picture Overlays“.

| XP | Rune3D (3D-Übersetzung) |
|---|---|
| Panorama (Hintergrund, Hue) | Grundfarbe des Himmels → echter Verlauf Zenit/Horizont |
| Fog-Grafik (Wolken/Nebel über der Karte; Opacity, Blend, Zoom, SX/SY) | prozedurale Wolkendecke in 160 m Höhe, zieht mit `cloud_speed` [SX, SY] und wirft **Schatten aufs Gelände** |
| Screen Tone (R/G/B −255..255, Grau 0..255) für Tag/Nacht/Stimmung | `tone` [R, G, B, Grau] auf das ganze Bild + Himmel |
| Tag/Nacht per Tönung (Community-Praxis) | `time` 0..24: Sonnenstand, Licht-/Himmelsfarbe, Dämmerung, Sterne, Mondlicht |

## Karten-JSON (alles optional)
```json
"sky": {"clouds": 0.4, "cloud_speed": [3, 1], "cloud_color": 16777215,
        "cloud_shadow": 0.45, "time": 17.5, "tone": [-20, -10, 10, 0]}
```
Ohne `sky`: bisheriges Licht, keine Wolken – aber der Himmel ist jetzt ein
Verlauf statt einer Vollfarbe (Editor und Spiel setzten vorher oben = unten).
Laufzeit: `Game.set_sky('time' => 21)` oder `Game.set_sky('tone' => [-60,-60,0,0])`.

## Technik
- `src/world/sky_settings.h`: reine Rechnung (`sky_look`) – Editor und Spiel nutzen dieselbe Funktion.
- `src/gfx/renderer.*`: Himmel blickabhängig (inverse View-Projection), Sonnenscheibe + Hof,
  Sterne, Wolken (fbm-Rauschen); Wolkenschatten im Szenen-Shader mit identischer Dichte;
  XP-Tönung in beiden Shadern.
- **Schärfere Schatten:** Schattenkarte folgt der Kamera (vorher fest 360 m um die
  Kartenmitte ≈ 18 cm/Texel), Bereich 110–360 m je Kamerahöhe, auf Texel eingerastet
  (kein Flimmern), 3×3-PCF, sanftes Ausblenden am Rand, kleinerer Bias.
- Ruby: `Rune::Gfx.sky_look(...)`, `Game.apply_sky_look`, `Game.set_sky`.

## F2-Fenster (Sofortkorrekturen aus dem Screenshot)
- Ton-Gruppe neu vermessen: „Lautstärke“, „Hintergrund“ und Hinweis waren abgeschnitten;
  jetzt „Musik (BGM)“ / „Klang (BGS)“ wie im XP.
- Farbfelder mit Windows-Farbwähler („…“); „Panorama-Farbe (hex)“ heißt „Himmelfarbe“.

## Nächste Fenster-Etappe (36) – geplant, nicht umgesetzt
1. **F2 mit Reitern** statt einer überladenen Seite: Allgemein (Name, Größe, Gelände) ·
   **Himmel & Licht** (Himmelfarbe, Wolken, Tageszeit, Tönung, Nebel, Wetter – inkl.
   Vorschau im 3D-Fenster) · Ton · Zufallskämpfe · Block-Zonen/Notiz. Bestehende
   Prüf- und Übernahmewege (`apply_props_from_dlg`) bleiben, `sky` kommt hinzu.
2. **Script-Editor wie XP:** nummerierte Liste aller Einträge (Defaults oben, Main unten),
   Suche Strg+F / Weitersuchen F3 / Ersetzen Strg+H, Rechtsklick Einfügen/Löschen,
   Markierung, welche Einträge beim Start geladen werden.
3. Alle Dialoge auf DPI-skalierte Koordinaten umstellen (heute feste Pixel).

## Prüfung (offen)
`json_test` (`sky_tests`), Build; im Spiel/Editor Karte mit `sky` laden: Wolken ziehen,
Schatten wandern, `time` 6/12/18/0 vergleichen, Schattenkanten nah an Häusern scharf.
