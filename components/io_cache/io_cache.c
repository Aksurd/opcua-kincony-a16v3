#include "io_cache.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "io_cache";

#define NUM_DISCRETE_INPUTS 16
#define NUM_DISCRETE_OUTPUTS 16
#define NUM_TEMP_SENSORS 4

typedef struct {
    uint16_t discrete_inputs_cache;
    uint16_t discrete_outputs_cache;
    float temperature_cache[NUM_TEMP_SENSORS];
    bool temp_valid[NUM_TEMP_SENSORS];
    uint64_t inputs_timestamp_ms;
    uint64_t outputs_timestamp_ms;
    uint64_t temp_timestamp_ms[NUM_TEMP_SENSORS];
    uint64_t inputs_server_timestamp_ms;
    uint64_t outputs_server_timestamp_ms;
    uint64_t temp_server_timestamp_ms[NUM_TEMP_SENSORS];
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
    for (int i = 0; i < NUM_TEMP_SENSORS; i++) {
        io_cache.temperature_cache[i] = 0.0f;
        io_cache.temp_valid[i] = false;
    }
    ESP_LOGI(TAG, "IO Cache initialized");
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

bool io_cache_get_temperature(int sensor_index, float *temp, uint64_t *source_timestamp, uint64_t *server_timestamp) {
    if (sensor_index < 0 || sensor_index >= NUM_TEMP_SENSORS) return false;
    bool success = false;
    if (xSemaphoreTake(io_cache.mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        if (io_cache.temp_valid[sensor_index]) {
            *temp = io_cache.temperature_cache[sensor_index];
            if (source_timestamp) *source_timestamp = io_cache.temp_timestamp_ms[sensor_index];
            if (server_timestamp) *server_timestamp = io_cache.temp_server_timestamp_ms[sensor_index];
            success = true;
        }
        xSemaphoreGive(io_cache.mutex);
    }
    return success;
}

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

void io_cache_update_temperature(int sensor_index, float new_temp, uint64_t source_timestamp_ms) {
    if (sensor_index < 0 || sensor_index >= NUM_TEMP_SENSORS) return;
    if (xSemaphoreTake(io_cache.mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        io_cache.temperature_cache[sensor_index] = new_temp;
        io_cache.temp_valid[sensor_index] = true;
        io_cache.temp_timestamp_ms[sensor_index] = source_timestamp_ms;
        io_cache.temp_server_timestamp_ms[sensor_index] = get_current_time_ms();
        xSemaphoreGive(io_cache.mutex);
    }
}
