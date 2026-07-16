// The vendored LVGL library build resolves its configuration in a way that
// leaves LV_USE_SNAPSHOT compiled out even when CONFIG_LV_USE_SNAPSHOT=y (the
// generated lv_conf.h omits the option and the vendored translation units do
// not pick up the Kconfig fallback), so lv_snapshot.c lands in the archive as
// an empty object. ESPHome-side translation units resolve the same option to 1
// via LV_CONF_SKIP + Kconfig, so compile the implementation here with that
// view. If a future LVGL build starts compiling the symbols itself this file
// will fail the link with duplicate definitions — delete it then.
#ifdef USE_ESP32
#include "esphome/components/lvgl/lvgl_esphome.h"
#if LV_USE_SNAPSHOT
extern "C" {
#include <src/draw/snapshot/lv_snapshot.c>
}
#endif
#endif
