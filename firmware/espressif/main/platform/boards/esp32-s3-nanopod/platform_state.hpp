#pragma once

#include <cstdint>

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "host/ui/lvgl/square_common/profiles/square_480.hpp"
#include "host/ui/lvgl/square_common/status_layer_transition.hpp"
#include "lvgl.h"
#include "platform/boards/esp32-s3-common/display_shadow.hpp"
#include "platform/boards/esp32-s3-nanopod/board_hardware.hpp"
#include "platform/buses/i2c_executor.hpp"
#include "platform/input/esp_lcd_touch_input.hpp"
#include "platform/lvgl/fonts/font_registry.hpp"
#include "platform/lvgl/guest_graphics_engine.hpp"
#include "platform/transports/development_display_control.hpp"
#include "platform/transports/usb_serial_jtag_local_control.hpp"

namespace micropixel::platform::esp32_s3_nanopod::detail {

namespace ui_profile = host_ui::lvgl::square_common::profiles::square_480;

inline constexpr char kTag[] = "esp32_s3_nanopod";
inline constexpr int32_t kWidth = static_cast<int32_t>(kDisplayWidth);
inline constexpr int32_t kHeight = static_cast<int32_t>(kDisplayHeight);
inline constexpr uint8_t kMaxTouchPoints = 1U;

struct NanopodInputState final {
    buses::I2cExecutor i2c_executor{};
    input::EspLcdTouchInput touch_input{kWidth, kHeight, kMaxTouchPoints};
};

struct NanopodBoardState final {
    explicit NanopodBoardState(NanopodInputState& input)
        : i2c_executor(input.i2c_executor), touch_input(input.touch_input) {}

    esp_lcd_panel_handle_t panel{};
    esp_lcd_panel_io_handle_t panel_io{};
    esp_lcd_panel_io_handle_t touch_io{};
    esp_lcd_touch_handle_t touch{};
    lv_display_t* display{};
    esp32_s3_common::DisplayShadow display_shadow{kWidth, kHeight};
    buses::I2cExecutor& i2c_executor;
    input::EspLcdTouchInput& touch_input;
    lvgl::FontRegistry fonts{};
    lvgl::GuestGraphicsEngine guest_graphics{kWidth, kHeight, fonts, graphics::SurfacePixelFormat::kRgb565};
    host_ui::lvgl::square_common::StaticStatusLayerTransition status_transition{};
    host_ui::lvgl::square_common::SquareSystemUiState ui{touch_input, guest_graphics, status_transition,
                                                         ui_profile::kSystemUiProfile};
    transports::UsbSerialJtagLocalControl local_control{};
    transports::DevelopmentDisplayControl development_display{};
    const char* panel_name{"CO5300"};
    const char* touch_name{"CST820"};
};

}  // namespace micropixel::platform::esp32_s3_nanopod::detail
