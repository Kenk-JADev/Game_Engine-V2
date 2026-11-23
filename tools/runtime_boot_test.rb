#encoding: utf-8
# ============================================================================
#  Echte Ruby-Bootstrap-/Launcher-/Spiel-Integration in einer temporären Kopie.
#  Native Win32-/D3D-/Sound-APIs sind Attrappen: kein GUI-/Hardwaretest.
#     ruby tools/runtime_boot_test.rb
# ============================================================================
require 'tmpdir'
require 'fileutils'
require 'json'
require 'open3'
require 'rbconfig'

module Rune
  module Log
    @errors = []
    @warnings = []
    class << self
      attr_reader :errors, :warnings
    end
    def self.info(_message); end
    def self.warn(message) = @warnings << message
    def self.error(message) = @errors << message
  end

  module World
    class << self
      attr_accessor :height_brushes
      attr_reader :last_args
    end
    def self.generate_raw(*args) = (@last_args = args; true)
    def self.ready? = true
    def self.height_at(_x, _z) = 0.0
  end

  module Gfx
    @boxes = 0
    @texts = []
    class << self
      attr_reader :boxes, :texts
    end
    def self.box(*_args) = (@boxes += 1)
    def self.rect(*_args); end
    def self.text(_x, _y, text, *_args) = (@texts << text)
    def self.texture(*_args); end
    def self.terrain_texture(*_args); end
    def self.terrain_splats(*_args); end
    def self.begin_project_textures; end
    def self.end_project_textures; end
    def self.sky(*_args); end
    def self.sun(*_args); end
    def self.fog(*_args); end
    def self.clear_color=(_color); end
  end

  module Physics
    @solids = []
    class << self
      attr_reader :solids
    end
    def self.clear_solids = @solids.clear
    def self.add_solid(*args) = @solids << args
    def self.solid_count = @solids.size
    def self.solid_at?(*_args) = false
    def self.clear_blocked; end
    def self.add_blocked(*_args); end
    def self.blocked?(*_args) = false
  end

  module Player
    @position = [0.0, 0.0, 0.0]
    def self.position = @position
    def self.teleport(x, y, z) = (@position = [x, y, z])
    def self.on_ground? = true
    def self.push_out(*_args); end
  end

  module Camera
    class << self
      attr_accessor :yaw, :mode, :distance
    end
    def self.ray(_x, _y) = []
    def self.info = 'Testkamera'
  end

  module Input
    @keys = []
    @previous_keys = []
    class << self
      attr_accessor :keys, :capture
    end
    def self.pressed?(key) = @keys.include?(key) && !@previous_keys.include?(key)
    def self.down?(key) = @keys.include?(key)
    def self.end_frame = (@previous_keys = @keys.dup)
    def self.mouse_position = [640, 360]
    def self.mouse_pressed?(_button) = false
    def self.captured? = !!@capture
  end

  module Window
    class << self
      attr_accessor :title
    end
    def self.width = 1280
    def self.height = 720
  end

  module Time
    def self.fps = 60.0
  end

  module Sound
    def self.register(*_args); end
    def self.play(*_args); end
    def self.loop(*_args); end
    def self.stop(*_args); end
  end

  module Assets
    def self.read_texture(*_args) = [:missing, nil]
  end

  module Simulation
    class << self
      attr_accessor :paused
    end
  end

  module App
    class << self
      attr_reader :quit_requested
    end
    def self.quit = (@quit_requested = true)
  end
end

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError, ScriptError => e
  puts "  FAIL #{name} (#{e.class}: #{e.message})"
  $fail += 1
end

# Nativer Esc-Mausfang und Frame-Ende wie in src/main.cpp; Rune.tick enthält
# auch das Zeichnen, damit pressed? bis zum Ende des gesamten Frames gilt.
def runtime_frame(dt = 0.016, draw: true)
  Rune::Input.capture = false if Rune::Input.pressed?(:escape)
  draw ? Rune.tick(dt) : Game.update(dt)
ensure
  Rune::Input.end_frame
end

PLAYTEST_PROJECT = ' A_ä_日_🚀'
PLAYTEST_MAP = ' Karte_日_🚀.'
OTHER_PROJECT = 'B_Projekt'
SPAWN_PROJECT = 'Start_Projekt'

# Ein eigener Ruby-Prozess startet den echten Bootstrap mit den Werten,
# die Rune3D vor VM-Start aus der CLI-Übergabe gesetzt hätte. Alle nativen
# Schnittstellen bleiben Attrappen; Win32 und D3D werden hier nicht getestet.
if ARGV[0] == '--playtest-child'
  root, scenario = ARGV[1], ARGV[2]
  Rune.const_set(:ROOT, root)
  Dir.chdir(root) do
    if scenario == 'inaccessible'
      # Reproduzierbarer Lesefehler ohne von OS-/Adminrechten abhängige ACLs.
      blocked = File.expand_path(File.join(root, 'projekte', PLAYTEST_PROJECT, 'data', 'maps'))
      original_children = Dir.method(:children)
      Dir.define_singleton_method(:children) do |path, *args, **kwargs|
        raise Errno::EACCES, path if File.expand_path(path) == blocked

        original_children.call(path, *args, **kwargs)
      end
    end
    load File.join(root, 'game', 'engine', 'boot.rb')
    case scenario
    when 'selected'
      check('F4 startet die ausgewählte Unicode-Karte statt der System-Startkarte') do
        Game.instance_variable_get(:@mode) == :game && Game.map_id == PLAYTEST_MAP &&
          Game.map['name'] == 'Ausgewählte Karte' && $project_name == PLAYTEST_PROJECT &&
          Database.map_ids.include?(PLAYTEST_MAP) && ENV['RUNE_PLAYTEST_MAP'].nil?
      end
      check('Terrain, Startpunkt, Hindernisse, Event und Encounter gehören zur ausgewählten Karte') do
        x, y, z = Rune::Player.position
        x == 13.0 && y == 0.2 && z == -7.0 && Rune::World.last_args[0..1] == [96, 88] &&
          Rune::Physics.solid_count.positive? && Game.events.map(&:name) == ['Prüf-Event'] &&
          $game_player.encounter_count.positive?
      end
      check('F4 im Spiel öffnet keinen Editor und verändert keine Kartendatei') do
        path = File.join(Project.data_path(PLAYTEST_PROJECT), 'maps', PLAYTEST_MAP + '.JSON')
        before = File.binread(path)
        scene = Game.instance_variable_get(:@scene)
        Game.instance_variable_set(:@intro, nil)
        Rune::Input.keys = [:f4]
        runtime_frame(0.016)
        Rune::Input.keys = []
        Game.instance_variable_get(:@scene).equal?(scene) && !defined?(Editor) && File.binread(path) == before
      ensure
        Rune::Input.keys = []
      end
      check('normaler Kartenwechsel funktioniert auch im externen Playtest') do
        Game.change_map('start')
        Game.map_id == 'start' && Game.map['name'] == 'Projekt-Start' && Rune::Player.position[0] == 1.0
      end
      check('F5-artiges Laden startet wieder die gewählte Karte im selben Projekt') do
        load File.join(root, 'game', 'main.rb')
        Game.map_id == PLAYTEST_MAP && Game.map['name'] == 'Ausgewählte Karte' &&
          Rune::Player.position[0] == 13.0
      end
      check('F5 liest die ausgewählte Kartendatei nach externer Änderung erneut') do
        path = File.join(Project.data_path(PLAYTEST_PROJECT), 'maps', PLAYTEST_MAP + '.JSON')
        before = File.binread(path)
        begin
          updated = JSON.parse(before)
          updated['start']['x'] = 15
          File.write(path, JSON.generate(updated), encoding: 'UTF-8')
          load File.join(root, 'game', 'main.rb')
          Game.map_id == PLAYTEST_MAP && Rune::Player.position[0] == 15.0
        ensure
          File.binwrite(path, before)
        end
      end
      check('nach Titelbildschirm und erneuter Wahl gilt wieder die normale Projekt-Startkarte') do
        Game.to_title!
        Launcher.start(PLAYTEST_PROJECT)
        Game.map_id == 'start' && Game.map['name'] == 'Projekt-Start'
      end
      check('F5 nach manueller Projektwahl erhält den normalen Start') do
        load File.join(root, 'game', 'main.rb')
        Game.map_id == 'start'
      end
      check('Kartenübernahme läuft nicht in ein anderes Projekt mit gleicher Karten-ID aus') do
        Game.to_title!
        Launcher.start(OTHER_PROJECT)
        Game.map_id == 'b_start' && Game.map['name'] == 'Projekt B'
      end
      check('kein Fehler im Laufzeit-Log') { Rune::Log.errors.empty? }
    when 'project_spawn', 'project_spawn_plain', 'project_spawn_other'
      expected_map = scenario == 'project_spawn_other' ? 'andere' : 'start'
      expected_x = scenario == 'project_spawn_other' ? 21.0 : 7.0
      check('expliziter Projektstart gilt für Neustart und passende F4-Karte') do
        Game.map_id == expected_map && Rune::Player.position[0] == expected_x &&
          (scenario == 'project_spawn_other' || Rune::Player.position[2] == -6.0)
      end
      check('RGSS-Startscript läuft nach Globals und Kartenaufbau') do
        $rgss_boot_seen == [[expected_map, 0.0]] && $game_variables[7] == 42
      end
      check('F5 führt RGSS-Startscript erneut nach dem Reset aus') do
        load File.join(root, 'game', 'main.rb')
        $rgss_boot_seen == [[expected_map, 0.0], [expected_map, 0.0]] &&
          $game_variables[7] == 42 && Rune::Player.position[0] == expected_x
      end
      check('Kartenwechsel verwendet weiterhin den Kartenstart, nicht den Projektstart') do
        Game.change_map('andere')
        other_ok = Rune::Player.position[0] == 21.0
        Game.change_map('start')
        other_ok && Rune::Player.position[0] == 1.0
      end
      check('F5 liest eine inzwischen im Editor geänderte Projekt-Startkarte und Position') do
        path = File.join(Project.data_path(SPAWN_PROJECT), 'system.json')
        before = File.binread(path)
        begin
          updated = JSON.parse(before)
          updated['start'].merge!('map' => 'andere', 'x' => 12, 'z' => -8, 'yaw' => 1.25)
          File.write(path, JSON.generate(updated), encoding: 'UTF-8')
          load File.join(root, 'game', 'main.rb')
          # Eine explizite F4-Karte bleibt gewählt; der Projektstart gilt nur
          # bei gleicher Karten-ID. Ohne F4 startet die neue Projektkarte.
          if scenario == 'project_spawn'
            Game.map_id == 'start' && Rune::Player.position[0] == 1.0
          else
            Game.map_id == 'andere' && Rune::Player.position[0] == 12.0 &&
              Rune::Player.position[2] == -8.0 && Rune::Camera.yaw == 1.25
          end
        ensure
          File.binwrite(path, before)
        end
      end
      check('kein Fehler im Laufzeit-Log') { Rune::Log.errors.empty? }
    when 'battle_save'
      # Dieser Kindprozess benutzt das echte Project-Modul und ausschließlich
      # die vom Elterntest angelegte temporäre Projektkopie, keine Save-Attrappe.
      slot_path = File.join(Project.saves_dir(PLAYTEST_PROJECT), 'slot1.json')
      script_path = File.join(Project.data_path(PLAYTEST_PROJECT), 'scripts.json')
      scripts_before = File.binread(script_path) if File.file?(script_path)
      saved_party = nil
      frame = lambda do |key|
        Rune::Input.keys = key ? [key] : []
        runtime_frame
      end
      Battle.singleton_class.send(:define_method, :rand) { 0.9 }
      begin
        check('Action-Event mit Projekt-Script wählt das KO-Ziel über Eingabeflanken') do
          Game.instance_variable_set(:@intro, nil)
          code = "party.find('arik').hp = 60\nparty.find('mira').hp = 0\nparty.find('mira').mp = 2\n"
          File.write(script_path, JSON.generate('scripts' => [{ 'name' => 'kampf_probe', 'code' => code }]),
                     encoding: 'UTF-8')
          scripts_loaded = Scripts.load_project && Scripts.defined?('kampf_probe')
          x, _y, z = Rune::Player.position
          Game.events << Event.new('id' => 77, 'name' => 'Kampfprobe', 'x' => x, 'z' => z, 'radius' => 1,
                                   'trigger' => 'action', 'commands' => [
                                     { 'cmd' => 'script', 'name' => 'kampf_probe' },
                                     { 'cmd' => 'battle', 'name' => 'waldweg',
                                       'can_escape' => true, 'can_lose' => true }
                                   ])
          frame.call(:e)
          started = Game.interpreter.running? && !Battle.active?
          frame.call(nil)
          fighting = Battle.active? && $game_temp.in_battle && Rune::Simulation.paused &&
                     Party.find('arik').hp == 60 && Party.find('mira').hp.zero? && Party.find('mira').mp == 2
          Battle.enemies.each { |enemy| enemy.instance_variable_set(:@atk, 0) }
          frame.call(:down)
          frame.call(:enter)
          frame.call(:enter) # gehalten: keine zweite Bestätigung
          held_item = Battle.phase == :item && Battle.actions.empty?
          frame.call(nil)
          frame.call(:enter)
          frame.call(:enter) # gehalten: Ziel noch nicht bestätigen
          held_target = Battle.phase == :item_target && Battle.actions.empty?
          frame.call(:right)
          frame.call(:enter)
          scripts_loaded && started && fighting && held_item && held_target && Battle.phase == :execute &&
            Battle.actions.one? && Battle.actions.first[:target].id == 'mira' &&
            Party.item_count('heiltrank') == 2
        end
        check('Item wirkt im Kampf und wird erst jetzt verbraucht') do
          frame.call(nil)
          Battle.turn == 1 && Party.find('mira').hp.between?(38, 40) &&
            Party.find('arik').hp + Party.find('mira').hp == 98 &&
            Party.find('mira').mp == 2 && Party.item_count('heiltrank') == 1
        end
        check('Sieg erhält die Partywerte und gibt die Simulation frei') do
          Battle.enemies.each { |enemy| enemy.hp = 0 }
          frame.call(nil)
          Battle.result == :sieg && !Battle.active? && !$game_temp.in_battle && !Rune::Simulation.paused
        end
        check('echtes Project schreibt die Party nach dem Kampf in eine Slot-Datei') do
          absent = !File.exist?(slot_path)
          saved_party = Party.snapshot
          saved = Game.save_game(1)
          from_disk = JSON.parse(File.read(slot_path, encoding: 'UTF-8'))
          absent && saved && File.file?(slot_path) && from_disk['map'] == PLAYTEST_MAP &&
            from_disk['party'] == saved_party && from_disk.dig('party', 'items', 'heiltrank') == 1
        end
        check('Game.load_game liest Datei und stellt HP, MP, Inventar und Karte wieder her') do
          Party.find('arik').hp = 1
          Party.find('mira').hp = 1
          Party.find('mira').mp = 40
          Party.add_item('heiltrank', 9)
          Game.change_map('start')
          changed = Game.map_id == 'start' && Party.snapshot != saved_party
          loaded = Game.load_game(1)
          changed && loaded && Game.map_id == PLAYTEST_MAP && Party.snapshot == saved_party &&
            Party.find('mira').hp.between?(38, 40) && Party.find('mira').mp == 2 &&
            Party.item_count('heiltrank') == 1 && !Battle.active? && !Rune::Simulation.paused
        end
        check('kein Fehler im Laufzeit-Log') { Rune::Log.errors.empty? }
      ensure
        Rune::Input.keys = []
        Battle.abort!
        Battle.singleton_class.send(:remove_method, :rand)
        scripts_before ? File.binwrite(script_path, scripts_before) : FileUtils.rm_f(script_path)
      end
    when 'project_only'
      check('Projekt ohne Kartenoption startet weiterhin die System-Startkarte') do
        Game.instance_variable_get(:@mode) == :game && Game.map_id == 'start' &&
          Game.map['name'] == 'Projekt-Start'
      end
    when 'unknown_project'
      check('unbekanntes Projekt mit Kartenoption zeigt einen Fehler') do
        Game.instance_variable_get(:@mode) == :playtest_error && Game.map_id.nil? &&
          Rune::Simulation.paused && ENV['RUNE_PLAYTEST_MAP'].nil?
      end
      check('F5 bei weiterhin fehlendem Projekt bleibt beim Fehler und wählt keine Ersatzkarte') do
        load File.join(root, 'game', 'main.rb')
        Game.instance_variable_get(:@mode) == :playtest_error && Game.map_id.nil? && Rune::Simulation.paused
      end
      restored_project = Project.path(ENV['RUNE_PROJECT'])
      begin
        FileUtils.cp_r(Project.path(PLAYTEST_PROJECT), restored_project)
        selected = File.join(restored_project, 'data', 'maps', PLAYTEST_MAP + '.JSON')
        backup = selected + '.test-backup'
        File.rename(selected, backup)
        check('wiederhergestelltes Projekt ohne gewählte Karte zeigt weiterhin einen Fehler') do
          load File.join(root, 'game', 'main.rb')
          Game.instance_variable_get(:@mode) == :playtest_error && Game.map_id.nil? && Rune::Simulation.paused
        end
        File.rename(backup, selected)
        check('F5 nach Wiederherstellung von Projekt und Karte startet die ursprüngliche Auswahl') do
          load File.join(root, 'game', 'main.rb')
          Game.instance_variable_get(:@mode) == :game && Game.map_id == PLAYTEST_MAP &&
            Game.map['name'] == 'Ausgewählte Karte' && Rune::Input.captured? && !Rune::Simulation.paused
        end
      ensure
        FileUtils.remove_entry(restored_project) if File.exist?(restored_project)
      end
    when 'unknown_project_change'
      check('unbekanntes Projekt startet zunächst keine Karte') do
        Game.instance_variable_get(:@mode) == :playtest_error && Game.map_id.nil?
      end
      check('geändertes Projekt nach Fehler übernimmt keine fremde Kartenvorgabe') do
        ENV['RUNE_PROJECT'] = OTHER_PROJECT
        load File.join(root, 'game', 'main.rb')
        Game.instance_variable_get(:@mode) == :game && $project_name == OTHER_PROJECT &&
          Game.map_id == 'b_start' && !Rune::Simulation.paused
      end
    when 'delete_retry'
      check('ausgewählte Karte zunächst gestartet') { Game.map_id == PLAYTEST_MAP }
      check('laufender Kampf pausiert die Simulation vor dem F5-Fehler') do
        Game.instance_variable_set(:@intro, nil)
        started = Battle.start('waldweg')
        runtime_frame(0.016, draw: false)
        started && Battle.active? && $game_temp.in_battle && Rune::Simulation.paused
      end
      path = File.join(Project.data_path(PLAYTEST_PROJECT), 'maps', PLAYTEST_MAP + '.JSON')
      backup = path + '.test-backup'
      begin
        File.rename(path, backup)
        check('entfernte F4-Karte bricht den Kampf ab und zeigt den pausierten Fehler') do
          load File.join(root, 'game', 'main.rb')
          Game.instance_variable_get(:@mode) == :playtest_error && Rune::Simulation.paused &&
            !Battle.active? && !$game_temp.in_battle
        end
      ensure
        File.rename(backup, path) if File.exist?(backup)
      end
      check('F5 nach Wiederherstellung startet die gewählte Karte und gibt die Simulation frei') do
        load File.join(root, 'game', 'main.rb')
        Game.instance_variable_get(:@mode) == :game && Game.map_id == PLAYTEST_MAP &&
          Rune::Input.captured? && !Rune::Simulation.paused
      end
    when 'repair'
      check('Karte mit falscher JSON-Wurzel zeigt einen Playtest-Fehler') do
        Game.instance_variable_get(:@mode) == :playtest_error && Rune::Simulation.paused
      end
      path = File.join(Project.data_path(PLAYTEST_PROJECT), 'maps', 'liste.JSON')
      before = File.binread(path)
      begin
        repaired = { 'name' => 'Repariert', 'terrain' => {}, 'objects' => [], 'events' => [],
                     'start' => { 'x' => 4, 'z' => 5 } }
        File.write(path, JSON.generate(repaired), encoding: 'UTF-8')
        check('F5 liest die reparierte Karte statt der alten ungültigen Cache-Wurzel') do
          load File.join(root, 'game', 'main.rb')
          Game.instance_variable_get(:@mode) == :game && Game.map_id == 'liste' &&
            Game.map['name'] == 'Repariert' && Rune::Input.captured? && !Rune::Simulation.paused
        end
      ensure
        File.binwrite(path, before)
      end
    else
      check('fehlendes Projekt oder ungültige Karte starten keine andere Karte') do
        Game.instance_variable_get(:@mode) == :playtest_error && Game.map_id.nil? &&
          Rune::Simulation.paused && !Rune::Input.captured? && ENV['RUNE_PLAYTEST_MAP'].nil?
      end
      check('Fehler wird sichtbar gezeichnet und Esc beendet den Spielprozess') do
        Game.draw
        Rune::Input.keys = [:escape]
        runtime_frame(0.016, draw: false)
        Rune::Gfx.texts.include?('Playtest abgebrochen') &&
          Rune::Gfx.texts.any? { |text| text.include?('[F5]') && text.include?('[Esc]') } &&
          Rune::App.quit_requested && Rune::Log.warnings.any? { |message| message.include?('Playtest abgebrochen') }
      ensure
        Rune::Input.keys = []
      end
      if scenario == 'traversal'
        check('Pfadflucht wurde nicht als Datenbankpfad gelesen') do
          Database.instance_variable_get(:@cache).keys.none? { |key| key.include?('..') }
        end
      end
    end
  end
  puts($fail.zero? ? 'PLAYTEST-KIND OK (0 Fehler)' : "PLAYTEST-KIND FEHLGESCHLAGEN (#{$fail} Fehler)")
  exit($fail.zero? ? 0 : 1)
end

repo = File.expand_path('..', __dir__)
Dir.mktmpdir('rune-laufzeit-') do |root|
  FileUtils.cp_r(File.join(repo, 'game'), root)
  Rune.const_set(:ROOT, root)
  previous_project = ENV.delete('RUNE_PROJECT')
  previous_map = ENV.delete('RUNE_PLAYTEST_MAP')
  begin
    Dir.chdir(root) do
      check('echter Bootstrap lädt Launcher und Demo-Kulisse ohne Ingame-Editor') do
        load File.join(root, 'game', 'engine', 'boot.rb')
        !defined?(Editor) && !File.exist?(File.join(root, 'game', 'engine', 'editor.rb')) &&
          Game.instance_variable_get(:@mode) == :launcher &&
          Launcher.instance_variable_get(:@scene).root.children.any? { |n| n.name.start_with?('ed_') }
      end
      check('Launcher aktualisiert und zeichnet den echten Szenenbaum') do
        vorher = Rune::Gfx.boxes
        runtime_frame(0.016)
        Rune::Gfx.boxes > vorher && Rune::Gfx.texts.any? { |t| t.include?('Willkommen') }
      end
      check('Demo-Projekt startet über den echten Launcher mit Hindernissen und Events') do
        Project.ensure_demo
        Launcher.start('dorf')
        scene = Game.instance_variable_get(:@scene)
        Game.map_id == 'dorf' && scene.root.children.any? { |n| n.name.start_with?('ed_') } &&
          Rune::Physics.solid_count.positive? && Game.events.size == 6 && Rune::Input.captured?
      end
      check('F4 im echten Spiel-Tick ändert weder Editorzustand noch Kartendatei') do
        map_path = File.join(Project.data_path('dorf'), 'maps', 'dorf.json')
        vorher = File.binread(map_path)
        scene = Game.instance_variable_get(:@scene)
        Game.instance_variable_set(:@intro, nil)
        Rune::Input.keys = [:f4]
        runtime_frame(0.016)
        Rune::Input.keys = []
        Game.instance_variable_get(:@scene).equal?(scene) && !defined?(Editor) &&
          !Game.respond_to?(:playtest_start) && File.binread(map_path) == vorher
      ensure
        Rune::Input.keys = []
      end
      check('F5-artiges Neuladen behält das Projekt und baut die Spielszene neu') do
        alte_szene = Game.instance_variable_get(:@scene)
        load File.join(root, 'game', 'main.rb')
        Game.instance_variable_set(:@intro, nil)
        runtime_frame(0.016)
        Game.map_id == 'dorf' && Game.instance_variable_get(:@scene) != alte_szene &&
          Rune::Physics.solid_count.positive? && !defined?(Editor)
      end
      check('kein Fehler im Laufzeit-Log') { Rune::Log.errors.empty? }
    end
  ensure
    ENV['RUNE_PROJECT'] = previous_project
    ENV['RUNE_PLAYTEST_MAP'] = previous_map
  end

  # Zwei isolierte Projekte; die gewählte Karte unterscheidet sich bewusst
  # von der System-Startkarte und hat eine großgeschriebene Dateiendung.
  a_data = File.join(root, 'projekte', PLAYTEST_PROJECT, 'data')
  b_data = File.join(root, 'projekte', OTHER_PROJECT, 'data')
  spawn_data = File.join(root, 'projekte', SPAWN_PROJECT, 'data')
  [a_data, b_data, spawn_data].each do |data|
    FileUtils.mkdir_p(File.join(data, 'maps'))
    FileUtils.cp(File.join(root, 'game', 'data', 'database.json'), File.join(data, 'database.json'))
  end
  write_json = lambda do |path, value|
    File.write(path, JSON.pretty_generate(value) + "\n", encoding: 'UTF-8')
  end
  standard = { 'name' => 'Projekt-Start', 'terrain' => { 'width' => 120, 'height' => 110,
               'resolution' => 64, 'amplitude' => 0 }, 'objects' => [], 'events' => [],
               'start' => { 'x' => 1, 'z' => 2 } }
  chosen = { 'name' => 'Ausgewählte Karte', 'terrain' => { 'width' => 96, 'height' => 88,
             'resolution' => 64, 'amplitude' => 0 },
             'objects' => [{ 'type' => 'haus', 'x' => 0, 'z' => 0 }],
             'events' => [{ 'id' => 1, 'name' => 'Prüf-Event', 'x' => 3, 'z' => 4,
                            'trigger' => 'action', 'commands' => [] }],
             'start' => { 'x' => 13, 'z' => -7, 'yaw' => 0.5 },
             'encounters' => { 'troops' => ['waldweg'], 'steps' => 8 } }
  write_json.call(File.join(a_data, 'system.json'), { 'name' => 'A', 'start' => { 'map' => 'start' } })
  write_json.call(File.join(a_data, 'maps', 'start.json'), standard)
  write_json.call(File.join(a_data, 'maps', PLAYTEST_MAP + '.JSON'), chosen)
  write_json.call(File.join(b_data, 'system.json'), { 'name' => 'B', 'start' => { 'map' => 'b_start' } })
  write_json.call(File.join(b_data, 'maps', 'b_start.json'), standard.merge('name' => 'Projekt B'))
  write_json.call(File.join(b_data, 'maps', PLAYTEST_MAP + '.JSON'), chosen.merge('name' => 'Fremde Karte'))
  write_json.call(File.join(spawn_data, 'system.json'), { 'name' => 'Startprojekt',
    'start' => { 'map' => 'start', 'x' => 7, 'z' => -6, 'yaw' => 0.75, 'use_project_position' => true } })
  write_json.call(File.join(spawn_data, 'maps', 'start.json'), standard)
  write_json.call(File.join(spawn_data, 'maps', 'andere.json'), standard.merge('start' => { 'x' => 21, 'z' => 2 }))
  write_json.call(File.join(spawn_data, 'scripts.json'), { 'scripts' => [
    { 'name' => 'rgss_start', 'autoload' => true,
      'code' => "($rgss_boot_seen ||= []) << [$game_map.map_id, $game_variables[7]]\n$game_variables[7] = 42" },
    { 'name' => 'nur_event', 'code' => "raise 'darf nicht automatisch starten'" }
  ] })
  write_json.call(File.join(a_data, 'secret.json'), chosen.merge('name' => 'Nicht als Karte laden'))
  File.write(File.join(a_data, 'maps', 'kaputt.json'), '{', encoding: 'UTF-8')
  File.write(File.join(a_data, 'maps', 'liste.JSON'), '[]', encoding: 'UTF-8')
  FileUtils.mkdir_p(File.join(a_data, 'maps', 'ordner.json'))

  scenarios = [
    ['ausgewählte Karte und Projektwechsel', 'selected', PLAYTEST_PROJECT, PLAYTEST_MAP],
    ['Kampf-Item und Spielstand auf Platte', 'battle_save', PLAYTEST_PROJECT, PLAYTEST_MAP],
    ['Projekt ohne Kartenoption', 'project_only', PLAYTEST_PROJECT, nil],
    ['expliziter Projektstart mit F4', 'project_spawn', SPAWN_PROJECT, 'start'],
    ['expliziter Projektstart ohne Kartenoption', 'project_spawn_plain', SPAWN_PROJECT, nil],
    ['F4 auf anderer Karte behält deren Kartenstart', 'project_spawn_other', SPAWN_PROJECT, 'andere'],
    ['Karte nach F5 entfernt und wiederhergestellt', 'delete_retry', PLAYTEST_PROJECT, PLAYTEST_MAP],
    ['fehlende Karte', 'missing', PLAYTEST_PROJECT, 'fehlt'],
    ['nicht lesbares Kartenverzeichnis', 'inaccessible', PLAYTEST_PROJECT, PLAYTEST_MAP],
    ['kaputtes Karten-JSON', 'broken', PLAYTEST_PROJECT, 'kaputt'],
    ['Karten-JSON ohne Objektwurzel', 'array', PLAYTEST_PROJECT, 'liste'],
    ['reparierte Kartenwurzel nach F5', 'repair', PLAYTEST_PROJECT, 'liste'],
    ['Kartenverzeichnis statt Datei', 'directory', PLAYTEST_PROJECT, 'ordner'],
    ['versuchte Pfadflucht', 'traversal', PLAYTEST_PROJECT, '../secret'],
    ['unbekanntes Projekt und F5-Wiederherstellung', 'unknown_project', 'nicht_vorhanden', PLAYTEST_MAP],
    ['Projektwechsel nach unbekanntem Projekt', 'unknown_project_change', 'nicht_vorhanden', PLAYTEST_MAP]
  ]
  playtest_env = lambda do |project, map|
    env = { 'RUNE_PROJECT' => project, 'RUNE_PLAYTEST_MAP' => map }
    # Das eingebettete Windows-Ruby verwendet UTF-8. Ein nacktes POSIX-C-
    # Locale würde ENV sonst als ASCII-8BIT liefern und den Test verfälschen.
    env['LC_ALL'] = 'C.UTF-8' unless RbConfig::CONFIG['host_os'].match?(/mswin|mingw/)
    env
  end
  scenarios.each do |label, scenario, project, map|
    env = playtest_env.call(project, map)
    out, err, status = Open3.capture3(env, RbConfig.ruby, '-EUTF-8', File.expand_path(__FILE__),
                                      '--playtest-child', root, scenario)
    ok = status.success? && out.include?('PLAYTEST-KIND OK (0 Fehler)')
    check("separater Ruby-Start: #{label}") { ok }
    puts "#{out}#{err}" unless ok
  end

  # Unter Windows mit nicht case-sensitiven Verzeichnissen sind diese beiden
  # Namen dieselbe Datei. Nur bei tatsächlich zwei Einträgen prüfen.
  lower = File.join(a_data, 'maps', 'doppelt.json')
  upper = File.join(a_data, 'maps', 'doppelt.JSON')
  write_json.call(lower, standard)
  write_json.call(upper, chosen)
  if Dir.children(File.join(a_data, 'maps')).count { |f| f.downcase == 'doppelt.json' } == 2
    env = playtest_env.call(PLAYTEST_PROJECT, 'doppelt')
    out, err, status = Open3.capture3(env, RbConfig.ruby, '-EUTF-8', File.expand_path(__FILE__),
                                      '--playtest-child', root, 'ambiguous')
    ok = status.success? && out.include?('PLAYTEST-KIND OK (0 Fehler)')
    check('mehrdeutige Endungen starten keine zufällige Karte') { ok }
    puts "#{out}#{err}" unless ok
  end
end

puts($fail.zero? ? 'RUNTIME-BOOT-TEST OK (0 Fehler)' : "RUNTIME-BOOT-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
