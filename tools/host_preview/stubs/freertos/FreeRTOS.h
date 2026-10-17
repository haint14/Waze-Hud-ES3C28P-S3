#pragma once
#include <cstdint>
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef struct { int x; } portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED {0}
#define pdMS_TO_TICKS(x) ((TickType_t)(x))
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY 0xffffffffu
void taskENTER_CRITICAL(portMUX_TYPE *);
void taskEXIT_CRITICAL(portMUX_TYPE *);
