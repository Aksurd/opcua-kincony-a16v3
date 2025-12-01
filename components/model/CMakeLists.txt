idf_component_register(SRCS "model.c" "ds18b20.c"
                    INCLUDE_DIRS "include" "../open62541lib/include"
                    REQUIRES driver)
