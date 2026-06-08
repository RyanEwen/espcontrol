#pragma once

// Internal implementation detail for button_grid.h. Include button_grid.h from device YAML.

#include "esphome/core/version.h"

struct ImageCardCtx {
  lv_obj_t *widget = nullptr;
  lv_obj_t *btn = nullptr;
  esphome::artwork_image::ArtworkImage *image = nullptr;
  std::string entity_id;
  std::string source_url;
  std::string url;
  uint32_t refresh_interval_ms = 0;
  uint32_t next_refresh_ms = 0;
  bool active = false;
  bool callbacks_bound = false;
  bool requested_once = false;
  bool timer_only = false;
};

inline ImageCardCtx *image_card_contexts() {
  static ImageCardCtx contexts[4];
  return contexts;
}

inline void image_card_hide(ImageCardCtx *ctx) {
  if (!ctx) return;
  if (ctx->widget) lv_obj_add_flag(ctx->widget, LV_OBJ_FLAG_HIDDEN);
}

inline void image_card_apply_downloaded(ImageCardCtx *ctx) {
  if (!ctx || !ctx->active || !ctx->widget || !ctx->image) return;
  if (ctx->image->get_url() != ctx->url) return;
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 4, 0)
  lv_image_set_src(ctx->widget, ctx->image->get_lv_image_dsc());
#else
  lv_img_set_src(ctx->widget, ctx->image->get_lv_img_dsc());
#endif
  lv_obj_clear_flag(ctx->widget, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_background(ctx->widget);
  lv_obj_invalidate(ctx->widget);
  if (ctx->btn) lv_obj_invalidate(ctx->btn);
  notify_dashboard_content_changed();
}

inline void image_card_bind_callbacks(ImageCardCtx *ctx) {
  if (!ctx || !ctx->image || ctx->callbacks_bound) return;
  ctx->callbacks_bound = true;
  ctx->image->add_on_finished_callback([ctx](bool) {
    image_card_apply_downloaded(ctx);
  });
  ctx->image->add_on_error_callback([ctx]() {
    ESP_LOGW("image_card", "Image download failed for %s", ctx->entity_id.c_str());
    image_card_hide(ctx);
  });
}

inline void reset_image_card_pool(const GridConfig &cfg) {
  ImageCardCtx *contexts = image_card_contexts();
  int count = cfg.image_card_image_count;
  if (count > 4) count = 4;
  for (int i = 0; i < count; i++) {
    contexts[i].active = false;
    contexts[i].widget = nullptr;
    contexts[i].btn = nullptr;
    contexts[i].entity_id.clear();
    contexts[i].source_url.clear();
    contexts[i].url.clear();
    contexts[i].refresh_interval_ms = 0;
    contexts[i].next_refresh_ms = 0;
    contexts[i].requested_once = false;
    contexts[i].timer_only = false;
    contexts[i].image = cfg.image_card_images ? cfg.image_card_images[i] : nullptr;
    if (contexts[i].image) contexts[i].image->release();
  }
}

inline ImageCardCtx *acquire_image_card_context(const GridConfig &cfg) {
  ImageCardCtx *contexts = image_card_contexts();
  int count = cfg.image_card_image_count;
  if (count > 4) count = 4;
  for (int i = 0; i < count; i++) {
    if (!contexts[i].active && contexts[i].image) {
      contexts[i].active = true;
      image_card_bind_callbacks(&contexts[i]);
      return &contexts[i];
    }
  }
  return nullptr;
}

inline bool image_card_position_widget(lv_obj_t *btn, lv_obj_t *widget,
                                       lv_coord_t *target_width = nullptr,
                                       lv_coord_t *target_height = nullptr) {
  if (!btn || !widget) return false;
  lv_obj_update_layout(btn);
  lv_coord_t width = lv_obj_get_width(btn);
  lv_coord_t height = lv_obj_get_height(btn);
  if (width <= 0 || height <= 0) return false;
  lv_coord_t pad_left = lv_obj_get_style_pad_left(btn, LV_PART_MAIN);
  lv_coord_t pad_top = lv_obj_get_style_pad_top(btn, LV_PART_MAIN);
  lv_obj_set_pos(widget, -pad_left, -pad_top);
  lv_obj_set_size(widget, width, height);
  if (target_width) *target_width = width;
  if (target_height) *target_height = height;
  return true;
}

inline void image_card_apply_widget_geometry(lv_obj_t *btn, lv_obj_t *widget,
                                             esphome::artwork_image::ArtworkImage *image) {
  if (!image) return;
  lv_coord_t width = 0;
  lv_coord_t height = 0;
  if (!image_card_position_widget(btn, widget, &width, &height)) return;
  image->set_target_size(width, height);
  image->set_resize_mode(esphome::artwork_image::ImageResizeMode::COVER);
}

inline void setup_image_card(BtnSlot &s) {
  lv_obj_clear_flag(s.btn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(s.btn, LV_OBJ_FLAG_CLICKABLE);
  if (s.icon_lbl) lv_obj_add_flag(s.icon_lbl, LV_OBJ_FLAG_HIDDEN);
  if (s.sensor_container) lv_obj_add_flag(s.sensor_container, LV_OBJ_FLAG_HIDDEN);
  if (s.text_lbl) lv_obj_add_flag(s.text_lbl, LV_OBJ_FLAG_HIDDEN);
  if (s.subpage_lbl) lv_obj_add_flag(s.subpage_lbl, LV_OBJ_FLAG_HIDDEN);

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 4, 0)
  lv_obj_t *img = lv_image_create(s.btn);
#else
  lv_obj_t *img = lv_img_create(s.btn);
#endif
  lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(img, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(img, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(img, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(img, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_radius(img, lv_obj_get_style_radius(s.btn, LV_PART_MAIN), LV_PART_MAIN);
  lv_obj_set_style_clip_corner(img, true, LV_PART_MAIN);
  lv_obj_set_user_data(s.sensor_container, img);
}

inline std::string image_card_join_url(const std::string &base, const std::string &path) {
  if (path.empty() || path == "unknown" || path == "unavailable") return "";
  if (path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0) return path;
  if (base.empty() || path[0] != '/') return "";
  return base + path;
}

inline std::string image_card_cache_bust_url(const std::string &url) {
  if (url.empty()) return "";
  std::string next = url;
  next += (next.find('?') == std::string::npos) ? "?time=" : "&time=";
  next += std::to_string(esphome::millis());
  return next;
}

inline void image_card_schedule_next_refresh(ImageCardCtx *ctx, uint32_t now = esphome::millis()) {
  if (!ctx || ctx->refresh_interval_ms == 0) {
    if (ctx) ctx->next_refresh_ms = 0;
    return;
  }
  ctx->next_refresh_ms = now + ctx->refresh_interval_ms;
}

inline void image_card_request_source_url(ImageCardCtx *ctx) {
  if (!ctx || !ctx->active || !ctx->image || ctx->source_url.empty()) return;
  uint32_t now = esphome::millis();
  ctx->url = image_card_cache_bust_url(ctx->source_url);
  ctx->requested_once = true;
  image_card_schedule_next_refresh(ctx, now);
  image_card_apply_widget_geometry(ctx->btn, ctx->widget, ctx->image);
  ESP_LOGI("image_card", "Downloading camera image for %s", ctx->entity_id.c_str());
  ctx->image->request_update_url(ctx->url);
}

inline void image_card_handle_picture(ImageCardCtx *ctx, const GridConfig &cfg,
                                      esphome::StringRef picture) {
  if (!ctx || !ctx->active || !ctx->image) return;
  std::string base = cfg.home_assistant_base_url ? cfg.home_assistant_base_url() : "";
  std::string raw = string_ref_limited(picture, 4096);
  std::string url = image_card_join_url(base, raw);
  if (url.empty()) {
    ESP_LOGW("image_card", "No usable entity_picture URL for %s", ctx->entity_id.c_str());
    image_card_hide(ctx);
    return;
  }
  ctx->source_url = url;
  if (!ctx->requested_once || !ctx->timer_only || ctx->refresh_interval_ms == 0) {
    image_card_request_source_url(ctx);
  } else if (ctx->next_refresh_ms == 0) {
    image_card_schedule_next_refresh(ctx);
  }
}

inline void image_card_refresh_due() {
  ImageCardCtx *contexts = image_card_contexts();
  uint32_t now = esphome::millis();
  for (int i = 0; i < 4; i++) {
    ImageCardCtx *ctx = &contexts[i];
    if (!ctx->active || ctx->refresh_interval_ms == 0 ||
        ctx->source_url.empty() || !ctx->requested_once) {
      continue;
    }
    if ((int32_t)(now - ctx->next_refresh_ms) >= 0) {
      image_card_request_source_url(ctx);
    }
  }
}

inline bool bind_image_card(BtnSlot &s, const ParsedCfg &p, const GridConfig &cfg) {
  if (p.type != "image") return false;
  lv_obj_t *widget = s.sensor_container
    ? static_cast<lv_obj_t *>(lv_obj_get_user_data(s.sensor_container))
    : nullptr;
  if (p.entity.empty()) return true;
  if (p.entity.rfind("camera.", 0) != 0) {
    ESP_LOGW("image_card", "Image card only supports camera entities: %s", p.entity.c_str());
    return true;
  }
  ImageCardCtx *ctx = acquire_image_card_context(cfg);
  if (!ctx) {
    ESP_LOGW("image_card", "No image card downloader available for %s", p.entity.c_str());
    return true;
  }
  ctx->widget = widget;
  ctx->btn = s.btn;
  ctx->entity_id = p.entity;
  ctx->refresh_interval_ms = image_card_refresh_interval_ms(p);
  ctx->timer_only = image_card_timer_only_refresh(p);
  image_card_apply_widget_geometry(ctx->btn, ctx->widget, ctx->image);

  ha_subscribe_attribute(
    p.entity,
    std::string("entity_picture"),
    std::function<void(esphome::StringRef)>(
      [ctx, cfg](esphome::StringRef picture) {
        image_card_handle_picture(ctx, cfg, picture);
      })
  );
  ha_get_attribute(
    p.entity,
    std::string("entity_picture"),
    std::function<void(esphome::StringRef)>(
      [ctx, cfg](esphome::StringRef picture) {
        image_card_handle_picture(ctx, cfg, picture);
      })
  );
  return true;
}
