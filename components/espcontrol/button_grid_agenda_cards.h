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

// Per-calendar accent colors, indexed by the entity's position in the card's
// calendar list (Calendar-Card-Pro-style attribution).
inline constexpr uint32_t kAgendaCalendarColors[] = {
    0x66BB6A,  // green
    0xEF5350,  // red
    0x42A5F5,  // blue
    0xAB47BC,  // purple
    0xFFB300,  // amber
    0x26C6DA,  // cyan
};

inline uint32_t agenda_source_color(uint8_t source, uint32_t /*unused*/) {
  return kAgendaCalendarColors[source % (sizeof(kAgendaCalendarColors) /
                                         sizeof(kAgendaCalendarColors[0]))];
}

struct AgendaCardRef {
  lv_obj_t *list{nullptr};
  const lv_font_t *title_font{nullptr};
  const lv_font_t *small_font{nullptr};
  const lv_font_t *big_font{nullptr};
  uint32_t accent{0};
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

// The panel clock as of the last service tick, so a card registered during a
// grid rebuild can render its fetcher's cached events immediately instead of
// sitting blank until the next tick's fetch answers.
inline int32_t &agenda_service_today() {
  static int32_t today = 0;
  return today;
}
inline bool &agenda_service_use_12h() {
  static bool use_12h = false;
  return use_12h;
}
inline bool &agenda_service_clock_seen() {
  static bool seen = false;
  return seen;
}

inline void agenda_card_render(AgendaCardRef &ref, const AgendaList &list,
                               int32_t today_number, bool use_12h);

// Build the list container that fills the card and register it for updates.
// Fonts are borrowed from the slot's own labels so every device profile keeps
// its typography without new font roles.
inline void register_agenda_card(lv_obj_t *btn, const lv_font_t *title_font,
                                 const lv_font_t *small_font,
                                 const lv_font_t *big_font, uint32_t accent,
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
  lv_obj_set_style_pad_row(list, 10, 0);
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
  ref.big_font = big_font;
  ref.accent = accent;
  ref.entities = entities;
  ref.label = label;
  const AgendaList &cached = agenda_card_fetchers()[count].last_result();
  if (agenda_service_clock_seen() && !cached.empty()) {
    agenda_card_render(ref, cached, agenda_service_today(),
                       agenda_service_use_12h());
  }
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

// ── Shared row building blocks ───────────────────────────────────────────
// The card and the screensaver overlay render the same Calendar-Card-Pro
// anatomy: a day row holding a date column (weekday / big day number / MONTH)
// beside a column of accent-tinted event boxes.

// Height of the weekday/day/month date stack; event boxes for single-event
// days match it exactly. Must mirror the label pinning in agenda_day_row_.
inline int agenda_date_stack_height_(const lv_font_t *small_font,
                                     const lv_font_t *big_font) {
  const int small_lh = small_font ? lv_font_get_line_height(small_font) : 12;
  const int big_lh = big_font ? lv_font_get_line_height(big_font) : 24;
  return 2 * (small_lh - small_lh / 5) + (big_lh - big_lh / 6);
}

inline const char *agenda_weekday_name_(int32_t day_number) {
  static const char *const kWeekdays[] = {"Sun", "Mon", "Tue", "Wed",
                                          "Thu", "Fri", "Sat"};
  return kWeekdays[agenda_weekday(day_number)];
}

// Creates a day row with its date column and returns the events column that
// event boxes for this day should be added to.
inline lv_obj_t *agenda_day_row_(lv_obj_t *parent, int32_t day_number,
                                 int date_col_w, const lv_font_t *small_font,
                                 const lv_font_t *big_font,
                                 int *date_col_height = nullptr) {
  static const char *const kMonths[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

  lv_obj_t *day_row = lv_obj_create(parent);
  lv_obj_set_size(day_row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(day_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(day_row, 0, 0);
  lv_obj_set_style_pad_all(day_row, 0, 0);
  lv_obj_set_style_pad_column(day_row, 6, 0);
  lv_obj_clear_flag(day_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(day_row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_flex_flow(day_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(day_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);

  int year = 0, month = 1, day = 1;
  agenda_civil_from_days(day_number, &year, &month, &day);
  lv_obj_t *date_col = lv_obj_create(day_row);
  lv_obj_set_size(date_col, date_col_w, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(date_col, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(date_col, 0, 0);
  lv_obj_set_style_pad_all(date_col, 0, 0);
  lv_obj_set_style_pad_row(date_col, 0, 0);
  lv_obj_clear_flag(date_col, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(date_col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(date_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  // The date stack reads as one unit in the reference: everything centered,
  // and each line pinned below its font's full line height to shed the
  // leading (safe: weekday/day/month glyphs have no descenders).
  const int small_lh = small_font ? lv_font_get_line_height(small_font) : 12;
  const int big_lh = big_font ? lv_font_get_line_height(big_font) : 24;
  char text[8];
  std::snprintf(text, sizeof(text), "%s", agenda_weekday_name_(day_number));
  lv_obj_t *weekday = agenda_row_label_(date_col, text, small_font, 0xFFFFFF,
                                        LV_OPA_60);
  lv_obj_set_style_text_align(weekday, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_height(weekday, small_lh - small_lh / 5);
  std::snprintf(text, sizeof(text), "%d", day);
  lv_obj_t *num = agenda_row_label_(date_col, text, big_font, 0xFFFFFF,
                                    LV_OPA_COVER);
  lv_obj_set_style_text_align(num, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_height(num, big_lh - big_lh / 6);
  std::snprintf(text, sizeof(text), "%s", kMonths[(month - 1) % 12]);
  lv_obj_t *month_lbl = agenda_row_label_(date_col, text, small_font, 0xFFFFFF,
                                          LV_OPA_60);
  lv_obj_set_style_text_align(month_lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_height(month_lbl, small_lh - small_lh / 5);

  lv_obj_t *events_col = lv_obj_create(day_row);
  lv_obj_set_size(events_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(events_col, 1);
  lv_obj_set_style_bg_opa(events_col, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(events_col, 0, 0);
  lv_obj_set_style_pad_all(events_col, 0, 0);
  lv_obj_set_style_pad_row(events_col, 4, 0);
  lv_obj_clear_flag(events_col, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(events_col, LV_FLEX_FLOW_COLUMN);
  if (date_col_height != nullptr) {
    // Measure the laid-out date stack so event boxes can match it exactly
    // instead of trusting font arithmetic.
    lv_obj_update_layout(date_col);
    *date_col_height = lv_obj_get_height(date_col);
    if (*date_col_height <= 0)
      *date_col_height = agenda_date_stack_height_(small_font, big_font);
  }
  return events_col;
}

// Event box: accent-tinted background with a solid accent bar on its left
// edge, single-line ellipsized title with the "in N days" marker right-aligned
// beside it, the time range, and the location when present.
inline void agenda_event_box_(lv_obj_t *events_col, const AgendaEntry &entry,
                              int32_t today_number, bool use_12h,
                              uint32_t accent, const lv_font_t *title_font,
                              const lv_font_t *small_font,
                              int min_height = 0) {
  const bool has_location = !entry.location.empty();
  const int title_h = title_font ? lv_font_get_line_height(title_font) : 16;
  const uint32_t color = agenda_source_color(entry.source, accent);
  lv_obj_t *box = lv_obj_create(events_col);
  lv_obj_set_size(box, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(box, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_20, 0);
  lv_obj_set_style_radius(box, 5, 0);
  lv_obj_set_style_border_width(box, 0, 0);
  lv_obj_set_style_pad_all(box, 6, 0);
  lv_obj_set_style_pad_top(box, 9, 0);
  lv_obj_set_style_pad_bottom(box, 9, 0);
  lv_obj_set_style_pad_left(box, 11, 0);
  lv_obj_set_style_pad_row(box, 2, 0);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  // A day's lone event box is stretched to the date stack's height (text
  // vertically centered); days with several events keep natural heights.
  if (min_height > 0) {
    lv_obj_set_style_min_height(box, min_height, 0);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
  }

  lv_obj_t *bar = lv_obj_create(box);
  lv_obj_set_size(bar, 3, lv_pct(100));
  lv_obj_add_flag(bar, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_align(bar, LV_ALIGN_LEFT_MID, -8, 0);
  lv_obj_set_style_bg_color(bar, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_radius(bar, 2, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title_row = lv_obj_create(box);
  lv_obj_set_size(title_row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(title_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(title_row, 0, 0);
  lv_obj_set_style_pad_all(title_row, 0, 0);
  lv_obj_set_style_pad_column(title_row, 4, 0);
  lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *title = lv_label_create(title_row);
  lv_label_set_text(title, entry.summary.c_str());
  if (title_font != nullptr)
    lv_obj_set_style_text_font(title, title_font, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_flex_grow(title, 1);
  // A single ellipsized line, like the reference: LONG_DOT only truncates
  // once the height is pinned to one text line.
  lv_obj_set_height(title, title_h);
  lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);

  char rel[16];
  agenda_format_relative(rel, sizeof(rel), entry.when.day_number, today_number);
  if (rel[0] != '\0') {
    lv_obj_t *marker = lv_label_create(title_row);
    lv_label_set_text(marker, rel);
    if (small_font != nullptr)
      lv_obj_set_style_text_font(marker, small_font, 0);
    lv_obj_set_style_text_color(marker, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_opa(marker, LV_OPA_50, 0);
  }

  char range[48];
  agenda_format_range(range, sizeof(range), entry, use_12h, today_number);
  agenda_row_label_(box, range, small_font, 0xFFFFFF, LV_OPA_60);
  if (has_location) {
    agenda_row_label_(box, entry.location.c_str(), small_font, 0xFFFFFF,
                      LV_OPA_40);
  }
}

// Render the agenda into the card's list container: one row per day with a
// date column beside accent-tinted event boxes — the Calendar Card Pro layout.
inline void agenda_card_render(AgendaCardRef &ref, const AgendaList &list,
                               int32_t today_number, bool use_12h) {
  if (ref.list == nullptr) return;
  lv_obj_clean(ref.list);

  if (list.empty()) {
    agenda_row_label_(ref.list, espcontrol_i18n("No upcoming events"),
                      ref.small_font, 0xFFFFFF, LV_OPA_60);
    return;
  }

  lv_obj_update_layout(ref.list);
  const int available = lv_obj_get_content_height(ref.list);
  const int title_h = ref.title_font ? lv_font_get_line_height(ref.title_font) : 16;
  const int small_h = ref.small_font ? lv_font_get_line_height(ref.small_font) : 12;
  const int big_h = ref.big_font ? lv_font_get_line_height(ref.big_font) : 24;
  int date_col_w = big_h + big_h / 4;   // fits two digits of the day number
  if (date_col_w < 40) date_col_w = 40;
  int used = 0;

  const std::vector<AgendaEntry> &entries = list.entries();
  lv_obj_t *events_col = nullptr;
  int date_stack_h = 0;
  for (std::size_t i = 0; i < entries.size(); i++) {
    const AgendaEntry &entry = entries[i];
    const bool has_location = !entry.location.empty();
    const int event_h = title_h + small_h + (has_location ? small_h : 0) + 10;
    const int day_min_h = small_h * 2 + big_h;
    const int needed = list.starts_new_day(i)
                           ? (event_h > day_min_h ? event_h : day_min_h) + 6
                           : event_h;
    if (used + needed > available && used > 0) break;
    used += needed;

    if (list.starts_new_day(i)) {
      events_col = agenda_day_row_(ref.list, entry.when.day_number, date_col_w,
                                   ref.small_font, ref.big_font, &date_stack_h);
    }
    if (events_col == nullptr) continue;
    const bool sole_event_of_day =
        list.starts_new_day(i) &&
        (i + 1 >= entries.size() || list.starts_new_day(i + 1));
    agenda_event_box_(events_col, entry, today_number, use_12h, ref.accent,
                      ref.title_font, ref.small_font,
                      sole_event_of_day ? date_stack_h : 0);
  }
}

// ── Screensaver overlay rendering ────────────────────────────────────────
// A compact agenda for the photo screensaver: the next event, the current
// day's events, or a short upcoming list — rendered with the same day-row and
// tinted-event-box anatomy as the grid card, into a caller-owned box whose
// background (and its opacity) the YAML controls. Returns false when there is
// nothing to show so the caller can hide the box entirely.

enum class AgendaOverlayStyle : uint8_t { NEXT_EVENT, TODAY, UPCOMING };

inline bool agenda_overlay_render(lv_obj_t *box, const AgendaList &list,
                                  AgendaOverlayStyle style, int32_t today_number,
                                  bool use_12h, const lv_font_t *title_font,
                                  const lv_font_t *small_font,
                                  const lv_font_t *big_font) {
  if (box == nullptr) return false;
  lv_obj_clean(box);
  if (list.empty()) return false;
  const std::vector<AgendaEntry> &entries = list.entries();

  const int big_h = big_font ? lv_font_get_line_height(big_font) : 24;
  int date_col_w = big_h + big_h / 4;
  if (date_col_w < 40) date_col_w = 40;

  std::vector<const AgendaEntry *> picked;
  if (style == AgendaOverlayStyle::NEXT_EVENT) {
    picked.push_back(&entries.front());
  } else if (style == AgendaOverlayStyle::TODAY) {
    for (const AgendaEntry &entry : entries) {
      if (entry.when.day_number != today_number) continue;
      picked.push_back(&entry);
      if (picked.size() >= 4) break;
    }
    // Nothing left today: fall back to the next event so the overlay stays
    // useful instead of disappearing.
    if (picked.empty()) picked.push_back(&entries.front());
  } else {
    for (const AgendaEntry &entry : entries) {
      picked.push_back(&entry);
      if (picked.size() >= 5) break;
    }
  }

  int32_t prev_day = INT32_MIN;
  lv_obj_t *events_col = nullptr;
  int date_stack_h = 0;
  for (std::size_t i = 0; i < picked.size(); i++) {
    const AgendaEntry *entry = picked[i];
    if (entry->when.day_number != prev_day || events_col == nullptr) {
      prev_day = entry->when.day_number;
      events_col = agenda_day_row_(box, entry->when.day_number, date_col_w,
                                   small_font, big_font, &date_stack_h);
    }
    const bool sole_event_of_day =
        (i + 1 >= picked.size() ||
         picked[i + 1]->when.day_number != entry->when.day_number) &&
        (i == 0 || picked[i - 1]->when.day_number != entry->when.day_number);
    agenda_event_box_(events_col, *entry, today_number, use_12h, 0, title_font,
                      small_font, sole_event_of_day ? date_stack_h : 0);
  }
  return true;
}

// Periodic service driven from YAML with the panel clock. Fetches each card's
// calendars on first sight and every kAgendaCardRefreshMs afterwards.
inline void agenda_cards_service(int year, int month, int day, int hour,
                                 int minute, int second, bool use_12h) {
  const uint32_t now_ms = esphome::millis();
  const int32_t today = agenda_days_from_civil(year, month, day);
  agenda_service_today() = today;
  agenda_service_use_12h() = use_12h;
  agenda_service_clock_seen() = true;
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
