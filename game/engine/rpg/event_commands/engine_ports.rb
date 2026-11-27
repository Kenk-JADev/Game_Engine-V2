#encoding: utf-8
# ==============================================================================
# Rpg::EventCommands::EnginePorts — die Ports an der echten Rune3D-Engine.
#
# Etappe 43: Die 90 typisierten XP-Befehle wirken damit im laufenden Spiel.
# Jeder Adapter übersetzt genau eine Schnittstelle aus ports.rb auf die
# vorhandenen Engine-Module:
#
#   GameState (Schalter/Variablen/Self-Switches), $game_system (Timer, Audio,
#   Windowskin, Zugriffe), Party, Battle, Pictures, ScreenFx, Weather, Fade,
#   Mover, Shop, NameInput, MessageWindow, $game_temp (XP-Aufträge), Game.
#
# Regeln:
#  - Alle Engine-Objekte werden injiziert (Vorgabe = die echten Module). Damit
#    ist jeder Adapter einzeln mit Attrappen prüfbar.
#  - Was die Engine nicht kann, wird GEMELDET statt vorgetäuscht:
#    EnginePorts.fehlt(befehl, grund) schreibt eine Warnung ins Log und in eine
#    Liste. Die Liste ist die Wahrheit für Doku und Tests.
#  - IDs: Die Rpg-Schicht nutzt eigene Nummern, die Engine Namen ("arik",
#    "heiltrank"). Ressourcen loest auf: erst die Bindungstabelle des Projekts,
#    dann der Key-Endteil der Ressource ("item/heiltrank" -> "heiltrank").
# ==============================================================================

module Rpg
  module EventCommands
    module EnginePorts
      # ------------------------------------------------------- Meldungen
      class << self
        def warnungen
          @warnungen ||= []
        end

        def reset_warnungen!
          @warnungen = []
        end

        # Ein Befehl, den die Engine (noch) nicht ausdrücken kann.
        def fehlt(befehl, grund)
          eintrag = { command: befehl.to_s, grund: grund.to_s }
          warnungen << eintrag
          Rune::Log.warn("Rpg-Event: #{befehl} — #{grund}") if defined?(Rune::Log)
          nil
        end

        def fehlt?(befehl)
          warnungen.any? { |eintrag| eintrag[:command] == befehl.to_s }
        end

        # XP-Befehle tragen operation add/subtract und einen positiven Betrag.
        def vorzeichen(operation, betrag)
          wert = betrag.to_i
          operation.to_s == 'subtract' ? -wert : wert
        end
      end

      # ------------------------------------------------------- Ressourcen
      # Rpg-ID (1, 3, 9) -> Engine-ID ("arik", "heiltrank", "waldweg").
      class Ressourcen
        def initialize(katalog: nil, bindungen: {})
          @katalog = katalog
          @bindungen = Rpg::Support.stringify_keys(bindungen || {})
        end

        def engine_id(sammlung, wert)
          return wert.to_s if wert.is_a?(String) && !wert.match?(/\A\d+\z/)
          return wert.to_s if wert.nil?

          tabelle = @bindungen[sammlung.to_s] || {}
          return tabelle[wert.to_i].to_s if tabelle.key?(wert.to_i)
          return tabelle[wert.to_s].to_s if tabelle.key?(wert.to_s)

          eintrag = liste(sammlung).find { |kandidat| kandidat['id'] == wert.to_i }
          return wert.to_s unless eintrag

          endteil = eintrag['key'].to_s.split('/').last
          endteil.to_s.empty? ? eintrag['name'].to_s : endteil
        end

        def liste(sammlung)
          return [] unless @katalog

          (@katalog.resources[sammlung.to_s] || [])
        end
      end

      # ------------------------------------------------------- Tab 1: Zustand
      class SwitchStore < Ports::ISwitchStore
        def initialize(zustand: nil)
          @zustand = zustand
        end

        def zustand = @zustand || ::GameState
        def get(id) = zustand.switch(id.to_s)
        def set(id, value) = zustand.set_switch(id.to_s, value)
      end

      class VariableStore < Ports::IVariableStore
        def initialize(zustand: nil)
          @zustand = zustand
        end

        def zustand = @zustand || ::GameState
        def get(id) = zustand.variable(id.to_s)
        def set(id, value) = zustand.set_variable(id.to_s, value)
      end

      class SelfSwitchStore < Ports::ISelfSwitchStore
        def initialize(zustand: nil)
          @zustand = zustand
        end

        def zustand = @zustand || ::GameState
        def get(event_key, letter) = zustand.self_switch(event_key.to_s, letter)
        def set(event_key, letter, value) = zustand.set_self_switch(event_key.to_s, letter, value)
      end

      class TimerService < Ports::ITimerPort
        def initialize(system: nil)
          @system = system
        end

        def system = @system || $game_system

        def remaining
          return 0.0 unless system

          system.timer_seconds.to_f
        end

        def change(operation, seconds)
          return EnginePorts.fehlt('control_timer', 'kein $game_system') unless system

          wert = seconds.to_f
          case operation.to_s
          when 'start', 'set' then system.timer_start(wert)
          when 'stop'
            system.timer = 0
            system.timer_working = false
          when 'add' then system.timer_start(remaining + wert)
          when 'subtract' then system.timer_start([remaining - wert, 0.0].max)
          else EnginePorts.fehlt('control_timer', "unbekannte Operation '#{operation}'")
          end
        end
      end

      class InventoryService < Ports::IInventoryService
        def initialize(party: nil, ressourcen: nil)
          @party = party
          @ressourcen = ressourcen || Ressourcen.new
        end

        def party = @party || ::Party

        def change(kind, id, amount, include_equipment, operation: 'add')
          sammlung = kind.to_s
          name = @ressourcen.engine_id(sammlung, id)
          menge = amount.to_i
          case sammlung
          when 'items'
            operation.to_s == 'subtract' ? party.take_item(name, menge) : party.add_item(name, menge)
          when 'weapons', 'armors'
            party.change_equipment_stock(sammlung, name, menge,
                                         include_equipment: include_equipment, operation: operation)
          else
            return EnginePorts.fehlt("change_#{sammlung}", "unbekannte Sammlung '#{sammlung}'")
          end
          nil
        end

        def count(kind, id)
          sammlung = kind.to_s
          name = @ressourcen.engine_id(sammlung, id)
          case sammlung
          when 'items' then party.item_count(name).to_i
          when 'weapons', 'armors' then party.equipment_count(sammlung, name).to_i
          else 0
          end
        end
      end

      class PartyService < Ports::IPartyService
        def initialize(party: nil, ressourcen: nil)
          @party = party
          @ressourcen = ressourcen || Ressourcen.new
        end

        def party = @party || ::Party
        def gold = party.gold.to_i
        def members = party.actors.map(&:id)
        def steps = party.respond_to?(:steps) ? party.steps.to_i : 0

        def change_gold(operation, amount)
          wert = amount.to_i
          wert = -wert if operation.to_s == 'subtract'
          party.add_gold(wert)
        end

        def change_member(actor_id, operation, initialize: false)
          name = @ressourcen.engine_id('actors', actor_id)
          if operation.to_s == 'remove'
            party.member_remove(name)
          else
            party.member_add(name)
            party.heal_all if initialize
          end
        end
      end

      # Zwölf Heldenbefehle. Neun davon kann die Engine, drei meldet sie.
      class ActorService < Ports::IActorService
        def initialize(party: nil, ressourcen: nil)
          @party = party
          @ressourcen = ressourcen || Ressourcen.new
        end

        def party = @party || ::Party

        # XP-Parameternamen auf die Engine-Werte (database.json).
        PARAMETER = { 'max_hp' => 'max_hp', 'max_sp' => 'max_mp', 'strength' => 'atk',
                      'dexterity' => 'def', 'agility' => 'agi', 'intelligence' => 'spi' }.freeze

        def change(command:, params:)
          parameter = Rpg::Support.stringify_keys(params)
          ziel = @ressourcen.engine_id('actors', parameter['actor_id'])
          wert = EnginePorts.vorzeichen(parameter['operation'], parameter['amount'])
          case command.to_s
          when 'change_hp' then aendere_hp(ziel, wert, parameter)
          when 'change_sp' then party.change_mp(ziel, wert)
          when 'change_exp' then party.change_exp(ziel, wert)
          when 'change_level' then party.change_level(ziel, wert)
          when 'recover_all' then party.heal_all
          when 'change_state'
            zustand = @ressourcen.engine_id('states', parameter['state_id'])
            party.change_state(ziel, zustand, mode: parameter['operation'].to_s == 'remove' ? 'remove' : 'add',
                                              turns: parameter['turns'])
          when 'change_parameters'
            name = PARAMETER.fetch(parameter['parameter'].to_s, parameter['parameter'].to_s)
            party.change_param(ziel, name, wert)
          when 'change_skills'
            fertigkeit = @ressourcen.engine_id('skills', parameter['skill_id'])
            party.change_skill(ziel, fertigkeit, mode: parameter['operation'].to_s == 'remove' ? 'remove' : 'add')
          when 'change_actor_name' then party.rename(ziel, parameter['name'])
          when 'change_equipment'
            item = parameter['item_id']
            item_id = item.is_a?(Hash) ? (item['id'] || item['item_id']) : item
            leer = item_id.nil? || item_id.to_s.strip.empty? || item_id.to_s == '0'
            kind = parameter['slot'].to_s == 'weapon' ? 'weapons' : 'armors'
            resolved = leer ? item_id : @ressourcen.engine_id(kind, item_id)
            party.change_equipment(ziel, parameter['slot'], resolved)
          when 'change_actor_class'
            party.change_class(ziel, @ressourcen.engine_id('classes', parameter['class_id']))
          when 'change_actor_graphic'
            party.change_graphic(ziel, character_key: parameter['character_key'],
                                 battler_key: parameter['battler_key'], face_key: parameter['face_key'])
          else
            EnginePorts.fehlt(command, 'unbekannter Heldenbefehl')
          end
        end

        private

        # XP: ohne "Allow Knockout" lässt ein Schaden mindestens 1 HP übrig.
        def aendere_hp(ziel, wert, parameter)
          held = party.find(ziel) || party.held(ziel)
          if wert.negative? && !parameter.fetch('allow_knockout', false) && held
            wert = [wert, -(held.hp - 1)].max
          end
          party.change_hp(ziel, wert)
        end
      end

      class SystemSettings < Ports::ISystemSettingsService
        def initialize(system: nil)
          @system = system
        end

        def system = @system || $game_system

        def windowskin(key:)
          return EnginePorts.fehlt('change_windowskin', 'kein $game_system') unless system

          system.windowskin_name = key.to_s
          MapEffects.windowskin(key.to_s) if defined?(::MapEffects)
        end

        def save_access(allowed:)
          return EnginePorts.fehlt('change_save_access', 'kein $game_system') unless system

          system.save_disabled = !allowed
        end

        def menu_access(allowed:)
          return EnginePorts.fehlt('change_menu_access', 'kein $game_system') unless system

          system.menu_disabled = !allowed
        end

        def encounter(allowed:)
          return EnginePorts.fehlt('change_encounter', 'kein $game_system') unless system

          system.encounter_disabled = !allowed
        end
      end

      # ------------------------------------------------------- Tab 2: Karte
      class MapController < Ports::IMapController
        def initialize(spiel: nil, temp: nil, mover: nil, spieler: nil, ressourcen: nil)
          @spiel = spiel
          @temp = temp
          @mover = mover
          @spieler = spieler
          @ressourcen = ressourcen || Ressourcen.new
        end

        def spiel = @spiel || ::Game
        def temp = @temp || $game_temp
        def mover = @mover || ::Mover
        def spieler = @spieler || $game_player

        # XP 201: Auftrag über $game_temp, TempActions.tick führt ihn im
        # nächsten Frame aus (Kartenwechsel mit Startposition und Blickwinkel).
        def transfer(map_id:, x: nil, y: nil, direction: nil, fade: nil, **_rest)
          return EnginePorts.fehlt('transfer_player', 'kein $game_temp') unless temp

          temp.player_new_map_id = @ressourcen.engine_id('maps', map_id)
          temp.player_new_x = x
          temp.player_new_y = y
          temp.player_new_direction = richtung(direction)
          temp.player_transferring = true
        end

        def set_event_location(target:, event_id: nil, x: nil, y: nil, **_rest)
          kennung = target.to_s == 'event' ? @ressourcen.engine_id('events', event_id) : event_id
          ereignis = ereignis_fuer(target, kennung)
          return EnginePorts.fehlt('set_event_location', "Event '#{event_id}' nicht gefunden") unless ereignis

          ereignis.x = x.to_f if x
          ereignis.z = y.to_f if y
          ereignis
        end

        def scroll(direction:, distance:, speed: 4, **_rest)
          return EnginePorts.fehlt('scroll_map', 'MapEffects fehlt') unless defined?(::MapEffects)

          ::MapEffects.scroll(direction, distance, speed)
        end

        def settings(target:, asset_key:, color: nil, **_rest)
          return EnginePorts.fehlt('change_map_settings', 'MapEffects fehlt') unless defined?(::MapEffects)

          ::MapEffects.map_settings(target, asset_key, color: color)
        end

        def fog_tone(color:, **_rest)
          return EnginePorts.fehlt('change_fog_color_tone', 'MapEffects fehlt') unless defined?(::MapEffects)

          ::MapEffects.fog_tone(color)
        end

        def fog_opacity(opacity:, **_rest)
          return EnginePorts.fehlt('change_fog_opacity', 'MapEffects fehlt') unless defined?(::MapEffects)

          ::MapEffects.fog_opacity(opacity)
        end

        def transparent(flag:)
          return EnginePorts.fehlt('change_transparent_flag', 'kein $game_player') unless spieler

          spieler.transparent = flag
          ::MapEffects.transparent(flag) if defined?(::MapEffects)
        end

        def move_route(target:, steps: [], repeat: false, ignore: false, event_id: nil, **_rest)
          tempo = nil
          frequenz = nil
          wiederholen = repeat
          ueberspringen = ignore
          schritte = []
          Array(steps).each do |roh|
            schritt = Rpg::Support.stringify_keys(roh)
            text, tempo_aus_schritt, freq, route_opt = schritt_text(schritt)
            tempo = tempo_aus_schritt if tempo_aus_schritt
            frequenz = freq if freq
            case route_opt
            when :repeat then wiederholen = true
            when :skip then ueberspringen = true
            end
            schritte << text if text
          end
          ziel = target.to_s == 'event' ? @ressourcen.engine_id('events', event_id) : ziel_fuer(target)
          mover.start(ziel, schritte, repeat: wiederholen, speed: tempo,
                      frequency: frequenz, ignore: ueberspringen)
        end

        def route_active? = mover.active?

        def erase_event(event_key:)
          ereignis = ereignis_fuer('event', event_key)
          return EnginePorts.fehlt('erase_event', "Event '#{event_key}' nicht gefunden") unless ereignis

          ereignis.erase!
        end

        private

        # Ein Rpg-Schritt ist ein Hash (action/direction/dx/dz/value). Mover
        # erwartet Text wie "move_up", "wait", "jump:1:0"; frequency/repeat/skip
        # sind Routen-Optionen (Etappe 48). Rückgabe: [text, tempo, freq, opt].
        def schritt_text(schritt)
          case schritt['action'].to_s
          when 'walk' then ["move_#{schritt['direction']}", nil, nil, nil]
          when 'turn' then ["turn_#{schritt['direction']}", nil, nil, nil]
          when 'wait' then ['wait', nil, nil, nil]
          when 'jump' then [format('jump:%d:%d', schritt['dx'].to_i, schritt['dz'].to_i), nil, nil, nil]
          when 'speed' then [nil, schritt['value'].to_f, nil, nil]
          when 'frequency' then [nil, nil, schritt['value'].to_i, nil]
          when 'repeat' then [nil, nil, nil, :repeat]
          when 'skip' then [nil, nil, nil, :skip]
          else
            EnginePorts.fehlt('set_move_route', "unbekannter Schritt '#{schritt['action']}'")
            [nil, nil, nil, nil]
          end
        end

        def richtung(wert)
          { 'up' => 8, 'down' => 2, 'left' => 4, 'right' => 6 }[wert.to_s] || 0
        end

        def ziel_fuer(target)
          return 'player' if target.to_s == 'player'

          target.to_s
        end

        def ereignis_fuer(target, kennung)
          return nil unless spiel.respond_to?(:events)

          liste = spiel.events
          liste = liste.values if liste.is_a?(Hash)
          schluessel = kennung.to_s
          liste.find do |ereignis|
            ereignis.respond_to?(:name) && ereignis.name.to_s == schluessel ||
              ereignis.respond_to?(:id) && ereignis.id.to_s == schluessel
          end
        end
      end

      class AnimationService < Ports::IAnimationService
        def play(key: nil, animation_key: nil, target: 'player', event_id: nil, **_rest)
          return EnginePorts.fehlt('show_animation', 'MapEffects fehlt') unless defined?(::MapEffects)

          ereignis = nil
          if target.to_s != 'player' && defined?(::Game) && ::Game.respond_to?(:events)
            ereignis = ::Game.events.find { |e| e.respond_to?(:name) && e.name.to_s == event_id.to_s }
          end
          ::MapEffects.show_animation(target, animation_key || key, event: ereignis)
        end
      end

      class PictureService < Ports::IPictureService
        def initialize(bilder: nil)
          @bilder = bilder
        end

        def bilder = @bilder || ::Pictures

        def show(picture_slot_id:, picture_key:, x: 0, y: 0, scale_x: 100, scale_y: 100,
                 opacity: 255, blend: 0, **_rest)
          bilder.show(picture_slot_id.to_i, picture_key.to_s, x: x.to_f, y: y.to_f,
                      zoom_x: scale_x.to_i, zoom_y: scale_y.to_i,
                      opacity: opacity.to_i, blend: blend.to_i)
        end

        def move(picture_slot_id:, x: nil, y: nil, scale_x: nil, scale_y: nil,
                 opacity: nil, blend: nil, duration: 0.0, **_rest)
          bilder.move(picture_slot_id.to_i, x: x, y: y, zoom_x: scale_x, zoom_y: scale_y,
                      opacity: opacity, blend: blend, frames: frames(duration))
        end

        def rotate(picture_slot_id:, degrees: 0, **_rest)
          bilder.rotate(picture_slot_id.to_i, speed: degrees.to_f)
        end

        def tone(picture_slot_id:, color: {}, duration: 0.0, **_rest)
          bilder.tint(picture_slot_id.to_i, tone: ScreenEffectService.kanaele(color), frames: frames(duration))
        end

        def erase(picture_slot_id:, **_rest)
          bilder.erase(picture_slot_id.to_i)
        end

        private

        def frames(sekunden)
          [(sekunden.to_f * ScreenEffectService.frame_rate).round, 0].max
        end
      end

      class WeatherService < Ports::IWeatherService
        NAMEN = { 'none' => 'kein', 'rain' => 'regen', 'snow' => 'schnee', 'storm' => 'regen' }.freeze

        def initialize(wetter: nil)
          @wetter = wetter
        end

        def wetter = @wetter || ::Weather

        def set(kind:, intensity: 1, duration: 0.0)
          # Sturm wird als kräftiger Regen geführt (die Engine kennt kein/regen/schnee).
          name = NAMEN.fetch(kind.to_s, 'kein')
          staerke = kind.to_s == 'none' ? 0.0 : [intensity.to_f, 0.1].max
          staerke = [staerke, 8.0].max if kind.to_s == 'storm'
          begin
            wetter.set(name, staerke, duration.to_f)
          rescue ArgumentError
            wetter.set(name, staerke)
          end
        end
      end

      class TransitionService < Ports::ITransitionService
        def initialize(fade: nil)
          @fade = fade
        end

        def fade = @fade || ::Fade

        def prepare(key: nil, **_rest)
          # Etappe 48: den Übergangs-Key führen; execute_transition blendet.
          ::MapEffects.prepare_transition(key.to_s) if key && defined?(::MapEffects)
          nil
        end

        # Die Engine kennt nur aus/ein. Ein Key wie "transition/fade_in"
        # blendet ein, alles andere blendet aus (XP-Standardübergang).
        def execute(key: nil, seconds: 0.5, **_rest)
          teile = key.to_s.downcase.split(%r{[/_\-. ]})
          richtung = teile.include?('in') || teile.include?('fadein') ? 'ein' : 'aus'
          fade.start(richtung, seconds.to_f)
        end

        def busy? = fade.laeuft?
      end

      class ScreenEffectService < Ports::IScreenEffectService
        # Dieselbe Bildrate wie ScreenFx/Pictures (XP rechnet in 40er-Frames,
        # die Engine nimmt Graphics.frame_rate, wenn sie vorhanden ist).
        def self.frame_rate
          if defined?(::Graphics) && ::Graphics.respond_to?(:frame_rate)
            ::Graphics.frame_rate.to_f
          else
            40.0
          end
        end

        def self.kanaele(farbe)
          werte = Rpg::Support.stringify_keys(farbe || {})
          { r: werte['red'], g: werte['green'], b: werte['blue'], gray: werte['gray'] }
        end

        def initialize(effekte: nil)
          @effekte = effekte
        end

        def effekte = @effekte || ::ScreenFx

        def tone(color: {}, duration: 0.0, **_rest)
          effekte.tone(**ScreenEffectService.kanaele(color), frames: frames(duration))
        end

        def flash(color: {}, duration: 0.0, strength: nil, **_rest)
          effekte.flash(blitz_name(color), strength: (strength || effekte::MAX_STAERKE).to_i,
                                            frames: frames(duration))
        end

        def shake(strength: 5, speed: 5, duration: 0.0, **_rest)
          effekte.shake(power: strength.to_i, speed: speed.to_i, frames: frames(duration))
        end

        # ScreenFx kennt weiss/rot/gruen/blau/gelb. Ein XP-RGB-Wert wird auf
        # den naechsten dieser Namen abgebildet und die Näherung gemeldet.
        def blitz_name(color)
          werte = ScreenEffectService.kanaele(color)
          rot = werte[:r].to_i.clamp(0, 255)
          gruen = werte[:g].to_i.clamp(0, 255)
          blau = werte[:b].to_i.clamp(0, 255)
          kandidat = effekte::FARBEN.min_by do |_name, hex|
            dr = ((hex >> 16) & 255) - rot
            dg = ((hex >> 8) & 255) - gruen
            db = (hex & 255) - blau
            (dr * dr) + (dg * dg) + (db * db)
          end
          name = kandidat.first
          exakt = effekte::FARBEN[name] == ((rot << 16) | (gruen << 8) | blau)
          EnginePorts.fehlt('screen_flash', "RGB(#{rot},#{gruen},#{blau}) naechstliegend '#{name}'") unless exakt
          name
        end

        private

        def frames(sekunden)
          [(sekunden.to_f * ScreenEffectService.frame_rate).round, 0].max
        end
      end

      class AudioService < Ports::IAudioService
        def initialize(system: nil)
          @system = system
        end

        def system = @system || $game_system

        def ton(key, volume, pitch)
          { 'name' => key.to_s, 'volume' => volume.nil? ? 100 : volume.to_i,
            'pitch' => pitch.nil? ? 100 : pitch.to_i }
        end

        def play_bgm(key:, volume: 100, pitch: 100)
          return EnginePorts.fehlt('play_bgm', 'kein $game_system') unless system

          system.bgm_play(ton(key, volume, pitch))
        end

        def fade_out_bgm(key: nil, seconds: 1.0)
          return EnginePorts.fehlt('fade_out_bgm', 'kein $game_system') unless system

          system.bgm_fade(seconds.to_f)
        end

        def play_bgs(key:, volume: 100, pitch: 100)
          return EnginePorts.fehlt('play_bgs', 'kein $game_system') unless system

          system.bgs_play(ton(key, volume, pitch))
        end

        def fade_out_bgs(key: nil, seconds: 1.0)
          return EnginePorts.fehlt('fade_out_bgs', 'kein $game_system') unless system

          system.bgs_fade(seconds.to_f)
        end

        def memorize_bgm_bgs
          return EnginePorts.fehlt('memorize_bgm_bgs', 'kein $game_system') unless system

          system.bgm_memorize
          system.bgs_memorize
        end

        def restore_bgm_bgs
          return EnginePorts.fehlt('restore_bgm_bgs', 'kein $game_system') unless system

          system.bgm_restore
          system.bgs_restore
        end

        def play_me(key:, volume: 100, pitch: 100)
          return EnginePorts.fehlt('play_me', 'kein $game_system') unless system

          system.me_play(ton(key, volume, pitch))
        end

        def play_se(key:, volume: 100, pitch: 100)
          return EnginePorts.fehlt('play_se', 'kein $game_system') unless system

          system.se_play(ton(key, volume, pitch))
        end

        def stop_se(key: nil)
          return EnginePorts.fehlt('stop_se', 'kein $game_system') unless system

          system.se_stop
        end

        def set_battle_bgm(key:, volume: 100, pitch: 100)
          return EnginePorts.fehlt('change_battle_bgm', 'kein $game_system') unless system

          system.battle_bgm = ton(key, volume, pitch)
        end

        def set_battle_end_me(key:, volume: 100, pitch: 100)
          return EnginePorts.fehlt('change_battle_end_me', 'kein $game_system') unless system

          system.battle_end_me = ton(key, volume, pitch)
        end
      end

      # ------------------------------------------------------- Tab 3: Kampf
      class BattleController < Ports::IBattleController
        def initialize(kampf: nil, ressourcen: nil)
          @kampf = kampf
          @ressourcen = ressourcen || Ressourcen.new
        end

        def kampf = @kampf || ::Battle

        def start(troop_id:, can_escape: true, can_lose: false)
          kampf.start(@ressourcen.engine_id('troops', troop_id),
                      can_escape: can_escape, can_lose: can_lose)
        end

        # Ergebnis des letzten Kampfes: Battle.result (:sieg/:niederlage/:flucht).
        ERGEBNIS = { 'sieg' => 'win', 'niederlage' => 'lose', 'flucht' => 'escape' }.freeze

        def result
          return 'win' unless kampf.respond_to?(:result)

          ERGEBNIS.fetch(kampf.result.to_s, 'win')
        end

        def abort = kampf.abort!
        def enemy_present?(slot) = !gegner(slot).nil?

        def enemy(command:, params:)
          parameter = Rpg::Support.stringify_keys(params)
          # Etappe 48: die Engine führt alle Gegnerbefehle selbst (inkl.
          # appearance/transform/animation). Ressourcen-IDs vorher auflösen.
          parameter['state_id'] = @ressourcen.engine_id('states', parameter['state_id']) if parameter['state_id']
          parameter['enemy_id'] = @ressourcen.engine_id('enemies', parameter['enemy_id']) if parameter['enemy_id']
          if kampf.respond_to?(:enemy_command)
            ok = kampf.enemy_command(command, parameter)
            return EnginePorts.fehlt(command, "Gegnerplatz #{parameter['slot']} ist leer oder kein Kampf") unless ok
            return nil
          end
          EnginePorts.fehlt(command, 'Kampfmodul ohne enemy_command')
        end

        def force_action(slot:, action_key:, target_slot: -1, **_rest)
          if kampf.respond_to?(:enemy_command)
            kampf.enemy_command('force_action', { 'slot' => slot, 'action_key' => action_key,
                                                  'target_slot' => target_slot })
            return nil
          end
          EnginePorts.fehlt('force_action', 'Kampfmodul ohne enemy_command')
        end

        private

        def gegner(slot)
          return kampf.enemy_slot(slot) if kampf.respond_to?(:enemy_slot)
          return nil unless kampf.respond_to?(:enemies)

          kampf.enemies[slot.to_i]
        end
      end

      # ------------------------------------------------------- Tab 1/3: UI
      # Text, Wahl und Zahleneingabe laufen über den laufenden Event-Interpreter
      # der Engine (er zeichnet die Fenster bereits). Laden und Namenseingabe
      # nutzen Shop/NameInput, Screens die XP-Aufträge in $game_temp.
      class UINavigationService < Ports::IUINavigationService
        TASTEN = { 2 => :down, 4 => :left, 6 => :right, 8 => :up,
                   11 => :e, 12 => :escape, 13 => :enter, 14 => :shift }.freeze
        # Zwischenspeicher der Zahleneingabe (XP schreibt direkt in die
        # Zielvariable; der Handler übernimmt den Wert nach dem Warten).
        ZAHL_VARIABLE = 'rpg_eingabe'.freeze

        def initialize(interpreter: nil, temp: nil, fenster: nil, laden: nil,
                       namenseingabe: nil, party: nil, eingabe: nil, ressourcen: nil)
          @interpreter = interpreter
          @temp = temp
          @fenster = fenster
          @laden = laden
          @namenseingabe = namenseingabe
          @party = party
          @eingabe = eingabe
          @ressourcen = ressourcen || Ressourcen.new
          @wahl_offen = false
          @zahl_offen = false
          @taste_offen = false
          @name_offen = false
        end

        attr_accessor :interpreter

        def temp = @temp || $game_temp
        def fenster = @fenster || ::MessageWindow
        def laden = @laden || ::Shop
        def namenseingabe = @namenseingabe || ::NameInput
        def party = @party || ::Party
        def eingabe = @eingabe || (defined?(Rune::Input) ? Rune::Input : nil)

        def message(lines:, options: {})
          return EnginePorts.fehlt('show_text', 'kein Event-Interpreter injiziert') unless @interpreter

          optionen = Rpg::Support.stringify_keys(options || {})
          fenster.set(position: position(optionen['position']),
                      frame: rahmen(optionen['background']),
                      transparent: optionen['background'].to_s == 'transparent')
          @interpreter.say(Array(lines))
        end

        # Wahl (XP 102). Der Befehl wird frame-weise wiederholt: beim ersten
        # Aufruf öffnet er das Fenster, danach liefert er die gewählte Option.
        # Solange das Fenster offen ist, meldet busy? true und der Interpreter
        # wartet (Result.wait(:ui, erneut: true)).
        def choices(choices:, options: {})
          return 0 unless @interpreter

          optionen = Rpg::Support.stringify_keys(options || {})
          if @wahl_offen
            return 0 if @interpreter.busy_choices?

            @wahl_offen = false
            index = @interpreter.choice_index.to_i
            # XP "Cancel: Branch": Abbruch (ESC) bekommt einen eigenen Zweig.
            if index == :aus || index == -1 || (index == @wahl_abbruch_index && @wahl_abbruch_index >= 0)
              @interpreter.push_branch(Array(@wahl_abbruch_befehle)) if @wahl_abbruch_befehle.any?
              return @wahl_abbruch_index >= 0 ? @wahl_abbruch_index : -1
            end
            return index
          end

          abbruch = if optionen['cancel_behavior'].to_s == 'disabled'
                      :aus
                    else
                      idx = optionen['cancel_index'].to_i
                      idx.negative? ? :aus : idx
                    end
          @wahl_abbruch_index = optionen['cancel_behavior'].to_s == 'branch' ? optionen['cancel_index'].to_i : -1
          @wahl_abbruch_befehle = Array(optionen['cancel_branch'])
          unless @interpreter.choose(Array(choices), cancel: abbruch)
            EnginePorts.fehlt('show_choices', 'Event-Interpreter ist belegt (Text/Wahl/Zahl läuft bereits)')
            return 0
          end
          @wahl_offen = true
          0
        end

        # Zahleneingabe (XP 103): wie die Wahl erst öffnen, dann Ergebnis lesen.
        def input_number(digits: 4)
          return 0 unless @interpreter

          if @zahl_offen
            return 0 if @interpreter.busy_number?

            @zahl_offen = false
            return GameState.variable(ZAHL_VARIABLE).to_i
          end

          unless @interpreter.number_input(variable: ZAHL_VARIABLE, digits: digits.to_i)
            EnginePorts.fehlt('input_number', 'Event-Interpreter ist belegt (Text/Wahl/Zahl läuft bereits)')
            return 0
          end
          @zahl_offen = true
          0
        end

        # Tasteneingabe (XP 105): wartet, bis eine der XP-Tasten kommt.
        def button_input
          return 0 unless eingabe

          nummer = taste_nummer
          if nummer.positive?
            @taste_offen = false
            return nummer
          end
          @taste_offen = true
          0
        end

        # Abfrage ohne Warten (Bedingung "Taste gedrückt").
        def last_button
          return 0 unless eingabe

          taste_nummer
        end

        def text_options(background: nil, position: nil, speed: nil, no_fast: nil, **_rest)
          fenster.set(position: position(position), frame: rahmen(background),
                      transparent: background.to_s == 'transparent')
          # Etappe 48: Textgeschwindigkeit/No-Fast werden geführt (MessageWindow).
          fenster.speed = speed if speed && fenster.respond_to?(:speed=)
          fenster.no_fast = no_fast unless no_fast.nil? || !fenster.respond_to?(:no_fast=)
          nil
        end

        # XP 302: Shop.start erwartet [{'item'=>id, 'price'=>n}]. Die Engine
        # führt nur Items; Waffen/Rüstungen und Lagermengen meldet sie.
        def shop(key: nil, goods: [])
          waren = []
          Array(goods).each do |ware|
            eintrag = Rpg::Support.stringify_keys(ware)
            art = eintrag['kind'].to_s
            unless art == 'item'
              EnginePorts.fehlt('shop_processing', "Ware '#{art}' kennt der Laden nicht (nur Items)")
              next
            end
            EnginePorts.fehlt('shop_processing', 'Lagermenge wird nicht geführt (Laden ist unbegrenzt)') if eintrag['quantity'].to_i.positive?
            waren << { 'item' => @ressourcen.engine_id('items', eintrag['id']),
                       'price' => eintrag['price'].to_i }
          end
          laden.start(waren)
        end

        # Namenseingabe (XP 303): öffnen, warten, dann den neuen Namen lesen.
        def name_input(actor_id:, max_length: 8)
          name = @ressourcen.engine_id('actors', actor_id)
          if @name_offen
            return '' if namenseingabe.active?

            @name_offen = false
            held = party.held(name)
            return held ? held.name.to_s : ''
          end

          unless namenseingabe.start(name, max: max_length.to_i)
            EnginePorts.fehlt('name_input_processing', "Held '#{name}' ist nicht in der Party")
            return ''
          end
          @name_offen = true
          ''
        end

        def menu
          return EnginePorts.fehlt('call_menu_screen', 'kein $game_temp') unless temp

          temp.menu_calling = true
        end

        def save
          return EnginePorts.fehlt('call_save_screen', 'kein $game_temp') unless temp

          temp.save_calling = true
        end

        def game_over
          return EnginePorts.fehlt('game_over', 'kein $game_temp') unless temp

          temp.gameover = true
        end

        def title
          return EnginePorts.fehlt('return_to_title_screen', 'kein $game_temp') unless temp

          temp.to_title = true
        end

        def busy?
          return true if laden.respond_to?(:active?) && laden.active?
          return true if namenseingabe.respond_to?(:active?) && namenseingabe.active?
          # Tasteneingabe wartet nur, solange keine Taste gedrückt ist.
          return true if @taste_offen && taste_nummer.zero?
          return false unless @interpreter

          @interpreter.text_waiting? || @interpreter.busy_choices? || @interpreter.busy_number?
        end

        private

        def taste_nummer
          TASTEN.each { |nummer, taste| return nummer if eingabe.pressed?(taste) }
          0
        end

        def position(wert)
          { 'top' => 'oben', 'middle' => 'mitte', 'bottom' => 'unten' }[wert.to_s]
        end

        def rahmen(wert)
          return nil if wert.nil?

          wert.to_s != 'transparent'
        end
      end

      class ScriptService < Ports::IScriptService
        def initialize(scripts: nil)
          @scripts = scripts
        end

        def scripts = @scripts || (defined?(::Scripts) ? ::Scripts : nil)

        def run(script_key: nil, expression: nil, code: nil, context: {})
          quelle = code || expression
          if quelle && scripts.respond_to?(:eval_code)
            return scripts.eval_code(quelle, context)
          elsif script_key && scripts.respond_to?(:run)
            return scripts.run(script_key.to_s, context)
          end

          EnginePorts.fehlt('script', "Script '#{script_key}' — kein Script-Modul injiziert")
        end
      end

      # ------------------------------------------------------- Zusammenbau
      # Baut den vollständigen PortSet für das laufende Spiel. Alle Parameter
      # sind injizierbar; ohne Angaben greifen die echten Engine-Module.
      def self.build(katalog: nil, bindungen: {}, interpreter: nil, zustand: nil,
                     system: nil, party: nil, kampf: nil, spiel: nil, temp: nil,
                     mover: nil, spieler: nil, bilder: nil, wetter: nil, fade: nil,
                     effekte: nil, laden: nil, namenseingabe: nil, fenster: nil,
                     eingabe: nil, scripts: nil, dispatcher: nil)
        ressourcen = Ressourcen.new(katalog: katalog, bindungen: bindungen)
        Ports::PortSet.new(
          switches: SwitchStore.new(zustand: zustand),
          variables: VariableStore.new(zustand: zustand),
          self_switches: SelfSwitchStore.new(zustand: zustand),
          timer: TimerService.new(system: system),
          inventory: InventoryService.new(party: party, ressourcen: ressourcen),
          party: PartyService.new(party: party, ressourcen: ressourcen),
          actors: ActorService.new(party: party, ressourcen: ressourcen),
          map: MapController.new(spiel: spiel, temp: temp, mover: mover,
                                 spieler: spieler, ressourcen: ressourcen),
          animation: AnimationService.new,
          pictures: PictureService.new(bilder: bilder),
          weather: WeatherService.new(wetter: wetter),
          transition: TransitionService.new(fade: fade),
          screen: ScreenEffectService.new(effekte: effekte),
          audio: AudioService.new(system: system),
          battle: BattleController.new(kampf: kampf, ressourcen: ressourcen),
          ui: UINavigationService.new(interpreter: interpreter, temp: temp, fenster: fenster,
                                      laden: laden, namenseingabe: namenseingabe, party: party,
                                      eingabe: eingabe, ressourcen: ressourcen),
          script: ScriptService.new(scripts: scripts),
          system_settings: SystemSettings.new(system: system),
          dispatcher: dispatcher || Rpg::Ports::EventDispatcher.new
        )
      end
    end
  end
end
