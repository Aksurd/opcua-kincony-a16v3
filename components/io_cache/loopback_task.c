// file: components/io_cache/loopback_task.c
#include "io_cache.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "loopback_task";

#define LOOPBACK_UPDATE_INTERVAL_MS 1  // 1 мс период для быстрого loopback

static uint64_t get_current_time_ms(void) {
    return (uint64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void loopback_task(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    ESP_LOGI(TAG, "Loopback task started (1 ms period)");
    
    while (1) {
        // 1. Читаем текущее значение loopback_input из кэша
        uint16_t input_value = 0;
        uint64_t input_timestamp = 0;
        
        if (io_cache_get_loopback_input(&input_value, &input_timestamp, NULL)) {
            // 2. Копируем его в loopback_output (базовый loopback)
            io_cache_set_loopback_output(input_value, input_timestamp);
            
            // Можно добавить логику здесь:
            // Например, инвертировать биты: uint16_t output_value = ~input_value;
            // Или применять маски: uint16_t output_value = input_value & 0x00FF;
            // Пока делаем простое копирование: output = input
        }
        
        // Задержка для 1 мс периода
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(LOOPBACK_UPDATE_INTERVAL_MS));
    }
}

void loopback_task_start(void) {
    xTaskCreatePinnedToCore(loopback_task, "loopback_task", 2048, NULL, 
                           configMAX_PRIORITIES - 1, NULL, 1);  // Высокий приоритет
    ESP_LOGI(TAG, "Loopback task created");
}
