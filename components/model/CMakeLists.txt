idf_component_register(SRCS "model.c" "ds18b20.c"
                    INCLUDE_DIRS "include" "../open62541lib/include"
                    REQUIRES esp32-pcf8574 driver)
