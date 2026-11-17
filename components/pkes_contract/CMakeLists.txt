# Contrato GATT + HMAC + helpers BLE/cabecalho compartilhados por key/, vehicle/ e relays/.
# REQUIRES bt: ble_contract.h usa os tipos de UUID do NimBLE (host/ble_uuid.h) e
#              ble_link.c chama a API de GAP/GATT.
# mbedtls: pkes_hmac.c calcula o HMAC (PSA no IDF v6, MD legado no v5.4).
# log: esp_log nos helpers.
idf_component_register(SRCS "pkes_hmac.c" "ble_link.c" "run_meta.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt mbedtls log)

# Cabecalho de execucao (M-13): commit git e estado, lidos no configure. "+mod" marca
# mudancas nao commitadas em arquivos rastreados (untracked nao conta, senao os builds
# de scratch dentro da arvore sujariam a marca).
execute_process(COMMAND git rev-parse --short HEAD
                WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
                OUTPUT_VARIABLE _run_commit
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                RESULT_VARIABLE _run_rc)
if(NOT _run_rc EQUAL 0 OR _run_commit STREQUAL "")
    set(_run_commit "sem-git")
endif()
execute_process(COMMAND git status --porcelain --untracked-files=no
                WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
                OUTPUT_VARIABLE _run_dirty
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
if(NOT _run_dirty STREQUAL "")
    set(_run_commit "${_run_commit}+mod")
endif()
string(TIMESTAMP _run_build_time "%Y-%m-%d %H:%M")

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    "RUN_GIT_COMMIT=\"${_run_commit}\""
    "RUN_BUILD_TIME=\"${_run_build_time}\"")
