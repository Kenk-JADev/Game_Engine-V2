#encoding: utf-8
# ==============================================================================
# Rpg::EventCommands::Interpreter — Frame-Stack, Wait- und Ablaufsteuerung
# ==============================================================================

module Rpg
  module EventCommands
    class Interpreter
      MAX_DEPTH = 16
      MAX_STEPS_PER_UPDATE = 200
      MAX_LOOP_REPEATS = 10_000

      Frame = Struct.new(:commands, :index, :indent, :labels, :kind,
                         :loop_command, :repeats, :context, keyword_init: true) do
        def loop? = kind == :loop
      end

      # erneut: Der Befehl wird nach dem Warten nochmal gelesen (XP: ein
      # blockierender Befehl liefert erst false, dann sein Ergebnis).
      Wait = Data.define(:kind, :seconds, :erneut) do
        def initialize(kind:, seconds: 0.0, erneut: false)
          super(kind: kind, seconds: seconds, erneut: erneut)
        end
      end

      attr_reader :catalog, :ports, :registry, :dispatcher, :frames, :wait,
                  :random, :event_key
      attr_accessor :choice_result

      def initialize(catalog:, ports: Ports::PortSet.new,
                     dispatcher: nil, registry: Handlers::Registry.new,
                     random: Random.new, event_key: nil, context: {})
        raise ArgumentError, 'Katalog fehlt' unless catalog

        @catalog = catalog
        @ports = ports
        @dispatcher = dispatcher || ports.dispatcher
        @registry = registry
        @random = random
        @event_key = event_key
        @context = context.dup
        @frames = []
        @wait = nil
        @running = false
        @choice_result = nil
      end

      def start(program, context: {}, event_key: nil)
        @context = context.dup.merge(program_id: program.id)
        @event_key = event_key || @context[:event_key] || @context['event_key'] ||
                     "program/#{program.id}"
        @frames = [Frame.new(commands: program.commands, index: 0, indent: 0,
                             labels: program.labels, kind: :program, repeats: 0,
                             context: @context)]
        @wait = nil
        @running = true
        @choice_result = nil
        self
      end

      def running? = @running && !@frames.empty?
      def busy? = running? && !@wait.nil?
      def context = @frames.last&.context || @context

      # Befehlszeiger des obersten Frames — XP nennt ihn @index. Nil, wenn
      # kein Programm läuft. Zusammen mit call_stack ist der komplette
      # Ausführungszustand von außen lesbar (Tests, Debugger, Editor).
      def program_counter = @frames.last&.index
      def depth = @frames.size
      def wait_kind = @wait&.kind
      def waiting? = !@wait.nil?

      def current_command
        frame = @frames.last
        return nil unless frame

        frame.commands[frame.index]
      end

      # Aufrufstapel: Programm, Blöcke, Schleifen und Common-Event-Aufrufe in
      # der Reihenfolge ihrer Entstehung, jeweils mit eigenem Befehlszeiger.
      def call_stack
        @frames.map do |frame|
          { kind: frame.kind, indent: frame.indent, index: frame.index,
            commands: frame.commands.size, repeats: frame.repeats,
            program_id: frame.context[:program_id],
            common_event_id: frame.context[:common_event_id] }
        end.freeze
      end

      def common_event_depth = @frames.count { |frame| frame.kind == :common }

      def update(delta_seconds = 0.0)
        return self unless running?

        if @wait
          tick_wait(delta_seconds.to_f)
          return self unless wait_ready?

          @wait = nil
        end

        schritte = 0
        while running? && schritte < MAX_STEPS_PER_UPDATE
          schritte += 1
          frame = @frames.last
          if frame.index >= frame.commands.size
            finish_frame(frame)
            next
          end

          command = frame.commands[frame.index]
          ergebnis = execute(command, frame)
          wende_an(ergebnis, frame, command)
          if @wait
            # XP (063 - Interpreter 1.rb) zählt den Wartebefehl schon im selben
            # Update herunter: die Schleife läuft weiter und sieht im nächsten
            # Durchgang "@wait_count > 0". Deshalb bekommt ein frisch gesetztes
            # Warten das Delta dieses Updates ebenfalls ab.
            tick_wait(delta_seconds.to_f)
            break unless wait_ready?

            @wait = nil
          end
        end
        self
      end

      def run(program, delta_seconds: 1.0, max_updates: 1000)
        start(program)
        max_updates.times do
          break unless running?

          update(delta_seconds)
        end
        self
      end

      def emit(type, payload = {})
        @dispatcher.publish(type, payload)
        payload
      end

      def register(type, handler)
        @registry.register(type, handler)
        self
      end

      private

      def execute(command, frame)
        definition = @catalog.definition(command.commandType)
        raise KeyError, "Kein Katalogeintrag für #{command.commandType}" unless definition

        emit('command.executed', type: command.commandType, code: command.code,
                                 indent: command.indent, frame: frame.kind)
        handler = @registry.for(definition)
        handler.call(command, self)
      rescue StandardError => e
        emit('command.failed', type: command.commandType, error: e.message)
        raise
      end

      def wende_an(ergebnis, frame, command)
        case ergebnis.action
        when :next then frame.index += 1
        when :push then push_block(ergebnis.value, command)
        when :call then call_common(ergebnis.value)
        when :jump then jump_to(ergebnis.value)
        when :break_loop then break_loop
        when :exit_frame then exit_frame
        when :exit_all then beenden
        when :wait
          # Wie XP (063 - Interpreter 1.rb: nach erfolgreichem Befehl
          # "@index += 1"): Der Befehl ist ausgeführt, danach wird gewartet.
          # Ohne diesen Schritt würde derselbe Wait-Befehl ewig neu starten
          # und das Programm liefe nie zu Ende.
          # Ausnahme erneut: Wahl/Zahl/Taste/Name liefern ihr Ergebnis erst,
          # wenn das Fenster zu ist - dann wird derselbe Befehl nochmal gelesen.
          wiederholen = ergebnis.data.fetch(:erneut, false)
          frame.index += 1 unless wiederholen
          @wait = Wait.new(kind: ergebnis.value, seconds: ergebnis.data.fetch(:seconds, 0.0).to_f,
                           erneut: wiederholen)
        else frame.index += 1
        end
      end

      def push_block(name, command, loop: nil)
        eltern_frame = @frames.last
        block = command.block(name)
        eltern_frame.index += 1
        return if block.empty?

        ist_schleife = loop.nil? ? command.commandType == 'loop' : loop
        @frames << Frame.new(
          commands: block,
          index: 0,
          indent: eltern_frame.indent + 1,
          labels: labels_fuer(block),
          kind: ist_schleife ? :loop : :block,
          loop_command: ist_schleife ? command : nil,
          repeats: 0,
          context: eltern_frame.context
        )
      end

      def finish_frame(frame)
        if frame.loop? && frame.repeats < MAX_LOOP_REPEATS
          frame.index = 0
          frame.repeats += 1
          emit('loop.repeated', repeats: frame.repeats)
          return
        end
        emit('loop.limit', repeats: frame.repeats) if frame.loop? && frame.repeats >= MAX_LOOP_REPEATS
        emit('common_event.finished', common_event_id: frame.context[:common_event_id]) if frame.kind == :common
        @frames.pop
        @running = false if @frames.empty?
      end

      def break_loop
        index = @frames.rindex(&:loop?)
        unless index
          emit('loop.missing')
          return
        end
        @frames.slice!(index..-1)
        emit('loop.broken')
      end

      def jump_to(name)
        ziel = nil
        frame_index = nil
        @frames.each_with_index do |frame, index|
          if frame.labels.key?(name)
            ziel = frame.labels[name]
            frame_index = index
            break
          end
        end
        unless ziel
          emit('jump.missing', label: name)
          return
        end
        @frames.slice!((frame_index + 1)..-1) if frame_index + 1 < @frames.size
        frame = @frames[frame_index]
        frame.index = frame.commands.index(ziel) || 0
        emit('jump.executed', label: name)
      end

      def call_common(id)
        if @frames.count { |frame| frame.kind == :common } >= MAX_DEPTH
          emit('common_event.recursion_limit', common_event_id: id)
          return
        end
        programm = @catalog.program(id)
        unless programm
          emit('common_event.missing', common_event_id: id)
          return
        end
        eltern_frame = @frames.last
        eltern_frame.index += 1
        emit('common_event.started', common_event_id: id)
        @frames << Frame.new(
          commands: programm.commands,
          index: 0,
          indent: eltern_frame.indent + 1,
          labels: programm.labels,
          kind: :common,
          repeats: 0,
          context: eltern_frame.context.merge(common_event_id: id.to_i)
        )
      end

      def tick_wait(seconds)
        return unless @wait.kind == :time

        @wait = Wait.new(kind: @wait.kind, seconds: [@wait.seconds - seconds, 0.0].max, erneut: @wait.erneut)
      end

      def wait_ready?
        case @wait.kind
        when :time then @wait.seconds <= 0.0
        when :move then !@ports.map.route_active?
        when :transition then !@ports.transition.busy?
        when :ui then !@ports.ui.busy?
        else true
        end
      end

      def labels_fuer(commands)
        commands.each_with_object({}) do |command, treffer|
          treffer[command.params['name']] = command if command.commandType == 'label'
        end.freeze
      end

      def exit_frame
        frame = @frames.last
        if @frames.size > 1
          emit('common_event.finished', common_event_id: frame.context[:common_event_id]) if frame.kind == :common
          @frames.pop
        else
          beenden
        end
      end

      def beenden
        @frames.clear
        @running = false
        @wait = nil
      end
    end
  end
end
