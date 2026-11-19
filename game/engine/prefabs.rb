#encoding: utf-8
# ============================================================================
#  Prefabs: die Objekttypen, die im Editor gesetzt werden koennen.
#
#  Eine Quelle fuer Spiel und RuneEditor: die Definitionen liegen in
#  game/data/prefabs.json (Export: ruby tools/export_prefabs.rb).
#
#  Ein Prefab ist eine Liste von Kaesten ("parts"), alle Koordinaten relativ
#  zur Standflaeche des Objekts (y = 0 ist der Boden). `s` = Groesse,
#  `d` = Versatz, `c` = Farbe, `solid` = Hindernis fuer den Spieler.
#
#  Im Karten-JSON steht nur:  {"type":"haus","x":-10.0,"z":-8.0,"rot":0.0}
# ============================================================================
require 'json'

module Prefabs
  PREFAB_FILE = File.expand_path('../data/prefabs.json', __dir__)
  _raw = JSON.parse(File.read(PREFAB_FILE, encoding: 'utf-8'))
  LIST = _raw['prefabs'].map do |p|
    parts = p['parts'].map do |q|
      part = { d: q['d'].map(&:to_f), s: q['s'].map(&:to_f), c: q['c'] }
      part[:tex] = q['tex'] if q['tex']
      part[:tile] = q['tile'].to_f if q['tile']
      part[:solid] = true if q['solid']
      part
    end
    prefab = { id: p['id'], name: p['name'], parts: parts }
    prefab[:anim] = p['anim'] if p['anim']
    prefab
  end.freeze



  def self.by_id(id)
    LIST.find { |p| p[:id] == id.to_s }
  end

  def self.ids
    LIST.map { |p| p[:id] }
  end

  def self.name_of(id)
    (by_id(id) || { name: id.to_s })[:name]
  end

  # Grober Bodenradius eines Prefabs (fuer Nahe-Loeschung im Editor):
  # groesste horizontale Ausdehnung ueber alle Teile, Drehung ignoriert.
  def self.radius_of(id)
    prefab = by_id(id)
    return 0.0 unless prefab
    r = 0.0
    prefab[:parts].each do |p|
      dx, _, dz = p[:d]
      ext = Math.sqrt(dx * dx + dz * dz) + [p[:s][0], p[:s][2]].max * 0.5
      r = ext if ext > r
    end
    r
  end

  # Baut die Kaesten eines Objekts in die Szene. Rueckgabe: die neuen Knoten.
  #   base_y  = Gelaendehoehe an der Objektposition
  #   prefix  = Namenspraefix, z.B. "ed_3"
  def self.instantiate(scene, obj, base_y, prefix)
    prefab = by_id(obj['type'])
    return [] unless prefab

    rot = (obj['rot'] || 0.0).to_f
    override = obj['tex'].to_s
    nodes = prefab[:parts].map.with_index do |part, i|
      dx, dy, dz = part[:d]
      # Versatz um die Hochachse drehen, damit gedrehte Objekte stimmen.
      if rot != 0.0
        cos = Math.cos(rot)
        sin = Math.sin(rot)
        dx, dz = dx * cos + dz * sin, -dx * sin + dz * cos
      end
      node = Node.new(name: "#{prefix}_#{i}",
                      position: [obj['x'].to_f + dx, base_y + dy, obj['z'].to_f + dz],
                      rotation: [0.0, rot, 0.0],
                      scale: part[:s].map(&:to_f),
                      color: part[:c],
                      solid: part[:solid] ? true : false)
      node.texture = part[:tex] if part[:tex]
      node.tile    = part[:tile].to_f if part[:tile]
      # Etappe 17f: Objekt-Material aus der Materialbase ueberschreibt das
      # Prefab-Teil (alle Teile, inklusive Kachelgroesse des Materials).
      unless override.empty?
        node.texture = override
        node.tile = Textures.tile_of(override)
      end
      scene.add(node)
      node
    end

    # Animationen: bislang nur der Kristall (Drehen + Schweben). Der Block
    # bekommt die Ruhehoehe mit, damit er unabhaengig vom Standort funktioniert.
    if prefab[:anim] == 'spin'
      rest_y = base_y + prefab[:parts][0][:d][1]
      nodes[0].on_update do |n, dt|
        n.rotation[1] += dt * 1.4
        n.rotation[2]  = Math.sin(n.rotation[1] * 0.7) * 0.15
        n.position[1]  = rest_y + Math.sin(n.rotation[1] * 0.9) * 0.25
      end
    end

    nodes
  end
end