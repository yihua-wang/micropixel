#include "platform/boards/esp32-s3-nanopod/display_hardware.hpp"

#include <iterator>

#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_cst820.h"
#include "platform/boards/esp32-s3-nanopod/board_hardware.hpp"
#include "platform/boards/esp32-s3-nanopod/platform_state.hpp"

namespace micropixel::platform::esp32_s3_nanopod::detail {
namespace {

constexpr int kLcdCommandBits = 32;
constexpr int kLcdParameterBits = 8;
constexpr size_t kPanelIoQueueDepth = 10U;

constexpr uint8_t kPage20[]{0x20};
constexpr uint8_t kRegister19[]{0x10};
constexpr uint8_t kRegister1c[]{0xa0};
constexpr uint8_t kPage00[]{0x00};
constexpr uint8_t kRegisterC4[]{0x80};
constexpr uint8_t kPixelFormat[]{0x55};
constexpr uint8_t kTearEffect[]{0x00};
constexpr uint8_t kDisplayControl[]{0x20};
constexpr uint8_t kMaximumBrightness[]{0xff};
constexpr uint8_t kHbmBrightness[]{0xff};
constexpr uint8_t kColumnRange[]{0x00, 0x00, 0x01, 0xcb};
constexpr uint8_t kRowRange[]{0x00, 0x00, 0x01, 0xcb};

const co5300_lcd_init_cmd_t kCo5300Initialization[]{
    {0x11, nullptr, 0, 120},          {0xfe, kPage20, sizeof(kPage20), 0}, {0x19, kRegister19, 1, 0},
    {0x1c, kRegister1c, 1, 0},        {0xfe, kPage00, sizeof(kPage00), 0}, {0xc4, kRegisterC4, 1, 0},
    {0x3a, kPixelFormat, 1, 0},       {0x35, kTearEffect, 1, 0},           {0x53, kDisplayControl, 1, 0},
    {0x51, kMaximumBrightness, 1, 0}, {0x63, kHbmBrightness, 1, 0},        {0x2a, kColumnRange, 4, 0},
    {0x2b, kRowRange, 4, 0},
};

void ReleaseDisplayHardware(NanopodBoardState& state, bool bus_initialized) {
    if (state.panel != nullptr) {
        (void)esp_lcd_panel_del(state.panel);
        state.panel = nullptr;
    }
    if (state.panel_io != nullptr) {
        (void)esp_lcd_panel_io_del(state.panel_io);
        state.panel_io = nullptr;
    }
    if (bus_initialized) {
        (void)spi_bus_free(kLcdSpiHost);
    }
}

}  // namespace

esp_err_t InitializeDisplayHardware(BoardHardware& hardware, NanopodBoardState& state) {
    spi_bus_config_t bus_config{};
    bus_config.data0_io_num = kLcdData0;
    bus_config.data1_io_num = kLcdData1;
    bus_config.sclk_io_num = kLcdClock;
    bus_config.data2_io_num = kLcdData2;
    bus_config.data3_io_num = kLcdData3;
    bus_config.data4_io_num = GPIO_NUM_NC;
    bus_config.data5_io_num = GPIO_NUM_NC;
    bus_config.data6_io_num = GPIO_NUM_NC;
    bus_config.data7_io_num = GPIO_NUM_NC;
    bus_config.max_transfer_sz =
        static_cast<int>(kDisplayWidth * CONFIG_MICROPIXEL_LVGL_PARTIAL_BUFFER_HEIGHT * sizeof(uint16_t));
    esp_err_t status = spi_bus_initialize(kLcdSpiHost, &bus_config, SPI_DMA_CH_AUTO);
    if (status != ESP_OK) {
        return status;
    }

    esp_lcd_panel_io_spi_config_t io_config{};
    io_config.cs_gpio_num = kLcdChipSelect;
    io_config.dc_gpio_num = GPIO_NUM_NC;
    io_config.spi_mode = 0;
    io_config.pclk_hz = kLcdPixelClockHz;
    io_config.trans_queue_depth = kPanelIoQueueDepth;
    io_config.lcd_cmd_bits = kLcdCommandBits;
    io_config.lcd_param_bits = kLcdParameterBits;
    io_config.flags.quad_mode = true;
    io_config.flags.psram_dma_direct = true;
    status = esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(kLcdSpiHost), &io_config, &state.panel_io);
    if (status != ESP_OK) {
        ReleaseDisplayHardware(state, true);
        return status;
    }

    co5300_vendor_config_t vendor_config{};
    vendor_config.init_cmds = kCo5300Initialization;
    vendor_config.init_cmds_size = std::size(kCo5300Initialization);
    vendor_config.flags.use_qspi_interface = true;

    esp_lcd_panel_dev_config_t panel_config{};
    panel_config.reset_gpio_num = kLcdReset;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = 16;
    panel_config.vendor_config = &vendor_config;
    status = esp_lcd_new_panel_co5300(state.panel_io, &panel_config, &state.panel);
    if (status == ESP_OK) {
        status = esp_lcd_panel_reset(state.panel);
    }
    if (status == ESP_OK) {
        status = esp_lcd_panel_init(state.panel);
    }
    if (status == ESP_OK) {
        status = esp_lcd_panel_set_gap(state.panel, kDisplayGapX, kDisplayGapY);
    }
    if (status != ESP_OK) {
        ReleaseDisplayHardware(state, true);
        return status;
    }

    hardware.SetPanel(state.panel);
    return ESP_OK;
}

esp_err_t InitializeTouchHardware(BoardHardware& hardware, NanopodBoardState& state) {
    esp_lcd_panel_io_i2c_config_t io_config{};
    io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_CST820_ADDRESS;
    io_config.scl_speed_hz = 400000U;
    io_config.control_phase_bytes = 1U;
    io_config.dc_bit_offset = 0U;
    io_config.lcd_cmd_bits = 8U;
    io_config.flags.disable_control_phase = true;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(hardware.I2cBus(), &io_config, &state.touch_io), kTag,
                        "create CST820 panel IO failed");

    esp_lcd_touch_config_t touch_config{};
    touch_config.x_max = kWidth;
    touch_config.y_max = kHeight;
    touch_config.rst_gpio_num = kTouchReset;
    touch_config.int_gpio_num = kTouchInterrupt;
    touch_config.levels.reset = 0;
    touch_config.levels.interrupt = 0;
    const esp_err_t status = esp_lcd_touch_new_i2c_cst820(state.touch_io, &touch_config, &state.touch);
    if (status != ESP_OK) {
        (void)esp_lcd_panel_io_del(state.touch_io);
        state.touch_io = nullptr;
    }
    return status;
}

}  // namespace micropixel::platform::esp32_s3_nanopod::detail
