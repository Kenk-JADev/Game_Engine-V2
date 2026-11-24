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
                     :atk, :def, :spi, :agi, :exp, keyword_init: true) do
    def damaged? = hp < max_hp

    # XP-Route für Scripts: `$game_party.actors[0].exp += 50` (RGSS) bzw.
    # `actor.exp = 50` löst denselben Stufenaufstieg aus wie Party.gain_exp.
    def exp=(value)
      Party.set_exp(self, value)
    end

    # Gesamt-Exp, die die nächste Stufe braucht (XP: Game_Actor#next_exp).
    def next_exp
      Party.exp_for_level(level.to_i + 1)
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
    @actors = (db['actors'] || []).filter_map do |h|
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
    @gold = (db['gold'] || 0).to_i
    @steps = 0   # F3: XP Game_Party#steps - zählt die Schritte des Helden
    Rune::Log.info("Party: #{@actors.size} Helden, #{@items.size} Item-Sorten, " \
                   "#{@gold} Gold")
    @actors
  end

  # F3 (XP Game_Party): Heldenliste und Schrittzähler.
  def self.actors
    @actors ||= []
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

  def self.find(id)
    @actors.find { |a| a.id == id.to_s }
  end

  # ------------------------------------------------------------- Inventar

  def self.item_def(id)
    list = (Database.database['items'] || [])
    list.find { |h| h.is_a?(Hash) && h['id'].to_s == id.to_s }
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

  # amount <= 0 heilt vollständig, sonst um amount.
  def self.heal_all(amount = 0)
    @actors.each do |a|
      a.hp = amount.to_i.positive? ? [a.hp + amount.to_i, a.max_hp].min : a.max_hp
      a.mp = amount.to_i.positive? ? [a.mp + amount.to_i, a.max_mp].min : a.max_mp
    end
    Rune::Log.info("Party: geheilt (#{amount.to_i.positive? ? "+#{amount.to_i}" : 'voll'})")
    true
  end

  # ------------------------------------------------------- Speichern/Laden

  def self.snapshot
    {
      'gold' => @gold,
      'steps' => steps,
      'items' => @items.dup,
      # Seit dem Level-Up (D4) wachsen auch max_hp/max_mp und die
      # Angriffswerte - sie gehören damit in den Spielstand, sonst steht ein
      # geladener Held wieder auf den Datenbank-Startwerten.
      'actors' => @actors.map do |a|
        { 'id' => a.id, 'level' => a.level, 'hp' => a.hp, 'mp' => a.mp, 'exp' => a.exp,
          'max_hp' => a.max_hp, 'max_mp' => a.max_mp,
          'atk' => a.atk, 'def' => a.def, 'spi' => a.spi, 'agi' => a.agi }
      end
    }
  end

  def self.restore!(snap)
    snap ||= {}
    @gold = (snap['gold'] || 0).to_i
    @steps = (snap['steps'] || 0).to_i   # alte Spielstände ohne 'steps' = 0
    @items = (snap['items'] || {}).map { |k, v| [k.to_s, v.to_i] }.reject { |_, v| v <= 0 }.to_h
    (snap['actors'] || []).each do |h|
      a = find(h['id'])
      next unless a
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
