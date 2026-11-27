# Etappe 25 — Event-Befehl XP 318 „Skills ändern“

**Stand:** Runde 44 — Implementierung lokal fertig. Es wurde bewusst nur der
teilweise vorhandene Eintrag 318 aus `docs/XP_VERGLEICH.md` bearbeitet;
bereits grüne XP-Punkte wurden nicht neu implementiert.

## Ergebnis

- [x] `Party.skill_def`, `forget_skill` und `change_skill` lösen Skills per
  ID/Name auf und lernen/vergessen sie für einen Held oder die ganze Party.
- [x] `actor_skill` im Interpreter: `name` (ID/Name), optionales `actor`
  (`alle`/Held), `mode` `add`/`remove` (Default `add`).
- [x] MapCheck validiert Pflichtfeld, Datenbank-Skill, Actor-Ziel und Modus.
- [x] F6 zeigt Fertigkeit, Ziel und Modus; Eintrag ist im Register und in
  allen C++-/Ruby-Befehlstypenlisten synchron.
- [x] `Actor#forget_skill`, `$game_party.forget_skill` und
  `Scripts::Context#forget_skill` ergänzen den Ruby-/RGSS-Zugang.
- [x] `tools/actor_skill_test.rb` deckt Modell, Interpreter und MapCheck ab;
  `json_test.cpp` prüft F6-Formular und Typregistrierung.

Beispiel:

```json
{"cmd":"actor_skill","actor":"arik","name":"eisspeer","mode":"add"}
{"cmd":"actor_skill","actor":"alle","name":"eisspeer","mode":"remove"}
```

**Verifikation:** `actor_skill_test.rb` (13 Checks), `sync_test.rb`,
`befehle_test.rb`; `test_all.rb` (51 OK, eine EXE-abhängige Suite übersprungen).
`build.ps1 -Test` ist grün; auch dort wurde nur `tex_import_test.rb` mangels
RuneEditor.exe übersprungen. Der Ordner hat keine `.git`-Metadaten, daher
gibt es hier keinen Commit-/CI-Status.
