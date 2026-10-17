#pragma once
#include "esp_err.h"
#include <cstdint>
typedef int nvs_handle_t;
enum { NVS_READONLY, NVS_READWRITE };
esp_err_t nvs_open(const char *, int, nvs_handle_t *);
void nvs_close(nvs_handle_t);
esp_err_t nvs_commit(nvs_handle_t);
esp_err_t nvs_get_u8(nvs_handle_t, const char *, uint8_t *);
esp_err_t nvs_get_i8(nvs_handle_t, const char *, int8_t *);
esp_err_t nvs_get_u32(nvs_handle_t, const char *, uint32_t *);
esp_err_t nvs_set_u8(nvs_handle_t, const char *, uint8_t);
esp_err_t nvs_set_i8(nvs_handle_t, const char *, int8_t);
esp_err_t nvs_set_u32(nvs_handle_t, const char *, uint32_t);
