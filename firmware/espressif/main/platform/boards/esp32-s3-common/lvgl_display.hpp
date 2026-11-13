#pragma once

#include <cstdint>

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"

namespace micropixel::platform::esp32_s3_common {

struct Landscape320State;

[[nodiscard]] esp_err_t RegisterLvglDisplay(Landscape320State& state, bool double_buffer);
[[nodiscard]] esp_err_t RegisterLvglDisplay(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io,
                                            lv_display_t*& display, int32_t width, int32_t height, bool double_buffer);

}  // namespace micropixel::platform::esp32_s3_common
