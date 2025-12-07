idf_component_register(SRCS "open62541.c"
                    INCLUDE_DIRS "include")
component_compile_options(-Wno-error=format= -Wno-format -Wempty-body)

# Определение для IEEE 754 на ESP32
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    UA_ARCHITECTURE_ESP32
    UA_ENABLE_NATIVE_IEEE_754
)
