#encoding: utf-8
# ============================================================================
#  Shop (Etappe 20e, RPG Maker XP 302 "Shop-Processing").
#
#  XP trennt den Befehl (Game_Temp#shop_goods) vom Fenster (Scene_Shop). Hier
#  macht das Modul die Rechenarbeit, das Fenster zeichnet nur:
#
#    Shop.start([{ 'item' => 'heiltrank', 'price' => 20 }])
#    Shop.active?          # Fenster offen? (Interpreter blockiert solange)
#    Shop.kaufen('heiltrank')
#    Shop.verkaufen('heiltrank')
#    Shop.close
#
#  Preise: `price` im Befehl ist der Ladenpreis und sticht den Datenbankpreis
#  (XP: "Preis" je Ware im Befehl). Verkauf bringt den halben Datenbankpreis
#  (XP: Game_Party#sell_price = price / 2), nie mehr als den Ladenpreis - sonst
#  wäre "kaufen und gleich verkaufen" ein Gold-Generator.
#
#  Der Shop ist absichtlich unbegrenzt (wie XP ohne Lagermenge): Bestand führen
#  wir erst, wenn ein Projekt es braucht (siehe Checkliste 20e).
# ============================================================================

module Shop
  MODI = %i[kaufen verkaufen].freeze

  @aktiv = false
  @waren = []       # [{ id:, name:, preis: }] - normalisiert
  @meldung = ''     # letzte Rückmeldung fürs Fenster ("Nicht genug Gold.")
  @modus = :kaufen  # Fenster-Reiter: Warenliste oder Inventar
  @sel = 0          # Cursor im aktuellen Reiter

  class << self
    attr_reader :waren, :meldung, :modus, :sel

    def active? = @aktiv

    # Warenliste normalisieren; unbekannte Items fliegen raus und landen im
    # Issue-Backlog. Eine leere Liste öffnet keinen Shop (XP zeigt dann ein
    # leeres Fenster - hier wäre das nur ein Hänger).
    def start(waren)
      liste = Array(waren).filter_map { |ware| warenung(ware) }
      if liste.empty?
        Rune::Log.warn('Shop: keine gültige Ware im Befehl')
        report('Shop: Befehl ohne gültige Ware')
        @aktiv = false
        @waren = []
        return false
      end
      @waren = liste
      @aktiv = true
      @meldung = ''
      @modus = :kaufen
      @sel = 0
      Rune::Log.info("Shop: #{liste.size} Ware(n) - " +
                     liste.map { |w| "#{w[:name]} #{w[:preis]}" }.join(', '))
      true
    end

    def close
      @aktiv = false
      @waren = []
      @meldung = ''
      @sel = 0
    end

    # --- Fenster-Bedienung (headless prüfbar) --------------------------------
    # Die UI ruft nur diese Methoden; die Rechenarbeit oben bleibt gleich.
    def liste
      @modus == :verkaufen ? verkaufsliste : @waren
    end

    # Inventar als Verkaufsliste (XP: Window_ShopSell zeigt alle Items).
    def verkaufsliste
      Party.items.keys.sort.filter_map do |id|
        next unless Party.item_def(id)
        { id: id, name: Party.item_name(id), preis: verkaufspreis(id),
          anzahl: Party.item_count(id) }
      end
    end

    def wechsle_modus!
      @modus = @modus == :kaufen ? :verkaufen : :kaufen
      @sel = 0
      @meldung = ''
      @modus
    end

    def hoch
      n = liste.size
      return @sel = 0 if n.zero?
      @sel = (@sel - 1) % n
    end

    def runter
      n = liste.size
      return @sel = 0 if n.zero?
      @sel = (@sel + 1) % n
    end

    # E/Enter: die markierte Ware kaufen bzw. verkaufen.
    def bestaetigen
      ware = liste[@sel]
      if ware.nil?
        @meldung = 'Keine Ware ausgewaehlt.'
        return nil
      end
      @modus == :kaufen ? kaufen(ware[:id]) : verkaufen(ware[:id])
    end

    # Esc: erst zurück zu den Waren, dann den Laden verlassen.
    # Rückgabe true = Fenster schließen.
    def zurueck
      if @modus == :verkaufen
        @modus = :kaufen
        @sel = 0
        @meldung = ''
        false
      else
        close
        true
      end
    end

    # Preis, den ein Item im Laden kostet (Befehlspreis, sonst Datenbank).
    def kaufpreis(item)
      id = item.to_s
      ware = @waren.find { |w| w[:id] == id }
      return ware[:preis] if ware

      datenbankpreis(id)
    end

    # XP: Game_Party#sell_price (halber Datenbankpreis). Der Ladenpreis deckelt,
    # damit ein teurer Laden nicht zum Gold-Generator beim Wiederverkauf wird.
    def verkaufspreis(item)
      id = item.to_s
      preis = datenbankpreis(id) / 2
      ware = @waren.find { |w| w[:id] == id }
      preis = [preis, ware[:preis] / 2].min if ware
      preis
    end

    # Kauft `anzahl` Stück. Rückgabe: :ok, :unbekannt (kein Item), :kein_gold,
    # :menge (anzahl < 1).
    def kaufen(item, anzahl = 1)
      id = item.to_s
      n = anzahl.to_i
      return :menge if n < 1
      return :unbekannt if Party.item_def(id).nil?

      kosten = kaufpreis(id) * n
      if Party.gold < kosten
        @meldung = "Nicht genug Gold (#{kosten} noetig)."
        Rune::Log.warn("Shop: Kauf von #{id} scheitert - #{kosten} Gold fehlen")
        return :kein_gold
      end
      Party.add_gold(-kosten)
      Party.add_item(id, n)
      @meldung = "#{Party.item_name(id)} gekauft (#{kosten} Gold)."
      Rune::Log.info("Shop: #{n}x #{id} gekauft (#{kosten} Gold)")
      :ok
    end

    # Verkauft `anzahl` Stück aus dem Inventar. Rückgabe: :ok, :unbekannt,
    # :kein_item (nicht im Inventar), :menge.
    def verkaufen(item, anzahl = 1)
      id = item.to_s
      n = anzahl.to_i
      return :menge if n < 1
      return :unbekannt if Party.item_def(id).nil?

      if Party.item_count(id) < n
        @meldung = "Nicht genug im Inventar (#{id})."
        Rune::Log.warn("Shop: Verkauf von #{id} scheitert - nur #{Party.item_count(id)} im Inventar")
        return :kein_item
      end
      Party.take_item(id, n)
      erloes = verkaufspreis(id) * n
      Party.add_gold(erloes)
      @meldung = "#{Party.item_name(id)} verkauft (+#{erloes} Gold)."
      Rune::Log.info("Shop: #{n}x #{id} verkauft (#{erloes} Gold)")
      :ok
    end

    private

    def warenung(ware)
      return nil unless ware.is_a?(Hash)

      id = ware['item'].to_s.strip
      return nil if id.empty?

      defi = Party.item_def(id)
      if defi.nil?
        Rune::Log.warn("Shop: Item '#{id}' nicht in database.json")
        report("Shop: Item '#{id}' nicht in database.json")
        return nil
      end
      { id: id, name: Party.item_name(id), preis: preis_aus(ware, id) }
    end

    def preis_aus(ware, id)
      wert = ware['price']
      return wert.to_i if wert.is_a?(Numeric)
      return wert.to_i unless wert.to_s.strip.empty?

      datenbankpreis(id)
    end

    def datenbankpreis(id)
      defi = Party.item_def(id)
      preis = defi && defi['price']
      return preis.to_i if preis.is_a?(Numeric)

      preis.to_s.strip.empty? ? 0 : preis.to_i
    end

    def report(text)
      Issues.report('Shop', text) if defined?(Issues) && Issues.respond_to?(:report)
    end
  end
end
