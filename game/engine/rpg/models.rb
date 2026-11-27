#encoding: utf-8
# ============================================================================
#  Rpg::Models - unveränderliche Vorlagen aller 13 Datenbankbereiche.
#
#  Jede Klasse ist ein Data.define. Über Support::Immutable werden auch
#  verschachtelte Arrays/Hashes beim Anlegen eingefroren. Diese Objekte sind
#  reine Template-Daten: Sie kennen keine Spielszene, kein Rendering und keinen
#  Audioaufruf. Veränderliche Werte leben in Rpg::Runtime*.
# ============================================================================

module Rpg
  module Models
    # --- gemeinsame Unterstrukturen -----------------------------------------

    CatalogEntry = Data.define(:id, :name) do
      prepend Support::Immutable
    end

    Color = Data.define(:r, :g, :b) do
      prepend Support::Immutable
    end

    GraphicsConfig = Data.define(:windowskin_key, :title_key, :gameover_key,
                                 :battle_transition_key, :battleback_key) do
      prepend Support::Immutable
    end

    AudioConfig = Data.define(:title_bgm, :battle_bgm, :battle_end_me, :gameover_me,
                              :cursor_se, :decision_se, :cancel_se, :buzzer_se,
                              :equip_se, :shop_se, :save_se, :load_se,
                              :battle_start_se, :escape_se, :actor_collapse_se,
                              :enemy_collapse_se) do
      prepend Support::Immutable
    end

    Vocabulary = Data.define(:hp, :sp, :atk, :pdef, :mdef, :eva, :str, :dex, :agi,
                             :int, :exp, :gold, :level, :item, :skill, :weapon,
                             :armor, :equip, :status, :save, :load, :gameover) do
      prepend Support::Immutable
    end

    SystemData = Data.define(:magic_number, :initial_party, :start_map_id, :start_x,
                             :start_y, :graphics, :audio, :sound_cues, :vocabulary,
                             :asset_catalog, :element_ids, :species_ids,
                             :switch_ids, :variable_ids) do
      prepend Support::Immutable

      # Kataloge sind in Repositories gespeichert; hier stehen nur die IDs,
      # damit System trotzdem eine einzige Bezugsquelle bleibt.
      def element(id) = @element_ids.include?(id.to_i)
      def switch(id) = @switch_ids.include?(id.to_i)
      def variable(id) = @variable_ids.include?(id.to_i)
      def asset?(key) = @asset_catalog.key?(key.to_s)
    end

    # --- Actors / Classes ----------------------------------------------------

    GrowthCurve = Data.define(:base, :per_level) do
      prepend Support::Immutable
    end

    EquipmentRef = Data.define(:slot, :item_id, :fixed) do
      prepend Support::Immutable
    end

    Actor = Data.define(:id, :name, :class_id, :initial_level, :final_level,
                        :exp_basis, :exp_inflation, :character_key, :character_hue,
                        :battler_key, :battler_hue, :growth, :start_equipment) do
      prepend Support::Immutable
    end

    Learning = Data.define(:level, :skill_id) do
      prepend Support::Immutable
    end

    ClassData = Data.define(:id, :name, :position, :weapon_ids, :armor_ids,
                            :element_multipliers, :state_multipliers, :learnings) do
      prepend Support::Immutable
    end

    # --- Skills / Items -------------------------------------------------------

    Skill = Data.define(:id, :name, :icon_key, :description, :scope, :occasion,
                        :animation1_id, :animation2_id, :menu_se_key,
                        :common_event_id, :sp_cost, :power, :atk_f, :eva_f,
                        :str_f, :dex_f, :agi_f, :int_f, :hit, :pdef_f, :mdef_f,
                        :variance, :element_ids, :plus_state_ids, :minus_state_ids) do
      prepend Support::Immutable
    end

    Item = Data.define(:id, :name, :icon_key, :description, :scope, :occasion,
                       :animation1_id, :animation2_id, :menu_se_key,
                       :common_event_id, :price, :consumable, :hp_recovery_rate,
                       :sp_recovery_rate, :hp_recovery_value, :sp_recovery_value,
                       :param_inc, :element_ids, :plus_state_ids, :minus_state_ids) do
      prepend Support::Immutable
    end

    # --- Equipment ------------------------------------------------------------

    Weapon = Data.define(:id, :name, :icon_key, :description, :animation1_id,
                         :animation2_id, :price, :atk, :pdef, :mdef, :eva,
                         :str_plus, :dex_plus, :agi_plus, :int_plus,
                         :element_ids, :plus_state_ids) do
      prepend Support::Immutable
    end

    Armor = Data.define(:id, :name, :icon_key, :description, :kind, :auto_state_id,
                        :price, :pdef, :mdef, :eva, :str_plus, :dex_plus,
                        :agi_plus, :int_plus, :element_resistances,
                        :state_resistances) do
      prepend Support::Immutable
    end

    # --- Enemies / Troops -----------------------------------------------------

    EnemyAction = Data.define(:skill_id, :priority, :condition) do
      prepend Support::Immutable
    end

    EnemyTreasure = Data.define(:item_id, :weapon_id, :armor_id, :probability) do
      prepend Support::Immutable
    end

    Enemy = Data.define(:id, :name, :battler_key, :battler_hue, :maxhp, :maxsp,
                        :str, :dex, :agi, :int, :atk, :pdef, :mdef, :eva,
                        :animation1_id, :animation2_id, :element_multipliers,
                        :state_multipliers, :actions, :exp, :gold, :treasure) do
      prepend Support::Immutable
    end

    TroopMember = Data.define(:enemy_id, :x, :z) do
      prepend Support::Immutable
    end

    BattleEvent = Data.define(:id, :name, :condition, :switch_id, :commands) do
      prepend Support::Immutable
    end

    Troop = Data.define(:id, :name, :background_key, :members, :events) do
      prepend Support::Immutable
    end

    # --- States ---------------------------------------------------------------

    State = Data.define(:id, :name, :animation_id, :restriction, :nonresistance,
                        :zero_hp, :cant_get_exp, :cant_evade, :slip_damage,
                        :rating, :hit_rate, :rates, :battle_only, :hold_turn,
                        :auto_release_prob, :shock_release_prob,
                        :guard_element_ids, :plus_state_ids, :minus_state_ids) do
      prepend Support::Immutable
    end

    # --- Animations -----------------------------------------------------------

    AnimationFrame = Data.define(:cell_x, :cell_y, :width, :height) do
      prepend Support::Immutable
    end

    AnimationTiming = Data.define(:frame, :se_key, :flash_target, :color,
                                  :duration, :condition) do
      prepend Support::Immutable
    end

    Animation = Data.define(:id, :name, :sheet_key, :position, :frame_max,
                            :frames, :timings) do
      prepend Support::Immutable
    end

    # --- Tilesets -------------------------------------------------------------

    TileProperty = Data.define(:index, :passable, :passable_4dirs, :priority,
                               :bush, :counter, :terrain_tag) do
      prepend Support::Immutable
    end

    Tileset = Data.define(:id, :name, :tileset_key, :autotile_keys,
                          :panorama_key, :fog_key, :battleback_key,
                          :fog_opacity, :fog_blend_type, :fog_zoom, :fog_sx,
                          :fog_sy, :tile_properties) do
      prepend Support::Immutable
    end

    # --- Common Events --------------------------------------------------------

    EventCommand = Data.define(:code, :params) do
      prepend Support::Immutable
    end

    CommonEvent = Data.define(:id, :name, :trigger, :condition_switch_id,
                              :commands) do
      prepend Support::Immutable
    end
  end
end
