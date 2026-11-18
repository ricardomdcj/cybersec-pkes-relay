idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt nvs_flash esp_timer pkes_contract cyd serial_ota)

# Optional default campaign label (tags the log/CSV lines; the RUN command overrides it):
#   idf.py -DRUN_LABEL=baseline build      (or -DRUN_LABEL=relay)
if(DEFINED RUN_LABEL)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE "RUN_LABEL=\"${RUN_LABEL}\"")
endif()
