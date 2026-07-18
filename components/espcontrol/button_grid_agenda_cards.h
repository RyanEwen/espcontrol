#ifndef ESPCONTROL_BUTTON_GRID_AGENDA_CARDS_H
#define ESPCONTROL_BUTTON_GRID_AGENDA_CARDS_H

#pragma once

// Runtime registry and renderer for Agenda cards. Each card holds a
// comma-separated list of calendar entities; a shared service tick (driven
// from YAML with the panel time) fetches upcoming events through
// calendar.get_events and renders them as a Calendar-Card-Pro-style list:
// dimmed day headings, then one row per event with a colored accent bar, the
// event title, and its time underneath. Rows beyond the tile clip.

#include "calendar_agenda.h"
#include "calendar_agenda_runtime.h"

namespace espcontrol {

inline constexpr int kMaxAgendaCards = 6;
inline constexpr uint32_t kAgendaCardRefreshMs = 10 * 60 * 1000;
inline constexpr uint32_t kAgendaAccentFallback = 0x4FC3F7;
inline constexpr int kAgendaCardMaxRows = 4;

struct AgendaCardRef {
  lv_obj_t *list{nullptr};
  const lv_font_t *title_font{nullptr};
  const lv_font_t *small_font{nullptr};
  uint32_t accent{kAgendaAccentFallback};
  std::string entities;
  std::string label;
  uint32_t last_fetch_ms{0};
  bool fetched_once{false};
};

inline AgendaCardRef *agenda_card_refs() {
  static AgendaCardRef refs[kMaxAgendaCards];
  return refs;
}

inline int &agenda_card_count() {
  static int count = 0;
  return count;
}

inline AgendaFetcher *agenda_card_fetchers() {
  static AgendaFetcher fetchers[kMaxAgendaCards];
  return fetchers;
}

// Bumped on every grid rebuild and subpage teardown so a late fetch callback
// can never touch widgets from a previous grid generation.
inline uint32_t &agenda_cards_generation() {
  static uint32_t generation = 1;
  return generation;
}

inline void reset_agenda_cards() {
  agenda_card_count() = 0;
  agenda_cards_generation()++;
}

// Build the list container that fills the card and register it for updates.
// Fonts are borrowed from the slot's own labels so every device profile keeps
// its typography without new font roles.
inline void register_agenda_card(lv_obj_t *btn, const lv_font_t *title_font,
                                 const lv_font_t *small_font, uint32_t accent,
                                 const std::string &entities,
                                 const std::string &label) {
  int &count = agenda_card_count();
  if (count >= kMaxAgendaCards) {
    ESP_LOGW("agenda", "Too many agenda cards; skipping updates for extras");
    return;
  }

  lv_obj_t *list = lv_obj_create(btn);
  lv_obj_set_size(list, lv_pct(100), lv_pct(100));
  lv_obj_align(list, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 6, 0);
  lv_obj_set_style_pad_row(list, 4, 0);
  lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(list, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);

  AgendaCardRef &ref = agenda_card_refs()[count];
  ref = AgendaCardRef();
  ref.list = list;
  ref.title_font = title_font;
  ref.small_font = small_font;
  ref.accent = accent;
  ref.entities = entities;
  ref.label = label;
  count++;
}

inline lv_obj_t *agenda_row_label_(lv_obj_t *parent, const char *text,
                                   const lv_font_t *font, uint32_t color,
                                   lv_opa_t opa) {
  lv_obj_t *lbl = lv_label_create(parent);
  lv_label_set_text(lbl, text);
  if (font != nullptr) lv_obj_set_style_text_font(lbl, font, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
  lv_obj_set_style_text_opa(lbl, opa, 0);
  lv_obj_set_width(lbl, lv_pct(100));
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
  return lbl;
}

// Render the agenda into the card's list container.
inline void agenda_card_render(AgendaCardRef &ref, const AgendaList &list,
                               int32_t today_number, bool use_12h) {
  if (ref.list == nullptr) return;
  lv_obj_clean(ref.list);

  if (list.empty()) {
    agenda_row_label_(ref.list, espcontrol_i18n("No upcoming events"),
                      ref.small_font, 0xFFFFFF, LV_OPA_60);
    return;
  }

  int rows = 0;
  const std::vector<AgendaEntry> &entries = list.entries();
  for (std::size_t i = 0; i < entries.size() && rows < kAgendaCardMaxRows; i++) {
    if (list.starts_new_day(i)) {
      char heading[24];
      agenda_format_day_heading(heading, sizeof(heading),
                                entries[i].when.day_number, today_number);
      lv_obj_t *head = agenda_row_label_(ref.list, heading, ref.small_font,
                                         0xFFFFFF, LV_OPA_50);
      lv_obj_set_style_pad_top(head, i == 0 ? 0 : 2, 0);
      rows++;
      if (rows >= kAgendaCardMaxRows) break;
    }

    // Event row: accent bar on the left, then title with the time beneath.
    lv_obj_t *row = lv_obj_create(ref.list);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, 6, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_STRETCH,
                          LV_FLEX_ALIGN_START);

    lv_obj_t *bar = lv_obj_create(row);
    lv_obj_set_size(bar, 3, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(bar, lv_color_hex(ref.accent), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 2, 0);
    lv_obj_set_style_min_height(bar, 14, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *body = lv_obj_create(row);
    lv_obj_set_size(body, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_set_style_pad_row(body, 0, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);

    agenda_row_label_(body, entries[i].summary.c_str(), ref.title_font,
                      0xFFFFFF, LV_OPA_COVER);
    char time_buf[16];
    agenda_format_time(time_buf, sizeof(time_buf), entries[i].when, use_12h);
    if (time_buf[0] != '\0') {
      agenda_row_label_(body, time_buf, ref.small_font, 0xFFFFFF, LV_OPA_60);
    }
    rows++;
  }
}

// Periodic service driven from YAML with the panel clock. Fetches each card's
// calendars on first sight and every kAgendaCardRefreshMs afterwards.
inline void agenda_cards_service(int year, int month, int day, int hour,
                                 int minute, int second, bool use_12h) {
  const uint32_t now_ms = esphome::millis();
  const int32_t today = agenda_days_from_civil(year, month, day);
  for (int i = 0; i < agenda_card_count(); i++) {
    AgendaCardRef &ref = agenda_card_refs()[i];
    if (ref.entities.empty()) continue;
    if (ref.fetched_once && now_ms - ref.last_fetch_ms < kAgendaCardRefreshMs) {
      continue;
    }
    ref.fetched_once = true;
    ref.last_fetch_ms = now_ms;

    AgendaFetcher &fetcher = agenda_card_fetchers()[i];
    fetcher.set_entities(agenda_split_entities(ref.entities));
    fetcher.set_max_events(kAgendaMaxEvents);
    const uint32_t generation = agenda_cards_generation();
    const int index = i;
    fetcher.set_on_ready([index, generation, today, use_12h](const AgendaList &list) {
      if (generation != agenda_cards_generation()) return;  // grid rebuilt
      if (index >= agenda_card_count()) return;
      AgendaCardRef &current = agenda_card_refs()[index];
      // Belt and braces: never render into a widget LVGL no longer tracks.
      if (current.list == nullptr || !lv_obj_is_valid(current.list)) return;
      agenda_card_render(current, list, today, use_12h);
    });

    char start[24];
    std::snprintf(start, sizeof(start), "%04d-%02d-%02d %02d:%02d:%02d", year,
                  month, day, hour, minute, second);
    int ey = 0, em = 0, ed = 0;
    agenda_civil_from_days(today + 14, &ey, &em, &ed);
    char end[24];
    std::snprintf(end, sizeof(end), "%04d-%02d-%02d 00:00:00", ey, em, ed);
    const int64_t now_epoch = static_cast<int64_t>(today) * 86400 +
                              hour * 3600 + minute * 60 + second;
    fetcher.refresh(std::string(start), std::string(end), now_epoch);
  }
}

}  // namespace espcontrol

#endif  // ESPCONTROL_BUTTON_GRID_AGENDA_CARDS_H
