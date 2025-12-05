#include "io_cache.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "io_cache";

#define NUM_DISCRETE_INPUTS 16
#define NUM_DISCRETE_OUTPUTS 16
// УДАЛЕНО: #define NUM_TEMP_SENSORS 4

typedef struct {
    uint16_t discrete_inputs_cache;
    uint16_t discrete_outputs_cache;
    // УДАЛЕНО: float temperature_cache[NUM_TEMP_SENSORS];
    // УДАЛЕНО: bool temp_valid[NUM_TEMP_SENSORS];
    uint64_t inputs_timestamp_ms;
    uint64_t outputs_timestamp_ms;
    // УДАЛЕНО: uint64_t temp_timestamp_ms[NUM_TEMP_SENSORS];
    uint64_t inputs_server_timestamp_ms;
    uint64_t outputs_server_timestamp_ms;
    // УДАЛЕНО: uint64_t temp_server_timestamp_ms[NUM_TEMP_SENSORS];
    SemaphoreHandle_t mutex;
} io_cache_t;

static io_cache_t io_cache;

static uint64_t get_current_time_ms(void) {
    return (uint64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void io_cache_init(void) {
    memset(&io_cache, 0, sizeof(io_cache_t));
    io_cache.mutex = xSemaphoreCreateMutex();
    if (io_cache.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return;
    }
    // УДАЛЕНО: инициализация температуры
}

uint16_t io_cache_get_discrete_inputs(uint64_t *source_timestamp, uint64_t *server_timestamp) {
    uint16_t val = 0;
    if (xSemaphoreTake(io_cache.mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        val = io_cache.discrete_inputs_cache;
        if (source_timestamp) *source_timestamp = io_cache.inputs_timestamp_ms;
        if (server_timestamp) *server_timestamp = io_cache.inputs_server_timestamp_ms;
        xSemaphoreGive(io_cache.mutex);
    }
    return val;
}

uint16_t io_cache_get_discrete_outputs(uint64_t *source_timestamp, uint64_t *server_timestamp) {
    uint16_t val = 0;
    if (xSemaphoreTake(io_cache.mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        val = io_cache.discrete_outputs_cache;
        if (source_timestamp) *source_timestamp = io_cache.outputs_timestamp_ms;
        if (server_timestamp) *server_timestamp = io_cache.outputs_server_timestamp_ms;
        xSemaphoreGive(io_cache.mutex);
    }
    return val;
}

// УДАЛЕНО ВСЯ ФУНКЦИЯ: bool io_cache_get_temperature(...)

void io_cache_update_discrete_inputs(uint16_t new_val, uint64_t source_timestamp_ms) {
    if (xSemaphoreTake(io_cache.mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        io_cache.discrete_inputs_cache = new_val;
        io_cache.inputs_timestamp_ms = source_timestamp_ms;
        io_cache.inputs_server_timestamp_ms = get_current_time_ms();
        xSemaphoreGive(io_cache.mutex);
    }
}

void io_cache_update_discrete_outputs(uint16_t new_val, uint64_t source_timestamp_ms) {
    if (xSemaphoreTake(io_cache.mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        io_cache.discrete_outputs_cache = new_val;
        io_cache.outputs_timestamp_ms = source_timestamp_ms;
        io_cache.outputs_server_timestamp_ms = get_current_time_ms();
        xSemaphoreGive(io_cache.mutex);
    }
}

// УДАЛЕНО ВСЯ ФУНКЦИЯ: void io_cache_update_temperature(...)
