#include "pcf8574.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "PCF8574";

static bool i2c_initialized = false;
static i2c_port_t current_i2c_port = I2C_NUM_0;

// Инициализация шины I2C
bool pcf8574_i2c_init(const pcf8574_config_t *config) {
    if (config == NULL) {
        ESP_LOGE(TAG, "Config is NULL");
        return false;
    }
    
    if (i2c_initialized && current_i2c_port == config->i2c_port) {
        return true; // Уже инициализирован
    }
    
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = config->sda_pin,
        .scl_io_num = config->scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = config->clk_speed,
    };
    
    esp_err_t err = i2c_param_config(config->i2c_port, &i2c_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(err));
        return false;
    }
    
    err = i2c_driver_install(config->i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(err));
        return false;
    }
    
    i2c_initialized = true;
    current_i2c_port = config->i2c_port;
    ESP_LOGI(TAG, "I2C initialized on port %d, SDA=%d, SCL=%d, speed=%lu",
             config->i2c_port, config->sda_pin, config->scl_pin, config->clk_speed);
    
    return true;
}

// Инициализация устройства PCF8574
void pcf8574_init(pcf8574_dev_t *dev, uint8_t address, i2c_port_t i2c_port) {
    if (dev == NULL) {
        ESP_LOGE(TAG, "Device descriptor is NULL");
        return;
    }
    
    dev->address = address;
    dev->i2c_port = i2c_port;
    
    ESP_LOGI(TAG, "PCF8574 device initialized at address 0x%02X on port %d", 
             address, i2c_port);
}

// Чтение байта с PCF8574
uint8_t pcf8574_read(const pcf8574_dev_t *dev) {
    if (dev == NULL) {
        ESP_LOGE(TAG, "Device descriptor is NULL");
        return 0xFF;
    }
    
    uint8_t data = 0xFF;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->address << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(dev->i2c_port, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read failed from 0x%02X: %s", dev->address, esp_err_to_name(ret));
        return 0xFF;
    }
    
    return data;
}

// Запись байта в PCF8574
bool pcf8574_write(const pcf8574_dev_t *dev, uint8_t data) {
    if (dev == NULL) {
        ESP_LOGE(TAG, "Device descriptor is NULL");
        return false;
    }
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(dev->i2c_port, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write failed to 0x%02X: %s", dev->address, esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

// Установка бита выхода
bool pcf8574_set_bit(const pcf8574_dev_t *dev, uint8_t bit, bool value) {
    if (dev == NULL || bit > 7) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }
    
    // Сначала читаем текущее состояние
    uint8_t current = pcf8574_read(dev);
    if (current == 0xFF) {
        return false; // Ошибка чтения
    }
    
    // Модифицируем нужный бит
    if (value) {
        current |= (1 << bit);   // Установить бит
    } else {
        current &= ~(1 << bit);  // Сбросить бит
    }
    
    // Записываем обратно
    return pcf8574_write(dev, current);
}

// Чтение бита входа
bool pcf8574_get_bit(const pcf8574_dev_t *dev, uint8_t bit) {
    if (dev == NULL || bit > 7) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }
    
    uint8_t data = pcf8574_read(dev);
    if (data == 0xFF) {
        return false; // Ошибка чтения
    }
    
    return (data >> bit) & 0x01;
}
