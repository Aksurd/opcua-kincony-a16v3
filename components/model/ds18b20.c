#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "ds18b20.h"
#include "model.h"

// == global defines =============================================

static const char* TAG = "DS18B20";

float ds_temperature = 0.;
static int error_count = 0;  // Счетчик последовательных ошибок

// == get temperature ============================================

float getDSTemperature() { 
    return ds_temperature; 
}

// == reset pulse ================================================

static int ds18b20_reset()
{
    gpio_set_direction(DS18B20_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DS18B20_GPIO, 0);
    esp_rom_delay_us(480);
    
    gpio_set_direction(DS18B20_GPIO, GPIO_MODE_INPUT);
    esp_rom_delay_us(70);
    
    int presence = gpio_get_level(DS18B20_GPIO);
    esp_rom_delay_us(410);
    
    return (presence == 0);
}

// == write one bit ==============================================

static void ds18b20_write_bit(int bit)
{
    gpio_set_direction(DS18B20_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DS18B20_GPIO, 0);
    
    if (bit) {
        esp_rom_delay_us(6);
        gpio_set_direction(DS18B20_GPIO, GPIO_MODE_INPUT);
        esp_rom_delay_us(64);
    } else {
        esp_rom_delay_us(60);
        gpio_set_direction(DS18B20_GPIO, GPIO_MODE_INPUT);
        esp_rom_delay_us(10);
    }
}

// == write one byte =============================================

static void ds18b20_write_byte(uint8_t data)
{
    for (int i = 0; i < 8; i++) {
        ds18b20_write_bit(data & 0x01);
        data >>= 1;
    }
}

// == read one bit ===============================================

static int ds18b20_read_bit()
{
    int bit = 0;
    
    // 1. Короткий низкий уровень для инициации чтения
    gpio_set_direction(DS18B20_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DS18B20_GPIO, 0);
    esp_rom_delay_us(2);
    
    // 2. ОТПУСКАЕМ линию (переводим в INPUT) ДО чтения
    gpio_set_direction(DS18B20_GPIO, GPIO_MODE_INPUT);
    
    // 3. Ждем когда датчик установит уровень
    esp_rom_delay_us(10);
    
    // 4. Читаем значение
    bit = gpio_get_level(DS18B20_GPIO);
    
    // 5. Ждем до конца 60µs слота
    esp_rom_delay_us(48);
    
    return bit;
}

// == read one byte ==============================================

static uint8_t ds18b20_read_byte()
{
    uint8_t data = 0;
    
    for (int i = 0; i < 8; i++) {
        data >>= 1;
        if (ds18b20_read_bit()) {
            data |= 0x80;
        }
    }
    
    return data;
}

// Правильная CRC8 функция для DS18B20
static uint8_t ds18b20_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    
    for (uint8_t i = 0; i < len; i++) {
        uint8_t inbyte = data[i];
        for (uint8_t j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) 
                crc ^= 0x8C;
            inbyte >>= 1;
        }
    }
    return crc;
}

// == read temperature from DS18B20 ==============================

int readDS18B20()
{
    ESP_LOGI(TAG, "Starting DS18B20 read...");
    
    if (!ds18b20_reset()) {
        ESP_LOGE(TAG, "DS18B20 device not found - no presence pulse");
        error_count++;  // Увеличиваем счетчик ошибок
        return DS18B20_DEVICE_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "DS18B20 responded with presence pulse");
    
    // Skip ROM command (assume single device on bus)
    ds18b20_write_byte(0xCC);
    // Convert temperature command
    ds18b20_write_byte(0x44);
    
    ESP_LOGI(TAG, "Conversion started, waiting 1s...");
    
    // Wait for conversion
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    if (!ds18b20_reset()) {
        ESP_LOGE(TAG, "DS18B20 device not found after conversion");
        error_count++;  // Увеличиваем счетчик ошибок
        return DS18B20_DEVICE_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "DS18B20 present for data read");
    
    // Skip ROM command
    ds18b20_write_byte(0xCC);
    // Read scratchpad command
    ds18b20_write_byte(0xBE);
    
    // Read scratchpad
    uint8_t scratchpad[9];
    ESP_LOGI(TAG, "Reading scratchpad...");
    for (int i = 0; i < 9; i++) {
        scratchpad[i] = ds18b20_read_byte();
        ESP_LOGI(TAG, "Scratchpad[%d] = 0x%02x", i, scratchpad[i]);
    }
    
    // Verify CRC
    uint8_t crc = ds18b20_crc8(scratchpad, 8);
    
    if (crc != scratchpad[8]) {
        ESP_LOGE(TAG, "CRC error: calculated=0x%02x, received=0x%02x", 
                crc, scratchpad[8]);
        error_count++;  // Увеличиваем счетчик ошибок
        return DS18B20_CRC_ERROR;
    }
    
    // Если дошли сюда - успешное чтение!
    error_count = 0;  // Сбрасываем счетчик ошибок
    
    ESP_LOGI(TAG, "CRC OK: 0x%02x", crc);
    
    // Convert temperature data
    int16_t temp_raw = (scratchpad[1] << 8) | scratchpad[0];
    ds_temperature = temp_raw / 16.0;
    
    ESP_LOGI(TAG, "Temperature read successfully: %.2f°C (raw: 0x%04x)", 
            ds_temperature, temp_raw);
    return DS18B20_OK;
}

// Read temperature and return it to be used on temperature variable node
float ReadDSTemperature(int gpio){
    float temperatureFromSensor;
    ESP_LOGI(TAG, "=== Reading DS18B20 on GPIO %d ===", gpio);
    
    int ret = readDS18B20();
    
    if (ret != DS18B20_OK){
        ESP_LOGE(TAG, "DS18B20 read error: %d, error_count: %d", ret, error_count);
        
        // Если 3 ошибки подряд - возвращаем -1000
        if (error_count >= 3) {
            ESP_LOGE(TAG, "3 consecutive errors - returning -1000");
            return -1000.0;
        }
        else {
            // Возвращаем последнее успешное значение
            return getDSTemperature();
        }
    }
    else {
        temperatureFromSensor = getDSTemperature();
        ESP_LOGI(TAG, "=== Temperature reading complete: %.2f°C ===", temperatureFromSensor);
        return temperatureFromSensor;
    }
}
