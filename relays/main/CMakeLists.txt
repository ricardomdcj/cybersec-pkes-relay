# REQUIRES explicito: no ESP-IDF 6.x os drivers/stacks vivem em componentes separados
# e o componente main precisa lista-los. bt = controlador BT + host NimBLE;
# esp_wifi traz tambem o esp_now.h; esp_timer e usado no autoteste do tunel.
idf_component_register(SRCS "main.c" "link.c" "relay_a.c" "relay_b.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt esp_wifi esp_netif esp_event nvs_flash esp_timer pkes_contract cyd serial_ota)

# Permite escolher papel e modo sem editar os headers:
#   idf.py build -DRELAY_ROLE=1 -DRELAY_LINK_SELFTEST=0
# (os headers usam #ifndef, entao o define da linha de comando prevalece).
if(DEFINED RELAY_ROLE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE RELAY_ROLE=${RELAY_ROLE})
endif()
if(DEFINED RELAY_LINK_SELFTEST)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE RELAY_LINK_SELFTEST=${RELAY_LINK_SELFTEST})
endif()
