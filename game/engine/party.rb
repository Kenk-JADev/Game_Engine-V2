#encoding: utf-8
# ============================================================================
#  Party + Inventar (Etappe 17g, RPG-Maker-XP-Datenbank).
#
#  Die Datenbank steht in data/database.json:
#    {
#      "gold": 120,
#      "actors": [ { "id":"arik", "name":"Arik", "class":"Krieger",
#                    "level":1, "hp":120, "mp":20, "atk":14, "def":10,
#                    "spi":6, "agi":9, "exp":0 } ],
#      "items":  [ { "id":"heiltrank", "name":"Heiltrank", "kind":"heal",
#                    "power":40, "price":25, "desc":"Heilt 40 HP." } ]
#    }
#
#  Stufenaufstieg (D4, Runde 22): Erfahrung läuft über `Party.gain_exp`
#  (bzw. `actor.exp = n`); XP-Kurve mit Stufe 1 = 0 Exp und
#  exp(n) = exp(n-1) + ((n-1) ** 2.7 + 0.5).to_i (XP: 2.4 +
#  exp_inflation/100, Default 30). Rune3D speichert GESAMT-Exp, XP den Rest
#  je Stufe - alte Spielstände bleiben damit gültig.
#
#  Party hält den veränderlichen Spielzustand (HP/MP, Gold, Inventar) und
#  wandert als Snapshot in die Spielstände. Die Item-Definitionen bleiben in
#  der Datenbank; das Inventar speichert nur Mengen.
# ============================================================================

class Party
  Actor = Struct.new(:id, :name, :klass, :level, :max_hp, :max_mp, :hp, :mp,
                     :atk, :def, :spi, :agi, :exp, :states, keyword_init: true) do
    # Etappe 24 (XP 313): Zustände hängen wie im Kampf am Helden
    # ([{ id:, turns: }]); außerhalb des Kampfes läuft `turns` nicht ab.
    def initialize(**felder)
      super
      self.states ||= []
    end

    def damaged? = hp < max_hp

    # XP: Game_Actor#state?(id) bzw. #states - für Scripts und Menü.
    def state?(id) = states.any? { |s| s[:id] == id.to_s }

    def state_names = Party.state_names(self)

    # XP-Route für Scripts: `$game_party.actors[0].exp += 50` (RGSS) bzw.
    # `actor.exp = 50` löst denselben Stufenaufstieg aus wie Party.gain_exp.
    def exp=(value)
      Party.set_exp(self, value)
    end

    # Gesamt-Exp, die die nächste Stufe braucht (XP: Game_Actor#next_exp).
    def next_exp
      Party.exp_for_level(level.to_i + 1)
    end

    # Gelernte Fertigkeiten (XP: Game_Actor#skills) - IDs aus database.json.
    def skills
      Party.skills(self)
    end

    def learn_skill(skill_id)
      Party.learn_skill(self, skill_id)
    end

    def forget_skill(skill_id)
      Party.forget_skill(self, skill_id)
    end

    def hp_line
      "#{name}  Lv #{level}  HP #{hp}/#{max_hp}  MP #{mp}/#{max_mp}"
    end
  end

  @actors = []
  @items = {}
  @gold = 0

  class << self
    attr_reader :actors, :items, :gold
  end

  # Höchste Stufe (XP: final_level = 99) und Kurven-Exponent.
  MAX_LEVEL = 99
  EXP_POW = 2.7

  # Gesamt-Exp, die Stufe `level` verlangt (Stufe 1 = 0). Die Kurve wird
  # einmal berechnet; Index = Stufe.
  def self.exp_for_level(level)
    stufe = level.to_i
    return 0 if stufe <= 1
    @exp_kurve ||= begin
      kurve = [0, 0]
      (2..MAX_LEVEL).each { |i| kurve[i] = kurve[i - 1] + (((i - 1) ** EXP_POW) + 0.5).to_i }
      kurve
    end
    @exp_kurve[[stufe, MAX_LEVEL].min] || 0
  end

  # Erfahrung setzen (auch der Weg über `actor.exp = n`). Überschüssige
  # Erfahrung hebt mehrere Stufen auf einmal. Rückgabe: neue Stufe.
  def self.set_exp(actor, value)
    return actor.level if actor.nil?
    gesamtziel = actor[:exp] = [[value.to_i, 0].max, exp_for_level(MAX_LEVEL)].min
    while actor.level.to_i < MAX_LEVEL && gesamtziel >= exp_for_level(actor.level.to_i + 1)
      level_up!(actor)
    end
    actor.level
  end

  # Erfahrung gutschreiben (Kampfsieg). Rückgabe: Anzahl Stufen, die der
  # Held dadurch aufgestiegen ist (0 = keine).
  def self.gain_exp(actor, amount)
    return 0 unless actor.respond_to?(:exp) && actor.respond_to?(:level)
    vorher = actor.level.to_i
    set_exp(actor, actor.exp.to_i + amount.to_i)
    actor.level.to_i - vorher
  end

  # Stufe um eins erhöhen. Ohne Klassentabellen (XP: RPG::Class-Parameter)
  # wächst jeder Wert prozentual. HP/MP bleiben wie in XP stehen (nur das
  # Maximum steigt) - geheilt wird beim Aufstieg nicht.
  def self.level_up!(actor)
    hp_plus = [(actor.max_hp.to_i * 0.08).round, 1].max
    mp_plus = actor.max_mp.to_i.positive? ? [(actor.max_mp.to_i * 0.08).round, 1].max : 0
    actor.level = actor.level.to_i + 1
    actor.max_hp = actor.max_hp.to_i + hp_plus
    actor.max_mp = actor.max_mp.to_i + mp_plus
    %i[atk def spi agi].each do |wert|
      actor[wert] = actor[wert].to_i + [((actor[wert].to_i) * 0.05).round, 1].max
    end
    Rune::Log.info("Party: #{actor.name} erreicht Stufe #{actor.level}")
    actor.level
  end

  # Aus der Datenbank (Hash) neu aufsetzen; HP/MP starten voll.
  def self.setup(db)
    db ||= {}
    # Etappe 20d (XP 129): @katalog haelt alle Datenbank-Helden, @actors nur
    # die Mitglieder. Ohne party_member-Befehl sind das zunaechst alle - wie
    # in XP, wo Game_Party#setup_members alle Helden einfaengt.
    @katalog = (db['actors'] || []).filter_map do |h|
      next unless h.is_a?(Hash)
      id = h['id'].to_s
      next if id.empty?
      max_hp = (h['hp'] || 100).to_i
      max_mp = (h['mp'] || 20).to_i
      Actor.new(id: id, name: (h['name'] || id).to_s, klass: h['class'].to_s,
                level: (h['level'] || 1).to_i,
                max_hp: max_hp, max_mp: max_mp, hp: max_hp, mp: max_mp,
                atk: (h['atk'] || 10).to_i, def: (h['def'] || 8).to_i,
                spi: (h['spi'] || 6).to_i, agi: (h['agi'] || 8).to_i,
                exp: (h['exp'] || 0).to_i)
    end
    @items = {}
    (db['items'] || []).each do |h|
      next unless h.is_a?(Hash)
      id = h['id'].to_s
      next if id.empty?
      @items[id] = (h['start'] || 0).to_i if (h['start'] || 0).to_i.positive?
    end
    # Fertigkeiten je Held (20a): Liste von Skill-IDs aus der Datenbank.
    @skills = {}
    (db['actors'] || []).each do |h|
      next unless h.is_a?(Hash)
      id = h['id'].to_s
      next if id.empty?
      @skills[id] = Array(h['skills']).map(&:to_s).reject(&:empty?).uniq
    end
    @gold = (db['gold'] || 0).to_i
    @steps = 0   # F3: XP Game_Party#steps - zählt die Schritte des Helden
    @actors = @katalog.dup
    Rune::Log.info("Party: #{@actors.size} Helden, #{@items.size} Item-Sorten, " \
                   "#{@gold} Gold")
    @actors
  end

  # F3 (XP Game_Party): Heldenliste und Schrittzähler.
  def self.actors
    @actors ||= []
  end

  # Etappe 20d (XP 129, Game_Party#add_actor/remove_actor): Mitgliederliste
  # aendern. Rueckgabe sagt, was passiert ist:
  #   :ok           - geaendert
  #   :unbekannt    - kein Held mit dieser ID in der Datenbank
  #   :mitglied     - war schon im Team (XP ignoriert das still)
  #   :kein_mitglied- war nicht im Team
  #   :letzter      - das letzte Mitglied darf nicht gehen (sonst steht Kampf,
  #                   Menue und Spielstand ohne Held da; XP erlaubt das zwar,
  #                   Rune3D braucht mindestens einen)
  def self.member_add(id)
    held = (@katalog || []).find { |a| a.id == id.to_s }
    return :unbekannt if held.nil?
    return :mitglied if @actors.include?(held)
    @actors << held
    Rune::Log.info("Party: #{held.name} tritt bei (#{@actors.size} Mitglieder)")
    :ok
  end

  def self.member_remove(id)
    held = (@actors || []).find { |a| a.id == id.to_s }
    return :kein_mitglied if held.nil?
    return :letzter if @actors.size <= 1
    @actors.delete(held)
    Rune::Log.info("Party: #{held.name} verlaesst das Team (#{@actors.size} Mitglieder)")
    :ok
  end

  # Etappe 20e (XP 303): Zugriff auf ALLE Datenbank-Helden, auch wenn sie
  # nicht im Team sind - die Namenseingabe darf jeden umbenennen
  # ($game_actors[id] in XP).
  def self.held(id)
    key = id.to_s.strip
    return nil if key.empty?

    (@katalog || []).find { |a| a.id == key }
  end

  # Umbenennen (Etappe 20e): gilt fuer Mitglied und Katalog gleichzeitig, weil
  # @actors dieselben Actor-Objekte haelt.
  def self.rename(id, name)
    held = held(id)
    return nil if held.nil?

    neuer = name.to_s.strip
    return nil if neuer.empty?

    held.name = neuer
    Rune::Log.info("Party: Held #{held.id} heisst jetzt #{neuer}")
    held
  end

  def self.member?(id)
    (@actors || []).any? { |a| a.id == id.to_s }
  end

  def self.steps
    @steps ||= 0
  end

  def self.increase_steps
    @steps = (@steps || 0) + 1
  end

  def self.leader
    @actors.first
  end

  # Gelernte Fertigkeiten eines Helden (Array von Skill-IDs). Akzeptiert den
  # Actor oder seine ID; unbekannte Helden haben keine Fertigkeiten.
  def self.skills(actor)
    id = actor.is_a?(String) ? actor : (actor.respond_to?(:id) ? actor.id.to_s : nil)
    return [] if id.nil? || id.empty?
    @skills ||= {}
    @skills[id] ||= []
  end

  # Fertigkeit lernen (XP: Game_Actor#learn_skill). Doppelte/alte Einträge
  # werden ignoriert; die Gültigkeit prüft der Aufrufer (Battle/MapCheck).
  def self.learn_skill(actor, skill_id)
    id = actor.is_a?(String) ? actor.to_s : (actor.respond_to?(:id) ? actor.id.to_s : nil)
    skill = skill_id.to_s
    return false if id.nil? || id.empty? || skill.empty?
    liste = skills(actor)
    return false if liste.include?(skill)
    liste << skill
    true
  end

  def self.forget_skill(actor, skill_id)
    id = actor.is_a?(String) ? actor.to_s : (actor.respond_to?(:id) ? actor.id.to_s : nil)
    skill = skill_id.to_s
    return false if id.nil? || id.empty? || skill.empty?
    skills(actor).delete(skill) ? true : false
  end

  # Definition aus database.json auflösen; Events dürfen ID oder Namen nennen.
  def self.skill_def(skill)
    key = skill.to_s.strip
    return nil if key.empty?

    liste = (defined?(::Database) && ::Database.respond_to?(:database) &&
             ::Database.database.is_a?(Hash) ? ::Database.database['skills'] : nil) || []
    liste.find { |h| h.is_a?(Hash) && h['id'].to_s.casecmp?(key) } ||
      liste.find { |h| h.is_a?(Hash) && h['name'].to_s.casecmp?(key) }
  end

  # XP 318: Skill an Party-Helden lernen oder vergessen. Rückgabe ist die
  # Anzahl geänderter Helden; unbekannte Skills/Ziele/Modi haben eigene Codes.
  def self.change_skill(ziel, skill, mode: 'add')
    modus = mode.to_s.strip.downcase
    modus = 'add' if modus.empty?
    return :modus_unbekannt unless %w[add remove].include?(modus)

    definition = skill_def(skill)
    return :unbekannt unless definition

    actors = ziel_actors(ziel)
    return :ziel_unbekannt if actors.empty?

    skill_id = definition['id'].to_s
    actors.count do |actor|
      modus == 'add' ? learn_skill(actor, skill_id) : forget_skill(actor, skill_id)
    end
  end

  def self.find(id)
    @actors.find { |a| a.id == id.to_s }
  end

  # ------------------------------------------------------------- Inventar

  def self.item_def(id)
    list = (Database.database['items'] || [])
    list.find { |h| h.is_a?(Hash) && h['id'].to_s == id.to_s }
  end

  # Gesichtsfarbe eines Helden (20b-Nacharbeit): optional "face" in database.json,
  # sonst nil - die UI leitet dann eine stabile Farbe aus dem Namen ab.
  def self.face_color(actor)
    id = actor.is_a?(String) ? actor : (actor.respond_to?(:id) ? actor.id.to_s : nil)
    return nil if id.nil?

    eintrag = (Database.database['actors'] || []).find { |h| h.is_a?(Hash) && h['id'].to_s == id }
    wert = eintrag && eintrag['face']
    return nil unless wert.is_a?(Integer) && wert >= 0

    wert
  end

  def self.item_name(id)
    (item_def(id) || {})['name'] || id.to_s
  end

  def self.item_count(id)
    @items[id.to_s].to_i
  end

  # Menge ändern (negative Werte nehmen weg, nie unter 0).
  def self.add_item(id, count = 1)
    key = id.to_s
    return 0 if key.empty?
    n = item_count(key) + count.to_i
    n = 0 if n.negative?
    if n.zero?
      @items.delete(key)
    else
      @items[key] = n
    end
    Rune::Log.info("Party: Item '#{key}' #{count.to_i >= 0 ? '+' : ''}#{count.to_i} " \
                   "(jetzt #{n})")
    n
  end

  def self.take_item(id, count = 1)
    add_item(id, -count.to_i.abs)
  end

  # Nur positive Stärke kann einen Heil-/Manatrank verwendbar machen.
  # Fehlerhafte oder negative Werte dürfen keinen versteckten Schaden auslösen.
  def self.item_power(defi)
    value = defi && defi['power']
    return 0 unless value.is_a?(Integer) || value.is_a?(String) ||
                    (value.is_a?(Float) && value.finite?)

    [value.to_i, 0].max
  end

  def self.usable_item?(id)
    defi = item_def(id)
    defi && %w[heal mana].include?(defi['kind'].to_s) && item_power(defi).positive?
  end

  # Eine Actor-Referenz muss wirklich zur aktuellen Party gehören; Structs
  # mit denselben Werten sind nicht automatisch derselbe Held.
  def self.item_target(target)
    return nil if target.nil?
    return @actors.find { |actor| actor.equal?(target) } if target.is_a?(Actor)

    find(target)
  end

  def self.item_applicable?(id, target)
    hero = item_target(target)
    return false unless hero && item_count(id).positive? && usable_item?(id)

    case item_def(id)['kind'].to_s
    when 'heal' then hero.hp < hero.max_hp
    when 'mana' then hero.mp < hero.max_mp
    else false
    end
  end

  # Ohne Ziel bleibt die bisherige automatische Auswahl erhalten. Mit Ziel
  # (Actor oder ID) wird nur bei tatsächlicher Wirkung ein Item verbraucht.
  # Ein Heiltrank darf wie bisher auch einen Helden mit 0 HP wiederbeleben.
  def self.use_item(id, target: nil)
    key = id.to_s
    defi = item_def(key)
    unless defi
      Rune::Log.warn("Party: unbekanntes Item '#{key}'")
      Issues.report('Party', "unbekanntes Item '#{key}'")
      return false
    end
    return false unless item_count(key).positive? && usable_item?(key)

    if target.nil?
      hero = if defi['kind'].to_s == 'heal'
               @actors.find(&:damaged?) || leader
             else
               @actors.find { |actor| actor.mp < actor.max_mp } || leader
             end
      return false unless hero
    else
      hero = item_target(target)
      return false unless item_applicable?(key, hero)
    end

    amount = item_power(defi)
    if defi['kind'].to_s == 'heal'
      hero.hp = [[hero.hp + amount, hero.max_hp].min, 0].max
      Rune::Log.info("Party: #{hero.name} erhält #{item_name(key)} (+#{amount} HP)")
    else
      hero.mp = [[hero.mp + amount, hero.max_mp].min, 0].max
      Rune::Log.info("Party: #{hero.name} erhält #{item_name(key)} (+#{amount} MP)")
    end
    take_item(key, 1)
    true
  end

  # --------------------------------------------------------------- Gold/HP

  def self.add_gold(amount)
    @gold = [@gold + amount.to_i, 0].max
    Rune::Log.info("Party: Gold #{amount.to_i >= 0 ? '+' : ''}#{amount.to_i} (jetzt #{@gold})")
    @gold
  end

  # amount <= 0 heilt vollständig, sonst um amount. Vollständig heilen ist
  # XP 314 (recover_all): es entfernt auch alle Zustände - eine Teilheilung um
  # `amount` lässt sie stehen (Rune3D-Zusatz, XP kennt nur "alles").
  def self.heal_all(amount = 0)
    voll = !amount.to_i.positive?
    @actors.each do |a|
      a.hp = voll ? a.max_hp : [a.hp + amount.to_i, a.max_hp].min
      a.mp = voll ? a.max_mp : [a.mp + amount.to_i, a.max_mp].min
      states(a).clear if voll
    end
    Rune::Log.info("Party: geheilt (#{voll ? 'voll' : "+#{amount.to_i}"})")
    true
  end

  # ------------------------------------------- Werte ändern (Etappe 20d)
  # XP 311/312/315/316 (HP/SP/EXP/Stufe ändern) mit Ziel "alle" oder einem
  # Actor. Rune3D rechnet mit EINER vorzeichenbehafteten Menge statt mit
  # XP-Operation (+/-/setzen): positiv heilt/erhöht, negativ senkt. Dadurch
  # braucht das Formular nur ein Zahlenfeld.
  #
  # Ziel-Schreibweise: nil, "" oder "alle" = ganze Party; sonst Actor-ID oder
  # Name (Gross-/Kleinschreibung egal).
  def self.ziel_actors(ziel)
    name = ziel.to_s.strip
    return @actors if name.empty? || name.casecmp?('alle')

    treffer = @actors.find { |a| a.id.to_s.casecmp?(name) } ||
              @actors.find { |a| a.name.to_s.casecmp?(name) }
    treffer ? [treffer] : []
  end

  def self.change_hp(ziel, amount)
    actors = ziel_actors(ziel)
    return 0 if actors.empty?

    delta = amount.to_i
    actors.each do |a|
      alt = a.hp
      a.hp = [[alt + delta, a.max_hp].min, 0].max
      Rune::Log.info("Party: #{a.name} HP #{alt} -> #{a.hp}")
    end
    actors.size
  end

  # SP heissen in Rune3D MP (XP-Feld "mp" in database.json) - der Befehl folgt
  # der XP-Bezeichnung, die Daten bleiben beim gewohnten Namen.
  def self.change_mp(ziel, amount)
    actors = ziel_actors(ziel)
    return 0 if actors.empty?

    delta = amount.to_i
    actors.each do |a|
      alt = a.mp
      a.mp = [[alt + delta, a.max_mp].min, 0].max
      Rune::Log.info("Party: #{a.name} MP #{alt} -> #{a.mp}")
    end
    actors.size
  end

  # EXP über den normalen Weg (set_exp): Stufenaufstieg inklusive Wachstum,
  # negative Mengen senken die Erfahrung ohne Stufenabbau (XP verliert beim
  # Senken ebenfalls keine Stufe).
  def self.change_exp(ziel, amount)
    actors = ziel_actors(ziel)
    actors.each { |a| set_exp(a, a.exp.to_i + amount.to_i) }
    actors.size
  end

  # Stufe setzen: Aufstieg über level_up! (Wachstum wie im Kampf), Abstieg
  # senkt nur die Stufe - max_hp/atk usw. bleiben stehen, weil Rune3D keine
  # Klassentabellen hat, aus denen man sie neu berechnen könnte (LOGIK AD.7).
  def self.change_level(ziel, amount)
    actors = ziel_actors(ziel)
    actors.each do |a|
      ziel_stufe = [[a.level.to_i + amount.to_i, MAX_LEVEL].min, 1].max
      if ziel_stufe > a.level
        (ziel_stufe - a.level).times { level_up!(a) }
      elsif ziel_stufe < a.level
        Rune::Log.info("Party: #{a.name} Stufe #{a.level} -> #{ziel_stufe}")
        a.level = ziel_stufe
        a[:exp] = [exp_for_level(ziel_stufe), a.exp.to_i].min
      end
    end
    actors.size
  end

  # ------------------------------------------- Zustände (Etappe 24, XP 313)
  # XP hält Zustände am Game_Battler und damit auch am Game_Actor außerhalb
  # des Kampfes. Rune3D speichert dieselbe Struktur wie der Kampf
  # ([{ id:, turns: }], höchstens 8) direkt am Party::Actor; die Definitionen
  # stehen in database.json unter "states" (Felder id, name, kind, power,
  # turns - siehe 20a/Kampf).
  MAX_STATES = 8

  # Ziel-Schreibweise wie bei ziel_actors: nil/""/"alle" = ganze Party.
  # Der Zustand darf als ID oder als Name angegeben werden (Groß-/
  # Kleinschreibung egal); Rückgabe ist die Definition oder nil.
  def self.state_def(zustand)
    key = zustand.to_s.strip
    return nil if key.empty?

    liste = (defined?(::Database) && ::Database.respond_to?(:database) &&
             ::Database.database.is_a?(Hash) ? ::Database.database['states'] : nil) || []
    liste.find { |h| h.is_a?(Hash) && h['id'].to_s.casecmp?(key) } ||
      liste.find { |h| h.is_a?(Hash) && h['name'].to_s.casecmp?(key) }
  end

  # Anzeige "Gift(2) Schutz" - die Dauer nur, wenn sie gesetzt ist.
  def self.state_names(actor)
    (actor&.states || []).filter_map do |s|
      defi = state_def(s[:id])
      next "#{s[:id]}(#{s[:turns]})" if defi.nil? && s[:turns]

      name = defi ? (defi['name'] || s[:id]).to_s : s[:id].to_s
      s[:turns] ? "#{name}(#{s[:turns]})" : name
    end
  end

  def self.states(actor)
    actor&.states ||= []
  end

  # Zustand hinzufügen oder entfernen (XP 313). Rückgabe:
  #   Anzahl geänderter Helden   - :ok
  #   :unbekannt                 - kein Zustand mit dieser ID/diesem Namen
  #   :voll                      - alle Ziele hatten schon 8 Zustände
  # Bei "add" wird ein vorhandener Zustand aufgefrischt (Dauer wie im Kampf
  # verlängert) - XP macht dasselbe über add_state.
  def self.change_state(ziel, zustand, mode: 'add', turns: nil)
    defi = state_def(zustand)
    return :unbekannt if defi.nil?

    id = defi['id'].to_s
    dauer = turns.nil? ? nil : [turns.to_i, 1].max
    entfernen = mode.to_s.strip.casecmp?('remove')
    actors = ziel_actors(ziel)
    return 0 if actors.empty?

    geaendert = 0
    voll = false
    actors.each do |a|
      liste = states(a)
      if entfernen
        vorher = liste.size
        liste.reject! { |s| s[:id] == id }
        geaendert += 1 if liste.size != vorher
        next
      end
      vorhanden = liste.find { |s| s[:id] == id }
      if vorhanden
        neu = dauer || (defi['turns'] || 0).to_i
        vorhanden[:turns] = [vorhanden[:turns].to_i, neu].max if neu.positive?
        geaendert += 1
      elsif liste.size >= MAX_STATES
        voll = true
        Rune::Log.warn("Party: #{a.name} hat schon #{MAX_STATES} Zustände")
      else
        liste << { id: id, turns: dauer }
        geaendert += 1
        Rune::Log.info("Party: #{a.name} erhält Zustand #{defi['name'] || id}")
      end
    end
    return :voll if geaendert.zero? && voll

    geaendert
  end

  # XP 314 (recover_all) entfernt alle Zustände - heal_all nutzt das beim
  # vollständigen Heilen.
  def self.clear_states(ziel = 'alle')
    actors = ziel_actors(ziel)
    actors.each { |a| states(a).clear }
    actors.size
  end

  # ------------------------------------ Parameter ändern (Etappe 24, XP 317)
  # XP 317 ändert max_hp/max_mp/atk (XP kennt nur drei Parameter); Rune3D
  # bietet die volle Liste, weil Datenbank, Kampf und Menü alle sechs Werte
  # führen. Vorzeichenbehaftet wie bei 311/312.
  PARAMETER = %w[max_hp max_mp atk def spi agi].freeze

  def self.change_param(ziel, parameter, amount)
    wert = parameter.to_s.strip.downcase
    return :unbekannt unless PARAMETER.include?(wert)

    actors = ziel_actors(ziel)
    return 0 if actors.empty?

    delta = amount.to_i
    untergrenze = wert.start_with?('max_') ? 1 : 0
    actors.each do |a|
      alt = a[wert].to_i
      neu = [[alt + delta, untergrenze].max, 9_999_999].min
      a[wert] = neu
      # Sinkt das Maximum unter die aktuellen HP/MP, wird gekappt (XP kappt
      # beim Ändern von maxhp ebenso über den Setter).
      a.hp = [a.hp, a.max_hp].min if wert == 'max_hp'
      a.mp = [a.mp, a.max_mp].min if wert == 'max_mp'
      Rune::Log.info("Party: #{a.name} #{wert} #{alt} -> #{neu}")
    end
    actors.size
  end

  # ------------------------------------------------------- Speichern/Laden

  def self.snapshot
    {
      'gold' => @gold,
      'steps' => steps,
      'items' => @items.dup,
      'skills' => (@skills || {}).transform_values(&:dup),
      # Seit dem Level-Up (D4) wachsen auch max_hp/max_mp und die
      # Angriffswerte - sie gehören damit in den Spielstand, sonst steht ein
      # geladener Held wieder auf den Datenbank-Startwerten.
      'actors' => @actors.map do |a|
        { 'id' => a.id, 'name' => a.name,
          'level' => a.level, 'hp' => a.hp, 'mp' => a.mp, 'exp' => a.exp,
          'max_hp' => a.max_hp, 'max_mp' => a.max_mp,
          'atk' => a.atk, 'def' => a.def, 'spi' => a.spi, 'agi' => a.agi,
          # Etappe 24 (XP 313): Zustände gehören in den Spielstand; `turns`
          # bleibt erhalten und läuft erst im Kampf ab.
          'states' => states(a).map { |s| { 'id' => s[:id], 'turns' => s[:turns] } } }
      end
    }
  end

  def self.restore!(snap)
    snap ||= {}
    @gold = (snap['gold'] || 0).to_i
    @steps = (snap['steps'] || 0).to_i   # alte Spielstände ohne 'steps' = 0
    @items = (snap['items'] || {}).map { |k, v| [k.to_s, v.to_i] }.reject { |_, v| v <= 0 }.to_h
    # Fertigkeiten (20a): alte Spielstände ohne 'skills' behalten die
    # Datenbank-Liste aus Party.setup.
    if snap['skills'].is_a?(Hash)
      @skills = snap['skills'].each_with_object({}) do |(id, liste), hash|
        hash[id.to_s] = Array(liste).map(&:to_s).reject(&:empty?).uniq
      end
    end
    # Mitgliederliste zuerst (XP 129): der Spielstand fuehrt nur die Helden im
    # Team; @actors ist danach genau diese Liste (Reihenfolge wie gespeichert).
    # Alte Spielstaende ohne 'actors' und leere Listen lassen das Team stehen.
    geladen = Array(snap['actors']).filter_map do |h|
      next unless h.is_a?(Hash)
      (@katalog || []).find { |a| a.id == h['id'].to_s }
    end
    @actors = geladen.uniq unless geladen.empty?
    (snap['actors'] || []).each do |h|
      a = find(h['id'])
      next unless a
      # Etappe 20e: umbenannte Helden (XP 303) behalten ihren Namen.
      a.name = h['name'].to_s.strip unless h['name'].to_s.strip.empty?
      a.level = (h['level'] || a.level).to_i
      # Bewusst am Setter vorbei: ein alter Spielstand darf beim Laden keine
      # Stufen aufholen (die Werte sind dort schon eingerechnet).
      a[:exp] = (h['exp'] || a.exp).to_i
      # Gewachsene Werte aus dem Spielstand übernehmen; alte Saves ohne diese
      # Felder behalten die Datenbankwerte.
      a.max_hp = [[(h['max_hp'] || a.max_hp).to_i, 1].max, 9_999_999].min
      a.max_mp = [[(h['max_mp'] || a.max_mp).to_i, 0].max, 9_999_999].min
      %i[atk def spi agi].each do |wert|
        a[wert] = (h[wert.to_s] || a[wert]).to_i
      end
      a.hp = [[(h['hp'] || a.hp).to_i, a.max_hp].min, 0].max
      a.mp = [[(h['mp'] || a.mp).to_i, a.max_mp].min, 0].max
      # Zustände (Etappe 24): alte Spielstände ohne 'states' setzen den Helden
      # auf die Datenbank-Startwerte zurück - dort stehen keine Zustände.
      a.states = Array(h['states']).filter_map do |eintrag|
        next unless eintrag.is_a?(Hash)
        id = eintrag['id'].to_s
        next if id.empty?
        { id: id, turns: eintrag['turns'].nil? ? nil : eintrag['turns'].to_i }
      end.first(MAX_STATES)
    end
    Rune::Log.info("Party wiederhergestellt: #{@gold} Gold, #{@items.size} Item-Sorten")
    true
  end

  # Zeilen für das Menü-Panel.
  def self.status_lines
    lines = @actors.map(&:hp_line)
    lines << "Gold: #{@gold}"
    if @items.empty?
      lines << 'Inventar: leer'
    else
      @items.sort_by { |id, _| item_name(id) }.each do |id, n|
        lines << "#{item_name(id)} x#{n}"
      end
    end
    lines
  end
end
