idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt nvs_flash esp_timer pkes_contract cyd serial_ota)

# Rotulo opcional da campanha de medida (marca as linhas de log/CSV):
#   idf.py -DRUN_LABEL=baseline build      (ou -DRUN_LABEL=relay)
if(DEFINED RUN_LABEL)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE "RUN_LABEL=\"${RUN_LABEL}\"")
endif()
