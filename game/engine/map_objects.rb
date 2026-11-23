#encoding: utf-8
# Laufzeit-Szenenbau für die Prefab-Objekte einer Karte. Spiel und Launcher
# verwenden denselben Aufbau; die Bearbeitung gehört zu RuneEditor.exe.
class MapObjects
  def initialize(scene, height_at:)
    @scene = scene
    @height_at = height_at
    @indices = {}.compare_by_identity
  end

  # Nur die von diesem Aufbau erzeugten Knoten ersetzen. Skripte dürfen
  # beliebige eigene Namen verwenden, auch den historischen Präfix ed_.
  def rebuild(objects)
    @scene.root.children.reject! { |node| @indices.key?(node) }
    @indices = {}.compare_by_identity
    objects.each_with_index do |obj, index|
      y = @height_at.call(obj['x'].to_f, obj['z'].to_f)
      # Den alten Namen für bestehende Skripte beibehalten; für die Zuordnung
      # wird aber die tatsächliche Knotenidentität statt des Namens genutzt.
      Prefabs.instantiate(@scene, obj, y, "ed_#{index}").each do |node|
        @indices[node] = index
      end
    end
    @scene.refresh_solids
    self
  end

  def object_index(node)
    @indices[node]
  end
end
