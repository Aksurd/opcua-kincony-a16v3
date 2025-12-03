#include "open62541.h"
#include "model.h"
#include "driver/gpio.h"
#include "ds18b20.h"
#include "io_cache.h"
#include "pcf8574.h"
#include "esp_log.h"

static const char *TAG = "model";

/* DS18B20 Temperature */

UA_StatusCode
readDSTemperature(UA_Server *server,
                const UA_NodeId *sessionId, void *sessionContext,
                const UA_NodeId *nodeId, void *nodeContext,
                UA_Boolean sourceTimeStamp, const UA_NumericRange *range,
                UA_DataValue *dataValue) {
    // Используем кеш вместо прямого чтения
    float temp_cached = 0.0f;
    uint64_t source_ts = 0, server_ts = 0;
    
    if (io_cache_get_temperature(0, &temp_cached, &source_ts, &server_ts)) {
        // Данные из кеша (<1 мс)
        UA_Float temperature = (UA_Float)temp_cached;
        UA_Variant_setScalarCopy(&dataValue->value, &temperature,
                                 &UA_TYPES[UA_TYPES_FLOAT]);
        
        // Устанавливаем временные метки если запрошено
        if (sourceTimeStamp && source_ts > 0) {
            dataValue->sourceTimestamp = UA_DateTime_fromUnixTime((UA_Int64)(source_ts / 1000));
        }
        
        ESP_LOGD(TAG, "Temperature from cache: %.2f°C (source ts: %llu)", 
                 temp_cached, source_ts);
    } else {
        // Fallback: прямое чтение (медленно)
        UA_Float temperature = (UA_Float)ReadDSTemperature(DS18B20_GPIO);
        UA_Variant_setScalarCopy(&dataValue->value, &temperature,
                                 &UA_TYPES[UA_TYPES_FLOAT]);
        ESP_LOGW(TAG, "Cache miss for temperature, using direct read: %.2f°C", 
                 (float)temperature);
    }
    
    dataValue->hasValue = true;
    return UA_STATUSCODE_GOOD;
}


void
addDSTemperatureDataSourceVariable(UA_Server *server) {
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Temperature");
    attr.description = UA_LOCALIZEDTEXT("en-US", "Ambient temperature with caching");
    attr.dataType = UA_TYPES[UA_TYPES_FLOAT].typeId;
    attr.accessLevel = UA_ACCESSLEVELMASK_READ;

    UA_NodeId currentNodeId = UA_NODEID_STRING(1, "temperature");
    UA_QualifiedName currentName = UA_QUALIFIEDNAME(1, "Ambient Temperature");
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_NodeId variableTypeNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE);

    UA_DataSource timeDataSource;
    timeDataSource.read = readDSTemperature;
    timeDataSource.write = NULL;
    UA_Server_addDataSourceVariableNode(server, currentNodeId, parentNodeId,
                                        parentReferenceNodeId, currentName,
                                        variableTypeNodeId, attr,
                                        timeDataSource, NULL, NULL);
}

// =================== DISCRETE I/O FUNCTIONS ===================

// Дескрипторы устройств PCF8574
static pcf8574_dev_t dio_in1, dio_in2, dio_out1, dio_out2;
static bool dio_initialized = false;

// Инициализация дискретных I/O
void discrete_io_init(void) {
    if (dio_initialized) {
        return;
    }
    
    // Конфигурация I2C
    pcf8574_config_t i2c_config = {
        .i2c_port = I2C_NUM_0,
        .sda_pin = 9,
        .scl_pin = 10,
        .clk_speed = 400000
    };
    
    if (!pcf8574_i2c_init(&i2c_config)) {
        ESP_LOGE(TAG, "Failed to initialize I2C for discrete I/O");
        return;
    }
    
    // Инициализация устройств
    pcf8574_init(&dio_in1, DIO_IN1_ADDR, I2C_NUM_0);
    pcf8574_init(&dio_in2, DIO_IN2_ADDR, I2C_NUM_0);
    pcf8574_init(&dio_out1, DIO_OUT1_ADDR, I2C_NUM_0);
    pcf8574_init(&dio_out2, DIO_OUT2_ADDR, I2C_NUM_0);
    
    // Инициализация выходов в безопасное состояние (все выключены)
    pcf8574_write(&dio_out1, 0xFF); // Все биты = 1 (выключено)
    pcf8574_write(&dio_out2, 0xFF);
    
    dio_initialized = true;
    ESP_LOGI(TAG, "Discrete I/O initialized");
}

// Чтение 16 дискретных входов (медленная функция для обновления кеша)
uint16_t read_discrete_inputs_slow(void) {
    // Ленивая инициализация при первом вызове
    if (!dio_initialized) {
        ESP_LOGI(TAG, "First call to discrete I/O - initializing...");
        discrete_io_init();
        if (!dio_initialized) {
            ESP_LOGE(TAG, "Failed to initialize discrete I/O");
            return 0xFFFF;
        }
    }
    
    uint8_t in1 = pcf8574_read(&dio_in1);
    uint8_t in2 = pcf8574_read(&dio_in2);
    
    // Инвертируем: PCF8574: 0=сигнал есть, 1=нет -> делаем 1=сигнал есть
    in1 = ~in1;
    in2 = ~in2;
    
    uint16_t inputs = ((uint16_t)in2 << 8) | in1;
    ESP_LOGD(TAG, "Direct read inputs: 0x%04X", inputs);
    return inputs;
}

// Запись 16 дискретных выходов
void write_discrete_outputs_slow(uint16_t outputs) {
    // Ленивая инициализация при первом вызове
    if (!dio_initialized) {
        ESP_LOGI(TAG, "First call to discrete I/O - initializing...");
        discrete_io_init();
        if (!dio_initialized) {
            ESP_LOGE(TAG, "Failed to initialize discrete I/O");
            return;
        }
    }
    
    uint8_t out1 = outputs & 0xFF;
    uint8_t out2 = (outputs >> 8) & 0xFF;
    
    // Инвертируем: 1 в бите = включить выход -> PCF8574: 0=включить
    out1 = ~out1;
    out2 = ~out2;
    
    pcf8574_write(&dio_out1, out1);
    pcf8574_write(&dio_out2, out2);
    
    ESP_LOGD(TAG, "Direct write outputs: 0x%04X", outputs);
}

// =================== OPC UA FUNCTIONS FOR DISCRETE I/O ===================

// Функция чтения дискретных входов для OPC UA (с использованием кеша)
UA_StatusCode
readDiscreteInputs(UA_Server *server,
                  const UA_NodeId *sessionId, void *sessionContext,
                  const UA_NodeId *nodeId, void *nodeContext,
                  UA_Boolean sourceTimeStamp, const UA_NumericRange *range,
                  UA_DataValue *dataValue) {
    // Используем кеш вместо прямого чтения
    uint64_t source_ts = 0, server_ts = 0;
    UA_UInt16 inputs = io_cache_get_discrete_inputs(&source_ts, &server_ts);
    
    UA_Variant_setScalarCopy(&dataValue->value, &inputs,
                           &UA_TYPES[UA_TYPES_UINT16]);
    
    // Устанавливаем временные метки если запрошено
    if (sourceTimeStamp && source_ts > 0) {
        dataValue->sourceTimestamp = UA_DateTime_fromUnixTime((UA_Int64)(source_ts / 1000));
    }
    
    dataValue->hasValue = true;
    ESP_LOGD(TAG, "Inputs from cache: 0x%04X (source ts: %llu)", inputs, source_ts);
    return UA_STATUSCODE_GOOD;
}

// Функция чтения дискретных выходов для OPC UA (с использованием кеша)
UA_StatusCode
readDiscreteOutputs(UA_Server *server,
                   const UA_NodeId *sessionId, void *sessionContext,
                   const UA_NodeId *nodeId, void *nodeContext,
                   UA_Boolean sourceTimeStamp, const UA_NumericRange *range,
                   UA_DataValue *dataValue) {
    // Используем кеш вместо прямого чтения
    uint64_t source_ts = 0, server_ts = 0;
    UA_UInt16 outputs = io_cache_get_discrete_outputs(&source_ts, &server_ts);
    
    UA_Variant_setScalarCopy(&dataValue->value, &outputs,
                           &UA_TYPES[UA_TYPES_UINT16]);
    
    // Устанавливаем временные метки если запрошено
    if (sourceTimeStamp && source_ts > 0) {
        dataValue->sourceTimestamp = UA_DateTime_fromUnixTime((UA_Int64)(source_ts / 1000));
    }
    
    dataValue->hasValue = true;
    ESP_LOGD(TAG, "Outputs from cache: 0x%04X (source ts: %llu)", outputs, source_ts);
    return UA_STATUSCODE_GOOD;
}

// Функция записи дискретных выходов для OPC UA (обновляем кеш и физическое устройство)
UA_StatusCode
writeDiscreteOutputs(UA_Server *server,
                    const UA_NodeId *sessionId, void *sessionContext,
                    const UA_NodeId *nodeId, void *nodeContext,
                    const UA_NumericRange *range, const UA_DataValue *data) {
    if (data->hasValue && UA_Variant_isScalar(&data->value) &&
        data->value.type == &UA_TYPES[UA_TYPES_UINT16]) {
        UA_UInt16 outputs = *(UA_UInt16*)data->value.data;
        
        // 1. Обновляем физическое устройство (медленно)
        write_discrete_outputs_slow((uint16_t)outputs);
        
        // 2. Обновляем кеш с текущей временной меткой
        uint64_t timestamp_ms = (uint64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        io_cache_update_discrete_outputs((uint16_t)outputs, timestamp_ms);
        
        ESP_LOGD(TAG, "Outputs written: 0x%04X (ts: %llu)", (uint16_t)outputs, timestamp_ms);
        return UA_STATUSCODE_GOOD;
    }
    return UA_STATUSCODE_BADTYPEMISMATCH;
}

// Добавление переменных дискретных I/O в OPC UA сервер
void
addDiscreteIOVariables(UA_Server *server) {
    // 1. Переменная для ЧТЕНИЯ входов (только чтение)
    UA_VariableAttributes inputAttr = UA_VariableAttributes_default;
    inputAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Discrete Inputs");
    inputAttr.description = UA_LOCALIZEDTEXT("en-US", "16 discrete inputs with caching");
    inputAttr.dataType = UA_TYPES[UA_TYPES_UINT16].typeId;
    inputAttr.accessLevel = UA_ACCESSLEVELMASK_READ;
    
    UA_DataSource inputDataSource;
    inputDataSource.read = readDiscreteInputs;
    inputDataSource.write = NULL;
    
    UA_NodeId inputNodeId = UA_NODEID_STRING(1, "discrete_inputs");
    UA_QualifiedName inputName = UA_QUALIFIEDNAME(1, "Discrete Inputs");
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_NodeId variableTypeNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE);
    
    UA_Server_addDataSourceVariableNode(server, inputNodeId, parentNodeId,
                                        parentReferenceNodeId, inputName,
                                        variableTypeNodeId, inputAttr,
                                        inputDataSource, NULL, NULL);
    
    // 2. Переменная для ВЫХОДОВ (чтение/запись)
    UA_VariableAttributes outputAttr = UA_VariableAttributes_default;
    outputAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Discrete Outputs");
    outputAttr.description = UA_LOCALIZEDTEXT("en-US", "16 discrete outputs with caching");
    outputAttr.dataType = UA_TYPES[UA_TYPES_UINT16].typeId;
    outputAttr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
    
    UA_DataSource outputDataSource;
    outputDataSource.read = readDiscreteOutputs;
    outputDataSource.write = writeDiscreteOutputs;
    
    UA_NodeId outputNodeId = UA_NODEID_STRING(1, "discrete_outputs");
    UA_QualifiedName outputName = UA_QUALIFIEDNAME(1, "Discrete Outputs");
    
    UA_Server_addDataSourceVariableNode(server, outputNodeId, parentNodeId,
                                        parentReferenceNodeId, outputName,
                                        variableTypeNodeId, outputAttr,
                                        outputDataSource, NULL, NULL);
    
    ESP_LOGI(TAG, "Discrete I/O variables added to OPC UA server (with caching)");
}

// =================== MAIN INIT FUNCTION ===================

void model_init_task(void) {
    // Инициализация дискретных I/O
    discrete_io_init();
    
    // Инициализация DS18B20
    ds18b20_init_task();
    
    ESP_LOGI(TAG, "Model initialized with DS18B20 and Discrete I/O");
}

// ===== ФУНКЦИИ С ВРЕМЕННЫМИ МЕТКАМИ ДЛЯ OPC UA =====

uint16_t read_discrete_inputs_with_timestamps(uint64_t *source_ts, uint64_t *server_ts) {
    return io_cache_get_discrete_inputs(source_ts, server_ts);
}

uint16_t read_discrete_outputs_with_timestamps(uint64_t *source_ts, uint64_t *server_ts) {
    return io_cache_get_discrete_outputs(source_ts, server_ts);
}

float read_temperature_with_timestamps(uint64_t *source_ts, uint64_t *server_ts) {
    float temp = 0.0f;
    if (io_cache_get_temperature(0, &temp, source_ts, server_ts)) {
        return temp;
    }
    return -100.0f; // Значение ошибки
}

/* ===== ПРОСТЫЕ БЫСТРЫЕ ФУНКЦИИ ===== */

uint16_t read_discrete_inputs_fast(void) {
    return io_cache_get_discrete_inputs(NULL, NULL);
}

uint16_t read_discrete_outputs_fast(void) {
    return io_cache_get_discrete_outputs(NULL, NULL);
}

float read_temperature_fast(void) {
    float temp = 0.0f;
    if (io_cache_get_temperature(0, &temp, NULL, NULL)) {
        return temp;
    }
    return -100.0f;
}

/* ===== ДИАГНОСТИЧЕСКИЕ ТЕГИ ДЛЯ ИЗМЕРЕНИЯ ПРОИЗВОДИТЕЛЬНОСТИ ===== */

static uint16_t diagnostic_counter = 0;
static uint16_t loopback_input = 0;
static uint16_t loopback_output = 0;

uint16_t get_diagnostic_counter(void) {
    diagnostic_counter++;
    return diagnostic_counter;
}

uint16_t get_loopback_input(void) {
    return loopback_input;
}

void set_loopback_input(uint16_t val) {
    loopback_input = val;
    loopback_output = val;  /* Мгновенный loopback */
}

uint16_t get_loopback_output(void) {
    return loopback_output;
}

/* ===== OPC UA CALLBACKS ДЛЯ ДИАГНОСТИЧЕСКИХ ТЕГОВ ===== */

UA_StatusCode
readDiagnosticCounter(UA_Server *server,
                     const UA_NodeId *sessionId, void *sessionContext,
                     const UA_NodeId *nodeId, void *nodeContext,
                     UA_Boolean sourceTimeStamp, const UA_NumericRange *range,
                     UA_DataValue *dataValue) {
    diagnostic_counter++;
    UA_UInt16 counter = diagnostic_counter;
    UA_Variant_setScalarCopy(&dataValue->value, &counter,
                           &UA_TYPES[UA_TYPES_UINT16]);
    dataValue->hasValue = true;
    dataValue->sourceTimestamp = UA_DateTime_now();
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
readLoopbackInput(UA_Server *server,
                  const UA_NodeId *sessionId, void *sessionContext,
                  const UA_NodeId *nodeId, void *nodeContext,
                  UA_Boolean sourceTimeStamp, const UA_NumericRange *range,
                  UA_DataValue *dataValue) {
    UA_UInt16 value = loopback_input;
    UA_Variant_setScalarCopy(&dataValue->value, &value,
                           &UA_TYPES[UA_TYPES_UINT16]);
    dataValue->hasValue = true;
    dataValue->sourceTimestamp = UA_DateTime_now();
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
writeLoopbackInput(UA_Server *server,
                   const UA_NodeId *sessionId, void *sessionContext,
                   const UA_NodeId *nodeId, void *nodeContext,
                   const UA_NumericRange *range, const UA_DataValue *data) {
    if (data->hasValue && UA_Variant_isScalar(&data->value) &&
        data->value.type == &UA_TYPES[UA_TYPES_UINT16]) {
        UA_UInt16 value = *(UA_UInt16*)data->value.data;
        loopback_input = (uint16_t)value;
        loopback_output = (uint16_t)value;  /* Мгновенный loopback */
        return UA_STATUSCODE_GOOD;
    }
    return UA_STATUSCODE_BADTYPEMISMATCH;
}

UA_StatusCode
readLoopbackOutput(UA_Server *server,
                   const UA_NodeId *sessionId, void *sessionContext,
                   const UA_NodeId *nodeId, void *nodeContext,
                   UA_Boolean sourceTimeStamp, const UA_NumericRange *range,
                   UA_DataValue *dataValue) {
    UA_UInt16 value = loopback_output;
    UA_Variant_setScalarCopy(&dataValue->value, &value,
                           &UA_TYPES[UA_TYPES_UINT16]);
    dataValue->hasValue = true;
    dataValue->sourceTimestamp = UA_DateTime_now();
    return UA_STATUSCODE_GOOD;
}

/* ===== ЗАДАЧА ДЛЯ ОБНОВЛЕНИЯ КЕША ===== */

void io_polling_task(void *pvParameters) {
    ESP_LOGI(TAG, "IO Polling Task started");
    
    TickType_t last_temp_update = 0;
    const TickType_t temp_interval_ticks = pdMS_TO_TICKS(1000);  // 1 секунда
    const TickType_t inputs_interval_ticks = pdMS_TO_TICKS(20);   // 20 мс
    
    while (1) {
        TickType_t current_tick = xTaskGetTickCount();
        uint64_t timestamp_ms = (uint64_t)(current_tick * portTICK_PERIOD_MS);
        
        // Обновление температуры (раз в секунду)
        if (current_tick - last_temp_update >= temp_interval_ticks) {
            float temp = ReadDSTemperature(DS18B20_GPIO);  // медленное чтение
            io_cache_update_temperature(0, temp, timestamp_ms);
            last_temp_update = current_tick;
            ESP_LOGD(TAG, "Temperature cache updated: %.2f°C", temp);
        }
        
        // Обновление дискретных входов (каждые 20 мс)
        static TickType_t last_inputs_update = 0;
        if (current_tick - last_inputs_update >= inputs_interval_ticks) {
            uint16_t inputs = read_discrete_inputs_slow();  // медленное чтение
            io_cache_update_discrete_inputs(inputs, timestamp_ms);
            last_inputs_update = current_tick;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));  // Базовая задержка цикла
    }
}
