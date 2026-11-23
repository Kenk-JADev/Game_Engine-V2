#encoding: utf-8
# ============================================================================
#  Echte Ruby-Bootstrap-/Launcher-/Spiel-Integration in einer temporären Kopie.
#  Native Win32-/D3D-/Sound-APIs sind Attrappen: kein GUI-/Hardwaretest.
#     ruby tools/runtime_boot_test.rb
# ============================================================================
require 'tmpdir'
require 'fileutils'

module Rune
  module Log
    @errors = []
    class << self
      attr_reader :errors
    end
    def self.info(_message); end
    def self.warn(_message); end
    def self.error(message) = @errors << message
  end

  module World
    class << self
      attr_accessor :height_brushes
    end
    def self.generate_raw(*_args) = true
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
    class << self
      attr_accessor :keys, :capture
    end
    def self.pressed?(key) = @keys.include?(key)
    def self.down?(key) = @keys.include?(key)
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
    def self.quit; end
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

repo = File.expand_path('..', __dir__)
Dir.mktmpdir('rune-laufzeit-') do |root|
  FileUtils.cp_r(File.join(repo, 'game'), root)
  Rune.const_set(:ROOT, root)
  previous_project = ENV.delete('RUNE_PROJECT')
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
        Rune.tick(0.016)
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
        Rune.tick(0.016)
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
        Rune.tick(0.016)
        Game.map_id == 'dorf' && Game.instance_variable_get(:@scene) != alte_szene &&
          Rune::Physics.solid_count.positive? && !defined?(Editor)
      end
      check('kein Fehler im Laufzeit-Log') { Rune::Log.errors.empty? }
    end
  ensure
    ENV['RUNE_PROJECT'] = previous_project
  end
end

puts($fail.zero? ? 'RUNTIME-BOOT-TEST OK (0 Fehler)' : "RUNTIME-BOOT-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
