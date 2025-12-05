#include "io_cache.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "model.h"
#include <stdint.h>

static const char *TAG = "io_polling";

#define POLL_INPUTS_INTERVAL_MS     20

static uint64_t get_current_time_ms(void) {
    return (uint64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void io_polling_task(void *pvParameters) {
    TickType_t xLastInputsTime = xTaskGetTickCount();
    
    ESP_LOGI(TAG, "IO polling task started (240 MHz)");
    
    while (1) {
        TickType_t xNow = xTaskGetTickCount();
        
        if ((xNow - xLastInputsTime) * portTICK_PERIOD_MS >= POLL_INPUTS_INTERVAL_MS) {
            uint16_t inputs = read_discrete_inputs_slow();
            uint64_t timestamp = get_current_time_ms();
            io_cache_update_discrete_inputs(inputs, timestamp);
            xLastInputsTime = xNow;
        }
        

        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void io_polling_task_start(void) {
    xTaskCreatePinnedToCore(io_polling_task, "io_poll", 4096, NULL, 
                           8, NULL, 1);
    ESP_LOGI(TAG, "IO polling task created");
}
