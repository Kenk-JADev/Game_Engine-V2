# encoding: utf-8
# Exportiert Prefabs::LIST nach game/data/prefabs.json (eine Quelle für
# Spiel und RuneEditor). Bei Prefab-Änderungen: ruby tools/export_prefabs.rb
require './game/engine/prefabs.rb'
require 'json'

# Kompakt-Pretty: Objekte mehrzeilig, Zahlen-Arrays in einer Zeile.
def dump(v, indent = 0)
  pad = '  ' * indent
  case v
  when Hash
    return '{}' if v.empty?
    lines = v.map { |k, x| "#{pad}  #{k.to_s.inspect}: #{dump(x, indent + 1)}" }
    "{\n" + lines.join(",\n") + "\n#{pad}}"
  when Array
    return '[]' if v.empty?
    if v.all? { |x| x.is_a?(Numeric) }
      '[' + v.map(&:to_s).join(', ') + ']'
    else
      lines = v.map { |x| "#{pad}  #{dump(x, indent + 1)}" }
      "[\n" + lines.join(",\n") + "\n#{pad}]"
    end
  else
    v.inspect
  end
end

list = Prefabs::LIST.map do |p|
  h = { 'id' => p[:id], 'name' => p[:name] }
  h['anim'] = p[:anim] if p[:anim]
  h['parts'] = p[:parts].map do |q|
    part = { 'd' => q[:d], 's' => q[:s], 'c' => q[:c] }
    part['tex'] = q[:tex] if q[:tex]
    part['tile'] = q[:tile] if q[:tile]
    part['solid'] = true if q[:solid]
    part
  end
  h
end

out = "{\n  \"prefabs\": #{dump(list, 1)}\n}\n"
File.write('game/data/prefabs.json', out)
puts "prefabs.json geschrieben: #{list.size} Prefabs"
