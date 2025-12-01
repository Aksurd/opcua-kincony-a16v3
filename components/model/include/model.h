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
