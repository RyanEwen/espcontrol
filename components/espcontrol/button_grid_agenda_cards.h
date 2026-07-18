#ifndef ESPCONTROL_BUTTON_GRID_AGENDA_CARDS_H
#define ESPCONTROL_BUTTON_GRID_AGENDA_CARDS_H

#pragma once

// Runtime registry for Agenda cards. Each card holds a comma-separated list of
// calendar entities; a shared service tick (driven from YAML with the panel
// time) fetches upcoming events through calendar.get_events and renders the
// next event into the tile: its start ("9:00 AM", "Tomorrow") in the sensor
// label and its summary in the text label.

#include "calendar_agenda.h"
#include "calendar_agenda_runtime.h"

namespace espcontrol {

inline constexpr int kMaxAgendaCards = 6;
inline constexpr uint32_t kAgendaCardRefreshMs = 10 * 60 * 1000;

struct AgendaCardRef {
  lv_obj_t *sensor_lbl{nullptr};
  lv_obj_t *unit_lbl{nullptr};
  lv_obj_t *text_lbl{nullptr};
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

// Bumped on every grid rebuild so a late fetch callback can never touch
// widgets from a previous grid generation.
inline uint32_t &agenda_cards_generation() {
  static uint32_t generation = 1;
  return generation;
}

inline void reset_agenda_cards() {
  agenda_card_count() = 0;
  agenda_cards_generation()++;
}

inline void register_agenda_card(lv_obj_t *sensor_lbl, lv_obj_t *unit_lbl,
                                 lv_obj_t *text_lbl, const std::string &entities,
                                 const std::string &label) {
  int &count = agenda_card_count();
  if (count >= kMaxAgendaCards) {
    ESP_LOGW("agenda", "Too many agenda cards; skipping updates for extras");
    return;
  }
  AgendaCardRef &ref = agenda_card_refs()[count];
  ref = AgendaCardRef();
  ref.sensor_lbl = sensor_lbl;
  ref.unit_lbl = unit_lbl;
  ref.text_lbl = text_lbl;
  ref.entities = entities;
  ref.label = label;
  count++;
}

inline void agenda_card_apply(AgendaCardRef &ref, const AgendaList &list,
                              int32_t today_number, bool use_12h) {
  if (ref.sensor_lbl == nullptr || ref.text_lbl == nullptr) return;
  if (list.empty()) {
    lv_label_set_text(ref.sensor_lbl, "--");
    lv_label_set_text(ref.text_lbl,
                      ref.label.empty() ? espcontrol_i18n("No events")
                                        : ref.label.c_str());
    if (ref.unit_lbl != nullptr) lv_label_set_text(ref.unit_lbl, "");
    return;
  }
  const AgendaEntry &next = list.entries().front();
  // Sensor area: today's events show the time; later days show the day.
  char when[24];
  if (next.when.day_number == today_number && !next.when.all_day) {
    agenda_format_time(when, sizeof(when), next.when, use_12h);
  } else if (next.when.day_number == today_number) {
    std::snprintf(when, sizeof(when), "%s", espcontrol_i18n("Today"));
  } else {
    agenda_format_day_heading(when, sizeof(when), next.when.day_number,
                              today_number);
  }
  lv_label_set_text(ref.sensor_lbl, when);
  if (ref.unit_lbl != nullptr) lv_label_set_text(ref.unit_lbl, "");
  lv_label_set_text(ref.text_lbl, next.summary.c_str());
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
      agenda_card_apply(agenda_card_refs()[index], list, today, use_12h);
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
