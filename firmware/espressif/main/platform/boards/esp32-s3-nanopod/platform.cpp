#include "platform/platform.hpp"

#include <array>
#include <cstdint>
#include <optional>

#include "device/contracts/sensors.hpp"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "host/ui/lvgl/square_common/square_system_ui.hpp"
#include "lvgl.h"
#include "platform/adapters/graphics_adapter.hpp"
#include "platform/boards/esp32-s3-common/lvgl_display.hpp"
#include "platform/boards/esp32-s3-nanopod/board_config.hpp"
#include "platform/boards/esp32-s3-nanopod/board_hardware.hpp"
#include "platform/boards/esp32-s3-nanopod/display_hardware.hpp"
#include "platform/boards/esp32-s3-nanopod/platform_state.hpp"
#include "platform/boards/esp32-s3-nanopod/presentation.hpp"
#include "platform/drivers/sensors/qmi8658.hpp"
#include "platform/input/esp_lcd_touch_input.hpp"
#include "platform/lvgl/guest_graphics_operations.hpp"
#include "platform/memory/ext_ram_bss.hpp"
#include "platform/memory/internal_ram.hpp"
#include "platform/sensors/polled_vector_sensor_peripheral.hpp"
#include "platform/transports/development_display_control.hpp"
#include "platform/wifi/native_wifi_radio.hpp"
#include "platform/wifi/wifi_manager.hpp"

namespace micropixel::platform {
namespace {

namespace board_detail = esp32_s3_nanopod::detail;
constexpr device::PeripheralChannelId kAccelerationChannel = 1U;
constexpr device::PeripheralChannelId kAngularVelocityChannel = 2U;

esp_err_t InitializeDisplay(esp32_s3_nanopod::BoardHardware& hardware, board_detail::NanopodBoardState& state) {
    ESP_RETURN_ON_ERROR(board_detail::InitializeDisplayHardware(hardware, state), board_detail::kTag,
                        "initialize CO5300 display failed");
#if CONFIG_MICROPIXEL_S3_NANOPOD_LVGL_DOUBLE_BUFFER
    constexpr bool kDoubleBuffer = true;
#else
    constexpr bool kDoubleBuffer = false;
#endif
    return esp32_s3_common::RegisterLvglDisplay(state.panel, state.panel_io, state.display, board_detail::kWidth,
                                                board_detail::kHeight, kDoubleBuffer);
}

esp_err_t InitializeTouch(esp32_s3_nanopod::BoardHardware& hardware, board_detail::NanopodBoardState& state) {
    ESP_RETURN_ON_ERROR(board_detail::InitializeTouchHardware(hardware, state), board_detail::kTag,
                        "initialize CST820 touch failed");
    ESP_RETURN_ON_ERROR(state.i2c_executor.Initialize(), board_detail::kTag, "start I2C executor failed");
    return state.touch_input.Initialize(state.touch, state.i2c_executor);
}

class Esp32S3NanoPodBoard final : public Board {
   public:
    Esp32S3NanoPodBoard()
        : state_(TaskState(input_state_)),
          graphics_context_{.engine = &state_.guest_graphics, .hooks = state_.ui.GraphicsHooks()},
          graphics_(lvgl::MakeGuestGraphicsOperations(graphics_context_)),
          sensor_channels_{{
              {kAccelerationChannel, MICROPIXEL_SENSOR_ACCELERATION, acceleration_, "nanopod_accel"},
              {kAngularVelocityChannel, MICROPIXEL_SENSOR_ANGULAR_VELOCITY, angular_velocity_, "nanopod_gyro"},
          }},
          sensors_(sensor_channels_, "nanopod_sensors"),
          presentation_(
              state_,
              [](void* context, int percent) {
                  return static_cast<esp32_s3_nanopod::BoardHardware*>(context)->SetBrightness(percent);
              },
              &hardware_),
          system_ui_(state_.ui, presentation_) {
        state_.guest_graphics.SetPresentationHooks(state_.ui.GuestFrameHooks());
    }

    [[nodiscard]] esp_err_t Initialize(BoardContext& context) override {
        ESP_RETURN_ON_FALSE(memory::IsInternalObject(*this), ESP_ERR_INVALID_STATE, board_detail::kTag,
                            "Board control objects must reside in internal RAM");
        presentation_.BindAudioEngine(context.AudioEngine());
        ESP_LOGI(board_detail::kTag, "initializing ESP32-S3 NanoPod");
        ESP_RETURN_ON_ERROR(hardware_.Initialize(), board_detail::kTag, "initialize board hardware failed");
        ESP_RETURN_ON_ERROR(InitializeDisplay(hardware_, state_), board_detail::kTag, "initialize display failed");
        ESP_RETURN_ON_ERROR(state_.display_shadow.Initialize(state_.display), board_detail::kTag,
                            "initialize PSRAM displayed shadow failed");
        ESP_RETURN_ON_ERROR(InitializeTouch(hardware_, state_), board_detail::kTag, "initialize touch failed");

        const esp_err_t sensor_status = inertial_.Initialize(hardware_.I2cBus());
        if (sensor_status != ESP_OK) {
            ESP_LOGW(board_detail::kTag, "QMI8658 unavailable for this boot: %s", esp_err_to_name(sensor_status));
        }
        ESP_RETURN_ON_ERROR(sensors_.Initialize(state_.i2c_executor), board_detail::kTag,
                            "initialize sensor polling failed");
        ESP_RETURN_ON_ERROR(state_.guest_graphics.Initialize(state_.display, nullptr), board_detail::kTag,
                            "initialize RGB565 Guest graphics failed");

        if (esp_lv_adapter_lock(-1) != ESP_OK) {
            return ESP_FAIL;
        }
        const esp_err_t ui_status = state_.ui.InitializeLocked(state_.display);
        esp_lv_adapter_unlock();
        ESP_RETURN_ON_ERROR(ui_status, board_detail::kTag, "initialize 460x460 Host UI failed");

        ESP_RETURN_ON_ERROR(esp_lv_adapter_start(), board_detail::kTag, "start LVGL adapter failed");
        ESP_RETURN_ON_ERROR(state_.touch_input.Start(state_.display), board_detail::kTag,
                            "start CST820 touch input failed");
        ESP_RETURN_ON_ERROR(state_.development_display.Start(state_.touch_input, state_.local_control,
                                                             board_detail::kWidth, board_detail::kHeight,
                                                             transports::DevelopmentCaptureHook::For(presentation_)),
                            board_detail::kTag, "start USB Serial/JTAG development control failed");
        ESP_RETURN_ON_ERROR(hardware_.SetBrightness(80), board_detail::kTag, "set startup brightness failed");

        ESP_LOGI(board_detail::kTag, "NanoPod ready: panel=CO5300 touch=CST820 460x460 RGB565 QSPI=40MHz");
        BoardRegistration& registration = registration_.emplace(device::BoardInfo{
            .board = "ESP32-S3 NanoPod",
            .host_chip = "ESP32-S3",
            .firmware_target = "esp32-s3-nanopod",
            .wifi_coprocessor = "Native ESP32-S3",
            .touch_controller = state_.touch_name,
            .display =
                {
                    .driver = state_.panel_name,
                    .interface = "QSPI 40 MHz",
                    .pixel_format = "RGB565",
                    .width_pixels = board_detail::kWidth,
                    .height_pixels = board_detail::kHeight,
                },
            .graphics_acceleration = "CPU only; QSPI DMA transport",
        });
        registration.SetInput(state_.ui.Input());
        registration.SetGraphics(graphics_);
        registration.SetWifi(wifi_);
        registration.SetLocalControl(state_.local_control);
        registration.SetSystemUi(system_ui_);
        bool registered = true;
        if (acceleration_.available()) {
            registered =
                registration.AddSensor(sensors_, kAccelerationChannel, "Built-in QMI8658 accelerometer") && registered;
        }
        if (angular_velocity_.available()) {
            registered =
                registration.AddSensor(sensors_, kAngularVelocityChannel, "Built-in QMI8658 gyroscope") && registered;
        }
        return registered && context.Publish(registration) ? ESP_OK : ESP_ERR_INVALID_STATE;
    }

    void BindBackgroundExecutor(work::BackgroundExecutor& executor) override {
        state_.ui.BindBackgroundExecutor(executor);
        state_.guest_graphics.BindBackgroundExecutor(executor);
        wifi_.BindBackgroundExecutor(executor);
    }

   private:
    std::optional<BoardRegistration> registration_{};
    esp32_s3_nanopod::BoardHardware hardware_{};

    static board_detail::NanopodBoardState& TaskState(board_detail::NanopodInputState& input) {
        static MICROPIXEL_EXT_RAM_BSS board_detail::NanopodBoardState state(input);
        return state;
    }

    board_detail::NanopodInputState input_state_{};
    board_detail::NanopodBoardState& state_;
    lvgl::GuestGraphicsOperationsContext graphics_context_{};
    adapters::GraphicsAdapter graphics_;
    drivers::Qmi8658 inertial_{esp32_s3_nanopod::board::kQmi8658I2cAddress};
    drivers::Qmi8658::Vector acceleration_{inertial_, drivers::Qmi8658::Kind::kAcceleration};
    drivers::Qmi8658::Vector angular_velocity_{inertial_, drivers::Qmi8658::Kind::kAngularVelocity};
    std::array<sensors::PolledVectorSensorPeripheral::Channel, 2> sensor_channels_;
    sensors::PolledVectorSensorPeripheral sensors_;
    board_detail::NanopodPresentation presentation_;
    host_ui::lvgl::square_common::SquareSystemUi system_ui_;
    wifi::NativeWifiRadio wifi_radio_{};
    wifi::WifiManager wifi_{wifi_radio_};
};

}  // namespace

Board& ConfiguredBoard() {
    static Esp32S3NanoPodBoard board;
    return board;
}

}  // namespace micropixel::platform
