#include "io_cache.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "model.h"
#include "ds18b20.h"

static const char *TAG = "io_polling";

#define POLL_INPUTS_INTERVAL_MS     20
#define POLL_TEMP_INTERVAL_MS       1000

static uint64_t get_current_time_ms(void) {
    return (uint64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void io_polling_task(void *pvParameters) {
    TickType_t xLastInputsTime = xTaskGetTickCount();
    TickType_t xLastTempTime = xTaskGetTickCount();
    
    ESP_LOGI(TAG, "IO polling task started (240 MHz)");
    
    while (1) {
        TickType_t xNow = xTaskGetTickCount();
        
        if ((xNow - xLastInputsTime) * portTICK_PERIOD_MS >= POLL_INPUTS_INTERVAL_MS) {
            uint16_t inputs = read_discrete_inputs_slow();
            uint64_t timestamp = get_current_time_ms();
            io_cache_update_discrete_inputs(inputs, timestamp);
            xLastInputsTime = xNow;
        }
        
        if ((xNow - xLastTempTime) * portTICK_PERIOD_MS >= POLL_TEMP_INTERVAL_MS) {
            float temp = ReadDSTemperature(DS18B20_GPIO);
            uint64_t timestamp = get_current_time_ms();
            io_cache_update_temperature(0, temp, timestamp);
            xLastTempTime = xNow;
        }
        
        vTaskDelay(pdMS_TO_TICKS(5));  // 5ms для 240 МГц
    }
}

void io_polling_task_start(void) {
    xTaskCreatePinnedToCore(io_polling_task, "io_poll", 4096, NULL, 
                           configMAX_PRIORITIES - 2, NULL, 1);
    ESP_LOGI(TAG, "IO polling task created");
}
