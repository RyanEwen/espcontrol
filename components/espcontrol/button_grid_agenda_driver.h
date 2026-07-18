#ifndef ESPCONTROL_BUTTON_GRID_AGENDA_DRIVER_H
#define ESPCONTROL_BUTTON_GRID_AGENDA_DRIVER_H

#pragma once

// Lifecycle driver for the Agenda card: a read-only tile showing the next
// upcoming event from one or more Home Assistant calendars. Registration and
// rendering live in button_grid_agenda_cards.h; fetching goes through the
// shared calendar.get_events response-template path.
//
// Contract coverage marker: "agenda".

namespace espcontrol::cards {

inline bool agenda_driver_matches(const Context &context) {
  return !context.legacy_dispatch &&
         context.runtime.driver == card_runtime::CardDriverId::AGENDA;
}

inline bool agenda_driver_setup_visual(
    BtnSlot &slot, const ParsedCfg &config, const Context &context,
    const CardPalette &palette) {
  if (!agenda_driver_matches(context)) return false;

  if (palette.has_sensor_color) {
    lv_obj_set_style_bg_color(
      slot.btn, lv_color_hex(palette.sensor_val),
      static_cast<lv_style_selector_t>(LV_PART_MAIN) |
        static_cast<lv_style_selector_t>(LV_STATE_DEFAULT));
  }
  // The agenda draws its own event list over the whole tile; the standard
  // icon/sensor/name widgets stay hidden. Fonts are borrowed from the slot's
  // labels so device typography profiles apply unchanged: the name label's
  // font for event titles and the unit label's smaller font for times and
  // day headings.
  lv_obj_add_flag(slot.icon_lbl, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(slot.sensor_container, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(slot.text_lbl, LV_OBJ_FLAG_HIDDEN);
  const lv_font_t *title_font =
      lv_obj_get_style_text_font(slot.text_lbl, LV_PART_MAIN);
  const lv_font_t *small_font =
      lv_obj_get_style_text_font(slot.unit_lbl, LV_PART_MAIN);
  const uint32_t accent =
      palette.has_on ? palette.on_val : kAgendaAccentFallback;
  register_agenda_card(slot.btn, title_font, small_font, accent, config.entity,
                       config.label);
  return true;
}

inline bool agenda_driver_attach_interaction(
    BtnSlot &slot, const ParsedCfg &, const Context &context) {
  if (!agenda_driver_matches(context)) return false;
  lv_obj_clear_flag(slot.btn, LV_OBJ_FLAG_CLICKABLE);
  return true;
}

inline bool agenda_driver_bind_data(
    BtnSlot &, const ParsedCfg &, const Context &context) {
  // Fetching needs the panel clock, which YAML supplies to
  // agenda_cards_service(); registration during visual setup is enough here.
  return agenda_driver_matches(context);
}

inline bool agenda_driver_refresh_layout(
    BtnSlot &, const ParsedCfg &, const Context &context,
    const DisplayProfile &, int, int) {
  return agenda_driver_matches(context);
}

inline bool agenda_driver_cleanup(
    BtnSlot &, const ParsedCfg &, const Context &context) {
  // The agenda registry is reset before each grid rebuild; cards own no
  // dynamic allocations of their own.
  return agenda_driver_matches(context);
}

}  // namespace espcontrol::cards

#endif  // ESPCONTROL_BUTTON_GRID_AGENDA_DRIVER_H
