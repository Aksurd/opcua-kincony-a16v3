#include "opcua_esp32.h"
#include "model.h"
#include "io_cache.h"

#define EXAMPLE_ESP_MAXIMUM_RETRY 10

#define TAG "OPCUA_ESP32"
#define SNTP_TAG "SNTP"
#define MEMORY_TAG "MEMORY"
#define ENABLE_MDNS 1

static bool obtain_time(void);
static void initialize_sntp(void);

UA_ServerConfig *config;
static UA_Boolean sntp_initialized = false;
static UA_Boolean running = true;
static UA_Boolean isServerCreated = false;
RTC_DATA_ATTR static int boot_count = 0;
static struct tm timeinfo;
static time_t now = 0;

static UA_StatusCode
UA_ServerConfig_setUriName(UA_ServerConfig *uaServerConfig, const char *uri, const char *name)
{
    // delete pre-initialized values
    UA_String_clear(&uaServerConfig->applicationDescription.applicationUri);
    UA_LocalizedText_clear(&uaServerConfig->applicationDescription.applicationName);

    uaServerConfig->applicationDescription.applicationUri = UA_String_fromChars(uri);
    uaServerConfig->applicationDescription.applicationName.locale = UA_STRING_NULL;
    uaServerConfig->applicationDescription.applicationName.text = UA_String_fromChars(name);

    for (size_t i = 0; i < uaServerConfig->endpointsSize; i++)
    {
        UA_String_clear(&uaServerConfig->endpoints[i].server.applicationUri);
        UA_LocalizedText_clear(
            &uaServerConfig->endpoints[i].server.applicationName);

        UA_String_copy(&uaServerConfig->applicationDescription.applicationUri,
                       &uaServerConfig->endpoints[i].server.applicationUri);

        UA_LocalizedText_copy(&uaServerConfig->applicationDescription.applicationName,
                              &uaServerConfig->endpoints[i].server.applicationName);
    }

    return UA_STATUSCODE_GOOD;
}

static void opcua_task(void *arg)
{
    // BufferSize's got to be decreased due to latest refactorings in open62541 v1.2rc.
    UA_Int32 sendBufferSize = 16384;
    UA_Int32 recvBufferSize = 16384;

    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    ESP_LOGI(TAG, "Fire up OPC UA Server.");
    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);
    UA_ServerConfig_setMinimalCustomBuffer(config, 4840, 0, sendBufferSize, recvBufferSize);

    const char *appUri = "open62541.esp32.server";
    UA_String hostName = UA_STRING("opcua-esp32");
    
    UA_ServerConfig_setUriName(config, appUri, "OPC_UA_Server_ESP32");
    UA_ServerConfig_setCustomHostname(config, hostName);

    // Определяем Node IDs для всех переменных
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_NodeId variableTypeNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE);

    /* Добавляем диагностические переменные для тестирования производительности */

    // 1. Счётчик (пила) - только чтение
    UA_VariableAttributes counterAttr = UA_VariableAttributes_default;
    counterAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Diagnostic Counter");
    counterAttr.description = UA_LOCALIZEDTEXT("en-US", "Incremental counter for timing tests");
    counterAttr.dataType = UA_TYPES[UA_TYPES_UINT16].typeId;
    counterAttr.accessLevel = UA_ACCESSLEVELMASK_READ;

    UA_DataSource counterDataSource;
    counterDataSource.read = readDiagnosticCounter;
    counterDataSource.write = NULL;

    UA_NodeId counterNodeId = UA_NODEID_STRING(1, "diagnostic_counter");
    UA_QualifiedName counterName = UA_QUALIFIEDNAME(1, "Diagnostic Counter");

    UA_Server_addDataSourceVariableNode(server, counterNodeId, parentNodeId,
                                        parentReferenceNodeId, counterName,
                                        variableTypeNodeId, counterAttr,
                                        counterDataSource, NULL, NULL);

    // 2. Loopback Input (чтение/запись)
    UA_VariableAttributes loopbackInAttr = UA_VariableAttributes_default;
    loopbackInAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Loopback Input");
    loopbackInAttr.description = UA_LOCALIZEDTEXT("en-US", "Write value here, read from Loopback Output");
    loopbackInAttr.dataType = UA_TYPES[UA_TYPES_UINT16].typeId;
    loopbackInAttr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    UA_DataSource loopbackInDataSource;
    loopbackInDataSource.read = readLoopbackInput;
    loopbackInDataSource.write = writeLoopbackInput;

    UA_NodeId loopbackInNodeId = UA_NODEID_STRING(1, "loopback_input");
    UA_QualifiedName loopbackInName = UA_QUALIFIEDNAME(1, "Loopback Input");

    UA_Server_addDataSourceVariableNode(server, loopbackInNodeId, parentNodeId,
                                        parentReferenceNodeId, loopbackInName,
                                        variableTypeNodeId, loopbackInAttr,
                                        loopbackInDataSource, NULL, NULL);

    // 3. Loopback Output (только чтение)
    UA_VariableAttributes loopbackOutAttr = UA_VariableAttributes_default;
    loopbackOutAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Loopback Output");
    loopbackOutAttr.description = UA_LOCALIZEDTEXT("en-US", "Mirror of Loopback Input (read-only)");
    loopbackOutAttr.dataType = UA_TYPES[UA_TYPES_UINT16].typeId;
    loopbackOutAttr.accessLevel = UA_ACCESSLEVELMASK_READ;

    UA_DataSource loopbackOutDataSource;
    loopbackOutDataSource.read = readLoopbackOutput;
    loopbackOutDataSource.write = NULL;

    UA_NodeId loopbackOutNodeId = UA_NODEID_STRING(1, "loopback_output");
    UA_QualifiedName loopbackOutName = UA_QUALIFIEDNAME(1, "Loopback Output");

    UA_Server_addDataSourceVariableNode(server, loopbackOutNodeId, parentNodeId,
                                        parentReferenceNodeId, loopbackOutName,
                                        variableTypeNodeId, loopbackOutAttr,
                                        loopbackOutDataSource, NULL, NULL);

    /* Add Information Model Objects Here */
    addDSTemperatureDataSourceVariable(server);
    addDiscreteIOVariables(server);

    ESP_LOGI(TAG, "Heap Left : %d", xPortGetFreeHeapSize());
    UA_StatusCode retval = UA_Server_run_startup(server);
    if (retval == UA_STATUSCODE_GOOD)
    {
        while (running)
        {
            UA_Server_run_iterate(server, false);
            /* ОПТИМИЗАЦИЯ: было 100ms, теперь 1ms */
           // vTaskDelay(pdMS_TO_TICKS(1));
            ESP_ERROR_CHECK(esp_task_wdt_reset());
            taskYIELD();
        }
        UA_Server_run_shutdown(server);
    }
    ESP_ERROR_CHECK(esp_task_wdt_delete(NULL));
}

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(SNTP_TAG, "Notification of a time synchronization event");
}

static void initialize_sntp(void)
{
    ESP_LOGI(SNTP_TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_setservername(1, "time.google.com");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
    sntp_initialized = true;
}

static bool obtain_time(void)
{
    initialize_sntp();
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    memset(&timeinfo, 0, sizeof(struct tm));
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry <= retry_count)
    {
        ESP_LOGI(SNTP_TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        ESP_ERROR_CHECK(esp_task_wdt_reset());
    }
    time(&now);
    localtime_r(&now, &timeinfo);
    ESP_ERROR_CHECK(esp_task_wdt_delete(NULL));
    return timeinfo.tm_year > (2016 - 1900);
}

static void opc_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (sntp_initialized != true)
    {
        if (timeinfo.tm_year < (2016 - 1900))
        {
            ESP_LOGI(SNTP_TAG, "Time is not set yet. Settting up network connection and getting time over NTP.");
            if (!obtain_time())
            {
                ESP_LOGE(SNTP_TAG, "Could not get time from NTP. Using default timestamp.");
            }
            time(&now);
        }
        localtime_r(&now, &timeinfo);
        ESP_LOGI(SNTP_TAG, "Current time: %d-%02d-%02d %02d:%02d:%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }

    if (!isServerCreated)
    {
        xTaskCreatePinnedToCore(opcua_task, "opcua_task", 24336, NULL, 10, NULL, 1);
        ESP_LOGI(MEMORY_TAG, "Heap size after OPC UA Task : %d", esp_get_free_heap_size());
        isServerCreated = true;
    }
}

static void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
}

static void connection_scan(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, GOT_IP_EVENT, &opc_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(BASE_IP_EVENT, DISCONNECT_EVENT, &disconnect_handler, NULL));
    ESP_ERROR_CHECK(example_connect());
}

void app_main(void)
{
    ++boot_count;
    
    /* ИНИЦИАЛИЗАЦИЯ КЭША И ЗАДАЧИ ОПРОСА */
    ESP_LOGI(TAG, "Initializing IO cache system...");
    io_cache_init();
    io_polling_task_start();
    vTaskDelay(pdMS_TO_TICKS(100)); /* Дать время на первый опрос железа */
    
    // Workaround for CVE-2019-15894
    nvs_flash_init();
    if (esp_flash_encryption_enabled())
    {
        esp_flash_write_protect_crypt_cnt();
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    connection_scan();
}
