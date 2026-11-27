#encoding: utf-8
# ============================================================================
#  Rpg::EventCommands — Einstieg für die typisierte Event-Command-Schicht.
#
#  Nur diese Datei wird von rpg.rb geladen. Alle Teilmodule liegen unter
#  game/engine/rpg/event_commands/ und bleiben ohne UI-/Renderer-Abhängigkeit.
# ============================================================================

module Rpg
  module EventCommands
  end
end

require 'rpg/event_commands/support'
require 'rpg/event_commands/layout'
require 'rpg/event_commands/models'
require 'rpg/event_commands/catalog'
require 'rpg/event_commands/typed'
require 'rpg/event_commands/ports'
require 'rpg/event_commands/handlers'
require 'rpg/event_commands/loader'
require 'rpg/event_commands/command_validator'
require 'rpg/event_commands/interpreter'
require 'rpg/event_commands/engine_ports'
