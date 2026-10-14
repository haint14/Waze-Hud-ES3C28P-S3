idf_component_register(
    SRCS
        "app_main.cpp"
        "bluetooth/ble_transport.cpp"
        "protocol/hlp_core.c"
        "protocol/hlp_protocol.cpp"
        "protocol/hlp_decoder.cpp"
        "state/hud_state_store.cpp"
        "display/display_driver.cpp"
        "display/hud_renderer.cpp"
        "display/bitmap_font.cpp"
        "assets/generated_assets.cpp"
        "config/device_config.cpp"
        "system/system_status.cpp"
        "ota/wifi_ota_manager.cpp"
        "touch/touch_sensor.cpp"
    INCLUDE_DIRS "."
    REQUIRES bt esp_adc esp_lcd esp_driver_gpio esp_driver_ledc esp_driver_spi esp_hw_support nvs_flash json esp_wifi esp_http_server esp_netif app_update
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>
    -Wall -Wextra -Werror=return-type
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    WAZE_HUD_FIRMWARE_VERSION="${PROJECT_VER}"
)
