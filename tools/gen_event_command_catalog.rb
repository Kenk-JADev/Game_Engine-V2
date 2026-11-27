#encoding: utf-8
# ============================================================================
#  Etappe 44: Gibt Katalog + Layout der Rpg-Event-Befehle als JSON aus.
#  Grundlage fuer tools/gen_event_command_catalog.py, das daraus den
#  portablen C++-Katalog src/editor/event_command_catalog.h erzeugt.
#     ruby -I game/engine tools/gen_event_command_catalog.rb > katalog.json
# ============================================================================
$LOAD_PATH.unshift(File.join(__dir__, '..', 'game', 'engine'))
require 'rpg'
k = Rpg.load_event_commands
layout = Rpg::EventCommands::Layout.tabs.map do |t|
  { 'number' => t.number, 'title' => t.title,
    'columns' => t.columns.map { |c|
      { 'side' => c.side.to_s,
        'sections' => c.sections.map { |s|
          { 'key' => s.key, 'title' => s.title,
            'entries' => s.entries.map { |e| { 'type' => e.type, 'code' => e.code, 'label' => e.label, 'xp_name' => e.xp_name } } }
        } }
    } }
end
defs = k.definitions.map do |d|
  { 'type' => d.type, 'code' => d.code, 'tab' => d.tab, 'column' => d.column.to_s,
    'section' => d.section, 'label' => d.label, 'xp_name' => d.xp_name, 'position' => d.position,
    'blocks' => d.blocks, 'summary' => d.summary,
    'fields' => d.fields.values.map { |f|
      { 'name' => f.name, 'kind' => f.kind.to_s, 'required' => f.required, 'default' => f.default,
        'values' => f.values, 'min' => f.min, 'max' => f.max, 'ref' => f.ref, 'key' => f.key }
    } }
end
puts '===LAYOUT==='
puts JSON.generate(layout)
puts '===CATALOG==='
puts JSON.generate(defs)
