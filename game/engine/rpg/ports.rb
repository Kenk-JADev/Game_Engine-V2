#encoding: utf-8
# ============================================================================
#  Rpg::Ports - Entkopplung von Audio und Ereignissen.
#
#  Die Datenbankschicht nennt nur Asset-Keys und Ereignisnamen. Wer diese
#  Namen später an reale Engine-Systeme hängt, entscheidet die Aufrufstelle
#  per Dependency Injection. Es gibt hier bewusst keinen direkten Aufruf von
#  Rune::Sound, Audio, Graphics oder irgendeiner UI-Klasse.
# ============================================================================

module Rpg
  module Ports
    # Minimalvertrag für einen Audioadapter.
    class AudioPort
      def play(_key)
        raise NotImplementedError, 'AudioPort#play muss von einem Adapter implementiert werden'
      end
    end

    # Standard: stumm. Für Headless-Tests und Serverläufe.
    class NullAudio < AudioPort
      def play(_key) = nil
    end

    # Merkt sich alle Aufrufe; praktisch für Service-Tests ohne Soundkarte.
    class RecordingAudio < AudioPort
      attr_reader :calls

      def initialize
        @calls = []
      end

      def play(key)
        @calls << key.to_s
        key.to_s
      end
    end

    # Minimalvertrag für eine Ereignissenke (Log, UI, Tests, anderer Service).
    class EventSink
      def notify(_type, _payload)
        raise NotImplementedError, 'EventSink#notify muss von einem Adapter implementiert werden'
      end
    end

    class NullEventSink < EventSink
      def notify(_type, _payload) = nil
    end

    class MemoryEventSink < EventSink
      Event = Struct.new(:type, :payload)

      attr_reader :events

      def initialize
        @events = []
      end

      def notify(type, payload)
        @events << Event.new(type.to_s, payload)
      end

      def types = @events.map(&:type).freeze
    end

    # Kleiner In-Prozess-Dispatcher. Kein globaler Singleton: jede Engine-
    # Instanz bekommt ihren eigenen Dispatcher injiziert.
    class EventDispatcher
      def initialize(sink: NullEventSink.new)
        @sink = sink
        @handlers = Hash.new { |hash, key| hash[key] = [] }
      end

      # Gibt eine Unsubscribe-Funktion zurück.
      def subscribe(type, &handler)
        raise ArgumentError, 'subscribe braucht einen Block' unless handler

        @handlers[type.to_s] << handler
        -> { @handlers[type.to_s].delete(handler) }
      end

      def publish(type, payload = {})
        typ = type.to_s
        @sink.notify(typ, payload)
        @handlers.fetch(typ, []).each { |handler| handler.call(payload) }
        payload
      end
    end
  end
end
