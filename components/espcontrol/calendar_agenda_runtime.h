#ifndef ESPCONTROL_CALENDAR_AGENDA_RUNTIME_H
#define ESPCONTROL_CALENDAR_AGENDA_RUNTIME_H

#pragma once

// Device-side calendar agenda fetcher. Calls Home Assistant's
// calendar.get_events for one or more calendar entities, accumulates the
// returned events into the host-tested espcontrol::AgendaList, and invokes a
// ready callback with the finalized, time-ordered, day-grouped list.
//
// The action-response JSON support (USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON)
// is enabled by the espcontrol component. When it is unavailable this degrades
// to a no-op so non-API builds still compile.

#include <functional>
#include <string>
#include <vector>

#include "calendar_agenda.h"
#include "button_grid_ha.h"

namespace espcontrol {

using AgendaReadyCallback = std::function<void(const AgendaList &)>;

class AgendaFetcher {
 public:
  void set_entities(std::vector<std::string> entities) {
    this->entities_ = std::move(entities);
  }
  void set_max_events(std::size_t max_events) { this->max_events_ = max_events; }
  void set_on_ready(AgendaReadyCallback callback) { this->on_ready_ = std::move(callback); }

  bool has_entities() const { return !this->entities_.empty(); }

  // Start a fetch covering [start_datetime, end_datetime] (Home Assistant local
  // "YYYY-MM-DD HH:MM:SS" strings). now_epoch filters out events that already
  // ended. A new refresh supersedes any in-flight one.
  void refresh(const std::string &start_datetime, const std::string &end_datetime,
               int64_t now_epoch) {
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
    if (this->entities_.empty()) {
      this->pending_.clear();
      if (this->on_ready_) this->on_ready_(this->accumulated_);
      return;
    }
    const uint32_t generation = ++this->generation_;
    this->accumulated_.clear();
    this->now_epoch_ = now_epoch;
    this->pending_ = static_cast<int>(this->entities_.size());

    for (const std::string &entity : this->entities_) {
      const uint32_t call_id = next_agenda_call_id_();
      esphome::api::HomeassistantActionRequest req;
      if (!ha_action_begin(req, "calendar.get_events", false, 3, call_id)) {
        this->finish_one_(generation);
        continue;
      }
      ha_action_add_entity(req, entity);
      ha_action_add_data(req, "start_date_time", start_datetime.c_str());
      ha_action_add_data(req, "end_date_time", end_datetime.c_str());

      auto *self = this;
      const bool registered = ha_register_action_response_callback(
          call_id,
          [self, generation](const esphome::api::ActionResponse &response) {
            self->handle_response_(generation, response);
          });
      if (!registered) {
        this->finish_one_(generation);
        continue;
      }
      if (!ha_action_send(req)) {
        ha_cancel_action_response_callback(call_id, "send failed");
        this->finish_one_(generation);
      }
    }
#else
    (void) start_datetime;
    (void) end_datetime;
    (void) now_epoch;
    if (this->on_ready_) this->on_ready_(this->accumulated_);
#endif
  }

 protected:
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  static uint32_t next_agenda_call_id_() {
    static uint32_t call_id = 600000;
    if (call_id == 0) call_id = 600000;
    return call_id++;
  }

  void handle_response_(uint32_t generation, const esphome::api::ActionResponse &response) {
    if (generation != this->generation_) return;  // superseded by a newer refresh
    if (response.is_success()) {
      // { "calendar.foo": { "events": [ {start, end, summary, location}, ... ] } }
      JsonObjectConst root = response.get_json();
      for (JsonPairConst calendar : root) {
        JsonArrayConst events = calendar.value()["events"];
        if (events.isNull()) continue;
        for (JsonObjectConst event : events) {
          const char *start = event["start"] | "";
          const char *summary = event["summary"] | "";
          this->accumulated_.add(start, std::string(summary));
        }
      }
    } else {
      ESP_LOGW("agenda", "calendar.get_events failed: %s",
               response.get_error_message().c_str());
    }
    this->finish_one_(generation);
  }

  void finish_one_(uint32_t generation) {
    if (generation != this->generation_) return;
    if (this->pending_ > 0) this->pending_--;
    if (this->pending_ > 0) return;
    this->accumulated_.finalize(this->max_events_, this->now_epoch_);
    if (this->on_ready_) this->on_ready_(this->accumulated_);
  }
#endif

  std::vector<std::string> entities_;
  std::size_t max_events_{kAgendaMaxEvents};
  AgendaReadyCallback on_ready_;
  AgendaList accumulated_;
  int pending_{0};
  uint32_t generation_{0};
  int64_t now_epoch_{0};
};

}  // namespace espcontrol

#endif  // ESPCONTROL_CALENDAR_AGENDA_RUNTIME_H
