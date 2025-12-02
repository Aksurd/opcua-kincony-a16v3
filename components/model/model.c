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
    UA_Float temperature = ReadDSTemperature(DS18B20_GPIO);
    UA_Variant_setScalarCopy(&dataValue->value, &temperature,
                             &UA_TYPES[UA_TYPES_FLOAT]);
    dataValue->hasValue = true;
    return UA_STATUSCODE_GOOD;
}


void
addDSTemperatureDataSourceVariable(UA_Server *server) {
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Temperature");
    attr.dataType = UA_TYPES[UA_TYPES_FLOAT].typeId;
    attr.accessLevel = UA_ACCESSLEVELMASK_READ;

    UA_NodeId currentNodeId = UA_NODEID_STRING(1, "temperature");
    UA_QualifiedName currentName = UA_QUALIFIEDNAME(1, "Ambient Temperature");
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_NodeId variableTypeNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE);

    UA_DataSource timeDataSource;
    timeDataSource.read = readDSTemperature;
    UA_Server_addDataSourceVariableNode(server, currentNodeId, parentNodeId,
                                        parentReferenceNodeId, currentName,
                                        variableTypeNodeId, attr,
                                        timeDataSource, NULL, NULL);
}

// =================== DISCRETE I/O FUNCTIONS ===================

// Дескрипторы устройств PCF8574
static pcf8574_dev_t dio_in1, dio_in2, dio_out1, dio_out2;
static uint16_t current_inputs = 0;
static uint16_t current_outputs = 0;
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

// Чтение 16 дискретных входов
uint16_t read_discrete_inputs(void) {
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
    
    current_inputs = ((uint16_t)in2 << 8) | in1;
    return current_inputs;
}

// Запись 16 дискретных выходов
void write_discrete_outputs(uint16_t outputs) {
    // Ленивая инициализация при первом вызове
    if (!dio_initialized) {
        ESP_LOGI(TAG, "First call to discrete I/O - initializing...");
        discrete_io_init();
        if (!dio_initialized) {
            ESP_LOGE(TAG, "Failed to initialize discrete I/O");
            return;
        }
    }
    
    current_outputs = outputs;
    
    uint8_t out1 = outputs & 0xFF;
    uint8_t out2 = (outputs >> 8) & 0xFF;
    
    // Инвертируем: 1 в бите = включить выход -> PCF8574: 0=включить
    out1 = ~out1;
    out2 = ~out2;
    
    pcf8574_write(&dio_out1, out1);
    pcf8574_write(&dio_out2, out2);
}

// Чтение текущих выходов
uint16_t get_current_outputs(void) {
    // Ленивая инициализация при первом вызове
    if (!dio_initialized) {
        ESP_LOGI(TAG, "First call to discrete I/O - initializing...");
        discrete_io_init();
        if (!dio_initialized) {
            ESP_LOGE(TAG, "Failed to initialize discrete I/O");
            return 0xFFFF;
        }
    }
    return current_outputs;
}

// =================== OPC UA FUNCTIONS FOR DISCRETE I/O ===================

// Функция чтения дискретных входов для OPC UA
UA_StatusCode
readDiscreteInputs(UA_Server *server,
                  const UA_NodeId *sessionId, void *sessionContext,
                  const UA_NodeId *nodeId, void *nodeContext,
                  UA_Boolean sourceTimeStamp, const UA_NumericRange *range,
                  UA_DataValue *dataValue) {
    UA_UInt16 inputs = read_discrete_inputs();
    UA_Variant_setScalarCopy(&dataValue->value, &inputs,
                           &UA_TYPES[UA_TYPES_UINT16]);
    dataValue->hasValue = true;
    return UA_STATUSCODE_GOOD;
}

// Функция чтения дискретных выходов для OPC UA
UA_StatusCode
readDiscreteOutputs(UA_Server *server,
                   const UA_NodeId *sessionId, void *sessionContext,
                   const UA_NodeId *nodeId, void *nodeContext,
                   UA_Boolean sourceTimeStamp, const UA_NumericRange *range,
                   UA_DataValue *dataValue) {
    UA_UInt16 outputs = get_current_outputs();
    UA_Variant_setScalarCopy(&dataValue->value, &outputs,
                           &UA_TYPES[UA_TYPES_UINT16]);
    dataValue->hasValue = true;
    return UA_STATUSCODE_GOOD;
}

// Функция записи дискретных выходов для OPC UA
UA_StatusCode
writeDiscreteOutputs(UA_Server *server,
                    const UA_NodeId *sessionId, void *sessionContext,
                    const UA_NodeId *nodeId, void *nodeContext,
                    const UA_NumericRange *range, const UA_DataValue *data) {
    if (data->hasValue && UA_Variant_isScalar(&data->value) &&
        data->value.type == &UA_TYPES[UA_TYPES_UINT16]) {
        UA_UInt16 outputs = *(UA_UInt16*)data->value.data;
        write_discrete_outputs((uint16_t)outputs);
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
    inputAttr.description = UA_LOCALIZEDTEXT("en-US", "16 discrete inputs (read-only)");
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
    outputAttr.description = UA_LOCALIZEDTEXT("en-US", "16 discrete outputs (read/write)");
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
    
    ESP_LOGI(TAG, "Discrete I/O variables added to OPC UA server");
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

/* ===== БЫСТРЫЕ ФУНКЦИИ ===== */

uint16_t read_discrete_inputs_fast(void) {
    uint64_t source_ts = 0, server_ts = 0;
    return io_cache_get_discrete_inputs(&source_ts, &server_ts);
}

uint16_t read_discrete_outputs_fast(void) {
    uint64_t source_ts = 0, server_ts = 0;
    return io_cache_get_discrete_outputs(&source_ts, &server_ts);
}

float read_temperature_fast(void) {
    uint64_t source_ts = 0, server_ts = 0;
    float temp = 0.0f;
    if (io_cache_get_temperature(0, &temp, &source_ts, &server_ts)) {
        return temp;
    }
    return -100.0f;
}
