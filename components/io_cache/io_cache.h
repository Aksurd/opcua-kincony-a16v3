#ifndef IO_CACHE_H
#define IO_CACHE_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

void io_cache_init(void);
uint16_t io_cache_get_discrete_inputs(uint64_t *source_timestamp, uint64_t *server_timestamp);
uint16_t io_cache_get_discrete_outputs(uint64_t *source_timestamp, uint64_t *server_timestamp);
// УДАЛЕНО: bool io_cache_get_temperature(int sensor_index, float *temp, uint64_t *source_timestamp, uint64_t *server_timestamp);
void io_cache_update_discrete_inputs(uint16_t new_val, uint64_t source_timestamp_ms);
void io_cache_update_discrete_outputs(uint16_t new_val, uint64_t source_timestamp_ms);
// УДАЛЕНО: void io_cache_update_temperature(int sensor_index, float new_temp, uint64_t source_timestamp_ms);
void io_polling_task_start(void);

#ifdef __cplusplus
}
#endif

#endif

// ADC функции кэша
bool io_cache_get_adc_channel(int channel, float *value, uint64_t *source_timestamp, uint64_t *server_timestamp);
float* io_cache_get_all_adc_channels(void);
void io_cache_update_adc_channel(int channel, float new_value, uint64_t source_timestamp_ms);
void io_cache_update_all_adc_channels(float* values, uint64_t source_timestamp_ms);
