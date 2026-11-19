#encoding: utf-8
# ============================================================================
#  Spielzustand: Schalter (bool), Variablen (Zahlen) und Self-Switches
#  (Etappe 19d, RPG Maker XP: Schalter A-D pro Event), initiiert aus
#  game/data/system.json. Lebt nur im Speicher; gespeichert werden die Daten
#  erst mit einer Save-Funktion (kommt später mit dem Editor).
# ============================================================================

class GameState
  @switches = {}
  @variables = {}
  @self_switches = {}   # Etappe 19d: "karte|event|buchstabe" => bool

  # Setzt Anfangs-/Standardwerte aus der Systemdatei zurück.
  def self.reset!(initial)
    @switches = {}
    @variables = {}
    @self_switches = {}
    (initial['switches'] || {}).each { |k, v| @switches[k.to_s] = !!v }
    (initial['variables'] || {}).each { |k, v| @variables[k.to_s] = v.to_f }
    Rune::Log.info("GameState: #{@switches.size} Schalter, #{@variables.size} Variablen")
  end

  def self.switch(name)
    !!@switches[name.to_s]
  end

  def self.set_switch(name, value)
    @switches[name.to_s] = !!value
  end

  def self.toggle_switch(name)
    @switches[name.to_s] = !switch(name)
  end

  # --- Self-Switches (Etappe 19d, RPG Maker XP) -----------------------------
  # Schalter A-D, die nur für EIN Event gelten. Der Schlüssel kommt vom Event
  # (Event#self_switch_key = "karte|name"), der Buchstabe wird normalisiert.

  def self.self_switch(key, letter)
    !!@self_switches["#{key}|#{letter.to_s.upcase}"]
  end

  def self.set_self_switch(key, letter, value)
    l = letter.to_s.upcase
    unless %w[A B C D].include?(l)
      Rune::Log.warn("GameState: ungültiger Self-Switch-Buchstabe '#{letter}' (A-D)")
      return
    end
    @self_switches["#{key}|#{l}"] = !!value
  end

  def self.variable(name)
    @variables.fetch(name.to_s, 0.0)
  end

  def self.set_variable(name, value)
    @variables[name.to_s] = value.to_f
  end

  def self.add_variable(name, amount)
    @variables[name.to_s] = variable(name) + amount.to_f
  end

  # Kurzbeschreibung für Debug-Logs.
  def self.inspect_short
    "sw=#{@switches.select { |_, v| v }.keys.join(',')} var=#{@variables.inspect}"
  end

  # ---------------------------------------------------- Speichern / Laden

  # Zustand als JSON-fähigen Hash (Schalter + Variablen).
  def self.snapshot
    { 'switches' => @switches.dup, 'variables' => @variables.dup,
      'self_switches' => @self_switches.dup }
  end

  # Zustand aus einem (evtl. aus JSON stammenden) Hash wiederherstellen.
  def self.restore!(snap)
    @switches = (snap['switches'] || {}).map { |k, v| [k.to_s, !!v] }.to_h
    @variables = (snap['variables'] || {}).map { |k, v| [k.to_s, v.to_f] }.to_h
    @self_switches = (snap['self_switches'] || {}).map { |k, v| [k.to_s, !!v] }.to_h
  end
end