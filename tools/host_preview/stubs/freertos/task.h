#pragma once
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
void vTaskDelay(TickType_t);
void vTaskDelete(void *);
TickType_t xTaskGetTickCount();
BaseType_t xTaskCreate(void (*)(void *), const char *, int, void *, int, void *);
BaseType_t xTaskCreatePinnedToCore(void (*)(void *), const char *, int, void *, int, void *, int);
