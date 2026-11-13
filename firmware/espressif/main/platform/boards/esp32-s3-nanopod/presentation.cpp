#include "platform/boards/esp32-s3-nanopod/presentation.hpp"

#include <algorithm>

#include "esp_log.h"
#include "platform/audio/audio_engine.hpp"
#include "platform/boards/esp32-s3-nanopod/platform_state.hpp"
#include "platform/lvgl/display/screen_capture.hpp"

namespace micropixel::platform::esp32_s3_nanopod::detail {
namespace {

constexpr char kPresentationTag[] = "nanopod_ui";

}  // namespace

std::expected<host_ui::ScreenCapture, host_ui::SystemUiError> NanopodPresentation::CaptureScreenJpeg() {
    return lvgl::CaptureScreenJpeg(state_.display, kWidth, kHeight,
                                   {.pixels = state_.display_shadow.Pixels(),
                                    .stride = state_.display_shadow.Stride(),
                                    .format = lvgl::DisplayCapturePixelFormat::kRgb565,
                                    .ready = state_.display_shadow.Ready()});
}

void NanopodPresentation::ApplyBrightness(uint8_t percent) {
    const esp_err_t status = brightness_setter_ != nullptr
                                 ? brightness_setter_(brightness_context_, std::min<uint8_t>(percent, 100U))
                                 : ESP_ERR_NOT_SUPPORTED;
    if (status != ESP_OK) {
        ESP_LOGW(kPresentationTag, "brightness update failed: %s", esp_err_to_name(status));
    }
}

void NanopodPresentation::ApplyVolume(uint8_t percent) {
    if (audio_ != nullptr) {
        audio_->SetMasterVolumePercent(percent);
    }
}

}  // namespace micropixel::platform::esp32_s3_nanopod::detail
