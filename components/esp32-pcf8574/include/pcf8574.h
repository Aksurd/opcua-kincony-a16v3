#ifndef PCF8574_H
#define PCF8574_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

// Конфигурация I2C для PCF8574
typedef struct {
    i2c_port_t i2c_port;      // Порт I2C (обычно I2C_NUM_0)
    int sda_pin;              // GPIO для SDA
    int scl_pin;              // GPIO для SCL
    uint32_t clk_speed;       // Скорость I2C (обычно 100000 или 400000)
} pcf8574_config_t;

// Дескриптор устройства PCF8574
typedef struct {
    uint8_t address;          // Адрес устройства (0x20-0x27)
    i2c_port_t i2c_port;      // Порт I2C
} pcf8574_dev_t;

/**
 * @brief Инициализация шины I2C
 * 
 * @param config Конфигурация I2C
 * @return true Успешная инициализация
 * @return false Ошибка инициализации
 */
bool pcf8574_i2c_init(const pcf8574_config_t *config);

/**
 * @brief Инициализация устройства PCF8574
 * 
 * @param dev Дескриптор устройства
 * @param address Адрес устройства (0x20-0x27)
 * @param i2c_port Порт I2C
 */
void pcf8574_init(pcf8574_dev_t *dev, uint8_t address, i2c_port_t i2c_port);

/**
 * @brief Чтение байта с PCF8574
 * 
 * @param dev Дескриптор устройства
 * @return uint8_t Прочитанный байт (0xFF при ошибке)
 */
uint8_t pcf8574_read(const pcf8574_dev_t *dev);

/**
 * @brief Запись байта в PCF8574
 * 
 * @param dev Дескриптор устройства
 * @param data Байт для записи
 * @return true Успешная запись
 * @return false Ошибка записи
 */
bool pcf8574_write(const pcf8574_dev_t *dev, uint8_t data);

/**
 * @brief Установка бита выхода
 * 
 * @param dev Дескриптор устройства
 * @param bit Номер бита (0-7)
 * @param value Значение (true = 1, false = 0)
 * @return true Успешно
 * @return false Ошибка
 */
bool pcf8574_set_bit(const pcf8574_dev_t *dev, uint8_t bit, bool value);

/**
 * @brief Чтение бита входа
 * 
 * @param dev Дескриптор устройства
 * @param bit Номер бита (0-7)
 * @return true Бит установлен (1)
 * @return false Бит сброшен (0) или ошибка
 */
bool pcf8574_get_bit(const pcf8574_dev_t *dev, uint8_t bit);

#ifdef __cplusplus
}
#endif

#endif // PCF8574_H
