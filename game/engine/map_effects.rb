#encoding: utf-8
# ============================================================================
#  MapEffects — Etappe 48: Karten-/Bildschirm-Effekte der XP-Befehle 203-221.
#
#  Bündelt die Laufzeitwirkungen, die bisher als "Engine-Grenze" gemeldet wurden:
#    scroll_map (203)          -> freier Kamera-Versatz (Rune::Camera.scroll)
#    change_map_settings (204) -> Panorama/Nebel/Battleback-Key + Farbe
#    change_fog_color_tone(205)-> Nebelfarbe/Ton (Distanznebel + Overlay)
#    change_fog_opacity (206)  -> Nebel-Deckkraft (Overlay)
#    show_animation (207)      -> Partikelanimation an Spieler/Event
#    change_transparent_flag(208) -> $game_player.transparent
#    prepare_transition (221)  -> Übergangs-Key für execute_transition (222)
#    change_windowskin (131)   -> Fenstergrafik-Key (MessageWindow/UI)
#
#  Kopflos testbar: die Setter führen Zustand und melden fehlende Renderer nur
#  einmal; update/draw sind ohne Rune::Gfx/Camera stille No-Ops.
# ============================================================================

module MapEffects
  @scroll = nil            # { x:, z:, rest:, dauer:, von_x:, von_z:, ziel_x:, ziel_z: }
  @fog = { color: 0xBFD8EC, opacity: 0, tone: { r: 0, g: 0, b: 0, gray: 0 } }
  @panorama_key = ''
  @fog_key = ''
  @battleback_key = ''
  @transition_key = ''
  @windowskin_key = ''
  @gemeldet = {}

  class << self
    attr_reader :fog, :panorama_key, :fog_key, :battleback_key, :transition_key, :windowskin_key

    def reset!
      @scroll = nil
      @fog = { color: 0xBFD8EC, opacity: 0, tone: { r: 0, g: 0, b: 0, gray: 0 } }
      @panorama_key = ''
      @fog_key = ''
      @battleback_key = ''
      @transition_key = ''
      @windowskin_key = ''
      set_camera_scroll(0.0, 0.0)
      apply_fog
    end

    # XP 203: direction up/down/left/right, distance in Kacheln (1 m), speed 1..10.
    def scroll(direction, distance, speed)
      dist = distance.to_i.clamp(1, 999).to_f
      dx, dz = case direction.to_s
               when 'up' then [0.0, -dist]
               when 'down' then [0.0, dist]
               when 'left' then [-dist, 0.0]
               when 'right' then [dist, 0.0]
               else [0.0, 0.0]
               end
      current = camera_scroll
      dauer = dist / (speed.to_i.clamp(1, 10) * 2.0)
      @scroll = { von_x: current[0], von_z: current[1],
                  ziel_x: current[0] + dx, ziel_z: current[1] + dz,
                  dauer: dauer, rest: dauer }
      Rune::Log.info("MapEffects: Scroll #{direction} #{dist} m (#{speed})") if defined?(Rune::Log)
      true
    end

    def scrolling? = !@scroll.nil?

    # XP 204: target panorama/fog/battleback, asset_key, optional color.
    def map_settings(target, asset_key, color: nil)
      case target.to_s
      when 'panorama'
        @panorama_key = asset_key.to_s
        register_image('panorama', @panorama_key)
      when 'fog'
        @fog_key = asset_key.to_s
        register_image('fog', @fog_key)
      when 'battleback'
        @battleback_key = asset_key.to_s
        register_image('battleback', @battleback_key)
      else return false
      end
      if color.is_a?(Hash)
        @fog[:color] = rgb(color)
        apply_fog
      end
      true
    end

    # XP 205: Nebelfarbe/Ton. color = {r,g,b,gray} -255..255.
    def fog_tone(color)
      c = color.is_a?(Hash) ? color : {}
      @fog[:tone] = { r: c['r'].to_i.clamp(-255, 255), g: c['g'].to_i.clamp(-255, 255),
                      b: c['b'].to_i.clamp(-255, 255), gray: c['gray'].to_i.clamp(-255, 255) }
      base = @fog[:color]
      r = ((base >> 16) & 0xFF) + @fog[:tone][:r]
      g = ((base >> 8) & 0xFF) + @fog[:tone][:g]
      b = (base & 0xFF) + @fog[:tone][:b]
      @fog[:color] = (r.clamp(0, 255) << 16) | (g.clamp(0, 255) << 8) | b.clamp(0, 255)
      apply_fog
      true
    end

    # XP 206: Deckkraft 0..255.
    def fog_opacity(opacity)
      @fog[:opacity] = opacity.to_i.clamp(0, 255)
      apply_fog
      true
    end

    # XP 207: Animation an Spieler/Event. Ohne Animationsdatei ein Partikelburst.
    def show_animation(target, animation_key, event: nil)
      pos = target_position(target, event)
      return false unless pos
      x, y, z = pos
      if defined?(Effects)
        Effects.burst(x, y + 1.0, z, color: 0x7FE0FF, count: 20, speed: 2.6, life: 0.8)
      end
      Rune::Log.info("MapEffects: Animation '#{animation_key}' bei #{target}") if defined?(Rune::Log)
      true
    end

    # XP 208: Spieler durchsichtig.
    def transparent(flag)
      return false unless defined?($game_player) && $game_player
      $game_player.transparent = !!flag
      true
    end

    # XP 221: Übergangs-Key merken; execute_transition (222) blendet damit.
    def prepare_transition(key)
      @transition_key = key.to_s
      register_image('transition', @transition_key)
      true
    end

    # XP 131: Windowskin-Key. UI.message zeichnet ihn, sobald geladen.
    def windowskin(key)
      @windowskin_key = key.to_s
      register_image('windowskin', @windowskin_key)
      if defined?(MessageWindow)
        MessageWindow.instance_variable_set(:@skin_key, @windowskin_key)
      end
      true
    end

    def update(dt)
      return unless @scroll
      @scroll[:rest] = [@scroll[:rest] - dt, 0.0].max
      anteil = @scroll[:dauer].positive? ? (1.0 - @scroll[:rest] / @scroll[:dauer]) : 1.0
      x = @scroll[:von_x] + (@scroll[:ziel_x] - @scroll[:von_x]) * anteil
      z = @scroll[:von_z] + (@scroll[:ziel_z] - @scroll[:von_z]) * anteil
      set_camera_scroll(x, z)
      @scroll = nil if @scroll[:rest].zero?
    end

    # Hintergrund (Panorama) vor der Welt; Nebel-Overlay nach der Welt.
    def draw_background
      return if @panorama_key.empty?
      draw_fullscreen("panorama:#{@panorama_key}", 1.0)
    end

    def draw_fog
      return if @fog[:opacity].zero? && @fog_key.empty?
      w = Rune::Window.width
      h = Rune::Window.height
      alpha = @fog[:opacity] / 255.0
      if !@fog_key.empty?
        draw_fullscreen("fog:#{@fog_key}", alpha)
      elsif alpha.positive?
        Rune::Gfx.rect(0, 0, w, h, @fog[:color], alpha * 0.6)
      end
    end

    def draw_battleback
      return if @battleback_key.empty?
      draw_fullscreen("battleback:#{@battleback_key}", 1.0)
    end

    private

    def camera_scroll
      return [0.0, 0.0] unless defined?(Rune::Camera) && Rune::Camera.respond_to?(:scroll)
      Rune::Camera.scroll
    rescue StandardError
      [0.0, 0.0]
    end

    def set_camera_scroll(x, z)
      return unless defined?(Rune::Camera) && Rune::Camera.respond_to?(:scroll=)
      Rune::Camera.scroll = [x.to_f, z.to_f]
    rescue StandardError => e
      warn_once("MapEffects: Kamera-Scroll nicht verfügbar (#{e.message})")
    end

    def apply_fog
      return unless defined?(Rune::Gfx) && Rune::Gfx.respond_to?(:fog)
      on = @fog[:opacity].positive? || !@fog_key.empty?
      Rune::Gfx.fog(on, @fog[:color], 180.0)
    rescue StandardError => e
      warn_once("MapEffects: Nebel nicht verfügbar (#{e.message})")
    end

    def register_image(prefix, key)
      return if key.empty?
      return unless defined?(ImageLoader)
      name = "#{prefix}:#{key}"
      ImageLoader.register(name, key)
    rescue StandardError => e
      warn_once("MapEffects: Bild '#{key}' nicht ladbar (#{e.message})")
    end

    def draw_fullscreen(name, alpha)
      return unless defined?(Rune::Gfx) && Rune::Gfx.respond_to?(:picture)
      w = Rune::Window.width
      h = Rune::Window.height
      Rune::Gfx.picture(name, 0, 0, w, h, alpha)
    rescue StandardError
      nil
    end

    def target_position(target, event)
      case target.to_s
      when 'player'
        return nil unless defined?(Rune::Player)
        Rune::Player.position
      when 'this_event', 'event'
        return nil unless event.respond_to?(:box_center)
        event.box_center
      else nil
      end
    end

    def rgb(color)
      r = color['r'].to_i.clamp(0, 255)
      g = color['g'].to_i.clamp(0, 255)
      b = color['b'].to_i.clamp(0, 255)
      (r << 16) | (g << 8) | b
    end

    def warn_once(text)
      return if @gemeldet[text]
      @gemeldet[text] = true
      Rune::Log.warn(text) if defined?(Rune::Log)
    end
  end
end
