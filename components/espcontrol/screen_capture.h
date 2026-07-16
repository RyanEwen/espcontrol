#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>

#include "esphome/components/lvgl/lvgl_esphome.h"

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

// ── Screen capture ────────────────────────────────────────────────────
// Renders the active LVGL screen to a PSRAM buffer so the web server can
// serve it at /espcontrol/screenshot (see web_server_idf). Used to compare
// the physical rendering against the web setup-page preview.
//
// Threading: the HTTP handler runs on the httpd task and only touches the
// atomic request/ready flags; the actual LVGL render happens on the ESPHome
// main loop via screen_capture_process() (called from a small interval in
// common/addon/screen_capture.yaml). Requires CONFIG_LV_USE_SNAPSHOT=y; when
// the feature is disabled this compiles to a stub and the endpoint returns
// 501.

struct ScreenCaptureState {
  std::atomic<bool> requested{false};
  std::atomic<bool> ready{false};
  uint8_t *pixels = nullptr;   // RGB565, row-major, stride bytes per row
  int32_t width = 0;
  int32_t height = 0;
  uint32_t stride = 0;
};

inline ScreenCaptureState &screen_capture_state() {
  static ScreenCaptureState state;
  return state;
}

inline void screen_capture_release() {
  auto &s = screen_capture_state();
  s.ready.store(false);
#ifdef ESP_PLATFORM
  if (s.pixels != nullptr) heap_caps_free(s.pixels);
#else
  free(s.pixels);
#endif
  s.pixels = nullptr;
}

// Called from the httpd task: ask the main loop for a capture.
inline void screen_capture_request() {
  auto &s = screen_capture_state();
  if (s.ready.load()) return;  // previous capture not yet collected
  s.requested.store(true);
}

// Called from the ESPHome main loop (LVGL's thread). Renders the active
// screen when a capture was requested.
inline void screen_capture_process() {
#if LV_USE_SNAPSHOT
  auto &s = screen_capture_state();
  if (!s.requested.exchange(false)) return;
  if (s.ready.load()) return;

  lv_obj_t *screen = lv_screen_active();
  if (screen == nullptr) return;
  lv_obj_update_layout(screen);
  int32_t width = lv_obj_get_width(screen);
  int32_t height = lv_obj_get_height(screen);
  if (width <= 0 || height <= 0) return;

  uint32_t stride = lv_draw_buf_width_to_stride(width, LV_COLOR_FORMAT_RGB565);
  size_t size = (size_t) stride * (size_t) height;
  // The full frame does not fit in internal heap; use PSRAM explicitly.
#ifdef ESP_PLATFORM
  uint8_t *pixels = (uint8_t *) heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (pixels == nullptr) {
    pixels = (uint8_t *) heap_caps_malloc(size, MALLOC_CAP_8BIT);
  }
#else
  uint8_t *pixels = (uint8_t *) malloc(size);
#endif
  if (pixels == nullptr) {
    ESP_LOGW("screen_capture", "Failed to allocate %u bytes for capture", (unsigned) size);
    return;
  }

  lv_draw_buf_t draw_buf;
  if (lv_draw_buf_init(&draw_buf, width, height, LV_COLOR_FORMAT_RGB565, stride, pixels, size) !=
      LV_RESULT_OK) {
    ESP_LOGW("screen_capture", "Failed to init capture draw buffer");
#ifdef ESP_PLATFORM
    heap_caps_free(pixels);
#else
    free(pixels);
#endif
    return;
  }
  if (lv_snapshot_take_to_draw_buf(screen, LV_COLOR_FORMAT_RGB565, &draw_buf) != LV_RESULT_OK) {
    ESP_LOGW("screen_capture", "Snapshot render failed");
#ifdef ESP_PLATFORM
    heap_caps_free(pixels);
#else
    free(pixels);
#endif
    return;
  }

  s.pixels = pixels;
  s.width = width;
  s.height = height;
  s.stride = stride;
  s.ready.store(true);
  ESP_LOGI("screen_capture", "Captured %dx%d screen (%u bytes)", (int) width, (int) height,
           (unsigned) size);
#endif  // LV_USE_SNAPSHOT
}

inline bool screen_capture_supported() {
#if LV_USE_SNAPSHOT
  return true;
#else
  return false;
#endif
}
