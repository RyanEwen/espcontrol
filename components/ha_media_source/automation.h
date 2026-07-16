#pragma once

#ifdef USE_ESP_IDF

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "ha_media_source.h"

namespace esphome {
namespace ha_media_source {

template<typename... Ts>
class SetConnectionAction : public Action<Ts...>, public Parented<HaMediaSource> {
 public:
  TEMPLATABLE_VALUE(std::string, url)
  TEMPLATABLE_VALUE(std::string, token)

  void play(Ts... x) override {
    this->parent_->set_connection(this->url_.value(x...), this->token_.value(x...));
  }
};

template<typename... Ts>
class SetFolderAction : public Action<Ts...>, public Parented<HaMediaSource> {
 public:
  TEMPLATABLE_VALUE(std::string, folder)

  void play(Ts... x) override { this->parent_->set_folder(this->folder_.value(x...)); }
};

template<typename... Ts>
class RefreshAction : public Action<Ts...>, public Parented<HaMediaSource> {
 public:
  void play(Ts... /*x*/) override { this->parent_->refresh_index(); }
};

}  // namespace ha_media_source
}  // namespace esphome

#endif  // USE_ESP_IDF
