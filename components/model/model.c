#include "open62541.h"
#include "model.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "io_cache.h"
#include "pcf8574.h"
#include "esp_log.h"

static const char *TAG = "model";


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
    
    ESP_LOGI(TAG, "Model initialized with Discrete I/O");
}

/* ===== ПРОСТЫЕ БЫСТРЫЕ ФУНКЦИИ ===== */

uint16_t read_discrete_inputs_fast(void) {
    return io_cache_get_discrete_inputs(NULL, NULL);
}

uint16_t read_discrete_outputs_fast(void) {
    return io_cache_get_discrete_outputs(NULL, NULL);
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

// =================== ADC FUNCTIONS ===================

static uint16_t adc_cache[NUM_ADC_CHANNELS] = {0};
static adc_oneshot_unit_handle_t adc1_handle = NULL;
static bool adc_initialized = false;
static uint64_t adc_timestamps_ms[NUM_ADC_CHANNELS] = {0};
static uint64_t adc_server_timestamps_ms[NUM_ADC_CHANNELS] = {0};

// Инициализация ADC
void adc_init(void) {
    if (adc1_handle != NULL) {
        return;
    }
    
    // Конфигурация ADC unit
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));
    
    // Конфигурация каналов
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    
    // Конфигурируем 4 канала
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, OUR_ADC_CHANNEL_1, &config)); // GPIO4
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, OUR_ADC_CHANNEL_2, &config)); // GPIO6  
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, OUR_ADC_CHANNEL_3, &config)); // GPIO7
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, OUR_ADC_CHANNEL_4, &config)); // GPIO5
    
    adc_initialized = true;
    ESP_LOGI(TAG, "ADC initialized with oneshot driver (4 channels)");
}

// Чтение ADC канала
uint16_t read_adc_channel_slow(uint8_t channel) {
    if (adc1_handle == NULL || channel >= NUM_ADC_CHANNELS) {
        return 0;
    }
    
    adc_channel_t channel_id;
    switch(channel) {
        case 0: channel_id = OUR_ADC_CHANNEL_1; break;  // GPIO4
        case 1: channel_id = OUR_ADC_CHANNEL_2; break;  // GPIO6
        case 2: channel_id = OUR_ADC_CHANNEL_3; break;  // GPIO7
        case 3: channel_id = OUR_ADC_CHANNEL_4; break;  // GPIO5
        default: return 0;
    }
    
    int raw = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, channel_id, &raw));
    
    // Возвращаем сырое значение (0-4095)
    return (uint16_t)raw;
}

// Обновление всех каналов ADC
void update_all_adc_channels_slow(void) {
    if (adc1_handle == NULL) {
        adc_init();
        if (adc1_handle == NULL) {
            return;
        }
    }
    
    uint64_t timestamp = (uint64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    
    for (int i = 0; i < NUM_ADC_CHANNELS; i++) {
        uint16_t value = read_adc_channel_slow(i);
        adc_cache[i] = value;
        adc_timestamps_ms[i] = timestamp;
        adc_server_timestamps_ms[i] = timestamp;
        
        // Также обновляем глобальный кэш
        io_cache_update_adc_channel(i, (float)value, timestamp);
    }
}

// Быстрое чтение из кэша
uint16_t read_adc_channel_fast(uint8_t channel) {
    if (channel >= NUM_ADC_CHANNELS) {
        return 0;
    }
    return adc_cache[channel];
}

// Получение всех значений ADC
uint16_t* get_all_adc_channels_fast(void) {
    return adc_cache;
}

// =================== OPC UA FUNCTIONS FOR ADC ===================

UA_StatusCode readAdcChannel(UA_Server *server,
                           const UA_NodeId *sessionId, void *sessionContext,
                           const UA_NodeId *nodeId, void *nodeContext,
                           UA_Boolean sourceTimeStamp, const UA_NumericRange *range,
                           UA_DataValue *dataValue) {
    uint8_t channel = (uintptr_t)nodeContext;
    
    if (channel >= NUM_ADC_CHANNELS) {
        return UA_STATUSCODE_BADINTERNALERROR;
    }
    
    uint16_t value = adc_cache[channel];
    UA_Variant_setScalarCopy(&dataValue->value, &value, &UA_TYPES[UA_TYPES_UINT16]);
    
    if (sourceTimeStamp && adc_timestamps_ms[channel] > 0) {
        dataValue->sourceTimestamp = UA_DateTime_fromUnixTime((UA_Int64)(adc_timestamps_ms[channel] / 1000));
    }
    
    dataValue->hasValue = true;
    return UA_STATUSCODE_GOOD;
}

// Добавление ADC переменных в OPC UA сервер
void addAdcVariables(UA_Server *server) {
    char* channel_names[] = {"ADC1", "ADC2", "ADC3", "ADC4"};
    char* descriptions[] = {
        "Analog Input 1 (GPIO4) - Raw ADC code",
        "Analog Input 2 (GPIO6) - Raw ADC code", 
        "Analog Input 3 (GPIO7) - Raw ADC code",
        "Analog Input 4 (GPIO5) - Raw ADC code"
    };
    
    for (int i = 0; i < NUM_ADC_CHANNELS; i++) {
        UA_VariableAttributes attr = UA_VariableAttributes_default;
        attr.displayName = UA_LOCALIZEDTEXT("en-US", channel_names[i]);
        attr.description = UA_LOCALIZEDTEXT("en-US", descriptions[i]);
        attr.dataType = UA_TYPES[UA_TYPES_UINT16].typeId;
        attr.accessLevel = UA_ACCESSLEVELMASK_READ;
        
        UA_DataSource dataSource;
        dataSource.read = readAdcChannel;
        dataSource.write = NULL;
        
        char nodeIdStr[32];
        snprintf(nodeIdStr, sizeof(nodeIdStr), "adc_channel_%d", i + 1);
        
        UA_NodeId nodeId = UA_NODEID_STRING(1, nodeIdStr);
        UA_QualifiedName name = UA_QUALIFIEDNAME(1, channel_names[i]);
        UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
        UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
        UA_NodeId variableTypeNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE);
        
        UA_Server_addDataSourceVariableNode(server, nodeId, parentNodeId,
                                          parentReferenceNodeId, name,
                                          variableTypeNodeId, attr,
                                          dataSource, (void*)(uintptr_t)i, NULL);
    }
    
    ESP_LOGI(TAG, "ADC variables added to OPC UA server (%d channels, raw codes)", NUM_ADC_CHANNELS);
}
