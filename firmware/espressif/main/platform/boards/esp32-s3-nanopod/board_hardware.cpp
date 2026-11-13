#include "platform/boards/esp32-s3-nanopod/board_hardware.hpp"

#include "esp_check.h"
#include "esp_lcd_co5300.h"
#include "esp_lv_adapter.h"

namespace micropixel::platform::esp32_s3_nanopod {
namespace {

constexpr char kTag[] = "nanopod_hw";

}  // namespace

esp_err_t BoardHardware::Initialize() {
    if (initialized_ || i2c_bus_ != nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    gpio_config_t enable_config{};
    enable_config.pin_bit_mask = BIT64(kLcdEnable);
    enable_config.mode = GPIO_MODE_OUTPUT;
    enable_config.pull_up_en = GPIO_PULLUP_DISABLE;
    enable_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    enable_config.intr_type = GPIO_INTR_DISABLE;
    ESP_RETURN_ON_ERROR(gpio_config(&enable_config), kTag, "configure display enable failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(kLcdEnable, 1), kTag, "enable display failed");

    i2c_master_bus_config_t bus_config{};
    bus_config.i2c_port = kI2cPort;
    bus_config.sda_io_num = kI2cSda;
    bus_config.scl_io_num = kI2cScl;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7U;
    bus_config.flags.enable_internal_pullup = true;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &i2c_bus_), kTag, "create internal I2C bus failed");

    initialized_ = true;
    return ESP_OK;
}

esp_err_t BoardHardware::SetBrightness(int percent) {
    if (!initialized_ || panel_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    const uint8_t bounded = static_cast<uint8_t>(percent < 0 ? 0 : (percent > 100 ? 100 : percent));
    const esp_err_t lock_status = esp_lv_adapter_lock(-1);
    if (lock_status != ESP_OK) {
        return lock_status;
    }
    const esp_err_t status = esp_lcd_panel_co5300_set_brightness(panel_, bounded);
    esp_lv_adapter_unlock();
    return status;
}

}  // namespace micropixel::platform::esp32_s3_nanopod
