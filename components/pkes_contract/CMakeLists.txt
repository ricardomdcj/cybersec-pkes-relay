# Contrato GATT + HMAC compartilhados por key/, vehicle/ e relays/.
# REQUIRES bt: ble_contract.h usa os tipos de UUID do NimBLE (host/ble_uuid.h).
# mbedtls: pkes_hmac.c calcula o HMAC (PSA no IDF v6, MD legado no v5.4).
idf_component_register(SRCS "pkes_hmac.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt mbedtls)
