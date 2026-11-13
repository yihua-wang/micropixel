list(APPEND micropixel_sources
    "platform/sensors/polled_vector_sensor_peripheral.cpp"
    "platform/boards/esp32-s3-nanopod/board_hardware.cpp"
    "platform/boards/esp32-s3-nanopod/display_hardware.cpp"
    "platform/boards/esp32-s3-nanopod/platform.cpp"
    "platform/boards/esp32-s3-nanopod/presentation.cpp"
    "platform/drivers/sensors/qmi8658.cpp"
    "platform/input/esp_lcd_touch_input.cpp"
    "platform/transports/development_display_control.cpp"
    "platform/transports/usb_serial_jtag_local_control.cpp"
)

list(APPEND micropixel_requires
    espressif__esp_lcd_co5300
    viewesmart__esp_lcd_touch_cst820
)
