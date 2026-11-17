# Explicit REQUIRES: in ESP-IDF 6.x the drivers/stacks live in separate components
# The main component must list them. bt = BT controller + NimBLE host;
# esp_wifi also brings esp_now.h; esp_timer is used by the tunnel self-test.
idf_component_register(SRCS "main.c" "link.c" "relay_a.c" "relay_b.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt esp_wifi esp_netif esp_event nvs_flash esp_timer pkes_contract cyd serial_ota)

# Lets you choose role and mode without editing the headers:
#   idf.py build -DRELAY_ROLE=1 -DRELAY_LINK_SELFTEST=0
# (the headers use #ifndef, so the command-line define wins).
if(DEFINED RELAY_ROLE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE RELAY_ROLE=${RELAY_ROLE})
endif()
if(DEFINED RELAY_LINK_SELFTEST)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE RELAY_LINK_SELFTEST=${RELAY_LINK_SELFTEST})
endif()
