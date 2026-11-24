#encoding: utf-8
# ============================================================================
#  Etappe 20d: $game_temp-Aufträge ausführen (LOGIK E.3).
#
#  XP lässt Scripts Szenenwechsel über $game_temp anfordern - der Map-Scene-
#  Update liest die Flags und startet Kampf/Menü/Speichern/Kartenwechsel.
#  Bis Runde 28 waren die Felder in Rune3D stumm gespeichert: Scripts setzten
#  `$game_temp.battle_calling = true` und nichts passierte.
#
#  Die Ausführung selbst hängt an der App (Kampf, Menü, Kartenwechsel), darum
#  bekommt `TempActions.tick` die Handler als Lambdas - so bleibt die Logik
#  kopflos testbar (tools/temp_flags_test.rb) und `Game.update` nur die
#  Verdrahtung.
# ============================================================================

module TempActions
  # Führt genau EINEN offenen Auftrag aus (XP prüft pro Frame der Reihe nach
  # Kampf -> Menü/Speichern -> Transfer und kehrt danach zurück).
  # Handler-Schlüssel:
  #   :battle   -> (troop_id, can_escape, can_lose)
  #   :menu     -> (speichern_gewuenscht)
  #   :transfer -> (map_id, x, z, yaw) - x/z/yaw sind nil, wenn XP sie nicht setzt
  #   :title    -> ()
  # Rückgabe: true, wenn etwas ausgeführt wurde (der Frame endet dann).
  def self.tick(handler = {})
    return false unless defined?($game_temp) && $game_temp

    t = $game_temp
    if t.battle_calling
      # Erst löschen: Battle.start verweigert einen zweiten Kampf, ein Flag,
      # das stehen bleibt, würde sonst jeden Frame erneut anfragen.
      t.battle_calling = false
      Rune::Log.info("$game_temp: Kampf angefordert (Truppe '#{t.battle_troop_id}')")
      ruf(handler[:battle], t.battle_troop_id, !!t.battle_can_escape, !!t.battle_can_lose)
      return true
    end

    if t.menu_calling || t.save_calling
      speichern = !!t.save_calling
      t.menu_calling = false
      t.save_calling = false
      Rune::Log.info("$game_temp: #{speichern ? 'Speichern' : 'Menü'} angefordert")
      ruf(handler[:menu], speichern)
      return true
    end

    if t.player_transferring
      t.player_transferring = false
      karte = karte_fuer(t.player_new_map_id)
      if karte.nil?
        Rune::Log.warn("$game_temp: Zielkarte '#{t.player_new_map_id}' unbekannt")
        Issues.report('TempActions', "$game_temp.player_new_map_id '#{t.player_new_map_id}' unbekannt")
      else
        x, z = startposition(t)
        yaw = yaw_fuer(t.player_new_direction)
        Rune::Log.info("$game_temp: Kartenwechsel nach '#{karte}'" \
                       "#{x && z ? " bei (#{x}, #{z})" : ''}#{yaw ? " yaw #{yaw.round(3)}" : ''}")
        ruf(handler[:transfer], karte, x, z, yaw)
      end
      return true
    end

    if t.to_title
      t.to_title = false
      Rune::Log.info('$game_temp: Titel angefordert')
      ruf(handler[:title])
      return true
    end

    false
  end

  # XP-Karten sind numerisch, Rune3D-Karten heißen nach Datei ("dorf"). Beides
  # geht: der Name direkt, oder die 1-basierte Position in der sortierten
  # Kartenliste (dieselbe Reihenfolge, die Database.map_ids liefert).
  def self.karte_fuer(ziel)
    ids = karten_ids
    name = ziel.to_s
    return name if ids.include?(name)
    return nil unless ziel.is_a?(Integer) || name.match?(/\A\d+\z/)

    index = name.to_i
    return nil if index < 1 || index > ids.size

    ids[index - 1]
  end

  # XP-Startposition: `player_new_x`/`player_new_y` sind Kacheln, Rune3D rechnet
  # in Weltkoordinaten. Beide Werte zusammen ungleich 0 gelten als Position;
  # (0, 0) bedeutet "bisherige Stelle behalten", weil XP-Events die Felder
  # nicht immer setzen und 0 dort auch eine gültige Kachel wäre.
  def self.startposition(t)
    x = t.player_new_x.to_f
    z = t.player_new_y.to_f
    return [nil, nil] if x.zero? && z.zero?

    [x, z]
  end

  # XP-Richtungen 2/4/6/8 (unten/links/rechts/oben) auf die Kamera-Yaw legen.
  # Konvention: 8 (oben) = π = Engine-Standard aus dem Teleport-Befehl, 2 = 0,
  # 6 = +π/2 (rechts drehen), 4 = −π/2. 0/andere = Ausrichtung behalten.
  RICHTUNG_YAW = { 2 => 0.0, 6 => Math::PI / 2, 4 => -Math::PI / 2, 8 => Math::PI }.freeze

  def self.yaw_fuer(richtung)
    RICHTUNG_YAW[richtung.to_i]
  end

  def self.karten_ids
    return [] unless defined?(Database) && Database.respond_to?(:map_ids)

    Database.map_ids
  rescue StandardError => e
    Rune::Log.warn("TempActions: Kartenliste nicht lesbar (#{e.message})")
    []
  end

  # Fehlender Handler ist kein Fehler (z. B. im Test): Warnung statt Absturz.
  def self.ruf(handler, *args)
    unless handler.respond_to?(:call)
      Rune::Log.warn('$game_temp: Auftrag ohne Handler ignoriert')
      return nil
    end
    handler.call(*args)
  end
end
