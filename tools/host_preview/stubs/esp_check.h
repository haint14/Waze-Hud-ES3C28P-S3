#pragma once
#include "esp_err.h"
#include "esp_log.h"
#define ESP_RETURN_ON_ERROR(x, tag, ...) do { esp_err_t e_ = (x); if (e_ != ESP_OK) return e_; } while (0)
#define ESP_RETURN_ON_FALSE(c, err, tag, ...) do { if (!(c)) return (err); } while (0)
