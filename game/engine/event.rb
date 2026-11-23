#encoding: utf-8
# ============================================================================
#  Event: ein interaktiver Punkt in der Welt, definiert in game/data/maps/*.json.
#
#  Seit Etappe 18g kann ein Event mehrere SEITEN haben (wie RPG Maker XP):
#    "pages": [
#      { "conditions": { "switch":  {"name":"auftrag", "value":true},
#                        "variable": {"name":"kristalle", "op":">=", "value":3},
#                        "self_switch": {"letter":"A", "value":true} },        # 19d
#        "trigger": "action", "commands": [...] },
#      { "conditions": {}, "trigger": "action", "commands": [...] }
#    ]
#
#  Es gewinnt immer die Seite mit dem HÖCHSTEN Index, deren Bedingungen
#  erfüllt sind (Seite 0 = Standard, leere Bedingungen = immer erfüllt).
#  Wechselt die aktive Seite, wird das Event als "noch nicht ausgelöst"
#  behandelt - touch/auto-Events feuern auf der neuen Seite erneut.
#
#  Auslöser (trigger, pro Seite):
#    "action"   - Spieler muss in Reichweite sein und E drucken
#    "touch"    - feuert einmalig, sobald der Spieler in Reichweite kommt
#    "auto"     - feuert einmalig beim Kartenstart
#    "parallel" - startet immer wieder, wenn der Interpreter frei ist (18f)
#  Kommandos laufen durch den Interpreter. Die Auslöse-Logik steht in der
#  Spiellogik (main.rb), nicht hier - das Event bleibt reine Daten.
#  Alte Karten ohne "pages" laufen weiter (eine implizite Seite ohne
#  Bedingungen aus den Top-Level-Feldern trigger/commands).
# ============================================================================

EventPage = Struct.new(:conditions, :trigger, :commands)

class Event
  attr_reader :id, :name, :radius, :pages
  attr_accessor :fired, :x, :z

  def initialize(h)
    @id       = h['id'] || 0
    @name     = h['name'] || 'Event'
    @x        = h['x'].to_f
    @z        = h['z'].to_f
    @radius   = (h['radius'] || 2.0).to_f
    # y ist ein Versatz zur Geländehöhe, nicht eine absolute Welthöhe.
    @height_offset = h['y'].is_a?(Numeric) && h['y'].to_f.finite? ? h['y'].to_f : 0.0
    @visual   = h['visual']
    @rotation = h['rot'].is_a?(Numeric) && h['rot'].to_f.finite? ? h['rot'].to_f : 0.0
    @pages    = Event.build_pages(h)
    @fired    = false
    @page_idx = 0
  end

  # Seitenliste aus den Kartendaten bauen (altes Format bleibt kompatibel).
  def self.build_pages(h)
    raw = h['pages']
    if raw.is_a?(Array) && !raw.empty?
      raw.map do |p|
        next EventPage.new({}, 'action', []) unless p.is_a?(Hash)
        trig = (p['trigger'] || 'action').to_s
        trig = 'action' unless %w[action touch auto parallel].include?(trig)
        EventPage.new(p['conditions'].is_a?(Hash) ? p['conditions'] : {},
                      trig,
                      p['commands'].is_a?(Array) ? p['commands'] : [])
      end
    else
      # Altes Format: trigger/commands stehen direkt am Event.
      [EventPage.new({}, (h['trigger'] || 'action').to_s, h['commands'] || [])]
    end
  end

  # Sind die Bedingungen einer Seite erfüllt? (leer = immer ja)
  # ev: das Event selbst - nur für Self-Switch-Bedingungen nötig (19d).
  def self.conditions_met?(c, ev = nil)
    return true unless c.is_a?(Hash)

    if (sw = c['switch']) && sw.is_a?(Hash) && sw['name']
      return false if GameState.switch(sw['name']) != !!sw['value']
    end
    if (va = c['variable']) && va.is_a?(Hash) && va['name']
      a = GameState.variable(va['name'])
      b = (va['value'] || 0).to_f
      op = (va['op'] || '==').to_s
      ok = case op
           when '==' then a == b
           when '!=' then a != b
           when '>'  then a > b
           when '>=' then a >= b
           when '<'  then a < b
           when '<=' then a <= b
           else true   # unbekannter Operator blockiert die Seite nicht
           end
      return false unless ok
    end
    # Self-Switch-Bedingung (Etappe 19d): ohne Event-Kontext nie erfüllt.
    if (ss = c['self_switch']) && ss.is_a?(Hash) && ss['letter']
      return false unless ev
      return false if GameState.self_switch(ev.self_switch_key, ss['letter']) != !!ss['value']
    end
    true
  end

  # Aktive Seite neu bestimmen (höchster Index mit erfüllten Bedingungen).
  # Seitenwechsel setzt 'fired' zurück, damit touch/auto erneut feuern.
  # F2 (XP-Gleichheit): erfüllt KEINE Seite ihre Bedingungen, ist das Event
  # inaktiv (page_index nil) - es löst nichts aus und hat keine Kommandos.
  def refresh_page!
    idx = @pages.rindex { |p| Event.conditions_met?(p.conditions, self) }
    return if idx == @page_idx

    @fired = false
    @page_idx = idx
  end

  def page_index
    @page_idx
  end

  # true, solange mindestens eine Seite ihre Bedingungen erfüllt.
  def active?
    !@page_idx.nil?
  end

  # Aktive Seite oder nil (Event inaktiv).
  def active_page
    @page_idx.nil? ? nil : @pages[@page_idx]
  end

  def trigger
    active_page&.trigger
  end

  def commands
    active_page&.commands || []
  end

  # Liegt der Spieler in Reichweite (nur XZ, Höhe egal)?
  def near?(px, pz)
    (px - @x) * (px - @x) + (pz - @z) * (pz - @z) <= @radius * @radius
  end

  # F6 (XP-Gleichheit): die XP-Koordinate y ist die Kartenebene (= Welt-z
  # in Rune3D), nicht die Höhe - so lesen/schreiben XP-Skripte Event#y.
  def y
    @z
  end

  def y=(value)
    @z = value.to_f
  end

  # Ein im Platzier-Editor erzeugtes Schild ist ein normales Event mit
  # optionaler Darstellung. Die beiden Kästen sind nur sichtbar, nicht
  # anklickbare Hindernisse; Picking benutzt das zugehörige Event.
  def add_visual(scene, index)
    return [] unless @visual == 'schild'

    y = Game.gy(@x, @z) + @height_offset
    prefix = "event_visual_#{index}"
    [scene.add(Node.new(name: "#{prefix}_pfosten", position: [@x, y + 0.65, @z],
                        rotation: [0.0, @rotation, 0.0], scale: [0.18, 1.3, 0.18],
                        color: 0x76502F)),
     scene.add(Node.new(name: "#{prefix}_tafel", position: [@x, y + 1.55, @z],
                        rotation: [0.0, @rotation, 0.0], scale: [1.35, 0.65, 0.15],
                        color: 0xD5AB69))]
  end

  # --- Picking (Maus): Events werden als Kugel getroffen -------------------
  def box_center
    [@x, Game.gy(@x, @z) + @height_offset + 1.0, @z]
  end

  def pick_radius
    r = @radius * 0.55
    r < 0.9 ? 0.9 : r
  end

  # Startet die Kommandos der aktiven Seite im (freien) Interpreter.
  def manual_fire(interp)
    # F2: ein inaktives Event (keine erfüllte Seite) startet nichts.
    return false unless active?

    interp.start(commands, self)
  end

  # Schlüssel für Self-Switches (Etappe 19d): Karte + Event-Name. XP nutzt
  # Karte + Event-ID; bei uns sind IDs optional (Default 0), Namen dagegen
  # tragen auch die Savegames ('fired'-Liste, NPC-Snapshots) - also passen
  # sie besser ins vorhandene Datenmodell.
  def self_switch_key
    map = (defined?(Game) && Game.respond_to?(:map_id)) ? Game.map_id : 'welt'
    "#{map}|#{@name}"
  end
end
