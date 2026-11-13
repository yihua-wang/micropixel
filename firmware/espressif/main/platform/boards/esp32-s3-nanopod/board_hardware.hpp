#pragma once

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"

namespace micropixel::platform::esp32_s3_nanopod {

inline constexpr i2c_port_num_t kI2cPort = I2C_NUM_0;
inline constexpr gpio_num_t kI2cSda = GPIO_NUM_3;
inline constexpr gpio_num_t kI2cScl = GPIO_NUM_2;

inline constexpr spi_host_device_t kLcdSpiHost = SPI2_HOST;
inline constexpr gpio_num_t kLcdEnable = GPIO_NUM_40;
inline constexpr gpio_num_t kLcdClock = GPIO_NUM_9;
inline constexpr gpio_num_t kLcdChipSelect = GPIO_NUM_8;
inline constexpr gpio_num_t kLcdData0 = GPIO_NUM_10;
inline constexpr gpio_num_t kLcdData1 = GPIO_NUM_11;
inline constexpr gpio_num_t kLcdData2 = GPIO_NUM_12;
inline constexpr gpio_num_t kLcdData3 = GPIO_NUM_13;
inline constexpr gpio_num_t kLcdReset = GPIO_NUM_7;
inline constexpr gpio_num_t kTouchReset = GPIO_NUM_5;
inline constexpr gpio_num_t kTouchInterrupt = GPIO_NUM_4;
inline constexpr uint32_t kLcdPixelClockHz = 40'000'000U;
inline constexpr uint32_t kDisplayWidth = 460U;
inline constexpr uint32_t kDisplayHeight = 460U;
inline constexpr int kDisplayGapX = 10;
inline constexpr int kDisplayGapY = 0;

class BoardHardware final {
   public:
    [[nodiscard]] esp_err_t Initialize();
    [[nodiscard]] esp_err_t SetBrightness(int percent);
    void SetPanel(esp_lcd_panel_handle_t panel) { panel_ = panel; }
    [[nodiscard]] i2c_master_bus_handle_t I2cBus() const { return i2c_bus_; }

   private:
    i2c_master_bus_handle_t i2c_bus_{};
    esp_lcd_panel_handle_t panel_{};
    bool initialized_{};
};

}  // namespace micropixel::platform::esp32_s3_nanopod
