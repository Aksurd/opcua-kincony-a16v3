#include "open62541.h"

/* GPIO Numbers */
// #define BLINK_GPIO 2
//#define DHT22_GPIO 4
//#define RELAY_0_GPIO 32
//#define RELAY_1_GPIO 33
// #define RELAY_2_GPIO 26
// #define RELAY_3_GPIO 27

/* DS18B20 Temperature */
#define DS18B20_GPIO 47

/* PCF8574 Addresses for KC868-A16v3 */
#define DIO_IN1_ADDR  0x22  // Input module 1
#define DIO_IN2_ADDR  0x21  // Input module 2
#define DIO_OUT1_ADDR 0x24  // Relay/output module 1
#define DIO_OUT2_ADDR 0x25  // Relay/output module 2

// Явно включаем заголовочные файлы библиотек
#include "ds18b20.h"

// Функции для работы с DS18B20
void ds18b20_init_task(void);
float read_ds18b20_temperature(void);

UA_StatusCode
readDSTemperature(UA_Server *server,
                const UA_NodeId *sessionId, void *sessionContext,
                const UA_NodeId *nodeId, void *nodeContext,
                UA_Boolean sourceTimeStamp, const UA_NumericRange *range,
                UA_DataValue *dataValue);

void
addDSTemperatureDataSourceVariable(UA_Server *server);

// Функции для дискретных I/O
void discrete_io_init(void);
uint16_t read_discrete_inputs(void);
void write_discrete_outputs(uint16_t outputs);
uint16_t get_current_outputs(void);

UA_StatusCode
readDiscreteInputs(UA_Server *server,
                  const UA_NodeId *sessionId, void *sessionContext,
                  const UA_NodeId *nodeId, void *nodeContext,
                  UA_Boolean sourceTimeStamp, const UA_NumericRange *range,
                  UA_DataValue *dataValue);

UA_StatusCode
readDiscreteOutputs(UA_Server *server,
                   const UA_NodeId *sessionId, void *sessionContext,
                   const UA_NodeId *nodeId, void *nodeContext,
                   UA_Boolean sourceTimeStamp, const UA_NumericRange *range,
                   UA_DataValue *dataValue);

UA_StatusCode
writeDiscreteOutputs(UA_Server *server,
                    const UA_NodeId *sessionId, void *sessionContext,
                    const UA_NodeId *nodeId, void *nodeContext,
                    const UA_NumericRange *range, const UA_DataValue *data);

void addDiscreteIOVariables(UA_Server *server);
void model_init_task(void);
