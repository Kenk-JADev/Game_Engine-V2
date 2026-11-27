#encoding: utf-8
# ============================================================================
#  Rpg - modulare Datenbankschicht (Etappe 40).
#
#  Nur dieser Einstieg wird von boot.rb geladen. Alle Klassen liegen in
#  game/engine/rpg/ und haben keine Abhängigkeit zu UI, Rendering oder den
#  bestehenden $data_*/$game_*-Globalen.
# ============================================================================

module Rpg
  # Erzeugt eine Datenbank explizit pro Aufruf; bewusst kein Singleton.
  def self.load_database(path: Loader::DEFAULT_PATH,
                         file_system: Loader::LocalFileSystem.new,
                         reporter: Support::NullReporter.new)
    Loader.new(file_system: file_system, reporter: reporter).load(path)
  end

  # Baut die zusammenhängenden Services mit injizierten Ports. Aufrufer
  # entscheidet, ob Audio stumm, aufzeichnend oder an die Engine angebunden ist.
  def self.build_services(database:, audio_port: Ports::NullAudio.new,
                          dispatcher: Ports::EventDispatcher.new, random: nil)
    zustände = StateResolver.new(database: database, dispatcher: dispatcher)
    ausrüstung = EquipmentStatAggregator.new(database: database)
    kalkulator = DamageCalculator.new(state_resolver: zustände,
                                      equipment_aggregator: ausrüstung, random: random)
    formeln = CombatFormulaService.new(
      database: database, state_resolver: zustände,
      equipment_aggregator: ausrüstung, calculator: kalkulator,
      dispatcher: dispatcher, random: random
    )
    tiles = TileQueryService.new(database: database, dispatcher: dispatcher)
    audio = AudioService.new(database: database, audio_port: audio_port,
                             dispatcher: dispatcher)
    ereignisse = EventExecutionService.new(database: database, dispatcher: dispatcher,
                                           audio_service: audio)
    {
      state_resolver: zustände, equipment_aggregator: ausrüstung,
      damage_calculator: kalkulator, combat_formulas: formeln,
      tile_query: tiles, audio: audio, event_execution: ereignisse,
      runtime: RuntimeFactory.new(database), dispatcher: dispatcher
    }
  end

  # Lädt die eigene Event-Command-Konfiguration explizit pro Aufruf; keine
  # Globaldatenbank und kein Singleton.
  def self.load_event_commands(path: EventCommands::Loader::DEFAULT_PATH,
                               file_system: EventCommands::Support::LocalFileSystem.new,
                               reporter: EventCommands::Support::NullReporter.new)
    EventCommands::Loader.new(file_system: file_system, reporter: reporter).load(path)
  end

  # Baut einen Interpreter mit injizierten Ports. Ohne eigene Ports entstehen
  # stumme Null-Adapter, die sich für Tests und Trockenläufe eignen.
  def self.build_event_interpreter(catalog:, ports: EventCommands::Ports::PortSet.new,
                                   dispatcher: nil, registry: nil, random: nil)
    EventCommands::Interpreter.new(
      catalog: catalog,
      ports: ports,
      dispatcher: dispatcher || ports.dispatcher,
      registry: registry || EventCommands::Handlers::Registry.new,
      random: random || Random.new
    )
  end
end

require 'rpg/support'
require 'rpg/models'
require 'rpg/ports'
require 'rpg/repositories'
require 'rpg/runtime'
require 'rpg/loader'
require 'rpg/services'
require 'rpg/event_commands'
