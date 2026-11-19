#encoding: utf-8
# ============================================================================
#  Spielzustand: Schalter (bool) und Variablen (Zahlen), initiiert aus
#  game/data/system.json. Lebt nur im Speicher; gespeichert werden die Daten
#  erst mit einer Save-Funktion (kommt spaeter mit dem Editor).
# ============================================================================

class GameState
  @switches = {}
  @variables = {}

  # Setzt Anfangs-/Standardwerte aus der Systemdatei zurueck.
  def self.reset!(initial)
    @switches = {}
    @variables = {}
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

  def self.variable(name)
    @variables.fetch(name.to_s, 0.0)
  end

  def self.set_variable(name, value)
    @variables[name.to_s] = value.to_f
  end

  def self.add_variable(name, amount)
    @variables[name.to_s] = variable(name) + amount.to_f
  end

  # Kurzbeschreibung fuer Debug-Logs.
  def self.inspect_short
    "sw=#{@switches.select { |_, v| v }.keys.join(',')} var=#{@variables.inspect}"
  end

  # ---------------------------------------------------- Speichern / Laden

  # Zustand als JSON-faehigen Hash (Schalter + Variablen).
  def self.snapshot
    { 'switches' => @switches.dup, 'variables' => @variables.dup }
  end

  # Zustand aus einem (evtl. aus JSON stammenden) Hash wiederherstellen.
  def self.restore!(snap)
    @switches = (snap['switches'] || {}).map { |k, v| [k.to_s, !!v] }.to_h
    @variables = (snap['variables'] || {}).map { |k, v| [k.to_s, v.to_f] }.to_h
  end
end