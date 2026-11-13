#pragma once

#include "esp_err.h"

namespace micropixel::platform::esp32_s3_nanopod {

class BoardHardware;

namespace detail {

struct NanopodBoardState;

[[nodiscard]] esp_err_t InitializeDisplayHardware(BoardHardware& hardware, NanopodBoardState& state);
[[nodiscard]] esp_err_t InitializeTouchHardware(BoardHardware& hardware, NanopodBoardState& state);

}  // namespace detail
}  // namespace micropixel::platform::esp32_s3_nanopod
