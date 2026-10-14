set(waze_hud_extra_requires "")
if(IDF_TARGET STREQUAL "esp32s3")
    # ES3C28P speaker path: ES8311 codec over legacy I2C + I2S output.
    list(APPEND waze_hud_extra_requires driver esp_driver_i2s espressif__es8311)
endif()
# WiFi OTA and its touch/BOOT trigger (upstream B16) are not used on any board of this
# project: the S3 has no XPT2046 (GPIO33/36 are PSRAM pins) and the OTA page is an
# unauthenticated upload on a fixed-password SoftAP. Both boards build inert stubs; the
# upstream files ota/wifi_ota_manager.cpp and touch/touch_sensor.cpp stay in the tree
# untouched so merges from upstream remain clean.
set(waze_hud_board_sources "ota/wifi_ota_manager_stub.cpp" "touch/touch_sensor_stub.cpp")

idf_component_register(
    SRCS
        "app_main.cpp"
        "audio/overspeed_audio.cpp"
        "audio/overspeed_voice.cpp"
        "bluetooth/ble_transport.cpp"
        "protocol/hlp_core.c"
        "protocol/hlp_protocol.cpp"
        "protocol/hlp_decoder.cpp"
        "state/hud_state_store.cpp"
        "state/trip_stats.cpp"
        "display/display_driver.cpp"
        "display/hud_renderer.cpp"
        "display/bitmap_font.cpp"
        "assets/generated_assets.cpp"
        "config/device_config.cpp"
        "system/system_status.cpp"
        ${waze_hud_board_sources}
    INCLUDE_DIRS "."
    REQUIRES bt esp_adc esp_lcd esp_driver_gpio esp_driver_ledc esp_driver_spi esp_hw_support nvs_flash json esp_netif
             ${waze_hud_extra_requires}
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>
    -Wall -Wextra -Werror=return-type
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    WAZE_HUD_FIRMWARE_VERSION="${PROJECT_VER}"
)
