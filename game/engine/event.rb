#encoding: utf-8
# ============================================================================
#  Event: ein interaktiver Punkt in der Welt, definiert in game/data/maps/*.json.
#
#  Seit Etappe 18g kann ein Event mehrere SEITEN haben (wie RPG Maker XP):
#    "pages": [
#      { "conditions": { "switch":  {"name":"auftrag", "value":true},
#                        "variable": {"name":"kristalle", "op":">=", "value":3} },
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
  def self.conditions_met?(c)
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
    true
  end

  # Aktive Seite neu bestimmen (höchster Index mit erfüllten Bedingungen).
  # Seitenwechsel setzt 'fired' zurück, damit touch/auto erneut feuern.
  def refresh_page!
    idx = @pages.rindex { |p| Event.conditions_met?(p.conditions) } || 0
    @fired = false if idx != @page_idx
    @page_idx = idx
  end

  def page_index
    @page_idx
  end

  def active_page
    @pages[@page_idx] || @pages.last
  end

  def trigger
    active_page.trigger
  end

  def commands
    active_page.commands
  end

  # Liegt der Spieler in Reichweite (nur XZ, Höhe egal)?
  def near?(px, pz)
    (px - @x) * (px - @x) + (pz - @z) * (pz - @z) <= @radius * @radius
  end

  # --- Picking (Maus): Events werden als Kugel getroffen -------------------
  def box_center
    [@x, Game.gy(@x, @z) + 1.0, @z]
  end

  def pick_radius
    r = @radius * 0.55
    r < 0.9 ? 0.9 : r
  end

  # Startet die Kommandos der aktiven Seite im (freien) Interpreter.
  def manual_fire(interp)
    interp.start(commands, self)
  end
end
